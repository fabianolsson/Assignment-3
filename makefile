CC = gcc
CFLAGS = -lm -O2 -march=native -fopenmp -lgomp

.PHONY : clean all

all : cell_distance

cell_distance : 
	$(CC) -o $@ $(CFLAGS) cell_distance.c

clean :
	rm -f cell_distance
