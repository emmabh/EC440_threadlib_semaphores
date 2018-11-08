
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

//TCB Values
#define MAX_THREADS 128
#define STACK_SIZE 32767

//Frequency of scheduler
#define FREQ_MS 50

//Registers
#define JB_BX 0
#define JB_SI 1
#define JB_DI 2
#define JB_BP 3
#define JB_SP 4
#define JB_PC 5

//Thread states
#define PREPARING 0
#define READY 1
#define EXITED 2
#define UNUSED 3
#define FIRST_READY 4
#define BLOCKED 5


struct threadNode{
	pthread_t id;
	struct threadNode *next;

};

struct threadQueue{
	struct threadNode *head;
	struct threadNode *tail;
	int length;

};


//Thread struct
struct thread{
	pthread_t id;
	int status;
	int prevStatus;
	jmp_buf registers;
	unsigned int *stack;
	void* exit_status;
	int start_ptr;
	int stack_ptr;
	pthread_t join_id;

};

struct semaphore{
	int value;
	struct threadQueue *q;
	int initFlag;
	sem_t *id;

};

void enqueue(struct threadQueue* q, pthread_t thread);
pthread_t dequeue(struct threadQueue* q);


//Array of all threads
static struct thread allThreads[MAX_THREADS];

//Array of semaphores
static struct semaphore allSems[MAX_THREADS];

//Array of threads' active values
static int currThreads[MAX_THREADS] = {0};

//Tracking the number of active threads
static int activeThreads = 0;

//Global current thread
static int currThread = 0;

//Return current thread
pthread_t pthread_self(){
	//If at least one active thread
	if(currThread > 0){
		return allThreads[currThread].id;
	}
	//If no active thread
	else{
		return -1;
	}
}

//Create a new thread and register it into the TCB so it'll be scheduled
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg){
	int i;
	int newThread;
	currThreads[0] = 1;

	//Starting from the first thread, check if any slots are open
	for(i = 1; i < MAX_THREADS; i++){
		if(currThreads[i] == 0)
		{
			newThread = i;
			break;
		}
	}

	//If 128 threads already, exit
	if(i == MAX_THREADS){
		printf("ERROR: New thread not created. Too many threads in use\n");
	}

	//If room for more threads, add to TCB
	else{

		//Creating the new thread
		*thread = newThread;
		allThreads[newThread].id = newThread;
		allThreads[newThread].status = PREPARING;
		allThreads[newThread].exit_status = NULL;
		allThreads[newThread].stack = malloc(STACK_SIZE);
		allThreads[newThread].prevStatus = UNUSED;
		allThreads[newThread].join_id = -1;

		//Setting up registers and stack
		//Registers are prepared below, and then the state is saved in the scheduler

		//Arg to the start routine is 2nd from top of stack
		allThreads[newThread].stack[(STACK_SIZE/4)-1] = (unsigned int)arg;

		//Exit is top of stack
		allThreads[newThread].stack[(STACK_SIZE/4)-2] = (unsigned int)pthread_exit_wrapper;

		//mangle stack ptr	
		allThreads[newThread].stack_ptr = ptr_mangle((int)(allThreads[newThread].stack + STACK_SIZE/4 - 2));

		//mangle PC
		allThreads[newThread].start_ptr = ptr_mangle((int)start_routine);

		//Save state of empty buffer
		setjmp(allThreads[newThread].registers);

		//Number of active threads increment
		activeThreads++;

		//Set the slot to full
		currThreads[newThread] = 1;

		//Set state to ready for the first time
		allThreads[newThread].status = FIRST_READY;

		//If first thread created, setup the subsystem
		if(activeThreads == 1){
			initialize_sys();
		}

		//Schedule
		scheduler();
		

	}

	return 0;
}

void scheduler(){
	//Setjmp returns 0 on direct invocation
	if(setjmp(allThreads[currThread].registers) == 0){
		//Free the stack space of any exited threads and set status to unused every time we leave the main thread
		if(currThread == 0){
			int i;
			for(i = 0; i < MAX_THREADS; i++){
				if(allThreads[i].status == EXITED){
					if(allThreads[i].stack != NULL){
						free(allThreads[i].stack);
					}
					allThreads[i].status = UNUSED;
				}
			}
		}


		//Use round robin to find next thread ready to execute
		currThread++;
		currThread%= MAX_THREADS;

		while(allThreads[currThread].status != READY && allThreads[currThread].status != FIRST_READY){
				currThread++;
				currThread %= MAX_THREADS;
		}

		//Setup the stack ptr and program counter if this is the first time the thread is being run
		if(allThreads[currThread].status == FIRST_READY){
			allThreads[currThread].registers[0].__jmpbuf[JB_SP] = allThreads[currThread].stack_ptr;
			allThreads[currThread].registers[0].__jmpbuf[JB_PC] = allThreads[currThread].start_ptr;
			allThreads[currThread].status = READY;
		}

		//Unblock signal
		sigset_t signal_set;
		sigemptyset(&signal_set);
		sigaddset(&signal_set, SIGALRM);
		sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

		//Context switch
		longjmp(allThreads[currThread].registers, 1);


	//If nonzero setjmp, longjmp returning to setjmp above
	}else{
		//Unblock and run thread
		sigset_t signal_set;
		sigemptyset(&signal_set);
		sigaddset(&signal_set, SIGALRM);
		sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

	}
}

void initialize_sys(){
        //Adding TCB for main program
        allThreads[0].id = 0;
        allThreads[0].status = READY;
        allThreads[0].stack = NULL;
        allThreads[0].exit_status = NULL;

        setjmp(allThreads[0].registers);
        activeThreads++;

        //Initializing signal & form connection with scheduler
        struct sigaction sigAct;
        memset(&sigAct, 0, sizeof(sigAct));
        sigAct.sa_sigaction = scheduler;
        sigAct.sa_flags = SA_NODEFER;
        sigaction(SIGALRM, &sigAct, NULL);

        //Start the timer to go off every 50ms
        struct itimerval timer;
        timer.it_value.tv_usec = FREQ_MS*1000;
        timer.it_value.tv_sec = 0;
        timer.it_interval.tv_usec = FREQ_MS*1000;
        timer.it_interval.tv_sec = 0;
        if(setitimer(ITIMER_REAL, &timer, NULL) == -1){
                printf("ERROR: Timer could not be initialized\n");
                exit(-1);
        }

}

void pthread_exit(void *value_ptr){
	//Block signal while thread exiting
	
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGALRM);
	sigprocmask(SIG_BLOCK, &signal_set, NULL);

	currThreads[currThread] = 0;
	allThreads[currThread].status = EXITED;
	allThreads[currThread].exit_status = value_ptr;

	if(allThreads[currThread].join_id != -1 && allThreads[allThreads[currThread].join_id].status == BLOCKED){
		allThreads[allThreads[currThread].join_id].status = allThreads[allThreads[currThread].join_id].prevStatus;
	}


	activeThreads--;

	scheduler();

	//Should never reach this line
	__builtin_unreachable();

}



static int ptr_mangle(int p)
{
    unsigned int ret;
    asm(" movl %1, %%eax;\n"
        " xorl %%gs:0x18, %%eax;" " roll $0x9, %%eax;"
        " movl %%eax, %0;"
        : "=r"(ret)
        : "r"(p)
        : "%eax"
        );
    return ret;
}
/*-------------------- PROJECT 3-------------------------*/
void lock(){
	sigset_t sigset1;
	sigemptyset(&sigset1);
	sigaddset(&sigset1, SIGALRM);
	sigprocmask(SIG_BLOCK, &sigset1, NULL);
}

void unlock(){
	sigset_t sigset1;
	sigemptyset(&sigset1);
	sigaddset(&sigset1, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigset1, NULL);
}

void pthread_exit_wrapper()
{

    unsigned int res;
    asm("movl %%eax, %0\n":"=r"(res)); 
    pthread_exit((void *) res);

}

int pthread_join(pthread_t thread, void **value_ptr){

	if(value_ptr != NULL && allThreads[thread].join_id == -1){
		value_ptr = &allThreads[thread].exit_status;
	}

	//The calling thread must block until the target thread terminates
	if(allThreads[thread].join_id == -1 && allThreads[thread].status != UNUSED && allThreads[thread].status != EXITED){
		allThreads[currThread].prevStatus = allThreads[currThread].status;
		allThreads[currThread].status = BLOCKED;

		//Intialize the join id for the target
		allThreads[thread].join_id = allThreads[currThread].id;

		//Schedule
		scheduler();

		allThreads[thread].join_id = -1;

		return 0;
	}else if(allThreads[thread].join_id == -1){
		return 0;	
	}else{
		return -1;
	}
}


int sem_init(sem_t *sem, int pshared, unsigned value){
	lock();
	int i;
	int newSem;
	for(i = 1; i < MAX_THREADS; i++){
		if(allSems[i].initFlag == 0)
		{
			newSem = i;
			break;
		}
	}

	//If 128 threads already, exit
	if(i == MAX_THREADS){
		printf("ERROR: New semaphore not created. Too many semaphores in use\n");

		unlock();

		return -1;

	}

	//If room for more threads, add to TCB
	else{
		sem->__align = newSem;
		allSems[newSem].value = value;
		allSems[newSem].initFlag = 1;
		allSems[newSem].id = sem;


		allSems[newSem].q = malloc(sizeof(struct threadQueue));
		allSems[newSem].q->length = 0;


		unlock();

		return 0;

	}

}

int sem_wait(sem_t *sem){
	lock();
	if(allSems[sem->__align].value > 0){
		allSems[sem->__align].value--;
		unlock();
		return 1;
	}else{
		//enqueue the calling thread
		enqueue(allSems[sem->__align].q, pthread_self());

		//Save the previous status
		allThreads[currThread].prevStatus = allThreads[currThread].status;

		//Block the thread
		allThreads[currThread].status = BLOCKED;

		unlock();
		//Force a signal to go to scheduler
		raise(SIGALRM);

		return 0;
	}
}

int sem_post(sem_t *sem){
	lock();

	if(allSems[sem->__align].value >= 0 ){
		//Increment
		allSems[sem->__align].value++;

		//If any processes are in this semaphore's queue, wake the next one up, dequeue reset its state, go back to wait
		if(allSems[sem->__align].q->length > 0){
			pthread_t next = dequeue(allSems[sem->__align].q);

			//Make the status unblocked
			allThreads[next].status = allThreads[next].prevStatus;
			//RE LOCK the semaphore
			allSems[sem->__align].value--;

		}

		unlock();
		return 0;
	}else{
		printf("ERROR: Trying to post a semaphore that has not yet been obtained\n");
		unlock();
		return 1;
	}

}

int sem_destroy(sem_t *sem){
	lock();
	if(allSems[sem->__align].q->length > 0 && allSems[sem->__align].initFlag == 1){
		printf("ERROR: Undefined behavior, threads waiting for this semaphore\n");
		unlock();
		return -1;
	}

	if(allSems[sem->__align].initFlag == 0){
		printf("ERROR: Destroying a semaphore that has not been initialized\n");
		unlock();
		return -1;
	}

	allSems[sem->__align].initFlag = 0;
	free(allSems[sem->__align].q);

	unlock();
	return 1;


}

/*---------------QUEUE FUNCTIONS-----------------------*/

void enqueue(struct threadQueue* q, pthread_t thread){
	struct threadNode *n = (struct threadNode*)malloc(sizeof(struct threadNode));
	n->id = thread;
	n->next = NULL;
	q->length = (q->length) + 1;

	if(q->head == NULL){
		q->head = n;
		q->head->next = q->tail;

		return;
	}else if(q->tail == NULL){
		q->head->next = n;
		q->tail = n;

		return;
	}else{
		q->tail->next = n;
		q->tail = n;

		return;
	}
}

pthread_t dequeue(struct threadQueue* q){
	if(q->head == NULL){
		printf("ERROR: No nodes in the queue\n");
		return 0;
	}

	pthread_t topID = q->head->id;
	q->head = q->head->next;
	q->length = q->length - 1;

	return topID;
}



