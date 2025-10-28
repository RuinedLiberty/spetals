#pragma once

#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

namespace Bots {
namespace Priority {

struct Context {
    Simulation *sim;
    Entity &camera;
    Entity &player;
    // camera-space info
    float half_w;
    float half_h;
    float cx;
    float cy;
};

struct Decision {
    enum Type { None, Attack, Loot, Rearrange, SeekHealMob, Wander } type = None;
    EntityID target = NULL_ENTITY; // mob or drop id depending on type
    float score = 0.0f;
};

// Health ratios for behavior
constexpr float LOW_HP  = 0.45f; // below this, seek/equip heals
constexpr float FULL_HP = 0.95f; // above this, revert heals to secondary

// Compute the best next action for a bot given the current world state
Decision evaluate(Context const &ctx);

// Carry out inventory rearrangement policy:
// - If low HP and healing is present in secondary, bring it to main (now probabilistically multiple)
// - When healthy, move pure-heal petals to secondary, prefer replacing them with
//   petals whose damage strictly exceeds the heal’s damage (Rose ≈ 5).
//   Then probabilistically trash excess pure-heals in secondary.
void apply_rearrange(Context &ctx);

// Utilities
float rarity_sum_main(Entity const &player);

// "Heals-only" (heals regardless of also dealing damage) – used for low-HP acquisition/equip logic.
bool is_heal_only(PetalID::T id);

// Strictly "petals that deal damage" (projectiles/melee/body/anything that can harm)
bool is_damage_petal(PetalID::T id);

} // namespace Priority
} // namespace Bots
