
#define COUNT 10
#include <stdlib.h>
#include "tracerlib.h"
#include <stdio.h>

double *A;
double *B;
double *C;
double *D;
size_t *I;

struct chunck
{
	 size_t start,end;
	 double *data;
};

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
	for(int i = 0; i < COUNT*4; i++)
	{
		A[i] = B[i] + 1;
	}
}

void basic2() 
{
        for(int i = 0; i < COUNT; i++)
        {
                A[i] = C[i] + 1;
        }
}

void dependant()


{
	for(int i = 1; i < COUNT*3; i++)
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

void loop_bounds_dynamic(size_t start, size_t end, double *data, double *data2)
{
	for(size_t i = start; i <= end; i++)
	{
		data[i] = data[i]+data2[i]*2;
	}
}

void caseloop()
{
	for(int i = 0; i < COUNT; i++)
	{
		switch(i)
		{
		case 1:
			A[i] = B[i]+C[i];
		default:
			C[i] = B[i]+1;
		}
	}
}

int main()
{
	A = (double *) malloc(sizeof(double) * COUNT * 10);
	B = (double *) malloc(sizeof(double) * COUNT * 10);
	C = (double *) malloc(sizeof(double) * COUNT * 10);
	I = (size_t *) malloc(sizeof(size_t)*COUNT * 10);
	chunck *chuncks = (chunck *) malloc (sizeof(chunck)*2);
	chuncks[0].data = (double *) malloc(sizeof(double) * COUNT * 10);
	chuncks[1].data = (double *) malloc(sizeof(double) * COUNT * 10);

 for(size_t i = 0; i < COUNT*10; i++)
	{
		chuncks[0].data[i] = 1;
		chuncks[1].data[i] = 1;
	}

	chuncks[0].start = COUNT;
	chuncks[0].end = COUNT+COUNT;
	chuncks[1].start = COUNT;
	chuncks[1].end = COUNT+COUNT;

	loop_bounds_dynamic(chuncks[0].start,chuncks[0].end,chuncks[0].data,chuncks[1].data);

	init();
	basic();
	
	init();
	indirect();
	
	init();
	dependant();
	basic2();
	caseloop();

    init();
    C[0] = reduction();
    
	free(A);
	free(B);
	free(C);
	free(I);

	return 0;
}

