#ifndef TRACER_DECODE_H
#define TRACER_DECODE_H

#include "instlib.H"

enum instructionType { IGNORED_INS_TYPE, NORMAL_INS_TYPE, MOVEONLY_INS_TYPE, X87_INS_TYPE};

//Decode Instructions for tool
instructionType decodeInstructionData(ADDRINT ip);

//Decode Instructions for debug tracing
void instructionTracing(VOID * ip, VOID * addr, long int value, const char *called_from);

//Get Human readable string for instruction
VOID disassemblyToBuff(char * decodeBuffer, VOID *ip, const xed_decoded_inst_t *ins);

#endif
