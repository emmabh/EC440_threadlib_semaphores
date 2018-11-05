#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "threadQueue.h"


/*void initQueue(struct threadQueue* q, struct threadNode *head){
	q->head = NULL;
	q->head->next = q->tail;
	q->tail = NULL;
	q->length = 0;

	return;
}*/

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
		return 0;
	}

	pthread_t topID = q->head->id;
	q->head = q->head->next;
	q->length = q->length - 1;

	return topID;
}