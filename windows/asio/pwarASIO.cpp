#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include "pwarASIO.h"
#include "pwarASIOLog.h"
#include "../../protocol/pwar_packet.h"
#pragma comment(lib, "ws2_32.lib")

//------------------------------------------------------------------------------------------

// extern
void getNanoSeconds(ASIOTimeStamp *time);

// local

double AsioSamples2double (ASIOSamples* samples);

static const double twoRaisedTo32 = 4294967296.;
static const double twoRaisedTo32Reciprocal = 1. / twoRaisedTo32;

//------------------------------------------------------------------------------------------
// on windows, we do the COM stuff.

#if WINDOWS

// class id. !!! NOTE: !!! you will obviously have to create your own class id!
// {188135E1-D565-11d2-854F-00A0C99F5D19}
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
	sampleRate = 44100.;
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
#if TESTWAVES
	sawTooth = sineWave = 0;
#endif
	for (i = 0; i < kNumOutputs; i++)
	{
		outputBuffers[i] = 0;
		outMap[i] = 0;
	}
	callbacks = 0;
	activeInputs = activeOutputs = 0;
	toggle = 0;
    startUdpListener();
}

//------------------------------------------------------------------------------------------
pwarASIO::~pwarASIO ()
{
    stopUdpListener();
	stop ();
	outputClose ();
	inputClose ();
	disposeBuffers ();
}

//------------------------------------------------------------------------------------------
void pwarASIO::getDriverName (char *name)
{
	strcpy (name, "Sample ASIO");
    pwarASIOLog::Send("pwarASIO::getDriverName called");
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
	if (active)
		return true;
	strcpy (errorMessage, "ASIO Driver open Failure!");
	if (inputOpen ())
	{
		if (outputOpen ())
		{
			active = true;
			return true;
		}
	}
	timerOff (); // de-activate 'hardware'
	outputClose ();
	inputClose ();
	return false;
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
		//timerOn (); // activate 'hardware'
		started = true;
		return ASE_OK;
	}
	return ASE_NotPresent;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::stop ()
{
	started = false;
	timerOff (); // de-activate 'hardware'
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
	*_inputLatency = inputLatency;
	*_outputLatency = outputLatency;
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
ASIOError pwarASIO::getBufferSize (long *minSize, long *maxSize,
	long *preferredSize, long *granularity)
{
	*minSize = *maxSize = *preferredSize = blockFrames; // allow this size only
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
#if WINDOWS
	info->type = ASIOSTFloat32LSB;
#else
	info->type = ASIOSTInt16MSB;
#endif
	info->channelGroup = 0;
	info->isActive = ASIOFalse;
	long i;
	if (info->isInput)
	{
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
		for (i = 0; i < activeOutputs; i++)
		{
			if (outMap[i] == info->channel)
			{
				info->isActive = ASIOTrue;
				break;
			}
		}
	}
	strcpy(info->name, "Sample ");
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
//--------------------------------------------------------------------------------------------------------
// input
//--------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
bool pwarASIO::inputOpen ()
{
#if TESTWAVES
    sineWave = new float[blockFrames];
    if (!sineWave)
    {
        strcpy (errorMessage, "ASIO Sample Driver: Out of Memory!");
        return false;
    }
    makeSine (sineWave);
    sawTooth = new float[blockFrames];
    if (!sawTooth)
    {
        strcpy(errorMessage, "ASIO Sample Driver: Out of Memory!");
        return false;
    }
    makeSaw(sawTooth);
#endif
    return true;
}
#if TESTWAVES
#include <math.h>
const double pi = 0.3141592654;
void pwarASIO::makeSine (float *wave)
{
    double frames = (double)blockFrames;
    double i, f = (pi * 2.) / frames;
    for (i = 0; i < frames; i++)
        *wave++ = (float)sin(f * i);
}
void pwarASIO::makeSaw(float *wave)
{
    double frames = (double)blockFrames;
    double i, f = 2. / frames;
    for (i = 0; i < frames; i++)
        *wave++ = (float)(-1. + f * i);
}
#endif
//---------------------------------------------------------------------------------------------
void pwarASIO::inputClose ()
{
#if TESTWAVES
    if (sineWave)
        delete[] sineWave;
    sineWave = 0;
    if (sawTooth)
        delete[] sawTooth;
    sawTooth = 0;
#endif
}
//---------------------------------------------------------------------------------------------
void pwarASIO::input()
{
#if TESTWAVES
    long i;
    float *in = 0;
    for (i = 0; i < activeInputs; i++)
    {
        in = inputBuffers[i];
        if (in)
        {
            if (toggle)
                in += blockFrames;
            if ((i & 1) && sawTooth)
                memcpy(in, sawTooth, (unsigned long)(blockFrames * sizeof(float)));
            else if (sineWave)
                memcpy(in, sineWave, (unsigned long)(blockFrames * sizeof(float)));
        }
    }
#endif
}
//------------------------------------------------------------------------------------------------------------------
// output
//------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------
bool pwarASIO::outputOpen()
{
	return true;
}

//---------------------------------------------------------------------------------------------
void pwarASIO::outputClose ()
{
}

//---------------------------------------------------------------------------------------------
// Refactored output to take a packet parameter
void pwarASIO::output(const rt_stream_packet_t& packet)
{
    // Send the provided packet over UDP to 10.0.0.171:8321
    WSADATA wsaData;
    SOCKET sockfd;
    struct sockaddr_in servaddr;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) == 0) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd != INVALID_SOCKET) {
            memset(&servaddr, 0, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(8321);
            inet_pton(AF_INET, "10.0.0.171", &servaddr.sin_addr);
            sendto(sockfd, (const char*)&packet, sizeof(rt_stream_packet_t), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
            closesocket(sockfd);
        }
        WSACleanup();
    }
}

//---------------------------------------------------------------------------------------------
void pwarASIO::bufferSwitch ()
{
	if (started && callbacks)
	{
		getNanoSeconds(&theSystemTime); // latch system time
		input();
		// Prepare packet
		rt_stream_packet_t packet;
		float* outputSamples = outputBuffers[0] + (toggle ? blockFrames : 0);
		packet.n_samples = (blockFrames < 256) ? blockFrames : 256;
		memcpy(packet.samples, outputSamples, packet.n_samples * sizeof(float));
		for (size_t i = packet.n_samples; i < 256; ++i) {
			packet.samples[i] = 0.0f;
		}
		output(packet);
		samplePosition += blockFrames;
		if (timeInfoMode)
			bufferSwitchX ();
		else
			callbacks->bufferSwitch (toggle, ASIOFalse);
		toggle = toggle ? 0 : 1;
	}
}

void pwarASIO::switchBuffersFromPwarPacket(const rt_stream_packet_t& packet)
{

    size_t to_copy = (blockFrames < packet.n_samples) ? blockFrames : packet.n_samples;
    for (long i = 0; i < activeInputs; i++) {
        float* dest = inputBuffers[i] + (toggle ? blockFrames : 0);
        memcpy(dest, packet.samples, to_copy * sizeof(float));
        for (size_t j = to_copy; j < blockFrames; ++j)
            dest[j] = 0.0f;
    }

    // Prepare output packet from output buffer
    rt_stream_packet_t out_packet;
    float* outputSamples = outputBuffers[0] + (toggle ? blockFrames : 0);
    out_packet.n_samples = (blockFrames < 256) ? blockFrames : 256;
    memcpy(out_packet.samples, outputSamples, out_packet.n_samples * sizeof(float));
    for (size_t i = out_packet.n_samples; i < 256; ++i) {
        out_packet.samples[i] = 0.0f;
    }

    out_packet.seq = packet.seq; // Copy sequence number

    output(out_packet);
    samplePosition += blockFrames;
    if (timeInfoMode)
    {
        bufferSwitchX();
    }
    else {
        callbacks->bufferSwitch(toggle, ASIOFalse);
    }
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
double AsioSamples2double (ASIOSamples* samples)
{
	double a = (double)(samples->lo);
	if (samples->hi)
		a += (double)(samples->hi) * twoRaisedTo32;
	return a;
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

#endif // WINDOWS

