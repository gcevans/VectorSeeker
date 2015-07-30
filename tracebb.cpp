
#include "tracebb.h"
#include "tracer_decode.h"

#include "assert.h"

extern unsigned instructionCount;

VOID handleBaseInstBB(const instructionLocationsData &ins, ShadowMemory &shadowMemory, FILE *out)
{
	++instructionCount;
		
	long value = 0;
	for(auto reg : ins.registers_read)
	{
		value = max(shadowMemory.readReg(reg),value);
	}
	
	if(value > 0 && (ins.type != MOVEONLY_INS_TYPE))
	{
		value = value + 1;
	}

	for(auto reg : ins.registers_written)
	{
		shadowMemory.writeReg(reg, value);
	}
	
	if(value > 0 && (!((ins.type == MOVEONLY_INS_TYPE) && KnobSkipMove)))
	{
		instructionResults[ins.ip].addToDepth(value);
	}
	
	if(KnobDebugTrace)
		instructionTracing((VOID *)ins.ip,NULL,value,"Base",out,shadowMemory);
}

VOID handleMemInstBB(const instructionLocationsData &ins, const pair<ADDRINT,UINT32>&one, const pair<ADDRINT,UINT32>&two, ShadowMemory &shadowMemory, FILE *out)
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
	}

	if(type2 & READ_OPERATOR_TYPE)
	{
		value = max(shadowMemory.readMem((ADDRINT)addr2), value);
	}

	for(unsigned int i = 0; i < ins.registers_read.size(); i++)
	{
		value = max(shadowMemory.readReg(ins.registers_read[i]),value);
	}
		
	if(value > 0 && (ins.type != MOVEONLY_INS_TYPE))
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
		if(shadowMemory.memIsArray(addr1))
			region1 = 1;
		else
			region1 = 0;

		shadowMemory.writeMem((ADDRINT)addr1, max(value,region1));
	}

	if(type2 & WRITE_OPERATOR_TYPE)
	{
		if(shadowMemory.memIsArray(addr2))
			region2 = 1;
		else
			region2 = 0;

		shadowMemory.writeMem((ADDRINT)addr2, max(value,region2));
	}

	value = max(max(value,region1),region2);

	if(value > 0 && !(((ins.type == MOVEONLY_INS_TYPE) && KnobSkipMove)))
	{
		instructionResults[(ADDRINT)ip].addToDepth(value);
	}

	if(KnobDebugTrace)
	{
		instructionTracing(ip,addr2,value,"Mem",out, shadowMemory);
		fprintf(out,"<%p,%u><%p,%u>\n", addr1, type1, addr2, type2);
	}

}

VOID BBData::pushInstruction(instructionLocationsData ins)
{
	assert(ins.type != IGNORED_INS_TYPE);
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
			fprintf(out,"%p\t%s\n", (void *) instructions[i].ip, debugData[instructions[i].ip].instruction.c_str());
		}
	}
	fprintf(out, "Successors:\n");
	for (auto it = successors.begin(); it != successors.end(); ++it)
	{
		fprintf(out, "%p\n",(void * ) (*it) );
	}
	fprintf(out, "################\n");
}

void printAddrs(const vector<pair<ADDRINT,UINT32> > &addrs, FILE *out)
{
	fprintf(out, "Start Block Addresses\n");
	for(auto p : addrs)
	{
		fprintf(out, "<%p,%d>", (void *) p.first, (int) p.second);
	}
	fprintf(out, "\n");
}

//VOID BBData::execute(vector<pair<ADDRINT,UINT32> > &addrs, ShadowMemory &shadowMemory, FILE *out)
VOID BBData::execute(ExecutionContex &contexts, ShadowMemory &shadowMemory, FILE *out)
{
	if(KnobBBSummary)
		this->execution_count += 1;

	if(instructions.size() == 0) // probably a bare call
		return;

	size_t memOpsCount = 0;

	if(KnobDebugTrace)
	{
		fprintf(out, "Executing Block %p with %d instructions expected %d\n", (void *) instructions.front().ip, (int) instructions.size(), expected_num_ins);
	}

	for(size_t i = 0; i < instructions.size(); i++)
	{
		// execute instruction
		if(instructions[i].type == X87_INS_TYPE)
		{
			instructionTracing((VOID *) instructions[i].ip, NULL, 0, "x87error", out, shadowMemory);
		}

		if(instructions[i].memOperands == 0)
		{
			handleBaseInstBB(instructions[i], shadowMemory, out);
		}
		else if(instructions[i].memOperands < 3)
		{
			if( (memOpsCount+2) > contexts.addrs.size())
			{
				fprintf(out, "incomplete block data\n");

//				if(KnobDebugTrace)
				{
					fprintf(out, "Lacking Addrs %p\t%s\n", (void *) instructions[i].ip, debugData[instructions[i].ip].instruction.c_str());
					this->printBlock(out);
				}
				break;
			}

			if(contexts.pred[memOpsCount/2])
				handleMemInstBB(instructions[i], contexts.addrs[memOpsCount], contexts.addrs[memOpsCount+1], shadowMemory, out);
	
			memOpsCount += 2;
		}
		else
		{
			instructionTracing((VOID *) instructions[i].ip,NULL,0,"unhandledMemOp",out, shadowMemory);
		}
	}
	if(KnobDebugTrace)
	{
		fprintf(out, "Done Executing Block Ending %p\n", (void *) instructions.back().ip);
		this->printBlock(out);
		printAddrs(contexts.addrs, out);
	}
	contexts.addrs.clear();
}

VOID BBData::addSuccessors(ADDRINT s)
{
	successors.insert(s);
}

vector<ADDRINT> BBData::getAddrs()
{
	vector<ADDRINT> addrs;

	for(auto ins : instructions)
	{
		addrs.push_back(ins.ip);
	}
	return addrs;
}

BBData::BBData()
{
	execution_count = 0;
	expected_num_ins = 0;
}

BBData::~BBData()
{

}

void BBData::swap(BBData &s)
{
	vector<instructionLocationsData> tmp_ins = s.instructions;
	s.instructions = instructions;
	instructions = tmp_ins;

	set<ADDRINT>tmp_suc = s.successors;
	s.successors = successors;
	successors = tmp_suc;
	
	UINT32 tmp_count = s.execution_count;
	s.execution_count = execution_count;
	execution_count = tmp_count;
	
	USIZE tmp_expect = s.expected_num_ins;
	s.expected_num_ins = expected_num_ins;
	expected_num_ins = tmp_expect;
}

BBData& BBData::operator=(const BBData& s)
{
	BBData tmp(s);
	swap(tmp);
	return *this;
}

BBData& BBData::operator=(BBData&& rhs)
{
	swap(rhs);
	return *this;
}

BBData::BBData(const BBData& s)
{
	instructions = s.instructions;
	successors = s.successors;
	execution_count = s.execution_count;
	expected_num_ins = s.expected_num_ins;
}

int BBData::getNumSuccessors() const
{
	return successors.size();
}

int BBData::getNumInstuructions() const
{
	return instructions.size();
}

int BBData::getNumExecution() const
{
	return execution_count;
}
