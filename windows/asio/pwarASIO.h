#ifndef _asiosmpl_
#define _asiosmpl_

#include "asiosys.h"
#include <thread>
#include "../../protocol/pwar_packet.h"

#define TESTWAVES 1
// when true, will feed the left input (to host) with
// a sine wave, and the right one with a sawtooth

enum
{
	kBlockFrames = 256,
	kNumInputs = 16,
	kNumOutputs = 16
};

#if WINDOWS

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
#else

#include "asiodrvr.h"

//---------------------------------------------------------------------------------------------
class pwarASIO : public AsioDriver
{
public:
	pwarASIO ();
	~pwarASIO ();
#endif

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

	void bufferSwitch ();
	long getMilliSeconds () {return milliSeconds;}

private:
friend void myTimer();

	bool inputOpen ();
#if TESTWAVES
	void makeSine (float *wave);
	void makeSaw (float *wave);
#endif
	void inputClose ();
	void input ();

	bool outputOpen ();
	void outputClose ();
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
#if TESTWAVES
    float *sineWave, *sawTooth;
#endif
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
};

#endif

