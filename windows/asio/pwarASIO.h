/*
 * pwarASIO.h - PipeWire ASIO Relay (PWAR) project
 *
 * (c) 2025 Philip K. Gisslow
 * This file is part of the PipeWire ASIO Relay (PWAR) project.
 */

#ifndef __PWAR_ASIO_H__
#define __PWAR_ASIO_H__

#include "asiosys.h"
#include <thread>
#include <string>
#include "../../protocol/pwar_packet.h"

#include "rpc.h"
#include "rpcndr.h"
#ifndef COM_NO_WINDOWS_H
#include <windows.h>
#include "ole2.h"
#endif
#include "combase.h"
#include "iasiodrv.h"

constexpr int kMinBlockFrames = 32;
constexpr int kMaxBlockFrames = 2048;
constexpr int kDefaultBlockFrames = 128;
constexpr int kBlockFramesGranularity = 32;
constexpr int kNumInputs = 2;
constexpr int kNumOutputs = 2;

class pwarASIO : public IASIO, public CUnknown {
public:
    pwarASIO(LPUNKNOWN pUnk, HRESULT* phr);
    ~pwarASIO();

    DECLARE_IUNKNOWN
    static CUnknown* CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);
    virtual HRESULT STDMETHODCALLTYPE NonDelegatingQueryInterface(REFIID riid, void** ppvObject);

    ASIOBool init(void* sysRef);
    void getDriverName(char* name);
    long getDriverVersion();
    void getErrorMessage(char* string);

    ASIOError start();
    ASIOError stop();
    ASIOError getChannels(long* numInputChannels, long* numOutputChannels);
    ASIOError getLatencies(long* inputLatency, long* outputLatency);
    ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity);
    ASIOError canSampleRate(ASIOSampleRate sampleRate);
    ASIOError getSampleRate(ASIOSampleRate* sampleRate);
    ASIOError setSampleRate(ASIOSampleRate sampleRate);
    ASIOError getClockSources(ASIOClockSource* clocks, long* numSources);
    ASIOError setClockSource(long index);
    ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp);
    ASIOError getChannelInfo(ASIOChannelInfo* info);
    ASIOError createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks);
    ASIOError disposeBuffers();
    ASIOError controlPanel();
    ASIOError future(long selector, void* opt);
    ASIOError outputReady();
    long getMilliSeconds() const { return milliSeconds; }
    
    // Dynamic buffer size support
    ASIOError setBufferSize(long newBufferSize);
    bool isValidBufferSize(long bufferSize) const;

private:
    void output(const pwar_packet_t& packet);
    void bufferSwitchX();
    void udp_iocp_listener();
    void startUdpListener();
    void stopUdpListener();
    void initUdpSender();
    void closeUdpSender();
    void parseConfigFile();

    float *output_buffers;
    float *input_buffers;

    double samplePosition;
    double sampleRate;
    ASIOCallbacks* callbacks;
    ASIOTime asioTime;
    ASIOTimeStamp theSystemTime;
    float* inputBuffers[kNumInputs * 2];
    float* outputBuffers[kNumOutputs * 2];
    long inMap[kNumInputs];
    long outMap[kNumOutputs];
    long blockFrames;
    long inputLatency;
    long outputLatency;
    long activeInputs;
    long activeOutputs;
    long toggle;
    long milliSeconds;
    bool active;
    bool started;
    bool timeInfoMode;
    bool tcRead;
    char errorMessage[128]{};
    uint64_t _timestamp = 0;
    std::thread udpListenerThread;
    bool udpListenerRunning = false;
    SOCKET udpSendSocket = INVALID_SOCKET;
    bool udpWSAInitialized = false;
    struct sockaddr_in udpSendAddr;
    std::string udpSendIp = "192.168.66.2";
};

#endif // __PWAR_ASIO_H__
