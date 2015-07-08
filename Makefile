cc=gcc
cflags=-g -Wall -O2 -fopenmp -std=c99
bins=count barrier omp

all: ${bins} 

%:%.c
	${cc} ${cflags} -o $@ $<

clean:
	rm -f ${bins}
