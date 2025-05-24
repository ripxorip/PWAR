#pragma once
#include <string>

class pwarASIOLog {
public:
    pwarASIOLog(const char* ip, int port);
    // Call once before any logging (optional, will auto-init on first Send)
    static void Init();
    // Call once at shutdown (optional, will auto-cleanup on process exit)
    static void Shutdown();
    // Thread-safe UDP log send
    static void Send(const char* msg);
private:
    static void EnsureInit();
};
