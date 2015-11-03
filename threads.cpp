#include "threads.h"
#include <assert.h>
#include <string>
#include <sstream>

// TLS globals
TLS_KEY tls_key;
#ifdef THREADSAFE
PIN_LOCK lock;
#endif
THREADID numThreads;


void initVSThreading()
{
	numThreads = 0;
    // Initialize the lock
    #ifdef THREADSAFE
    PIN_InitLock(&lock);
    #endif
}


void initThread(THREADID threadid, bool tracing)
{
	#ifdef THREADSAFE
    PIN_GetLock(&lock, threadid+1);
    #endif
    numThreads++;
    #ifdef THREADSAFE
	PIN_ReleaseLock(&lock);
    #endif

    thread_data_t* tdata = new thread_data_t;
    tdata->lastBB = 0;
    tdata->tracing = tracing;

    // String local debuglog
    stringstream logbuilder;
    string logfile;
    logbuilder << "thread_" << threadid << ".log";
    logbuilder >> logfile;
    if(KnobDebugTrace)
        tdata->debuglog = fopen(logfile.c_str(), "w");

    assert(tdata != nullptr);

    PIN_SetThreadData(tls_key, tdata, threadid);

    #ifndef THREADSAFE
    if(numThreads>1)
        cerr << "Traced program has multiple threads and Vector Seeker is not built with thread support" << endl;
    #endif
}

// function to access thread-specific data
thread_data_t* get_tls(THREADID threadid)
{
    thread_data_t* tdata = 
          static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, threadid));
    return tdata;
}
