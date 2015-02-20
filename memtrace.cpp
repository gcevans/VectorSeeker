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
/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include "pin.H"
#include <map>
#include <iostream>
#include <fstream>
#include <set>
#include <vector>

struct Allocation
{
	unsigned long	id;
	ADDRINT	start;
	ADDRINT size;
};

// Holds info for a single procedure
typedef struct RtnCount
{
    string _name;
    string _image;
    ADDRINT _address;
    RTN _rtn;
    struct RtnCount * _next;
} RTN_COUNT;

// Linked list of instruction counts for each routine
RTN_COUNT * RtnList = 0;

const char * StripPath(const char * path)
{
    const char * file = strrchr(path,'/');
    if (file)
        return file+1;
    else
        return path;
}

struct classcomp {
  bool operator() (const Allocation& lhs, const Allocation& rhs) const
  {return lhs.start < rhs.start;}
};


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

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

std::ofstream TraceFile;
map<VOID *,long> addrHashPtr;
//map<VOID *,long> addrHashPtrRead;
//map<VOID *,long> addrHashPtrWrite;
//map<VOID *,set<RTN_COUNT *> > addrHashRoutineInfo;
//set<string> routineNames;
set<Allocation,classcomp> allocationSet;
vector<long> allocationAccesses;
vector<vector<long> > allocationRead;
vector<vector<long> > allocationWrite;
vector<vector<vector<long> > > allocationReadWindow;
vector<vector<vector<long> > > allocationWriteWindow;
unsigned long windowCount = 0;
vector<unsigned long long> allocationFirst;
vector<unsigned long long> allocationLast;
vector<ADDRINT> allocationStarts;
vector<ADDRINT> allocationSizes;
unsigned long long totalAccesses = 0;
ADDRINT mallocSize;
unsigned long	freeId = 0;
unsigned long	vectorWidth = 16; // width in bytes


/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "memtrace.out", "specify trace file name");


// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr, VOID *routine)
{
	totalAccesses += 1;
	set<Allocation,classcomp>::iterator it;
	for(it = allocationSet.begin(); it != allocationSet.end(); it++)
	{
		if( ((unsigned long)addr >= it->start) && ( (unsigned long)addr <= it->start + it->size - 1) )
		{
			allocationAccesses[it->id] += 1;
			if(allocationFirst[it->id] == 0)
				allocationFirst[it->id] = totalAccesses;
			allocationLast[it->id] = totalAccesses;
			allocationRead[it->id][(unsigned long)addr - it->start] += 1;
			return;
		}
	
	}
//	addrHashRoutineInfo[addr].insert((RTN_COUNT *)routine);
	addrHashPtr[addr] += 1;
//	addrHashPtrRead[addr] += 1;
//	addrHashPtrWrite[addr] += 0;

}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr, VOID *routine)
{
	totalAccesses += 1;
	set<Allocation,classcomp>::iterator it;
	for(it = allocationSet.begin(); it != allocationSet.end(); it++)
	{
		if( ((unsigned long)addr >= it->start) && ( (unsigned long)addr <= it->start + it->size - 1) )
		{
			allocationAccesses[it->id] += 1;
			if(allocationFirst[it->id] == 0)
				allocationFirst[it->id] = totalAccesses;
			allocationLast[it->id] = totalAccesses;
			allocationWrite[it->id][(unsigned long)addr - it->start] += 1;
			return;
		}
	
	}
//	addrHashRoutineInfo[addr].insert((RTN_COUNT *)routine);
	addrHashPtr[addr] += 1;
//	addrHashPtrWrite[addr] += 1;
//	addrHashPtrRead[addr] += 0;
}


/* ===================================================================== */
/* Analysis routines                                                     */
/* ===================================================================== */

// Used for both malloc and free
VOID Arg1Before(CHAR * name, ADDRINT arg)
{
	if(strcmp(name, MALLOC) == 0)
	{
	    mallocSize = arg;
	}
	else
	{
		Allocation tmp;
		tmp.start = arg;

		set<Allocation,classcomp>::iterator it;
		it = allocationSet.find(tmp);
		if(it != allocationSet.end())
		{
			allocationSet.erase(it);
		}
		else
		{
			//not a vallid pointer
		}
	}
}

VOID MallocAfter(ADDRINT ret)
{
	vector<long> newAllocation;
	Allocation tmp;
	
	tmp.id = freeId++;
	tmp.start = ret;
	tmp.size = mallocSize;
	allocationSet.insert(tmp);

	allocationFirst.resize(	allocationFirst.size() + 1);
	allocationLast.resize(	allocationFirst.size() + 1);

	allocationRead.push_back(newAllocation);
	allocationRead[tmp.id].resize(mallocSize);

	allocationWrite.push_back(newAllocation);
	allocationWrite[tmp.id].resize(mallocSize);

	allocationAccesses.resize(allocationAccesses.size() + 1);

	allocationSizes.resize(allocationSizes.size() + 1);
	allocationSizes[tmp.id] = mallocSize;

	allocationStarts.resize(allocationStarts.size() + 1);
	allocationStarts[tmp.id] = ret;
}


// Pin calls this function every time a new rtn is executed
VOID Routine(RTN rtn, VOID *v)
{
    
    // Allocate a counter for this routine
    RTN_COUNT * rc = new RTN_COUNT;

    // The RTN goes away when the image is unloaded, so save it now
    // because we need it in the fini
    rc->_name = RTN_Name(rtn);
    rc->_image = StripPath(IMG_Name(SEC_Img(RTN_Sec(rtn))).c_str());
    rc->_address = RTN_Address(rtn);

    // Add to list of routines
    rc->_next = RtnList;
    RtnList = rc;

    RTN_Open(rtn);

    // For each instruction of the routine
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
    {

		// Instruments memory accesses using a predicated call, i.e.
		// the instrumentation is called iff the instruction will actually be executed.
		//
		// The IA-64 architecture has explicitly predicated instructions. 
		// On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
		// prefixed instructions appear as predicated instructions in Pin.
		UINT32 memOperands = INS_MemoryOperandCount(ins);
	
		// Iterate over each memory operand of the instruction.
		for (UINT32 memOp = 0; memOp < memOperands; memOp++)
		{
			if (INS_MemoryOperandIsRead(ins, memOp))
			{
				INS_InsertPredicatedCall(
					ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
					IARG_INST_PTR,
					IARG_MEMORYOP_EA, memOp,
					IARG_PTR, rc,
					IARG_END);
			}
			// Note that in some architectures a single memory operand can be 
			// both read and written (for instance incl (%eax) on IA-32)
			// In that case we instrument it once for read and once for write.
			if (INS_MemoryOperandIsWritten(ins, memOp))
			{
				INS_InsertPredicatedCall(
					ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
					IARG_INST_PTR,
					IARG_MEMORYOP_EA, memOp,
					IARG_PTR, rc,
					IARG_END);
			}
		}
    }

    RTN_Close(rtn);
   
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

        // Instrument malloc() for the input argument value and the return value.
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)Arg1Before,
                       IARG_ADDRINT, MALLOC,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR)MallocAfter,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(mallocRtn);
    }

    // Find the free() function.
    RTN freeRtn = RTN_FindByName(img, FREE);
    if (RTN_Valid(freeRtn))
    {
        RTN_Open(freeRtn);
        // Instrument free()
        RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR)Arg1Before,
                       IARG_ADDRINT, FREE,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_Close(freeRtn);
    }
}




VOID Fini(INT32 code, VOID *v)
{
	unsigned long long bestVectorTime = 0;
	unsigned long long totalVectorAcceses = 0;
	unsigned long long bestVectorAcceses = 0;
	unsigned long long totalMemory = 0;
	unsigned long long totalNonArrayAccesses = 0;
//	unsigned long long MinTime;
	unsigned long long stride4ArrayTime = 0;
	unsigned long long stride8ArrayTime = 0;
	TraceFile << "### Start Histogram ###" << endl;
	TraceFile << "### Start Allocation Access Histogram ###" << endl;
	for(unsigned int i = 0; i < allocationAccesses.size(); i++)
	{
		totalMemory += allocationSizes[i];
		long max = 0;
		long max4 = 0;
		long max8 = 0;
		long min = allocationRead[i][0] + allocationWrite[i][0] + 1;
//		TraceFile << allocationStarts[i] << "[" << allocationSizes[i] << "]<" << allocationFirst[i] << "-" << allocationLast[i] << ">:" <<  allocationAccesses[i];
		TraceFile << i << "[" << allocationSizes[i] << "]<" << allocationFirst[i] << "-" << allocationLast[i] << ">:" <<  allocationAccesses[i];
		for(unsigned int j = 0; j < allocationRead[i].size(); j++)
		{
			long tmpAccesses = allocationRead[i][j] + allocationWrite[i][j];
			totalVectorAcceses += tmpAccesses;
			if( tmpAccesses > max )
				max = tmpAccesses;
			if( tmpAccesses < min  && tmpAccesses  > 0)
				min = tmpAccesses;
			if(tmpAccesses > max4)
				max4 = tmpAccesses;
			if(tmpAccesses > max8)
				max8 = tmpAccesses;
			if( (j+1)%16 == 0)
			{
				stride4ArrayTime += max4;
				max4 = 0;
			}
			if( (j+1)%32 == 0)
			{
				stride8ArrayTime += max8;
				max8 = 0;
			}
		}
		stride4ArrayTime += max4;
		stride8ArrayTime += max8;
		bestVectorTime += max;
		bestVectorAcceses += max;
//		TraceFile << " max unit accesses " << max << " min positive unit accesses " << min <<  endl;
		TraceFile << " max unit accesses " << max <<  endl;
	}
	TraceFile << "### Start Non Allocation Access Histogram ###" << endl;
	map<VOID *,long>::iterator it;
	map<VOID *,long>::iterator itRead;
	map<VOID *,long>::iterator itWrite;
	long max = 0;
//	for ( it=addrHashPtr.begin(), itRead=addrHashPtrRead.begin(), itWrite=addrHashPtrWrite.begin(); it != addrHashPtr.end(); it++, itRead++, itWrite++ )
	for ( it=addrHashPtr.begin(); it != addrHashPtr.end(); it++)
	{
		set<RTN_COUNT *>::iterator rtnit;
// 		TraceFile << (*it).first << ":" << (long) (*it).second << " r " << (long) (*itRead).second << " w " << (long) (*itWrite).second << " <";
// 		for(rtnit = addrHashRoutineInfo[(*it).first].begin(); rtnit != addrHashRoutineInfo[(*it).first].end(); rtnit++)
// 		{
// 			TraceFile << (*rtnit)->_name << ",";
// 		}
// 		TraceFile << ">" << endl;
		if( (*it).second > max)
			max = (*it).second;
		bestVectorTime += (*it).second;
		totalNonArrayAccesses += (*it).second;
		totalMemory += 1;
	}
	TraceFile << "### End Non Allocation Access Histogram ###" << endl;
	TraceFile << "Total Memory Accesses " << totalAccesses << endl;
	TraceFile << "Total Array Accesses " << totalVectorAcceses << endl;
	TraceFile << "Total Array Accesses/Total Memory Accesses " << (double)totalVectorAcceses/(double)totalAccesses << endl;
// 	TraceFile << "Total Non Array Accesses " << totalNonArrayAccesses << endl;
// 	TraceFile << "Total Ammount of Memory Accessed " << totalMemory << endl;
// 	TraceFile << "Sum of Most Accessed Locations Accesses in all Arrays " << bestVectorAcceses << endl;
// 	TraceFile << "Most Accessed Locations Accesses of Non Arrays Memory " << max << endl;
	TraceFile << "Best Array Access Speedup " << (double) totalVectorAcceses / (double) bestVectorAcceses << endl;
	TraceFile << "4 wide Array Access Speedup " << (double) totalVectorAcceses / (double) stride4ArrayTime << endl;
	TraceFile << "8 wide Array Access Speedup " << (double) totalVectorAcceses / (double) stride8ArrayTime << endl;
// 	TraceFile << "Best Non Array Memory Access Speedup " << (double) totalNonArrayAccesses / (double) (max + bestVectorAcceses) << endl;
// 	TraceFile << "Best Speedup (Arrays Vectorized + Memory Sequential) = " << (double)totalAccesses / (double) bestVectorTime << endl;
// 	TraceFile << "Best Speedup (Arrays Vectorized + Memory Vectorized) = " << (double)totalAccesses / (double) (max + bestVectorAcceses)  << endl;
// 	MinTime = (unsigned long long) (1 + (double)totalAccesses / (double) totalMemory);
// 	TraceFile << "Maximum Speedup (Cel(Total Accesses / Total Memory Accessed)) = " << (double)totalAccesses / (double) MinTime << endl;
	TraceFile << "#eof" << endl;
    TraceFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize pin & symbol manager
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) return Usage();
    
    
	// Write to a file since cout and cerr maybe closed by the application
    TraceFile.open(KnobOutputFile.Value().c_str());
//    TraceFile << hex;
//    TraceFile.setf(ios::showbase);

    // Register Routine to be called to instrument rtn
    RTN_AddInstrumentFunction(Routine, 0);

	IMG_AddInstrumentFunction(Image, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
