// Hadas Babayov 322807629

#ifndef EX4_THREADPOOL_H
#define EX4_THREADPOOL_H

#include "osqueue.h"
#include <pthread.h>

// threadPool status - wait fot task \ destroy and running all \ destroy and running only running threads.
enum Status {wait, finishAll, finishOnlyRunning};

// This task we insert to queue.
typedef struct task{
    void (*computeFunc) (void *);
    void* param;
} Task;


typedef struct thread_pool {
   enum Status status;
   int numOfThreads;
   pthread_t *allThreads;
   OSQueue *queue;
   int isDestroy;
   pthread_mutex_t queueMutex;
   pthread_mutex_t statusMutex;
   pthread_mutex_t updateMutex;
   pthread_cond_t cv;
}ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif //EX4_THREADPOOL_H


