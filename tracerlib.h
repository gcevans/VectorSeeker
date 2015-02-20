#ifndef TRACERLIB_H
#define TRACERLIB_H
#include <stdlib.h>

#ifdef __cplusplus 
extern "C" {
#endif

#define LOOP_START "_tracer_loopstart"
void _tracer_loopstart(long long id);
#define LOOP_END "_tracer_loopend"
void _tracer_loopend(long long id);

#define TRACE_ON "_tracer_traceon"
void _tracer_traceon();
void ftracer_traceon_();

#define TRACE_OFF "_tracer_traceoff"
void _tracer_traceoff();
void ftracer_traceoff_();

#define ARRAY_MEM "_tracer_array_memory"
void _tracer_array_memory(void *start, size_t length);
void ftracer_array_memory_(void **start, int *length);

#define ARRAY_MEM_CLEAR "_tracer_array_memory_clear"
void _tracer_array_memory_clear(void *start);
void ftracer_array_memory_clear_(void *start);

#ifdef __cplusplus 
}
#endif

#endif
