#include <Server/Account/WasmGalleryStore.hh>

#include <unordered_map>
#include <mutex>
#include <vector>

namespace {
    std::unordered_map<std::string, std::vector<uint8_t>> g_gallery_bits;
    std::mutex g_mu;
}

namespace WasmGalleryStore {

static void ensure_bits(std::vector<uint8_t> &bits, size_t n_mobs) {
    size_t need = (n_mobs + 7) / 8;
    if (bits.size() != need) bits.assign(need, 0);
}

bool get_gallery_bits(const std::string &account_id, std::vector<uint8_t> &bits) {
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_gallery_bits.find(account_id);
    if (it == g_gallery_bits.end()) {
        bits.clear();
        return false;
    }
    bits = it->second;
    return true;
}

bool record_kill(const std::string &account_id, int mob_id) {
    if (account_id.empty() || mob_id < 0) return false;
    std::lock_guard<std::mutex> lk(g_mu);
    auto &bits = g_gallery_bits[account_id];
    ensure_bits(bits, 256);
    size_t idx = static_cast<size_t>(mob_id);
    size_t byteIndex = idx >> 3;
    size_t bitIndex = idx & 7u;
    if (byteIndex >= bits.size()) {
        bits.resize(byteIndex + 1, 0);
    }
    bits[byteIndex] = static_cast<uint8_t>(bits[byteIndex] | (1u << bitIndex));
    return true;
}

} // namespace WasmGalleryStore
