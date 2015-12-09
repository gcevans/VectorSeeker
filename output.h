#ifndef OUTPUT_H
#define OUTPUT_H

#include "tracer.h"
#include "tracebb.h"
#include "resultvector.h"

void logBasicBlock(BBL bbl, ADDRINT id, FILE *bbl_log);
void finalOutput(FILE *trace, FILE* bbl_log, unordered_map<ADDRINT, ResultVector > &instructionResults);
void traceOutput(FILE *trace, unordered_map<ADDRINT, ResultVector > &instructionResults);

#endif