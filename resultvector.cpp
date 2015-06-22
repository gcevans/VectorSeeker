#include "resultvector.h"
#include <algorithm>

void ResultVector::addToDepth(long depth)
{
	vectors[depth] += 1;
	execution_count += 1;
}

long ResultVector::getExecutionCount()
{
	return execution_count;
}

long ResultVector::readDepthCount(long depth)
{
	return vectors[depth];
}


bool ResultVector::isSingle()
{
	if( vectors.size() == 1)
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
	for(auto rawv : vectors)
	{
		sorted_vectors.push_back(std::make_pair(rawv.first,rawv.second));
	}
	std::sort(sorted_vectors.begin(), sorted_vectors.end(), sortByFirst);
}


bool ResultVector::vectorsGreater(int minVector)
{
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
}



