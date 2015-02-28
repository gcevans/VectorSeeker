
#define COUNT 10
#include <stdlib.h>
#include "tracerlib.h"
#include <stdio.h>

double *A;
double *B;
double *C;
double *D;
size_t *I;

void init()
{
	for(int i = 0; i < COUNT; i++)
	{
		A[i] = 1;
		B[i] = 1;
		C[i] = 1;
	}

	for(int i = 0; i < COUNT; i++)
	{
		I[i] = i;
	}
}

void indirect()
{
	for(int i = 0; i < COUNT; i++)
	{
		A[I[i]] = B[i] + 1;
	}	
}

void basic()
{
	for(int i = 0; i < COUNT; i++)
	{
		A[i] = B[i] + 1;
	}
}

void dependant()
{
	for(int i = 1; i < COUNT; i++)
	{
		C[i] = C[i-1] + 1;
	}	
}

double reduction()
{
    double c = 0;
    for(int i = 0; i < COUNT; i++)
    {
        c += (A[i] * B[i]);
    }
    return c;
}

int main()
{
	A = (double *) malloc(sizeof(double) * COUNT);
	B = (double *) malloc(sizeof(double) * COUNT);
	C = (double *) malloc(sizeof(double) * COUNT);
	I = (size_t *) malloc(sizeof(size_t)*COUNT);

	init();
	basic();
	
	init();
	indirect();
	
	init();
	dependant();

    init();
    C[0] = reduction();
    
	free(A);
	free(B);
	free(C);
	free(I);

	return 0;
}

