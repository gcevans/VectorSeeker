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

// Shared Globals
unsigned instructionCount;

// Globals
unsigned tracinglevel;
int InTraceFunction;

ShadowMemory shadowMemory;

unordered_map<ADDRINT,instructionLocationsData > instructionLocations;
unordered_map<ADDRINT, instructionDebugData> debugData;
unordered_map<ADDRINT, ResultVector > instructionResults;
unsigned vectorInstructionCountSavings;
int traceRegionCount;

FILE * trace;

list<long long> loopStack;

unordered_map<ADDRINT, BBData> basicBlocks;
vector<pair<ADDRINT,UINT32> > rwAddressLog;
ADDRINT lastBB;
ADDRINT UBBID;

// BBL Debug Globals
FILE *bbl_log;

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
		int files_differ = debugData[a->ip].file_name.compare(debugData[b->ip].file_name);
		if(files_differ == 0)
			return(debugData[a->ip].line_number < debugData[b->ip].line_number);

		return(files_differ < 0);
	}
}

bool instructionLocationsDataPointerAddrSort(instructionLocationsData *a, instructionLocationsData *b)
{
	return(a->ip < b->ip);
}

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
	"m", "1", "Try to vectorize move instructions");

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
	
KNOB<bool> KnobForShowNoFileInfo(KNOB_MODE_WRITEONCE, "pintool",
	"show-all", "0", "show instrucions with no file info");

KNOB<bool> KobForPrintBasicBlocks(KNOB_MODE_WRITEONCE, "pintool",
	"print-bb", "0", "print all basic blocks traced at end of log");

KNOB<bool> KnobBBVerstion(KNOB_MODE_WRITEONCE, "pintool",
	"bb", "0", "Use PIN basic block mode");

KNOB<bool> KnobBBDotLog(KNOB_MODE_WRITEONCE, "pintool",
	"bb-dot", "0", "Log Pin Basic Blocks in dot format");


void buildOutputMaps(vector<instructionLocationsData *> &pmap, map< string,map<int,vector<instructionLocationsData *>>> &lmap)
{
	for(auto ipit = instructionLocations.begin(); ipit != instructionLocations.end(); ++ipit)
	{
		pmap.push_back(&(ipit->second));
		lmap[debugData[ipit->second.ip].file_name][debugData[ipit->second.ip].line_number].push_back(&(ipit->second));
	}
	
	sort(pmap.begin(),pmap.end(),instructionLocationsDataPointerSort);
}


VOID writeLog()
{
	char decodeBuffer[1024];
	bool linesFound = false;
	bool filesFound = false;	
	vector<instructionLocationsData *> profile_list; // sorted instructions
	map< string,map<int,vector<instructionLocationsData *>>>line_map; // map of lines to instuctions
	vector<instructionLocationsData *> *current_line; // instructions in the current line

	if(!KnobForFrontend)
		fprintf(trace, "#start instruction log\n");

	buildOutputMaps(profile_list,line_map);

	for(auto ins : profile_list)
	{
		linesFound = true;

		string file_name = debugData[ins->ip].file_name;
		int line_number = debugData[ins->ip].line_number;

		if(file_name == "")
			file_name = "NO FILE INFORMATION";
		else
		{
			filesFound = true;
		}

		if(!(ins->logged) && (file_name != "NO FILE INFORMATION" || KnobForShowNoFileInfo ) && (ins->execution_count > 0))
		{
			std::ostringstream loopstackstring;
			for(std::list<long long>::reverse_iterator it = ins->loopid.rbegin(); it != ins->loopid.rend();)
			{
				loopstackstring << *it;
				it++;
				if(it != ins->loopid.rend())
					loopstackstring << ',';
			}
				
			fprintf(trace,"%s,%d<%s>:%ld\n", file_name.c_str(),line_number,loopstackstring.str().c_str(),ins->execution_count);

			current_line = &(line_map[debugData[ins->ip].file_name][line_number]);
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
	vector<instructionLocationsData *> profile_list;
	map< string,map<int,vector<instructionLocationsData *> > >line_map;
	vector<instructionLocationsData *> *current_line;

	buildOutputMaps(profile_list,line_map);

	for(auto ins : profile_list)
	{	
		string file_name = debugData[ins->ip].file_name;
		int line_number = debugData[ins->ip].line_number;

		if(!(ins->logged) && (file_name != "") && (ins->execution_count > 0))
		{
			bool isVectorizable = false;
			bool isNotVectorizable = false;
			current_line = &(line_map[debugData[ins->ip].file_name][line_number]);
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

			if(isVectorizable && !isNotVectorizable)
				fprintf(trace,"V:%s,%d:%ld\n", file_name.c_str(),line_number,ins->execution_count);
			else if(isVectorizable && isNotVectorizable)
				fprintf(trace,"P:%s,%d:%ld\n", file_name.c_str(),line_number,ins->execution_count);
			else
				fprintf(trace,"N:%s,%d:%ld\n", file_name.c_str(),line_number,ins->execution_count);
		}
	}
	if(!KnobForFrontend)
		fprintf(trace, "#end instruction log\n");
}

void WriteBBDotLog(FILE *out)
{
	for(auto it = basicBlocks.begin(); it != basicBlocks.end(); it++)
	{
		(*it).second.printBlock(out);
	}

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
	if(KnobBBDotLog)
	{
		WriteBBDotLog(bbl_log);
	}
	fclose(trace);
}

VOID recoredBaseInst(VOID *ip)
{
	if(tracinglevel == 0)
		return;

	// char buff[256];
	// disassemblyToBuff(buff, (void *) ip);
	// fprintf(trace, "%p\t%s\n", (void *) ip, buff);

	instructionCount++;
		
	long value = 0;
	instructionLocationsData *current_instruction = &(instructionLocations[(ADDRINT)ip]);
	
	for(unsigned int i = 0; i < current_instruction->registers_read.size(); i++)
	{
		value = max(shadowMemory.readReg(current_instruction->registers_read[i]),value);
	}
	
	if(value > 0 && (current_instruction->type != MOVEONLY_INS_TYPE))
	{
		value = value +1;
	}

	for(unsigned int i = 0; i < current_instruction->registers_written.size(); i++)
	{
		shadowMemory.writeReg(current_instruction->registers_written[i], value);
	}
	
	if(value > 0 && (!((current_instruction->type == MOVEONLY_INS_TYPE) && KnobSkipMove)))
	{
		instructionResults[(ADDRINT)ip].addToDepth(value);
		current_instruction->execution_count += 1;
		current_instruction->loopid = loopStack;
	}
	if(KnobDebugTrace)
		instructionTracing(ip,NULL,value,"Base",trace,shadowMemory);
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

	// char buff[256];
	// disassemblyToBuff(buff, (void *) ip);
	// fprintf(trace, "%p\t%s addr1 = %p addr2 = %p\n", (void *) ip, buff, addr1, addr2);


	if(KnobBBVerstion)
	{
		rwAddressLog.push_back( make_pair((ADDRINT) addr1, t1) );
		rwAddressLog.push_back( make_pair((ADDRINT) addr2, t2) );
		// fprintf(trace, "two ops written for %p\n", ip);
		return;
	}

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
		
	if(value > 0 && (current_instruction->type != MOVEONLY_INS_TYPE))
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
		if(shadowMemory.memIsArray(addr1))
			region1 = 1;
		else
			region1 = 0;

		shadowMemory.writeMem((ADDRINT)addr1, max(value,region1));
//		fprintf(trace,"W1[%p]=%ld\n",addr1,shadowMemory[(ADDRINT)addr1]);
	}

	if(type2 & WRITE_OPERATOR_TYPE)
	{
		if(shadowMemory.memIsArray(addr2))
			region2 = 1;
		else
			region2 = 0;

		shadowMemory.writeMem((ADDRINT)addr2, max(value,region2));
//		fprintf(trace,"W2[%p]=%ld\n",addr2,shadowMemory[(ADDRINT)addr2]);
	}

	value = max(max(value,region1),region2);

	if(value > 0 && !(((current_instruction->type == MOVEONLY_INS_TYPE) && KnobSkipMove)))
	{
		instructionResults[(ADDRINT)ip].addToDepth(value);
		current_instruction->execution_count += 1;
		current_instruction->loopid = loopStack;
	}

	if(KnobDebugTrace)
	{
		instructionTracing(ip,addr2,value,"Mem",trace, shadowMemory);
		fprintf(trace,"<%p,%u><%p,%u>", addr1, type1, addr2, type2);
		// shadowMemory.printAllocationMap(trace);
	}
}

VOID blockTracer(VOID *ip, ADDRINT id)
{
	if(!KnobBBVerstion)
		return;

	if(tracinglevel && lastBB)
	{
		// fprintf(trace, "Call UBBID %p\n", (void *) lastBB );
		basicBlocks[lastBB].execute(rwAddressLog, shadowMemory, trace);
		basicBlocks[lastBB].addSuccessors((ADDRINT) ip);
		if(KnobDebugTrace)
		{
			fprintf(trace, "BBL %p executed\n", (void *) lastBB);
		//	basicBlocks[lastBB].printBlock(trace);
		}
	}
	lastBB = id;
}


void logBasicBlock(BBL bbl, ADDRINT id)
{
	fprintf(bbl_log, "Basic Block starting at %p ending %p with Addresss %p UBBID %p\n",(void *) INS_Address(BBL_InsHead(bbl)), (void *) INS_Address(BBL_InsTail(bbl)), (void *) BBL_Address(bbl), (void *) id);
	for(auto ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins) )
	{
		fprintf(bbl_log, "%p\t%s\n", (void *) INS_Address(ins), INS_Disassemble(ins).c_str());
	}
	fprintf(bbl_log, "Baisc Block Finished\n");
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
	    		if(BBL_Original(bbl))
	    		{
	    			UBBID++;
	    			if(KnobDebugTrace)
	    			{
		    			logBasicBlock(bbl, UBBID);
	    			}
		    		BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)blockTracer, IARG_INST_PTR, IARG_ADDRINT, UBBID, IARG_CALL_ORDER, CALL_ORDER_FIRST-5, IARG_END);
		    		basicBlocks[UBBID].expected_num_ins = BBL_NumIns(bbl);
		    		// insturment each instruction in the curent basic block
			    	for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins) )
			    	{
						ADDRINT ip = INS_Address(ins);

			    		if(INS_IsOriginal(ins))
						{
							instructionType insType;
							insType = decodeInstructionData(ip, instructionLocations);
							instructionLocations[ip].type = insType;
							instructionLocations[ip].rtn_name = rtn_name;
							UINT32 memOperands = INS_MemoryOperandCount(ins);
							instructionLocations[ip].memOperands = memOperands;

							if(insType == IGNORED_INS_TYPE)
								continue;

							// Instruments memory accesses using a predicated call, i.e.
							// the instrumentation is called iff the instruction will actually be executed.
							//
							// The IA-64 architecture has explicitly predicated instructions. 
							// On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
							// prefixed instructions appear as predicated instructions in Pin.
							
							if(memOperands == 0)
							{
								if(!KnobBBVerstion)
									INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)recoredBaseInst, IARG_INST_PTR, IARG_CALL_ORDER, CALL_ORDER_FIRST, IARG_END);
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

								INS_InsertPredicatedCall(
									ins, IPOINT_BEFORE, (AFUNPTR)RecordMemReadWrite,
									IARG_INST_PTR,
									IARG_MEMORYOP_EA, addr1,
									IARG_UINT32, type1,
									IARG_MEMORYOP_EA, addr2,
									IARG_UINT32, type2,
									IARG_CALL_ORDER, CALL_ORDER_FIRST,
									IARG_END);
							}
							else
							{
								assert(false);
							}

							//push instruction on current basic block
							basicBlocks[UBBID].pushInstruction(instructionLocations[ip]);
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

	shadowMemory.arrayMemClear(start);
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

void clearVectors()
{
	for(auto it = instructionLocations.begin(); it != instructionLocations.end(); ++it)
	{
		it->second.logged = false;
		it->second.execution_count = 0;
	}
	instructionResults.clear();
}

// Clear state to just initialized vectors
VOID clearState()
{
	clearVectors();
	shadowMemory.clear();
	
	lastBB = 0; //this may be the wrong place
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
		blockTracer(NULL, 0); // trace final block
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
	traceOn(name, start, threadid);
}

VOID traceFunctionExit(CHAR * name, ADDRINT start, THREADID threadid)
{
	traceOff(name,start,threadid);
}

// memory allocations other then from malloc
VOID arrayMem(ADDRINT start, size_t size)
{
	if(KnobMallocPrinting)
		fprintf(trace,"Memory Allocation Found start = %p size = %p\n", (void *) start, (void *) size);

	shadowMemory.arrayMem(start, size, tracinglevel);
	
}

VOID arrayMemClear(ADDRINT start)
{
	if(KnobMallocPrinting)
		fprintf(trace,"Memory Allocation Found cleared = %p\n", (void *) start);

	shadowMemory.arrayMemClear(start);
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
		shadowMemory.arrayMem(ret, tdata->malloc_size, tracinglevel);

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
        RTN_InsertCall(traceRtn, IPOINT_AFTER, (AFUNPTR)traceOff,
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
	if(KnobDebugTrace || KnobBBDotLog)
	{
		bbl_log = fopen("bbl.log", "w");
	}
	UBBID = 0;
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
