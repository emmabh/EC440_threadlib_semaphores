When a thread is created, an array of active values checks to see if there is a free slot for a new thread (i.e., there aren't 128 active threads). If so, the thread is initialized: its registers (PC, SP), as well as its stack of size 32767 bytes. If this is the first thread to be created, we move to intialize_sys(), which sets up the timer interrupts every 50ms that direct us to the scheduler, and intializes the 0th active thread values for our main thread. 

Then, we go to the scheduler immediately every time a thread is created. Every time the scheduler is about to leave the main thread, it clears the stack space of any other exited threads. The scheduler then uses round robin to find out the next thread that is ready to be executed. If this is the first time this thread is ready, the stack pointer and PC are set, and the status is changed from FIRST_READY to READY. Then we unblock the timer, and switch the thread context to this next ready thread.

pthread_exit() is the return address at the top of the stack, so when the start_routine of the thread is done executing, it goes straight to pthread_exit(), which blocks the timer interrupt so we can then set the status to EXITED, then UNUSED, and free its stack space and slot in the array of active flags. We also decrement the number of active threads, then go back to the scheduler().

pthread_self() consists of returning the pthread_t value of the global value of the current thread.

pthread_join is implemented by changing join_id of the thread passed as an argument to the thread id of the calling thread, and blocking the calling thread. Then, when this argument thread exits, it unblocks the calling thread, returning from the join the next time the calling thread is scheduled.

Continuing on from the thread library, locking & unlocking is implemented by using sigprocmask to block and unblock the signals accordingly.

Semaphores were also implemented in this version. The semaphore struct has a sem_t* id, an integer value, a pointer to a thread queue, and an initialization flag. 

Initialization of a semaphore is done in the same way a new thread is initialized. When there aren't 128 active semaphores, sem_init searches for the lowest one with its initialization flag set to 0. It then sets the ID to this value, sets the value to the user provided value, its initialization flag to 1.

This also allocates a queue the size of a threadQueue.

sem_wait blocks the timer, then checks if the value of the semaphore is greater than 0. If it is, it decrements the value and returns. Otherwise, it puts the current thread ID into a queue of waiting threats. This queue is a queue of nodes that can be enqueued and dequeued. This queue has a head and a tail, and inserts a new node with a next pointer and a pthread_t value.

If a node is inserted into the queue for this semaphore, the current thread becomes blocked and then a signal is raised.

sem_post blocks the timer, and then checks if the semaphore is currently being used. If so, it increments the semaphore's value. If there is a thread waiting in the semaphore's queue, that thread is dequeued, unblocked, and then it takes the semaphore and decrements its value.

If the current semaphore is not in use, sem_post does not increment its value.

sem_destroy returns an error if there are nodes in the queue, or if the semaphore is not initialized. If there are no errors, this function sets the initFlag to 0, and frees the queue.