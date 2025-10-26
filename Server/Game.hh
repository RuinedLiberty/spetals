#pragma once

#include <Server/TeamManager.hh>

#include <Shared/Simulation.hh>

#include <set>
#include <string>

class Client;

class GameInstance {
    std::set<Client *> clients;
    TeamManager team_manager;
public:
    Simulation simulation;
    GameInstance();
    void init();
    void tick();
    void add_client(Client *);
    void remove_client(Client *);

    // Send the current mob gallery to all connected clients for this account id
    void send_mob_gallery_to_account(const std::string &account_id);
#ifdef WASM_SERVER
    // Helpers for WASM Node bridge to seed/mark gallery via DB and WASM in-memory store
    void wasm_send_gallery_for(const std::string &account_id);
    void wasm_gallery_mark_for(const std::string &account_id, int mob_id);
#endif
};