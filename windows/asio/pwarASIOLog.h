#pragma once
#include <string>

class pwarASIOLog {
public:
    pwarASIOLog(const char* ip, int port);
    static void Init();
    static void Shutdown();
    static void Send(const char* msg);
private:
    static void EnsureInit();
};
