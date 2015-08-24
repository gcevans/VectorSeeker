#ifndef SHADOW_H
#define SHADOW_H

#include "pin.H"
#include "instlib.H"
#include <unordered_map>
#include <map>

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
	CacheLine& operator=(CacheLine&& rhs);
	void swap(CacheLine &s);
};

class ShadowMemory
{
private:
	long shadowRegisters[XED_REG_LAST]; // Register Memory
	unordered_map<ADDRINT,CacheLine> cacheShadowMemory;
	map<ADDRINT,size_t> allocationMap;

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
	//Print allocation map
	void printAllocationMap(FILE *out);
};

class ShadowMemoryNoCache
{
	long shadowRegisters[XED_REG_LAST]; // Register Memory
	map<ADDRINT,long> shadowMemory;
	map<ADDRINT,size_t> allocationMap;

public:
	//Access Memory
	long readMem(ADDRINT address)
	{
		return shadowMemory[address];
	};
	//Access Register
	long readReg(size_t reg)
	{
		return shadowRegisters[reg];
	};
	//Set Memory
	void writeMem(ADDRINT address, long depth)
	{
		shadowMemory[address] = depth;
	}
	//Set Register
	void writeReg(size_t reg, long depth)
	{
		shadowRegisters[reg] = depth;
	};
	//Clear
	void clear()
	{
		for(int i = 0; i < XED_REG_LAST; i++)
		{
			shadowRegisters[i] = 0;
		}

		shadowMemory.clear();

		for(auto it = allocationMap.begin(); it != allocationMap.end(); it++)
		{
			size_t start = it->first;
			for(size_t i = 0; i < allocationMap[start]; i++)
				this->writeMem(start+i, 1);
		}		
	};
	//Add Memory Allocation
	void arrayMem(ADDRINT start, size_t size, bool tracinglevel)
	{
		if(size > 0)
			allocationMap[start] = size;
		
		if(tracinglevel)
			for(size_t i = 0; i < size; i++)
				this->writeMem(start+i, 1);
	};
	//Free Memroy Allocation
	void arrayMemClear(ADDRINT start)
	{
		for(size_t i = 0; i < allocationMap[start]; i++)
			this->writeMem(start+i, 0);
	
		allocationMap.erase(start);	
	};
	//Check if addr is allocated
	bool memIsArray(VOID *addr)
	{
		auto it = allocationMap.upper_bound((size_t) addr);

		if(it != allocationMap.end())
		{
			--it;
			if(((size_t)(*it).first + (*it).second > (size_t)addr))
				return true;
	}

	return false;
	};
	//Print allocation map
	void printAllocationMap(FILE *out)
	{
		for(auto a : allocationMap)
		{
			fprintf(out, "[%p,%p]", (void*) a.first, (void *)(a.first + a.second));
		}
		fprintf(out, "\n");
	};

};

#endif
