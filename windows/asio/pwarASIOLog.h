/*
 * pwarASIOLog.h - PipeWire ASIO Relay (PWAR) project
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

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
