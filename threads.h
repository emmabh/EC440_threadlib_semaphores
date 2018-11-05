#ifndef THREAD_H
#define THREAD_H


//Returns current thread id
pthread_t pthread_self();

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);

void initialize_sys();

void pthread_exit(void *value_ptr);

void pthread_exit_wrapper();

void scheduler();

void lock();

void unlock();

int pthread_join(pthread_t thread, void **value_ptr);


#endif