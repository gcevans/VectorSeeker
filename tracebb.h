#ifndef TRACEBB_H
#define TRACEBB_H

#include "shadow.h"
#include "instructions.h"
#include "resultvector.h"

#include <vector>
#include <set>
#include <unordered_map>
#include <atomic>

extern atomic<unsigned> instructionCount;
extern unordered_map<ADDRINT,instructionLocationsData > instructionLocations;
extern list<long long> loopStack;

struct ExecutionContex
{
	vector<pair<ADDRINT,UINT32> > addrs;
	vector<bool> pred;
};


#include "bbdata.h"



#endif