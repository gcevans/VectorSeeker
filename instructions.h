#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include "instlib.H"
#include <list>	
#include <vector>

// These are used as a bit field to check operand types
enum OPTYPES : UINT32
{
	NONE_OPERATOR_TYPE = 0,
	READ_OPERATOR_TYPE = 1,
	WRITE_OPERATOR_TYPE = 2,
	BOTH_OPERATOR_TYPE = 3	
};

enum instructionType
{
	IGNORED_INS_TYPE,	// Instruction is not traced at all
	NORMAL_INS_TYPE,	// Standard instruction
	MOVEONLY_INS_TYPE,	// Instruction only moves memory or registers
	X87_INS_TYPE		// X87 Instruction that is not supported
};

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
	USIZE memReadSize;
	USIZE memWriteSize;
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
