#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#ifndef __rdtsc
static inline uint64_t __rdtsc(){
  uint64_t rdtsc;
  __asm__ volatile ("rdtsc" : "=A" (rdtsc));
  return rdtsc;
}
#endif

static inline void cpu_pause(){
  __asm__ ("pause");
}

typedef struct corebarrier_t{
  volatile uint64_t userbarrier_arrive;
  volatile uint64_t userbarrier_depart;
  uint64_t padding[6];  // pad things out to fill the cacheline.
} corebarrier_t;

typedef struct threadbarrier_t{
  uint8_t usersense;
  uint16_t mycoretid;
  uint64_t coreval;
  corebarrier_t* me;
} threadbarrier_t;

typedef struct barrier_t{
  threadbarrier_t* tbs;
  corebarrier_t* cbs;
  volatile uint8_t* countbs;
} barrier_t;

void countBarrier(volatile uint8_t* bar, size_t ntpc, int lid, int incr){
  uint8_t target;
  if(incr == 1) target = ntpc;
  else target = 0;
  __sync_fetch_and_add(bar, incr);
  while(*bar != target) cpu_pause();
  __sync_fetch_and_add(bar+8, incr);
  while(*(bar+8) != target) cpu_pause();
}

// local barrier bar is a thread-local variable.
void userCoreBarrier(threadbarrier_t* bar){
  int mysense = bar->usersense;
  int coretid = bar->mycoretid;
  int mycoresense = mysense ? bar->coreval : 0;
  corebarrier_t *me = bar->me;

  // signal my arrival
  ((char *)&me->userbarrier_arrive)[coretid] = mysense;
  // wait for others to arrive
  while (me->userbarrier_arrive != mycoresense)
    cpu_pause();

  // signal my departure
  ((char *)&me->userbarrier_depart)[coretid] = mysense;
  // wait for others to depart
  while (me->userbarrier_depart != mycoresense)
    cpu_pause();

  bar->usersense = 1 - mysense;
}

barrier_t init_barrier(size_t ncores){
  threadbarrier_t* barriers;
  corebarrier_t* corebarriers;
  volatile uint8_t* countbarriers;
  #pragma omp parallel
  {
    int nthreads = omp_get_num_threads();
    int tperc = nthreads / ncores;
    #pragma omp master
    {
      printf("nthreads = %d, ncores = %ld\n", nthreads, ncores);
      assert(nthreads % ncores == 0);
      assert(tperc <= 8);
      barriers = (threadbarrier_t*) aligned_alloc(nthreads * sizeof(threadbarrier_t), 64);
      corebarriers = (corebarrier_t*) aligned_alloc(ncores * sizeof(corebarrier_t), 64);
      countbarriers = (volatile uint8_t*) aligned_alloc(ncores * 64, 64);
    }
    #pragma omp barrier

    // Calculate global and local thread id.
    // Note: Assumption here is that you want 4 threads per core...
    int gid = omp_get_thread_num();
    int lid = gid % tperc;
    int cid = gid / tperc;
         
    // countbarrier init
    countbarriers[cid * 64] = 0;

    // Setup the core's barrier variable.
    corebarrier_t* me = corebarriers + cid;
    me->userbarrier_arrive = 0;
    me->userbarrier_depart = 0;
         
    // Setup the thread's local barrier variable.
    threadbarrier_t bar;
    bar.usersense = 1;
    bar.mycoretid = lid;
    bar.coreval = 0;
    for(int i=0; i<tperc; i++)
      bar.coreval |= 0x1 << 8*i;
    bar.me = me;
    barriers[gid] = bar;
  #pragma omp barrier
  }
  return (barrier_t){barriers, corebarriers, countbarriers};
}

/**
 * Local barrier test harness.
 */
int main(int argc, char* argv[])
{
  if(argc < 2) {
    printf("usage: ./barrier <ncores>\n");
    exit(-1);
  }
  static uint64_t iterations = 1000000;
  int ncores = atoi(argv[1]);
  barrier_t barrier = init_barrier(ncores);
  threadbarrier_t* barriers = barrier.tbs;
  volatile uint8_t* countbarriers = barrier.countbs;
  
  #pragma omp parallel
  {
    // Test 1: OpenMP Barrier.
    // Note: This is really a "best-case" with no jitter, so incredibly misleading.
    {
      uint64_t t0 = __rdtsc();
      for (int r = 0; r < iterations; r++){
        #pragma omp barrier
      }
      uint64_t t1 = __rdtsc();
      #pragma omp master
      {
        printf("OpenMP Barrier: %ld cycles\n", (t1-t0) / iterations);
      }
    }
    #pragma omp barrier
         
    // Test 2: Local Barrier.
    // Note: Same best-case assumption applies.
    {
      int gid = omp_get_thread_num();
      uint64_t t0 = __rdtsc();
      for (int r = 0; r < iterations; r++){
        userCoreBarrier(barriers + gid);
      }
      uint64_t t1 = __rdtsc();
      #pragma omp master
      {
        printf("Local Barrier: %ld cycles\n", (t1-t0) / iterations);
      }
    }
    
    // Test 3: Count Barrier.
    // Note: Same best-case assumption applies.
    {
      int gid = omp_get_thread_num();
      int ntpc = omp_get_num_threads() / ncores;
      int cid = gid / ntpc;
      int lid = gid % ntpc;
      int incr = 1;
      countbarriers[cid * 64] = 0;
      countbarriers[cid * 64 + 8] = 0;
#pragma omp barrier
      //printf("%d's  countbarrier = %d\n", gid, countbarriers[cid * 64]);
      uint64_t t0 = __rdtsc();
      for (int r = 0; r < iterations; r++){
        //printf("calling countbarrier with cid = %d, ntpc = %d, lid = %d\n", cid, ntpc, lid);
        countBarrier(countbarriers + (64*cid), ntpc, lid, incr);
        incr *= -1;
      }
      uint64_t t1 = __rdtsc();
      #pragma omp master
      {
        printf("Count Barrier: %ld cycles\n", (t1-t0) / iterations);
      }
    }
    #pragma omp barrier
  }
  free(barrier.tbs);
  free(barrier.cbs);
}




