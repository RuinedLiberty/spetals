#ifdef WASM_SERVER
#include <Server/Client.hh>
#include <Server/Server.hh>

#include <Shared/Config.hh>

#include <iostream>
#include <string>
#include <unordered_map>

#include <emscripten.h>
#ifdef WASM_SERVER
#include <Server/Account/WasmAccountStore.hh>
#endif

// -----------------------------------------------------------------------------
// uWS bridge state: runtime map of integer ws_id -> WebSocket wrapper
// -----------------------------------------------------------------------------
std::unordered_map<int, WebSocket *> WS_MAP;


// -----------------------------------------------------------------------------
// Externs: bridge helpers to attach account ids to ws connections
// -----------------------------------------------------------------------------
extern "C" void set_ws_account(int ws_id, const char *account_id) {

    auto it = WS_MAP.find(ws_id);
    if (it == WS_MAP.end() || !account_id) return;
    it->second->getUserData()->account_id = std::string(account_id);
}






size_t const MAX_BUFFER_LEN = 1024;
static uint8_t INCOMING_BUFFER[MAX_BUFFER_LEN] = {0};

extern "C" {
    void on_connect(int ws_id) {
        
        WebSocket *ws = new WebSocket(ws_id);
        WS_MAP.insert({ws_id, ws});
    }

    void on_disconnect(int ws_id, int reason) {
        auto iter = WS_MAP.find(ws_id);
        if (iter == WS_MAP.end()) {
            
            return;
        }
        
        Client::on_disconnect(iter->second, reason, {});
        WS_MAP.erase(ws_id);
        delete iter->second;
    }

    void tick() {
        Server::tick();
    }

    void on_message(int ws_id, uint32_t len) {
        auto iter = WS_MAP.find(ws_id);
        //WebSocket *ws = WS_MAP[ws_id];
        if (iter == WS_MAP.end()) return;
        std::string_view message(reinterpret_cast<char const *>(INCOMING_BUFFER), len);
        Client::on_message(iter->second, message, 0);
    }
}

extern "C" void record_mob_kill_js(const char *account_id_c, int mob_id) {
    EM_ASM({
        try { Module.recordMobKill(UTF8ToString($0), $1); } catch(e) {}
    }, account_id_c, mob_id);
}

extern "C" void record_petal_obtained_js(const char *account_id_c, int petal_id) {
    EM_ASM({
        try { Module.recordPetalObtained(UTF8ToString($0), $1); } catch(e) {}
    }, account_id_c, petal_id);
}




extern "C" void wasm_gallery_mark_for(const char *account_id_c, int mob_id) {
    if (!account_id_c) return;
    WasmAccountStore::set_bit(WasmAccountStore::Category::MobGallery, std::string(account_id_c), mob_id);
}


extern "C" void wasm_send_gallery_for(const char *account_id_c) {
    if (!account_id_c) return;
    Server::game.send_mob_gallery_to_account(std::string(account_id_c));
}

extern "C" void wasm_petal_gallery_mark_for(const char *account_id_c, int petal_id) {
    if (!account_id_c) return;
    WasmAccountStore::set_bit(WasmAccountStore::Category::PetalGallery, std::string(account_id_c), petal_id);
}


extern "C" void wasm_send_petal_gallery_for(const char *account_id_c) {
    if (!account_id_c) return;
    Server::game.send_petal_gallery_to_account(std::string(account_id_c));
}




// -----------------------------------------------------------------------------
// WebSocketServer runtime (Node.js via EM_ASM)
// - HTTP endpoints: /, /gardn-client.js, /gardn-client.wasm, /auth/*, /api/*
// - WS endpoint: handled by uWS bridge
// - DB: SQLite with WAL + FULL + FK enforcement
// -----------------------------------------------------------------------------
WebSocketServer::WebSocketServer() {
    EM_ASM((function(){
        // ---- Node runtime (injected) ----


        const WSS = require("ws");

        const http = require("http");
        const https = require("https");
        const fs = require("fs");
        const crypto = require("crypto");
        const sqlite3 = require("sqlite3").verbose();

        // Session cookie name and TTL (persistent login across restarts)
        const SESS_COOKIE = "sess";
        const SESSION_TTL_MS = 1000 * 60 * 60 * 24 * 30; // 30 days
        // Active connection maps
        Module.userActiveWs = Module.userActiveWs || new Map(); // discord_user_id -> ws_id
        Module.ws_connections = Module.ws_connections || {};
        Module.sessionByWs = Module.sessionByWs || new Map(); // ws_id -> discord_user_id


        // Discord config (prefer env, fallback to provided defaults)
        const DISCORD = {
            client_id: (process.env.DISCORD_CLIENT_ID || "1431349079459364954"),
            client_secret: (process.env.DISCORD_CLIENT_SECRET || "7rKcr9JJxN0OhVnSzDfSRV2k5kxcp31M"),
            authorize: "https://discord.com/api/oauth2/authorize",
            token: "https://discord.com/api/oauth2/token",
            me: "https://discord.com/api/users/@me"
        };

        function parseCookies(header) {
            const out = {};
            if (!header) return out;
            header.split(";").forEach(function(c) {
                const idx = c.indexOf("=");
                if (idx === -1) return;
                const k = c.slice(0, idx).trim();
                const v = c.slice(idx + 1).trim();
                out[k] = decodeURIComponent(v);
            });
            return out;
        }
                function setCookie(res, name, value, opts) {
            opts = opts || {};
            let cookie = name + "=" + encodeURIComponent(value);
            if (opts.maxAge) cookie += "; Max-Age=" + Math.floor(opts.maxAge/1000);
            cookie += "; HttpOnly; SameSite=Lax; Path=/";
            if (opts.secure) cookie += "; Secure";
            res.setHeader("Set-Cookie", cookie);
        }
        function clearCookie(res, name) {
            res.setHeader("Set-Cookie", name + "=; Max-Age=0; Path=/; HttpOnly; SameSite=Lax");
        }
        function makeToken() { return crypto.randomBytes(32).toString("hex"); }


                // SQLite init (secure, single canonical path)
        const ALLOW_INIT_DB = (process.env.ALLOW_INIT_DB === '1');
        var DB_PATH = process.env.SPETALS_DB_PATH || process.env.DB_PATH;
        if (!DB_PATH) {
            if (!ALLOW_INIT_DB) { console.error('Fatal: SPETALS_DB_PATH not set and ALLOW_INIT_DB != 1.'); process.exit(1); }
            DB_PATH = 'data.db';
        }
        if (!fs.existsSync(DB_PATH)) {
            if (!ALLOW_INIT_DB) { console.error('Fatal: DB does not exist at ' + DB_PATH + '; set ALLOW_INIT_DB=1 to initialize.'); process.exit(1); }
        }
        var db = new sqlite3.Database(DB_PATH);
        db.serialize(function() {
            try { db.run('PRAGMA journal_mode=WAL'); } catch(e) {}
            try { db.run('PRAGMA synchronous=FULL'); } catch(e) {}
            try { db.run('PRAGMA foreign_keys=ON'); } catch(e) {}
            try { db.run('PRAGMA busy_timeout=5000'); } catch(e) {}
            db.run('CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)');
            db.get('SELECT value FROM meta WHERE key=?', ['db_instance_id'], function(err,row){
                if (!row) {
                    try { db.run('INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)', ['db_instance_id', (crypto.randomBytes(16).toString('hex'))]); } catch(e) {}
                }
            });
            db.get('SELECT value FROM meta WHERE key=?', ['schema_version'], function(err,row){
                if (!row) { try { db.run('INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)', ['schema_version','1']); } catch(e) {} }
            });
            db.run('CREATE TABLE IF NOT EXISTS users (\
                discord_id TEXT PRIMARY KEY,\
                username TEXT,\
                created_at INTEGER,\
                account_xp INTEGER DEFAULT 0,\
                account_level INTEGER DEFAULT 1\
            )');
            db.run('CREATE TABLE IF NOT EXISTS accounts (\
                id TEXT PRIMARY KEY,\
                created_at INTEGER NOT NULL,\
                updated_at INTEGER NOT NULL,\
                banned INTEGER NOT NULL DEFAULT 0,\
                ban_reason TEXT\
            )');
                        db.run('CREATE TABLE IF NOT EXISTS discord_links (\
                account_id TEXT NOT NULL,\
                discord_user_id TEXT NOT NULL UNIQUE,\
                created_at INTEGER NOT NULL\
            )');
            db.run('CREATE TABLE IF NOT EXISTS sessions (\
                id TEXT PRIMARY KEY,\
                account_id TEXT NOT NULL,\
                created_at INTEGER NOT NULL,\
                expires_at INTEGER NOT NULL,\
                revoked INTEGER NOT NULL DEFAULT 0\
            )');

            db.run('CREATE TABLE IF NOT EXISTS mob_kills (\
                account_id TEXT NOT NULL,\
                mob_id INTEGER NOT NULL,\
                kills INTEGER NOT NULL DEFAULT 0,\
                PRIMARY KEY (account_id, mob_id)\
            )');
            db.run('CREATE TABLE IF NOT EXISTS petal_obtained (\
                account_id TEXT NOT NULL,\
                petal_id INTEGER NOT NULL,\
                obtained INTEGER NOT NULL DEFAULT 1,\
                PRIMARY KEY (account_id, petal_id)\
            )');
        });
        console.log('Using DB at: ' + DB_PATH);

        // Expose helpers to persist and fetch mob gallery
        Module.recordMobKill = function(accountId, mobId) {

            try {
                db.run('INSERT INTO mob_kills (account_id, mob_id, kills) VALUES (?, ?, 1) ON CONFLICT(account_id, mob_id) DO UPDATE SET kills = kills + 1', [accountId, mobId]);
            } catch(e) {}
        };
        Module.recordPetalObtained = function(accountId, petalId) {
            try {
                db.run('INSERT INTO petal_obtained (account_id, petal_id, obtained) VALUES (?, ?, 1) ON CONFLICT(account_id, petal_id) DO NOTHING', [accountId, petalId]);
            } catch(e) {}
        };
                Module.seedGalleryForAccount = function(accountId) {
            try {
                // Seed mob gallery
                db.all('SELECT mob_id FROM mob_kills WHERE account_id=?', [accountId], function(err, rows){
                    try {
                        const len1 = lengthBytesUTF8(accountId) + 1;
                        const ptr1 = _malloc(len1);
                        stringToUTF8(accountId, ptr1, len1);
                        if (!err && rows && rows.length) {
                            for (const r of rows) {
                                try { _wasm_gallery_mark_for(ptr1, r.mob_id|0); } catch(e) {}
                            }
                        }
                        try { _wasm_send_gallery_for(ptr1); } catch(e) {}
                        _free(ptr1);
                    } catch(e) {}
                });
                // Seed petal gallery
                db.all('SELECT petal_id FROM petal_obtained WHERE account_id=?', [accountId], function(err, rows){
                    try {
                        const len2 = lengthBytesUTF8(accountId) + 1;
                        const ptr2 = _malloc(len2);
                        stringToUTF8(accountId, ptr2, len2);
                        if (!err && rows && rows.length) {
                            for (const r of rows) {
                                try { _wasm_petal_gallery_mark_for(ptr2, r.petal_id|0); } catch(e) {}
                            }
                        }
                        try { _wasm_send_petal_gallery_for(ptr2); } catch(e) {}
                        _free(ptr2);
                    } catch(e) {}
                });
            } catch(e) {}
        };






        async function exchangeCodeForToken(code, redirect_uri) {
            var params = new URLSearchParams();
            params.append("client_id", DISCORD.client_id);
            params.append("client_secret", DISCORD.client_secret);
            params.append("grant_type", "authorization_code");
            params.append("code", code);
            params.append("redirect_uri", redirect_uri);
            return new Promise(function(resolve, reject) {
                var req = https.request(DISCORD.token, { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" } }, function(res) {
                    var data = "";
                    res.on("data", function(c){ data += c; });
                    res.on("end", function(){
                        try { resolve(JSON.parse(data)); } catch(e){ reject(e); }
                    });
                });
                req.on("error", reject);
                req.write(params.toString());
                req.end();
            });
        }
        async function fetchUser(access_token) {
            return new Promise(function(resolve, reject) {
                var req = https.request(DISCORD.me, { headers: { "Authorization": "Bearer " + access_token } }, function(res) {
                    var data = "";
                    res.on("data", function(c){ data += c; });
                    res.on("end", function(){ try { resolve(JSON.parse(data)); } catch(e){ reject(e); } });
                });
                req.on("error", reject);
                req.end();
            });
        }

        const server = http.createServer(async function(req, res) {
            try {
                // OAuth routes
                if (req.url.startsWith("/auth/login")) {
                    var host = req.headers.host || "";
                    var xfproto = (req.headers["x-forwarded-proto"] || "").toString().split(",")[0].trim();
                    var proto = xfproto || (host.startsWith("localhost") ? "http" : "https");
                    var redirect_uri = proto + "://" + host + "/auth/callback";
                    var url = new URL(DISCORD.authorize);
                    url.searchParams.set("client_id", DISCORD.client_id);
                    url.searchParams.set("redirect_uri", redirect_uri);
                    url.searchParams.set("response_type", "code");
                    url.searchParams.set("scope", "identify");
                    res.writeHead(302, { "Location": url.toString() });
                    return res.end();
                }
                if (req.url.startsWith("/auth/callback")) {
                    const u = new URL(req.url, "http://localhost");
                    const code = u.searchParams.get("code");
                    if (!code) { res.writeHead(400).end("Missing code"); return; }
                    try {
                        var host = req.headers.host || "";
                        var xfproto = (req.headers["x-forwarded-proto"] || "").toString().split(",")[0].trim();
                        var proto = xfproto || (host.startsWith("localhost") ? "http" : "https");
                        var redirect_uri = proto + "://" + host + "/auth/callback";
                        var tokenResp = await exchangeCodeForToken(code, redirect_uri);
                        if (!tokenResp.access_token) { res.writeHead(400).end("Auth failed"); return; }
                        var user = await fetchUser(tokenResp.access_token);
                        if (!user || !user.id) { res.writeHead(400).end("User fetch failed"); return; }
                                                // upsert user into DB
                        const now = Date.now();
                        await new Promise(function(resolve, reject) {
                            db.run(
                                'INSERT INTO users (discord_id, username, created_at) VALUES (?, ?, ?) ON CONFLICT(discord_id) DO UPDATE SET username=excluded.username',
                                [user.id, (user.username || ""), Math.floor(now/1000)],
                                function(err){ if (err) reject(err); else resolve(); }
                            );
                        });
                        // ensure game account exists & linked
                        if (!Module.accByDiscord) Module.accByDiscord = new Map();
                        let accountId = await new Promise(function(resolve, reject){
                            db.get('SELECT account_id FROM discord_links WHERE discord_user_id=?', [user.id], function(err, row){
                                if (err) reject(err); else resolve(row ? row.account_id : null);
                            });
                        });
                                                if (!accountId) {
                            if (process.env.ALLOW_ACCOUNT_CREATE !== '1') {
                                res.writeHead(500); res.end('Account linking disabled (ALLOW_ACCOUNT_CREATE != 1).'); return;
                            }
                            const gen = (crypto.randomUUID ? crypto.randomUUID() : (function(){ const r = crypto.randomBytes(16).toString('hex'); return r.slice(0,8)+'-'+r.slice(8,12)+'-'+r.slice(12,16)+'-'+r.slice(16,20)+'-'+r.slice(20); })());
                            accountId = gen;
                            await new Promise(function(resolve, reject){
                                db.run('INSERT INTO accounts (id, created_at, updated_at, banned) VALUES (?, ?, ?, 0)', [accountId, Math.floor(now/1000), Math.floor(now/1000)], function(err){ if (err) reject(err); else resolve(); });
                            });
                            await new Promise(function(resolve, reject){
                                db.run('INSERT INTO discord_links (account_id, discord_user_id, created_at) VALUES (?, ?, ?)', [accountId, user.id, Math.floor(now/1000)], function(err){ if (err) reject(err); else resolve(); });
                            });
                        }

                                                Module.accByDiscord.set(user.id, accountId);
                        // create persistent session in DB
                        const token = makeToken();
                        const createdAt = Math.floor(Date.now()/1000);
                        const expiresAt = Math.floor((Date.now()+SESSION_TTL_MS)/1000);
                        await new Promise((resolve,reject)=>{ db.run('INSERT INTO sessions (id, account_id, created_at, expires_at, revoked) VALUES (?, ?, ?, ?, 0)', [token, accountId, createdAt, expiresAt], function(err){ if (err) reject(err); else resolve(); }); });
                        var secure = (proto === 'https');
                        setCookie(res, SESS_COOKIE, token, { maxAge: SESSION_TTL_MS, secure });
                        res.writeHead(302, { "Location": "/" });
                        return res.end();

                    } catch(err) {
                        res.writeHead(500); res.end("OAuth Error"); return;
                    }
                }
                                if (req.url.startsWith("/auth/logout")) {
                    const cookies = parseCookies(req.headers.cookie||"");
                    const tok = cookies[SESS_COOKIE];
                    if (tok) { try { db.run('UPDATE sessions SET revoked=1 WHERE id=?', [tok]); } catch(e) {} }
                    clearCookie(res, SESS_COOKIE);
                    res.writeHead(302, { "Location": "/" });
                    return res.end();
                }


                // API: minimal account info
                                if (req.url.startsWith("/api/me")) {
                    const cookies = parseCookies(req.headers.cookie||"");
                    const tok = cookies[SESS_COOKIE];
                    const row = tok ? await new Promise((resolve)=>{ db.get('SELECT account_id, expires_at, revoked FROM sessions WHERE id=? LIMIT 1', [tok], function(err,row){ resolve(err?null:row); }); }) : null;
                    if (!row || row.revoked) { res.writeHead(401).end("Unauthorized"); return; }
                    const now = Math.floor(Date.now()/1000);
                    if (now > Number(row.expires_at)) { res.writeHead(401).end("Unauthorized"); return; }
                    const link = await new Promise((resolve)=>{ db.get('SELECT discord_user_id FROM discord_links WHERE account_id=? LIMIT 1', [row.account_id], function(err,row){ resolve(err?null:row); }); });
                    if (!link) { res.writeHead(401).end("Unauthorized"); return; }
                    db.all("SELECT discord_id, username, account_xp, account_level FROM users WHERE discord_id=?", [link.discord_user_id], function(err, rows) {
                        if (err) { res.writeHead(500).end("DB Error"); return; }
                        const out = (rows && rows[0]) ? rows[0] : { discord_id: link.discord_user_id, username: '', account_xp: 0, account_level: 1 };
                        res.writeHead(200, { "Content-Type": "application/json" });
                        res.end(JSON.stringify(out));
                    });
                    return;
                }


                // Static file server
                let encodeType = "text/html";
                let file = "index.html";
                switch (req.url) {
                    case "/":
                        break;
                    case "/gardn-client.js":
                        encodeType = "application/javascript";
                        file = "gardn-client.js";
                        break;
                    case "/gardn-client.wasm":
                        encodeType = "application/wasm";
                        file = "gardn-client.wasm";
                        break;
                    default:
                        file = "";
                        break;
                }
                if (fs.existsSync(file)) {
                    res.writeHead(200, {"Content-Type": encodeType});
                    res.end(fs.readFileSync(file));
                    return;
                }
                res.writeHead(404, {"Content-Type": encodeType});
                res.end();
            } catch(err) {
                res.writeHead(500); res.end("Server Error");
            }
        });

        const port = $0;
        server.listen(port, function() {
            console.log("Server running at http://localhost:" + port);
        });
        
                // Automated backups
        (function(){
            try {
                const path = require('path');
                const BACKUP_DIR = process.env.DB_BACKUP_DIR || 'db_backups';
                const RET = parseInt(process.env.DB_BACKUP_RETENTION || '30', 10);
                if (!fs.existsSync(BACKUP_DIR)) { try { fs.mkdirSync(BACKUP_DIR, { recursive: true }); } catch(e) {} }
                function doBackup(){
                    try {
                        if (!fs.existsSync(DB_PATH)) return;
                        const ts = new Date().toISOString().replace(/[:T]/g,'-').split('.')[0];
                        const dest = path.join(BACKUP_DIR, 'backup-' + ts + '.db');
                        fs.copyFileSync(DB_PATH, dest);
                        // Cleanup old backups
                        const files = fs.readdirSync(BACKUP_DIR).filter(f=>f.endsWith('.db')).map(f=>({f, t: fs.statSync(path.join(BACKUP_DIR,f)).mtimeMs})).sort((a,b)=>b.t-a.t);
                        for (let i=RET; i<files.length; ++i) {
                            try { fs.unlinkSync(path.join(BACKUP_DIR, files[i].f)); } catch(e) {}
                        }
                    } catch(e) {}
                }
                doBackup();
                setInterval(doBackup, 1000 * 60 * 60 * 6);
            } catch(e) {}
        })();

        const wss = new WSS.Server({ "server": server });


        let curr_id = 0;
                    wss.on("connection", function(ws, req) {
            const ws_id = curr_id;
            Module.ws_connections[ws_id] = ws;
            curr_id = (curr_id + 1) | 0;

            // Authenticate via cookie using DB-backed sessions
            const cookies = parseCookies(req.headers.cookie||"");
            const tok = cookies[SESS_COOKIE];
            db.get('SELECT account_id, expires_at, revoked FROM sessions WHERE id=? LIMIT 1', [tok], function(err, srow){
                try {
                    if (err || !srow || srow.revoked) { try { ws.close(4002, "Auth Required"); } catch(e){} return; }
                    const now = Math.floor(Date.now()/1000);
                    if (now > Number(srow.expires_at)) { try { ws.close(4002, "Auth Required"); } catch(e){} return; }
                    db.get('SELECT discord_user_id FROM discord_links WHERE account_id=? LIMIT 1', [srow.account_id], function(err2, link){
                        if (err2 || !link) { try { ws.close(4002, "Auth Required"); } catch(e){} return; }
                        // Single-session enforcement by discord id
                        const prev = Module.userActiveWs.get(link.discord_user_id);
                        if (prev !== undefined && Module.ws_connections[prev]) {
                            try { Module.ws_connections[prev].close(4002, "Another session started"); } catch(e){}
                        }
                        Module.userActiveWs.set(link.discord_user_id, ws_id);
                        Module.sessionByWs.set(ws_id, link.discord_user_id);
                        // proceed with mapped account
                        const accountIdForConn = srow.account_id;
                        const discordDisplay = (link.discord_user_id || 'unknown');
                        console.log('Client connected: account_id=' + (accountIdForConn || 'unknown') + ', discord=' + discordDisplay);
                        _on_connect(ws_id);
                        if (accountIdForConn) {
                            try {
                                const len = lengthBytesUTF8(accountIdForConn) + 1;
                                const ptr = _malloc(len);
                                stringToUTF8(accountIdForConn, ptr, len);
                                _set_ws_account(ws_id, ptr);
                                _free(ptr);
                            } catch (e) {}
                            try { Module.seedGalleryForAccount(accountIdForConn); } catch(e) {}
                        }
                        // attach ws handlers
                        ws.on("message", function(message) {
                            let data = new Uint8Array(message);
                            const len = data.length > $2 ? $2 : data.length;
                            data = data.subarray(0, len);
                            HEAPU8.set(data, $1);
                            _on_message(ws_id, len);
                        });
                        ws.on("close", function(reason) {
                            const uid = Module.sessionByWs.get(ws_id);
                            if (uid && Module.userActiveWs.get(uid) === ws_id)
                                Module.userActiveWs.delete(uid);
                            Module.sessionByWs.delete(ws_id);
                            console.log('Client disconnected: account_id=' + (accountIdForConn || 'unknown') + ', discord=' + discordDisplay + ', code=' + reason);
                            _on_disconnect(ws_id, reason);
                            delete Module.ws_connections[ws_id];
                        });
                    });
                } catch(e) { try { ws.close(4002, "Auth Required"); } catch(_){} }
            });
        })

    })(), SERVER_PORT, INCOMING_BUFFER, MAX_BUFFER_LEN);
}




void Server::run() {
    EM_ASM({
        setInterval(_tick, $0);
    }, 1000 / TPS);
}

void Client::send_packet(uint8_t const *packet, size_t size) {
    if (ws == nullptr) return;
    ws->send(packet, size);
}

WebSocket::WebSocket(int id) : ws_id(id) {
    client.ws = this;
}

void WebSocket::send(uint8_t const *packet, size_t size) {
    EM_ASM({
        if (!Module.ws_connections || !Module.ws_connections[$0]) return;
        const ws = Module.ws_connections[$0];
        ws.send(HEAPU8.subarray($1,$1+$2));
    }, ws_id, packet, size);
}

static int _ws_is_authenticated(int id) {
    return EM_ASM_INT({
        if (!Module.sessionByWs) return 0;
        return Module.sessionByWs.has($0) ? 1 : 0;
    }, id);
}

bool ws_is_authenticated(int id) {
    return _ws_is_authenticated(id) != 0;
}


void WebSocket::end(int code, std::string const &message) {
    EM_ASM({
        if (!Module.ws_connections || !Module.ws_connections[$0]) return;
        const ws = Module.ws_connections[$0];
        ws.close($1, UTF8ToString($2));
    }, ws_id, code, message.c_str());
}

Client *WebSocket::getUserData() {
    return &client;
}

// extern "C" bool ws_is_authenticated(int id);



WebSocketServer Server::server;
#endif
