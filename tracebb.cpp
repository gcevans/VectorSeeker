
#include "tracebb.h"
#include "tracer_decode.h"

#include "assert.h"

extern unsigned instructionCount;

VOID handleX87Inst(VOID *ip, ShadowMemory &shadowMemory, FILE *out)
{
	instructionCount++;

	instructionTracing(ip, NULL, 0, "x87error", out, shadowMemory);
}

VOID handleBaseInst(const instructionLocationsData &ins, ShadowMemory &shadowMemory, FILE *out)
{
	instructionCount++;
		
	long value = 0;
	
	for(unsigned int i = 0; i < ins.registers_read.size(); i++)
	{
		value = max(shadowMemory.readReg(ins.registers_read[i]),value);
	}
	
	if(value > 0)
	{
		value = value + 1;
	}

	for(unsigned int i = 0; i < ins.registers_written.size(); i++)
	{
		shadowMemory.writeReg(ins.registers_written[i], value);
	}
	
	if(value > 0 && (!((ins.type == MOVEONLY_INS_TYPE) && KnobSkipMove) ) )
	{
		instructionResults[ins.ip].addToDepth(value);
		// ins->execution_count += 1;
		// ins->loopid = loopStack;
//		fprintf(trace,"ins.loopid size = %d\n", (int) ins.loopid.size());
	}
	if(!KnobDebugTrace)
	return;

	instructionTracing((VOID *)ins.ip,NULL,value,"recoredBaseInst",out,shadowMemory);
}

VOID BBData::pushInstruction(instructionLocationsData ins)
{
	instructions.push_back(ins);
}

VOID BBData::printBlock(FILE *out)
{
	if(instructions.size() == 0)
	{
		fprintf(out, "Empty Basic Block\n"); // probably a bare call
	}
	else
	{
		fprintf(out, "%s\n", instructions.front().rtn_name.c_str());
		fprintf(out, "Basic Block start:%p end:%p\n", (void *) instructions.front().ip, (void *) instructions.back().ip );
		for(size_t i = 0; i < instructions.size(); i++)
		{
			char decode[128];
			disassemblyToBuff(decode, (void *) instructions[i].ip);
			fprintf(out,"%p\t%s\n", (void *) instructions[i].ip, decode);
		}
	}
	fprintf(out, "Successors:\n");
	for (auto it = successors.begin(); it != successors.end(); ++it)
	{
		fprintf(out, "%p\n",(void * ) (*it) );
	}
	fprintf(out, "################\n");
}

VOID BBData::execute(vector<pair<ADDRINT,UINT32> > &addrs, ShadowMemory &shadowMemory, FILE *out)
{
	if(instructions.size() == 0)
		return;

	// fprintf(out, "Executing Block %p\n", (void *) instructions.front().ip);
	for(size_t i = 0; i < instructions.size(); i++)
	{
		// char buff[256];
		// disassemblyToBuff(buff, (void *) instructions[i].ip);
		// fprintf(out, "%s\n", buff);
		//execute instruction
		if(instructions[i].type == X87_INS_TYPE)
		{
			handleX87Inst((VOID *)instructions[i].ip, shadowMemory, out);
		}

		// if(instructions[i].memOperands == 0)
		// {
		// 	handleBaseInst(instructions[i], shadowMemory, out);
		// }
	}

	addrs.clear();
}

VOID BBData::addSuccessors(ADDRINT s)
{
	successors.insert(s);
}
