#include <Server/Account/AccountLevel.hh>
#ifndef WASM_SERVER
#include <Server/AuthDB.hh>
#else
#include <Server/Account/WasmAccountStore.hh>
#endif
#include <Shared/StaticData.hh>

namespace AccountLevel {
    static uint32_t account_level_score_to_pass(uint32_t level) {
        if (level >= MAX_LEVEL) return 0xffffffffu; // effectively infinite
        // Slower than in-game: reuse in-game curve but scale up 2x
        uint32_t base = score_to_pass_level(level);
        return base * 2u;
    }
    
    uint32_t get_xp_needed_for_next(uint32_t level) {
        return account_level_score_to_pass(level);
    }

#ifndef WASM_SERVER
    bool add_xp(const std::string &account_id, uint32_t xp) {
        return AuthDB::add_account_xp(account_id, (int)xp);
    }

    bool get_level_and_xp(const std::string &account_id, uint32_t &level_out, uint32_t &xp_out) {
        int total_xp = 0;
        if (!AuthDB::get_account_xp(account_id, total_xp)) return false;
        uint32_t level = 1;
        int remaining = total_xp;
        while (level < MAX_LEVEL) {
            uint32_t need = account_level_score_to_pass(level);
            if ((uint32_t)remaining < need) break;
            remaining -= need;
            ++level;
        }
        level_out = level;
        xp_out = (uint32_t)remaining;
        return true;
    }
#else
    bool add_xp(const std::string &account_id, uint32_t xp) {
        return WasmAccountStore::add_xp(account_id, xp);
    }

    bool get_level_and_xp(const std::string &account_id, uint32_t &level_out, uint32_t &xp_out) {
        uint32_t total_xp = 0;
        WasmAccountStore::get_xp(account_id, total_xp);
        uint32_t level = 1;
        uint32_t remaining = total_xp;
        while (level < MAX_LEVEL) {
            uint32_t need = account_level_score_to_pass(level);
            if (remaining < need) break;
            remaining -= need;
            ++level;
        }
        level_out = level;
        xp_out = remaining;
        return true;
    }
#endif
}
