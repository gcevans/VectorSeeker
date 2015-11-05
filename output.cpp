#include "output.h"
#include <algorithm>
#include "tracer_decode.h"


bool sortBySecond(pair<long,long> i, pair<long,long> j)
{
	return(i.second > j.second);
}


struct instructionLocationsDataPointerSort
{
	unordered_map<ADDRINT, ResultVector > *instructionResults;
	bool operator()(instructionLocationsData *a, instructionLocationsData *b)
	{
		if((*instructionResults)[a->ip].getExecutionCount() > (*instructionResults)[b->ip].getExecutionCount())
			return true;
		else if((*instructionResults)[a->ip].getExecutionCount() < (*instructionResults)[b->ip].getExecutionCount())
			return false;
		else
		{
			int files_differ = debugData[a->ip].file_name.compare(debugData[b->ip].file_name);
			if(files_differ == 0)
				return(debugData[a->ip].line_number < debugData[b->ip].line_number);

			return(files_differ < 0);
		}
	}

};

bool instructionLocationsDataPointerAddrSort(instructionLocationsData *a, instructionLocationsData *b)
{
	return(a->ip < b->ip);
}


void buildOutputMaps(vector<instructionLocationsData *> &pmap, map< string,map<int,vector<instructionLocationsData *>>> &lmap, unordered_map<ADDRINT, ResultVector > &instructionResults)
{
	instructionLocationsDataPointerSort sorter;
	sorter.instructionResults = &instructionResults;
	for(auto ipit = instructionLocations.begin(); ipit != instructionLocations.end(); ++ipit)
	{
		pmap.push_back(&(ipit->second));
		lmap[debugData[ipit->second.ip].file_name][debugData[ipit->second.ip].line_number].push_back(&(ipit->second));
	}
	
	sort(pmap.begin(),pmap.end(),sorter);
}

VOID writeLog(FILE * trace, unordered_map<ADDRINT, ResultVector > &instructionResults)
{
	bool linesFound = false;
	bool filesFound = false;	
	vector<instructionLocationsData *> profile_list; // sorted instructions
	map< string,map<int,vector<instructionLocationsData *>>>line_map; // map of lines to instuctions
	vector<instructionLocationsData *> *current_line; // instructions in the current line

	if(!KnobForFrontend)
	{
		#ifdef NOSHAODWCACHE
		fprintf(trace, "#start instruction log NO SHADOWCACHE\b");
		#else
		fprintf(trace, "#start instruction log\n");
		#endif
	}

	buildOutputMaps(profile_list,line_map,instructionResults);

	for(auto ins : profile_list)
	{
		linesFound = true;

		string file_name = debugData[ins->ip].file_name;
		int line_number = debugData[ins->ip].line_number;

		if(file_name == "")
			file_name = "NO FILE INFORMATION";
		else
		{
			filesFound = true;
		}

		if(!(ins->logged) && (file_name != "NO FILE INFORMATION" || KnobForShowNoFileInfo ) && (instructionResults[ins->ip].getExecutionCount() > 0))
		{
#ifdef LOOPSTACK
			std::ostringstream loopstackstring;
			for(auto it = ins->loopid.rbegin(); it != ins->loopid.rend();)
			{
				loopstackstring << *it;
				it++;
				if(it != ins->loopid.rend())
					loopstackstring << ',';
			}
				
			fprintf(trace,"%s,%d<%s>:%ld\n", file_name.c_str(),line_number,loopstackstring.str().c_str(),instructionResults[ins->ip].getExecutionCount());
#endif
			fprintf(trace,"%s,%d:%ld\n", file_name.c_str(),line_number,instructionResults[ins->ip].getExecutionCount());

			current_line = &(line_map[debugData[ins->ip].file_name][line_number]);
			sort(current_line->begin(),current_line->end(),instructionLocationsDataPointerAddrSort);
			for(unsigned int j = 0; j < current_line->size(); j++)
			{
				if(instructionResults[(*current_line)[j]->ip].getExecutionCount() > 0)
				{
					(*current_line)[j]->logged = true;
					VOID *ip = (VOID *)(*current_line)[j]->ip;
					int vector_count = -1;
					int current_vector_size = -1;
					bool once = false;
					map<long,long>::iterator timeit;
	
					fprintf(trace,"\t%p:%s:%s\n\t%s\n\t\t",ip,getInstCategoryString(ip),debugData[(ADDRINT) ip].instruction.c_str(),instructionLocations[(ADDRINT)ip].rtn_name.c_str());
	
					if(!KnobForVectorSummary)
					{
						vector<pair<long,long> >sorted_vectors;
						instructionResults[(ADDRINT) ip].sortedVectors(sorted_vectors);
						for(size_t i = 0; i < sorted_vectors.size(); i++)
						{
							fprintf(trace,"<%ld,%ld>",sorted_vectors[i].first,sorted_vectors[i].second);
						}
					}
					else
					{
						vector<pair<long,long> >sorted_vectors;
						instructionResults[(ADDRINT) ip].sortedVectors(sorted_vectors);
						for(size_t i = 0; i < sorted_vectors.size(); i++)
						{
							if(!once)
							{
								once = true;
								current_vector_size = sorted_vectors[i].second;
								vector_count = 0;
							}
							if(current_vector_size != sorted_vectors[i].second)
							{
								if(vector_count != 1)
									fprintf(trace,"x%d",vector_count);
	
								vector_count = 0;
								current_vector_size = sorted_vectors[i].second;
							}
							if(vector_count == 0)
							{
								fprintf(trace,"<%ld,%ld>",sorted_vectors[i].first,sorted_vectors[i].second);
							}
							vector_count++;
							vectorInstructionCountSavings = vectorInstructionCountSavings + (sorted_vectors[i].second - 1);
						}
						if(vector_count != 1)
							fprintf(trace,"x%d",vector_count);
					}
					fprintf(trace,"\n");
				}
			}
		}
	}
	if(!KnobForFrontend)
	{	
		if( linesFound && !KnobForShowNoFileInfo && !filesFound)
			fprintf(trace, "Insturctions were found but they had no debug info on source file or were only mov instructions\n");
		fprintf(trace, "#end instruction log\n");
	}
}

VOID writeOnOffLog(FILE * trace, unordered_map<ADDRINT, ResultVector > &instructionResults)
{
	vector<instructionLocationsData *> profile_list;
	map< string,map<int,vector<instructionLocationsData *> > >line_map;
	vector<instructionLocationsData *> *current_line;

	buildOutputMaps(profile_list,line_map,instructionResults);

	for(auto ins : profile_list)
	{	
		string file_name = debugData[ins->ip].file_name;
		int line_number = debugData[ins->ip].line_number;

		if(!(ins->logged) && (file_name != "") && (instructionResults[ins->ip].getExecutionCount() > 0))
		{
			bool isVectorizable = false;
			bool isNotVectorizable = false;
			current_line = &(line_map[debugData[ins->ip].file_name][line_number]);
			sort(current_line->begin(),current_line->end(),instructionLocationsDataPointerAddrSort);
			for(unsigned int j = 0; j < current_line->size(); j++)
			{
				(*current_line)[j]->logged = true;
				if(instructionResults[(*current_line)[j]->ip].getExecutionCount() > 0)
				{
					bool noVectorGreaterThenOne = true;
					if(instructionResults[(*current_line)[j]->ip].vectorsGreater(KnobMinVectorCount.Value()))
					{
						isVectorizable = true;
						noVectorGreaterThenOne = false;
					}
					if(noVectorGreaterThenOne)
						isNotVectorizable = true;
				}
			}

			if(isVectorizable && !isNotVectorizable)
				fprintf(trace,"V:%s,%d:%ld\n", file_name.c_str(),line_number,instructionResults[ins->ip].getExecutionCount() );
			else if(isVectorizable && isNotVectorizable)
				fprintf(trace,"P:%s,%d:%ld\n", file_name.c_str(),line_number,instructionResults[ins->ip].getExecutionCount() );
			else
				fprintf(trace,"N:%s,%d:%ld\n", file_name.c_str(),line_number,instructionResults[ins->ip].getExecutionCount() );
		}
	}
	if(!KnobForFrontend)
		fprintf(trace, "#end instruction log\n");
}

void WriteBBDotLog(FILE *out)
{
	for(auto bb : basicBlocks)
	{
		bb.printBlock(out);
	}
}

void writeBBReport(FILE *trace, unordered_map<ADDRINT, ResultVector > &instructionResults)
{

	for(auto bb : basicBlocks)
	{
		vector<ADDRINT> addrs = bb.getAddrs();
		bool allSingle = true;
		bool allZero = true;
		bool someGreater = false;
		for(auto addr : addrs)
		{
			if(!(instructionResults[addr].isSingle() || instructionResults[addr].isZero()))
				allSingle = false;
			if(!instructionResults[addr].isZero())
				allZero = false;
			if(instructionResults[addr].vectorsGreater(KnobMinVectorCount.Value()))
				someGreater = true;
		}
		if(allSingle && !allZero && someGreater)
		{
//			bb.printBlock(trace);
			fprintf(trace, "Basic block staring %p end %p\n", (void *)addrs.front(), (void *)addrs.back() );
			map<string,vector<int>> sources;
			for(auto addr : addrs)
			{
				sources[debugData[addr].file_name].push_back(debugData[addr].line_number);
			}
			for(auto line : sources)
			{
				sort(line.second.begin(), line.second.end());

				fprintf(trace, "%s,%d", line.first.c_str(), line.second.front() );
				if(line.second.front() != line.second.back())
					fprintf(trace, "-%d\n", line.second.back());
				else
					fprintf(trace, "\n");
			}
		}
	}

}

bool checkvector(BBData bb, unordered_map<ADDRINT, ResultVector > &instructionResults)
{
		vector<ADDRINT> addrs = bb.getAddrs();
		bool allSingle = true;
		bool allZero = true;
		bool someGreater = false;
		for(auto addr : addrs)
		{
			if(!(instructionResults[addr].isSingle() || instructionResults[addr].isZero()))
				allSingle = false;
			if(!instructionResults[addr].isZero())
				allZero = false;
			if(instructionResults[addr].vectorsGreater(KnobMinVectorCount.Value()))
				someGreater = true;
		}

		if(allSingle && !allZero && someGreater)
		{
			return true;
		}

	return false;
}

void BBSummary(FILE *trace, unordered_map<ADDRINT, ResultVector > &instructionResults)
{
	int bbcount = 0;
	int bbvector = 0;
	int bbvectorexecutedtotal = 0;
	int bbexecuted = 0;
	int bbexecutedtotal = 0;
	int bbminsize = INT_MAX;
	int bbmaxsize = 0;
	int minsuccessors = INT_MAX;
	int maxsuccessors = 0;

	for(auto bb : basicBlocks)
	{
		++bbcount;
		if(bb.getNumExecution())
		{
			++bbexecuted;
			bbexecutedtotal += bb.getNumExecution();

			if(checkvector(bb, instructionResults))
			{
				++bbvector;
				bbvectorexecutedtotal += bb.getNumExecution();
			}

			if(bb.getNumInstuructions() < bbminsize)
				bbminsize = bb.getNumInstuructions();

			if(bb.getNumInstuructions() > bbmaxsize)
				bbmaxsize = bb.getNumInstuructions();

			if(bb.getNumSuccessors() < minsuccessors )
				minsuccessors = bb.getNumSuccessors();

			if(bb.getNumSuccessors() > maxsuccessors)
				maxsuccessors = bb.getNumSuccessors();
		}
	}

	fprintf(trace,"Number of different blocks = %d\n", bbcount);
	fprintf(trace,"Number of different blocks vectorizeable  = %d\n", bbvector);
	fprintf(trace,"Number of different blocks executed = %d\n", bbexecuted);
	fprintf(trace,"Number of blocks executed as vector= %d\n", bbvectorexecutedtotal);
	fprintf(trace,"Number of blocks executed = %d\n", bbexecutedtotal);
	fprintf(trace,"Smallest Number of instructions in a block = %d\n", bbminsize);
	fprintf(trace,"Largest Number of instructions in a block = %d\n", bbmaxsize);
	fprintf(trace,"Smallest Number of succesors in a block = %d\n", minsuccessors);
	fprintf(trace,"Largest Number of succesors in a block = %d\n", maxsuccessors);

}

void logBasicBlock(BBL bbl, ADDRINT id, FILE *bbl_log)
{
	fprintf(bbl_log, "Basic Block starting at %p ending %p with Addresss %p UBBID %p\n",(void *) INS_Address(BBL_InsHead(bbl)), (void *) INS_Address(BBL_InsTail(bbl)), (void *) BBL_Address(bbl), (void *) id);
	for(auto ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins) )
	{
		fprintf(bbl_log, "%p\t%s\n", (void *) INS_Address(ins), INS_Disassemble(ins).c_str());
	}
	fprintf(bbl_log, "Baisc Block Finished\n");
}



void finalOutput(FILE *trace, FILE *bbl_log, unordered_map<ADDRINT, ResultVector > &instructionResults)
{
	if(KnobSummaryOn)
	{
		unsigned localInstructionCount = instructionCount;
		fprintf(trace, "#Start Summary\n");
		fprintf(trace, "#Total Instructions (No Vector Instructions) = %u\n", localInstructionCount);
		fprintf(trace, "#Total Instructions (Vector Instructions) = %u\n", localInstructionCount - vectorInstructionCountSavings);
	}

	if(KnobBBSummary)
	{
		BBSummary(trace, instructionResults);
	}

	// print basic block info
	if(KobForPrintBasicBlocks)
	{
		for(auto bb : basicBlocks)
		{
			bb.printBlock(trace);
		}
	}

	// Stub for dot format graph of basic blocks
	if(KnobBBDotLog)
	{
		WriteBBDotLog(bbl_log);
	}
	fclose(trace);
}

void traceOutput(FILE *trace, unordered_map<ADDRINT, ResultVector > &instructionResults)
{

	if(KnobVectorLineSummary)
	{
		writeOnOffLog(trace, instructionResults);
	}
	else if(KnobBBReport)
	{
		writeBBReport(trace, instructionResults);
	}
	else
	{
		writeLog(trace, instructionResults);
	}
}
