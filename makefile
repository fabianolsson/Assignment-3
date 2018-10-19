CC = gcc
CFLAGS = -lm -O2 -march=native -fopenmp -lgomp

.PHONY : clean

all : cell_distance cell_dist_perfectionist

cell_distance : 
	$(CC) -o $@ $(CFLAGS) cell_distance.c

cell_dist_perfectionist : 
	$(CC) -o $@ $(CFLAGS) cell_dist_perfectionist.c
clean :
	rm -f cell_distance
	rm -f cell_dist_perfectionist
