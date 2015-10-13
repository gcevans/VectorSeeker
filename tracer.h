#ifndef TRACER_H
#define TRACER_H

#include "pin.H"
#include <unordered_map>
#include "resultvector.h"
#include "instructions.h"

extern KNOB<bool> KnobDebugTrace;
extern KNOB<bool> KnobSkipMove;
extern KNOB<bool> KnobBBVerstion;
extern KNOB<bool> KnobBBSummary;

extern unordered_map<ADDRINT, ResultVector> instructionResults;
extern unordered_map<ADDRINT, instructionDebugData> debugData;

#ifdef THREADSAFE
extern PIN_LOCK lock;
#endif

#endif
