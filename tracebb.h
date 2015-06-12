#ifndef TRACEBB_H
#define TRACEBB_H

#include "tracer.h"
#include "shadow.h"

#include <vector>
#include <set>

class BBData
{
	vector<instructionLocationsData> instructions;
	set<ADDRINT> successors;

public:
	VOID pushInstruction(instructionLocationsData ins);
	VOID printBlock(FILE *out);
	VOID execute(vector<pair<ADDRINT,UINT32> > &addrs, ShadowMemory &shadowMemory, FILE *out);
	VOID addSuccessors(ADDRINT successor);
};

#endif