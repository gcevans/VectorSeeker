#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include "instlib.H"
#include <list>	
#include <vector>

enum instructionType { IGNORED_INS_TYPE, NORMAL_INS_TYPE, MOVEONLY_INS_TYPE, X87_INS_TYPE};

struct instructionLocationsData
{
	instructionLocationsData(){ip=0;execution_count=0;line_number=0;col_number=0;logged = FALSE;};
	vector<xed_reg_enum_t> registers_read;
	vector<xed_reg_enum_t> registers_written;
	long execution_count;
	list<long long> loopid;
	ADDRINT ip;
	string file_name;
	int line_number;
	int col_number;
	bool logged;
	instructionType type;
	UINT32 memOperands;
	string rtn_name;
};

struct instructionDebugData
{
	string file_name;
	int line_number;
	int col_number;
	bool logged;
	string instruction;
};

#endif
