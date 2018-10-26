#Assignment 3 hpcgroup058
Report divided into 3 parts:

**Data consumption**: explaining how we limit our total data size to 1024^3 bytes of data at any given time.

**Cell assumption**: explaining how we handle the fact of making no assumptions about the nr of cells.

**Program description**: description of implementation of program in an overarching way. 

##Data consumption
We have 7 major data consuming allocations in our program. Firstly a malloc array `data`, allocating space for two times the number of lines we read in at a time from file. It will store the indata as `char`, and with each line/point/cell being 24 bytes longand our choice to read in 1000 lines at a time, it will allocate 2*24*1000 = 48000 bytes.

Next we have two sets of three mallocs. Each set representing the three coordinates of the points. The reason for having two sets of these are discussed later. Each coordinate stores the values as `float`, maximally allocating a `BLOCK` of floats, where `BLOCKSIZE` is set to 40000. As float takes up 4 bytes, each coordinate array will allocate 40000*4 bytes, with the 6 such arrays then allocating 40000*4*6 = 960000 bytes. 

Further we have a smaller array `nrOfDiffs` holding the number of differences which allocates space for 3500 ints of 4 bytes, giving 3500*4 = 14000 bytes. The rest of the program memory is taken up by individual variables not contributing any significant memory usage. In total then we have approximatley:

48000 + 14000 + 960000 = 1022000 bytes, giving a large margin to the upper limit. 

###Valgrind report
Valgrind reported some memory leaks in the program. The memory allocation associated with the leaks were reported to start at row 58 where a `#pragma omp parallel` is declared. This however seems to be a common report when using openMP and Valgrind and is not a real memory leak per se, as the bytes are still reachable. A description can be found [here](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=36298). 

##Cell assumption
To avoid making any assumptions on the number of cells we use `fseek` to go to the end of the cell_eX file used. Then use `ftell` to return the number of bytes at end of file and divide by the number of bytes of a cell (24 bytes as we read chars). This will give us the number of lines/cells/points in the file. We then return to start of file to read it. Code for above, types omitted:



~~~ c
#define INFILE "cells" //Naming file in current directory "cells" and not cell_e5, etc.

//In main

infile = fopen(INFILE, "r")
fseek(infile, 0, SEEK_END);
lSize = ftell(infile);
rewind(infile);

np_total = lSize/24;
~~~

This is not sufficient however, as trying to read and compare all data at once would be unfeasible when reaching the upper assumption limit 2^32 on number of cells. To solve this we divide the data up into blocks of size `BLOCK` as mentioned earlier. This is set to 40000. We then read in data and store chunks of 40000 points in the allocated coordinate variables `x_1, y_1, z_1` which we then compare to the points inside the block itself and then to the points in another block of 40000 points, which are stored in the second set of coordinate variables `x_2, y_2, z_2`. Then we iterate, holding the first block static while changing the comparison block to the next 40000 points, etc, until it has run through the whole data. Then we iterate the first block and repeat.
In this way we're only holding 80000 points at any time, not exceeding the memory limit while still not making assumptions on the number of points.


##Program description
A description of the code in broad terms follows, with some core/critical code snippets included for clarification.

**Definitions and variable/pointer declarations**
At the start of the code some core constants are defined, their definitions were seen under the **data size** header above. As we read the data as `char` their lengths are defined by nr of bytes. `LINES` is set to 1000, limiting the number of lines/cells/points read in to 1000 at a time. We define a `BLOCK` to be 40000

Next, variables are declared and memory allocated to hold the indata as seen under **data size**. Note that 2000 bytes are allocated, and not the 1000 required to read one data chunk. This is helpful in reading the data later. The allocated memory is divided between two pointers, one pointing at the start of the first 1000 bytes and the other at the start of the next 1000 bytes. A temporary pointer is also declared to allow for switching between which pointer points at which set of bytes:



~~~ c
char * data = malloc (sizeof(char) * 2 * LINELENGTH * LINES)
char * bfr1 = data;
char * bfr2 = data + LINELENGTH * LINES;
char * temp;
~~~


**Variables for storing coordinates and differences**
Next the currently used `cells` file is checked and total nr of points/cells/lines is found as described in **Cell assumption**. This is followed by the declaration of the array holding the nr of differences:



~~~ c
int nOfDiffs[3500] = {0};
~~~


As we could assume the limit of the coordinates being -10 to 10, the maximum distance between any two points is ~34.64. With a precision of 0.01 this gives a total nr of possible distances as ~3464. This can be utilized to store each distance multiplied by 100 at the corresponding index in `nOfDiffs`, avoiding having to search in and sort arrays.

**Reading and parsing data**
We now initiate a large for loop. As described in **Cell_assumption**, the loop will run through the total nr of points, incrementing with `BLOCK` each iteration. This outer loop will control the number of points in the first block, which will be the block held steady while we iterate through and block up the rest of the data for comparison with this block. In the case that we have reached the end of the total data and there is not `BLOCK` data left to be allocated for, we allocate for whatever data is left:

~~~c
for(size_t i = 0; i < np_total; i += BLOCK){
	size_t np_local1;
	float *x_1, *y_1, *z_1;

	if(i+BLOCK > np_total)
		np_local_1 = np_total-i;
	else
		np_local_1 = BLOCK;

	x_1 = malloc(np_local_1 * sizeof(float));
	y_1 = malloc(np_local_1 * sizeof(float));
	z_1 = malloc(np_local_1 * sizeof(float));
.
.
.
~~~ 

The above loop envelops all that follows beneath.

We seek to the end of the current block in the infile and the data is read in chunks of 1000 lines/points at a time and stored in `bfr1`. The pointers are then switched so that `bfr2` points at the read in data while `bfr1` points at the empty next chunk of 1000 bytes:


~~~ c
fseek(infile, i*LINELENGTH, SEEK_SET);
ret = fread(bfr1, LINELENGTH, LINES, infile);
for (size_t cx = 0; (cx+1)*LINES < np_local_1; ++cx) {

temp = bfr1;
bfr1 = bfr2;
bfr2 = temp;
~~~


The data is then parsed, using pointer arithmetics to find the start of the x, y and z coordinates and storing them as floats in the above declared arrays `x[] y[] z[]` with `strtof`.



~~~ C
for (size_t px = cx*LINES; px < (cx+1)*LINES; ++px) {
    x_1[px] = strtof(bfr2 + LINELENGTH*(px-cx*LINES), NULL);
    y_1[px] = strtof(bfr2 + LINELENGTH*(px-cx*LINES) + NUMLENGTH, NULL);
    z_1[px] = strtof(bfr2 + LINELENGTH*(px-cx*LINES) + 2*NUMLENGTH, NULL);
    }
~~~


Then the next 1000 lines are read into `bfr1` and the above is repeated until the we have read in the whole block.
    

**Calculating, counting and storing differences**
The differences are calculated using double for loops, parallelized with `schedule(guided)`. Difference of each coordinate is calculated and then combined into distance between the points. The index of the difference is calculated as mentioned earlier, multiplying the difference by 100 and adding 0.5 to round upwards as it is stored in an `int`. A reduction is applied to `nrOfDiffs[iDiff]++` to count the number of occurences of the differences, each thread calculating their own occurences and adding it all together in the end. At this first encounter we compare the points in the current block with the other points in the same block, so the for loop started higher up is still running:



~~~ c
#pragma omp parallel for schedule(guided) reduction(+:nOfDiffs)
	for (size_t px1 = 0; px1 < np_local_1; ++px1){
	    for (size_t px2 = 0; px2 < px1; ++px2){

	    	float xDiff = x_1[px1] - x_1[px2];
		float yDiff = y_1[px1] - y_1[px2];
		float zDiff = z_1[px1] - z_1[px2];

		float diff = sqrtf ( xDiff*xDiff + yDiff*yDiff + zDiff*zDiff );
		int iDiff = (int)(diff*100 + .5);
		.
		.
		.
		nrOfDiffs[iDiff]++;
	    }
	}	   
~~~

When the block has been compared to itself, we start the inner for loop comparing it to other blocks. It looks very much as a copy of the above except for a few differences, most notably we declare the second set of coordinate arrays `x_2, y_2, z_2` and use these while calculating the differences. Note also that we compare from behind, letting the first loop go forward through the data one block at a time and then letting this inner loop iterate through the blocks that has already been held by the outer loop. In this way we never have to consider this inner loop running out of points to put in a block at the end, as it will always end one block behind the outer loop:

~~~c
for(size_t j = 0; j < i; j += BLOCK){
	size_t np_local_2;
	float *x_2, *y_2, *z_2;

	np_local_2 = BLOCK;

	x_2 = malloc(np_local_2 * sizeof(float));
	y_2 = malloc(np_local_2 * sizeof(float));
	z_2 = malloc(np_local_2 * sizeof(float));

	//Parsing of data looks the same except storing in these second coordinate variables
	.
	.
	.
#pragma omp parallel for schedule(guided) reduction(+:nrOfDiffs)
	for(size_t px1 = 0; px1 < np_local_1; ++px1){
		for(size_t px2 = 0; px2 < np_local_2; ++px2){
			float xDiff = x_1[px1]-x_2[px2];
			float yDiff = y_1[px1]-y_2[px2];
			float zDiff = z_1[px1]-z_2[px2];
			float diff = sqrt(xDiff*xDiff + yDiff*yDiff + zDiff*zDiff);
			int iDiff = (int)(diff*100 + .5);

			nrOfDiffs[iDiff]++;
		}
	}
	free(x_2);
	free(y_2);
	free(z_2);
}
free(x_1);
free(y_1);
free(z_1);

// Here is end of outer loop started further above.
~~~

