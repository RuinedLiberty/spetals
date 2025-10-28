#include <Server/Bots/Priorities.hh>

#include <cmath>

namespace Bots { namespace Priority {

static inline bool in_fov(Context const &c, Entity const &e) {
    return std::fabs(e.get_x() - c.cx) <= c.half_w && std::fabs(e.get_y() - c.cy) <= c.half_h;
}

static inline uint8_t rarity_of(PetalID::T id) {
    return (id == PetalID::kNone) ? 0 : PETAL_DATA[id].rarity;
}

static inline bool has_basic(Entity const &player) {
    uint8_t N = player.get_loadout_count();
    for (uint8_t i=0;i<N+MAX_SLOT_COUNT;++i) if (player.get_loadout_ids(i) == PetalID::kBasic) return true;
    return false;
}

Decision evaluate(Context const &ctx) {
    Decision d; // defaults to None, score 0

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
    // Priority 3 (inventory): Replace any lower-rarity main petal with highest-rarity from secondary; trash replaced.
    uint8_t mainN = ctx.player.get_loadout_count();
    uint8_t secStart = mainN;

    auto r_of = [&](PetalID::T id){ return rarity_of(id); };

    // Find worst main slot by rarity
    uint8_t worst_i = 0; uint8_t worst_r = 255;
    for (uint8_t i=0;i<mainN;++i) {
        uint8_t r = r_of(ctx.player.get_loadout_ids(i));
        if (r < worst_r) { worst_r = r; worst_i = i; }
    }
    // Find best secondary petal by rarity (any petal), strictly better than worst
    uint8_t best_j = 255; uint8_t best_r = 0;
    for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
        uint8_t idx = secStart + j; PetalID::T sid = ctx.player.get_loadout_ids(idx);
        if (sid == PetalID::kNone) continue;
        uint8_t r = r_of(sid);
        if (r > best_r) { best_r = r; best_j = j; }
    }
    if (best_j != 255 && best_r > worst_r) {
        // Upgrade: move best secondary into worst main and trash the replaced
        uint8_t idx = secStart + best_j;
        ctx.player.set_loadout_ids(worst_i, ctx.player.get_loadout_ids(idx));
        ctx.player.set_loadout_ids(idx, PetalID::kNone);
        return;
    }

    // Priority 2 (inventory): Promote damage petals into main by replacing Basics, then trash Basics in secondary
    // 1) Promote: swap one secondary damage petal into the first Basic slot in main
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

    // 2) Trash Basic in secondary (one per call)
    for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
        uint8_t idx = secStart + j;
        if (ctx.player.get_loadout_ids(idx) == PetalID::kBasic) { ctx.player.set_loadout_ids(idx, PetalID::kNone); return; }
    }
}

// Utilities (minimal for current needs)
float rarity_sum_main(Entity const &) { return 0.0f; }

bool is_heal_only(PetalID::T id) {
    if (id == PetalID::kNone) return false; auto const &pd = PETAL_DATA[id];
    return (pd.attributes.burst_heal > 0 || pd.attributes.constant_heal > 0) && (pd.damage <= 0.01f);
}

bool is_damage_petal(PetalID::T id) {
    if (id == PetalID::kNone) return false; auto const &pd = PETAL_DATA[id];
    return pd.damage > 0.01f || pd.attributes.extra_body_damage > 0.0f;
}

} } // namespace Bots::Priority
