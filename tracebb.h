#ifndef TRACEBB_H
#define TRACEBB_H

#include "tracer.h"
#include "shadow.h"
#include "instructions.h"

#include <vector>
#include <set>
#include <unordered_map>

extern unsigned instructionCount;
extern unordered_map<ADDRINT,instructionLocationsData > instructionLocations;
extern list<long long> loopStack;

struct ExecutionContex
{
	vector<pair<ADDRINT,UINT32> > addrs;
	vector<bool> pred;
};

class BBData
{
	vector<instructionLocationsData> instructions;
	set<ADDRINT> successors;
	int execution_count;

public:
	USIZE expected_num_ins;

	BBData();
	BBData(const BBData& s);
	~BBData();
	BBData& operator=(const BBData& s);
	BBData& operator=(BBData&& rhs);
	void swap(BBData &s);
	VOID pushInstruction(instructionLocationsData ins);
	VOID printBlock(FILE *out);
	// VOID execute(vector<pair<ADDRINT,UINT32> > &addrs, ShadowMemory &shadowMemory, FILE *out);
	VOID execute(ExecutionContex &contexts, ShadowMemory &shadowMemory, FILE *out);
	VOID addSuccessors(ADDRINT successor);
	vector<ADDRINT> getAddrs();
	int getNumSuccessors() const;
	int getNumInstuructions() const;
	int getNumExecution() const;
};



#endif