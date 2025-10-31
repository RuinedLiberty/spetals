#pragma once

#include <string>
#include <vector>

namespace AuthDB {
    // Initialize SQLite database and ensure schema exists
    // db_path: path to sqlite file (default data.db when empty)
    bool init(const std::string &db_path = "");

    // Create or fetch account for discord user id. Returns account_id (uuid) in out param.
    bool upsert_account_for_discord(const std::string &discord_user_id, std::string &account_id_out);

    // Create a session for account_id. ttl_seconds for expiry. Returns sid (opaque hex string)
    bool create_session(const std::string &account_id, int ttl_seconds, std::string &sid_out);

    // Validate a session token and resolve account uuid
    // Returns true if valid (and not banned), filling account_id_out with UUID (36-chars)
    bool validate_session_and_get_account(const std::string &sid, std::string &account_id_out);

    // Fetch discord username by account id (if stored). Returns true if found.
    bool get_discord_username(const std::string &account_id, std::string &username_out);

    // Fetch discord id and username by account id. Returns true if discord id found.
    bool get_discord_info(const std::string &account_id, std::string &discord_id_out, std::string &username_out);

    // Record a mob kill for this account. mob_id is the numeric MobID::T.
    // Returns true on success.
    bool record_mob_kill(const std::string &account_id, int mob_id);

    // Fetch list of mob_ids this account has killed/seen. Returns true on success.
    bool get_mob_ids(const std::string &account_id, std::vector<int> &mob_ids_out);

    // Petal gallery: record newly obtained petal (from inventory acquisition)
    bool record_petal_obtained(const std::string &account_id, int petal_id);

    // Fetch list of obtained petal ids for this account.
    bool get_petal_ids(const std::string &account_id, std::vector<int> &petal_ids_out);

    // Account XP/Level persistence
    bool add_account_xp(const std::string &account_id, int xp);
    bool get_account_xp(const std::string &account_id, int &xp_out);

    // Returns the account id with highest total account_xp. True if found.
    bool get_top_account_by_xp(std::string &account_id_out);
}



