#pragma once

#include <Shared/EntityDef.hh>
#include <string>

namespace AccountLink {
    void map_camera(const EntityID &camera_id, const std::string &account_id);
    void unmap_camera(const EntityID &camera_id);

    void map_player(const EntityID &player_id, const std::string &account_id);
    void unmap_player(const EntityID &player_id);

    std::string get_account_for_entity(const EntityID &entity_id);
}
