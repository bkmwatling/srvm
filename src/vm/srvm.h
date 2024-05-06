#ifndef SRVM_H
#define SRVM_H

#include "../stc/fatp/string_view.h"

#include "thread_managers/thread_manager.h"

/* --- Preprocessor constants ----------------------------------------------- */

#define NO_MATCH          0
#define MATCH             1
#define MATCHES_EXHAUSTED 2

/* --- Type definitions ----------------------------------------------------- */

typedef struct srvm SRVM;

/* --- SRVM function prototypes --------------------------------------------- */

/**
 * Construct a Symbolic Regular Expression Virtual Machine with given thream
 * manager and program.
 *
 * @param[in] thread_manager the thread manager for the SRVM to use
 * @param[in] prog           the program for the SRVM to execute
 *
 * @return the constructed SRVM
 */
SRVM *srvm_new(ThreadManager *thread_manager, const Program *prog);

/**
 * Free the memory allocated for the SRVM.
 *
 * @param[in] self the SRVM to free
 */
void srvm_free(SRVM *self);

/**
 * Execute the SRVM against an input string.
 *
 * @param[in] self the SRVM to execute
 * @param[in] text the input string to match against
 *
 * @return truthy value if the SRVM matched against the input string; else 0
 */
int srvm_match(SRVM *self, const char *text);

/**
 * Find the next match of the regex of the SRVM inside the input string if
 * possible for partial matching.
 *
 * @param[in] self the SRVM to execute
 * @param[in] text the input string to find next match in
 *
 * @return truthy value if the SRVM found a match in the input string; else 0
 */
int srvm_find(SRVM *self, const char *text);

/**
 * Get the string view into the input string of the capture at the given index
 * from the previous match.
 *
 * @param[in] self the SRVM to get the capture from
 * @param[in] idx  the index of the capture
 *
 * @return the string view of the capture
 */
StcStringView srvm_capture(SRVM *self, len_t idx);

/**
 * Get the string views of all the captures from the previous match of the SRVM.
 *
 * @param[in]  self      the SRVM to get the captures from
 * @param[out] ncaptures the number of captures in the returned array
 *
 * @return the array of capture string views from the SRVM
 */
StcStringView *srvm_captures(SRVM *self, len_t *ncaptures);

/**
 * Determine whether the regex represented by the program matches the input text
 * by using the given thread manager.
 *
 * Convenience function if the same program does not need to be matched again
 * and capture information is not needed.
 *
 * @param[in] thread_manager the thread manager to execute with
 * @param[in] prog           the program to executein
 * @param[in] text           the input string to match against
 *
 * @return truthy value if the program matched against the input string; else 0
 */
int srvm_matches(ThreadManager *thread_manager,
                 const Program *prog,
                 const char    *text);

#endif /* SRVM_H */
