#ifndef ALL_MATCHES_H
#define ALL_MATCHES_H

#include <stdio.h>

#include "thread_manager.h"

/**
 * Construct a thread manager that tracks and reports all matches by the
 * underlying thread manager.
 *
 * @param[in] thread_manager the underlying thread manager
 * @param[in] logfile        the file stream for logging captures on match
 * @param[in] text           the input string being matched against
 *
 * @return the constructed all matches thread manager
 */
ThreadManager *all_matches_thread_manager_new(ThreadManager *thread_manager,
                                              FILE          *logfile,
                                              const char    *text);

#endif
