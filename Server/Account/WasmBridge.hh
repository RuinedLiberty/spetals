#pragma once

#ifdef WASM_SERVER
extern "C" void set_ws_account(int ws_id, const char *account_id);
#endif
