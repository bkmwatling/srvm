#ifndef MEMOISATION_H
#define MEMOISATION_H

#include "thread_manager.h"

/**
 * Construct a thread manager that implements memoisation around an underlying
 * thread manager.
 *
 * @param[in] thread_manager the underlying thread manager
 *
 * @return the constructed memoised thread manager
 */
ThreadManager *memoised_thread_manager_new(ThreadManager *thread_manager);

#endif
