#include "stddef.h"
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

static clock_t starttime;

void _tracer_traceon()
{
}

void ftracer_traceon_()
{
  printf("trace on\n");
  fflush(NULL);
  starttime = clock();
  _tracer_traceon();
}

void _tracer_traceoff()
{
}

void ftracer_traceoff_()
{
  clock_t endtime;
  endtime = clock();
  printf("trace off\n");
  printf("time = %f\n", (double)(endtime - starttime) / CLOCKS_PER_SEC );
  fflush(NULL);
  _tracer_traceoff();
}

void _tracer_loopstart(int id)
{
}

void _tracer_loopend(int id)
{
}

void _tracer_array_memory(void *start, size_t length)
{
}

void ftracer_array_memory_(void **start, int *length)
{
  _tracer_array_memory(*start, *length);
}

void _tracer_array_memory_clear(void *start)
{
}

void ftracer_array_memory_clear_(void *start)
{
  _tracer_array_memory_clear(start);
}

