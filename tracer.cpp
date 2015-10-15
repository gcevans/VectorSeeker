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
#include "output.h"
#include "threads.h"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <stdio.h>
#include <assert.h>
#include <climits>


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

#ifdef NOSHAODWCACHE
ShadowMemoryNoCache shadowMemory;
#else
ShadowMemory shadowMemory;
#endif

unordered_map<ADDRINT,instructionLocationsData > instructionLocations;
const unordered_map<ADDRINT,instructionLocationsData > &constInstructionLocations = instructionLocations;
unordered_map<ADDRINT, instructionDebugData> debugData;
unordered_map<ADDRINT, ResultVector > instructionResults;
unsigned vectorInstructionCountSavings;
int traceRegionCount;
bool inAlloc;

// Output Files
FILE * trace;
FILE *bbl_log;

#ifdef LOOPSTACK
list<long long> loopStack;
#endif

//unordered_map<size_t, BBData> basicBlocks;
vector<BBData> basicBlocks;
BBData empty;
// vector<pair<ADDRINT,UINT32> > rwAddressLog;
size_t UBBID;

// Local Functions
VOID clearState(THREADID threadid);

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
	"bb-dot", "0", "Log PIN basic blocks in dot format");

KNOB<bool> KnobBBReport(KNOB_MODE_WRITEONCE, "pintool",
	"bb-report", "0",  "Final report on basic blocks");

KNOB<bool> KnobBBSummary(KNOB_MODE_WRITEONCE, "pintool",
	"bb-summary", "0",  "Summary report on basic blocks");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
	if(tracinglevel != 0)
	{
		fprintf(trace,"Progam ended with tracing on\n");

		traceOutput(trace);
	}

	finalOutput(trace, bbl_log);
}

VOID recoredBaseInst(VOID *ip, THREADID threadid)
{
	#ifdef THREADSAFE
	assert(threadid <= numThreads);
	thread_data_t *tdata = get_tls(threadid);
    assert(tdata != nullptr);
    #endif

	if(tracinglevel == 0 || inAlloc)
		return;

	instructionCount++;
		
	long value = 0;
	auto ciItr = instructionLocations.find((ADDRINT)ip);
	assert(ciItr != instructionLocations.end());
	const instructionLocationsData *current_instruction = &(ciItr->second);
	
	for(unsigned int i = 0; i < current_instruction->registers_read.size(); i++)
	{
		value = max(tdata->registers.readReg(current_instruction->registers_read[i]),value);
	}
	
	if(value > 0 && (current_instruction->type != MOVEONLY_INS_TYPE))
	{
		value = value +1;
	}

	for(unsigned int i = 0; i < current_instruction->registers_written.size(); i++)
	{
		tdata->registers.writeReg(current_instruction->registers_written[i], value);
	}
	
	if(value > 0 && (!((current_instruction->type == MOVEONLY_INS_TYPE) && KnobSkipMove)))
	{
		instructionResults[(ADDRINT)ip].addToDepth(value);
#ifdef LOOPSTACK
		current_instruction->loopid = loopStack;
#endif
	}

	if(KnobDebugTrace)
		instructionTracing(ip,NULL,value,"Base",trace,shadowMemory,tdata->registers);
}

const UINT32 NONE_OPERATOR_TYPE = 0;
const UINT32 READ_OPERATOR_TYPE = 1;
const UINT32 WRITE_OPERATOR_TYPE = 2;
const UINT32 BOTH_OPERATOR_TYPE = 3;

VOID RecordMemReadWrite(VOID * ip, VOID * addr1, UINT32 t1, VOID *addr2, UINT32 t2, THREADID threadid, bool pred)
{
	UINT32 type1 = t1;
	UINT32 type2 = t2;

	if(tracinglevel == 0 || inAlloc)
		return;

	#ifdef THREADSAFE
	assert(threadid <= numThreads);
	thread_data_t *tdata = get_tls(threadid);
    assert(tdata != nullptr);
    #endif

	if(KnobBBVerstion)
	{
		tdata->rwContext.addrs.push_back( make_pair((ADDRINT) addr1, t1) );
		tdata->rwContext.addrs.push_back( make_pair((ADDRINT) addr2, t2) );
		tdata->rwContext.pred.push_back(pred);
		if(KnobDebugTrace)
			fprintf(trace,"%p:<%p,%u><%p,%u>\n", (void*) ip, addr1, type1, addr2, type2);
		return;
	}

	if(!pred)
	{
		if(KnobDebugTrace)
			fprintf(trace, "%p:pred not executed\n", (void*) ip);
		return;
	}

	instructionCount++;
	
	long value = 0;
	
	if(type1 & READ_OPERATOR_TYPE)
	{
		value = shadowMemory.readMem((ADDRINT)addr1);
	}

	if(type2 & READ_OPERATOR_TYPE)
	{
		value = max(shadowMemory.readMem((ADDRINT)addr2), value);
	}

	instructionLocationsData *current_instruction = &(instructionLocations[(ADDRINT)ip]);


	for(unsigned int i = 0; i < current_instruction->registers_read.size(); i++)
	{
		value = max(tdata->registers.readReg(current_instruction->registers_read[i]),value);
	}
		
	if(value > 0 && (current_instruction->type != MOVEONLY_INS_TYPE))
	{
		value = value +1;
	}

	for(unsigned int i = 0; i < current_instruction->registers_written.size(); i++)
	{
		tdata->registers.writeReg(current_instruction->registers_written[i], value);
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

	if(value > 0 && !(((current_instruction->type == MOVEONLY_INS_TYPE) && KnobSkipMove)))
	{
		instructionResults[(ADDRINT)ip].addToDepth(value);
#ifdef LOOPSTACK
		current_instruction->loopid = loopStack;
#endif
	}

	if(KnobDebugTrace)
	{
		instructionTracing(ip,addr2,value,"Mem",trace, shadowMemory, tdata->registers);
		fprintf(trace,"<%p,%u><%p,%u>", addr1, type1, addr2, type2);
	}
}

VOID blockTracer(VOID *ip, ADDRINT id, THREADID threadid)
{
	if(!KnobBBVerstion)
		return;

	#ifdef THREADSAFE
	assert(threadid <= numThreads);
	thread_data_t *tdata = get_tls(threadid);
    assert(tdata != nullptr);
    #endif

	if(tracinglevel && tdata->lastBB && !inAlloc)
	{
		basicBlocks[tdata->lastBB].execute(tdata->rwContext, shadowMemory, tdata->registers, trace);
		basicBlocks[tdata->lastBB].addSuccessors((ADDRINT) ip);
		if(KnobDebugTrace)
		{
			fprintf(trace, "BBL %p executed\n", (void *) tdata->lastBB);
			basicBlocks[tdata->lastBB].printBlock(trace);
		}
	}
	tdata->lastBB = id;
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
	    			basicBlocks.push_back(empty);
	    			if(KnobDebugTrace)
	    			{
		    			logBasicBlock(bbl, UBBID, bbl_log);
	    			}
		    		BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)blockTracer, IARG_INST_PTR, IARG_ADDRINT, UBBID, IARG_THREAD_ID, IARG_CALL_ORDER, CALL_ORDER_FIRST-5, IARG_END);
		    		basicBlocks[UBBID].expected_num_ins = BBL_NumIns(bbl);
		    		// insturment each instruction in the curent basic block
			    	for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins) )
			    	{
						ADDRINT ip = INS_Address(ins);

			    		if(INS_IsOriginal(ins))
						{
							instructionType insType;
							insType = decodeInstructionData(ip, instructionLocations);
							debugData[ip].instruction = INS_Disassemble(ins);
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
									INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)recoredBaseInst, IARG_INST_PTR, IARG_CALL_ORDER, CALL_ORDER_FIRST, IARG_THREAD_ID, IARG_END);
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

								INS_InsertCall(
									ins, IPOINT_BEFORE, (AFUNPTR)RecordMemReadWrite,
									IARG_INST_PTR,
									IARG_MEMORYOP_EA, addr1,
									IARG_UINT32, type1,
									IARG_MEMORYOP_EA, addr2,
									IARG_UINT32, type2,
									IARG_THREAD_ID,
									IARG_CALL_ORDER, CALL_ORDER_FIRST,
									IARG_EXECUTING,
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
	// inAlloc = true;

	if(KnobSupressMalloc)
		return;

	#ifdef THREADSAFE
	assert(threadid <= numThreads);
	thread_data_t* tdata = get_tls(threadid);
	#endif
	tdata->malloc_size = size;
}

// Free case
VOID FreeBefore(CHAR * name, ADDRINT start, THREADID threadid)
{
	// inAlloc = true;

	if(KnobSupressMalloc)
		return;

	shadowMemory.arrayMemClear(start);
}

VOID FreeAfter()
{
	// inAlloc = false;
}

// tracing on
VOID traceOn(CHAR * name, ADDRINT start, THREADID threadid)
{
	#ifdef THREADSAFE
	PIN_GetLock(&lock,threadid);
	#endif

	if(traceRegionCount > KnobTraceLimit.Value())
		return;

	if(tracinglevel == 0 && (KnobTraceLimit.Value() != 0))
	{
		traceRegionCount++;
	}
	if(tracinglevel == 0)
		clearState(threadid);

	tracinglevel++;
	if(!KnobForFrontend)
	    fprintf(trace,"tracing turned on\n");
 
    #ifdef THREADSAFE
	PIN_ReleaseLock(&lock);
    #endif
}

void clearVectors()
{
	for(auto it = instructionLocations.begin(); it != instructionLocations.end(); ++it)
	{
		it->second.logged = false;
	}
	instructionResults.clear();
}

// Clear state to just initialized vectors
VOID clearState(THREADID threadid)
{
	clearVectors();
	shadowMemory.clear();
	
	#ifdef THREADSAFE
	thread_data_t *tdata = get_tls(threadid);
	#endif
	tdata->lastBB = 0; //this may be the wrong place
	tdata->rwContext.addrs.clear();
	tdata->rwContext.pred.clear();
}

// tracing off
VOID traceOff(CHAR * name, ADDRINT start, THREADID threadid)
{
	#ifdef THREADSAFE
    PIN_GetLock(&lock, threadid+1);
    #endif

	if(traceRegionCount > KnobTraceLimit.Value())
	{	
	    #ifdef THREADSAFE
		PIN_ReleaseLock(&lock);
	    #endif
		return;
	}

	if(!KnobForFrontend)
	    fprintf(trace,"tracing turned off\n");

	if(tracinglevel < 1)
	{
		if(!KnobForFrontend)
			fprintf(trace,"Trace off called with tracing off\n");
	}
	else if(tracinglevel == 1)
	{
		blockTracer(NULL, 0,0 ); // trace final block
		shadowMemory.clear();

		traceOutput(trace);

		tracinglevel = 0;
	}
	else
	{
		tracinglevel--;
	}
    #ifdef THREADSAFE
	PIN_ReleaseLock(&lock);
    #endif
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

#ifdef LOOPSTACK
VOID loopStart(ADDRINT id)
{
	loopStack.push_front((long long) id);
}

VOID loopEnd(ADDRINT id)
{
	loopStack.pop_front();
}
#endif

VOID MallocAfter(ADDRINT ret, THREADID threadid)
{
	// inAlloc = false;

	if(KnobSupressMalloc)
		return;
	
	#ifdef THREADSAFE
	assert(threadid <= numThreads);
	thread_data_t* tdata = get_tls(threadid);
	#endif
		
	//Map version
	if(tdata->malloc_size > 0)
		shadowMemory.arrayMem(ret, tdata->malloc_size, tracinglevel);

	if(KnobMallocPrinting)
	    fprintf(trace,"malloc returns %p of size(%p)\n",(void *)ret,(void *)tdata->malloc_size);
}

VOID Image(IMG img, VOID *v)
{
    // Instrument the malloc() and free() functions. 

    //  Find the malloc() function.
    RTN mallocRtn = RTN_FindByName(img, MALLOC);
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);

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

        RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR)FreeBefore,
                       IARG_ADDRINT, FREE,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR)FreeAfter,
                       IARG_END);
        RTN_Close(freeRtn);
    }

    // Enable Tracing based on traceon 
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
    // End Tracing based on traceoff
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

    // Mark memmory as vector memory based on arrayMem calls.
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

#ifdef LOOPSTACK
    // Track loop contexts
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
#endif
 
 	// Start tracing based on fucntion name defaults to main
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
	initThread(threadid);
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

	// Initialize VS Threading Support
	initVSThreading();

	// Initialize VectorSeeker globals
	trace = fopen(KnobOutputFile.Value().c_str(), "w");
	if(KnobDebugTrace || KnobBBDotLog)
	{
		bbl_log = fopen("bbl.log", "w");
	}
	UBBID = 0;
	basicBlocks.push_back(empty);
	tracinglevel = 0;
	instructionCount = 0;
	vectorInstructionCountSavings = 0;
	traceRegionCount = 0;
	inAlloc = false;
#ifdef LOOPSTACK
	loopStack.push_front(-1);
#endif
	clearVectors();
	shadowMemory.clear();

    #ifndef THREADSAFE
    tdata = new thread_data_t;
    #endif
    
	// Obtain  a key for TLS storage.
	tls_key = PIN_CreateThreadDataKey(0);
	
    // Register ThreadStart to be called when a thread starts.
    PIN_AddThreadStartFunction(ThreadStart, 0);

    // Register Instruction to be called to instrument instructions
	IMG_AddInstrumentFunction(Image, 0);
	TRACE_AddInstrumentFunction(Trace, 0);


    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
