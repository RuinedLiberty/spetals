#pragma once

#include <Shared/EntityDef.hh>
#include <Server/Bots/Priorities.hh>
#include <string>
#include <vector>

class Simulation;
class Entity;

namespace Bots {

// Lightweight control output for a bot this tick
struct Control {
    float ax = 0.0f; // desired acceleration x
    float ay = 0.0f; // desired acceleration y
    uint8_t flags = 0; // InputFlags bitfield
};

// Spawn BOT_COUNT bots and initialize internal state
void spawn_all(Simulation *sim, uint32_t count);

// Maintain bot lifecycle (respawn, cleanup, etc.)
void on_tick(Simulation *sim);

// Compute human-like controls for the CPU-controlled camera's player based on a priority decision
void compute_controls(Simulation *sim, Entity &camera, Bots::Priority::Decision const &dec, Control &out_ctrl);

// Reposition all bot players and cameras near a point (for testing/visibility)
void focus_all_to(Simulation *sim, float x, float y, float radius);

} // namespace Bots
