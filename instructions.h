#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include "instlib.H"
#include <list>	
#include <vector>

extern const UINT32 NONE_OPERATOR_TYPE;
extern const UINT32 READ_OPERATOR_TYPE;
extern const UINT32 WRITE_OPERATOR_TYPE;
extern const UINT32 BOTH_OPERATOR_TYPE;

enum instructionType { IGNORED_INS_TYPE, NORMAL_INS_TYPE, MOVEONLY_INS_TYPE, X87_INS_TYPE};

struct instructionLocationsData
{
	instructionLocationsData(){ip=0;logged = FALSE;};
	vector<xed_reg_enum_t> registers_read;
	vector<xed_reg_enum_t> registers_written;
	list<long long> loopid;
	ADDRINT ip;
	bool logged;
	instructionType type;
	UINT32 memOperands;
	string rtn_name;
};

struct instructionDebugData
{
	instructionDebugData(){line_number=0;col_number=0;};
	string file_name;
	int line_number;
	int col_number; // Not reported
	bool logged;
	string instruction;
};

#endif
