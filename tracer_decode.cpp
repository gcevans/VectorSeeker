
#include "tracer_decode.h"

#include <stdio.h>
#include <assert.h>
#include "tracerlib.h"


#define STATIC_CAST(x,y) ((x) (y))

VOID disassemblyToBuffInternal(char * decodeBuffer, VOID *ip, const xed_decoded_inst_t *ins);

// Do no tracing for these instruction categories
bool isIgnoredInstruction(xed_category_enum_t cat)
{
	if(cat == XED_CATEGORY_NOP)
		return true;
	if(cat == XED_CATEGORY_CALL)
		return true;
	if(cat == XED_CATEGORY_SYSCALL)
		return true;

	return false;
}

// Instructions only move data and do no computation
bool isMoveOnlyInstruction(xed_category_enum_t cat)
{
	if(cat == XED_CATEGORY_DATAXFER)
		return true;

	return false;
}

// Instruction uses x87
bool isX87Instruction(xed_category_enum_t cat)
{
	return(cat == XED_CATEGORY_X87_ALU);
}

// Do not trace dependencies through these registers
bool isIgnoredRegister(xed_reg_enum_t r)
{
	if(xed_reg_class(r) == XED_REG_CLASS_CR)
		return true;
	if(xed_reg_class(r) == XED_REG_CLASS_DR)
		return true;
	if(xed_reg_class(r) == XED_REG_CLASS_FLAGS)
		return true;
	if(xed_reg_class(r) == XED_REG_CLASS_SR)
		return true;
	if(xed_reg_class(r) == XED_REG_CLASS_INVALID)
		return true;
	if(xed_reg_class(r) == XED_REG_CLASS_IP)
		return true;
	if(xed_reg_class(r) == XED_REG_CLASS_MXCSR)
		return true;
	if(r == XED_REG_RSP)
		return true;
	if(r == XED_REG_STACKPUSH)
		return true;
	if(r == XED_REG_STACKPOP)
		return true;
	
	return false;
}

// Instructions that clear all dependencies
bool isZeroingInstruction(xed_decoded_inst_t *ins)
{
	if(xed_decoded_inst_get_iclass(ins) == XED_ICLASS_XOR || xed_decoded_inst_get_iclass(ins) == XED_ICLASS_PXOR)
		if(
		xed_decoded_inst_get_reg(ins,xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(ins),0))) ==
			xed_decoded_inst_get_reg(ins,xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(ins),1)))
		)
			return true;
	
	return false;
}

// Decode instruction at ip saving needed items to the instructionLocations map
instructionType decodeInstructionData(ADDRINT ip, unordered_map<ADDRINT,instructionLocationsData > &instructionLocations)
{
	const xed_inst_t *xedins;
	xed_decoded_inst_t ins;
	instructionType insType = NORMAL_INS_TYPE;
	instructionLocations[(ADDRINT)ip].ip = (ADDRINT)ip;
	xed_state_t dstate;
	xed_state_zero(&dstate);
	xed_state_init2(&dstate,XED_MACHINE_MODE_LONG_64,XED_ADDRESS_WIDTH_64b);
	xed_decoded_inst_zero_set_mode(&(ins), &dstate);
	xed_decode(&(ins),STATIC_CAST(const xed_uint8_t*,ip),15);
	xedins = xed_decoded_inst_inst(&(ins));
	xed_category_enum_t cat = xed_inst_category(xedins);
	if(isIgnoredInstruction(cat))
		insType = IGNORED_INS_TYPE;
	if(isMoveOnlyInstruction(cat))
		insType = MOVEONLY_INS_TYPE;
	if(isX87Instruction(cat))
		insType = X87_INS_TYPE;
	
	int numOperands = xed_decoded_inst_noperands(&(ins));
	int numMemOps = xed_decoded_inst_number_of_memory_operands(&ins);

	if(!isZeroingInstruction(&(ins)))
	{
		for(int i = 0; i < numOperands; i++)
		{
			const xed_operand_t *curOp = xed_inst_operand(xedins, i);
			if(xed_operand_read(curOp) )
			{
				xed_operand_enum_t op_name = xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(&(ins)),i));
				xed_reg_enum_t r = xed_decoded_inst_get_reg(&(ins), op_name);
				if(r)
				{
					if(!isIgnoredRegister(r))
					{
						if(xed_reg_class(r) == XED_REG_CLASS_GPR)
						{
							instructionLocations[(ADDRINT)ip].registers_read.push_back(xed_get_largest_enclosing_register(r));
						}
						else if(xed_gpr_reg_class(r) == XED_REG_CLASS_XMM)
						{
							// read from YMM register
							instructionLocations[(ADDRINT)ip].registers_read.push_back((xed_reg_enum_t)(r+16));
						}
						else
						{
							instructionLocations[(ADDRINT)ip].registers_read.push_back(r);
						}
					}
				}
			}
		}
		for(int i = 0; i < numMemOps; i++)
		{
//			if( !xed_decoded_inst_mem_read(&ins,i) && !xed_decoded_inst_mem_written(&ins,i))
			{
				xed_reg_enum_t r = xed_decoded_inst_get_seg_reg(&ins,i);
				if(r != XED_REG_INVALID)
				{
					if(!isIgnoredRegister(r))
					{
						if(xed_reg_class(r) == XED_REG_CLASS_GPR)
						{
							instructionLocations[(ADDRINT)ip].registers_read.push_back(xed_get_largest_enclosing_register(r));
						}
						else if(xed_gpr_reg_class(r) == XED_REG_CLASS_XMM)
						{
							// read from YMM register
							instructionLocations[(ADDRINT)ip].registers_read.push_back((xed_reg_enum_t)(r+16));
						}
						else
						{
							instructionLocations[(ADDRINT)ip].registers_read.push_back(r);
						}
					}
				}
				r = xed_decoded_inst_get_base_reg(&ins,i);
				if(r != XED_REG_INVALID)
				{
					if(!isIgnoredRegister(r))
					{
						if(xed_reg_class(r) == XED_REG_CLASS_GPR)
						{
							instructionLocations[(ADDRINT)ip].registers_read.push_back(xed_get_largest_enclosing_register(r));
						}
						else if(xed_gpr_reg_class(r) == XED_REG_CLASS_XMM)
						{
							// read from YMM register
							instructionLocations[(ADDRINT)ip].registers_read.push_back((xed_reg_enum_t)(r+16));
						}
						else
						{
							instructionLocations[(ADDRINT)ip].registers_read.push_back(r);
						}
					}
				}
				r = xed_decoded_inst_get_index_reg(&ins,i);
				if(r != XED_REG_INVALID)
				{
					if(!isIgnoredRegister(r))
					{
						if(xed_reg_class(r) == XED_REG_CLASS_GPR)
						{
							instructionLocations[(ADDRINT)ip].registers_read.push_back(xed_get_largest_enclosing_register(r));
						}
						else if(xed_gpr_reg_class(r) == XED_REG_CLASS_XMM)
						{
							// read from YMM register
							instructionLocations[(ADDRINT)ip].registers_read.push_back((xed_reg_enum_t)(r+16));
						}
						else
						{
							instructionLocations[(ADDRINT)ip].registers_read.push_back(r);
						}
					}
				}
			}
		}
	}

	for(int i = 0; i < numOperands; i++)
	{
		const xed_operand_t *curOp = xed_inst_operand(xedins, i);
		if(xed_operand_written(curOp) )
		{
			xed_operand_enum_t op_name = xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(&(ins)),i));
			xed_reg_enum_t r = xed_decoded_inst_get_reg(&(ins), op_name);
			if(r)
			{
				if(!isIgnoredRegister(r))
				{
					if(xed_reg_class(r) == XED_REG_CLASS_GPR)
					{
						instructionLocations[(ADDRINT)ip].registers_written.push_back(xed_get_largest_enclosing_register(r));
					}
					else if(xed_gpr_reg_class(r) == XED_REG_CLASS_XMM)
					{
						// store in YMM register
						instructionLocations[(ADDRINT)ip].registers_written.push_back((xed_reg_enum_t)(r+16));
					}
					else
					{
						instructionLocations[(ADDRINT)ip].registers_written.push_back(r);
					}
				}
			}
		}
	}
	PIN_LockClient();
	PIN_GetSourceLocation(ip, &(instructionLocations[ip].col_number), &(instructionLocations[ip].line_number), &(instructionLocations[ip].file_name));
	PIN_GetSourceLocation(ip, &(debugData[ip].col_number), &(debugData[ip].line_number), &(debugData[ip].file_name));
	PIN_UnlockClient();
	return insType;
}

// Standard form of instruction tracing for debugging
void instructionTracing(VOID * ip, VOID * addr, long int value, const char *called_from, FILE *out, ShadowMemory &shadowMemory )
{
	char decodeBuffer[1024];
	xed_state_t dstate;
	xed_state_zero(&dstate);
	xed_state_init2(&dstate,XED_MACHINE_MODE_LONG_64,XED_ADDRESS_WIDTH_64b);
	xed_decoded_inst_t ins;
	xed_decoded_inst_zero_set_mode(&ins, &dstate);
	xed_decode(&ins,STATIC_CAST(const xed_uint8_t*,ip),15);
	disassemblyToBuffInternal(decodeBuffer, ip, &ins);
	const xed_inst_t *xedins = xed_decoded_inst_inst(&ins);

	int numOperands = xed_decoded_inst_noperands(&ins);
	int numMemOps = xed_decoded_inst_number_of_memory_operands(&ins);

	INT32 source_column = 0;
	INT32 source_line = 0;
	string source_file;
	PIN_LockClient();
	PIN_GetSourceLocation((ADDRINT)ip, &source_column, &source_line, &source_file);
	PIN_UnlockClient();


	// fprintf(out, "%p:%s,%d:%s\t\tOps(%d)", ip, source_file.c_str(), source_line, decodeBuffer,numOperands);
	fprintf(out, "%p:%s\t\tOps(%d)", ip, decodeBuffer,numOperands);

	for(int i = 0; i < numOperands; i++)
	{
		const xed_operand_t *curOp = xed_inst_operand(xedins, i);
		fprintf(out, "%d:", (i+1));
		if(xed_operand_read(curOp) )
		{
			xed_operand_enum_t op_name = xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(&ins),i));
			if(xed_decoded_inst_get_reg(&ins, xed_operand_name(curOp)))
			{
				xed_reg_enum_t r = xed_decoded_inst_get_reg(&ins, op_name);
				fprintf(out,"r:%s=%ld ",xed_reg_enum_t2str(r),shadowMemory.readReg(r));
				if(r != XED_REG_CR0)
					value = max(shadowMemory.readReg(r),value);
			}
			else
			{
				fprintf(out,"r:not a regitster (%s)",xed_operand_enum_t2str(op_name));
			}
		}
		if(xed_operand_written(curOp) )
		{
			xed_operand_enum_t op_name = xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(&ins),i));
			if(xed_decoded_inst_get_reg(&ins, xed_operand_name(curOp)))
			{
				xed_reg_enum_t r = xed_decoded_inst_get_reg(&ins, op_name);
				fprintf(out,"w:%s=%ld ",xed_reg_enum_t2str(r),shadowMemory.readReg(r));
				if(r != XED_REG_CR0)
					value = max(shadowMemory.readReg(r),value);
			}
			else
			{
				fprintf(out, "w:not a register (%s)",xed_operand_enum_t2str(op_name));
			}
		}
		if(!xed_operand_written(curOp) && !xed_operand_read(curOp))
		{
			fprintf(out,"not read or written ");
		}
	}
	
	for(int i = 0; i < numMemOps; i++)
	{
//		if( !xed_decoded_inst_mem_read(&ins,i) && !xed_decoded_inst_mem_written(&ins,i))
		{
			fprintf(out, "mem op(");
			xed_reg_enum_t seg = xed_decoded_inst_get_seg_reg(&ins,i);
			xed_reg_enum_t base = xed_decoded_inst_get_base_reg(&ins,i);
			xed_reg_enum_t index = xed_decoded_inst_get_index_reg(&ins,i);
			if (seg != XED_REG_INVALID)
			{
				fprintf(out, "seg=%s ",xed_reg_enum_t2str(seg));
			}
			if (base != XED_REG_INVALID)
			{
				fprintf(out, "base=%s ",xed_reg_enum_t2str(base));
			}
			if (index != XED_REG_INVALID)
			{
				fprintf(out, "index=%s ",xed_reg_enum_t2str(index));
			}
			fprintf(out, ")");
			
		}
	}
	
	if(addr != NULL)
		fprintf(out, "addr=%p ",addr);
	
	fprintf(out, "value=%ld\n",value);
}


VOID disassemblyToBuffInternal(char * decodeBuffer, VOID *ip, const xed_decoded_inst_t *ins)
{
	xed_print_info_t pi;
	xed_init_print_info(&pi);
	pi.p = ins;
	pi.blen = 1024;
	pi.buf = decodeBuffer;
	pi.context = 0;
	pi.disassembly_callback = 0;
	pi.runtime_address = STATIC_CAST(xed_uint64_t,ip);
	//pi.syntax = syntax
	pi.format_options_valid = 0; // use defaults
    pi.buf[0]=0; //allow use of strcat
    int ok = xed_format_generic(&pi);
    if (!ok)
    {
        pi.blen = xed_strncpy(pi.buf,"Error disassembling ",pi.blen);
        pi.blen = xed_strncat(pi.buf,
                               xed_syntax_enum_t2str(pi.syntax),
                               pi.blen);
        pi.blen = xed_strncat(pi.buf," syntax.",pi.blen);
    }
	//xed_format_intel(&ins,decodeBuffer,1024,STATIC_CAST(xed_uint64_t,ip));	
}

VOID disassemblyToBuff(char * decodeBuffer, VOID *ip)
{
	xed_state_t dstate;
	xed_state_zero(&dstate);
	xed_state_init2(&dstate,XED_MACHINE_MODE_LONG_64,XED_ADDRESS_WIDTH_64b);
	xed_decoded_inst_t ins;
	xed_decoded_inst_zero_set_mode(&ins, &dstate);
	xed_decode(&ins,STATIC_CAST(const xed_uint8_t*,ip),15);
	disassemblyToBuffInternal(decodeBuffer, ip, &ins);
}

const char *getInstCategoryString(VOID *ip)
{
	xed_state_t dstate;
	xed_state_zero(&dstate);
	xed_state_init2(&dstate,XED_MACHINE_MODE_LONG_64,XED_ADDRESS_WIDTH_64b);
	xed_decoded_inst_t ins;
	xed_decoded_inst_zero_set_mode(&ins, &dstate);
	xed_decode(&ins,STATIC_CAST(const xed_uint8_t*,ip),15);
	return xed_category_enum_t2str(xed_inst_category(xed_decoded_inst_inst(&(ins))));
}


