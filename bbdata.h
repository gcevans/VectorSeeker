#ifndef BBDATA_H
#define BBDATA_H

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
	VOID pushInstruction(const instructionLocationsData *ins);
	VOID printBlock(FILE *out);
	void incExecutionCount();
#ifdef NOSHAODWCACHE
	VOID execute(BBData block, ExecutionContex &contexts, ShadowMemoryNoCache &shadowMemory, ShadowRegisters &registers, unordered_map<ADDRINT, ResultVector > &instructionResults, FILE *out);
#else
	VOID execute(BBData block, ExecutionContex &contexts, ShadowMemory &shadowMemory, ShadowRegisters &registers, unordered_map<ADDRINT, ResultVector > &instructionResults, FILE *out);
#endif
	VOID addSuccessors(ADDRINT successor);
	vector<ADDRINT> getAddrs();
	int getNumSuccessors() const;
	int getNumInstuructions() const;
	int getNumExecution() const;
};


#endif
