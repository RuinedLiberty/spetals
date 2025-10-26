#pragma once

#include <string>
#include <vector>

namespace GalleryStore {
    // Fill bits with (MobID::kNumMobs + 7)/8 bytes bitmap of seen mobs for this account
    bool get_gallery_bits(const std::string &account_id, std::vector<uint8_t> &bits);
    // Record a kill/seen for this account
    bool record_kill(const std::string &account_id, int mob_id);
}
