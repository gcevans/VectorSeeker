#ifndef TRACEBB_H
#define TRACEBB_H

#include "tracer.h"
#include "tracer_decode.h"
#include <vector>

class BBData
{
	vector<instructionLocationsData> instructions;

public:
	VOID pushInstruction(instructionLocationsData ins);
	VOID printBlock(FILE *out);
	VOID execute(vector<ADDRINT> &addrs);
};

#endif