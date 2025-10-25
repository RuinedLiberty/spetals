#ifndef WASM_SERVER
#include <Server/Server.hh>

#include <Server/Client.hh>
#include <Server/AuthDB.hh>
#include <Server/PerSocketData.hh>
#include <Shared/Config.hh>

#include <App.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>



static bool parse_cookie_for_sid(std::string_view cookie, std::string &sid_out) {
    // Very simple parser for sid=...
    size_t pos = 0;
    while (pos < cookie.size()) {
        // skip spaces and semicolons
        while (pos < cookie.size() && (cookie[pos] == ' ' || cookie[pos] == ';')) pos++;
        size_t eq = cookie.find('=', pos);
        if (eq == std::string_view::npos) break;
        std::string key(cookie.substr(pos, eq - pos));
        pos = eq + 1;
        size_t sc = cookie.find(';', pos);
        std::string val(cookie.substr(pos, (sc == std::string_view::npos ? cookie.size() : sc) - pos));
        if (key == "sid") { sid_out = val; return true; }
        if (sc == std::string_view::npos) break;
        pos = sc + 1;
    }
    return false;
}

// validation through SQLite
static bool validate_session_and_get_account(std::string const &sid, std::string &account_id_out) {
    return AuthDB::validate_session_and_get_account(sid, account_id_out);
}


uWS::App Server::server = uWS::App({
    .key_file_name = "misc/key.pem",
    .cert_file_name = "misc/cert.pem",
    .passphrase = "1234"
}).ws<PerSocketData>("/*", {
    /* Settings */
    .compression = uWS::DISABLED,
    .maxPayloadLength = 1024,
    .idleTimeout = 15,
    .maxBackpressure = 1024 * MAX_PACKET_LEN,
    .closeOnBackpressureLimit = true,
    .resetIdleTimeoutOnSend = false,
    .sendPingsAutomatically = true,
    /* Handlers */
        .upgrade = [](auto *res, auto *req, auto *context) {
        // Extract sid from Cookie
        std::string sid;
        std::string_view cookie = req->getHeader("cookie");
        if (!parse_cookie_for_sid(cookie, sid)) {
            res->writeStatus("401 Unauthorized").end("Auth required");
            return;
        }
        std::string account_id;
        if (!validate_session_and_get_account(sid, account_id) || account_id.size() != 36) {
            res->writeStatus("401 Unauthorized").end("Invalid session");
            return;
        }
        // Prepare per-socket data with account id embedded
        PerSocketData psd{};
        std::memset(psd.account_id, 0, sizeof(psd.account_id));
        std::memcpy(psd.account_id, account_id.c_str(), account_id.size() > 36 ? 36 : account_id.size());
        psd.client = nullptr;

        // Upgrade
        res->template upgrade<PerSocketData>(psd,
            req->getHeader("sec-websocket-key"),
            req->getHeader("sec-websocket-protocol"),
            req->getHeader("sec-websocket-extensions"),
            context);
    },
        .open = [](auto *ws) {
        PerSocketData *psd = ws->getUserData();
        // Allocate a Client and attach
        psd->client = new Client();
        psd->client->ws = ws;
        // Store account id on Client for server-side logic/logging
        psd->client->account_id = std::string(psd->account_id);
        // Fetch a friendly Discord display (we return discord_user_id for now)
        std::string display;
        if (!psd->client->account_id.empty()) {
            AuthDB::get_discord_username(psd->client->account_id, display);
        }
        if (display.empty()) display = "unknown";
        std::cout << "Client connected: account_id=" << psd->client->account_id << ", discord=" << display << "\n";
    },
    .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
        Client::on_message(ws, message, opCode);
    },
    .dropped = [](auto *ws, std::string_view /*message*/, uWS::OpCode /*opCode*/) {
        std::cout << "dropped packet\n";
        PerSocketData *psd = ws->getUserData();
        Client *client = (psd ? psd->client : nullptr);
        if (client == nullptr) {
            ws->end(1006, "Dropped Message");
            return;
        }
        client->disconnect();
    },
    .drain = [](auto */*ws*/) {
        /* Check ws->getBufferedAmount() here */
    },
    .close = [](auto *ws, int code, std::string_view message) {
        PerSocketData *psd = ws->getUserData();
        Client *client = (psd ? psd->client : nullptr);
        if (client) {
            client->on_disconnect(ws, code, message);
            delete client;
            psd->client = nullptr;
        }
    }
}).listen(SERVER_PORT, [](auto *listen_socket) {
    if (listen_socket) {
        std::cout << "Listening on port " << SERVER_PORT << std::endl;
    }
});

void Server::run() {
    struct us_loop_t *loop = (struct us_loop_t *) uWS::Loop::get();
    struct us_timer_t *delayTimer = us_create_timer(loop, 0, 0);

    us_timer_set(delayTimer, [](us_timer_t *x){ Server::tick(); }, 1, 1000 / TPS);
    Server::server.run();
}

void Client::send_packet(uint8_t const *packet, size_t size) {
    if (ws == nullptr) return;
    std::string_view message(reinterpret_cast<char const *>(packet), size);
    ws->send(message, uWS::OpCode::BINARY, 0);
}
#endif