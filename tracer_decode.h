#ifndef TRACER_DECODE_H
#define TRACER_DECODE_H

#include "tracer.h"
#include "shadow.h"
#include "instructions.h"

//Decode Instructions for tool saving data to instruction map
instructionType decodeInstructionData(ADDRINT ip, unordered_map<ADDRINT,instructionLocationsData > &instructionLocations);

//Decode Instructions for debug tracing
#ifdef NOSHAODWCACHE
void instructionTracing(VOID * ip, VOID * addr, long int value, const char *called_from, FILE *out, ShadowMemoryNoCache &shadowMemory, ShadowRegisters &registers);
#else
void instructionTracing(VOID * ip, VOID * addr, long int value, const char *called_from, FILE *out, ShadowMemory &shadowMemory, ShadowRegisters &registers);
#endif

//Get Instruction Class String
const char *getInstCategoryString(VOID *ip);

#endif
