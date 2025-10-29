#include <Server/Bots/Priorities.hh>

#include <cmath>
#include <vector>
#include <unordered_map>
#include <Shared/Map.hh>


namespace Bots { namespace Priority {

static inline bool in_fov(Context const &c, Entity const &e) {
    return std::fabs(e.get_x() - c.cx) <= c.half_w && std::fabs(e.get_y() - c.cy) <= c.half_h;
}

static inline uint8_t rarity_of(PetalID::T id) {
    return (id == PetalID::kNone) ? 0 : PETAL_DATA[id].rarity;
}

static inline float damage_of(PetalID::T id) {
    return (id == PetalID::kNone) ? 0.0f : PETAL_DATA[id].damage + PETAL_DATA[id].attributes.extra_body_damage;
}

static inline bool is_ranged(PetalID::T id) {
    if (id == PetalID::kNone) return false;
    switch(id) {
        case PetalID::kDandelion:
        case PetalID::kPeas:
        case PetalID::kMissile:
        case PetalID::kPoisonPeas:
        case PetalID::kLPeas:
        case PetalID::kTriFaster:
        case PetalID::kFaster:
        case PetalID::kTriplet:
            return true;
        default:
            return PETAL_DATA[id].attributes.split_projectile || PETAL_DATA[id].attributes.spawn_count > 0;
    }
}

static inline bool has_basic(Entity const &player) {
    // Check both main and secondary
    uint8_t N = player.get_loadout_count();
    for (uint8_t i = 0; i < N + MAX_SLOT_COUNT; ++i)
        if (player.get_loadout_ids(i) == PetalID::kBasic) return true;
    return false;
}

// Convenience
static inline bool is_rose(PetalID::T id) { return id == PetalID::kRose; }
static inline float rose_damage_threshold() {
    float d = damage_of(PetalID::kRose);
    return (d > 0.0f ? d : 5.0f); // default to 5 if data not populated
}

// --- PURE HEAL definition (broadened) ---
// "Pure heal" = heals AND has negligible damage (treat tiny incidental damage as zero).
// Also explicitly treat Rose as pure-heal to match design intent.
static inline bool is_pure_heal(PetalID::T id) {
    if (id == PetalID::kNone) return false;
    auto const &pd = PETAL_DATA[id];
    bool heals = (pd.attributes.burst_heal > 0 || pd.attributes.constant_heal > 0);
    float dmg = damage_of(id);
    if (!heals) return false;

    if (is_rose(id)) return true;          // explicitly pure-heal behaviorally
    return (dmg < 0.5f);                   // tolerate tiny non-zero damage
}

static inline bool has_heal_in_main_excl_leaf(Entity const &player) {
    uint8_t N = player.get_loadout_count();
    for (uint8_t i=0;i<N;++i) {
        PetalID::T pid = player.get_loadout_ids(i);
        if (pid == PetalID::kLeaf) continue;
        if (is_pure_heal(pid)) return true;
    }
    return false;
}

struct HealPhaseState { bool was_low = false; bool cleanup_done = false; };
static std::unordered_map<uint16_t, HealPhaseState> g_heal_state;

static inline bool main_has_pure_heal(Entity const &player) {
    uint8_t N = player.get_loadout_count();
    for (uint8_t i=0;i<N;++i) {
        PetalID::T pid = player.get_loadout_ids(i);
        if (pid == PetalID::kLeaf) continue;
        if (is_pure_heal(pid)) return true;
    }
    return false;
}

// Helper: pick the best secondary slot whose damage strictly exceeds a threshold
static int pick_best_secondary_damage_slot(Entity const &player, uint8_t secStart, float threshold) {
    int best_j = -1; float best_dmg = -1.0f;
    for (uint8_t j = 0; j < MAX_SLOT_COUNT; ++j) {
        PetalID::T sid = player.get_loadout_ids(secStart + j);
        if (sid == PetalID::kNone) continue;
        if (!is_damage_petal(sid)) continue;
        float dmg = damage_of(sid);
        if (dmg > threshold && dmg > best_dmg) { best_dmg = dmg; best_j = (int)j; }
    }
    return best_j;
}

// Fallback: any "reasonable" swap-in (damage/Leaf/Basic) when nothing beats threshold
static int pick_any_reasonable_swapin(Entity const &player, uint8_t secStart) {
    for (uint8_t j=0; j<MAX_SLOT_COUNT; ++j) {
        PetalID::T sid = player.get_loadout_ids(secStart + j);
        if (sid == PetalID::kNone) continue;
        if (is_damage_petal(sid) || sid == PetalID::kLeaf || sid == PetalID::kBasic) return (int)j;
    }
    return -1;
}

Decision evaluate(Context const &ctx) {
    Decision d; // defaults to None, score 0

    float hpRatio = ctx.player.health / std::max(1.0f, ctx.player.max_health);

    // Priority 10: If overleveled for current zone, evacuate to the next zone (move right) and do not target/loot in this zone
    uint32_t player_level = score_to_level(ctx.player.get_score());
    uint32_t current_zone = Map::get_zone_from_pos(ctx.player.get_x(), ctx.player.get_y());
    uint32_t current_diff = MAP_DATA[current_zone].difficulty;
    uint32_t suitable_diff = Map::difficulty_at_level(player_level);
    bool overleveled_here = (current_diff < suitable_diff);
    if (overleveled_here) {
        d.type = Decision::Evacuate;
        d.target = NULL_ENTITY;
        d.score = 10.0f;
        return d;
    }

        // Priority 4: Low HP healing
    if (hpRatio < LOW_HP) {
        // 4a) If a healing petal drop is visible, go loot it (score 4)
        EntityID healDrop = NULL_ENTITY; float bestD2H = 0.0f;
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kDrop)) return; if (!in_fov(ctx, e)) return;
            PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return; if (!is_heal_only(pid)) return;
            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
            if (healDrop.null() || d2 < bestD2H) { healDrop = e.id; bestD2H = d2; }
        });
        if (!healDrop.null()) { d.type = Decision::Loot; d.target = healDrop; d.score = 4.0f; return d; }

        // 4b) Otherwise, seek mobs likely to drop heals (score 4)
        EntityID healMob = NULL_ENTITY; float bestD2M = 0.0f; uint8_t bestMinRarity = 0;
        // Determine suitable difficulty to avoid targeting mobs in overleveled zones
        uint32_t player_level_l = score_to_level(ctx.player.get_score());
        uint32_t suitable_diff_l = Map::difficulty_at_level(player_level_l);
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 150, ctx.half_h + 150, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kMob)) return; if (!in_fov(ctx, e)) return;
            uint32_t ez = Map::get_zone_from_pos(e.get_x(), e.get_y());
            if (MAP_DATA[ez].difficulty < suitable_diff_l) return; // skip mobs in overleveled zones
            auto const &md = MOB_DATA[e.get_mob_id()];
            uint8_t minHealRarity = 255; bool dropsHeal = false;
            for (uint32_t i=0;i<MAX_DROPS_PER_MOB;++i) {
                PetalID::T pid = md.drops[i]; if (pid >= PetalID::kNumPetals) continue;
                if (is_heal_only(pid)) { dropsHeal = true; minHealRarity = std::min<uint8_t>(minHealRarity, rarity_of(pid)); }
            }
            if (!dropsHeal) return;
            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
            if (healMob.null() || d2 < bestD2M || (std::fabs(d2 - bestD2M) < 1e-3 && minHealRarity > bestMinRarity)) {
                healMob = e.id; bestD2M = d2; bestMinRarity = minHealRarity;
            }
        });
        if (!healMob.null()) { d.type = Decision::SeekHealMob; d.target = healMob; d.score = 4.0f; return d; }
    }


    // FULL HP and heal in main (excluding Leaf): pursue any visible damage drop (prefer dmg > Rose)
    if (hpRatio >= FULL_HP && has_heal_in_main_excl_leaf(ctx.player)) {
        float thr = rose_damage_threshold();
        EntityID strongDrop = NULL_ENTITY; float strongD2 = 0.0f;
        EntityID anyDmgDrop = NULL_ENTITY; float anyD2 = 0.0f;

        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kDrop)) return; if (!in_fov(ctx, e)) return;
            PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return; if (!is_damage_petal(pid)) return;

            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;

            // Track nearest damage->Rose upgrade
            if (damage_of(pid) > thr) {
                if (strongDrop.null() || d2 < strongD2) { strongDrop = e.id; strongD2 = d2; }
            }
            // Track nearest any damage
            // BUGFIX: compare d2 against anyD2 (float), not anyDmgDrop (EntityID)
            if (anyDmgDrop.null() || d2 < anyD2) { anyDmgDrop = e.id; anyD2 = d2; }
        });

        if (!strongDrop.null()) { d.type = Decision::Loot; d.target = strongDrop; d.score = 4.0f; return d; }
        if (!anyDmgDrop.null()) { d.type = Decision::Loot; d.target = anyDmgDrop; d.score = 4.0f; return d; }
    }

    // Priority 3: If any visible drop has rarity higher than the lowest rarity in main, go loot it (score 3)
    uint8_t N = ctx.player.get_loadout_count();
    uint8_t worst = 255; for (uint8_t i=0;i<N;++i) worst = std::min<uint8_t>(worst, rarity_of(ctx.player.get_loadout_ids(i)));
    EntityID bestDrop = NULL_ENTITY; uint8_t bestR = 0; float bestD2 = 0.0f;
    ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
        if (!sm->ent_alive(e.id)) return;
        if (!e.has_component(kDrop)) return;
        if (!in_fov(ctx, e)) return;
        PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return;
        uint8_t r = rarity_of(pid);
        if (r > worst) {
            float dx = e.get_x() - ctx.player.get_x();
            float dy = e.get_y() - ctx.player.get_y();
            float d2 = dx*dx + dy*dy;
            if (bestDrop.null() || r > bestR || (r == bestR && d2 < bestD2)) { bestDrop = e.id; bestR = r; bestD2 = d2; }
        }
    });
    if (!bestDrop.null()) { d.type = Decision::Loot; d.target = bestDrop; d.score = 3.0f; return d; }

    // Priority 2: If bot sees any damage petal on ground and has Basic in inventory, go loot it (score 2)
    if (has_basic(ctx.player)) {
        EntityID dmgDrop = NULL_ENTITY; float bestD2b = 0.0f;
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kDrop)) return; if (!in_fov(ctx, e)) return;
            PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return; if (!is_damage_petal(pid)) return;
            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
            if (dmgDrop.null() || d2 < bestD2b) { dmgDrop = e.id; bestD2b = d2; }
        });
        if (!dmgDrop.null()) { d.type = Decision::Loot; d.target = dmgDrop; d.score = 2.0f; return d; }
    }

        // Rule 1: If bot isn't doing anything, attack the closest visible mob, value = 1
    EntityID closest = NULL_ENTITY; float bestDist2 = 0.0f;
    uint32_t player_level_r1 = score_to_level(ctx.player.get_score());
    uint32_t suitable_diff_r1 = Map::difficulty_at_level(player_level_r1);
    ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
        if (!sm->ent_alive(e.id)) return; if (!e.has_component(kMob)) return; if (e.get_team() == ctx.player.get_team()) return; if (!in_fov(ctx, e)) return;
        uint32_t ez = Map::get_zone_from_pos(e.get_x(), e.get_y());
        if (MAP_DATA[ez].difficulty < suitable_diff_r1) return; // don't target mobs in overleveled zones
        float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
        if (closest.null() || d2 < bestDist2) { closest = e.id; bestDist2 = d2; }
    });
    if (!closest.null()) { d.type = Decision::Attack; d.target = closest; d.score = 1.0f; return d; }


    // Priority 0.5: Wander if no mobs/loot targets; move generally rightward, full speed
    d.type = Decision::Wander; d.score = 0.5f; return d;
}


void apply_rearrange(Context &ctx) {
    uint8_t mainN = ctx.player.get_loadout_count();
    uint8_t secStart = mainN;

    auto r_of = [&](PetalID::T id){ return rarity_of(id); };

    float hpRatio = ctx.player.health / std::max(1.0f, ctx.player.max_health);

    // Track heal phase transitions for one-time cleanup behavior
    HealPhaseState &hs = g_heal_state[ctx.player.id.id];
    if (hpRatio < LOW_HP) { hs.was_low = true; hs.cleanup_done = false; }

    // Inventory unclog: if inventory is full and a visible drop has higher rarity
    // than the worst item we hold, delete the lowest-rarity petal to make room.
    auto is_full = [&]()->bool {
        for (uint8_t i = 0; i < mainN + MAX_SLOT_COUNT; ++i)
            if (ctx.player.get_loadout_ids(i) == PetalID::kNone) return false;
        return true;
    };

        uint8_t worst_idx_all = 255; uint8_t worst_r_all = 255;
    for (uint8_t i = 0; i < mainN + MAX_SLOT_COUNT; ++i) {
        PetalID::T pid = ctx.player.get_loadout_ids(i);
        if (pid == PetalID::kNone) continue;
        uint8_t r = r_of(pid);
        if (r < worst_r_all) { worst_r_all = r; worst_idx_all = i; }
    }

    if (is_full()) {

        // Case 1: We are already overlapping any drop -> free a slot immediately.
        bool overlapping_drop = false;
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 10, ctx.half_h + 10, [&](Simulation *sm, Entity &e){
            if (overlapping_drop) return;
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kDrop)) return; if (!in_fov(ctx, e)) return;
            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y();
            float thr = ctx.player.get_radius() + e.get_radius() + 1.0f;
            if (dx*dx + dy*dy <= thr*thr) overlapping_drop = true;
        });
                if (overlapping_drop && worst_idx_all != 255) {
            ctx.player.set_loadout_ids(worst_idx_all, PetalID::kNone);
            return;
        }

        // Case 2: If we can see a higher-rarity drop than our worst, free a slot to go get it.
        uint8_t best_visible_r = 0;
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kDrop)) return; if (!in_fov(ctx, e)) return;
            PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return;
            uint8_t r = rarity_of(pid);
            if (r > best_visible_r) best_visible_r = r;
        });
                if (best_visible_r > worst_r_all && worst_idx_all != 255) {
            ctx.player.set_loadout_ids(worst_idx_all, PetalID::kNone);
            return; // free a slot; pickup will succeed on collision
        }

    }



    // ============================
    // Priority 4 (LOW HP): Probabilistic MULTI-EQUIP of pure-heals from secondary
    // ============================
    if (hpRatio < LOW_HP) {
        // Collect pure-heals in secondary (Rose etc.)
        std::vector<uint8_t> heals;
        heals.reserve(MAX_SLOT_COUNT);
        for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
            uint8_t idx = secStart + j;
            PetalID::T sid = ctx.player.get_loadout_ids(idx);
            if (is_pure_heal(sid)) heals.push_back(idx);
        }

        if (!heals.empty()) {
            auto pick_replacement_slot = [&](void)->int {
                // Choose replacement slot in main: lowest rarity; tie → prefer ranged; tie → lowest damage.
                // Also: avoid replacing existing pure-heal in main (no-op).
                int rep_i = -1; uint8_t rep_r = 255; float rep_dmg = 1e9f; bool rep_is_ranged = false;
                for (uint8_t i=0;i<mainN;++i) {
                    PetalID::T mid = ctx.player.get_loadout_ids(i);
                    if (is_pure_heal(mid)) continue; // keep existing pure-heals
                    uint8_t r = r_of(mid);
                    float dmg = damage_of(mid);
                    bool rng = is_ranged(mid);
                    if (r < rep_r ||
                       (r == rep_r && (rng && !rep_is_ranged)) ||
                       (r == rep_r && rng == rep_is_ranged && dmg < rep_dmg)) {
                        rep_i = (int)i; rep_r = r; rep_dmg = dmg; rep_is_ranged = rng;
                    }
                }
                return rep_i;
            };

            auto equip_one = [&](uint8_t heal_idx)->bool {
                int rep_i = pick_replacement_slot();
                if (rep_i < 0) return false; // no non-heal slot left to replace
                PetalID::T heal = ctx.player.get_loadout_ids(heal_idx);
                PetalID::T replaced = ctx.player.get_loadout_ids((uint8_t)rep_i);
                ctx.player.set_loadout_ids((uint8_t)rep_i, heal);
                ctx.player.set_loadout_ids(heal_idx, replaced);
                return true;
            };

            // Probabilities per order
            for (size_t k=0; k<heals.size(); ++k) {
                float chance = 0.05f;               // default for 5th+
                if (k == 0) chance = 1.00f;         // 1st heal
                else if (k == 1) chance = 0.50f;    // 2nd
                else if (k == 2) chance = 0.25f;    // 3rd
                else if (k == 3) chance = 0.10f;    // 4th

                if (frand() <= chance) {
                    // After each swap, this secondary slot might now hold the replaced (non-heal) petal.
                    // That’s fine—subsequent iterations will ignore it because it’s no longer pure-heal.
                    equip_one(heals[k]);
                }
            }
            // Do not 'return' here—let other low-HP logic happen in future ticks.
        } else {
            // Fallback: legacy single-heal equip if only generic "heals-only" exist
            int heal_j = -1;
            for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
                PetalID::T sid = ctx.player.get_loadout_ids(secStart + j);
                if (is_heal_only(sid)) { heal_j = (int)j; break; }
            }
            if (heal_j >= 0) {
                // Choose replacement slot in main
                uint8_t rep_i = 0; uint8_t rep_r = 255; float rep_dmg = 1e9f; bool rep_is_ranged = false;
                for (uint8_t i=0;i<mainN;++i) {
                    PetalID::T mid = ctx.player.get_loadout_ids(i);
                    if (is_pure_heal(mid)) continue; // don't replace existing pure-heal
                    uint8_t r = r_of(mid);
                    float dmg = damage_of(mid);
                    bool rng = is_ranged(mid);
                    if (r < rep_r || (r == rep_r && (rng && !rep_is_ranged)) ||
                        (r == rep_r && rng == rep_is_ranged && dmg < rep_dmg)) {
                        rep_i = i; rep_r = r; rep_dmg = dmg; rep_is_ranged = rng;
                    }
                }
                uint8_t idx = secStart + (uint8_t)heal_j;
                PetalID::T replaced = ctx.player.get_loadout_ids(rep_i);
                PetalID::T heal = ctx.player.get_loadout_ids(idx);
                ctx.player.set_loadout_ids(rep_i, heal);
                ctx.player.set_loadout_ids(idx, replaced);
            }
        }
    }

    // ============================
    // Priority 4 (FULL HP): move pure-heals out of main
    // ============================
    if (hpRatio >= FULL_HP) {
        for (uint8_t i=0;i<mainN;++i) {
            PetalID::T mid = ctx.player.get_loadout_ids(i);
            if (mid == PetalID::kLeaf) continue;
            if (!is_pure_heal(mid)) continue;

            // Determine threshold: Rose => ~5; other heals => their own damage (often ~0)
            float thr = is_rose(mid) ? rose_damage_threshold() : damage_of(mid);

            // 1) Try to find a secondary petal strictly stronger than threshold
            int swap_j = pick_best_secondary_damage_slot(ctx.player, secStart, thr);

            // 2) Fallback: any reasonable damage/Leaf/Basic to ensure de-healing main
            if (swap_j < 0) swap_j = pick_any_reasonable_swapin(ctx.player, secStart);

            if (swap_j >= 0) {
                uint8_t idx = secStart + (uint8_t)swap_j;
                PetalID::T repl = ctx.player.get_loadout_ids(idx);
                ctx.player.set_loadout_ids(i, repl); // bring damage in
                ctx.player.set_loadout_ids(idx, mid); // move heal to secondary
            }
        }

        // One-time cleanup of excess pure-heal petals in secondary per heal cycle
        if (hs.was_low && !hs.cleanup_done && !main_has_pure_heal(ctx.player)) {
            std::vector<uint8_t> heals;
            heals.reserve(MAX_SLOT_COUNT);
            for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
                uint8_t idx = secStart + j;
                PetalID::T sid = ctx.player.get_loadout_ids(idx);
                if (is_pure_heal(sid)) heals.push_back(idx);
            }
            for (size_t k=0;k<heals.size();++k) {
                float chance = 0.0f;
                if (k == 0) chance = 0.02f;           // 1st heal: 2%
                else if (k == 1) chance = 0.25f;      // 2nd: 25%
                else if (k == 2) chance = 0.60f;      // 3rd: 60%
                else if (k == 3) chance = 0.85f;      // 4th: 85%
                else chance = 0.98f;                  // 5th+: 98%
                if (frand() < chance) {
                    ctx.player.set_loadout_ids(heals[k], PetalID::kNone);
                }
            }
            hs.cleanup_done = true;
            hs.was_low = false;
        }
    }

    // Priority 3 (inventory): Replace any lower-rarity main petal with highest-rarity from secondary (exclude heals)
    int worst_i = -1; uint8_t worst_r = 255;
    for (uint8_t i=0;i<mainN;++i) {
        PetalID::T mid = ctx.player.get_loadout_ids(i);
        if (is_heal_only(mid)) continue; // skip heals for upgrade target
        uint8_t r = r_of(mid);
        if (r < worst_r) { worst_r = r; worst_i = (int)i; }
    }
    if (worst_i < 0) {
        worst_r = 255;
        for (uint8_t i=0;i<mainN;++i) {
            uint8_t r = r_of(ctx.player.get_loadout_ids(i));
            if (r < worst_r) { worst_r = r; worst_i = (int)i; }
        }
    }
    int best_j = -1; uint8_t best_r = 0;
    for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
        uint8_t idx = secStart + j; PetalID::T sid = ctx.player.get_loadout_ids(idx);
        if (sid == PetalID::kNone) continue;
        if (is_heal_only(sid) || is_pure_heal(sid)) continue; // never pull heals into main for P3 upgrade
        uint8_t r = r_of(sid);
        if (r > best_r) { best_r = r; best_j = (int)j; }
    }
    if (best_j >= 0 && worst_i >= 0 && best_r > worst_r) {
        uint8_t idx = secStart + (uint8_t)best_j;
        ctx.player.set_loadout_ids((uint8_t)worst_i, ctx.player.get_loadout_ids(idx));
        ctx.player.set_loadout_ids(idx, PetalID::kNone);
        return;
    }

    // Priority 2 (inventory): Promote damage petals into main by replacing Basics, then trash Basics in secondary
    for (uint8_t i=0;i<mainN;++i) {
        if (ctx.player.get_loadout_ids(i) != PetalID::kBasic) continue;
        for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
            uint8_t idx = secStart + j; PetalID::T sid = ctx.player.get_loadout_ids(idx);
            if (sid == PetalID::kNone || sid == PetalID::kBasic) continue;
            if (!is_damage_petal(sid)) continue;
            ctx.player.set_loadout_ids(i, sid);
            ctx.player.set_loadout_ids(idx, PetalID::kNone); // trash basic
            return; // do one change per call
        }
    }

    // Trash Basic in secondary (one per call)
    for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
        uint8_t idx = secStart + j;
        if (ctx.player.get_loadout_ids(idx) == PetalID::kBasic) { ctx.player.set_loadout_ids(idx, PetalID::kNone); return; }
    }
}

// Utilities (minimal for current needs)
float rarity_sum_main(Entity const &) { return 0.0f; }

bool is_heal_only(PetalID::T id) {
    if (id == PetalID::kNone) return false; auto const &pd = PETAL_DATA[id];
    return (pd.attributes.burst_heal > 0 || pd.attributes.constant_heal > 0);
}

bool is_damage_petal(PetalID::T id) {
    if (id == PetalID::kNone) return false; auto const &pd = PETAL_DATA[id];
    // Treat any positive damage or body-damage as damage-capable
    return pd.damage > 0.01f || pd.attributes.extra_body_damage > 0.0f;
}

} } // namespace Bots::Priority
