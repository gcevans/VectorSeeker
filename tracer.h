#ifndef TRACER_H
#define TRACER_H

#include "pin.H"
#include <unordered_map>
#include "resultvector.h"

extern KNOB<bool> KnobDebugTrace;
extern KNOB<bool> KnobSkipMove;
extern KNOB<bool> KnobBBVerstion;

extern unordered_map<ADDRINT, ResultVector > instructionResults;

#endif
