#include <stdio.h>
#include "pin.H"
#include <assert.h>
#include "tracerlib.h"
#include "tracer_decode.h"
#include "tracer.h"


#define STATIC_CAST(x,y) ((x) (y))

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
instructionType decodeInstructionData(ADDRINT ip)
{
	const xed_inst_t *xedins;
	xed_decoded_inst_t ins;
	instructionType insType = NORMAL_INS_TYPE;
	instructionLocations[(ADDRINT)ip].ip = (ADDRINT)ip;
	xed_state_t dstate;
	xed_state_zero(&dstate);
	xed_state_init(&dstate,XED_MACHINE_MODE_LONG_64,XED_ADDRESS_WIDTH_64b,XED_ADDRESS_WIDTH_64b);
	xed_decoded_inst_zero_set_mode(&(ins), &dstate);
	xed_decode_cache(&(ins),STATIC_CAST(const xed_uint8_t*,ip),15,&xedDecodeCache);
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
				if(xed_operand_reg(curOp))
				{
					xed_operand_enum_t op_name = xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(&(ins)),i));
					xed_reg_enum_t r = xed_decoded_inst_get_reg(&(ins), op_name);
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
			if(xed_operand_reg(curOp))
			{
				xed_operand_enum_t op_name = xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(&(ins)),i));
				xed_reg_enum_t r = xed_decoded_inst_get_reg(&(ins), op_name);
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
	PIN_UnlockClient();
	return insType;
}

// Standard form of instruction tracing for debugging
void instructionTracing(VOID * ip, VOID * addr, long int value, const char *called_from)
{
	char decodeBuffer[1024];
	xed_state_t dstate;
	xed_state_zero(&dstate);
	xed_state_init(&dstate,XED_MACHINE_MODE_LONG_64,XED_ADDRESS_WIDTH_64b,XED_ADDRESS_WIDTH_64b);
	xed_decoded_inst_t ins;
	xed_decoded_inst_zero_set_mode(&ins, &dstate);
	xed_decode_cache(&ins,STATIC_CAST(const xed_uint8_t*,ip),15,&xedDecodeCache);
	xed_format_intel(&ins,decodeBuffer,1024,STATIC_CAST(xed_uint64_t,ip));
	const xed_inst_t *xedins = xed_decoded_inst_inst(&ins);

	int numOperands = xed_decoded_inst_noperands(&ins);
	int numMemOps = xed_decoded_inst_number_of_memory_operands(&ins);

	INT32 source_column = 0;
	INT32 source_line = 0;
	string source_file;
	PIN_LockClient();
	PIN_GetSourceLocation((ADDRINT)ip, &source_column, &source_line, &source_file);
	PIN_UnlockClient();


	fprintf(trace, "%p:%d:%s - %s:%s\t\t", ip,source_line, called_from, source_file.c_str(), decodeBuffer);

	for(int i = 0; i < numOperands; i++)
	{
		const xed_operand_t *curOp = xed_inst_operand(xedins, i);
		if(xed_operand_read(curOp) )
		{
			if(xed_decoded_inst_get_reg(&ins, xed_operand_name(curOp)))
			{
				xed_operand_enum_t op_name = xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(&ins),i));
				xed_reg_enum_t r = xed_decoded_inst_get_reg(&ins, op_name);
				fprintf(trace,"r:%s=%ld ",xed_reg_enum_t2str(r),shadowMemory.readReg(r));
				if(r != XED_REG_CR0)
					value = max(shadowMemory.readReg(r),value);
			}
			else
			{
				fprintf(trace,"not a regitster ");
			}
		}
		if(xed_operand_written(curOp) )
		{
			if(xed_operand_reg(curOp))
			{
				xed_operand_enum_t op_name = xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(&ins),i));
				xed_reg_enum_t r = xed_decoded_inst_get_reg(&ins, op_name);
				fprintf(trace,"w:%s=%ld ",xed_reg_enum_t2str(r),shadowMemory.readReg(r));
				if(r != XED_REG_CR0)
					value = max(shadowMemory.readReg(r),value);
			}
		}
		if(!xed_operand_written(curOp) && !xed_operand_read(curOp))
		{
			fprintf(trace,"not read or written ");
		}
	}
	
	for(int i = 0; i < numMemOps; i++)
	{
//		if( !xed_decoded_inst_mem_read(&ins,i) && !xed_decoded_inst_mem_written(&ins,i))
		{
			fprintf(trace, "mem op(");
			xed_reg_enum_t seg = xed_decoded_inst_get_seg_reg(&ins,i);
			xed_reg_enum_t base = xed_decoded_inst_get_base_reg(&ins,i);
			xed_reg_enum_t index = xed_decoded_inst_get_index_reg(&ins,i);
			if (seg != XED_REG_INVALID)
			{
				fprintf(trace, "seg=%s ",xed_reg_enum_t2str(seg));
			}
			if (base != XED_REG_INVALID)
			{
				fprintf(trace, "base=%s ",xed_reg_enum_t2str(base));
			}
			if (index != XED_REG_INVALID)
			{
				fprintf(trace, "index=%s ",xed_reg_enum_t2str(index));
			}
			fprintf(trace, ")");
			
		}
	}
	
	if(addr != NULL)
		fprintf(trace, "addr=%p ",addr);
	
	fprintf(trace, "value=%ld\n",value);
}

// Exessivly verbose instruction tracing
/*
void printdetails(xed_decoded_inst_t ins)
{
	int numMemOps = xed_decoded_inst_number_of_memory_operands(&ins);
	for(int i = 0; i < numMemOps; i++)
	{
		//xed_bool_t r_or_w = false;
        if ( xed_decoded_inst_mem_read(&ins,i)) {
            fprintf(trace,"read ");
            //r_or_w = true;
        }
        if (xed_decoded_inst_mem_written(&ins,i)) {
            fprintf(trace,"written ");
            //r_or_w = true;
        }
        xed_reg_enum_t seg = xed_decoded_inst_get_seg_reg(&ins,i);
		if (seg != XED_REG_INVALID) {
            fprintf(trace,"SEG= %s ",xed_reg_enum_t2str(seg));
        }
        xed_reg_enum_t base = xed_decoded_inst_get_base_reg(&ins,i);
        if (base != XED_REG_INVALID) {
            fprintf(trace,"BASE= %s/%s ",xed_reg_enum_t2str(base), xed_reg_class_enum_t2str(xed_reg_class(base)));
        }
        xed_reg_enum_t indx = xed_decoded_inst_get_index_reg(&ins,i);
        if (i == 0 && indx != XED_REG_INVALID) {
            fprintf(trace,"INDEX= %s/%s ",xed_reg_enum_t2str(indx),xed_reg_class_enum_t2str(xed_reg_class(indx)));
            if (xed_decoded_inst_get_scale(&ins,i) != 0) {
                // only have a scale if the index exists.
                fprintf(trace,"SCALE= %u ",xed_decoded_inst_get_scale(&ins,i));
            }
        }
	}
	int noperands = xed_inst_noperands(xed_decoded_inst_inst(&ins));
	fprintf(trace,"\niClass=%s",xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(&ins)));
	for(int i = 0; i < noperands; i++)
	{
		xed_reg_enum_t r;

		xed_operand_enum_t op_name = xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(&ins),i));
		fprintf(trace,"\t%s",xed_operand_enum_t2str(op_name));
		switch(op_name)
		{
		case XED_OPERAND_REG0:
		case XED_OPERAND_REG1:
		case XED_OPERAND_REG2:
		case XED_OPERAND_REG3:
		case XED_OPERAND_REG4:
		case XED_OPERAND_REG5:
		case XED_OPERAND_REG6:
		case XED_OPERAND_REG7:
		case XED_OPERAND_REG8:
		case XED_OPERAND_REG9:
		case XED_OPERAND_REG10:
		case XED_OPERAND_REG11:
		case XED_OPERAND_REG12:
		case XED_OPERAND_REG13:
		case XED_OPERAND_REG14:
		case XED_OPERAND_REG15:
		case XED_OPERAND_BASE0:
		case XED_OPERAND_BASE1:
 			r = xed_decoded_inst_get_reg(&ins, op_name);
 			fprintf(trace,"=%s",xed_reg_enum_t2str(r));
			break;
		default:
			break;
		}
	}
	

}
*/

