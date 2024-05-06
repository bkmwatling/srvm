#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stddef.h>
#include <stdio.h>

#include "thread_manager.h"

/* --- Data structures ------------------------------------------------------ */

typedef struct thread_pool ThreadPool;

/* --- API routines --------------------------------------------------------- */

/**
 * Create a new thread pool.
 *
 * A thread pool is used for managing thread memory.
 * Instead of killing a thread, a thread is added to the pool. When a new thread
 * is needed, a thread in the pool will be reused. If the pool is empty, then
 * NULL will be provided.
 *
 * @param[in] logfile the file for logging output
 *
 * @return the thread pool
 */
ThreadPool *thread_pool_new(FILE *logfile);

/**
 * Free the resources used by the thread pool.
 *
 * Reports in the logfile provided at creaion the total number of threads
 * freed.
 *
 * @param[in] self        the thread pool
 * @param[in] thread_free a function for freeing the resources of a thread
 */
void thread_pool_free(ThreadPool *self, void (*thread_free)(Thread *t));

/**
 * Get a new thread from the pool.
 *
 * The values of the returned thread's members should be considered garbage, and
 * need to be initialised appropriately.
 *
 * @param[in] self the thread pool
 *
 * @return a thread from the pool, or NULL if the pool is empty
 */
Thread *thread_pool_get_thread(ThreadPool *self);

/**
 * Add a thread to the pool.
 *
 * Threads in the pool should be considered 'dead', and should not be used after
 * calling this function, unless retrieved using `thread_pool_get_thread`.
 *
 * @param[in] self   the thread pool
 * @param[in] thread the thread
 */
void thread_pool_add_thread(ThreadPool *self, Thread *thread);

#endif
