
#define COUNT 100
#include <stdlib.h>
#include "dummy.h"
#include "tracerlib.h"
#include <stdio.h>


void foo(double *A,double *A1, double *B,char *C,size_t *I)
{
	_tracer_loopstart(1);
	_tracer_loopstart(2);
	for(int i = 0; i < COUNT; i++)
	{
		I[i] = i;
	}
	_tracer_loopend(2);
	
// 	for(int i = 1; i < COUNT; i++)
//  	{
//  		I[i] = I[i-1] + I[i] - 1;
//  	}
//  	I[0] = 0;
	
	for(int i = 0; i < COUNT; i++)
	{
		A[I[i]]=1;
	}
	
	for(int i = 0; i < COUNT; i++)
	{
		A1[i] = B[i] = 1;
		A[i] = 1;
		C[i] = 1;
	}
//	ftracer_traceon_();
	for(int i = 0; i < COUNT; i++)
	{
		A[i] = B[i] + 1;
	}
//	ftracer_traceoff_();
	for(int i = 1; i < COUNT; i++)
	{
		A[i] = add_doubles_function(A[i],1);
	}	
	for(int i = 1; i < COUNT; i++)
	{
		A[i] = A[i-1] + 1;
	}
	for(int i = 1; i < COUNT; i++)
	{
		A1[i] = add_doubles_function(A1[i-1],1);
	}
	for(int i = 0; i < COUNT; i++)
	{
		C[i] = (char)( (int) C[i] + (int) 2);
	}
	for(int i = 1; i < COUNT; i++)
	{
		C[i] = C[i-1] + 1;
	}
	_tracer_loopend(1);
}

int main()
{
	double *A = (double *) malloc(sizeof(double) * COUNT);
	double *A1 = (double *) malloc(sizeof(double) * COUNT);
	double *B = (double *) malloc(sizeof(double) * COUNT);
	char *C = (char *) malloc(sizeof(char) *COUNT);
	size_t *I = (size_t *) malloc(sizeof(size_t)*COUNT);
	double ASTACK[COUNT];
//	int size = sizeof(double) * COUNT;
	
	
//	_tracer_array_memory(&ASTACK[0],sizeof(double) * COUNT);
	//ftracer_array_memory_(&ASTACK[0],&size);

	foo(&ASTACK[0],A1,B,C,I);

	dummy2_1D(A,B);
	
//	_tracer_array_memory_clear(&ASTACK[0]);
	
	free(A);
	free(A1);
	free(B);

	return 0;
}

