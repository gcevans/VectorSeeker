#include "tracer.h"
#include "output.h"
#include "vsdisplay.h"

// Globals from tracer
vector<BBData> basicBlocks;


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

int main()
{

	// Read in decode file

	// Read in trace files

	// Build structures

	// Display output

	return 0;
}