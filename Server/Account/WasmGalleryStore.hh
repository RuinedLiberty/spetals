#pragma once

#include <string>
#include <vector>

namespace WasmGalleryStore {
    bool get_gallery_bits(const std::string &account_id, std::vector<uint8_t> &bits);
    bool record_kill(const std::string &account_id, int mob_id);
}
