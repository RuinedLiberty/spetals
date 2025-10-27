#include <Server/AuthDB.hh>

#include <sqlite3.h>
#include <string>
#include <cstring>
#include <ctime>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <filesystem>

namespace {
    sqlite3 *g_db = nullptr;
    std::string g_db_path;
}

namespace AuthDB {

bool init(const std::string &db_path) {
    if (g_db) return true;
    if (!db_path.empty()) {
        g_db_path = db_path;
    } else {
        const char *envp = std::getenv("SPETALS_DB_PATH");
        if (envp && *envp) {
            g_db_path = std::string(envp);
        } else {
            const char *allow = std::getenv("ALLOW_INIT_DB");
            if (!(allow && std::string(allow) == "1")) {
                std::cerr << "Fatal: SPETALS_DB_PATH not set and ALLOW_INIT_DB != 1. Refusing implicit DB.\n";
                return false;
            }
            g_db_path = std::string("data.db");
        }
    }
    bool exists_before = std::filesystem::exists(g_db_path);
    if (!exists_before) {
        const char *allow = std::getenv("ALLOW_INIT_DB");
        if (!(allow && std::string(allow) == "1")) {
            std::cerr << "Fatal: DB does not exist at " << g_db_path << "; set ALLOW_INIT_DB=1 to initialize.\n";
            return false;
        }
    }
    if (sqlite3_open(g_db_path.c_str(), &g_db) != SQLITE_OK) {
        std::cerr << "SQLite open failed: " << sqlite3_errmsg(g_db) << "\n";
        return false;
    }
    sqlite3_busy_timeout(g_db, 5000);
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(g_db, "PRAGMA synchronous=FULL;", nullptr, nullptr, nullptr);
    sqlite3_exec(g_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
        const char *schema =
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE IF NOT EXISTS accounts (\n"
        "  id TEXT PRIMARY KEY,\n"
        "  created_at INTEGER NOT NULL,\n"
        "  updated_at INTEGER NOT NULL,\n"
        "  banned INTEGER NOT NULL DEFAULT 0,\n"
        "  ban_reason TEXT\n"
        ");"
        "CREATE TABLE IF NOT EXISTS discord_links (\n"
        "  account_id TEXT NOT NULL,\n"
        "  discord_user_id TEXT NOT NULL UNIQUE,\n"
        "  created_at INTEGER NOT NULL,\n"
        "  FOREIGN KEY(account_id) REFERENCES accounts(id)\n"
        ");"
        "CREATE TABLE IF NOT EXISTS sessions (\n"
        "  id TEXT PRIMARY KEY,\n"
        "  account_id TEXT NOT NULL,\n"
        "  created_at INTEGER NOT NULL,\n"
        "  expires_at INTEGER NOT NULL,\n"
        "  revoked INTEGER NOT NULL DEFAULT 0,\n"
        "  user_agent_hash TEXT,\n"
        "  last_ip_hash TEXT,\n"
        "  FOREIGN KEY(account_id) REFERENCES accounts(id)\n"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_sessions_account ON sessions(account_id);"
        // New: mob_kills table to track which mobs an account has killed
        "CREATE TABLE IF NOT EXISTS mob_kills (\n"
        "  account_id TEXT NOT NULL,\n"
        "  mob_id INTEGER NOT NULL,\n"
        "  kills INTEGER NOT NULL DEFAULT 0,\n"
        "  PRIMARY KEY (account_id, mob_id),\n"
        "  FOREIGN KEY(account_id) REFERENCES accounts(id)\n"
        ");"
        // New: petal_obtained table to track which petals an account has obtained
        "CREATE TABLE IF NOT EXISTS petal_obtained (\n"
        "  account_id TEXT NOT NULL,\n"
        "  petal_id INTEGER NOT NULL,\n"
        "  obtained INTEGER NOT NULL DEFAULT 1,\n"
        "  PRIMARY KEY (account_id, petal_id),\n"
        "  FOREIGN KEY(account_id) REFERENCES accounts(id)\n"
        ");"
    ;
    char *errmsg = nullptr;
    if (sqlite3_exec(g_db, schema, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::cerr << "SQLite schema error: " << (errmsg ? errmsg : "") << "\n";
        if (errmsg) sqlite3_free(errmsg);
        return false;
    }
    // meta table for db_instance_id and schema_version
    sqlite3_exec(g_db, "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);", nullptr, nullptr, nullptr);
    // db_instance_id
    {
        sqlite3_stmt *s = nullptr;
        if (sqlite3_prepare_v2(g_db, "SELECT value FROM meta WHERE key='db_instance_id' LIMIT 1", -1, &s, nullptr) == SQLITE_OK) {
            int rc = sqlite3_step(s);
            sqlite3_finalize(s);
            if (rc != SQLITE_ROW) {
                auto rnd = [](){ unsigned r = (unsigned) std::rand(); return r; };
                auto hex16 = [](unsigned v){ char b[17]; std::snprintf(b, sizeof(b), "%08x", v); return std::string(b); };
                std::string inst = hex16(rnd()) + hex16(rnd());
                sqlite3_stmt *ins = nullptr;
                if (sqlite3_prepare_v2(g_db, "INSERT OR REPLACE INTO meta (key, value) VALUES ('db_instance_id', ?1)", -1, &ins, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(ins, 1, inst.c_str(), -1, SQLITE_STATIC);
                    sqlite3_step(ins);
                    sqlite3_finalize(ins);
                }
            }
        }
    }
    // schema_version
    {
        sqlite3_stmt *s = nullptr;
        if (sqlite3_prepare_v2(g_db, "SELECT value FROM meta WHERE key='schema_version' LIMIT 1", -1, &s, nullptr) == SQLITE_OK) {
            int rc = sqlite3_step(s);
            sqlite3_finalize(s);
            if (rc != SQLITE_ROW) {
                sqlite3_exec(g_db, "INSERT OR REPLACE INTO meta (key, value) VALUES ('schema_version','1')", nullptr, nullptr, nullptr);
            }
        }
    }
    return true;
}

static bool is_valid_uuid(const std::string &s) {
    return s.size() == 36 && s[8]=='-' && s[13]=='-' && s[18]=='-' && s[23]=='-';
}

bool upsert_account_for_discord(const std::string &discord_user_id, std::string &account_id_out) {
    if (!g_db) {
        if (!init("") ) return false;
    }
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, "SELECT account_id FROM discord_links WHERE discord_user_id=?1 LIMIT 1", -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, discord_user_id.c_str(), -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *acc = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        if (acc) account_id_out.assign(acc);
        sqlite3_finalize(stmt);
        return !account_id_out.empty();
    }
    sqlite3_finalize(stmt);
    // Not found: create new account and link
    // Generate a UUID v4 (simple random-based implementation)
    auto rnd = [](){ unsigned r = (unsigned) std::rand(); return r; };
    auto hex16 = [](unsigned v){ char b[17]; std::snprintf(b, sizeof(b), "%08x", v); return std::string(b); };
    std::string u = hex16(rnd()) + "-" + hex16(rnd()).substr(0,4) + "-4" + hex16(rnd()).substr(0,3) + "-a" + hex16(rnd()).substr(0,3) + "-" + hex16(rnd()) + hex16(rnd());
    if (!is_valid_uuid(u)) return false;
    std::time_t now = std::time(nullptr);
    sqlite3_exec(g_db, "BEGIN", nullptr, nullptr, nullptr);
    sqlite3_stmt *ins1 = nullptr;
    if (sqlite3_prepare_v2(g_db, "INSERT INTO accounts (id, created_at, updated_at, banned) VALUES (?1, ?2, ?2, 0)", -1, &ins1, nullptr) != SQLITE_OK) { sqlite3_exec(g_db, "ROLLBACK", nullptr, nullptr, nullptr); return false; }
    sqlite3_bind_text(ins1, 1, u.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins1, 2, now);
    if (sqlite3_step(ins1) != SQLITE_DONE) { sqlite3_finalize(ins1); sqlite3_exec(g_db, "ROLLBACK", nullptr, nullptr, nullptr); return false; }
    sqlite3_finalize(ins1);

    sqlite3_stmt *ins2 = nullptr;
    if (sqlite3_prepare_v2(g_db, "INSERT INTO discord_links (account_id, discord_user_id, created_at) VALUES (?1, ?2, ?3)", -1, &ins2, nullptr) != SQLITE_OK) { sqlite3_exec(g_db, "ROLLBACK", nullptr, nullptr, nullptr); return false; }
    sqlite3_bind_text(ins2, 1, u.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(ins2, 2, discord_user_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins2, 3, now);
    if (sqlite3_step(ins2) != SQLITE_DONE) { sqlite3_finalize(ins2); sqlite3_exec(g_db, "ROLLBACK", nullptr, nullptr, nullptr); return false; }
    sqlite3_finalize(ins2);

    sqlite3_exec(g_db, "COMMIT", nullptr, nullptr, nullptr);
    account_id_out = u;
    return true;
}

bool create_session(const std::string &account_id, int ttl_seconds, std::string &sid_out) {
    if (!g_db) {
        if (!init("") ) return false;
    }
    if (!is_valid_uuid(account_id)) return false;
    // generate 256-bit random hex
    unsigned char rnd[32];
    for (int i=0;i<32;i++) rnd[i] = (unsigned char)(std::rand() & 0xFF);
    static const char *hex = "0123456789abcdef";
    sid_out.resize(64);
    for (int i=0;i<32;i++){ sid_out[2*i] = hex[(rnd[i]>>4)&0xF]; sid_out[2*i+1] = hex[rnd[i]&0xF]; }

    std::time_t now = std::time(nullptr);
    std::time_t exp = now + (ttl_seconds>0? ttl_seconds : 28800);
    sqlite3_stmt *ins = nullptr;
    if (sqlite3_prepare_v2(g_db, "INSERT INTO sessions (id, account_id, created_at, expires_at, revoked) VALUES (?1, ?2, ?3, ?4, 0)", -1, &ins, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(ins, 1, sid_out.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 2, account_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins, 3, now);
    sqlite3_bind_int64(ins, 4, exp);
    bool ok = sqlite3_step(ins) == SQLITE_DONE;
    sqlite3_finalize(ins);
    return ok;
}

bool validate_session_and_get_account(const std::string &sid, std::string &account_id_out) {
    if (!g_db) {
        if (!init("") ) return false;
    }
    // sid must be hex 64 (256-bit) to pass basic sanity
    if (sid.size() < 32 || sid.size() > 128) return false;
    // Query session
    const char *sql =
        "SELECT account_id, expires_at, revoked FROM sessions WHERE id = ?1 LIMIT 1";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, sid.c_str(), -1, SQLITE_STATIC);
    bool ok = false;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *acc = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        sqlite3_int64 exp = sqlite3_column_int64(stmt, 1);
        int revoked = sqlite3_column_int(stmt, 2);
        sqlite3_finalize(stmt);
        if (!acc || std::strlen(acc) != 36) return false;
        std::time_t now = std::time(nullptr);
        if (revoked) return false;
        if (now > exp) return false;
        // Check ban
        const char *sql2 = "SELECT banned FROM accounts WHERE id = ?1 LIMIT 1";
        sqlite3_stmt *stmt2 = nullptr;
        if (sqlite3_prepare_v2(g_db, sql2, -1, &stmt2, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt2, 1, acc, -1, SQLITE_STATIC);
        int rc2 = sqlite3_step(stmt2);
        if (rc2 == SQLITE_ROW) {
            int banned = sqlite3_column_int(stmt2, 0);
            sqlite3_finalize(stmt2);
            if (banned) return false;
            account_id_out.assign(acc);
            ok = true;
        } else {
            sqlite3_finalize(stmt2);
        }
    } else {
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool get_discord_username(const std::string &account_id, std::string &username_out) {
    if (!g_db) {
        if (!init("") ) return false;
    }
    // We stored only discord_user_id; username changes on Discord are dynamic.
    // If you prefer latest username, store it in a separate column via auth service on each login.
    // For now, we will just return the discord_user_id as a stand-in.
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, "SELECT discord_user_id FROM discord_links WHERE account_id=?1 LIMIT 1", -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, account_id.c_str(), -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *did = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        if (did) username_out.assign(did);
        sqlite3_finalize(stmt);
        return !username_out.empty();
    }
    sqlite3_finalize(stmt);
    return false;
}

bool record_mob_kill(const std::string &account_id, int mob_id) {
    if (!g_db) { if (!init("") ) return false; }
    if (!is_valid_uuid(account_id)) return false;
    std::cout << "AuthDB: record_mob_kill account_id=" << account_id << ", mob_id=" << mob_id << "\n";
    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "INSERT INTO mob_kills (account_id, mob_id, kills) VALUES (?1, ?2, 1)\n"
        "ON CONFLICT(account_id, mob_id) DO UPDATE SET kills = kills + 1";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AuthDB: failed to prepare record_mob_kill: " << sqlite3_errmsg(g_db) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, account_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, mob_id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        std::cerr << "AuthDB: record_mob_kill step failed: " << sqlite3_errmsg(g_db) << "\n";
    }
    sqlite3_finalize(stmt);
    return ok;
}


bool get_mob_ids(const std::string &account_id, std::vector<int> &mob_ids_out) {
    mob_ids_out.clear();
    if (!g_db) { if (!init("") ) return false; }
    if (!is_valid_uuid(account_id)) return false;
    const char *sql = "SELECT mob_id FROM mob_kills WHERE account_id=?1";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AuthDB: get_mob_ids prepare failed: " << sqlite3_errmsg(g_db) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, account_id.c_str(), -1, SQLITE_STATIC);
    int rows = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int mob_id = sqlite3_column_int(stmt, 0);
        mob_ids_out.push_back(mob_id);
        rows++;
    }
    sqlite3_finalize(stmt);
    std::cout << "AuthDB: get_mob_ids account_id=" << account_id << " -> count=" << rows << "\n";
    return true;
}



bool record_petal_obtained(const std::string &account_id, int petal_id) {
    if (!g_db) { if (!init("") ) return false; }
    if (!is_valid_uuid(account_id)) return false;
    if (petal_id < 0) return false;
    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "INSERT INTO petal_obtained (account_id, petal_id, obtained) VALUES (?1, ?2, 1)\n"
        "ON CONFLICT(account_id, petal_id) DO NOTHING";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AuthDB: failed to prepare record_petal_obtained: " << sqlite3_errmsg(g_db) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, account_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, petal_id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool get_petal_ids(const std::string &account_id, std::vector<int> &petal_ids_out) {
    petal_ids_out.clear();
    if (!g_db) { if (!init("") ) return false; }
    if (!is_valid_uuid(account_id)) return false;
    const char *sql = "SELECT petal_id FROM petal_obtained WHERE account_id=?1";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AuthDB: get_petal_ids prepare failed: " << sqlite3_errmsg(g_db) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, account_id.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int petal_id = sqlite3_column_int(stmt, 0);
        petal_ids_out.push_back(petal_id);
    }
    sqlite3_finalize(stmt);
    return true;
}

} // namespace AuthDB


