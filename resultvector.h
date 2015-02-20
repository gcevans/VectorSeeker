#ifndef resultvector_H
#define resultvector_H
#include <map>
#include <vector>

using std::map;
using std::vector;
using std::pair;

struct StridedMemoryTrace
{
	long start;
	long end;
	long count;
};

class ResultVector
{
private:
	map<long,long> vectors;
	map<long, StridedMemoryTrace> stridedVectors;

public:
	void addToDepth(long depth);
	long readDepthCount(long depth);
	bool isSingle();
	void sortedVectors(vector<pair<long,long> > &sorted_vectors);
	bool vectorsGreater(int minVector);
	void clear();
};

#endif
