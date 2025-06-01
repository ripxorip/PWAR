/*
 * pwarASIO.cpp - PipeWire ASIO Relay (PWAR) project
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <fstream>
#include <sstream>
#include "pwarASIO.h"
#include "pwarASIOLog.h"
#include "../../protocol/pwar_packet.h"
#pragma comment(lib, "ws2_32.lib")

static constexpr double TWO_RAISED_TO_32 = 4294967296.0;
static constexpr double TWO_RAISED_TO_32_RECIP = 1.0 / TWO_RAISED_TO_32;

CLSID IID_ASIO_DRIVER = { 0x188135e1, 0xd565, 0x11d2, { 0x85, 0x4f, 0x0, 0xa0, 0xc9, 0x9f, 0x5d, 0x19 } };

CFactoryTemplate g_Templates[1] = {
    {L"PWARASIO", &IID_ASIO_DRIVER, pwarASIO::CreateInstance}
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

CUnknown* pwarASIO::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr) {
    return new pwarASIO(pUnk, phr);
}

STDMETHODIMP pwarASIO::NonDelegatingQueryInterface(REFIID riid, void** ppv) {
    if (riid == IID_ASIO_DRIVER) {
        return GetInterface(this, ppv);
    }
    return CUnknown::NonDelegatingQueryInterface(riid, ppv);
}

extern LONG RegisterAsioDriver(CLSID, char*, char*, char*, char*);
extern LONG UnregisterAsioDriver(CLSID, char*, char*);

HRESULT _stdcall DllRegisterServer() {
    LONG rc = RegisterAsioDriver(IID_ASIO_DRIVER, "PWARASIO.dll", "PWAR ASIO Driver", "PWAR ASIO", "Apartment");
    if (rc) {
        char errstr[128] = {0};
        sprintf(errstr, "Register Server failed ! (%d)", rc);
        MessageBoxA(nullptr, errstr, "ASIO sample Driver", MB_OK);
        return -1;
    }
    return S_OK;
}

HRESULT _stdcall DllUnregisterServer() {
    LONG rc = UnregisterAsioDriver(IID_ASIO_DRIVER, "PWARASIO.dll", "PWAR ASIO Driver");
    if (rc) {
        char errstr[128] = {0};
        sprintf(errstr, "Unregister Server failed ! (%d)", rc);
        MessageBoxA(nullptr, errstr, "ASIO Korg1212 I/O Driver", MB_OK);
        return -1;
    }
    return S_OK;
}

pwarASIO::pwarASIO(LPUNKNOWN pUnk, HRESULT* phr)
    : CUnknown("PWARASIO", pUnk, phr),
      samplePosition(0),
      sampleRate(48000.0),
      callbacks(nullptr),
      blockFrames(kBlockFrames),
      inputLatency(kBlockFrames),
      outputLatency(kBlockFrames * 2),
      activeInputs(0),
      activeOutputs(0),
      toggle(0),
      milliSeconds(static_cast<long>((kBlockFrames * 1000) / 48000.0)),
      active(false),
      started(false),
      timeInfoMode(false),
      tcRead(false),
      _timestamp(0),
      udpListenerRunning(false),
      udpSendSocket(INVALID_SOCKET),
      udpWSAInitialized(false),
      udpSendIp("192.168.66.2")
{
    for (long i = 0; i < kNumInputs; ++i) {
        inputBuffers[i] = nullptr;
        inMap[i] = 0;
    }
    for (long i = 0; i < kNumOutputs; ++i) {
        outputBuffers[i] = nullptr;
        outMap[i] = 0;
    }
    callbacks = nullptr;
    parseConfigFile();
    initUdpSender();
    startUdpListener();
}

pwarASIO::~pwarASIO() {
    closeUdpSender();
    stopUdpListener();
    stop();
    disposeBuffers();
}

void pwarASIO::getDriverName(char* name) {
    strcpy(name, "PWAR ASIO Driver");
}

long pwarASIO::getDriverVersion() {
    return 0x00000001L;
}

void pwarASIO::getErrorMessage(char* string) {
    strcpy(string, errorMessage);
}

ASIOBool pwarASIO::init(void* sysRef) {
    (void)sysRef;
    return true;
}

ASIOError pwarASIO::start() {
    if (!callbacks) return ASE_NotPresent;
    started = false;
    samplePosition = 0;
    theSystemTime.lo = theSystemTime.hi = 0;
    toggle = 0;
    started = true;
    return ASE_OK;
}

ASIOError pwarASIO::stop() {
    started = false;
    return ASE_OK;
}

ASIOError pwarASIO::getChannels(long* numInputChannels, long* numOutputChannels) {
    *numInputChannels = kNumInputs;
    *numOutputChannels = kNumOutputs;
    return ASE_OK;
}

ASIOError pwarASIO::getLatencies(long* _inputLatency, long* _outputLatency) {
    *_inputLatency = blockFrames;
    *_outputLatency = blockFrames * 2;
    return ASE_OK;
}

ASIOError pwarASIO::getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) {
    *minSize = *maxSize = *preferredSize = kBlockFrames;
    *granularity = 0;
    return ASE_OK;
}

ASIOError pwarASIO::canSampleRate(ASIOSampleRate sampleRate) {
    return (sampleRate == 44100.0 || sampleRate == 48000.0) ? ASE_OK : ASE_NoClock;
}

ASIOError pwarASIO::getSampleRate(ASIOSampleRate* sampleRate) {
    *sampleRate = this->sampleRate;
    return ASE_OK;
}

ASIOError pwarASIO::setSampleRate(ASIOSampleRate sampleRate) {
    if (sampleRate != 44100.0 && sampleRate != 48000.0) return ASE_NoClock;
    if (sampleRate != this->sampleRate) {
        this->sampleRate = sampleRate;
        asioTime.timeInfo.sampleRate = sampleRate;
        asioTime.timeInfo.flags |= kSampleRateChanged;
        milliSeconds = static_cast<long>((kBlockFrames * 1000) / this->sampleRate);
        if (callbacks && callbacks->sampleRateDidChange)
            callbacks->sampleRateDidChange(this->sampleRate);
    }
    return ASE_OK;
}

ASIOError pwarASIO::getClockSources(ASIOClockSource* clocks, long* numSources) {
    clocks->index = 0;
    clocks->associatedChannel = -1;
    clocks->associatedGroup = -1;
    clocks->isCurrentSource = ASIOTrue;
    strcpy(clocks->name, "Internal");
    *numSources = 1;
    return ASE_OK;
}

ASIOError pwarASIO::setClockSource(long index) {
    if (index == 0) {
        asioTime.timeInfo.flags |= kClockSourceChanged;
        return ASE_OK;
    }
    return ASE_NotPresent;
}

ASIOError pwarASIO::getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) {
    tStamp->lo = theSystemTime.lo;
    tStamp->hi = theSystemTime.hi;
    if (samplePosition >= TWO_RAISED_TO_32) {
        sPos->hi = static_cast<unsigned long>(samplePosition * TWO_RAISED_TO_32_RECIP);
        sPos->lo = static_cast<unsigned long>(samplePosition - (sPos->hi * TWO_RAISED_TO_32));
    } else {
        sPos->hi = 0;
        sPos->lo = static_cast<unsigned long>(samplePosition);
    }
    return ASE_OK;
}

ASIOError pwarASIO::getChannelInfo(ASIOChannelInfo* info) {
    if (info->channel < 0 || (info->isInput ? info->channel >= kNumInputs : info->channel >= kNumOutputs))
        return ASE_InvalidParameter;
    info->type = ASIOSTFloat32LSB;
    info->channelGroup = 0;
    info->isActive = ASIOFalse;
    if (info->isInput) {
        strcpy(info->name, "Input ");
        for (long i = 0; i < activeInputs; ++i) {
            if (inMap[i] == info->channel) {
                info->isActive = ASIOTrue;
                break;
            }
        }
    } else {
        strcpy(info->name, "Output ");
        for (long i = 0; i < activeOutputs; ++i) {
            if (outMap[i] == info->channel) {
                info->isActive = ASIOTrue;
                break;
            }
        }
    }
    return ASE_OK;
}

ASIOError pwarASIO::createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) {
    ASIOBufferInfo* info = bufferInfos;
    bool notEnoughMem = false;
    activeInputs = 0;
    activeOutputs = 0;
    blockFrames = bufferSize;
    for (long i = 0; i < numChannels; ++i, ++info) {
        if (info->isInput) {
            if (info->channelNum < 0 || info->channelNum >= kNumInputs)
                goto error;
            inMap[activeInputs] = info->channelNum;
            inputBuffers[activeInputs] = new float[blockFrames * 2];
            if (inputBuffers[activeInputs]) {
                info->buffers[0] = inputBuffers[activeInputs];
                info->buffers[1] = inputBuffers[activeInputs] + blockFrames;
            } else {
                info->buffers[0] = info->buffers[1] = nullptr;
                notEnoughMem = true;
            }
            ++activeInputs;
            if (activeInputs > kNumInputs) {
error:
                disposeBuffers();
                return ASE_InvalidParameter;
            }
        } else {
            if (info->channelNum < 0 || info->channelNum >= kNumOutputs)
                goto error;
            outMap[activeOutputs] = info->channelNum;
            outputBuffers[activeOutputs] = new float[blockFrames * 2];
            if (outputBuffers[activeOutputs]) {
                info->buffers[0] = outputBuffers[activeOutputs];
                info->buffers[1] = outputBuffers[activeOutputs] + blockFrames;
            } else {
                info->buffers[0] = info->buffers[1] = nullptr;
                notEnoughMem = true;
            }
            ++activeOutputs;
            if (activeOutputs > kNumOutputs) {
                --activeOutputs;
                disposeBuffers();
                return ASE_InvalidParameter;
            }
        }
    }
    if (notEnoughMem) {
        disposeBuffers();
        return ASE_NoMemory;
    }
    this->callbacks = callbacks;
    if (callbacks->asioMessage(kAsioSupportsTimeInfo, 0, 0, 0)) {
        timeInfoMode = true;
        asioTime.timeInfo.speed = 1.0;
        asioTime.timeInfo.systemTime.hi = asioTime.timeInfo.systemTime.lo = 0;
        asioTime.timeInfo.samplePosition.hi = asioTime.timeInfo.samplePosition.lo = 0;
        asioTime.timeInfo.sampleRate = sampleRate;
        asioTime.timeInfo.flags = kSystemTimeValid | kSamplePositionValid | kSampleRateValid;
        asioTime.timeCode.speed = 1.0;
        asioTime.timeCode.timeCodeSamples.lo = asioTime.timeCode.timeCodeSamples.hi = 0;
        asioTime.timeCode.flags = kTcValid | kTcRunning;
    } else {
        timeInfoMode = false;
    }
    return ASE_OK;
}

ASIOError pwarASIO::disposeBuffers() {
    callbacks = nullptr;
    stop();
    for (long i = 0; i < activeInputs; ++i)
        delete[] inputBuffers[i];
    activeInputs = 0;
    for (long i = 0; i < activeOutputs; ++i)
        delete[] outputBuffers[i];
    activeOutputs = 0;
    return ASE_OK;
}

ASIOError pwarASIO::controlPanel() {
    return ASE_NotPresent;
}

ASIOError pwarASIO::future(long selector, void* opt) {
    switch (selector) {
        case kAsioEnableTimeCodeRead: tcRead = true; return ASE_SUCCESS;
        case kAsioDisableTimeCodeRead: tcRead = false; return ASE_SUCCESS;
        case kAsioSetInputMonitor: return ASE_SUCCESS;
        case kAsioCanInputMonitor: return ASE_SUCCESS;
        case kAsioCanTimeInfo: return ASE_SUCCESS;
        case kAsioCanTimeCode: return ASE_SUCCESS;
    }
    return ASE_NotPresent;
}

void pwarASIO::output(const rt_stream_packet_t& packet) {
    if (udpSendSocket != INVALID_SOCKET) {
        sendto(udpSendSocket, reinterpret_cast<const char*>(&packet), sizeof(rt_stream_packet_t), 0, reinterpret_cast<const sockaddr*>(&udpSendAddr), sizeof(udpSendAddr));
    }
}

void pwarASIO::switchBuffersFromPwarPacket(const rt_stream_packet_t& packet) {
    size_t to_copy = (blockFrames < packet.n_samples) ? blockFrames : packet.n_samples;
    for (long i = 0; i < activeInputs; ++i) {
        float* dest = inputBuffers[i] + (toggle ? blockFrames : 0);
        memcpy(dest, packet.samples_ch1, to_copy * sizeof(float));
        for (size_t j = to_copy; j < blockFrames; ++j)
            dest[j] = 0.0f;
    }
    rt_stream_packet_t out_packet;
    out_packet.ts_pipewire_send = packet.ts_pipewire_send;
    samplePosition += blockFrames;
    if (timeInfoMode) {
        bufferSwitchX();
    } else {
        callbacks->bufferSwitch(toggle, ASIOFalse);
    }
    float* outputSamplesCh1 = outputBuffers[0] + (toggle ? blockFrames : 0);
    float* outputSamplesCh2 = outputBuffers[1] + (toggle ? blockFrames : 0);
    out_packet.n_samples = blockFrames;
    memcpy(out_packet.samples_ch1, outputSamplesCh1, out_packet.n_samples * sizeof(float));
    memcpy(out_packet.samples_ch2, outputSamplesCh2, out_packet.n_samples * sizeof(float));
    out_packet.seq = packet.seq;
    uint64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    out_packet.ts_asio_send = (nowNs - _timestamp) + packet.ts_pipewire_send;
    output(out_packet);
    toggle = toggle ? 0 : 1;
}

void pwarASIO::bufferSwitchX() {
    getSamplePosition(&asioTime.timeInfo.samplePosition, &asioTime.timeInfo.systemTime);
    if (tcRead) {
        asioTime.timeCode.timeCodeSamples.lo = asioTime.timeInfo.samplePosition.lo + static_cast<unsigned long>(600.0 * sampleRate);
        asioTime.timeCode.timeCodeSamples.hi = 0;
    }
    callbacks->bufferSwitchTimeInfo(&asioTime, toggle, ASIOFalse);
    asioTime.timeInfo.flags &= ~(kSampleRateChanged | kClockSourceChanged);
}

ASIOError pwarASIO::outputReady() {
    return ASE_NotPresent;
}

void pwarASIO::udp_packet_listener() {
    WSADATA wsaData;
    SOCKET sockfd;
    sockaddr_in servaddr{}, cliaddr{};
    int n;
    socklen_t len;
    char buffer[2048];
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        return;
    }
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        WSACleanup();
        return;
    }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(8321);
    if (bind(sockfd, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) == SOCKET_ERROR) {
        closesocket(sockfd);
        WSACleanup();
        return;
    }
    udpListenerRunning = true;
    while (udpListenerRunning) {
        len = sizeof(cliaddr);
        n = recvfrom(sockfd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&cliaddr), &len);
        if (n > 0 && n >= static_cast<int>(sizeof(rt_stream_packet_t))) {
            rt_stream_packet_t pkt;
            memcpy(&pkt, buffer, sizeof(rt_stream_packet_t));
            _timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            if (started) {
                switchBuffersFromPwarPacket(pkt);
            }
        }
    }
    closesocket(sockfd);
    WSACleanup();
}

void pwarASIO::startUdpListener() {
    if (!udpListenerRunning) {
        udpListenerRunning = true;
        udpListenerThread = std::thread(&pwarASIO::udp_packet_listener, this);
    }
}

void pwarASIO::stopUdpListener() {
    udpListenerRunning = false;
    if (udpListenerThread.joinable()) {
        udpListenerThread.join();
    }
}

void pwarASIO::parseConfigFile() {
    std::string configPath;
    const char* home = getenv("USERPROFILE");
    if (home && *home) {
        configPath = std::string(home) + "\\pwarASIO.cfg";
    } else {
        configPath = "pwarASIO.cfg";
    }
    std::ifstream cfg(configPath);
    if (!cfg.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(cfg, line)) {
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            if (key == "udp_send_ip") {
                udpSendIp = value;
                pwarASIOLog::Send("Read ip from config");
            }
        }
    }
}

void pwarASIO::initUdpSender() {
    constexpr int udp_port = 8321;
    if (!udpWSAInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) == 0) {
            udpWSAInitialized = true;
        } else {
            pwarASIOLog::Send("WSAStartup failed in initUdpSender");
            return;
        }
    }
    if (udpSendSocket == INVALID_SOCKET && udpWSAInitialized) {
        udpSendSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udpSendSocket != INVALID_SOCKET) {
            memset(&udpSendAddr, 0, sizeof(udpSendAddr));
            udpSendAddr.sin_family = AF_INET;
            udpSendAddr.sin_port = htons(udp_port);
            inet_pton(AF_INET, udpSendIp.c_str(), &udpSendAddr.sin_addr);
        } else {
            pwarASIOLog::Send("Failed to create UDP send socket");
        }
    }
}

void pwarASIO::closeUdpSender() {
    if (udpSendSocket != INVALID_SOCKET) {
        closesocket(udpSendSocket);
        udpSendSocket = INVALID_SOCKET;
    }
    if (udpWSAInitialized) {
        WSACleanup();
        udpWSAInitialized = false;
    }
}
