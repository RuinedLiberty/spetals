#include <Server/Account/WasmAccountStore.hh>

#include <unordered_map>
#include <mutex>
#include <vector>
#include <utility>
#ifdef WASM_SERVER
#include <emscripten.h>
#endif

namespace {
    // key: (category, account_id)
    struct Key {
        uint8_t cat;
        std::string account_id;
        bool operator==(Key const &o) const noexcept {
            return cat == o.cat && account_id == o.account_id;
        }
    };
    struct KeyHash {
        size_t operator()(Key const &k) const noexcept {
            std::hash<std::string> H;
            return (size_t)k.cat * 1315423911u ^ H(k.account_id);
        }
    };

    std::unordered_map<Key, std::vector<uint8_t>, KeyHash> g_bits;
    std::unordered_map<std::string, uint32_t> g_account_xp;
    std::mutex g_mu;
}

namespace WasmAccountStore {

static void ensure_bits(std::vector<uint8_t> &bits, size_t n) {
    size_t need = (n + 7) / 8;
    if (bits.size() != need) bits.assign(need, 0);
}

bool get_bits(Category category, const std::string &account_id, std::vector<uint8_t> &bits) {
    std::lock_guard<std::mutex> lk(g_mu);
    Key key{ (uint8_t)category, account_id };
    auto it = g_bits.find(key);
    if (it == g_bits.end()) { bits.clear(); return false; }
    bits = it->second;
    return true;
}

bool set_bit(Category category, const std::string &account_id, int id) {
    if (account_id.empty() || id < 0) return false;
    std::lock_guard<std::mutex> lk(g_mu);
    Key key{ (uint8_t)category, account_id };
    auto &bits = g_bits[key];
    ensure_bits(bits, 256);
    size_t idx = (size_t)id;
    size_t byteIndex = idx >> 3;
    size_t bitIndex = idx & 7u;
    if (byteIndex >= bits.size()) bits.resize(byteIndex + 1, 0);
    bits[byteIndex] = (uint8_t)(bits[byteIndex] | (1u << bitIndex));
    return true;
}

bool get_xp(const std::string &account_id, uint32_t &xp_out) {
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_account_xp.find(account_id);
    if (it == g_account_xp.end()) { xp_out = 0; return false; }
    xp_out = it->second;
    return true;
}

bool set_xp(const std::string &account_id, uint32_t xp) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_account_xp[account_id] = xp;
    return true;
}

bool add_xp(const std::string &account_id, uint32_t delta) {
    std::lock_guard<std::mutex> lk(g_mu);
    uint32_t &ref = g_account_xp[account_id];
    ref += delta;
    return true;
}

bool get_top_account(std::string &account_id_out) {
    account_id_out.clear();
#ifdef WASM_SERVER
    // Prefer authoritative DB-backed top account exposed by the Node host (Module.topAccount)
    int len = EM_ASM_INT({
        var s = (Module.topAccount || "");
        return lengthBytesUTF8(s) + 1;
    });
    if (len > 1 && len < 256) {
        std::vector<char> buf((size_t)len);
        EM_ASM({
            var s = (Module.topAccount || "");
            stringToUTF8(s, $0, $1);
        }, buf.data(), len);
        account_id_out.assign(buf.data());
        if (!account_id_out.empty()) return true;
    }
#endif
    // Fallback: compute from in-memory map (only covers accounts seen this process lifetime)
    std::lock_guard<std::mutex> lk(g_mu);
    uint32_t best = 0; bool have = false;
    for (auto const &kv : g_account_xp) {
        if (!have || kv.second > best) { best = kv.second; account_id_out = kv.first; have = true; }
    }
    return have;
}

} // namespace WasmAccountStore
