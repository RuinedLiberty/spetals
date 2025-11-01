#include <Shared/Map.hh>

#ifdef SERVERSIDE
#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#endif

#include <cmath>
#include <iostream>

using namespace Map;

uint32_t Map::difficulty_at_level(uint32_t level) {
    if (level / LEVELS_PER_EXTRA_SLOT > MAX_DIFFICULTY) return MAX_DIFFICULTY;
    return level / LEVELS_PER_EXTRA_SLOT;
}

uint32_t Map::get_zone_from_pos(float x, float y) {
    uint32_t ret = 0;
    for (uint32_t i = 1; i < MAP_DATA.size(); ++i) {
        struct ZoneDefinition const &zone = MAP_DATA[i];
        if (fclamp(x, zone.left, zone.right) == x && fclamp(y, zone.top, zone.bottom) == y)
            ret = i;
    }
    return ret;
}

uint32_t Map::get_suitable_difficulty_zone(uint32_t power) {
    std::vector<uint32_t> possible_zones;
    for (uint32_t i = 0; i < MAP_DATA.size(); ++i)
        if (MAP_DATA[i].difficulty == power) possible_zones.push_back(i);
    if (possible_zones.size() == 0) return 0;
    return possible_zones[frand() * possible_zones.size()];
}

#ifdef SERVERSIDE
#include <Shared/Simulation.hh>
void Map::remove_mob(Simulation *sim, uint32_t zone) {
    DEBUG_ONLY(assert(zone < MAP_DATA.size());)
    --sim->zone_mob_counts[zone];
}

void Map::spawn_random_mob(Simulation *sim, float x, float y) {
    uint32_t zone_id = Map::get_zone_from_pos(x, y);
    struct ZoneDefinition const &zone = MAP_DATA[zone_id];
    if (zone.density * (zone.right - zone.left) * (zone.bottom - zone.top) / (500 * 500) < sim->zone_mob_counts[zone_id]) return;

    auto is_clear_from_players = [&](float px, float py, float radius) -> bool {
        bool ok = true;
        sim->for_each<kFlower>([&](Simulation *sm, Entity &pl){
            if (!ok) return;
            if (pl.has_component(kMob)) return; // only check player/bot flowers
            if (!sm->ent_alive(pl.id)) return;
            float dx = pl.get_x() - px;
            float dy = pl.get_y() - py;
            float dist = std::sqrt(dx*dx + dy*dy);
            float minSep = pl.get_radius() + radius + 20.0f;
            if (dist < minSep) ok = false;
        });
        return ok;
    };

    float sum = 0;
    for (SpawnChance const &s : zone.spawns)
        sum += s.chance;
    sum *= frand();
    for (SpawnChance const &s : zone.spawns) {
        sum -= s.chance;
        if (sum <= 0) {
            // Try multiple offsets within the zone to avoid spawning on players
            float sx = x, sy = y; bool placed = false; float rr = 0.0f;
            // We need a temporary radius to check separation before entity exists.
            float temp_radius = 20.0f; // fallback default
            {
                // Peek the mob radius using a deterministic seed approx; use midpoint of range
                const MobData &md = MOB_DATA[s.id];
                temp_radius = (md.radius.lower + md.radius.upper) * 0.5f;
            }
            for (uint32_t attempt = 0; attempt < 20; ++attempt) {
                float px = lerp(zone.left, zone.right, frand());
                float py = lerp(zone.top, zone.bottom, frand());
                if (is_clear_from_players(px, py, temp_radius)) { sx = px; sy = py; placed = true; break; }
            }
            Entity &ent = alloc_mob(sim, s.id, sx, sy, NULL_ENTITY);
            ent.zone = zone_id;
            ent.immunity_ticks = TPS;
            BitMath::set(ent.flags, EntityFlags::kSpawnedFromZone);
            sim->zone_mob_counts[zone_id]++;
            return;
        }
    }
}


bool Map::find_spawn_location(Simulation *sim, float d, Vector &vref) {
    for (uint32_t i = 0; i < 10; ++i) {
        vref.set(frand() * ARENA_WIDTH, frand() * ARENA_HEIGHT);
        bool valid = true;
        sim->for_each<kFlower>([&](Simulation *, Entity &ent) {
            if (ent.has_component(kMob)) return;
            if (!valid) return;
            if (Vector(ent.get_x() - vref.x, ent.get_y() - vref.y).magnitude() < d) 
                valid = false;
        });
        if (valid) return true;
    }
    return false;
}
#endif