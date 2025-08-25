// torture_main.cpp
// Simple torture test for socket thread logic
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <avrt.h>
#include <mmsystem.h>
#include "../../protocol/pwar_packet.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "winmm.lib")

std::atomic<bool> running{true};

void udp_receive_thread() {
    // Raise thread priority and register with MMCSS
    // SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    // DWORD mmcssTaskIndex = 0;
    // HANDLE mmcssHandle = AvSetMmThreadCharacteristicsA("Pro Audio", &mmcssTaskIndex);
    // int prio = GetThreadPriority(GetCurrentThread());
    // if (prio != THREAD_PRIORITY_TIME_CRITICAL) {
    //     std::cerr << "Warning: Thread priority not set to TIME_CRITICAL!\n";
    // } else {
    //     std::cout << "Thread priority set to TIME_CRITICAL.\n";
    // }
    // if (!mmcssHandle) {
    //     std::cerr << "Warning: MMCSS registration failed!\n";
    // } else {
    //     std::cout << "MMCSS registration succeeded.\n";
    // }

    WSADATA wsaData;
    SOCKET sockfd;
    sockaddr_in servaddr{}, cliaddr{};
    int n;
    socklen_t len;
    char buffer[2048];

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!\n";
        return;
    }
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!\n";
        WSACleanup();
        return;
    }
    int rcvbuf = 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf));
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(8321);
    if (bind(sockfd, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed!\n";
        closesocket(sockfd);
        WSACleanup();
        return;
    }
    std::cout << "Listening for UDP packets on port 8321...\n";

    // Setup socket for sending responses
    SOCKET send_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(8321);
    inet_pton(AF_INET, "192.168.66.2", &dest_addr.sin_addr);

    while (running) {
        len = sizeof(cliaddr);
        int bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&cliaddr), &len);
        if (bytesReceived >= (int)sizeof(pwar_packet_t)) {
            pwar_packet_t pkt;
            memcpy(&pkt, buffer, sizeof(pwar_packet_t));
            // Respond with the same seq
            pwar_packet_t resp = pkt;
            sendto(send_sock, reinterpret_cast<const char*>(&resp), sizeof(resp), 0,
                   reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
        }
    }
    closesocket(sockfd);
    closesocket(send_sock);
    WSACleanup();
    // if (mmcssHandle) AvRevertMmThreadCharacteristics(mmcssHandle);
}

int main() {
    // Set main thread to highest priority and MMCSS
    // SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    // DWORD mmcssTaskIndex = 0;
    // HANDLE mmcssHandle = AvSetMmThreadCharacteristicsA("Pro Audio", &mmcssTaskIndex);
    // int prio = GetThreadPriority(GetCurrentThread());
    // if (prio != THREAD_PRIORITY_TIME_CRITICAL) {
    //     std::cerr << "[Main] Warning: Thread priority not set to TIME_CRITICAL!\n";
    // } else {
    //     std::cout << "[Main] Thread priority set to TIME_CRITICAL.\n";
    // }
    // if (!mmcssHandle) {
    //     std::cerr << "[Main] Warning: MMCSS registration failed!\n";
    // } else {
    //     std::cout << "[Main] MMCSS registration succeeded.\n";
    // }

    std::thread t(udp_receive_thread);

    std::cout << "Press Enter to quit...\n";
    std::cin.get();
    running = false;
    t.join();
    timeEndPeriod(1); // Restore timer resolution
    // if (mmcssHandle) AvRevertMmThreadCharacteristics(mmcssHandle);
    return 0;
}
