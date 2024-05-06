#ifndef LOCKSTEP_H
#define LOCKSTEP_H

#include <stdio.h>

#include "../../types.h"
#include "thread_manager.h"

/* --- Thompson ThreadManager function prototypes --------------------------- */

/**
 * Construct a thread manager that performs Thompson-style lockstep regex
 * matching.
 *
 * @param[in] ncounters  the number of counters needed
 * @param[in] memory_len the number of bytes to allocate for thread memory
 * @param[in] ncaptures  the number of captures needed
 * @param[in] logfile    the file for logging output
 *
 * @return the constructed Thompson-style thread manager
 */
ThreadManager *thompson_thread_manager_new(len_t ncounters,
                                           len_t memory_len,
                                           len_t ncaptures,
                                           FILE *logfile);

#endif
