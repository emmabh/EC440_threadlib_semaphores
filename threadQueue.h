#ifndef THREADQUEUE_H
#define THREADQUEUE_H

struct threadNode{
	pthread_t id;
	struct threadNode *next;

};

struct threadQueue{
	struct threadNode *head;
	struct threadNode *tail;
	int length;

};

//void initQueue(struct threadQueue* q, struct threadNode *head);
void enqueue(struct threadQueue* q, pthread_t thread);
pthread_t dequeue(struct threadQueue* q);

#endif