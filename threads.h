#ifndef THREADS_H
#define THREADS_H

#include "pin.H"
#include "shadow.h"
#include "tracebb.h"


extern TLS_KEY tls_key;

#ifdef THREADSAFE
extern PIN_LOCK lock;
#endif
extern THREADID numThreads;

class thread_data_t
{
public:
	size_t malloc_size;
		// UINT8 pad[64-sizeof(ADDRINT)];
	size_t lastBB;
	ExecutionContex rwContext;
	ShadowRegisters registers;
};

void initVSThreading();
void initThread(THREADID threadid);
thread_data_t* get_tls(THREADID threadid);


#endif