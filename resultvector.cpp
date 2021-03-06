#include "resultvector.h"
#include <algorithm>

void ResultVector::flush()
{
	for(auto i = size_t{0}; i < index; ++i)
	{
		vectors[cache[i]] += 1;
	}
	index = 0;
}

void ResultVector::addToDepth(long depth)
{
	++execution_count;
	if(index < CACHESIZE)
	{
		cache[index++] = depth;
	}
	else
	{
		vectors[depth] += 1;
		flush();
	}
}

long ResultVector::getExecutionCount()
{
	return execution_count;
}

long ResultVector::readDepthCount(long depth)
{
	flush();
	return vectors[depth];
}


const bool ResultVector::isSingle()
{
	flush();
	if( vectors.size() == 1)
		return true;
	
	return false;
}

const bool ResultVector::isZero()
{
	flush();
	if( vectors.size() == 0)
		return true;
	
	return false;
}

inline bool sortBySecond(pair<long,long> i, pair<long,long> j)
{
	return(i.second > j.second);
}

inline bool sortByFirst(pair<long,long> i, pair<long,long> j)
{
	return(i.first < j.first);
}

void ResultVector::sortedVectors(vector<pair<long,long> > &sorted_vectors)
{
	flush();
	for(auto rawv : vectors)
	{
		sorted_vectors.push_back(std::make_pair(rawv.first,rawv.second));
	}
	std::sort(sorted_vectors.begin(), sorted_vectors.end(), sortByFirst);
}


bool ResultVector::vectorsGreater(int minVector)
{
	flush();
	for(auto timeit = vectors.begin(); timeit != vectors.end(); ++timeit)
	{
		if(timeit->second > minVector)
		{
			return true;
		}
	}
	return false;
}

void ResultVector::clear()
{
	vectors.clear();
	execution_count = 0;
	index = 0;
}

void ResultVector::printVector(FILE *out)
{
	for(auto vec : vectors)
	{
		fprintf(out, "<%ld,%ld>", vec.first, vec.second);
	}
}

