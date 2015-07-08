#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv){
  if(argc != 2 ) {
    printf("usage: ./ompbarrier <ncores>\n");
    exit(-1);
  }

  long long iters = atol(argv[1]);
  #pragma omp parallel
  {
    for(int i=0; i<iters; i++){
      #pragma omp barrier
    }
  }
}
