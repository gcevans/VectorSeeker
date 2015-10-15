#ifndef TRACER_H
#define TRACER_H

#include "pin.H"
#include <unordered_map>
#include <vector>
#include "resultvector.h"
#include "instructions.h"
#include "tracebb.h"

extern KNOB<bool> KnobDebugTrace;
extern KNOB<bool> KnobSkipMove;
extern KNOB<bool> KnobBBVerstion;
extern KNOB<bool> KnobBBSummary;
extern KNOB<bool> KnobForVectorSummary;
extern KNOB<bool> KnobForFrontend;
extern KNOB<bool> KnobForShowNoFileInfo;
extern KNOB<int> KnobMinVectorCount;
extern KNOB<bool> KnobSummaryOn;
extern KNOB<bool> KobForPrintBasicBlocks;
extern KNOB<bool> KnobBBDotLog;
extern KNOB<bool> KnobBBReport;
extern KNOB<bool> KnobVectorLineSummary;

extern unordered_map<ADDRINT,instructionLocationsData > instructionLocations;
extern unordered_map<ADDRINT, ResultVector> instructionResults;
extern unordered_map<ADDRINT, instructionDebugData> debugData;
extern unsigned vectorInstructionCountSavings;
extern vector<BBData> basicBlocks;

#endif
