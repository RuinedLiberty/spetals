#include <Server/Account/WasmAccountStore.hh>

#include <unordered_map>
#include <mutex>
#include <vector>
#include <utility>

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

} // namespace WasmAccountStore
