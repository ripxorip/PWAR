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
#include "../../protocol/pwar_router.h"
#include "../../protocol/latency_manager.h"

#include <avrt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "avrt.lib")

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#endif

#define PWAR_MAX_CHANNELS 2

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
      blockFrames(kDefaultBlockFrames),
      inputLatency(kDefaultBlockFrames),
      outputLatency(kDefaultBlockFrames * 2),
      activeInputs(0),
      activeOutputs(0),
      toggle(0),
      milliSeconds(static_cast<long>((kDefaultBlockFrames * 1000) / 48000.0)),
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
    // Initialize buffer pointers to null
    for (long i = 0; i < kNumInputs * 2; ++i) {
        inputBuffers[i] = nullptr;
    }
    for (long i = 0; i < kNumOutputs * 2; ++i) {
        outputBuffers[i] = nullptr;
    }
    for (long i = 0; i < kNumInputs; ++i) {
        inMap[i] = 0;
    }
    for (long i = 0; i < kNumOutputs; ++i) {
        outMap[i] = 0;
    }
    
    // Initialize internal buffers to null
    input_buffers = nullptr;
    output_buffers = nullptr;
    
    callbacks = nullptr;
    strcpy(errorMessage, "No error");
    
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
    (void)sysRef;  // Unused parameter
    
    // Verify that our constants are valid
    if (kMinBlockFrames <= 0 || kMaxBlockFrames <= kMinBlockFrames || 
        kBlockFramesGranularity <= 0 || (kMinBlockFrames % kBlockFramesGranularity) != 0) {
        strcpy(errorMessage, "Invalid buffer size constants");
        return ASIOFalse;
    }
    
    strcpy(errorMessage, "Driver initialized successfully");
    return ASIOTrue;
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
    if (!numInputChannels || !numOutputChannels) {
        strcpy(errorMessage, "Null pointer passed to getChannels");
        return ASE_InvalidParameter;
    }
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
    if (!minSize || !maxSize || !preferredSize || !granularity) {
        strcpy(errorMessage, "Null pointer passed to getBufferSize");
        return ASE_InvalidParameter;
    }
    *minSize = kMinBlockFrames;
    *maxSize = kMaxBlockFrames;
    *preferredSize = kDefaultBlockFrames;
    *granularity = kBlockFramesGranularity;
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
        milliSeconds = static_cast<long>((blockFrames * 1000) / this->sampleRate);
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
    if (!info) {
        strcpy(errorMessage, "Null pointer passed to getChannelInfo");
        return ASE_InvalidParameter;
    }
    
    if (info->channel < 0 || (info->isInput ? info->channel >= kNumInputs : info->channel >= kNumOutputs)) {
        strcpy(errorMessage, "Channel index out of range");
        return ASE_InvalidParameter;
    }
    
    info->type = ASIOSTFloat32LSB;
    info->channelGroup = 0;
    info->isActive = ASIOFalse;
    
    if (info->isInput) {
        sprintf(info->name, "Input %ld", info->channel + 1);
        for (long i = 0; i < activeInputs; ++i) {
            if (inMap[i] == info->channel) {
                info->isActive = ASIOTrue;
                break;
            }
        }
    } else {
        sprintf(info->name, "Output %ld", info->channel + 1);
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
    if (!bufferInfos || !callbacks) {
        strcpy(errorMessage, "Null pointer passed to createBuffers");
        return ASE_InvalidParameter;
    }
    
    if (numChannels <= 0 || numChannels > (kNumInputs + kNumOutputs)) {
        strcpy(errorMessage, "Invalid number of channels");
        return ASE_InvalidParameter;
    }
    
    ASIOBufferInfo* info = bufferInfos;
    bool notEnoughMem = false;
    
    // Validate buffer size
    if (bufferSize < kMinBlockFrames || bufferSize > kMaxBlockFrames) {
        strcpy(errorMessage, "Buffer size out of supported range");
        return ASE_InvalidParameter;
    }
    
    // Check if buffer size is aligned to granularity
    if ((bufferSize % kBlockFramesGranularity) != 0) {
        strcpy(errorMessage, "Buffer size not aligned to granularity");
        return ASE_InvalidParameter;
    }
    
    activeInputs = 0;
    activeOutputs = 0;
    blockFrames = bufferSize;
    
    // Update latencies and timing based on new buffer size
    inputLatency = blockFrames;
    outputLatency = blockFrames * 2;
    milliSeconds = static_cast<long>((blockFrames * 1000) / sampleRate);
    
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
    // Initialize the router with the number of output channels
    pwar_router_init(&router, PWAR_MAX_CHANNELS);
    input_buffers = new float[PWAR_MAX_CHANNELS * blockFrames];
    output_buffers = new float[PWAR_MAX_CHANNELS * blockFrames];

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
    // Stop audio processing first
    stop();
    
    // Clear callbacks to prevent any more audio callbacks
    callbacks = nullptr;
    
    // Clean up input buffers
    for (long i = 0; i < activeInputs; ++i) {
        if (inputBuffers[i]) {
            delete[] inputBuffers[i];
            inputBuffers[i] = nullptr;
        }
    }
    activeInputs = 0;
    
    // Clean up output buffers
    for (long i = 0; i < activeOutputs; ++i) {
        if (outputBuffers[i]) {
            delete[] outputBuffers[i];
            outputBuffers[i] = nullptr;
        }
    }
    activeOutputs = 0;
    
    // Clean up internal buffers
    if (input_buffers) {
        delete[] input_buffers;
        input_buffers = nullptr;
    }
    if (output_buffers) {
        delete[] output_buffers;
        output_buffers = nullptr;
    }
    
    return ASE_OK;
}

ASIOError pwarASIO::controlPanel() {
    // For now, we don't provide a control panel
    // This could be extended to show buffer size settings
    char msg[256];
    sprintf(msg, "PWAR ASIO Driver v%d.%d.%d\n\n"
                 "Current buffer size: %ld samples\n"
                 "Supported range: %d - %d samples\n"
                 "Granularity: %d samples\n\n"
                 "Buffer size can be changed through your DAW's audio settings.",
                 (getDriverVersion() >> 16) & 0xFF,
                 (getDriverVersion() >> 8) & 0xFF,
                 getDriverVersion() & 0xFF,
                 blockFrames,
                 kMinBlockFrames,
                 kMaxBlockFrames,
                 kBlockFramesGranularity);
    
    MessageBoxA(nullptr, msg, "PWAR ASIO Driver", MB_OK | MB_ICONINFORMATION);
    return ASE_OK;
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

void pwarASIO::output(const pwar_packet_t& packet) {
    if (udpSendSocket != INVALID_SOCKET) {
        WSABUF buffer;
        buffer.buf = reinterpret_cast<CHAR*>(const_cast<pwar_packet_t*>(&packet));
        buffer.len = sizeof(pwar_packet_t);
        DWORD bytesSent = 0;
        int flags = 0;
        WSASendTo(udpSendSocket, &buffer, 1, &bytesSent, flags,
                  reinterpret_cast<sockaddr*>(&udpSendAddr), sizeof(udpSendAddr), NULL, NULL);
    }
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

    // --- Raise thread priority and register with MMCSS ---
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    DWORD mmcssTaskIndex = 0;
    HANDLE mmcssHandle = AvSetMmThreadCharacteristicsA("Pro Audio", &mmcssTaskIndex);
    // Debug: verify thread priority and MMCSS registration
    int prio = GetThreadPriority(GetCurrentThread());
    if (prio != THREAD_PRIORITY_TIME_CRITICAL) {
        pwarASIOLog::Send("Warning: Thread priority not set to TIME_CRITICAL!");
    } else {
        pwarASIOLog::Send("Thread priority set to TIME_CRITICAL.");
    }
    if (!mmcssHandle) {
        pwarASIOLog::Send("Warning: MMCSS registration failed!");
    } else {
        pwarASIOLog::Send("MMCSS registration succeeded.");
    }
    // -----------------------------------------------------

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        if (mmcssHandle) AvRevertMmThreadCharacteristics(mmcssHandle);
        return;
    }
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        WSACleanup();
        if (mmcssHandle) AvRevertMmThreadCharacteristics(mmcssHandle);
        return;
    }
    // Set SO_RCVBUF to minimal size for low latency
    int rcvbuf = 1024; // 1 KB
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf));
    // Disable UDP connection reset behavior
    DWORD bytesReturned = 0;
    BOOL bNewBehavior = FALSE;
    WSAIoctl(sockfd, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &bytesReturned, NULL, NULL);
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
    pwar_packet_t output_packets[32];
    uint32_t packets_to_send = 0;
    while (udpListenerRunning) {
        len = sizeof(cliaddr);
        WSABUF wsaBuf;
        wsaBuf.buf = buffer;
        wsaBuf.len = sizeof(buffer);
        DWORD bytesReceived = 0;
        DWORD flags = 0;
        int res = WSARecvFrom(sockfd, &wsaBuf, 1, &bytesReceived, &flags, reinterpret_cast<sockaddr*>(&cliaddr), &len, NULL, NULL);
        if (res == 0 && bytesReceived >= sizeof(pwar_packet_t)) {
            pwar_packet_t pkt;
            memcpy(&pkt, buffer, sizeof(pwar_packet_t));

            uint32_t chunk_size = pkt.n_samples;
            pkt.num_packets =  blockFrames / chunk_size;
            latency_manager_process_packet_client(&pkt);

            int samples_ready = pwar_router_process_streaming_packet(&router, &pkt, input_buffers, blockFrames, PWAR_MAX_CHANNELS);

            if (started && (samples_ready > 0)) {
                uint32_t seq = pkt.seq;

                latency_manager_start_audio_cbk_begin();

                // Do the ASIO things.. input in input_buffers
                size_t to_copy = blockFrames;

                for (long i = 0; i < activeInputs; ++i) {
                    float* dest = inputBuffers[i] + (toggle ? blockFrames : 0);

                    // Copy the first input channel..
                    memcpy(dest, input_buffers, to_copy * sizeof(float));

                    // Zero out the rest
                    for (size_t j = to_copy; j < blockFrames; ++j)
                        dest[j] = 0.0f;
                }
                samplePosition += blockFrames;

                if (timeInfoMode) {
                    bufferSwitchX();
                } else {
                    callbacks->bufferSwitch(toggle, ASIOFalse);
                }

                latency_manager_start_audio_cbk_end();

                float* outputSamplesCh1 = outputBuffers[0] + (toggle ? blockFrames : 0);
                float* outputSamplesCh2 = outputBuffers[1] + (toggle ? blockFrames : 0);

                memcpy(output_buffers, outputSamplesCh1, blockFrames * sizeof(float));
                memcpy(output_buffers + blockFrames, outputSamplesCh2, blockFrames * sizeof(float));

                // Send the result
                pwar_router_send_buffer(&router, chunk_size, output_buffers, samples_ready, PWAR_MAX_CHANNELS, output_packets, 32, &packets_to_send);

                uint64_t timestamp = latency_manager_timestamp_now();
                for (uint32_t i = 0; i < packets_to_send; ++i) {
                    output_packets[i].seq = seq;
                    output_packets[i].timestamp = timestamp;
                    output(output_packets[i]);
                }
                toggle = toggle ? 0 : 1;

                pwar_latency_info_t latency_info;
                if (latency_manager_time_for_sending_latency_info(&latency_info)) {
                    // Send the latency info over the socket
                    if (udpSendSocket != INVALID_SOCKET) {
                        WSABUF buffer;
                        buffer.buf = reinterpret_cast<CHAR*>(&latency_info);
                        buffer.len = sizeof(latency_info);
                        DWORD bytesSent = 0;
                        int flags = 0;
                        WSASendTo(udpSendSocket, &buffer, 1, &bytesSent, flags,
                                  reinterpret_cast<sockaddr*>(&udpSendAddr), sizeof(udpSendAddr), NULL, NULL);
                    }
                }
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
            // Set SO_SNDBUF to minimal size for low latency
            int sndbuf = 1024; // 1 KB
            setsockopt(udpSendSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&sndbuf, sizeof(sndbuf));
            // Disable UDP connection reset behavior
            DWORD bytesReturned = 0;
            BOOL bNewBehavior = FALSE;
            WSAIoctl(udpSendSocket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior),
                     NULL, 0, &bytesReturned, NULL, NULL);
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

// Dynamic buffer size support methods
ASIOError pwarASIO::setBufferSize(long newBufferSize) {
    if (!isValidBufferSize(newBufferSize)) {
        strcpy(errorMessage, "Invalid buffer size");
        return ASE_InvalidParameter;
    }
    
    // Can't change buffer size while active
    if (started) {
        strcpy(errorMessage, "Cannot change buffer size while driver is active");
        return ASE_NotPresent;  // Use standard ASIO error code
    }
    
    blockFrames = newBufferSize;
    inputLatency = blockFrames;
    outputLatency = blockFrames * 2;
    milliSeconds = static_cast<long>((blockFrames * 1000) / sampleRate);
    
    return ASE_OK;
}

bool pwarASIO::isValidBufferSize(long bufferSize) const {
    return (bufferSize >= kMinBlockFrames && 
            bufferSize <= kMaxBlockFrames && 
            (bufferSize % kBlockFramesGranularity) == 0);
}
