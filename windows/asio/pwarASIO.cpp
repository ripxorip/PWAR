#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include "pwarASIO.h"
#include "pwarASIOLog.h"
#include "../../protocol/pwar_packet.h"
#pragma comment(lib, "ws2_32.lib")

//------------------------------------------------------------------------------------------

// local

static const double twoRaisedTo32 = 4294967296.;
static const double twoRaisedTo32Reciprocal = 1. / twoRaisedTo32;

CLSID IID_ASIO_DRIVER = { 0x188135e1, 0xd565, 0x11d2, { 0x85, 0x4f, 0x0, 0xa0, 0xc9, 0x9f, 0x5d, 0x19 } };

CFactoryTemplate g_Templates[1] = {
    {L"PWARASIO", &IID_ASIO_DRIVER, pwarASIO::CreateInstance} 
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

CUnknown* pwarASIO::CreateInstance (LPUNKNOWN pUnk, HRESULT *phr)
{
	return (CUnknown*)new pwarASIO (pUnk,phr);
};

STDMETHODIMP pwarASIO::NonDelegatingQueryInterface (REFIID riid, void ** ppv)
{
	if (riid == IID_ASIO_DRIVER)
	{
		return GetInterface (this, ppv);
	}
	return CUnknown::NonDelegatingQueryInterface (riid, ppv);
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//		Register ASIO Driver
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
extern LONG RegisterAsioDriver (CLSID,char *,char *,char *,char *);
extern LONG UnregisterAsioDriver (CLSID,char *,char *);

//
// Server registration, called on REGSVR32.EXE "the dllname.dll"
//
HRESULT _stdcall DllRegisterServer()
{
	LONG	rc;
	char	errstr[128];

	rc = RegisterAsioDriver (IID_ASIO_DRIVER,"PWARASIO.dll","PWAR ASIO Driver","PWAR ASIO","Apartment");

	if (rc) {
		memset(errstr,0,128);
		sprintf(errstr,"Register Server failed ! (%d)",rc);
		MessageBox(0,(LPCTSTR)errstr,(LPCTSTR)"ASIO sample Driver",MB_OK);
		return -1;
	}

	return S_OK;
}

//
// Server unregistration
//
HRESULT _stdcall DllUnregisterServer()
{
	LONG	rc;
	char	errstr[128];

	rc = UnregisterAsioDriver (IID_ASIO_DRIVER,"PWARASIO.dll","PWAR ASIO Driver");

	if (rc) {
		memset(errstr,0,128);
		sprintf(errstr,"Unregister Server failed ! (%d)",rc);
		MessageBox(0,(LPCTSTR)errstr,(LPCTSTR)"ASIO Korg1212 I/O Driver",MB_OK);
		return -1;
	}

	return S_OK;
}

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
pwarASIO::pwarASIO (LPUNKNOWN pUnk, HRESULT *phr)
	: CUnknown("PWARASIO", pUnk, phr)
{
    pwarASIOLog logger("10.0.0.171", 1338);
    long i;

	blockFrames = kBlockFrames;
	inputLatency = blockFrames;		// typically
	outputLatency = blockFrames * 2;
	// typically blockFrames * 2; try to get 1 by offering direct buffer
	// access, and using asioPostOutput for lower latency
	samplePosition = 0;
	sampleRate = 48000.;
	milliSeconds = (long)((double)(kBlockFrames * 1000) / sampleRate);
	active = false;
	started = false;
	timeInfoMode = false;
	tcRead = false;
	for (i = 0; i < kNumInputs; i++)
	{
		inputBuffers[i] = 0;
		inMap[i] = 0;
	}
	for (i = 0; i < kNumOutputs; i++)
	{
		outputBuffers[i] = 0;
		outMap[i] = 0;
	}
	callbacks = 0;
	activeInputs = activeOutputs = 0;
	toggle = 0;
    parseConfigFile();
    initUdpSender();
    startUdpListener();
}

//------------------------------------------------------------------------------------------
pwarASIO::~pwarASIO ()
{
    closeUdpSender();
    stopUdpListener();
	stop ();
	disposeBuffers ();
}

//------------------------------------------------------------------------------------------
void pwarASIO::getDriverName (char *name)
{
	strcpy (name, "PWAR ASIO Driver");
}

//------------------------------------------------------------------------------------------
long pwarASIO::getDriverVersion ()
{
	return 0x00000001L;
}

//------------------------------------------------------------------------------------------
void pwarASIO::getErrorMessage (char *string)
{
	strcpy (string, errorMessage);
}

//------------------------------------------------------------------------------------------
ASIOBool pwarASIO::init (void* sysRef)
{
	sysRef = sysRef;
    return true;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::start ()
{
	if (callbacks)
	{
		started = false;
		samplePosition = 0;
		theSystemTime.lo = theSystemTime.hi = 0;
		toggle = 0;
		started = true;
		return ASE_OK;
	}
	return ASE_NotPresent;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::stop ()
{
	started = false;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::getChannels (long *numInputChannels, long *numOutputChannels)
{
	*numInputChannels = kNumInputs;
	*numOutputChannels = kNumOutputs;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::getLatencies (long *_inputLatency, long *_outputLatency)
{
    *_inputLatency = blockFrames;		// typically
	*_outputLatency = blockFrames * 2;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::getBufferSize (long *minSize, long *maxSize,
	long *preferredSize, long *granularity)
{
    // Currently locked to 128, can be configurable in the future.
	*minSize = *maxSize = *preferredSize = 128; // fixed size
	*granularity = 0;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::canSampleRate (ASIOSampleRate sampleRate)
{
	if (sampleRate == 44100. || sampleRate == 48000.) // allow these rates only
		return ASE_OK;
	return ASE_NoClock;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::getSampleRate (ASIOSampleRate *sampleRate)
{
	*sampleRate = this->sampleRate;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::setSampleRate (ASIOSampleRate sampleRate)
{
	if (sampleRate != 44100. && sampleRate != 48000.)
		return ASE_NoClock;
	if (sampleRate != this->sampleRate)
	{
		this->sampleRate = sampleRate;
		asioTime.timeInfo.sampleRate = sampleRate;
		asioTime.timeInfo.flags |= kSampleRateChanged;
		milliSeconds = (long)((double)(kBlockFrames * 1000) / this->sampleRate);
		if (callbacks && callbacks->sampleRateDidChange)
			callbacks->sampleRateDidChange (this->sampleRate);
	}
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::getClockSources (ASIOClockSource *clocks, long *numSources)
{
	// internal
	clocks->index = 0;
	clocks->associatedChannel = -1;
	clocks->associatedGroup = -1;
	clocks->isCurrentSource = ASIOTrue;
	strcpy(clocks->name, "Internal");
	*numSources = 1;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::setClockSource (long index)
{
	if (!index)
	{
		asioTime.timeInfo.flags |= kClockSourceChanged;
		return ASE_OK;
	}
	return ASE_NotPresent;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::getSamplePosition (ASIOSamples *sPos, ASIOTimeStamp *tStamp)
{
	tStamp->lo = theSystemTime.lo;
	tStamp->hi = theSystemTime.hi;
	if (samplePosition >= twoRaisedTo32)
	{
		sPos->hi = (unsigned long)(samplePosition * twoRaisedTo32Reciprocal);
		sPos->lo = (unsigned long)(samplePosition - (sPos->hi * twoRaisedTo32));
	}
	else
	{
		sPos->hi = 0;
		sPos->lo = (unsigned long)samplePosition;
	}
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::getChannelInfo (ASIOChannelInfo *info)
{
	if (info->channel < 0 || (info->isInput ? info->channel >= kNumInputs : info->channel >= kNumOutputs))
		return ASE_InvalidParameter;
	info->type = ASIOSTFloat32LSB;

	info->channelGroup = 0;
	info->isActive = ASIOFalse;
	long i;
	if (info->isInput)
	{
	    strcpy(info->name, "Input ");
		for (i = 0; i < activeInputs; i++)
		{
			if (inMap[i] == info->channel)
			{
				info->isActive = ASIOTrue;
				break;
			}
		}
	}
	else
	{
	    strcpy(info->name, "Output ");
		for (i = 0; i < activeOutputs; i++)
		{
			if (outMap[i] == info->channel)
			{
				info->isActive = ASIOTrue;
				break;
			}
		}
	}
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::createBuffers (ASIOBufferInfo *bufferInfos, long numChannels,
    long bufferSize, ASIOCallbacks *callbacks)
{
    ASIOBufferInfo *info = bufferInfos;
    long i;
    bool notEnoughMem = false;
    activeInputs = 0;
    activeOutputs = 0;
    blockFrames = bufferSize;
    for (i = 0; i < numChannels; i++, info++)
    {
        if (info->isInput)
        {
            if (info->channelNum < 0 || info->channelNum >= kNumInputs)
                goto error;
            inMap[activeInputs] = info->channelNum;
            inputBuffers[activeInputs] = new float[blockFrames * 2]; // double buffer, now float
            if (inputBuffers[activeInputs])
            {
                info->buffers[0] = inputBuffers[activeInputs];
                info->buffers[1] = inputBuffers[activeInputs] + blockFrames;
            }
            else
            {
                info->buffers[0] = info->buffers[1] = 0;
                notEnoughMem = true;
            }
            activeInputs++;
            if (activeInputs > kNumInputs)
            {
error:
                disposeBuffers();
                return ASE_InvalidParameter;
            }
        }
        else // output			
        {
            if (info->channelNum < 0 || info->channelNum >= kNumOutputs)
                goto error;
            outMap[activeOutputs] = info->channelNum;
            outputBuffers[activeOutputs] = new float[blockFrames * 2]; // double buffer, now float
            if (outputBuffers[activeOutputs])
            {
                info->buffers[0] = outputBuffers[activeOutputs];
                info->buffers[1] = outputBuffers[activeOutputs] + blockFrames;
            }
            else
            {
                info->buffers[0] = info->buffers[1] = 0;
                notEnoughMem = true;
            }
            activeOutputs++;
            if (activeOutputs > kNumOutputs)
            {
                activeOutputs--;
                disposeBuffers();
                return ASE_InvalidParameter;
            }
        }
    } 
    if (notEnoughMem)
    {
        disposeBuffers();
        return ASE_NoMemory;
    }
    this->callbacks = callbacks;
    if (callbacks->asioMessage (kAsioSupportsTimeInfo, 0, 0, 0))
    {
        timeInfoMode = true;
        asioTime.timeInfo.speed = 1.;
        asioTime.timeInfo.systemTime.hi = asioTime.timeInfo.systemTime.lo = 0;
        asioTime.timeInfo.samplePosition.hi = asioTime.timeInfo.samplePosition.lo = 0;
        asioTime.timeInfo.sampleRate = sampleRate;
        asioTime.timeInfo.flags = kSystemTimeValid | kSamplePositionValid | kSampleRateValid;
        asioTime.timeCode.speed = 1.;
        asioTime.timeCode.timeCodeSamples.lo = asioTime.timeCode.timeCodeSamples.hi = 0;
        asioTime.timeCode.flags = kTcValid | kTcRunning ;
    }
    else
        timeInfoMode = false;	
    return ASE_OK;
}

//---------------------------------------------------------------------------------------------
ASIOError pwarASIO::disposeBuffers()
{
    long i;
    callbacks = 0;
    stop();
    for (i = 0; i < activeInputs; i++)
        delete[] inputBuffers[i]; // now float
    activeInputs = 0;
    for (i = 0; i < activeOutputs; i++)
        delete[] outputBuffers[i]; // now float
    activeOutputs = 0;
    return ASE_OK;
}

//---------------------------------------------------------------------------------------------
ASIOError pwarASIO::controlPanel()
{
	return ASE_NotPresent;
}

//---------------------------------------------------------------------------------------------
ASIOError pwarASIO::future (long selector, void* opt) // !!! check properties 
{
	ASIOTransportParameters* tp = (ASIOTransportParameters*)opt;
	switch (selector)
	{
		case kAsioEnableTimeCodeRead: tcRead = true; return ASE_SUCCESS;
		case kAsioDisableTimeCodeRead: tcRead = false; return ASE_SUCCESS;
		case kAsioSetInputMonitor: return ASE_SUCCESS; // for testing!!!
		case kAsioCanInputMonitor: return ASE_SUCCESS; // for testing!!!
		case kAsioCanTimeInfo: return ASE_SUCCESS;
		case kAsioCanTimeCode: return ASE_SUCCESS;
	}
	return ASE_NotPresent;
}
//--------------------------------------------------------------------------------------------------------
// private methods
//--------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------
// output
//------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------
// Refactored output to take a packet parameter
void pwarASIO::output(const rt_stream_packet_t& packet)
{
    if (udpSendSocket != INVALID_SOCKET) {
        sendto(udpSendSocket, (const char*)&packet, sizeof(rt_stream_packet_t), 0, (struct sockaddr*)&udpSendAddr, sizeof(udpSendAddr));
    }
}

//---------------------------------------------------------------------------------------------

void pwarASIO::switchBuffersFromPwarPacket(const rt_stream_packet_t& packet)
{
    size_t to_copy = (blockFrames < packet.n_samples) ? blockFrames : packet.n_samples;
    for (long i = 0; i < activeInputs; i++) {
        float* dest = inputBuffers[i] + (toggle ? blockFrames : 0);
        memcpy(dest, packet.samples_ch1, to_copy * sizeof(float));
        for (size_t j = to_copy; j < blockFrames; ++j)
            dest[j] = 0.0f;
    }

    // Prepare output packet from output buffer
    rt_stream_packet_t out_packet;
    out_packet.ts_pipewire_send = packet.ts_pipewire_send; // Copy PipeWire send timestamp

    samplePosition += blockFrames;
    if (timeInfoMode)
    {
        bufferSwitchX();
    }
    else {
        callbacks->bufferSwitch(toggle, ASIOFalse);
    }

    float* outputSamplesCh1 = outputBuffers[0] + (toggle ? blockFrames : 0);
    float* outputSamplesCh2 = outputBuffers[1] + (toggle ? blockFrames : 0);

    out_packet.n_samples = blockFrames;
    memcpy(out_packet.samples_ch1, outputSamplesCh1, out_packet.n_samples * sizeof(float));
    memcpy(out_packet.samples_ch2, outputSamplesCh2, out_packet.n_samples * sizeof(float));

    out_packet.seq = packet.seq; // Copy sequence number

    uint64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    out_packet.ts_asio_send = (nowNs - _timestamp) + packet.ts_pipewire_send; // Calculate ASIO send timestamp

    output(out_packet);

    toggle = toggle ? 0 : 1;
}

//---------------------------------------------------------------------------------------------
// asio2 buffer switch
void pwarASIO::bufferSwitchX ()
{
	getSamplePosition (&asioTime.timeInfo.samplePosition, &asioTime.timeInfo.systemTime);
	long offset = toggle ? blockFrames : 0;
	if (tcRead)
	{
		asioTime.timeCode.timeCodeSamples.lo = asioTime.timeInfo.samplePosition.lo + 600.0 * sampleRate;
		asioTime.timeCode.timeCodeSamples.hi = 0;
	}
	callbacks->bufferSwitchTimeInfo (&asioTime, toggle, ASIOFalse);
	asioTime.timeInfo.flags &= ~(kSampleRateChanged | kClockSourceChanged);
}

//---------------------------------------------------------------------------------------------
ASIOError pwarASIO::outputReady ()
{
	return ASE_NotPresent;
}

//---------------------------------------------------------------------------------------------

void pwarASIO::udp_packet_listener() {
    WSADATA wsaData;
    SOCKET sockfd;
    struct sockaddr_in servaddr, cliaddr;
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

    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
        closesocket(sockfd);
        WSACleanup();
        return;
    }

    udpListenerRunning = true;
    while (udpListenerRunning) {
        len = sizeof(cliaddr);
        n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, &len);
        if (n > 0) {
            if (n >= sizeof(rt_stream_packet_t)) {
                rt_stream_packet_t pkt;
                memcpy(&pkt, buffer, sizeof(rt_stream_packet_t));

                _timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()).count();

                if (started) {
                    switchBuffersFromPwarPacket(pkt);
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
    // Only check user home directory for config file
    std::string configPath;
    char* home = getenv("USERPROFILE");
    if (home && *home) {
        configPath = std::string(home) + "\\pwarASIO.cfg";
    } else {
        configPath = "pwarASIO.cfg";
    }
    std::ifstream cfg(configPath);
    if (cfg.is_open()) {
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
    // If file is not found, udpSendIp remains default
}

void pwarASIO::initUdpSender() {
    int udp_port = 8321; // hardcoded
    if (!udpWSAInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) == 0) {
            udpWSAInitialized = true;
        }
    }
    if (udpSendSocket == INVALID_SOCKET && udpWSAInitialized) {
        udpSendSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udpSendSocket != INVALID_SOCKET) {
            memset(&udpSendAddr, 0, sizeof(udpSendAddr));
            udpSendAddr.sin_family = AF_INET;
            udpSendAddr.sin_port = htons(udp_port);
            inet_pton(AF_INET, udpSendIp.c_str(), &udpSendAddr.sin_addr);
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
