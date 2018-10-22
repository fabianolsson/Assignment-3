#include <omp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
//#define DEBUG
//#define TIMECHECK
#define INFILE "cells"
#define NUMLENGTH  8
#define LINELENGTH 3*NUMLENGTH //Not sure
#define LINES 1000


int main(int argc, char *argv[])
{
	FILE * infile;
	char * data = malloc (sizeof(char) * 2 * LINELENGTH * LINES);
	char * bfr1 = data;
	char * bfr2 = data + LINELENGTH * LINES;
	char * temp;
	long int lSize;
	size_t nPoints/*, nChunks*/;
	size_t readWait = 0, parseWait = 0;
	struct timespec tSleep={.tv_sec=0,.tv_nsec=50000};
#ifdef TIMECHECK
	struct timespec start, stop;
	long int tDiff;
#endif
	//omp_set_num_threads(data from args);

	size_t ret;
	infile = fopen (INFILE, "r");

#ifdef TIMECHECK
	timespec_get(&start, TIME_UTC);
#endif
	//Check file size, stolen from cplusplus.com
	fseek (infile, 0, SEEK_END);
	lSize = ftell (infile);
	rewind(infile);

	nPoints = lSize/24;	//There might be fewer points than this
//	nChunks = (nPoints + LINELENGTH*LINES - 1)/(LINELENGTH*LINES);	//TODO: Orphaned
//	float x[nPoints], y[nPoints], z[nPoints];
	float * x = (float*) malloc ( nPoints*sizeof(float) );
	float * y = (float*) malloc ( nPoints*sizeof(float) );
	float * z = (float*) malloc ( nPoints*sizeof(float) );
	int   nOfDiffs[3500] = {0};
	size_t nThreads = 1;
	for ( size_t ix = 1; ix < argc; ++ix ) 
		if ( argv[ix][0] == '-' && argv[ix][1] == 't' ) {
			nThreads = strtol(&argv[ix][2], NULL, 0);
#ifdef DEBUG
			printf("Number of threads: %d\n", nThreads);
#endif
		}
	//file reader. Meant to let parser handle first buffer while it
	//reads the second, and so on
omp_set_num_threads(nThreads);
#pragma omp parallel
	{
#pragma omp single
		{
			ret = fread (bfr1, LINELENGTH, LINES, infile);

			size_t cx;
			for ( cx=0; ret == LINES; ++cx ) {

				//Give parser new data
				temp = bfr1;
				bfr1 = bfr2;
				bfr2 = temp;

				//Let parser start
#pragma omp task			//parser
				{
					for ( size_t px=cx*LINES; px < (cx+1)*LINES; ++px ) {
						x[px] = strtof (bfr2 + LINELENGTH*(px-cx*LINES), NULL);	//TODO: Replace with own version
						y[px] = strtof (bfr2 + LINELENGTH*(px-cx*LINES) + NUMLENGTH, NULL);
						z[px] = strtof (bfr2 + LINELENGTH*(px-cx*LINES) + 2*NUMLENGTH, NULL);
					}

				}
				ret = fread (bfr1, LINELENGTH, LINES, infile);
#pragma omp taskwait
			}
#ifdef DEBUG
			printf ("cx is %d, nPoints is %d\n", cx, nPoints);
#endif
			bfr2=bfr1;
			for ( size_t px=cx*LINES; px < nPoints; ++px ) {
				x[px] = strtof (bfr2 + LINELENGTH*(px - cx*LINES), NULL);
				y[px] = strtof (bfr2 + LINELENGTH*(px - cx*LINES) + NUMLENGTH, NULL);
				z[px] = strtof (bfr2 + LINELENGTH*(px - cx*LINES) + 2*NUMLENGTH, NULL);
			}
			fclose(infile);
		
	

#ifdef TIMECHECK
		
			timespec_get (&stop,  TIME_UTC);
			tDiff = 1000000000*(stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec);
			printf ( "Time to read and parse: %d.%.9d\n", tDiff/1000000000, tDiff%1000000000 );
#endif
		}
		//handler
#pragma omp for schedule(guided) reduction(+:nOfDiffs)
		for ( size_t px1=0; px1 < nPoints; ++px1 ) {
			for ( size_t px2=0; px2 < px1; ++px2 ) {
				float xDiff = x[px1]-x[px2];		//TODO: Try to vectorize?
				float yDiff = y[px1]-y[px2];
				float zDiff = z[px1]-z[px2];
				float diff  = sqrtf ( xDiff*xDiff + yDiff*yDiff + zDiff*zDiff );
				int iDiff = (int)(diff*100 + .5);
#ifdef DEBUG
				if ( iDiff >= 3500 ) {
					printf ( "Distance too large: %d\n", iDiff );
					exit(1);
				}
#endif
				nOfDiffs[iDiff]++;
			}
		}
	}
	
	size_t total=0;
	for ( size_t ix=0; ix < 3500; ++ix ) {
		if ( nOfDiffs[ix] != 0 ) {
			total+=nOfDiffs[ix];
#ifdef DEBUG
			printf ( "Distance %d.%.2d occurs %d times\n", ix/100, ix%100, nOfDiffs[ix] );
#endif
			printf ( "%.2d.%.2d %d\n", ix/100, ix%100, nOfDiffs[ix] );
		}
	}
#ifdef DEBUG
	printf ( "Total %ld pairs of points\n", total );
#endif

	free(data);
	free(x);
	free(y);
	free(z);
	return 0;
}
