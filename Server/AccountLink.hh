#pragma once

#include <Shared/EntityDef.hh>
#include <string>

namespace AccountLink {
    // Map camera entity to account id (UUID string)
    void map_camera(const EntityID &camera_id, const std::string &account_id);
    void unmap_camera(const EntityID &camera_id);

    // Map player/base entity id to account id
    void map_player(const EntityID &player_id, const std::string &account_id);
    void unmap_player(const EntityID &player_id);

    // Lookup account id for a given entity (camera or player). Returns empty string if none.
    std::string get_account_for_entity(const EntityID &entity_id);
}
