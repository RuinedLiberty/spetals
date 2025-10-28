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
    enum Type { None, Attack, Loot, Rearrange, SeekHealMob } type = None;
    EntityID target = NULL_ENTITY; // mob or drop id depending on type
    float score = 0.0f;
};

// Compute the best next action for a bot given the current world state
Decision evaluate(Context const &ctx);

// Carry out inventory rearrangement policy:
// - If low HP and healing is present in secondary, bring it to main
// - When healthy, move heal-only petals to secondary, promote higher-rarity damage to main, trash basics in secondary
void apply_rearrange(Context &ctx);

// Utilities
float rarity_sum_main(Entity const &player);
bool is_heal_only(PetalID::T id);
bool is_damage_petal(PetalID::T id);

} // namespace Priority
} // namespace Bots
