#ifndef SHADOW_H
#define SHADOW_H

#include "pin.H"
#include "instlib.H"
#include <unordered_map>

const int cacheLineSize = 128;

class CacheLine
{
private:
	enum elementSizes {one, two, four};
	elementSizes elementSize;
	void *memory;
public:
	//read
	long read(unsigned int offset);
	//write
	void write(unsigned int offset, long value);
	//
	CacheLine();
	CacheLine(const CacheLine &s);
	~CacheLine();
	CacheLine& operator=(const CacheLine& s);
};

class ShadowMemory
{
private:
	long shadowRegisters[XED_REG_LAST]; // Register Memory
	unordered_map<ADDRINT,CacheLine> cacheShadowMemory;
	unordered_map<ADDRINT,size_t> allocationMap;

public:
	//Access Memory
	long readMem(ADDRINT address);
	//Access Register
	long readReg(size_t reg);
	//Set Memory
	void writeMem(ADDRINT address, long depth);
	//Set Register
	void writeReg(size_t reg, long depth);	
	//Clear
	void clear();
	//Add Memory Allocation
	void arrayMem(ADDRINT start, size_t size, bool tracinglevel);
	//Free Memroy Allocation
	void arrayMemClear(ADDRINT start);
	//Check if addr is allocated
	bool memIsArray(VOID *addr);
};

#endif
