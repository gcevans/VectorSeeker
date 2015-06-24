
#include "shadow.h"

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
	return cacheShadowMemory[address/cacheLineSize].read(address%cacheLineSize);
};

//Access Register
long ShadowMemory::readReg(size_t reg)
{
	return shadowRegisters[reg];	
};

//Set Memory
void ShadowMemory::writeMem(ADDRINT address, long depth)
{
	cacheShadowMemory[address/cacheLineSize].write(address%cacheLineSize, depth);
};

//Set Register
void ShadowMemory::writeReg(size_t reg, long depth)
{
	shadowRegisters[reg] = depth;
};

//Clear
void ShadowMemory::clear()
{
	for(int i = 0; i < XED_REG_LAST; i++)
	{
		shadowRegisters[i] = 0;
	}

	cacheShadowMemory.clear();

	for(auto it = allocationMap.begin(); it != allocationMap.end(); it++)
	{
		size_t start = it->first;
		for(size_t i = 0; i < allocationMap[start]; i++)
			this->writeMem(start+i, 1);
	}

};

void ShadowMemory::arrayMem(ADDRINT start, size_t size,bool tracinglevel)
{
	if(size > 0)
		allocationMap[start] = size;
	
	if(tracinglevel)
		for(size_t i = 0; i < size; i++)
			this->writeMem(start+i, 1);
}

void ShadowMemory::arrayMemClear(ADDRINT start)
{
	for(size_t i = 0; i < allocationMap[start]; i++)
		this->writeMem(start+i, 0);
	
	allocationMap.erase(start);
}

bool ShadowMemory::memIsArray(VOID *addr)
{
	for(auto it = allocationMap.begin(); it != allocationMap.end(); it++)
	{
		if( ( (size_t) addr >= (size_t)(*it).first) && ((size_t)(*it).first + (*it).second > (size_t)addr) )
		{
			return true;
		}

	}
	return false;
}

void ShadowMemory::printAllocationMap(FILE *out)
{
	for(auto a : allocationMap)
	{
		fprintf(out, "[%p,%p]", (void*) a.first, (void *)(a.first + a.second));
	}
	fprintf(out, "\n");
}



