
#include "tracebb.h"
#include "tracer_decode.h"

#include "assert.h"

extern unsigned instructionCount;

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

VOID handleMemInst(const instructionLocationsData &ins, pair<ADDRINT,UINT32>one, pair<ADDRINT,UINT32>two, ShadowMemory &shadowMemory, FILE *out)
//VOID RecordMemReadWrite(VOID * ip, VOID * addr1, UINT32 t1, VOID *addr2, UINT32 t2)
{
	VOID *ip = (VOID *) ins.ip;
	VOID *addr1 = (VOID *) one.first;
	UINT32 type1 = one.second;
	VOID *addr2 = (VOID *) two.first;
	UINT32 type2 = two.second;

	long value = 0;
	
	if(type1 & READ_OPERATOR_TYPE)
	{
		value = shadowMemory.readMem((ADDRINT)addr1);
//		fprintf(trace,"R1[%p]=%ld\n",addr1,shadowMemory.readMem((ADDRINT)addr1));
	}

	if(type2 & READ_OPERATOR_TYPE)
	{
		value = max(shadowMemory.readMem((ADDRINT)addr2), value);
//		fprintf(trace,"R2[%p]=%ld\n",addr2,shadowMemory.readMem((ADDRINT)addr2));
	}

//	fprintf(trace, "After all memory reads value = %ld\n", value);

	for(unsigned int i = 0; i < ins.registers_read.size(); i++)
	{
		value = max(shadowMemory.readReg(ins.registers_read[i]),value);
	}

//	fprintf(trace, "After all reads value = %ld\n", value);
		
	if(value > 0)
	{
		value = value +1;
	}

	for(unsigned int i = 0; i < ins.registers_written.size(); i++)
	{
		shadowMemory.writeReg(ins.registers_written[i], value);
	}
	
	long region1 = 0;
	long region2 = 0;
	if(type1 & WRITE_OPERATOR_TYPE)
	{
		if(memIsArray(addr1))
			region1 = 1;
		else
			region1 = 0;

		shadowMemory.writeMem((ADDRINT)addr1, max(value,region1));
//		fprintf(trace,"W1[%p]=%ld\n",addr1,shadowMemory[(ADDRINT)addr1]);
	}

	if(type2 & WRITE_OPERATOR_TYPE)
	{
		if(memIsArray(addr2))
			region2 = 1;
		else
			region2 = 0;

		shadowMemory.writeMem((ADDRINT)addr2, max(value,region2));
//		fprintf(trace,"W2[%p]=%ld\n",addr2,shadowMemory[(ADDRINT)addr2]);
	}

	value = max(max(value,region1),region2);

	if(value > 0 && ((ins.type == MOVEONLY_INS_TYPE) && KnobSkipMove) )
	{
		instructionResults[(ADDRINT)ip].addToDepth(value);
		// current_instruction->execution_count += 1;
		// current_instruction->loopid = loopStack;
	}

	if(!KnobDebugTrace)
		return;
		
	instructionTracing(ip,addr2,value,"RecordMemReadWrite",out, shadowMemory);
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

	size_t memOpsCount = 0;

	// fprintf(out, "Executing Block %p\n", (void *) instructions.front().ip);
	for(size_t i = 0; i < instructions.size(); i++)
	{
		instructionCount++;
		// char buff[256];
		// disassemblyToBuff(buff, (void *) instructions[i].ip);
		// fprintf(out, "%s\n", buff);
		//execute instruction
		if(instructions[i].type == X87_INS_TYPE)
		{
			instructionTracing((VOID *) instructions[i].ip, NULL, 0, "x87error", out, shadowMemory);
		}

		if(instructions[i].memOperands == 0)
		{
			handleBaseInst(instructions[i], shadowMemory, out);
		}
		else if(instructions[i].memOperands < 3)
		{
			// handleMemInst(instructions[i], addrs[memOpsCount], addrs[memOpsCount+1], shadowMemory, out);
			memOpsCount += 2;
		}
		else
		{
			instructionTracing((VOID *) instructions[i].ip,NULL,0,"unhandledMemOp",out, shadowMemory);
		}
	}

	addrs.clear();
}

VOID BBData::addSuccessors(ADDRINT s)
{
	successors.insert(s);
}
