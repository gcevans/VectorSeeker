#ifndef OUTPUT_H
#define OUTPUT_H

#include "tracer.h"

void logBasicBlock(BBL bbl, ADDRINT id, FILE *bbl_log);
void finalOutput(FILE *trace, FILE* bbl_log);
void traceOutput(FILE *trace);

#endif