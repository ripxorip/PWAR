#ifndef _asiosmpl_
#define _asiosmpl_

#include "asiosys.h"
#include <thread>
#include <string>
#include "../../protocol/pwar_packet.h"

enum
{
	kBlockFrames = 128,
	kNumInputs = 1,
	kNumOutputs = 2
};

#include "rpc.h"
#include "rpcndr.h"
#ifndef COM_NO_WINDOWS_H
#include <windows.h>
#include "ole2.h"
#endif

#include "combase.h"
#include "iasiodrv.h"

class pwarASIO : public IASIO, public CUnknown
{
public:
	pwarASIO(LPUNKNOWN pUnk, HRESULT *phr);
	~pwarASIO();

	DECLARE_IUNKNOWN
    //STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {      \
    //    return GetOwner()->QueryInterface(riid,ppv);            \
    //};                                                          \
    //STDMETHODIMP_(ULONG) AddRef() {                             \
    //    return GetOwner()->AddRef();                            \
    //};                                                          \
    //STDMETHODIMP_(ULONG) Release() {                            \
    //    return GetOwner()->Release();                           \
    //};

	// Factory method
	static CUnknown *CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE NonDelegatingQueryInterface(REFIID riid,void **ppvObject);

	ASIOBool init (void* sysRef);
	void getDriverName (char *name);	// max 32 bytes incl. terminating zero
	long getDriverVersion ();
	void getErrorMessage (char *string);	// max 128 bytes incl.

	ASIOError start ();
	ASIOError stop ();

	ASIOError getChannels (long *numInputChannels, long *numOutputChannels);
	ASIOError getLatencies (long *inputLatency, long *outputLatency);
	ASIOError getBufferSize (long *minSize, long *maxSize,
		long *preferredSize, long *granularity);

	ASIOError canSampleRate (ASIOSampleRate sampleRate);
	ASIOError getSampleRate (ASIOSampleRate *sampleRate);
	ASIOError setSampleRate (ASIOSampleRate sampleRate);
	ASIOError getClockSources (ASIOClockSource *clocks, long *numSources);
	ASIOError setClockSource (long index);

	ASIOError getSamplePosition (ASIOSamples *sPos, ASIOTimeStamp *tStamp);
	ASIOError getChannelInfo (ASIOChannelInfo *info);

	ASIOError createBuffers (ASIOBufferInfo *bufferInfos, long numChannels,
		long bufferSize, ASIOCallbacks *callbacks);
	ASIOError disposeBuffers ();

	ASIOError controlPanel ();
	ASIOError future (long selector, void *opt);
	ASIOError outputReady ();

	long getMilliSeconds () {return milliSeconds;}

private:

	void output(const rt_stream_packet_t& packet);

	void timerOn ();
	void timerOff ();
	void bufferSwitchX ();

	double samplePosition;
	double sampleRate;
	ASIOCallbacks *callbacks;
	ASIOTime asioTime;
	ASIOTimeStamp theSystemTime;
	float *inputBuffers[kNumInputs * 2];
	float *outputBuffers[kNumOutputs * 2];

	long inMap[kNumInputs];
	long outMap[kNumOutputs];
	long blockFrames;
	long inputLatency;
	long outputLatency;
	long activeInputs;
	long activeOutputs;
	long toggle;
	long milliSeconds;
	bool active, started;
	bool timeInfoMode, tcRead;
	char errorMessage[128];

	uint64_t _timestamp = 0;

    std::thread udpListenerThread;
    bool udpListenerRunning = false;
    void udp_packet_listener();
    void startUdpListener();
    void stopUdpListener();
	void switchBuffersFromPwarPacket(const rt_stream_packet_t &packet);

    SOCKET udpSendSocket = INVALID_SOCKET;
    bool udpWSAInitialized = false;
    struct sockaddr_in udpSendAddr;
    void initUdpSender();
    void closeUdpSender();

    std::string udpSendIp = "192.168.66.2";
    void parseConfigFile();
};

#endif

