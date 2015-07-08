cc=gcc
cflags=-g -Wall -O2 -fopenmp 
bins=count barrier omp

all: ${bins} 

%:%.c
	${cc} ${cflags} -o $@ $<

clean:
	rm -f ${bins}
