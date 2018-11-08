
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "threads.h"
#include "ptr_mangle.h"
#include "semaphore.h"


#define THREAD_CNT 10

static long val = 0;

void *count(void *arg){

long i;
 printf("I, %x, am trying to obtain the lock\n", (unsigned int)pthread_self());
 sem_wait(arg);
  printf("I, %x got the lock!\n", (unsigned int)pthread_self());
  for(i = 0; i < 100000000; i++){
    val++;
  }

 printf("I, %x, have finally incremented val to %ld\n", (unsigned int)pthread_self(), val);
 sem_post(arg);
 printf("I, %x, have returned the lock\n", (unsigned int)pthread_self());
 
  return arg;
}

int main(int argc, char **argv){
	pthread_t threads[THREAD_CNT];

  sem_t newSem;
  sem_init(&newSem, 0, 1);

    int i;

    //create THREAD_CNT threads
    for(i = 0; i<THREAD_CNT; i++) 
    {
        pthread_create(&threads[i], NULL, count, &(newSem));
    }

     for(i = 0; i<THREAD_CNT; i++) {
        printf("Join %d returns %d\n", i, pthread_join(threads[i], NULL));
    }


    return 0;
}
