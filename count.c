#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <omp.h>

static inline void cpu_pause(){
  __asm__ ("pause");
}

typedef struct corebarrier_s{
  volatile uint64_t arrive;
  volatile uint64_t depart;
  uint64_t padding[6];
} corebarrier_t;

typedef struct threadbarrier_s{
  int8_t incr; 
  int32_t threadspercore;
  corebarrier_t* mycore;
} threadbarrier_t;

typedef struct barrier_s{
  corebarrier_t* cbs;
  threadbarrier_t* tbs;
} barrier_t;

void countBarrier(threadbarrier_t* tb){
  uint64_t target = tb->incr == 1 ? tb->threadspercore : 0;
  __sync_fetch_and_add(&tb->mycore->arrive, tb->incr);
  while(tb->mycore->arrive != target) cpu_pause();
  __sync_fetch_and_add(&tb->mycore->depart, tb->incr);
  while(tb->mycore->depart != target) cpu_pause();
  tb->incr = -1 * tb->incr;
}

barrier_t allocBarrier(uint64_t nthreads, uint64_t ncores){
  threadbarrier_t* tbs = malloc(nthreads * sizeof(threadbarrier_t));
  corebarrier_t* cbs = malloc(ncores * sizeof(corebarrier_t));
  return (barrier_t){cbs, tbs};
}

void initBarrier(corebarrier_t* cbs, threadbarrier_t* tbs, uint64_t cid, uint64_t tid, uint32_t threadspercore){
  threadbarrier_t mytb = (threadbarrier_t){1, threadspercore, cbs + cid};
  cbs[cid].arrive = 0;
  cbs[cid].depart = 0;
  tbs[tid] = mytb;
}

int main(int argc, char* argv[]){
  if(argc != 3) {
    printf("usage: ./barrier <ncores> <niters>\n");
    exit(-1);
  }
  uint64_t ncores = atol(argv[1]);
  uint64_t iters = atol(argv[2]);
  barrier_t b;
  #pragma omp parallel
  {
    int nthreads = omp_get_num_threads();
    assert(nthreads % ncores == 0);
    int tid = omp_get_thread_num();
    int threadspercore = nthreads/ncores;
    int cid = tid/threadspercore;
    if(tid==0) b = allocBarrier(nthreads, ncores);
    #pragma omp barrier
    initBarrier(b.cbs, b.tbs, cid, tid, threadspercore);  
    #pragma omp barrier
    for(int i=0; i<iters; i++)
      countBarrier(b.tbs + tid);
  }
  free(b.tbs);
  free(b.cbs);
} 
