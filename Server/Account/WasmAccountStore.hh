#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace WasmAccountStore {
    enum class Category : uint8_t {
        MobGallery = 0,
        PetalGallery = 1
    };

    // Reads the bitset for given account and category. Returns true if any bits were found.
    bool get_bits(Category category, const std::string &account_id, std::vector<uint8_t> &bits);

    // Sets the bit for the given id under account+category. Returns true on success.
    bool set_bit(Category category, const std::string &account_id, int id);

    // Account XP helpers for WASM server runtime (in-memory, DB-backed via JS bridge)
    bool get_xp(const std::string &account_id, uint32_t &xp_out);
    bool set_xp(const std::string &account_id, uint32_t xp);
    bool add_xp(const std::string &account_id, uint32_t delta);
}
