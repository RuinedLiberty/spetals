#include <Server/Bots/BotManager.hh>
#include <Server/Bots/ForwardShims.hh>

extern "C" void Bots_spawn_all(Simulation *sim, unsigned int count) {
    Bots::spawn_all(sim, count);
}

extern "C" void Bots_on_tick(Simulation *sim) {
    Bots::on_tick(sim);
}
