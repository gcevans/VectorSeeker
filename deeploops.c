/*
 Test loops derived from the TEST SUITE FOR VECTORIZING COMPILERS
 
 gcevans@gmail.com

*/

/*

Copyright (c) 2009 University of Illinois at Urbana-Champaign.  All rights reserved.



Developed by: Polaris Research Group

              University of Illinois at Urbana-Champaign

              http://polaris.cs.uiuc.edu



Permission is hereby granted, free of charge, to any person obtaining a copy

of this software and associated documentation files (the "Software"), to

deal with the Software without restriction, including without limitation the

rights to use, copy, modify, merge, publish, distribute, sublicense, and/or

sell copies of the Software, and to permit persons to whom the Software is

furnished to do so, subject to the following conditions:

  1. Redistributions of source code must retain the above copyright notice,

     this list of conditions and the following disclaimers.

  2. Redistributions in binary form must reproduce the above copyright

     notice, this list of conditions and the following disclaimers in the

     documentation and/or other materials provided with the distribution.

  3. Neither the names of Polaris Research Group, University of Illinois at

     Urbana-Champaign, nor the names of its contributors may be used to endorse

     or promote products derived from this Software without specific prior

     written permission.



THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR

IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,

FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE

CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER

LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING

FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS

WITH THE SOFTWARE.
*/

/*
 * This test is a translation of the TEST SUITE FOR VECTORIZING COMPILERS
 * from Fortran to C. The collection was written by:
 *  - David Callahan
 *  - Jack Dongarra
 *  - David Levine
 * The original benchmark can be downloaded from:
 *                    http://www.netlib.org/benchmark/vectors
 * The changes that we made to the original code:
 *  - We used f2c to convert the code to C.
 *
 *  - In the original code, each loop is executed with array lengths of
 * 10, 100, 1000. In this version the array length can be set by the 
 * macros LEN and LEN2 for 1 and 2 dimensional arrays, respectively.
 *
 *  - Each loop is executed multiple times to increase the running time.
 * This number can be changed by the macro ntimes.
 *
 *  - The arrays start from 1 in Fortran and 0 in C. We have made all the
 * necessary changes in this regard.
 *
 *  - Fortran uses column-major and C uses row-major order. We have made 
 * all of the necessary changes in this regard
 *
 *  - We have expanded the collection with some of our loops. These loops
 * can be recognized by their function name; they start with an s and are
 * followed by a 4-digit number with the exception of the functions below
 * which came from the original TEST SUITE:
 * Loops: s2710-s2712, s2101-s2102, s2111, s3110-s3113, s4112-s4117, s4121
 *
 *  - All the arrays are declared statically and globally and all of them are
 * 16-byte aligned.
 *
 * To compile the collection, the dummy.c file needs to be compiled separately
 * and the generated object file is linked to this file. An example of a
 * makefile is included in the package for your reference.
 * 
 *
 *  The output includes three columns:
 *	Loop:		The name of the loop
 *	Time(Sec): 	The time in seconds to run the loop
 *	Checksum:	The checksum calculated when the test has run
 *
 *
 * This version of the Benchmark is provided by:
 *  - Saeed Maleki   (maleki1@illinois.edu)  - University of Illinois at Urbana-Champaign
 *  - David Padua    (padua@illinois.edu)    - University of Illinois at Urbana-Champaign
 *  - Maria Garzaran (garzaran@illinois.edu) - University of Illinois at Urbana-Champaign
 */

#define COUNT 10
#include <stdlib.h>
#include "tracerlib.h"
#include <stdio.h>


#define LEN 32000
#define LEN2 10

#define ntimes 100000

float array[LEN2*LEN2];

float x[LEN];
float temp;
int temp_int;

float a[LEN];
float b[LEN];
float c[LEN];
float d[LEN];
float e[LEN];
float aa[LEN2][LEN2];
float bb[LEN2][LEN2];
float cc[LEN2][LEN2];
float tt[LEN2][LEN2];

int indx[LEN];

float *xx;
float *yy;


int set1d(float arr[LEN], float value, int stride)
{
	int i;
	if (stride == -1) {
		for (int i = 0; i < LEN; i++) {
			arr[i] = 1. / (float) (i+1);
		}
	} else if (stride == -2) {
		for (int i = 0; i < LEN; i++) {
			arr[i] = 1. / (float) ((i+1) * (i+1));
		}
	} else {
		for (int i = 0; i < LEN; i += stride) {
			arr[i] = value;
		}
	}
	return 0;
}

int set1ds(int _n, float arr[LEN], float value, int stride)
{
	int i;
	if (stride == -1) {
		for (int i = 0; i < LEN; i++) {
			arr[i] = 1. / (float) (i+1);
		}
	} else if (stride == -2) {
		for (int i = 0; i < LEN; i++) {
			arr[i] = 1. / (float) ((i+1) * (i+1));
		}
	} else {
		for (int i = 0; i < LEN; i += stride) {
			arr[i] = value;
		}
	}
	return 0;
}

int set2d(float arr[LEN2][LEN2], float value, int stride)
{

//  -- initialize two-dimensional arraysft

	if (stride == -1) {
		for (int i = 0; i < LEN2; i++) {
			for (int j = 0; j < LEN2; j++) {
				arr[i][j] = 1. / (float) (i+1);
			}
		}
	} else if (stride == -2) {
		for (int i = 0; i < LEN2; i++) {
			for (int j = 0; j < LEN2; j++) {
				arr[i][j] = 1. / (float) ((i+1) * (i+1));
			}
		}
	} else {
		for (int i = 0; i < LEN2; i++) {
			for (int j = 0; j < LEN2; j += stride) {
				arr[i][j] = value;
			}
		}
	}
	return 0;
}

float sum1d(float arr[LEN]){
	float ret = 0.;
	for (int i = 0; i < LEN; i++)
		ret += arr[i];
	return ret;
}

float sum2d(float arr[LEN][LEN]){
	float ret = 0.;
	for (int i = 0; i < LEN2; i++)
		for (int j = 0; j < LEN2; j++)
			ret += arr[i][j];
	return ret;
}

void markmemory()
{
	_tracer_array_memory(&array[0],sizeof(float)*LEN2*LEN2);
	_tracer_array_memory(&x[0],sizeof(float)*LEN);
	_tracer_array_memory(&a[0],sizeof(float)*LEN);
	_tracer_array_memory(&b[0],sizeof(float)*LEN);
	_tracer_array_memory(&c[0],sizeof(float)*LEN);
	_tracer_array_memory(&d[0],sizeof(float)*LEN);
	_tracer_array_memory(&e[0],sizeof(float)*LEN);
	_tracer_array_memory(&aa[0],sizeof(float)*LEN2*LEN2);
	_tracer_array_memory(&bb[0],sizeof(float)*LEN2*LEN2);
	_tracer_array_memory(&cc[0],sizeof(float)*LEN2*LEN2);
	_tracer_array_memory(&tt[0],sizeof(float)*LEN2*LEN2);
	_tracer_array_memory(&indx[0],sizeof(float)*LEN);
}

int init(char* name)
{
	float any=0.;
	float zero=0.;
	float half=.5;
	float one=1.;
	float two=2.;
	float small = .000001;
	int unit =1;
	int frac=-1;
	int frac2=-2;

	if	(name == "s111 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
		set1d(c, any,frac2);
		set1d(d, any,frac2);
		set1d(e, any,frac2);
	} else if (name == "s112 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s113 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s114 ") {
		set2d(aa, any,frac);
		set2d(bb, any,frac2);
	} else if (name == "s115 ") {
		set1d(a, one,unit);
		set2d(aa,small,unit);
	} else if (name == "s116 ") {
		set1d(a, one,unit);
	} else if (name == "s118 ") {
		set1d(a, one,unit);
		set2d(bb,small,unit);
	} else if (name == "s119 ") {
		set2d(aa, one,unit);
		set2d(bb, any,frac2);
	} else if (name == "s121 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s122 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s123 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s124 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s125 ") {
		set1ds(LEN*LEN, array,zero,unit);
		set2d(aa, one,unit);
		set2d(bb,half,unit);
		set2d(cc, two,unit);
	} else if (name == "s126 ") {
		set2d(bb, one,unit);
		set1ds(LEN*LEN,array,any,frac);
		set2d(cc, any,frac);
	} else if (name == "s127 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s128 ") {
		set1d(a,zero,unit);
		set1d(b, two,unit);
		set1d(c, one,unit);
		set1d(d, one,unit);
	} else if (name == "s131 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s132 ") {
		set2d(aa, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s141 ") {
		set1ds(LEN*LEN,array, one,unit);
		set2d(bb, any,frac2);
	} else if (name == "s151 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s152 ") {
		set1d(a, one,unit);
		set1d(b,zero,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s161 ") {
		set1d(a, one,unit);
		set1ds(LEN/2,&b[0], one,2);
		set1ds(LEN/2,&b[1],-one,2);
		set1d(c, one,unit);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s162 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s171 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s172 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s173 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s174 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s175 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s176 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s211 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s212 ") {
		set1d(a, any,frac);
		set1d(b, one,unit);
		set1d(c, one,unit);
		set1d(d, any,frac);
	} else if (name == "s221 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
		set1d(d, any,frac);
	} else if (name == "s222 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
	} else if (name == "s231 ") {
		set2d(aa, one,unit);
		set2d(bb, any,frac2);
	} else if (name == "s232 ") {
		set2d(aa, one,unit);
		set2d(bb,zero,unit);
	} else if (name == "s233 ") {
		set2d(aa, any,frac);
		set2d(bb, any,frac);
		set2d(cc, any,frac);
	} else if (name == "s234 ") {
		set2d(aa, one,unit);
		set2d(bb, any,frac);
		set2d(cc, any,frac);
	} else if (name == "s235 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
		set2d(aa, one,unit);
		set2d(bb, any, frac2);
	} else if (name == "s241 ") {
		set1d(a, one,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
		set1d(d, one,unit);
	} else if (name == "s242 ") {
		set1d(a,small,unit);
		set1d(b,small,unit);
		set1d(c,small,unit);
		set1d(d,small,unit);
	} else if (name == "s243 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s244 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c,small,unit);
		set1d(d,small,unit);
	} else if (name == "s251 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s252 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
	} else if (name == "s253 ") {
		set1d(a, one,unit);
		set1d(b,small,unit);
		set1d(c, one,unit);
		set1d(d, any,frac);
	} else if (name == "s254 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
	} else if (name == "s255 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
	} else if (name == "s256 ") {
		set1d(a, one,unit);
		set2d(aa, two,unit);
		set2d(bb, one,unit);
	} else if (name == "s257 ") {
		set1d(a, one,unit);
		set2d(aa, two,unit);
		set2d(bb, one,unit);
	} else if (name == "s258 ") {
		set1d(a, any,frac);
		set1d(b,zero,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e,zero,unit);
		set2d(aa, any,frac);
	} else if (name == "s261 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
		set1d(c, any,frac2);
		set1d(d, one,unit);
	} else if (name == "s271 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s272 ") {
		set1d(a, one,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, two,unit);
	} else if (name == "s273 ") {
		set1d(a, one,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
		set1d(d,small,unit);
		set1d(e, any,frac);
	} else if (name == "s274 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s275 ") {
		set2d(aa, one,unit);
		set2d(bb,small,unit);
		set2d(cc,small,unit);
	} else if (name == "s276 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
		set1d(d, any,frac);
	} else if (name == "s277 ") {
		set1d(a, one,unit);
		set1ds(LEN/2,b, one,unit);
		set1ds(LEN/2,&b[LEN/2],-one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s278 ") {
		set1ds(LEN/2,a,-one,unit);
		set1ds(LEN/2,&a[LEN/2],one,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s279 ") {
		set1ds(LEN/2,a,-one,unit);
		set1ds(LEN/2,&a[LEN/2],one,unit);
//		set1d(a, -one,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s2710") {
		set1d(a, one,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s2711") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s2712") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s281 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
	} else if (name == "s291 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
	} else if (name == "s292 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
	} else if (name == "s293 ") {
		set1d(a, any,frac);
	} else if (name == "s2101") {
		set2d(aa, one,unit);
		set2d(bb, any,frac);
		set2d(cc, any,frac);
	} else if (name == "s2102") {
		set2d(aa,zero,unit);
	} else if (name == "s2111") {
//		set2d(aa, one,unit);
		set2d(aa,zero,unit);
	} else if (name == "s311 ") {
		set1d(a, any,frac);
	} else if (name == "s312 ") {
		set1d(a,1.000001,unit);
	} else if (name == "s313 ") {
		set1d(a, any,frac);
		set1d(b, any,frac);
	} else if (name == "s314 ") {
		set1d(a, any,frac);
	} else if (name == "s315 ") {
		set1d(a, any,frac);
	} else if (name == "s316 ") {
		set1d(a, any,frac);
	} else if (name == "s317 ") {
	} else if (name == "s318 ") {
		set1d(a, any,frac);
		a[LEN-1] = -two;
	} else if (name == "s319 ") {
		set1d(a,zero,unit);
		set1d(b,zero,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s3110") {
		set2d(aa, any,frac);
		aa[LEN2-1][LEN2-1] = two;
	} else if (name == "s3111") {
		set1d(a, any,frac);
	} else if (name == "s3112") {
		set1d(a, any,frac2);
		set1d(b,zero,unit);
	} else if (name == "s3113") {
		set1d(a, any,frac);
		a[LEN-1] = -two;
	} else if (name == "s321 ") {
		set1d(a, one,unit);
		set1d(b,zero,unit);
	} else if (name == "s322 ") {
		set1d(a, one,unit);
		set1d(b,zero,unit);
		set1d(c,zero,unit);
	} else if (name == "s323 ") {
		set1d(a, one,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s331 ") {
		set1d(a, any,frac);
		a[LEN-1] = -one;
	} else if (name == "s332 ") {
		set1d(a, any,frac2);
		a[LEN-1] = two;
	} else if (name == "s341 ") {
		set1d(a,zero,unit);
		set1d(b, any,frac);
	} else if (name == "s342 ") {
		set1d(a, any,frac);
		set1d(b, any,frac);
	} else if (name == "s343 ") {
		set2d(aa, any,frac);
		set2d(bb, one,unit);
	} else if (name == "s351 ") {
		set1d(a, one,unit);
		set1d(b, one,unit);
		c[0] = 1.;
	} else if (name == "s352 ") {
		set1d(a, any,frac);
		set1d(b, any,frac);
	} else if (name == "s353 ") {
		set1d(a, one,unit);
		set1d(b, one,unit);
		c[0] = 1.;
	} else if (name == "s411 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s412 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s413 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s414 ") {
		set2d(aa, one,unit);
		set2d(bb, any,frac);
		set2d(cc, any,frac);
	} else if (name == "s415 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
		a[LEN-1] = -one;
	} else if (name == "s421 ") {
		set1d(a, any,frac2);
	} else if (name == "s422 ") {
		set1d(array,one,unit);
		set1d(a, any,frac2);
	} else if (name == "s423 ") {
		set1d(array,zero,unit);
		set1d(a, any,frac2);
	} else if (name == "s424 ") {
		set1d(array,one,unit);
		set1d(a, any,frac2);
	} else if (name == "s431 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s432 ") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "s441 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
		set1ds(LEN/3,	&d[0],		-one,unit);
		set1ds(LEN/3,	&d[LEN/3],	zero,unit);
		set1ds(LEN/3+1, &d[(2*LEN/3)], one,unit);
	} else if (name == "s442 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s443 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s451 ") {
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s452 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c,small,unit);
	} else if (name == "s453 ") {
		set1d(a,zero,unit);
		set1d(b, any,frac2);
	} else if (name == "s471 ") {
		set1d(a, one,unit);
		set1d(b, one,unit);
		set1d(c, one,unit);
		set1d(d, any,frac);
		set1d(e, any,frac);
	} else if (name == "s481 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
		set1d(d, any,frac);
	} else if (name == "s482 ") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "s491 ") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
	} else if (name == "s4112") {
		set1d(a, one,unit);
		set1d(b, any,frac);
	} else if (name == "s4113") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, any,frac2);
	} else if (name == "s4114") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
	} else if (name == "s4115") {
		set1d(a, any,frac);
		set1d(b, any,frac);
	} else if (name == "s4116") {
		set1d(a, any,frac);
		set2d(aa, any,frac);
	} else if (name == "s4117") {
		set1d(a,zero,unit);
		set1d(b, one,unit);
		set1d(c, any,frac);
		set1d(d, any,frac);
	} else if (name == "s4121") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "va	") {
		set1d(a,zero,unit);
		set1d(b, any,frac2);
	} else if (name == "vag  ") {
		set1d(a,zero,unit);
		set1d(b, any,frac2);
	} else if (name == "vas  ") {
		set1d(a,zero,unit);
		set1d(b, any,frac2);
	} else if (name == "vif  ") {
		set1d(a,zero,unit);
		set1d(b, any,frac2);
	} else if (name == "vpv  ") {
		set1d(a,zero,unit);
		set1d(b, any,frac2);
	} else if (name == "vtv  ") {
		set1d(a, one,unit);
		set1d(b, one,unit);
	} else if (name == "vpvtv") {
		set1d(a, one,unit);
		set1d(b, any,frac);
		set1d(c, any,frac);
	} else if (name == "vpvts") {
		set1d(a, one,unit);
		set1d(b, any,frac2);
	} else if (name == "vpvpv") {
		set1d(a, any,frac2);
		set1d(b, one,unit);
		set1d(c,-one,unit);
	} else if (name == "vtvtv") {
		set1d(a, one,unit);
		set1d(b, two,unit);
		set1d(c,half,unit);
	} else if (name == "vsumr") {
		set1d(a, any,frac);
	} else if (name == "vdotr") {
		set1d(a, any,frac);
		set1d(b, any,frac);
	} else if (name == "vbor ") {
		set1d(a, any,frac);
		set1d(b, any,frac);
		set1d(c, one,frac);
		set1d(d, two,frac);
		set1d(e,half,frac);
		set2d(aa, any,frac);
	} else {
	}

	return 0;
}

void set(int* ip, float* s1, float* s2){
	xx = (float*) malloc(LEN*sizeof(float));

	for (int i = 0; i < LEN; i = i+5){
		ip[i]	= (i+4);
		ip[i+1] = (i+2);
		ip[i+2] = (i);
		ip[i+3] = (i+3);
		ip[i+4] = (i+1);

	}
	
	set1d(a, 1.,1);
	set1d(b, 1.,1);
	set1d(c, 1.,1);
	set1d(d, 1.,1);
	set1d(e, 1.,1);
	set2d(aa, 0.,-1);
	set2d(bb, 0.,-1);
	set2d(cc, 0.,-1);

	for (int i = 0; i < LEN; i++){
		indx[i] = (i+1) % 4+1;
	}
	*s1 = 1.0;
	*s2 = 2.0;
}

int s000()
{

//	linear dependence testing
//	no dependence - vectorizable

	for (int i = 0; i < LEN; i++) {
		a[i] = b[i] * c[i] + d[i] * e[i];
	}
	return 0;
}


int s112()
{
//	linear dependence testing
//	loop reversal
	for (int i = LEN - 2; i >= 0; i--) {
		a[i+1] = a[i] + b[i];
	}
	return 0;
}

int s1113()
{
	for (int i = 0; i < LEN; i++) {
		a[i] = a[LEN/2] + b[i];
	}
	return 0;
}

int s122(int n1, int n3)
{

//	induction variable recognition
//	variable lower and upper bound, and stride
//	reverse data access and jump in data access

	int j, k;
	j = 1;
	k = 0;
	for (int i = n1-1; i < LEN; i += n3) {
		k += j;
		a[i] += b[LEN - k];
	}
	
	return 0;
}

int main()
{
	markmemory();
	int n1 = 1;
	int n3 = 1;
	int* ip = (int *) malloc(LEN*sizeof(float));
	float s1,s2;
	// ---Media bench begin
	int len = LEN;
	static short A[LEN];
	_tracer_array_memory(&A[0],sizeof(float)*LEN);
	static char B[LEN];
	_tracer_array_memory(&B[0],sizeof(float)*LEN);
	// ---Media Bench end
	set(ip, &s1, &s2);
	
	printf("Loop\n");
	init( "s111 ");
	printf("S000\n");
	s000();
	init( "s112 ");
	printf("S112\n");
	s112();
	init( "s113 ");
	printf("s1113\n");
	s1113();
	init( "s122 ");
	printf("s122\n");
	s122(n1,n3);

	return 0;
}

