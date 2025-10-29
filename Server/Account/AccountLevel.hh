#pragma once
#include <string>

namespace AccountLevel {
    // Add XP to account and persist. Returns true on success.
    bool add_xp(const std::string &account_id, uint32_t xp);
    // Get current level (1..99) and xp towards next level. Returns true on success.
    bool get_level_and_xp(const std::string &account_id, uint32_t &level_out, uint32_t &xp_out);
    // Get XP needed to reach next level from a given level
    uint32_t get_xp_needed_for_next(uint32_t level);
}
