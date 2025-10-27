#pragma once

#include <string>
#include <vector>

namespace WasmPetalGalleryStore {
    bool get_gallery_bits(const std::string &account_id, std::vector<uint8_t> &bits);
    bool record_obtained(const std::string &account_id, int petal_id);
}
