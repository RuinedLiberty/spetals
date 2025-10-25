#pragma once

#include <cstddef>

class Client; // forward declaration

// Per-socket data kept by uWebSockets
// Trivially copyable. We allocate Client on heap and keep a pointer here.
struct PerSocketData {
    // UUID string (36 chars + null). Empty string means unauthenticated/unknown.
    char account_id[37];
    Client* client;
};
