#ifndef MEMOISATION_H
#define MEMOISATION_H

#include "../../vm/compiler.h"
#include "../smir.h"

/**
 * Prepend memoisation actions to states in the given state machine according to
 * the provided memoisation scheme.
 *
 * Note: This transform does not create a new state machine.
 *
 * MS_IN: Memoise states with more than 1 incoming transition.
 * MS_CN: Memoise all states that are targets of backedges.
 * MS_IAR: Not currently supported.
 *
 * @param[in] sm      the state machine
 * @param[in] memo    the memoisation scheme
 * @param[in] logfile the file for logging output
 *
 * @return the original state machine
 */
StateMachine *
transform_memoise(StateMachine *sm, MemoScheme memo, FILE *logfile);

#endif /* MEMOISATION_H */
