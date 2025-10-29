#pragma once

#include <Shared/Simulation.hh>

#ifdef __cplusplus
extern "C" {
#endif
    void Bots_spawn_all(Simulation *sim, unsigned int count);
    void Bots_on_tick(Simulation *sim);
#ifdef __cplusplus
}
#endif
