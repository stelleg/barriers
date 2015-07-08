all: count barrier omp

omp: omp.c
	gcc omp.c -g -Wall -O2 -fopenmp -o omp

count: count.c
	gcc count.c -g -Wall -O2 -fopenmp -o count

barrier: barrier.c
	gcc barrier.c -g -Wall -O2 -fopenmp -o barrier 
