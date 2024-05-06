#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdio.h>

#include "thread_manager.h"

/**
 * Construct a thread manager that benchmarks the performance of an SRVM
 * execution whilst using an underlying thread manager.
 *
 * @param[in] thread_manager the underlying thread manager
 * @param[in] logfile        the file stream for logging captures on match
 *
 * @return the constructed benchmark thread manager
 */
ThreadManager *benchmark_thread_manager_new(ThreadManager *thread_manager,
                                            FILE          *logfile);

#endif
