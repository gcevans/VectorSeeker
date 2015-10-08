
#include "shadow.h"
#include "tracer.h"

#include <assert.h>

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
	PIN_RWMutexReadLock(&lock);
	auto itr = cacheShadowMemory.find(address/cacheLineSize);
	if(itr != cacheShadowMemory.end())
	{
	    value = itr->second.read(address%cacheLineSize);		
	}
    PIN_RWMutexUnlock(&lock);
	return value;
};

//Access Register
long ShadowMemory::readReg(size_t reg)
{
	PIN_RWMutexReadLock(&lock);
	long value = shadowRegisters[reg];
    PIN_RWMutexUnlock(&lock);
	return value;	
};

//Set Memory
void ShadowMemory::writeMem(ADDRINT address, long depth)
{
	PIN_RWMutexWriteLock(&lock);
	this->writeMemUnlocked(address, depth);
    PIN_RWMutexUnlock(&lock);
};

void ShadowMemory::writeMemUnlocked(ADDRINT address, long depth)
{
	cacheShadowMemory[address/cacheLineSize].write(address%cacheLineSize, depth);	
}

//Set Register
void ShadowMemory::writeReg(size_t reg, long depth)
{
	PIN_RWMutexWriteLock(&lock);
	shadowRegisters[reg] = depth;
    PIN_RWMutexUnlock(&lock);
};

//Clear
void ShadowMemory::clear()
{
	PIN_RWMutexWriteLock(&lock);
	for(int i = 0; i < XED_REG_LAST; i++)
	{
		shadowRegisters[i] = 0;
	}

	cacheShadowMemory.clear();

	for(auto it = allocationMap.begin(); it != allocationMap.end(); it++)
	{
		size_t start = it->first;
		for(size_t i = 0; i < allocationMap[start]; i++)
			this->writeMemUnlocked(start+i, 1);
	}

    PIN_RWMutexUnlock(&lock);
};

void ShadowMemory::arrayMem(ADDRINT start, size_t size,bool tracinglevel)
{
	PIN_RWMutexWriteLock(&lock);
	if(size > 0)
		allocationMap[start] = size;
	
	if(tracinglevel)
		for(size_t i = 0; i < size; i++)
			this->writeMemUnlocked(start+i, 1);

    PIN_RWMutexUnlock(&lock);
}

void ShadowMemory::arrayMemClear(ADDRINT start)
{
	PIN_RWMutexWriteLock(&lock);
	for(size_t i = 0; i < allocationMap[start]; i++)
		this->writeMemUnlocked(start+i, 0);
	
	allocationMap.erase(start);
    PIN_RWMutexUnlock(&lock);
}

bool ShadowMemory::memIsArray(VOID *addr)
{
	bool isArray = false;
	PIN_RWMutexReadLock(&lock);
	auto it = allocationMap.upper_bound((size_t) addr);

	if(it != allocationMap.end() && !isArray)
	{
		--it;
		if(((size_t)(*it).first + (*it).second > (size_t)addr))
		{
			isArray = true;
		}
	}
    PIN_RWMutexUnlock(&lock);

	return isArray;

	// for(auto it = allocationMap.begin(); it != allocationMap.end(); it++)
	// {
	// 	if( ( (size_t) addr >= (size_t)(*it).first) && ((size_t)(*it).first + (*it).second > (size_t)addr) )
	// 	{
	// 		return true;
	// 	}

	// }
	// return false;
}

void ShadowMemory::printAllocationMap(FILE *out)
{
	PIN_RWMutexReadLock(&lock);
	for(auto a : allocationMap)
	{
		fprintf(out, "[%p,%p]", (void*) a.first, (void *)(a.first + a.second));
	}
	fprintf(out, "\n");
    PIN_RWMutexUnlock(&lock);
}


ShadowMemory::ShadowMemory()
{
	assert(PIN_RWMutexInit(&lock));
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


