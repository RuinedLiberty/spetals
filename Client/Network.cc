#include <Client/Game.hh>

#include <Client/Input.hh>
#include <Client/Ui/Ui.hh>
#include <Client/Debug.hh>

#include <Shared/Binary.hh>
#include <Shared/Config.hh>

using namespace Game;

void Game::on_message(uint8_t *ptr, uint32_t len) {
    Reader reader(ptr);
        switch(reader.read<uint8_t>()) {
        case Clientbound::kClientUpdate: {
            simulation_ready = 1;
            camera_id = reader.read<EntityID>();
            EntityID curr_id = reader.read<EntityID>();
            while(!(curr_id == NULL_ENTITY)) {
                assert(simulation.ent_exists(curr_id));
                Entity &ent = simulation.get_ent(curr_id);
                simulation._delete_ent(curr_id);
                curr_id = reader.read<EntityID>();
            }
            curr_id = reader.read<EntityID>();
            while(!(curr_id == NULL_ENTITY)) {
                uint8_t create = reader.read<uint8_t>();
                if (BitMath::at(create, 0)) simulation.force_alloc_ent(curr_id);
                assert(simulation.ent_exists(curr_id));
                Entity &ent = simulation.get_ent(curr_id);
                ent.read(&reader, BitMath::at(create, 0));
                if (BitMath::at(create, 1)) ent.pending_delete = 1;
                curr_id = reader.read<EntityID>();
            }
            simulation.arena_info.read(&reader, reader.read<uint8_t>());
            break;
        }
        case Clientbound::kMobGallery: {
            uint32_t bytes = reader.read<uint32_t>();
            for (uint32_t i=0;i<bytes;++i) {
                uint8_t b = reader.read<uint8_t>();
                for (uint32_t bit=0; bit<8; ++bit) {
                    uint32_t idx = i*8 + bit;
                    if (idx >= MobID::kNumMobs) break;
                    Game::seen_mobs[idx] = ((b >> bit) & 1) ? 1 : 0;
                }
            }
            break;
        }
                case Clientbound::kPetalGallery: {
            uint32_t bytes = reader.read<uint32_t>();
            for (uint32_t i=0;i<bytes;++i) {
                uint8_t b = reader.read<uint8_t>();
                for (uint32_t bit=0; bit<8; ++bit) {
                    uint32_t idx = i*8 + bit;
                    if (idx >= PetalID::kNumPetals) break;
                    Game::seen_petals[idx] = ((b >> bit) & 1) ? 1 : 0;
                }
            }
            // Ensure basic is always marked seen
            Game::seen_petals[PetalID::kBasic] = 1;
            break;
        }
                case Clientbound::kAccountLevel: {
            uint32_t lvl = reader.read<uint32_t>();
            uint32_t xp = reader.read<uint32_t>();
            Game::account_level = lvl;
            Game::account_xp = xp;
            break;
        }
        case Clientbound::kAccountLevelBar: {
            uint32_t lvl = reader.read<uint32_t>();
            uint32_t xp = reader.read<uint32_t>();
            uint32_t need = reader.read<uint32_t>();
            Game::account_level = lvl;
            Game::account_xp = xp;
            Game::account_xp_needed = need;
            break;
        }



        case Clientbound::kPingReply: {
            uint64_t sent = reader.read<uint64_t>();
            double now = Debug::get_timestamp();
            double rtt = now - (double)sent;
            Debug::ping_times.push_back(rtt);
            break;
        }
        default:
            break;
    }
}



void Game::send_inputs() {
    static double last_ping = 0;
    double now = Debug::get_timestamp();
    if (now - last_ping > 1000.0) {
        Writer pw(static_cast<uint8_t *>(OUTGOING_PACKET));
        pw.write<uint8_t>(Serverbound::kPing);
        pw.write<uint64_t>((uint64_t)now);
        socket.send(pw.packet, pw.at - pw.packet);
        last_ping = now;
    }

    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    writer.write<uint8_t>(Serverbound::kClientInput);

    if (Input::freeze_input) {
        writer.write<float>(0);
        writer.write<float>(0);
        writer.write<uint8_t>(0);
    } else {
        writer.write<float>(Input::game_inputs.x);
        writer.write<float>(Input::game_inputs.y);
        writer.write<uint8_t>(Input::game_inputs.flags);
    }
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::spawn_in() {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (Game::alive()) return;
    if (Game::on_game_screen == 0) {
        writer.write<uint8_t>(Serverbound::kClientSpawn);
        std::string name = Game::nickname;
        writer.write<std::string>(name);
        socket.send(writer.packet, writer.at - writer.packet);
    } else Game::on_game_screen = 0;
}

void Game::delete_petal(uint8_t pos) {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (!Game::alive()) return;
    writer.write<uint8_t>(Serverbound::kPetalDelete);
    writer.write<uint8_t>(pos);
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::swap_petals(uint8_t pos1, uint8_t pos2) {
    Writer writer(static_cast<uint8_t *>(OUTGOING_PACKET));
    if (!Game::alive()) return;
    writer.write<uint8_t>(Serverbound::kPetalSwap);
    writer.write<uint8_t>(pos1);
    writer.write<uint8_t>(pos2);
    socket.send(writer.packet, writer.at - writer.packet);
}

void Game::swap_all_petals() {
    if (!Game::alive()) return;
    for (uint32_t i = 0; i < Game::loadout_count; ++i)
        Ui::ui_swap_petals(i, i + Game::loadout_count);
}