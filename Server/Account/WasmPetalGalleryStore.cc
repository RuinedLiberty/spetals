#include <Server/Account/WasmPetalGalleryStore.hh>

#include <unordered_map>
#include <mutex>
#include <vector>

namespace {
    std::unordered_map<std::string, std::vector<uint8_t>> g_petal_gallery_bits;
    std::mutex g_mu_petal;
}

namespace WasmPetalGalleryStore {

static void ensure_bits(std::vector<uint8_t> &bits, size_t n_petals) {
    size_t need = (n_petals + 7) / 8;
    if (bits.size() != need) bits.assign(need, 0);
}

bool get_gallery_bits(const std::string &account_id, std::vector<uint8_t> &bits) {
    std::lock_guard<std::mutex> lk(g_mu_petal);
    auto it = g_petal_gallery_bits.find(account_id);
    if (it == g_petal_gallery_bits.end()) {
        bits.clear();
        return false;
    }
    bits = it->second;
    return true;
}

bool record_obtained(const std::string &account_id, int petal_id) {
    if (account_id.empty() || petal_id < 0) return false;
    std::lock_guard<std::mutex> lk(g_mu_petal);
    auto &bits = g_petal_gallery_bits[account_id];
    ensure_bits(bits, 256);
    size_t idx = static_cast<size_t>(petal_id);
    size_t byteIndex = idx >> 3;
    size_t bitIndex = idx & 7u;
    if (byteIndex >= bits.size()) {
        bits.resize(byteIndex + 1, 0);
    }
    bits[byteIndex] = static_cast<uint8_t>(bits[byteIndex] | (1u << bitIndex));
    return true;
}

} // namespace WasmPetalGalleryStore
