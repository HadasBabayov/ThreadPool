// Hadas Babayov 322807629

#include <pthread.h>
#include "threadPool.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * Free all allocated threadPool memory.
 *
 * @param threadPool
 */
void freeTreadPool(ThreadPool *threadPool) {
    free(threadPool->allThreads);
    osDestroyQueue(threadPool->queue);
    free(threadPool);
}

/**
 * Free all allocated threadPool memory, print error and exit.
 *
 * @param threadPool
 */
void freeAndExit(ThreadPool *threadPool) {
    freeTreadPool(threadPool);
    exit(-1);
}

/**
 * This function run all threads according to the status of the threadPool.
 *
 * @param arg - threadPool.
 * @return 0 if success.
 */
void *threadFunc(void *arg) {
    ThreadPool *pool = (ThreadPool *) arg;

    // Lock status mutex.
    if (pthread_mutex_lock(&pool->statusMutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAndExit(pool);
    }
    // Loop through the task in queue - if the queue isn't empty - take out one task and run it.
    while ((!osIsQueueEmpty(pool->queue) && pool->status == finishAll) || pool->status == wait) {
        // Unlock status mutex.
        if (pthread_mutex_unlock(&pool->statusMutex) != 0) {
            perror("Error in pthread_mutex_unlock");
            freeAndExit(pool);
        }

        // Dequeue one task and run this one.
        Task *task = NULL;
        if (pthread_mutex_lock(&pool->queueMutex) != 0) {
            perror("Error in pthread_mutex_lock");
            freeAndExit(pool);
        }
        if (!osIsQueueEmpty(pool->queue)) {
            task = osDequeue(pool->queue);
        }
        if (pthread_mutex_unlock(&pool->queueMutex) != 0) {
            perror("Error in pthread_mutex_unlock");
            freeAndExit(pool);
        }
        if (task != NULL) {
            task->computeFunc(task->param);
            // Free the allocated memory for task.
            free(task);
            task = NULL;
        }

        // If the queue is empty, and the threadPool isn't destroy, we wait to signal (when task insert).
        if (pthread_mutex_lock(&pool->updateMutex) != 0) {
            perror("Error in pthread_mutex_lock");
            freeAndExit(pool);
        }
        while (pool->status == wait && osIsQueueEmpty(pool->queue)) {
            if (pthread_cond_wait(&pool->cv, &pool->updateMutex) != 0) {
                perror("Error in pthread_cond_wait");
                freeAndExit(pool);
            }
        }
        if (pthread_mutex_unlock(&pool->updateMutex) != 0) {
            perror("Error in pthread_mutex_unlock");
            freeAndExit(pool);
        }

        if (pthread_mutex_lock(&pool->statusMutex) != 0) {
            perror("Error in pthread_mutex_lock");
            freeAndExit(pool);
        }
    }

    if (pthread_mutex_unlock(&pool->statusMutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAndExit(pool);
    }
    return (void *) 0;
}

/**
 * This function initialize all mutex in threadPool.
 *
 * @param threadPool
 */
void initMutex(ThreadPool *threadPool) {
    if (pthread_cond_init(&threadPool->cv, NULL) != 0) {
        perror("Error in pthread_cond_init");
        freeAndExit(threadPool);
    }
    if (pthread_mutex_init(&threadPool->updateMutex, NULL) != 0) {
        perror("Error in pthread_mutex_init");
        freeAndExit(threadPool);
    }
    if (pthread_mutex_init(&threadPool->statusMutex, NULL) != 0) {
        perror("Error in pthread_mutex_init");
        freeAndExit(threadPool);
    }
    if (pthread_mutex_init(&threadPool->queueMutex, NULL) != 0) {
        perror("Error in pthread_mutex_init");
        freeAndExit(threadPool);
    }
}

/**
 * This function creat threadPool -- allocate memory for all field.
 *
 * @param numOfThreads - number of threads that will be in this threadPool.
 * @return threadPool.
 */
ThreadPool *tpCreate(int numOfThreads) {
    ThreadPool *pool = (ThreadPool *) malloc(sizeof(ThreadPool));
    initMutex(pool);
    pool->numOfThreads = numOfThreads;
    pool->isDestroy = 0;
    pool->allThreads = (pthread_t *) malloc(numOfThreads * sizeof(pthread_t));
    pool->status = wait;
    pool->queue = osCreateQueue();
    int i;
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(&pool->allThreads[i], NULL, threadFunc, pool) != 0) {
            perror("Error in pthread_create");
            freeAndExit(pool);
        }
    }
    return pool;
}


/**
 * This function destroy threadPool and mutex.
 *
 * @param threadPool
 */
void destroy(ThreadPool *threadPool) {
    if (pthread_cond_destroy(&threadPool->cv) != 0) {
        perror("Error in pthread_cond_destroy");
        freeAndExit(threadPool);
    }
    if (pthread_mutex_destroy(&threadPool->queueMutex) != 0) {
        perror("Error in pthread_mutex_destroy");
        freeAndExit(threadPool);
    }
    if (pthread_mutex_destroy(&threadPool->statusMutex) != 0) {
        perror("Error in pthread_mutex_destroy");
        freeAndExit(threadPool);
    }
    if (pthread_mutex_destroy(&threadPool->updateMutex) != 0) {
        perror("Error in pthread_mutex_destroy");
        freeAndExit(threadPool);
    }
    while (!osIsQueueEmpty(threadPool->queue)){
        Task *task = osDequeue(threadPool->queue);
        free(task);
    }
    freeTreadPool(threadPool);
}


/**
 * This function destroy threadPool but before - run the task according to param shouldWaitForTasks.
 *
 * @param threadPool
 * @param shouldWaitForTasks - 1 -> finish run all tasks. 0 -> finish only tasks that running now and destroy.
 */

void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {
    // If the threadPool is destroy -- do nothing.
    if (pthread_mutex_lock(&threadPool->statusMutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAndExit(threadPool);
    }
    if (threadPool->isDestroy) {
        if (pthread_mutex_unlock(&threadPool->statusMutex) != 0) {
            perror("Error in pthread_mutex_unlock");
            freeAndExit(threadPool);
        }
        return;
    }
    if (pthread_mutex_unlock(&threadPool->statusMutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAndExit(threadPool);
    }

    // Change the status according to shouldWaitForTasks.
    if (pthread_mutex_lock(&threadPool->statusMutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAndExit(threadPool);
    }
    threadPool->isDestroy = 1;
    if (shouldWaitForTasks) {
        threadPool->status = finishAll;
    } else {
        threadPool->status = finishOnlyRunning;
    }
    if (pthread_mutex_unlock(&threadPool->statusMutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAndExit(threadPool);
    }

    if (pthread_mutex_lock(&threadPool->updateMutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAndExit(threadPool);
    }
    if (pthread_cond_broadcast(&threadPool->cv) != 0) {
        perror("Error in pthread_cond_broadcast");
        freeAndExit(threadPool);
    }
    if (pthread_mutex_unlock(&threadPool->updateMutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAndExit(threadPool);
    }

    // Join all threads -- loop (wait that all threads finish)
    int i;
    for (i = 0; i < threadPool->numOfThreads; ++i) {
        if (pthread_join(threadPool->allThreads[i], NULL) != 0) {
            perror("Error in pthread_join");
            freeAndExit(threadPool);
        }
    }

    // Destroy and free the allocated memory.
    destroy(threadPool);
}


/**
 * This function send signal - that new task insert to the queue.
 *
 * @param threadPool
 */

void condSignal(ThreadPool *threadPool) {
    if (pthread_mutex_lock(&threadPool->updateMutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAndExit(threadPool);
    }
    if (pthread_cond_signal(&threadPool->cv) != 0) {
        perror("Error in pthread_cond_signal");
        freeAndExit(threadPool);
    }
    if (pthread_mutex_unlock(&threadPool->updateMutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAndExit(threadPool);
    }
}


/**
 * This function insert new task to the queue.
 *
 * @param threadPool
 * @param task
 */

void enqueueTask(ThreadPool *threadPool, Task *task) {
    if (pthread_mutex_lock(&threadPool->queueMutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAndExit(threadPool);
    }
    osEnqueue(threadPool->queue, task);
    if (pthread_mutex_unlock(&threadPool->queueMutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAndExit(threadPool);
    }
}


/**
 * This function check if the threadPool destroy.
 *
 * @param threadPool
 * @return  -1 if the threadPool destroy and 0 if not.
 */

int checkIfDestroy(ThreadPool *threadPool) {
    if (pthread_mutex_lock(&threadPool->statusMutex) != 0) {
        perror("Error in pthread_mutex_lock");
        freeAndExit(threadPool);
    }
    if (threadPool->isDestroy) {
        if (pthread_mutex_unlock(&threadPool->statusMutex) != 0) {
            perror("Error in pthread_mutex_unlock");
            freeAndExit(threadPool);
        }
        return -1;
    }
    if (pthread_mutex_unlock(&threadPool->statusMutex) != 0) {
        perror("Error in pthread_mutex_unlock");
        freeAndExit(threadPool);
    }
    return 0;
}


/**
 * This function get new task (func & param) and insert it to the queue.
 * After the insert - send signal that insert new task.
 *
 * @param threadPool
 * @param computeFunc
 * @param param
 * @return 0 if success, -1 if destroy.
 */

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {

    if (checkIfDestroy(threadPool) == -1) {
        return -1;
    }

    // Creat new task.
    Task *task = (Task *) malloc(sizeof(Task));
    task->computeFunc = computeFunc;
    task->param = param;

    // Insert this task to Queue.
    enqueueTask(threadPool, task);

    // Send signal -- that insert new task.
    condSignal(threadPool);
    return 0;
}



