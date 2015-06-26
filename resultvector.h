#ifndef resultvector_H
#define resultvector_H
#include <vector>
#include <unordered_map>
#include <map>

using std::unordered_map;
using std::map;
using std::vector;
using std::pair;

const size_t CACHESIZE = 32;

class ResultVector
{
private:
	void flush();
	unordered_map<long,long> vectors;	// most used how to speed up?
										// crashes if made a map why?
	long cache[CACHESIZE];
	long execution_count;
	size_t index;

public:
	ResultVector(){execution_count=0; index=0;};
	void addToDepth(long depth);
	long readDepthCount(long depth);
	bool isSingle();
	bool isZero();
	void sortedVectors(vector<pair<long,long> > &sorted_vectors);
	bool vectorsGreater(int minVector);
	void clear();
	long getExecutionCount();
	void printVector(FILE *out);
};

#endif
