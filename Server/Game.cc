#include <Server/Game.hh>

#include <Server/Client.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>
#include <Server/Account/AccountLink.hh>
#include <Server/Bots/ForwardShims.hh>
#include <Server/Account/AccountLevel.hh>
#ifndef WASM_SERVER
#include <Server/AuthDB.hh>
#else
#include <Server/Account/WasmAccountStore.hh>
#endif




#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/Map.hh>

#include <vector>
#include <algorithm>
#include <cmath>


static void _send_mob_gallery_for(Client *client) {
    if (!client || client->account_id.empty()) return;
    Writer w(Server::OUTGOING_PACKET);
    w.write<uint8_t>(Clientbound::kMobGallery);
    uint32_t const N = MobID::kNumMobs;
    uint32_t bytes = (N + 7) / 8;
    std::vector<uint8_t> bits(bytes, 0);
#ifndef WASM_SERVER
    {
        std::vector<int> mob_ids;
        if (AuthDB::get_mob_ids(client->account_id, mob_ids)) {
            for (int id : mob_ids) {
                if (id >= 0 && id < (int)N) bits[(uint32_t)id >> 3] |= (1u << ((uint32_t)id & 7));
            }
        }
    }
#else
        {
        std::vector<uint8_t> cached;
        if (WasmAccountStore::get_bits(WasmAccountStore::Category::MobGallery, client->account_id, cached)) {
            std::fill(bits.begin(), bits.end(), 0);
            uint32_t to_copy = std::min<uint32_t>((uint32_t)cached.size(), bytes);
            if (to_copy > 0) {
                std::copy_n(cached.begin(), to_copy, bits.begin());
            }
        }
    }
#endif


    w.write<uint32_t>(bytes);
    for (uint32_t i=0;i<bytes;++i) w.write<uint8_t>(bits[i]);
    client->send_packet(w.packet, w.at - w.packet);
}

static void _send_account_level_for(Client *client) {
    if (!client || client->account_id.empty()) return;
    uint32_t lvl = 1, xp = 0;
    AccountLevel::get_level_and_xp(client->account_id, lvl, xp);
    Writer w(Server::OUTGOING_PACKET);
    w.write<uint8_t>(Clientbound::kAccountLevel);
    w.write<uint32_t>(lvl);
    w.write<uint32_t>(xp);
    client->send_packet(w.packet, w.at - w.packet);

    // Also send explicit xp requirement for next level
    uint32_t need = AccountLevel::get_xp_needed_for_next(lvl);
    Writer w2(Server::OUTGOING_PACKET);
    w2.write<uint8_t>(Clientbound::kAccountLevelBar);
    w2.write<uint32_t>(lvl);
    w2.write<uint32_t>(xp);
    w2.write<uint32_t>(need);
    client->send_packet(w2.packet, w2.at - w2.packet);
}

static void _send_petal_gallery_for(Client *client) {

    if (!client || client->account_id.empty()) return;
    Writer w(Server::OUTGOING_PACKET);
    w.write<uint8_t>(Clientbound::kPetalGallery);
    uint32_t const N = PetalID::kNumPetals;
    uint32_t bytes = (N + 7) / 8;
    std::vector<uint8_t> bits(bytes, 0);
#ifndef WASM_SERVER
    {
        std::vector<int> petal_ids;
        if (AuthDB::get_petal_ids(client->account_id, petal_ids)) {
            for (int id : petal_ids) {
                if (id >= 0 && id < (int)N) bits[(uint32_t)id >> 3] |= (1u << ((uint32_t)id & 7));
            }
        }
    }
#else
        {
        std::vector<uint8_t> cached;
        if (WasmAccountStore::get_bits(WasmAccountStore::Category::PetalGallery, client->account_id, cached)) {
            std::fill(bits.begin(), bits.end(), 0);
            uint32_t to_copy = std::min<uint32_t>((uint32_t)cached.size(), bytes);
            if (to_copy > 0) {
                std::copy_n(cached.begin(), to_copy, bits.begin());
            }
        }
    }
#endif

    // Always ensure Basic petal is visible
    bits[(uint32_t)PetalID::kBasic >> 3] |= (1u << ((uint32_t)PetalID::kBasic & 7));

    w.write<uint32_t>(bytes);
    for (uint32_t i=0;i<bytes;++i) w.write<uint8_t>(bits[i]);
    client->send_packet(w.packet, w.at - w.packet);
} 
 

static void _update_client(Simulation *sim, Client *client) {
    if (client == nullptr) return;
    if (!client->verified) return;
    if (sim == nullptr) return;
    if (!sim->ent_exists(client->camera)) return;
    std::set<EntityID> in_view;
    std::vector<EntityID> deletes;
    in_view.insert(client->camera);
    Entity &camera = sim->get_ent(client->camera);
    if (sim->ent_exists(camera.get_player())) 
        in_view.insert(camera.get_player());
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kClientUpdate);
    writer.write<EntityID>(client->camera);
        sim->spatial_hash.query(camera.get_camera_x(), camera.get_camera_y(), 
    960 / camera.get_fov() + 50, 540 / camera.get_fov() + 50, [&](Simulation *, Entity &ent){
        in_view.insert(ent.id);
    });
    // Also include nearby CPU-controlled cameras (bots) so client can debug their vision rectangles
    sim->for_each<kCamera>([&](Simulation *sm, Entity &other_cam){
        if (other_cam.id == camera.id) return;
        // Only replicate bot cameras
        if (!BitMath::at(other_cam.flags, EntityFlags::kCPUControlled)) return;
        float dx = std::fabs(other_cam.get_camera_x() - camera.get_camera_x());
        float dy = std::fabs(other_cam.get_camera_y() - camera.get_camera_y());
        if (dx <= 960 / camera.get_fov() + 50 && dy <= 540 / camera.get_fov() + 50) {
            in_view.insert(other_cam.id);
        }
    });


    for (EntityID const &i: client->in_view) {
        if (!in_view.contains(i)) {
            writer.write<EntityID>(i);
            deletes.push_back(i);
        }
    }

    for (EntityID const &i : deletes)
        client->in_view.erase(i);

    writer.write<EntityID>(NULL_ENTITY);
    //upcreates
    for (EntityID id: in_view) {
        DEBUG_ONLY(assert(sim->ent_exists(id));)
        Entity &ent = sim->get_ent(id);
        uint8_t create = !client->in_view.contains(id);
        writer.write<EntityID>(id);
        writer.write<uint8_t>(create | (ent.pending_delete << 1));
        ent.write(&writer, BitMath::at(create, 0));
        client->in_view.insert(id);
    }
    writer.write<EntityID>(NULL_ENTITY);
    //write arena stuff
    writer.write<uint8_t>(client->seen_arena);
    sim->arena_info.write(&writer, client->seen_arena);
    client->seen_arena = 1;
    client->send_packet(writer.packet, writer.at - writer.packet);

    // After the main update, send real account levels for visible real players (not bots)
    {
        Writer w(Server::OUTGOING_PACKET);
        w.write<uint8_t>(Clientbound::kEntityAccountLevels);
        for (EntityID const &id : in_view) {
            if (!sim->ent_exists(id)) continue;
            Entity &e = sim->get_ent(id);
            if (!e.has_component(kFlower)) continue;
            // Resolve account for this entity; bots are mapped to "bot:*" and should be skipped
            std::string acc = AccountLink::get_account_for_entity(e.id);
            if (acc.empty()) continue;
            if (acc.rfind("bot:", 0) == 0) continue; // starts with "bot:"
            uint32_t lvl = 1, xp = 0;
            AccountLevel::get_level_and_xp(acc, lvl, xp);
            w.write<EntityID>(e.id);
            w.write<uint32_t>(lvl);
        }
        // terminator
        w.write<EntityID>(NULL_ENTITY);
        client->send_packet(w.packet, w.at - w.packet);
    }
        {
        // Determine the global top account by total XP (offline or online)
        std::string top_acc;
#ifndef WASM_SERVER
        AuthDB::get_top_account_by_xp(top_acc);
#else
        WasmAccountStore::get_top_account(top_acc);
#endif
        EntityID top_player_ent = NULL_ENTITY;
        if (!top_acc.empty()) {
            // Find if this top account has a currently online player entity to anchor the crown.
            sim->for_each<kFlower>([&](Simulation *sm, Entity &pl){
                std::string acc = AccountLink::get_account_for_entity(pl.id);
                if (acc == top_acc) { top_player_ent = pl.id; }
            });
        }
        Writer w(Server::OUTGOING_PACKET);
        w.write<uint8_t>(Clientbound::kTopAccountLeader);
        w.write<EntityID>(top_player_ent);
        client->send_packet(w.packet, w.at - w.packet);
    }
}

GameInstance::GameInstance() : simulation(), clients(), team_manager(&simulation) {}

void GameInstance::init() {
    for (uint32_t i = 0; i < ENTITY_CAP / 2; ++i)
        Map::spawn_random_mob(&simulation, frand() * ARENA_WIDTH, frand() * ARENA_HEIGHT);
    #ifdef GAMEMODE_TDM
    team_manager.add_team(ColorID::kBlue);
    team_manager.add_team(ColorID::kRed);
    #endif
        // Spawn server-side CPU cameras/players to simulate population
    if (BOT_COUNT > 0) {
        Bots_spawn_all(&simulation, BOT_COUNT);
    }
}

void GameInstance::tick() {
    // IMPORTANT: Drive bot AI before simulation.tick so their inputs apply this frame
    Bots_on_tick(&simulation);
    simulation.tick();
    for (Client *client : clients)
        _update_client(&simulation, client);
    simulation.post_tick();
}

void GameInstance::add_client(Client *client) {
    DEBUG_ONLY(assert(client->game != this);)
    if (client->game != nullptr)
        client->game->remove_client(client);
    client->game = this;
    clients.insert(client);
    Entity &ent = simulation.alloc_ent();
    ent.add_component(kCamera);
    ent.add_component(kRelations);
    #ifdef GAMEMODE_TDM
    EntityID team = team_manager.get_random_team();
    ent.set_team(team);
    ent.set_color(simulation.get_ent(team).get_color());
    ++simulation.get_ent(team).player_count;
    #else
    ent.set_team(ent.id);
    ent.set_color(ColorID::kYellow); 
    #endif
    
    ent.set_fov(BASE_FOV);
    ent.set_respawn_level(1);
    for (uint32_t i = 0; i < loadout_slots_at_level(ent.get_respawn_level()); ++i)
        ent.set_inventory(i, PetalID::kBasic);
    for (uint32_t i = 0; i < loadout_slots_at_level(ent.get_respawn_level()); ++i)
        PetalTracker::add_petal(&simulation, ent.get_inventory(i));
        client->camera = ent.id;
    client->seen_arena = 0;

        if (!client->account_id.empty()) {
        AccountLink::map_camera(client->camera, client->account_id);
        _send_mob_gallery_for(client);
        _send_petal_gallery_for(client);
        _send_account_level_for(client);
    }

}

void GameInstance::remove_client(Client *client) {
    DEBUG_ONLY(assert(client->game == this);)
    clients.erase(client);
    if (simulation.ent_exists(client->camera)) {
        Entity &c = simulation.get_ent(client->camera);
        if (simulation.ent_exists(c.get_team()))
            --simulation.get_ent(c.get_team()).player_count;
        if (simulation.ent_exists(c.get_player()))
            simulation.request_delete(c.get_player());
        for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
            PetalTracker::remove_petal(&simulation, c.get_inventory(i));
        AccountLink::unmap_camera(client->camera);
        simulation.request_delete(client->camera);
    }
    client->game = nullptr;
}

void GameInstance::send_mob_gallery_to_account(const std::string &account_id) {
    for (Client *c : clients) {
        if (c && c->account_id == account_id) {
            _send_mob_gallery_for(c);
        }
    }
}

void GameInstance::send_petal_gallery_to_account(const std::string &account_id) {
    for (Client *c : clients) {
        if (c && c->account_id == account_id) {
            _send_petal_gallery_for(c);
        }
    }
}

void GameInstance::send_account_level_to_account(const std::string &account_id) {
    for (Client *c : clients) {
        if (c && c->account_id == account_id) {
            _send_account_level_for(c);
        }
    }
}


