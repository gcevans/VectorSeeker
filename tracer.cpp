/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2010 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */

#include "tracer.h"
#include "tracerlib.h"
#include "tracer_decode.h"
#include "resultvector.h"
#include "tracebb.h"
#include "instructions.h"
#include "tracer_decode.h"
#include "shadow.h"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <stdio.h>
#include <assert.h>


#define STATIC_CAST(x,y) ((x) (y))
/* ===================================================================== */
/* Names of malloc and free */
/* ===================================================================== */
#if defined(TARGET_MAC)
#define MALLOC "_malloc"
#define FREE "_free"
#else
#define MALLOC "malloc"
#define FREE "free"
#endif

#define TRACE_FUNCTION ""

#define MINTHRESHOLD 0
#define MINVECTORWIDTH 100

// Local Functions
VOID clearState();

// TLS globals
static  TLS_KEY tls_key;
PIN_LOCK lock;
INT32 numThreads = 0;

class thread_data_t
{
public:
	size_t malloc_size;
	UINT8 pad[64-sizeof(ADDRINT)];
};

// function to access thread-specific data
thread_data_t* get_tls(THREADID threadid)
{
    thread_data_t* tdata = 
          static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, threadid));
    return tdata;
}

bool sortBySecond(pair<long,long> i, pair<long,long> j)
{
	return(i.second > j.second);
}

bool instructionLocationsDataPointerSort(instructionLocationsData *a, instructionLocationsData *b)
{
	if(a->execution_count > b->execution_count)
		return true;
	else if(a->execution_count < b->execution_count)
		return false;
	else
	{
		int files_differ = a->file_name.compare(b->file_name);
		if(files_differ == 0)
			return(a->line_number < b->line_number);

		return(files_differ < 0);
	}
}

bool instructionLocationsDataPointerAddrSort(instructionLocationsData *a, instructionLocationsData *b)
{
	return(a->ip < b->ip);
}

// Shared Globals
unsigned instructionCount;

// Globals
unsigned tracinglevel;
map<ADDRINT,size_t> allocationMap;
ShadowMemory shadowMemory;
unordered_map<ADDRINT,instructionLocationsData > instructionLocations;
unsigned vectorInstructionCountSavings;
int traceRegionCount;
int InTraceFunction;
FILE * trace;
list<long long> loopStack;
unordered_map<ADDRINT, ResultVector > instructionResults;
unordered_map<ADDRINT, BBData> basicBlocks;
vector<pair<ADDRINT,UINT32> > rwAddressLog;
ADDRINT lastBB;
	
// Command line arguments
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "tracer.log", "specify output file name");

KNOB<string> KnobTraceFunction(KNOB_MODE_WRITEONCE, "pintool",
    "f", "main", "specify fucntion to start tracing");
    
KNOB<bool> KnobSummaryOn(KNOB_MODE_WRITEONCE, "pintool",
	"s", "0", "Enable summary data of whole run");

KNOB<bool> KnobVectorLineSummary(KNOB_MODE_WRITEONCE, "pintool",
	"l", "0", "Enable line summary");
	
KNOB<int> KnobMinVectorCount(KNOB_MODE_WRITEONCE, "pintool",
	"n", "100", "Line summary minimum count for vectorization");
	
KNOB<int> KnobTraceLimit(KNOB_MODE_WRITEONCE, "pintool",
	"trace-limit", "0", "Maximum number of trace regions 0 is unlimited");
	
KNOB<bool> KnobSkipMove(KNOB_MODE_WRITEONCE, "pintool",
	"m", "1", "Vectorize move instructions");

KNOB<bool> KnobDebugTrace(KNOB_MODE_WRITEONCE, "pintool",
	"D", "0", "Enable full debug trace");

KNOB<bool> KnobMallocPrinting(KNOB_MODE_WRITEONCE, "pintool",
	"log-malloc", "0", "log malloc calls");

KNOB<bool> KnobSupressMalloc(KNOB_MODE_WRITEONCE, "pintool",
	"no-malloc", "0", "Don't track malloc calls");

KNOB<bool> KnobForFrontend(KNOB_MODE_WRITEONCE, "pintool",
	"frontend", "0", "format for the frontend");

KNOB<bool> KnobForVectorSummary(KNOB_MODE_WRITEONCE, "pintool",
	"vector-summary", "0", "summerize vectors");
	
KNOB<bool> KnobForLoopScoping(KNOB_MODE_WRITEONCE, "pintool",
	"loop-scope", "0", "scope to loops using on and off functions");

KNOB<bool> KnobForShowNoFileInfo(KNOB_MODE_WRITEONCE, "pintool",
	"show-all", "0", "show instrucions with no file info");

KNOB<bool> KobForPrintBasicBlocks(KNOB_MODE_WRITEONCE, "pintool",
	"b", "0", "print all basic blocks traced at end of log");

bool memIsArray(VOID *addr)
{
	map<ADDRINT,size_t> ::iterator it;
	it = allocationMap.upper_bound((ADDRINT) addr);
	
	if((*it).first == (ADDRINT)addr)
	{
		return true;
	}
	else if(it != allocationMap.begin())
	{
		it--;
		if((size_t)(*it).first + (*it).second > (size_t)addr)
		{
			return true;
		}
	}
	return false;
}

VOID writeLog()
{
	char decodeBuffer[1024];
	bool linesFound = false;
	bool filesFound = false;
	if(!KnobForFrontend)
		fprintf(trace, "#start instruction log\n");
	
	unordered_map<ADDRINT,instructionLocationsData >::iterator ipit;
	vector<instructionLocationsData *> profile_list;
	map< string,map<int,vector<instructionLocationsData *> > >line_map;
	vector<instructionLocationsData *> *current_line;

	for(ipit = instructionLocations.begin(); ipit != instructionLocations.end(); ipit++)
	{
		profile_list.push_back(&(ipit->second));
		line_map[ipit->second.file_name][ipit->second.line_number].push_back(&(ipit->second));
	}
	
	sort(profile_list.begin(),profile_list.end(),instructionLocationsDataPointerSort);
	
	for(unsigned int i = 0; i < profile_list.size(); i++)
	{	
		linesFound = true;
		string file_name = profile_list[i]->file_name;
		if(file_name == "")
			file_name = "NO FILE INFORMATION";
		else
		{
			filesFound = true;
		}

		if(!(profile_list[i]->logged) && (file_name != "NO FILE INFORMATION" || KnobForShowNoFileInfo ) && (profile_list[i]->execution_count > 0))
		{
			std::ostringstream loopstackstring;
			for(std::list<long long>::reverse_iterator it = profile_list[i]->loopid.rbegin(); it != profile_list[i]->loopid.rend();)
			{
				loopstackstring << *it;
				it++;
				if(it != profile_list[i]->loopid.rend())
					loopstackstring << ',';
			}
				
			fprintf(trace,"%s,%d<%s>:%ld\n", file_name.c_str(),profile_list[i]->line_number,loopstackstring.str().c_str(),profile_list[i]->execution_count);
			current_line = &(line_map[profile_list[i]->file_name][profile_list[i]->line_number]);
			sort(current_line->begin(),current_line->end(),instructionLocationsDataPointerAddrSort);
			for(unsigned int j = 0; j < current_line->size(); j++)
			{
				if((*current_line)[j]->execution_count > 0)
				{
					(*current_line)[j]->logged = true;
					VOID *ip = (VOID *)(*current_line)[j]->ip;
					disassemblyToBuff(decodeBuffer, ip);
					int vector_count = -1;
					int current_vector_size = -1;
					bool once = false;
					map<long,long>::iterator timeit;
	
					fprintf(trace,"\t%p:%s:%s\n\t%s\n\t\t",ip,getInstCategoryString(ip),decodeBuffer,instructionLocations[(ADDRINT)ip].rtn_name.c_str());
	
					if(!KnobForVectorSummary)
					{
						vector<pair<long,long> >sorted_vectors;
						instructionResults[(ADDRINT) ip].sortedVectors(sorted_vectors);
						for(size_t i = 0; i < sorted_vectors.size(); i++)
						{
							fprintf(trace,"<%ld,%ld>",sorted_vectors[i].first,sorted_vectors[i].second);
						}
					}
					else
					{
						vector<pair<long,long> >sorted_vectors;
						instructionResults[(ADDRINT) ip].sortedVectors(sorted_vectors);
						for(size_t i = 0; i < sorted_vectors.size(); i++)
						{
							if(!once)
							{
								once = true;
								current_vector_size = sorted_vectors[i].second;
								vector_count = 0;
							}
							if(current_vector_size != sorted_vectors[i].second)
							{
								if(vector_count != 1)
									fprintf(trace,"x%d",vector_count);
	
								vector_count = 0;
								current_vector_size = sorted_vectors[i].second;
							}
							if(vector_count == 0)
							{
								fprintf(trace,"<%ld,%ld>",sorted_vectors[i].first,sorted_vectors[i].second);
							}
							vector_count++;
							vectorInstructionCountSavings = vectorInstructionCountSavings + (sorted_vectors[i].second - 1);
						}
						if(vector_count != 1)
							fprintf(trace,"x%d",vector_count);
					}
					fprintf(trace,"\n");
				}
			}
		}
	}
	if(!KnobForFrontend)
	{	
		if( linesFound && !KnobForShowNoFileInfo && !filesFound)
			fprintf(trace, "Insturctions were found but they had no debug info on source file or were only mov instructions\n");
		fprintf(trace, "#end instruction log\n");
	}
}

VOID writeOnOffLog()
{
	if(!KnobForFrontend)
		fprintf(trace, "#start instruction log\n");
	unordered_map<ADDRINT,instructionLocationsData >::iterator ipit;
	vector<instructionLocationsData *> profile_list;
	map< string,map<int,vector<instructionLocationsData *> > >line_map;
	vector<instructionLocationsData *> *current_line;

	for(ipit = instructionLocations.begin(); ipit != instructionLocations.end(); ipit++)
	{
		profile_list.push_back(&(ipit->second));
		line_map[ipit->second.file_name][ipit->second.line_number].push_back(&(ipit->second));
	}
	
	sort(profile_list.begin(),profile_list.end(),instructionLocationsDataPointerSort);
	
	for(unsigned int i = 0; i < profile_list.size(); i++)
	{	
		if(!(profile_list[i]->logged) && (profile_list[i]->file_name != "") && (profile_list[i]->execution_count > 0))
		{
			bool isVectorizable = false;
			bool isNotVectorizable = false;
			current_line = &(line_map[profile_list[i]->file_name][profile_list[i]->line_number]);
			sort(current_line->begin(),current_line->end(),instructionLocationsDataPointerAddrSort);
			for(unsigned int j = 0; j < current_line->size(); j++)
			{
				(*current_line)[j]->logged = true;
				if((*current_line)[j]->execution_count > 0)
				{
					bool noVectorGreaterThenOne = true;
					if(instructionResults[(*current_line)[j]->ip].vectorsGreater(KnobMinVectorCount.Value()))
					{
						isVectorizable = true;
						noVectorGreaterThenOne = false;
					}
					if(noVectorGreaterThenOne)
						isNotVectorizable = true;
				}
			}
			//
			if(isVectorizable && !isNotVectorizable)
				fprintf(trace,"V:%s,%d:%ld\n", profile_list[i]->file_name.c_str(),profile_list[i]->line_number,profile_list[i]->execution_count);
			else if(isVectorizable && isNotVectorizable)
				fprintf(trace,"P:%s,%d:%ld\n", profile_list[i]->file_name.c_str(),profile_list[i]->line_number,profile_list[i]->execution_count);
			else
				fprintf(trace,"N:%s,%d:%ld\n", profile_list[i]->file_name.c_str(),profile_list[i]->line_number,profile_list[i]->execution_count);
		}
	}
	if(!KnobForFrontend)
		fprintf(trace, "#end instruction log\n");
}


// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
	if(tracinglevel != 0)
	{
		fprintf(trace,"Progam ended with tracing on\n");
		if(!KnobVectorLineSummary)
			writeLog();
		else
			writeOnOffLog();
	}
	if(KnobSummaryOn)
	{
		fprintf(trace, "#Start Summary\n");
		fprintf(trace, "#Total Instructions (No Vector Instructions) = %u\n", instructionCount);
		fprintf(trace, "#Total Instructions (Vector Instructions) = %u\n", instructionCount - vectorInstructionCountSavings);
	}
	// print basic block info
	if(KobForPrintBasicBlocks)
	{
		for(auto it = basicBlocks.begin(); it != basicBlocks.end(); it++)
		{
			(*it).second.printBlock(trace);
		}
	}
	fclose(trace);
}

VOID recoredBaseInst(VOID *ip)
{
	if(tracinglevel == 0)
		return;

	instructionCount++;
		
	long value = 0;
	instructionLocationsData *current_instruction = &(instructionLocations[(ADDRINT)ip]);
	
	for(unsigned int i = 0; i < current_instruction->registers_read.size(); i++)
	{
		value = max(shadowMemory.readReg(current_instruction->registers_read[i]),value);
	}
	
	if(value > 0)
	{
		value = value +1;
	}

	for(unsigned int i = 0; i < current_instruction->registers_written.size(); i++)
	{
		shadowMemory.writeReg(current_instruction->registers_written[i], value);
	}
	
	if(value > 0)
	{
		instructionResults[(ADDRINT)ip].addToDepth(value);
		current_instruction->execution_count += 1;
		current_instruction->loopid = loopStack;
//		fprintf(trace,"current_instruction->loopid size = %d\n", (int) current_instruction->loopid.size());
	}
	if(!KnobDebugTrace)
	return;

	instructionTracing(ip,NULL,value,"recoredBaseInst",trace,shadowMemory);
}

VOID traceBaseInst(VOID *ip)
{
	if(tracinglevel == 0)
		return;

	instructionCount++;
		
	long value = 0;
	instructionLocationsData *current_instruction = &(instructionLocations[(ADDRINT)ip]);
	
	for(unsigned int i = 0; i < current_instruction->registers_read.size(); i++)
	{
		value = max(shadowMemory.readReg(current_instruction->registers_read[i]),value);
	}
	
	if(value > 0)
	{
		value = value +1;
	}

	for(unsigned int i = 0; i < current_instruction->registers_written.size(); i++)
	{
		shadowMemory.writeReg(current_instruction->registers_written[i], value);
	}

	if(!KnobDebugTrace)
		return;
	
	instructionTracing(ip,NULL,value,"traceBaseInst",trace, shadowMemory);
}

/*
VOID handleX87Inst(VOID *ip)
{
	if(tracinglevel == 0)
		return;

	instructionCount++;

	instructionTracing(ip,NULL,0,"x87error",trace, shadowMemory);
}

*/

VOID unhandledMemOp(VOID *ip)
{
	if(tracinglevel == 0)
		return;

	instructionCount++;

	instructionTracing(ip,NULL,0,"unhandledMemOp",trace, shadowMemory);
}

const UINT32 NONE_OPERATOR_TYPE = 0;
const UINT32 READ_OPERATOR_TYPE = 1;
const UINT32 WRITE_OPERATOR_TYPE = 2;
const UINT32 BOTH_OPERATOR_TYPE = 3;

VOID RecordMemReadWrite(VOID * ip, VOID * addr1, UINT32 t1, VOID *addr2, UINT32 t2)
{
	UINT32 type1 = t1;
	UINT32 type2 = t2;

	if(tracinglevel == 0)
		return;

	rwAddressLog.push_back( make_pair((ADDRINT) addr1, t1) );
	rwAddressLog.push_back( make_pair((ADDRINT) addr2, t2) );
//	fprintf(trace, "two ops written for %p\n", ip);

	instructionCount++;
	
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

	instructionLocationsData *current_instruction = &(instructionLocations[(ADDRINT)ip]);

//	fprintf(trace, "After all memory reads value = %ld\n", value);

	for(unsigned int i = 0; i < current_instruction->registers_read.size(); i++)
	{
		value = max(shadowMemory.readReg(current_instruction->registers_read[i]),value);
	}

//	fprintf(trace, "After all reads value = %ld\n", value);
		
	if(value > 0)
	{
		value = value +1;
	}

	for(unsigned int i = 0; i < current_instruction->registers_written.size(); i++)
	{
		shadowMemory.writeReg(current_instruction->registers_written[i], value);
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

	if(value > 0)
	{
		instructionResults[(ADDRINT)ip].addToDepth(value);
		current_instruction->execution_count += 1;
		current_instruction->loopid = loopStack;
	}

	if(!KnobDebugTrace)
		return;
		
	instructionTracing(ip,addr2,value,"RecordMemReadWrite",trace, shadowMemory);
}

VOID traceMemReadWrite(VOID * ip, VOID * addr1, UINT32 t1, VOID *addr2, UINT32 t2)
{
	UINT32 type1 = t1;
	UINT32 type2 = t2;

	if(tracinglevel == 0)
		return;

	rwAddressLog.push_back( make_pair((ADDRINT) addr1, t1) );
	rwAddressLog.push_back( make_pair((ADDRINT) addr2, t2) );
//	fprintf(trace, "two ops written for %p\n", ip);

	instructionCount++;
	
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

	instructionLocationsData *current_instruction = &(instructionLocations[(ADDRINT)ip]);

	for(unsigned int i = 0; i < current_instruction->registers_read.size(); i++)
	{
		value = max(shadowMemory.readReg(current_instruction->registers_read[i]),value);
	}

//	fprintf(trace, "After all reads value = %ld\n", value);

	if(value > 0)
	{
		value = value +1;
	}

	for(unsigned int i = 0; i < current_instruction->registers_written.size(); i++)
	{
		shadowMemory.writeReg(current_instruction->registers_written[i], value);
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

	if(!KnobDebugTrace)
		return;
		
	instructionTracing(ip,addr1,value,"traceMemReadWrite",trace, shadowMemory);
}

VOID blockTracer(VOID *ip)
{
	if(tracinglevel && lastBB)
	{
		basicBlocks[lastBB].execute(rwAddressLog, shadowMemory, trace);
		basicBlocks[lastBB].addSuccessors((ADDRINT) ip);
		if(KnobDebugTrace)
		{
			fprintf(trace, "BBL %p executed\n", (void *) lastBB);
			basicBlocks[lastBB].printBlock(trace);
		}
	}
	lastBB = (ADDRINT) ip;
}

VOID Trace(TRACE pintrace, VOID *v)
{
    for (BBL bbl = TRACE_BblHead(pintrace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
    	// Only instrument instructions in a vaid RTN that is neither malloc or free
    	if(RTN_Valid(INS_Rtn(BBL_InsHead(bbl))))
    	{
	    	string rtn_name = RTN_Name(INS_Rtn(BBL_InsHead(bbl)));
	    	if( (rtn_name != MALLOC) && (rtn_name != FREE))
	    	{
	    		BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)blockTracer, IARG_INST_PTR, IARG_END);
	    		// insturment each instruction in the curent basic block
		    	for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins) )
		    	{
					ADDRINT ip = LEVEL_PINCLIENT::INS_Address(ins);
		    		if(INS_IsOriginal(ins))
					{
						instructionType insType;
						insType = decodeInstructionData(ip, instructionLocations);
						instructionLocations[ip].type = insType;
						instructionLocations[ip].rtn_name = rtn_name;
						UINT32 memOperands = INS_MemoryOperandCount(ins);

						if(insType == IGNORED_INS_TYPE)
							continue;

						// if(insType == X87_INS_TYPE)
						// {
						// 	INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)handleX87Inst, IARG_INST_PTR, IARG_END);
						// }

						// Instruments memory accesses using a predicated call, i.e.
						// the instrumentation is called iff the instruction will actually be executed.
						//
						// The IA-64 architecture has explicitly predicated instructions. 
						// On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
						// prefixed instructions appear as predicated instructions in Pin.
						
						if(memOperands == 0)
						{
							if(!((insType == MOVEONLY_INS_TYPE) && KnobSkipMove))
							{
								INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)recoredBaseInst, IARG_INST_PTR, IARG_END);
							}
							else
							{
								INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)traceBaseInst, IARG_INST_PTR, IARG_END);
							}
						}
						else if( memOperands < 3)
						{
							UINT32 addr1 = 0;
							UINT32 type1 = NONE_OPERATOR_TYPE;
							UINT32 addr2 = 0;
							UINT32 type2 = NONE_OPERATOR_TYPE;
							UINT32 memOp= 0;
							
							if(INS_MemoryOperandIsRead(ins, memOp))
								type1 = type1 | READ_OPERATOR_TYPE;
							if(INS_MemoryOperandIsWritten(ins, memOp))
								type1 = type1 | WRITE_OPERATOR_TYPE;
							
							if(memOperands > 1)
							{
								memOp = 1;
								if(INS_MemoryOperandIsRead(ins, memOp))
									type2 = type2 | READ_OPERATOR_TYPE;
								if(INS_MemoryOperandIsWritten(ins, memOp))
									type2 = type2 | WRITE_OPERATOR_TYPE;
							}

							if(((insType == MOVEONLY_INS_TYPE) && KnobSkipMove))
							{
								INS_InsertPredicatedCall(
									ins, IPOINT_BEFORE, (AFUNPTR)traceMemReadWrite,
									IARG_INST_PTR,
									IARG_MEMORYOP_EA, addr1,
									IARG_UINT32, type1,
									IARG_MEMORYOP_EA, addr2,
									IARG_UINT32, type2,
									IARG_END);
							}
							else
							{
								INS_InsertPredicatedCall(
									ins, IPOINT_BEFORE, (AFUNPTR)RecordMemReadWrite,
									IARG_INST_PTR,
									IARG_MEMORYOP_EA, addr1,
									IARG_UINT32, type1,
									IARG_MEMORYOP_EA, addr2,
									IARG_UINT32, type2,
									IARG_END);
							}			
						}
						else
						{
							INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)unhandledMemOp, IARG_INST_PTR, IARG_END);

						}
						//push instruction on current basic block
						basicBlocks[BBL_Address(bbl)].pushInstruction(instructionLocations[ip]);
					}
					else
					{
						fprintf(trace, "Instruction %p not original\n", (void *) ip);
					}
				}
	    	}
		}
    }
}

// Malloc Stuff
VOID MallocBefore(CHAR * name, ADDRINT size, THREADID threadid)
{
	if(KnobSupressMalloc)
		return;
	thread_data_t* tdata = get_tls(threadid);
	tdata->malloc_size = size;
}

// Free case
VOID FreeBefore(CHAR * name, ADDRINT start, THREADID threadid)
{
	if(KnobSupressMalloc)
		return;
	if(tracinglevel)
	{
		for(size_t i = 0; i < allocationMap[start]; i++)
			shadowMemory.writeMem(start+i, 0);		
	}
		
	allocationMap.erase(start);
}

// tracing on
VOID traceOn(CHAR * name, ADDRINT start, THREADID threadid)
{
	if(traceRegionCount > KnobTraceLimit.Value())
		return;

	if(tracinglevel == 0 && (KnobTraceLimit.Value() != 0))
	{
		traceRegionCount++;
	}
	if(tracinglevel == 0)
		clearState();

	tracinglevel++;
	if(!KnobForFrontend)
	    fprintf(trace,"tracing turned on\n");
}

void clearRegisters()
{
	for(int i = 0; i < XED_REG_LAST; i++)
		shadowMemory.writeReg(i, 0);
}

void clearVectors()
{
	unordered_map<ADDRINT,instructionLocationsData >::iterator it;
	for(it = instructionLocations.begin(); it != instructionLocations.end(); it++)
	{
		it->second.logged = false;
		it->second.execution_count = 0;
	}
	instructionResults.clear();

}

// Clear state to just initialized vectors
VOID clearState()
{
	map<ADDRINT,size_t>::iterator it;
	clearVectors();
	clearRegisters();
	shadowMemory.clear();
	// Set Vector Memory
	for(it = allocationMap.begin(); it != allocationMap.end(); it++)
	{
		size_t start = it->first;
		for(size_t i = 0; i < allocationMap[start]; i++)
			shadowMemory.writeMem(start+i, 1);
	}
	lastBB = NULL; //this may be the wrong place
	rwAddressLog.clear();
}

// tracing off
VOID traceOff(CHAR * name, ADDRINT start, THREADID threadid)
{
	if(traceRegionCount > KnobTraceLimit.Value())
		return;

	if(!KnobForFrontend)
	    fprintf(trace,"tracing turned off\n");
	if(tracinglevel < 1)
	{
		if(!KnobForFrontend)
			fprintf(trace,"Trace off called with tracing off\n");
	}
	else if(tracinglevel == 1)
	{
		shadowMemory.clear();
		if(!KnobVectorLineSummary)
		{
			writeLog();
		}
		else
		{
			writeOnOffLog();
		}
		tracinglevel = 0;
	}
	else
	{
		tracinglevel--;
	}
}

VOID traceFunctionEntry(CHAR * name, ADDRINT start, THREADID threadid)
{
	if(KnobForLoopScoping)
	{
		InTraceFunction++;
	}
	else
	{
		traceOn(name, start, threadid);
	}
}

VOID traceFunctionExit(CHAR * name, ADDRINT start, THREADID threadid)
{
	if(KnobForLoopScoping && (InTraceFunction != 0))
	{
		InTraceFunction--;
	}
	else
	{
		traceOff(name,start,threadid);
	}
}

// memory allocations other then from malloc
VOID arrayMem(ADDRINT start, size_t size)
{
	if(KnobMallocPrinting)
		fprintf(trace,"Memory Allocation Found start = %p size = %p\n", (void *) start, (void *) size);
	
	if(size > 0)
		allocationMap[start] = size;
	
	if(tracinglevel)
		for(size_t i = 0; i < size; i++)
			shadowMemory.writeMem(start+i, 1);
}

VOID arrayMemClear(ADDRINT start)
{
	if(KnobMallocPrinting)
		fprintf(trace,"Memory Allocation Found cleared = %p\n", (void *) start);

	for(size_t i = 0; i < allocationMap[start]; i++)
		shadowMemory.writeMem(start+i, 0);
	
	allocationMap.erase(start);
}

VOID loopStart(ADDRINT id)
{
	loopStack.push_front((long long) id);
}

VOID loopEnd(ADDRINT id)
{
	loopStack.pop_front();
}

VOID MallocAfter(ADDRINT ret, THREADID threadid)
{
	if(KnobSupressMalloc)
		return;
	
	thread_data_t* tdata = get_tls(threadid);
		
	//Map version
	if(tdata->malloc_size > 0)
		allocationMap[ret] = tdata->malloc_size;
		
	if(tracinglevel > 0)
		for(size_t i = 0; i < allocationMap[ret]; i++)
			shadowMemory.writeMem(ret+i, 1);

	if(KnobMallocPrinting)
	    fprintf(trace,"malloc returns %p of size(%p)\n",(void *)ret,(void *)tdata->malloc_size);
}

VOID Image(IMG img, VOID *v)
{
    // Instrument the malloc() and free() functions.  Print the input argument
    // of each malloc() or free(), and the return value of malloc().
    //
    //  Find the malloc() function.
    RTN mallocRtn = RTN_FindByName(img, MALLOC);
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)MallocBefore,
                       IARG_ADDRINT, MALLOC,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR)MallocAfter,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);

        RTN_Close(mallocRtn);
    }

    // Find the free() function.
    RTN freeRtn = RTN_FindByName(img, FREE);
    if (RTN_Valid(freeRtn))
    {
        RTN_Open(freeRtn);
        // Instrument free() to print the input argument value.
        RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR)FreeBefore,
                       IARG_ADDRINT, FREE,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(freeRtn);
    }
    //Enable Tracing
    RTN traceRtn = RTN_FindByName(img, TRACE_ON);
    if (RTN_Valid(traceRtn))
    {
        RTN_Open(traceRtn);
        RTN_InsertCall(traceRtn, IPOINT_BEFORE, (AFUNPTR)traceOn,
                       IARG_ADDRINT, FREE,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(traceRtn);
    }
    //End Tracing
    traceRtn = RTN_FindByName(img, TRACE_OFF);
    if (RTN_Valid(traceRtn))
    {
        RTN_Open(traceRtn);
        RTN_InsertCall(traceRtn, IPOINT_BEFORE, (AFUNPTR)traceOff,
                       IARG_ADDRINT, FREE,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(traceRtn);
    }
    traceRtn = RTN_FindByName(img, ARRAY_MEM);
    if (RTN_Valid(traceRtn))
    {
        RTN_Open(traceRtn);
        RTN_InsertCall(traceRtn, IPOINT_BEFORE, (AFUNPTR)arrayMem,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_END);
        RTN_Close(traceRtn);
    }
    traceRtn = RTN_FindByName(img, ARRAY_MEM_CLEAR);
    if (RTN_Valid(traceRtn))
    {
        RTN_Open(traceRtn);
        RTN_InsertCall(traceRtn, IPOINT_BEFORE, (AFUNPTR)arrayMemClear,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_Close(traceRtn);
    }
    traceRtn = RTN_FindByName(img, LOOP_START);
    if (RTN_Valid(traceRtn))
    {
        RTN_Open(traceRtn);
        RTN_InsertCall(traceRtn, IPOINT_BEFORE, (AFUNPTR)loopStart,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_Close(traceRtn);
    }
    traceRtn = RTN_FindByName(img, LOOP_END);
    if (RTN_Valid(traceRtn))
    {
        RTN_Open(traceRtn);
        RTN_InsertCall(traceRtn, IPOINT_BEFORE, (AFUNPTR)loopEnd,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_Close(traceRtn);
    }
    traceRtn = RTN_FindByName(img, KnobTraceFunction.Value().c_str());
    if (RTN_Valid(traceRtn))
    {
        RTN_Open(traceRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(traceRtn, IPOINT_BEFORE, (AFUNPTR)traceFunctionEntry,
                       IARG_ADDRINT, MALLOC,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_InsertCall(traceRtn, IPOINT_AFTER, (AFUNPTR)traceFunctionExit,
                       IARG_ADDRINT, MALLOC,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID,
                       IARG_END);

        RTN_Close(traceRtn);
    }
}


VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    PIN_GetLock(&lock, threadid+1);
    numThreads++;
    PIN_ReleaseLock(&lock);

    thread_data_t* tdata = new thread_data_t;

    PIN_SetThreadData(tls_key, tdata, threadid);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("This Pintool finds vectors by greedy seach of the dataflow graph\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize pin
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) return Usage();

	// Initialize Xed decoder
	xed_tables_init();

	// Initialize VectorSeeker globals
	trace = fopen(KnobOutputFile.Value().c_str(), "w");
	tracinglevel = 0;
	instructionCount = 0;
	vectorInstructionCountSavings = 0;
	traceRegionCount = 0;
	loopStack.push_front(-1);
	clearState();

    // Initialize the lock
    PIN_InitLock(&lock);
    
	// Obtain  a key for TLS storage.
	tls_key = PIN_CreateThreadDataKey(0);
	
    // Register ThreadStart to be called when a thread starts.
    PIN_AddThreadStartFunction(ThreadStart, 0);

    // Register Instruction to be called to instrument instructions
//	RTN_AddInstrumentFunction(Routine, 0);
	IMG_AddInstrumentFunction(Image, 0);
	TRACE_AddInstrumentFunction(Trace, 0);


    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
