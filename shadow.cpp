#include "shadow.h"
#include <assert.h>

CacheLine::CacheLine()
{
	elementSize = one;
	memory = malloc(sizeof(unsigned char) * cacheLineSize);
}

CacheLine::~CacheLine()
{
	free(memory);
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
		case two:
			{
				unsigned short *ptr = (unsigned short *) memory;
				return (long) ptr[offset];
			}
		case four:
			{
				long *ptr = (long *) memory;
				return ptr[offset];
			}
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
//	return shadowMemory[address];	
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
//	shadowMemory[address] = depth;
	cacheShadowMemory[address/cacheLineSize].write(address%cacheLineSize, depth);
};
//Set Register
void ShadowMemory::writeReg(size_t reg, long depth)
{
	shadowRegisters[reg] = depth;
};
//Reset Region

//Clear
void ShadowMemory::clear()
{
//	shadowMemory.clear();
	cacheShadowMemory.clear();
};

