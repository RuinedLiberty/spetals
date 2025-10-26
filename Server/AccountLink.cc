#include <Server/AccountLink.hh>

#include <unordered_map>
#include <mutex>

namespace {
    std::unordered_map<uint32_t, std::string> g_entity_to_account; // key by EntityID.id to avoid hashing pair
    std::mutex g_mu;
}

namespace AccountLink {

void map_camera(const EntityID &camera_id, const std::string &account_id) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_entity_to_account[camera_id.id] = account_id;
}

void unmap_camera(const EntityID &camera_id) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_entity_to_account.erase(camera_id.id);
}

void map_player(const EntityID &player_id, const std::string &account_id) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_entity_to_account[player_id.id] = account_id;
}

void unmap_player(const EntityID &player_id) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_entity_to_account.erase(player_id.id);
}

std::string get_account_for_entity(const EntityID &entity_id) {
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_entity_to_account.find(entity_id.id);
    if (it != g_entity_to_account.end()) return it->second;
    return std::string();
}

} // namespace AccountLink
