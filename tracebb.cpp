#include "tracebb.h"

VOID BBData::pushInstruction(instructionLocationsData ins)
{
	instructions.push_back(ins);
}

VOID BBData::printBlock(FILE *out)
{
	fprintf(out, "%s\n", instructions.front().rtn_name.c_str());
	fprintf(out, "Basic Block start:%p end:%p\n", (void *) instructions.front().ip, (void *) instructions.back().ip );
	for(size_t i = 0; i < instructions.size(); i++)
	{
		char decode[128];
		disassemblyToBuff(decode, (void *) instructions[i].ip);
		fprintf(out,"%p\t%s\n", (void *) instructions[i].ip, decode);
	}
	fprintf(out, "Successors:\n");
	for (auto it = successors.begin(); it != successors.end(); ++it)
	{
		fprintf(out, "%p\n",(void * ) (*it) );
	}
	fprintf(out, "################\n");
}

VOID BBData::execute(vector<pair<ADDRINT,UINT32> > &addrs)
{
	fprintf(trace, "Executing Block\n");
	for(size_t i = 0; i < instructions.size(); i++)
	{
		//execute instruction
//		fprintf(trace, "%s\n", instructions[i].instruction.c_str());
	}

	addrs.clear();
}

VOID BBData::addSuccessors(ADDRINT s)
{
	successors.insert(s);
}
