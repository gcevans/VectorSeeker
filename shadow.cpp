
#include "shadow.h"
#include "tracer.h"

#include <assert.h>

//compute the region that an address is in
int getRegion(ADDRINT addr)
{
	return (addr >> regionsOff)%regionsBits;
}

CacheLine::CacheLine()
{
	elementSize = one;

	memory =  malloc(sizeof(unsigned char) * cacheLineSize);

	unsigned char *ptr = (unsigned char *) memory;

	for(int i = 0; i < cacheLineSize; i++)
	{
		ptr[i] = 0;
	}
}

CacheLine::~CacheLine()
{
	free(memory);
}

void CacheLine::swap(CacheLine &s)
{
	void *tmp_mem = memory;
	memory = s.memory;
	s.memory = tmp_mem;

	elementSizes tmp_ele = elementSize;
	elementSize = s.elementSize;
	s.elementSize = tmp_ele;
}

CacheLine& CacheLine::operator=(const CacheLine& s)
{
	CacheLine tmp(s);
	swap(tmp);
	return *this;
}

CacheLine& CacheLine::operator=(CacheLine&& rhs)
{
	swap(rhs);
	return *this;
}

CacheLine::CacheLine(const CacheLine &s)
{
	elementSize = s.elementSize;

	switch (elementSize)
	{
		case one:
			{
				memory =  malloc(sizeof(unsigned char) * cacheLineSize);
				unsigned char *ptr = (unsigned char *) memory;
				unsigned char *sptr = (unsigned char *) s.memory;
				for(int i = 0; i < cacheLineSize; i++)
				{
					ptr[i] = sptr[i];
				}
			break;
			}
		case two:
			{
				memory =  malloc(sizeof(unsigned short) * cacheLineSize);
				unsigned short *ptr = (unsigned short *) memory;
				unsigned short *sptr = (unsigned short *) s.memory;
				for(int i = 0; i < cacheLineSize; i++)
				{
					ptr[i] = sptr[i];
				}
			break;
			}
		case four:
			{
				memory = malloc(sizeof(long) * cacheLineSize);
				long *ptr = (long *) memory;
				long *sptr = (long *) s.memory;
				for(int i = 0; i < cacheLineSize; i++)
				{
					ptr[i] = sptr[i];
				}
			break;
			}
		default:
			assert(false);
	}
}

//read from cache line
long CacheLine::read(unsigned int offset)
{
	switch (elementSize)
	{
		case one:
			{
				unsigned char *ptr = (unsigned char *) memory;
				return (long) ptr[offset];
			}
			break;
		case two:
			{
				unsigned short *ptr = (unsigned short *) memory;
				return (long) ptr[offset];
			}
			break;
		case four:
			{
				long *ptr = (long *) memory;
				return ptr[offset];
			}
			break;
		default:
			assert(false);
	}
}

//write to cache line
void CacheLine::write(unsigned int offset,long depth)
{
	switch (elementSize)
	{
		case one:
			if( depth > 65535)
			{
				unsigned char *ptr = (unsigned char *) memory;
				long *p = (long *) malloc(sizeof(long) * cacheLineSize);
				for(int i = 0; i < cacheLineSize; i++)
					p[i] = ptr[i];
				free(memory);
				p[offset] = depth;
				memory = (void *) p;
				elementSize = four;
			}
			else if(depth > 255)
			{
				unsigned char *ptr = (unsigned char *) memory;
				unsigned short *p = (unsigned short *) malloc(sizeof(short) * cacheLineSize);
				for(int i = 0; i < cacheLineSize; i++)
					p[i] = ptr[i];
				free(memory);
				p[offset] = depth;
				memory = (void *) p;
				elementSize = two;
			}
			else
			{
				unsigned char *ptr = (unsigned char *) memory;
				ptr[offset] = (unsigned char) depth;
			}
			break;
		case two:
			if( depth > 65535)
			{
				unsigned short *ptr = (unsigned short *) memory;
				long *p = (long *) malloc(sizeof(long) * cacheLineSize);
				for(int i = 0; i < cacheLineSize; i++)
					p[i] = ptr[i];
				free(memory);
				p[offset] = depth;
				memory = (void *) p;
				elementSize = four;
			}
			else
			{
				unsigned short *ptr = (unsigned short *) memory;
				ptr[offset] = (unsigned short) depth;
			}
			break;
		case four:
			{
				long *ptr = (long *) memory;
				ptr[offset] = depth;
				break;
			}
		default:
			assert(false);
	}
}

//Access Memory
long ShadowMemory::readMem(ADDRINT address)
{
	long value = 0;
	int region = getRegion(address);
	#ifdef THREADSAFE
	PIN_RWMutexReadLock(&regionLock[region]);
	#endif
	auto itr = cacheShadowMemory[region].find(address/cacheLineSize);
	if(itr != cacheShadowMemory[region].end())
	{
	    value = itr->second.read(address%cacheLineSize);		
	}
	#ifdef THREADSAFE
    PIN_RWMutexUnlock(&regionLock[region]);
    #endif
	return value;
};

long ShadowMemory::readMem(ADDRINT address, USIZE width)
{
	long value = 0;
	for(USIZE i = 0; i < width; i++)
	{
		int region = getRegion(address+i);
		#ifdef THREADSAFE
		PIN_RWMutexReadLock(&regionLock[region]);
		#endif
		auto itr = cacheShadowMemory[region].find(address/cacheLineSize);
		if(itr != cacheShadowMemory[region].end())
		{
		    value = max(itr->second.read((address+i)%cacheLineSize),value);		
		}
		#ifdef THREADSAFE
	    PIN_RWMutexUnlock(&regionLock[region]);
	    #endif
		}
	return value;
};

//Set Memory
void ShadowMemory::writeMem(ADDRINT address, long depth)
{
	#ifdef THREADSAFE
	int region = getRegion(address);
	// PIN_RWMutexReadLock(&regionLock[region]);
	#endif
	// auto itr = cacheShadowMemory[region].find(address/cacheLineSize);
	// if(itr != cacheShadowMemory[region].end())
	// {
	// 	itr->second.write(address%cacheLineSize, depth);
	// 	#ifdef THREADSAFE
	//     PIN_RWMutexUnlock(&regionLock[region]);
	//     #endif
	//     return;
	// }
	// else
	{
		#ifdef THREADSAFE
	    // PIN_RWMutexUnlock(&regionLock[region]);
		PIN_RWMutexWriteLock(&regionLock[region]);
		#endif
		this->writeMemUnlocked(address, depth);
		#ifdef THREADSAFE
	    PIN_RWMutexUnlock(&regionLock[region]);
	    #endif
	}
};

void ShadowMemory::writeMemUnlocked(ADDRINT address, long depth)
{
	int region = getRegion(address);
	cacheShadowMemory[region][address/cacheLineSize].write(address%cacheLineSize, depth);	
}

//Clear
void ShadowMemory::clear()
{
	#ifdef THREADSAFE
	PIN_RWMutexWriteLock(&allocationLock);
	for(int i = 0; i < regionsNum; i++)
	{
		PIN_RWMutexWriteLock(&regionLock[i]);
	}
	#endif

	for(int region = 0; region < regionsNum; region++)
	{
		cacheShadowMemory[region].clear();
	}

	for(auto it = allocationMap.begin(); it != allocationMap.end(); it++)
	{
		size_t start = it->first;
		for(size_t i = 0; i < allocationMap[start]; i++)
			this->writeMemUnlocked(start+i, 1);
	}

	#ifdef THREADSAFE
	for(int i = 0; i < regionsNum; i++)
	{
	    PIN_RWMutexUnlock(&regionLock[i]);
	}
    PIN_RWMutexUnlock(&allocationLock);
    #endif
};

void ShadowMemory::arrayMem(ADDRINT start, size_t size,bool tracinglevel)
{
	#ifdef THREADSAFE
	PIN_RWMutexWriteLock(&allocationLock);
	for(int i = 0; i < regionsNum; i++)
	{
		PIN_RWMutexWriteLock(&regionLock[i]);
	}
	#endif
	if(size > 0)
		allocationMap[start] = size;
	
	if(tracinglevel)
		for(size_t i = 0; i < size; i++)
			this->writeMemUnlocked(start+i, 1);

	#ifdef THREADSAFE
	for(int i = 0; i < regionsNum; i++)
	{
	    PIN_RWMutexUnlock(&regionLock[i]);
	}
    PIN_RWMutexUnlock(&allocationLock);
    #endif
}

void ShadowMemory::arrayMemClear(ADDRINT start)
{
	#ifdef THREADSAFE
	PIN_RWMutexWriteLock(&allocationLock);
	for(int i = 0; i < regionsNum; i++)
	{
		PIN_RWMutexWriteLock(&regionLock[i]);
	}
	#endif
	for(size_t i = 0; i < allocationMap[start]; i++)
		this->writeMemUnlocked(start+i, 0);
	
	allocationMap.erase(start);
	#ifdef THREADSAFE
	for(int i = 0; i < regionsNum; i++)
	{
	    PIN_RWMutexUnlock(&regionLock[i]);
	}
    PIN_RWMutexUnlock(&allocationLock);
    #endif
}

bool ShadowMemory::memIsArray(VOID *addr)
{
	bool isArray = false;
	#ifdef THREADSAFE
	PIN_RWMutexReadLock(&allocationLock);
	#endif
	auto it = allocationMap.upper_bound((size_t) addr);

	if(it != allocationMap.end() && !isArray)
	{
		--it;
		if(((size_t)(*it).first + (*it).second > (size_t)addr))
		{
			isArray = true;
		}
	}
	#ifdef THREADSAFE
    PIN_RWMutexUnlock(&allocationLock);
    #endif

	return isArray;
}

void ShadowMemory::printAllocationMap(FILE *out)
{
	#ifdef THREADSAFE
	PIN_RWMutexReadLock(&allocationLock);
	#endif
	for(auto a : allocationMap)
	{
		fprintf(out, "[%p,%p]", (void*) a.first, (void *)(a.first + a.second));
	}
	fprintf(out, "\n");
	#ifdef THREADSAFE
    PIN_RWMutexUnlock(&allocationLock);
    #endif
}


ShadowMemory::ShadowMemory()
{
	#ifdef THREADSAFE
	for(auto i = 0; i < regionsNum; i++)
	{
		assert(PIN_RWMutexInit(regionLock));
	}
	assert(PIN_RWMutexInit(&allocationLock));
	#endif
}
ShadowMemory::ShadowMemory(const ShadowMemory &s)
{
	assert(false);
}
ShadowMemory::~ShadowMemory()
{
	assert(false);
}
ShadowMemory& ShadowMemory::operator=(const ShadowMemory& s)
{
	assert(false);
}
ShadowMemory& ShadowMemory::operator=(ShadowMemory&& rhs)
{
	assert(false);
}
void ShadowMemory::swap(ShadowMemory &s)
{
	assert(false);
}

//Access Register
long ShadowRegisters::readReg(size_t reg)
{
	long value = shadowRegisters[reg];
	return value;	
};

//Set Register
void ShadowRegisters::writeReg(size_t reg, long depth)
{
	shadowRegisters[reg] = depth;
};

