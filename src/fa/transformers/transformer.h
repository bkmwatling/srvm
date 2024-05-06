#ifndef TRANSFORMER_H
#define TRANSFORMER_H

#include "../smir.h"

typedef StateMachine *transformer_f(StateMachine *);

typedef int state_predicate_f(StateMachine *, state_id);
typedef int trans_predicate_f(StateMachine *, trans_id);

/**
 * Create the submachine induced by a set of state identifiers.
 *
 * The set is indicated by a boolean vector of length equal to the number of
 * states in the state machine. If a state identifier in the given state machine
 * maps to a non-zero value in the boolean vector, it will be in the new state
 * machine.
 *
 * Note that the old state identifiers will not necessarily map to corresponding
 * states in the transformed state machine.
 *
 * @param[in]     sm     the state machine
 * @param[in,out] states the boolean vector indicating which states to keep/
 *                       the map of old state identifiers to new state
 *                       identifiers where 0 indicates the state does not
 *                       exist.
 *
 * @return the submachine
 */
StateMachine *transform_from_states(StateMachine *sm, state_id *states);

/**
 * Create the submachine induced by a set of transition identifiers.
 *
 * The set is indicated by a vector of transition identifiers to keep.
 * If a transition identifier in the given state machine is in the vector, then
 * both of its source and destination states will be in the transformed state
 * machine.
 *
 * Note that the order transitions appear in the vector dictates the order in
 * which they are added to the outgoing transitions of the induced source state.
 * As such, it is possible transitions will be added to the same state in a
 * different order to the original state machine.
 *
 * @param[in] sm          the state machine
 * @param[in] transitions the vector indicating which transitions to keep
 *
 * @return the submachine
 */
StateMachine *transform_from_transitions(StateMachine *sm,
                                         trans_id     *tranitions);

/**
 * Create the submachine induced by the states matching a predicate.
 *
 * If the predicate is NULL, the original state machine is returned.
 *
 * @param[in] sm  the state machine
 * @param[in] spf the predicate function over states
 *
 * @return the submachine (possibly the original)
 */
StateMachine *transform_with_states(StateMachine *sm, state_predicate_f *spf);

/**
 * Create the submachine induced by the transitions matching a predicate.
 *
 * If the predicate is NULL, the original state machine is returned.
 *
 * @param[in] sm  the state machine
 * @param[in] tpf the predicate function over transitions
 *
 * @return the submachine (possibly the original)
 */
StateMachine *transform_with_trans(StateMachine *sm, trans_predicate_f *tpf);

#endif /* TRANSFORMER_H */
