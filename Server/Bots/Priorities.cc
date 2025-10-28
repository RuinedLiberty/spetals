#include <Server/Bots/Priorities.hh>

#include <cmath>

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
    // Heuristic: consider petals with projectile-like behavior as ranged by name/known IDs
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
    uint8_t N = player.get_loadout_count();
    for (uint8_t i=0;i<N+MAX_SLOT_COUNT;++i) if (player.get_loadout_ids(i) == PetalID::kBasic) return true;
    return false;
}

static inline bool has_heal_in_main_excl_leaf(Entity const &player) {
    uint8_t N = player.get_loadout_count();
    for (uint8_t i=0;i<N;++i) {
        PetalID::T pid = player.get_loadout_ids(i);
        if (pid == PetalID::kLeaf) continue;
        if (is_heal_only(pid)) return true;
    }
    return false;
}

Decision evaluate(Context const &ctx) {
    Decision d; // defaults to None, score 0

    float hpRatio = ctx.player.health / std::max(1.0f, ctx.player.max_health);

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
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 150, ctx.half_h + 150, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kMob)) return; if (!in_fov(ctx, e)) return;
            auto const &md = MOB_DATA[e.get_mob_id()];
            uint8_t minHealRarity = 255; bool dropsHeal = false;
            for (uint32_t i=0;i<MAX_DROPS_PER_MOB;++i) {
                PetalID::T pid = md.drops[i]; if (pid >= PetalID::kNumPetals) continue;
                if (is_heal_only(pid)) { dropsHeal = true; minHealRarity = std::min<uint8_t>(minHealRarity, rarity_of(pid)); }
            }
            if (!dropsHeal) return;
            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
            // Prefer closer; tiebreak by higher minimal heal rarity
            if (healMob.null() || d2 < bestD2M || (std::fabs(d2 - bestD2M) < 1e-3 && minHealRarity > bestMinRarity)) {
                healMob = e.id; bestD2M = d2; bestMinRarity = minHealRarity;
            }
        });
        if (!healMob.null()) { d.type = Decision::SeekHealMob; d.target = healMob; d.score = 4.0f; return d; }
    }

    // FULL_HP and heal in main (excluding Leaf): pursue any visible damage drop to replace heal (score 4)
    if (hpRatio >= FULL_HP && has_heal_in_main_excl_leaf(ctx.player)) {
        EntityID dmgDrop = NULL_ENTITY; float bestD2b = 0.0f;
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kDrop)) return; if (!in_fov(ctx, e)) return;
            PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return; if (!is_damage_petal(pid)) return;
            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
            if (dmgDrop.null() || d2 < bestD2b) { dmgDrop = e.id; bestD2b = d2; }
        });
        if (!dmgDrop.null()) { d.type = Decision::Loot; d.target = dmgDrop; d.score = 4.0f; return d; }
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
    ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
        if (!sm->ent_alive(e.id)) return; if (!e.has_component(kMob)) return; if (e.get_team() == ctx.player.get_team()) return; if (!in_fov(ctx, e)) return;
        float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
        if (closest.null() || d2 < bestDist2) { closest = e.id; bestDist2 = d2; }
    });
    if (!closest.null()) { d.type = Decision::Attack; d.target = closest; d.score = 1.0f; }
    return d;
}

void apply_rearrange(Context &ctx) {
    uint8_t mainN = ctx.player.get_loadout_count();
    uint8_t secStart = mainN;

    auto r_of = [&](PetalID::T id){ return rarity_of(id); };

    float hpRatio = ctx.player.health / std::max(1.0f, ctx.player.max_health);

    // Priority 4 (inventory while low HP): equip healing in main without trashing the replaced petal
    if (hpRatio < LOW_HP) {
        // Find any healing petal in secondary
        int heal_j = -1;
        for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
            PetalID::T sid = ctx.player.get_loadout_ids(secStart + j);
            if (is_heal_only(sid)) { heal_j = (int)j; break; }
        }
        if (heal_j >= 0) {
            // Choose replacement slot in main: lowest rarity; if tie, prefer ranged; else lowest damage
            uint8_t rep_i = 0; uint8_t rep_r = 255; float rep_dmg = 1e9f; bool rep_is_ranged = false;
            for (uint8_t i=0;i<mainN;++i) {
                PetalID::T mid = ctx.player.get_loadout_ids(i);
                uint8_t r = r_of(mid);
                float dmg = damage_of(mid);
                bool rng = is_ranged(mid);
                // Priority: lower rarity first; if equal rarity, ranged first; else lower damage
                if (r < rep_r || (r == rep_r && (rng && !rep_is_ranged)) || (r == rep_r && rng == rep_is_ranged && dmg < rep_dmg)) {
                    rep_i = i; rep_r = r; rep_dmg = dmg; rep_is_ranged = rng;
                }
            }
            // Swap secondary heal into chosen main slot; move replaced petal into the secondary slot (no trash)
            uint8_t idx = secStart + (uint8_t)heal_j;
            PetalID::T replaced = ctx.player.get_loadout_ids(rep_i);
            PetalID::T heal = ctx.player.get_loadout_ids(idx);
            ctx.player.set_loadout_ids(rep_i, heal);
            ctx.player.set_loadout_ids(idx, replaced);
            return;
        }
    }

    // Priority 4 (inventory while fully healed): move healing petals back to secondary by swapping with any damage petal in secondary (no trash)
    if (hpRatio >= FULL_HP) {
        // For each healing petal in main (exclude Leaf), try to bring a damage petal from secondary into that slot
        for (uint8_t i=0;i<mainN;++i) {
            PetalID::T mid = ctx.player.get_loadout_ids(i);
            if (mid == PetalID::kLeaf) continue;
            if (!is_heal_only(mid)) continue;
            int dmg_j = -1;
            for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
                PetalID::T sid = ctx.player.get_loadout_ids(secStart + j);
                if (is_damage_petal(sid) || sid == PetalID::kLeaf) { dmg_j = (int)j; break; }
            }
            if (dmg_j >= 0) {
                uint8_t idx = secStart + (uint8_t)dmg_j;
                PetalID::T dmg = ctx.player.get_loadout_ids(idx);
                // Swap: damage to main, heal to secondary
                ctx.player.set_loadout_ids(i, dmg);
                ctx.player.set_loadout_ids(idx, mid);
                return;
            }
        }
    }

    // Priority 3 (inventory): Replace any lower-rarity main petal with highest-rarity from secondary; trash replaced (but never use heal-only for upgrade, and avoid selecting heal-only as worst if possible)
    // Find worst main slot by rarity among non-heal-only first
    int worst_i = -1; uint8_t worst_r = 255;
    for (uint8_t i=0;i<mainN;++i) {
        PetalID::T mid = ctx.player.get_loadout_ids(i);
        if (is_heal_only(mid)) continue; // skip heals
        uint8_t r = r_of(mid);
        if (r < worst_r) { worst_r = r; worst_i = (int)i; }
    }
    // If all main slots were heals, fall back to overall worst (we still avoid using heal-only from secondary)
    if (worst_i < 0) {
        worst_r = 255;
        for (uint8_t i=0;i<mainN;++i) {
            uint8_t r = r_of(ctx.player.get_loadout_ids(i));
            if (r < worst_r) { worst_r = r; worst_i = (int)i; }
        }
    }
    // Find best secondary petal by rarity (exclude heal-only), strictly better than worst
    int best_j = -1; uint8_t best_r = 0;
    for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
        uint8_t idx = secStart + j; PetalID::T sid = ctx.player.get_loadout_ids(idx);
        if (sid == PetalID::kNone) continue;
        if (is_heal_only(sid)) continue; // never pull heals into main for P3 upgrade
        uint8_t r = r_of(sid);
        if (r > best_r) { best_r = r; best_j = (int)j; }
    }
    if (best_j >= 0 && worst_i >= 0 && best_r > worst_r) {
        // Upgrade: move best secondary into worst main and trash the replaced (only if replaced is not heal-only)
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
    return pd.damage > 0.01f || pd.attributes.extra_body_damage > 0.0f;
}

} } // namespace Bots::Priority
