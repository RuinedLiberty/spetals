#include <Server/Server.hh>

#include <Server/Game.hh>
#include <Server/Client.hh>
#ifndef WASM_SERVER
#include <Server/AuthDB.hh>
#endif

#include <Shared/Binary.hh>

#include <chrono>
#include <iostream>
#ifndef WASM_SERVER
#include <cstdlib>
#include <filesystem>
#endif




namespace Server {
    uint8_t OUTGOING_PACKET[MAX_PACKET_LEN] = {0};
    GameInstance game;
}

using namespace Server;

void Server::tick() {
    using namespace std::chrono_literals;
    auto start = std::chrono::steady_clock::now();
    Server::game.tick();
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> tick_time = end - start;
    if (tick_time > 5ms) std::cout << "tick took " << tick_time << '\n';
    // Heartbeat for debugging
    static int counter = 0;
    if ((++counter % (TPS*10)) == 0) {
        std::cout << "Server heartbeat: running, TPS=" << TPS << "\n";
    }

}

#ifndef WASM_SERVER
static void maybe_start_auth_service() {
    namespace fs = std::filesystem;
    try {
        // If node_modules is missing, try to install deps
        fs::path nm = fs::path("Server") / "auth" / "node_modules";
        if (!fs::exists(nm)) {
#ifndef _WIN32
            std::system("npm install --prefix Server/auth > auth_install.log 2>&1");
#else
            std::system("cmd /c npm install --prefix Server\\auth > auth_install.log 2>&1");
#endif
        }
    } catch (...) {
        // ignore
    }
#ifndef _WIN32
    // Best-effort: start Node auth service in background (Linux)
    // If it's already running, it will fail to bind and exit harmlessly.
    std::system("nohup node Server/auth/server.js > auth.log 2>&1 &");
#else
    // Windows fallback: start in a new window
    std::system("cmd /c start \"auth\" node Server\\auth\\server.js");
#endif
}
#endif

void Server::init() {
#ifndef WASM_SERVER
    // Initialize auth/session database (data.db by default)
    if (!AuthDB::init("")) {
        std::cerr << "Failed to init auth DB (data.db). Server will still start but WS auth will reject.\n";
    }
    // Attempt to start auth service automatically
    maybe_start_auth_service();
#endif

    Server::game.init();
    Server::run();
}



