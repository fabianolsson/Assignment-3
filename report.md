#Assignment 3 hpcgroup058
Report divided into 3 parts:

**Data consumption**: explaining how we limit our total data size to 1024^3 bytes of data at any given time.

**Cell assumption**: explaining how we handle the fact of making no assumptions about the nr of cells.

**Program description**: description of implementation of program in an overarching way. 

##Data consumption
We kept most of our data on stack, with a total stack size of 8192000 bytes the 1024^3 limit is not exceeded as this would result in a stack overflow otherwise. Our biggest stack objects are three `float` arrays holding the values for the coordinates of the points/cells, each taking a maximum of 400000 bytes for the largest input file (cells_e5) resulting in a total of 1200000 bytes. There is also a stack allocated `int` array holding the count for the ~3500 differences (slightly fewer in actuality) of 14000 bytes, giving a stack+heap total of 1214000 bytes. The rest are bytes from individual variables not taking up any significant space.

Apart from that we allocate one array to hold input data. This array allocates 48000 bytes on the heap as given below:


~~~ c
#define NUMLENGTH 8
#define LINELENGTH 3*NUMLENGTH
#define LINES 1000
.
.
.
//In main

char *data = malloc(sizeof(char) * 2 * LINELENGTH * LINES);
~~~


In summation the program does not exceed the size limit.

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

nPoints = lSize/24;
~~~


##Program description
A description of the code in broad terms follows, with some core/critical code snippets included for clarification.

**Definitions and variable/pointer declarations**
At the start of the code some core constants are defined, their definitions were seen under the **data size** header above. As we read the data as `char` their lengths are defined by nr of bytes. `LINES` is set to 1000, limiting the number of lines/cells/points read in to 1000 at a time.

Next, variables are declared and memory allocated to hold the indata as seen under **data size**. Note that 2000 bytes are allocated, and not the 1000 required to read one data chunk. This is helpful in reading the data later. The allocated memory is divided between two pointers, one pointing at the start of the first 1000 bytes and the other at the start of the next 1000 bytes. A temporary pointer is also declared to allow for switching between which pointer points at which set of bytes:



~~~ c
char * data = malloc (sizeof(char) * 2 * LINELENGTH * LINES)
char * bfr1 = data;
char * bfr2 = data + LINELENGTH * LINES;
char * temp;
~~~


**Variables for storing coordinates and differences**
Next the currently used `cells` file is checked and total nr of points/cells/lines is found as described in **Cell assumption**. This is followed by the declaration of the arrays holding the x,y,z coordinates of the points and the variable holding the nr of differences:



~~~ c
float x[nPoints], y[nPoints], z[nPoints];
int nOfDiffs[3500] = {0};
~~~


As we could assume the limit of the coordinates being -10 to 10, the maximum distance between any two points is ~34.64. With a precision of 0.01 this gives a total nr of possible distances as ~3464. This can be utilized to store each distance multiplied by 100 at the corresponding index in `nOfDiffs`, avoiding having to search in and sort arrays.

**Reading and parsing data**
Starting at the beginning of the infile the data is read in chunks of 1000 lines/points at a time and stored in `bfr1`. The pointers are then switched so that `bfr2` points at the read in data while `bfr1` points at the empty next chunk of 1000 bytes:



~~~ c
ret = fread(bfr1, LINELENGTH, LINES, infile);
for (size_t cx = 0; ret == LINES; ++cx) {

temp = bfr1;
bfr1 = bfr2;
bfr2 = temp;
~~~


The data is then parsed, using pointer arithmetics to find the start of the x, y and z coordinates and storing them as floats in the above declared arrays `x[] y[] z[]` with `strtof`.



~~~ C
for (size_t px = cx*LINES; px < (cx+1)*LINES; ++px) {
    x[px] = strtof(bfr2 + LINELENGTH*(px-cx*LINES), NULL);
    y[px] = strtof(bfr2 + LINELENGTH*(px-cx*LINES) + NUMLENGTH, NULL);
    z[px] = strtof(bfr2 + LINELENGTH*(px-cx*LINES) + 2*NUMLENGTH, NULL);
    }
~~~


Then the next 1000 lines are read into `bfr1` and the above is repeated until the condition `ret == LINES` is no longer met and the file has been read and all coordinates stored in the x,y,z arrays.

####Parallelisation of reading and parsing
Fabian! Can't write in swedish in markdown apparently... Don't know if we should say something about the parallelization here? Since it didn't add much to the speed and since `#pragma omp single` only run one thread I don't know how much parallelization we actually get when using that. It seems to be more of a thing to make certain parts of an otherwise `#pragma omp parallel` code block run sequentially. But we are using it for the whole block that also runs `#pragma omp parallel`, which I think means we don't parallelize it at all in reality.    

**Calculating, counting and storing differences**
The differences are calculated using double for loops, parallelized with `schedule(guided)`. Difference of each coordinate is calculated and then combined into distance between the points. The index of the difference is calculated as mentioned earlier, multiplying the difference by 100 and adding 0.5 to round upwards as it is stored in an `int`. A reduction is applied to `nrOfDiffs[iDiff]++` to count the number of occurences of the differences, each thread calculating their own occurences and adding it all together in the end:



~~~ c
#pragma omp parallel for schedule(guided) reduction(+:nOfDiffs)
	for (size_t px1 = 0; px1 < nPoints; ++px1){
	    for (size_t px2 = 0; px2 < px1; ++px2){

	    	float xDiff = x[px1] - x[px2];
		float yDiff = y[px1] - y[px2];
		float zDiff = z[px1] - z[px2];

		float diff = sqrtf ( xDiff*xDiff + yDiff*yDiff + zDiff*zDiff );
		int iDiff = (int)(diff*100 + .5);
		.
		.
		.
		nrOfDiffs[iDiff]++;
	    }
	}	   
~~~

