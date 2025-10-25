#include <Client/Socket.hh>
#include <Client/Game.hh>

#include <Shared/Binary.hh>
#include <Shared/Config.hh>

#include <cstring>
#include <iostream>

#include <emscripten.h>

uint8_t INCOMING_PACKET[64 * 1024] = {0};
uint8_t OUTGOING_PACKET[1 * 1024] = {0};

extern "C" {
    void on_message(uint8_t type, uint32_t len, char *reason) {
        if (type == 0) {
            std::printf("Connected\n");
            Writer w(INCOMING_PACKET);
            w.write<uint8_t>(Serverbound::kVerify);
            w.write<uint64_t>(VERSION_HASH);
            Game::reset();
            Game::socket.ready = 1; //force send
            Game::socket.send(w.packet, w.at - w.packet);
            Game::socket.ready = 0;
        } 
        else if (type == 2) {
            Game::on_game_screen = 0;
            Game::socket.ready = 0;
            std::printf("Disconnected [%d](%s)\n", len, reason);
            if (std::strlen(reason))
                Game::disconnect_message = std::format("Disconnected with code {} (\"{}\")", len, reason);
            else
                Game::disconnect_message = std::format("Disconnected with code {}", len);
            free(reason);
        }
        else if (type == 1) {
            Game::socket.ready = 1;
            Game::on_message(INCOMING_PACKET, len);
        }
    }
}

Socket::Socket() {}

void Socket::connect(std::string const url) {
    std::cout << "Connecting to " << url << '\n';
    EM_ASM({
        let configured = UTF8ToString($1);

        function resolveWsUrl() {
            try {
                if (configured && configured.length) return configured;
                // Build ws(s) URL from the current page location
                const isHttps = location.protocol === 'https:';
                const proto = isHttps ? 'wss:' : 'ws:';
                const host = location.host; // includes port if any
                return proto + '//' + host + '/ws/';
            } catch (e) {
                return configured || 'ws://localhost:' + ($2|0) + '/ws/';
            }
        }

        function connect() {
            // Avoid duplicate connects
            if (Module.socket && (Module.socket.readyState === 0 || Module.socket.readyState === 1)) return;
            const url = resolveWsUrl();
            try { console.log('Connecting to', url); } catch(e) {}
            let socket = Module.socket = new WebSocket(url);
            socket.binaryType = "arraybuffer";
            socket.onopen = function() {
                try { update_logged_in_as(); } catch(e) {}
                _on_message(0, 0, 0);
            };
            socket.onclose = function(a) {
                _on_message(2, a.code, stringToNewUTF8(a.reason));
                // If we were navigating for auth (back from Discord), refresh login state
                try { if (a.code === 1005) { update_logged_in_as(); } } catch(e) {}
                // Only retry if this tab is active
                if (Module.shouldAttemptConnection && Module.shouldAttemptConnection()) {
                    Module.scheduleConnect ? Module.scheduleConnect(1000) : setTimeout(connect, 1000);
                }
            };
            socket.onmessage = function(event) {
                HEAPU8.set(new Uint8Array(event.data), $0);
                _on_message(1, event.data.byteLength, 0);
            };
        }

        if (!Module._socketInit) {
            Module._socketInit = true;
            Module.socketReconnectTimer = null;
            Module.shouldAttemptConnection = function() {
                try {
                    // Active iff tab is visible and focused
                    return document.visibilityState === 'visible' && document.hasFocus();
                } catch (e) {
                    return true;
                }
            };
            Module.clearReconnect = function() {
                if (Module.socketReconnectTimer) { clearTimeout(Module.socketReconnectTimer); Module.socketReconnectTimer = null; }
            };
            Module.scheduleConnect = function(delay) {
                Module.clearReconnect();
                if (!Module.shouldAttemptConnection()) return;
                Module.socketReconnectTimer = setTimeout(connect, delay || 0);
            };
            // React to visibility/focus changes
            document.addEventListener('visibilitychange', function() {
                // On becoming visible, refresh login state first
                if (document.visibilityState === 'visible') { try { update_logged_in_as(); } catch(e) {} }
                if (Module.shouldAttemptConnection()) {
                    Module.scheduleConnect(0);
                } else {
                    // Hidden: stop reconnect attempts; keep existing connection (server may close if a new tab connects)
                    Module.clearReconnect();
                }
            });
            window.addEventListener('focus', function() {
                if (Module.shouldAttemptConnection()) Module.scheduleConnect(0);
            });
            window.addEventListener('blur', function() {
                // No action on blur. We rely on visibility change to close.
            });
        }

        if (Module.shouldAttemptConnection && Module.shouldAttemptConnection()) {
            Module.scheduleConnect ? Module.scheduleConnect(1000) : setTimeout(connect, 1000);
        }
    }, INCOMING_PACKET, url.c_str(), SERVER_PORT);
}


void Socket::send(uint8_t *ptr, uint32_t len) {
    if (ready == 0) return; 
    EM_ASM({
        if (Module.socket?.readyState == 1) {
            Module.socket.send(HEAPU8.subarray($0, $0+$1));
        }
    }, ptr, len);
}