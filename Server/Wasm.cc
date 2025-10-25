#ifdef WASM_SERVER
#include <Server/Client.hh>
#include <Server/Server.hh>

#include <Shared/Config.hh>

#include <iostream>
#include <string>
#include <unordered_map>

#include <emscripten.h>

std::unordered_map<int, WebSocket *> WS_MAP;

size_t const MAX_BUFFER_LEN = 1024;
static uint8_t INCOMING_BUFFER[MAX_BUFFER_LEN] = {0};

extern "C" {
    void on_connect(int ws_id) {
        std::printf("client connect: [%d]\n", ws_id);
        WebSocket *ws = new WebSocket(ws_id);
        WS_MAP.insert({ws_id, ws});
    }

    void on_disconnect(int ws_id, int reason) {
        auto iter = WS_MAP.find(ws_id);
        if (iter == WS_MAP.end()) {
            std::printf("unknown ws disconnect: [%d]", ws_id);
            return;
        }
        std::printf("client disconnect: [%d]\n", ws_id);
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

WebSocketServer::WebSocketServer() {
    EM_ASM((function(){
        const WSS = require("ws");

        const http = require("http");
        const https = require("https");
        const fs = require("fs");
        const crypto = require("crypto");
        const sqlite3 = require("sqlite3").verbose();

        // Minimal session store (in-memory)
        const SESS_COOKIE = "sess";
        const SESSION_TTL_MS = 1000 * 60 * 60 * 24 * 7; // 7 days
        Module.sessions = Module.sessions || new Map(); // token -> { userId, username, createdAt }
        Module.userActiveWs = Module.userActiveWs || new Map(); // userId -> ws_id
        Module.ws_connections = Module.ws_connections || {};
        Module.sessionByWs = Module.sessionByWs || new Map(); // ws_id -> userId

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
            res.setHeader("Set-Cookie", cookie);
        }
        function clearCookie(res, name) {
            res.setHeader("Set-Cookie", name + "=; Max-Age=0; Path=/; HttpOnly; SameSite=Lax");
        }
        function makeToken() { return crypto.randomBytes(32).toString("hex"); }

        // SQLite init
        var DB_PATH = process.env.DB_PATH || "data.db";
        if (!fs.existsSync(DB_PATH)) {
            fs.writeFileSync(DB_PATH, "");
        }
                var db = new sqlite3.Database(DB_PATH);
        db.serialize(function() {
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
        });

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
                            // Prefer crypto.randomUUID if available
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
                        // create session
                        const token = makeToken();
                        Module.sessions.set(token, { userId: user.id, username: user.username, createdAt: now });
                        setCookie(res, SESS_COOKIE, token, { maxAge: SESSION_TTL_MS });
                        res.writeHead(302, { "Location": "/" });
                        return res.end();
                    } catch(err) {
                        res.writeHead(500); res.end("OAuth Error"); return;
                    }
                }
                if (req.url.startsWith("/auth/logout")) {
                    const cookies = parseCookies(req.headers.cookie||"");
                    const tok = cookies[SESS_COOKIE];
                    if (tok) Module.sessions.delete(tok);
                    clearCookie(res, SESS_COOKIE);
                    res.writeHead(302, { "Location": "/" });
                    return res.end();
                }

                // API: minimal account info
                if (req.url.startsWith("/api/me")) {
                    const cookies = parseCookies(req.headers.cookie||"");
                    const tok = cookies[SESS_COOKIE];
                    const sess = tok ? Module.sessions.get(tok) : null;
                    if (!sess) { res.writeHead(401).end("Unauthorized"); return; }
                    db.all("SELECT discord_id, username, account_xp, account_level FROM users WHERE discord_id=?", [sess.userId], function(err, rows) {
                        if (err) { res.writeHead(500).end("DB Error"); return; }
                        const row = (rows && rows[0]) ? rows[0] : { discord_id: sess.userId, username: sess.username, account_xp: 0, account_level: 1 };
                        res.writeHead(200, { "Content-Type": "application/json" });
                        res.end(JSON.stringify(row));
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

                const port = (process.env.PORT ? parseInt(process.env.PORT, 10) : $0) || $0;
        server.listen(port, function() {
            console.log("Server running at http://localhost:" + port);
        });
        
        const wss = new WSS.Server({ "server": server });

        let curr_id = 0;
                    wss.on("connection", function(ws, req) {
            const ws_id = curr_id;
            Module.ws_connections[ws_id] = ws;
            curr_id = (curr_id + 1) | 0;

            // Authenticate via cookie
            const cookies = parseCookies(req.headers.cookie||"");
            const tok = cookies[SESS_COOKIE];
            const sess = tok ? Module.sessions.get(tok) : null;
            if (!sess) {
                try { ws.close(4002, "Auth Required"); } catch(e){}
                return;
            }
            // Single-session enforcement
            const prev = Module.userActiveWs.get(sess.userId);
            if (prev !== undefined && Module.ws_connections[prev]) {
                try { Module.ws_connections[prev].close(4002, "Another session started"); } catch(e){}
            }
            Module.userActiveWs.set(sess.userId, ws_id);
            Module.sessionByWs.set(ws_id, sess.userId);

            // Log account id and discord display
            const proceed = (accountId) => {
                const discordDisplay = (sess.username || sess.userId || 'unknown');
                console.log('Client connected: account_id=' + (accountId || 'unknown') + ', discord=' + discordDisplay);
                _on_connect(ws_id);
            };
            let accId = (Module.accByDiscord && Module.accByDiscord.get(sess.userId));
            if (!accId) {
                db.get('SELECT account_id FROM discord_links WHERE discord_user_id=?', [sess.userId], function(err, row){
                    if (row && row.account_id) {
                        if (!Module.accByDiscord) Module.accByDiscord = new Map();
                        Module.accByDiscord.set(sess.userId, row.account_id);
                        proceed(row.account_id);
                    } else {
                        proceed(null);
                    }
                });
            } else {
                proceed(accId);
            }


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
                _on_disconnect(ws_id, reason);
                delete Module.ws_connections[ws_id];
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
