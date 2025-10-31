#include <Server/EntityFunctions.hh>

#include <Server/PetalTracker.hh>
#include <Server/Spawn.hh>
#ifndef WASM_SERVER
#include <Server/AuthDB.hh>
#else
#include <Server/Account/WasmAccountStore.hh>
#endif
#include <Server/Account/AccountLink.hh>
#include <Server/Server.hh>

#include <Shared/Entity.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <algorithm>

#ifdef WASM_SERVER
extern "C" void record_mob_kill_js(const char *account_id_c, int mob_id);
extern "C" void add_account_xp_js(const char *account_id_c, int delta);
#endif

static void _alloc_drops(Simulation *sim, std::vector<PetalID::T> &success_drops, float x, float y) {
#ifdef DEBUG
    for (PetalID::T id : success_drops)
        assert(id != PetalID::kNone && id < PetalID::kNumPetals);
#endif
    size_t count = success_drops.size();
        for (size_t i = count; i > 0; --i) {
        PetalID::T drop_id = success_drops[i - 1];
        if (PETAL_DATA[drop_id].rarity == RarityID::kUnique && PetalTracker::get_count(sim, drop_id) > 0) {
            success_drops[i - 1] = success_drops[count - 1];
            --count;
            success_drops.pop_back();
        }
    }
    DEBUG_ONLY(assert(success_drops.size() == count);)
    if (count > 1) {
        for (size_t i = 0; i < count; ++i) {
            Entity &drop = alloc_drop(sim, success_drops[i]);
            drop.set_x(x);
            drop.set_y(y);
            drop.velocity.unit_normal(i * 2 * M_PI / count).set_magnitude(25);
        }
    } else if (count == 1) {
        Entity &drop = alloc_drop(sim, success_drops[0]);
        drop.set_x(x);
        drop.set_y(y);
    }
}

static void _add_score(Simulation *sim, EntityID const killer_id, Entity const &target) {
    if (!sim->ent_exists(killer_id)) return;
    Entity &killer = sim->get_ent(killer_id);
    if (killer.has_component(kScore))
        killer.set_score(killer.get_score() + target.score_reward);
}

void entity_on_death(Simulation *sim, Entity const &ent) {
    // don't do on_death for any despawned entity
    uint8_t natural_despawn = BitMath::at(ent.flags, EntityFlags::kIsDespawning) && ent.despawn_tick == 0;
    if (ent.score_reward > 0 && sim->ent_exists(ent.last_damaged_by) && !natural_despawn) {
        EntityID killer_id = sim->get_ent(ent.last_damaged_by).base_entity;
        _add_score(sim, killer_id, ent);
        // If this was a mob death, persist kill to the killer's account and push update
        if (ent.has_component(kMob)) {
            // For each player that damaged this mob, record the mob as seen for their account
            for (uint8_t i = 0; i < ent.damager_count; ++i) {
                EntityID damager = ent.damagers[i];
                if (sim->ent_alive(damager)) {
                    std::string acc = AccountLink::get_account_for_entity(damager);
                    if (!acc.empty()) {
#ifndef WASM_SERVER
                        AuthDB::record_mob_kill(acc, (int)ent.get_mob_id());
                        // Grant account XP equal to in-game XP earned from this mob
                        AuthDB::add_account_xp(acc, (int)ent.score_reward);
                        // Push updated account level/xp to this account so client bar updates live
                        Server::game.send_account_level_to_account(acc);
#else
                        // Update in-memory gallery and XP, and persist via JS bridge
                        WasmAccountStore::set_bit(WasmAccountStore::Category::MobGallery, acc, (int)ent.get_mob_id());
                        WasmAccountStore::add_xp(acc, (uint32_t)ent.score_reward);
                        record_mob_kill_js(acc.c_str(), (int)ent.get_mob_id());
                        add_account_xp_js(acc.c_str(), (int)ent.score_reward);
                        // Push updated account level/xp to this account so client bar updates live
                        Server::game.send_account_level_to_account(acc);
#endif
                        Server::game.send_mob_gallery_to_account(acc);
                    }
                }
            }
        }
    }

    if (ent.has_component(kFlower) && sim->ent_alive(ent.get_parent())) {
#ifndef WASM_SERVER
        AccountLink::unmap_player(ent.id);
#endif
        Entity &camera = sim->get_ent(ent.get_parent());

        EntityID killer_id = sim->ent_exists(ent.last_damaged_by) ?
            sim->get_ent(ent.last_damaged_by).base_entity : NULL_ENTITY;
        if (sim->ent_alive(killer_id)) {
            Entity const &killer = sim->get_ent(killer_id);
            if (killer.has_component(kName))
                camera.set_killed_by(killer.get_name());
            else
                camera.set_killed_by("");
        } else if (ent.poison_ticks > 0) {
            camera.set_killed_by("Poison");
        } else {
            camera.set_killed_by("");
        }
    }

    if (ent.has_component(kMob)) {
        if (BitMath::at(ent.flags, EntityFlags::kSpawnedFromZone))
            Map::remove_mob(sim, ent.zone);
        if (!natural_despawn && !(BitMath::at(ent.flags, EntityFlags::kNoDrops))) {
            struct MobData const &mob_data = MOB_DATA[ent.get_mob_id()];
            std::vector<PetalID::T> success_drops = {};
                        // drop_rates are stored as percentages. Apply global multiplier and floor at runtime.
                        for (uint32_t i = 0; i < mob_data.drops.size(); ++i) {
                PetalID::T drop_id = mob_data.drops[i];
                uint8_t rarity = PETAL_DATA[drop_id].rarity;
                float adjusted_pct = apply_drop_rate_modifiers(mob_data.drop_rates[i], rarity);
                float chance_prob = adjusted_pct / 100.0f;
                if (frand() < chance_prob)
                    success_drops.push_back(drop_id);
            }
            _alloc_drops(sim, success_drops, ent.get_x(), ent.get_y());
        }
        if (ent.get_mob_id() == MobID::kAntHole && ent.get_team() == NULL_ENTITY && frand() < DIGGER_SPAWN_CHANCE) {
            EntityID team = NULL_ENTITY;
            if (sim->ent_exists(ent.last_damaged_by))
                team = sim->get_ent(ent.last_damaged_by).get_team();
            alloc_mob(sim, MobID::kDigger, ent.get_x(), ent.get_y(), team);
        }

    } else if (ent.has_component(kPetal)) {
        if (ent.get_petal_id() == PetalID::kWeb || ent.get_petal_id() == PetalID::kTriweb)
            alloc_web(sim, 100, ent);

    } else if (ent.has_component(kFlower)) {
        std::vector<PetalID::T> potential = {};
        for (uint32_t i = 0; i < ent.get_loadout_count() + MAX_SLOT_COUNT; ++i) {
            DEBUG_ONLY(assert(ent.get_loadout_ids(i) < PetalID::kNumPetals));
            PetalTracker::remove_petal(sim, ent.get_loadout_ids(i));
            if (ent.get_loadout_ids(i) != PetalID::kNone && ent.get_loadout_ids(i) != PetalID::kBasic && frand() < 0.95)
                potential.push_back(ent.get_loadout_ids(i));
        }
                for (uint32_t i = 0; i < ent.deleted_petals.size(); ++i) {
            DEBUG_ONLY(assert(ent.deleted_petals[i] < PetalID::kNumPetals));
            // Petal was already removed from PetalTracker at trash time
            if (ent.deleted_petals[i] != PetalID::kNone && ent.deleted_petals[i] != PetalID::kBasic && frand() < 0.95)
                potential.push_back(ent.deleted_petals[i]);
        }
        // no need to deleted_petals.clear, the player dies
        std::sort(potential.begin(), potential.end(), [](PetalID::T a, PetalID::T b) {
            return PETAL_DATA[a].rarity < PETAL_DATA[b].rarity;
        });

        std::vector<PetalID::T> success_drops = {};
        uint32_t numDrops = potential.size();
        if (numDrops > 3)
            numDrops = 3;
        for (uint32_t i = 0; i < numDrops; ++i) {
            PetalID::T p_id = potential.back();
            if (PETAL_DATA[p_id].rarity >= RarityID::kRare && frand() < 0.05)
                p_id = PetalID::kPollen;
            success_drops.push_back(p_id);
            potential.pop_back();
        }
        _alloc_drops(sim, success_drops, ent.get_x(), ent.get_y());

        // if the camera is the one that disconnects
        // no need to re-add the petals to the petal tracker
        if (!sim->ent_alive(ent.get_parent()))
            return;

        Entity &camera = sim->get_ent(ent.get_parent());

        // reset all reloads and stuff
        uint32_t num_left = potential.size();

        // set respawn level
        uint32_t respawn_level = div_round_up(3 * score_to_level(ent.get_score()), 4);
        if (respawn_level > MAX_LEVEL)
            respawn_level = MAX_LEVEL;
        camera.set_respawn_level(respawn_level);

        uint32_t max_possible = MAX_SLOT_COUNT + loadout_slots_at_level(respawn_level);
        num_left = std::min(num_left, max_possible);

        // fill petals
        for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
            camera.set_inventory(i, PetalID::kNone); // force reset
        for (uint32_t i = 0; i < num_left; ++i) {
            DEBUG_ONLY(assert(potential.back() < PetalID::kNumPetals));
            PetalTracker::add_petal(sim, potential.back());
            camera.set_inventory(i, potential.back());
            potential.pop_back();
        }

        // only track up to max_possible
        for (uint32_t i = num_left; i < max_possible; ++i)
            camera.set_inventory(i, PetalID::kNone); // don't track kNone

        // fill with basics
        for (uint32_t i = num_left; i < loadout_slots_at_level(respawn_level); ++i) {
            PetalTracker::add_petal(sim, PetalID::kBasic);
            camera.set_inventory(i, PetalID::kBasic);
        }

    } else if (ent.has_component(kDrop)) {
        if (BitMath::at(ent.flags, EntityFlags::kIsDespawning))
            PetalTracker::remove_petal(sim, ent.get_drop_id());
    }
}
