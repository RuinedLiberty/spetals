#include <Shared/Config.hh>

extern const uint64_t VERSION_HASH = 19235684321324ull;

extern const uint32_t SERVER_PORT = 9001;
extern const uint32_t MAX_NAME_LENGTH = 16;

// Empty means: auto-detect from window.location (preferred). Override with a full ws:// or wss:// URL to force.
extern std::string const WS_URL = "";