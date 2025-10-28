#include <Server/Bots/Priorities.hh>

#include <cmath>

namespace Bots { namespace Priority {

static inline bool in_fov(Context const &c, Entity const &e) {
    return std::fabs(e.get_x() - c.cx) <= c.half_w && std::fabs(e.get_y() - c.cy) <= c.half_h;
}

Decision evaluate(Context const &ctx) {
    Decision d; // defaults to None, score 0
    // Rule 1: If bot isn't doing anything, attack the closest visible mob, value = 1
    // We always evaluate this independent of other action-priorities. For now it's the only rule.
    EntityID closest = NULL_ENTITY;
    float bestDist2 = 0.0f;
    ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
        if (!sm->ent_alive(e.id)) return;
        if (!e.has_component(kMob)) return;
        if (e.get_team() == ctx.player.get_team()) return;
        if (!in_fov(ctx, e)) return;
        float dx = e.get_x() - ctx.player.get_x();
        float dy = e.get_y() - ctx.player.get_y();
        float d2 = dx*dx + dy*dy;
        if (closest.null() || d2 < bestDist2) { closest = e.id; bestDist2 = d2; }
    });
    if (!closest.null()) { d.type = Decision::Attack; d.target = closest; d.score = 1.0f; }
    return d;
}

// No-op for now
void apply_rearrange(Context &) {}

// Utilities no-ops for now
float rarity_sum_main(Entity const &) { return 0.0f; }
bool is_heal_only(PetalID::T) { return false; }
bool is_damage_petal(PetalID::T) { return false; }

} } // namespace Bots::Priority
