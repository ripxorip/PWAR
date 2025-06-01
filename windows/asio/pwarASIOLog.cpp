/*
 * pwarASIOLog.cpp - PipeWire ASIO Relay (PWAR) project
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#include "pwarASIOLog.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
#pragma comment(lib, "ws2_32.lib")

static SOCKET sock = INVALID_SOCKET;
static sockaddr_in serverAddr = {};
static bool initialized = false;
static std::mutex g_udpLogMutex;
static std::string log_ip = "10.0.0.171";
static int log_port = 1338;

pwarASIOLog::pwarASIOLog(const char* ip, int port) {
    std::lock_guard<std::mutex> lock(g_udpLogMutex);
    log_ip = ip;
    log_port = port;
    initialized = false; // force re-init on next send
}

void pwarASIOLog::Init() {
    std::lock_guard<std::mutex> lock(g_udpLogMutex);
    if (initialized) return;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
        return;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return;
    }
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(log_port);
    serverAddr.sin_addr.s_addr = inet_addr(log_ip.c_str());
    initialized = true;
}

void pwarASIOLog::Shutdown() {
    std::lock_guard<std::mutex> lock(g_udpLogMutex);
    if (!initialized) return;
    closesocket(sock);
    WSACleanup();
    sock = INVALID_SOCKET;
    initialized = false;
}

void pwarASIOLog::EnsureInit() {
    if (!initialized) Init();
}

void pwarASIOLog::Send(const char* msg) {
    EnsureInit();
    if (!initialized) return;
    std::lock_guard<std::mutex> lock(g_udpLogMutex);
    std::string out(msg);
    if (out.empty() || out.back() != '\n') out += "\r\n";
    sendto(sock, out.c_str(), (int)out.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
}
