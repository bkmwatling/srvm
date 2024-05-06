#ifndef FLATTEN_H
#define FLATTEN_H

#include "../smir.h"

/**
 * XXX: ONLY TO BE USED ON THOMPSON-CONSTRUCTED STATE MACHINES,
 * BEFORE MEMOISATION
 */

/**
 * Flatten a state machine to have the properties that all states match a
 * character and all other actions are on transitions between states.
 *
 * NOTE: This algorithm also eliminates paths in the original state machine that
 * are equivalent to a higher priority path, where equivalence is determined by
 * the predicates on the path. The number of such eliminated paths is recorded
 * in the provided logfile.
 *
 * @param[in] sm      the state machine
 * @param[in] logfile file for logging
 *
 * @return the flattened state machine
 */
StateMachine *transform_flatten(StateMachine *sm, FILE *logfile);

#endif /* MEMOISATION_H */
