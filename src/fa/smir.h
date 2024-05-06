#ifndef SMIR_H
#define SMIR_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../vm/srvm.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define NULL_STATE       0
#define INITIAL_STATE_ID NULL_STATE
#define FINAL_STATE_ID   NULL_STATE

#define IS_INITIAL_STATE(sid) (sid == INITIAL_STATE_ID)
#define IS_FINAL_STATE(sid)   (sid == FINAL_STATE_ID)

/* --- Type definitions ----------------------------------------------------- */

typedef enum {
    ACT_BEGIN,
    ACT_END,

    ACT_CHAR,
    ACT_PRED,

    ACT_MEMO,
    ACT_SAVE,
    ACT_EPSCHK,
    ACT_EPSSET,
} ActionType;

typedef ActionType PredicateType;

typedef struct action               Action;
typedef Action                      Predicate;
typedef struct action_list          ActionList;
typedef struct action_list_iterator ActionListIterator;
typedef struct state_machine        StateMachine;

typedef uint32_t state_id; // 0 => nonexistent
typedef uint64_t trans_id; // (src state_id, idx into outgoing transitions)

typedef void compile_f(void *meta, Program *prog);

/* --- API ------------------------------------------------------------------ */

/**
 * Create a blank state machine.
 *
 * @param[in] regex the underlying regular expression
 *
 * @return the blank state machine
 */
StateMachine *smir_default(const char *regex);

/**
 * Create a new state machine with a given number of states.
 *
 * Each state is assigned a unique state identifier in [1...nstates].
 *
 * @param[in] regex   the underlying regular expression
 * @param[in] nstates the number of states
 *
 * @return the new state machine
 */
StateMachine *smir_new(const char *regex, uint32_t nstates);

/**
 * Free all memory used by the state machine.
 *
 * @param[in] self the state machine
 */
void smir_free(StateMachine *self);

/**
 * Compile a state machine.
 *
 * @param[in] self the state machine
 *
 * @return the compiled program
 */
Program *smir_compile(StateMachine *self);

/**
 * Create a new state in the state machine.
 *
 * @param[in] self the state machine
 *
 * @return the unique state identifier
 */
state_id smir_add_state(StateMachine *self);

/**
 * Get the current number of states in the state machine.
 *
 * @param[in] self the state machine
 *
 * @return the number of states
 */
size_t smir_get_num_states(StateMachine *self);

/**
 * Get the underlying reguler expression of a state machine.
 *
 * @param[in] self the state machine
 *
 * @return the underlying regular expression
 */
const char *smir_get_regex(StateMachine *self);

/**
 * Mark a state as initial by inserting a blank initial transition to the state.
 *
 * Note: The order of calls to this function determines the priorities by which
 * the initialisation functions are executed.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the unique transition identifier of the initialisation transition
 */
trans_id smir_set_initial(StateMachine *self, state_id sid);

/**
 * Get the initial transitions of the state machine.
 *
 * Convenience function equivalent to calling @ref smir_get_out_transitions
 * with @ref INITIAL_STATE_ID.
 *
 * @param[in]  self the state machine
 * @param[out] n    the number of initial transitions
 *
 * @return the identifiers representing the initial transitions
 */
trans_id *smir_get_initial(StateMachine *self, size_t *n);

/**
 * Mark a state as final.
 *
 * Note: When this function is called, the priority of the finalisation function
 * for the state will be lower than any transition added to the state before the
 * call.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the unique transition identifier of the finalisation transition
 */
trans_id smir_set_final(StateMachine *self, state_id sid);

/**
 * Add an outgoing transition to a state.
 *
 * The added transition will not have a destination state.
 * @see smir_set_dst.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the unique transition identifer
 */
trans_id smir_add_transition(StateMachine *self, state_id sid);

/**
 * Get the outgoing transitions of a state.
 *
 * The returned array is created with a call to `malloc`, and the memory
 * should be released using a single call to `free`.
 *
 * @param[in]  self the state machine
 * @param[in]  sid  the unique state identifier
 * @param[out] n    the length of the returned array
 *
 * @return an array of transition identifiers representing the outgoing
 *         transitions
 */
trans_id *smir_get_out_transitions(StateMachine *self, state_id sid, size_t *n);

/**
 * Get the number of actions on a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the number of actions
 */
size_t smir_state_get_num_actions(StateMachine *self, state_id sid);

/**
 * Get the actions of a state. Will return NULL if the `sid` is INITIAL_STATE_ID
 * or FINAL_STATE_ID.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the actions of the state, or NULL
 */
const ActionList *smir_state_get_actions(StateMachine *self, state_id sid);

/**
 * Append an action to a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] act  the action to append
 */
void smir_state_append_action(StateMachine *self,
                              state_id      sid,
                              const Action *act);

/**
 * Prepend an action to a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] act  the action to prepend
 */
void smir_state_prepend_action(StateMachine *self,
                               state_id      sid,
                               const Action *act);

/**
 * Set the actions of a state.
 *
 * Frees any memory in use by the existing list of actions.
 * Nothing changes if the provided list is NULL.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] acts the list of actions
 */
void smir_state_set_actions(StateMachine *self, state_id sid, ActionList *acts);

/**
 * Clone the list of actions of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the state identifier of the state whose actions should be
 *                 cloned
 *
 * @return the cloned list of actions
 */
ActionList *smir_state_clone_actions(StateMachine *self, state_id sid);

/**
 * Get the source state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the unique state identifier of the source state
 */
state_id smir_get_src(StateMachine *self, trans_id tid);

/**
 * Get the destination state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the unique state identifier of the destination state
 */
state_id smir_get_dst(StateMachine *self, trans_id tid);

/**
 * Set the destination state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] dst  the unique state identifier of the destination state
 */
void smir_set_dst(StateMachine *self, trans_id tid, state_id dst);

/**
 * Get the number of actions on a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the number of actions
 */
size_t smir_trans_get_num_actions(StateMachine *self, trans_id tid);

/**
 * Get the actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the actions of the transition
 */
const ActionList *smir_trans_get_actions(StateMachine *self, trans_id tid);

/**
 * Append an action to a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] act  the action to append
 */
void smir_trans_append_action(StateMachine *self,
                              trans_id      tid,
                              const Action *act);

/**
 * Prepend an action to a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] act  the action to prepend
 */
void smir_trans_prepend_action(StateMachine *self,
                               trans_id      tid,
                               const Action *act);

/**
 * Set the actions of a transition.
 *
 * Frees any memory in use by the existing list of actions.
 * Nothing changes if the provided list is NULL.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] acts the list of actions
 */
void smir_trans_set_actions(StateMachine *self, trans_id tid, ActionList *acts);

/**
 * Clone the list of actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the transition identifier of the transition whose actions
 *                 should be cloned
 *
 * @return the cloned list of actions
 */
ActionList *smir_trans_clone_actions(StateMachine *self, trans_id tid);

/**
 * Print the state machine.
 *
 * @param[in] self   the state machine
 * @param[in] stream the file stream to print to
 */
void smir_print(StateMachine *self, FILE *stream);

/* --- Action and ActionList functions -------------------------------------- */

/**
 * Create an action for zero-width assertions.
 *
 * Valid types are ACT_BEGIN and ACT_END.
 *
 * @param[in] type the type of zero-width assertion
 *
 * @return the action
 */
const Action *smir_action_zwa(ActionType type);

/**
 * Create an action for matching against a character.
 *
 * @param[in] ch the character
 *
 * @return the action
 */
const Action *smir_action_char(const char *ch);

/**
 * Create an action for matching against a predicate.
 *
 * @param[in] pred     the predicate
 *
 * @return the action
 */
const Action *smir_action_predicate(const Intervals *pred);

/**
 * Create an action which require relative pointers into memory.
 *
 * Valid types are ACT_SAVE, ACT_EPSCHK, ACT_EPSSET, ACT_MEMO.
 *
 * If it is ACT_SAVE, the identifier is the index into capture memory.
 * Otherwise, it is the unique regex identifier.
 *
 * @param[in] type the type of the action
 * @param[in] k    the identifer for this action
 *
 * @return the action
 */
const Action *smir_action_num(ActionType type, size_t k);

/**
 * Clone an action.
 *
 * @param[in] self the action to clone
 *
 * @return the cloned action
 */
const Action *smir_action_clone(const Action *self);

/**
 * Free the memory of the action.
 *
 * @param[in] self the action to free
 */
void smir_action_free(const Action *self);

/**
 * Get the type of the action.
 *
 * @param[in] self the action
 *
 * @return the type of the action
 */
ActionType smir_action_type(const Action *self);

/**
 * Get the number associated with an action if possible.
 *
 * Note: this function returns 0 if the type of the action is not ACT_MEMO,
 * ACT_SAVE, ACT_EPSCHK, or ACT_EPSSET.
 *
 * @param[in] self the action
 *
 * @return the number associated with the action
 */
size_t smir_action_get_num(const Action *self);

/**
 * Print the action.
 *
 * @param[in] self   the action
 * @param[in] stream the file stream to print to
 */
void smir_action_print(const Action *self, FILE *stream);

/**
 * Create an empty list of actions.
 *
 * @return the empty list of actions
 */
ActionList *smir_action_list_new(void);

/**
 * Clone the list of actions.
 *
 * @param[in] self the list of actions to clone
 *
 * @return the cloned list of actions
 */
ActionList *smir_action_list_clone(const ActionList *self);

/**
 * Clone the list of actions into an existing list.
 *
 * Note: If the list to clone into is not empty, this will insert the cloned
 * items at the end of the list.
 *
 * @param[in] self  the list of actions to clone
 * @param[in] clone the preallocated list to clone into
 */
void smir_action_list_clone_into(const ActionList *self, ActionList *clone);

/**
 * Remove (and free) the elements of a list of actions.
 *
 * @param[in] self the list of actions
 */
void smir_action_list_clear(ActionList *self);

/**
 * Free the list of actions.
 *
 * @param[in] self the list of actions
 */
void smir_action_list_free(ActionList *self);

/**
 * Get the number of actions in a list of actions.
 *
 * NOTE: Currently O(n) complexity.
 *
 * @param[in] self the list of actions
 *
 * @return the number of actions in the list (0 if the list is NULL or empty)
 */
size_t smir_action_list_len(const ActionList *self);

/**
 * Push an action to the back of a list of actions.
 *
 * @param[in] self the list of actions
 * @param[in] act  the action to push to the back
 */
void smir_action_list_push_back(ActionList *self, const Action *act);

/**
 * Push an action to the front of a list of actions.
 *
 * @param[in] self the list of actions
 * @param[in] act  the action to push to the front
 */
void smir_action_list_push_front(ActionList *self, const Action *act);

/**
 * Append a list of actions to another list of actions.
 *
 * Note: the appended list will be drained after the operation (becomes empty).
 *
 * @param[in] self the list of actions to append to
 * @param[in] acts the list of actions to append
 */
void smir_action_list_append(ActionList *self, ActionList *acts);

/**
 * Prepend a list of actions to another list of actions.
 *
 * Note: the prepended list will be drained after the operation (becomes empty).
 *
 * @param[in] self the list of actions to prepend to
 * @param[in] acts the list of actions to prepend
 */
void smir_action_list_prepend(ActionList *self, ActionList *acts);

/**
 * Create an interator for a list of actions to be able to go over the actions
 * of the list.
 *
 * Note: the iterator must be freed with a single call to free.
 *
 * @param[in] self the list of actions to iterator over
 *
 * @return the interator for the list of actions
 */
ActionListIterator *smir_action_list_iter(const ActionList *self);

/**
 * Get the next action in the action list iterator.
 *
 * Note: you can call this function interchangeably with
 * @ref smir_action_list_iterator_prev, but note that if that function returns
 * NULL, then this function will return NULL for the rest of the iterator's
 * lifetime.
 *
 * @param[in] self the action list iterator
 *
 * @return the next action in the action list iterator if there is one;
 *         else NULL
 */
const Action *smir_action_list_iterator_next(ActionListIterator *self);

/**
 * Get the previous action in the action list iterator.
 *
 * Note: you can call this function interchangeably with
 * @ref smir_action_list_iterator_next, but note that if that function returns
 * NULL, then this function will return NULL for the rest of the iterator's
 * lifetime.
 *
 * @param[in] self the action list iterator
 *
 * @return the previous action in the action list iterator if there is one;
 *         else NULL
 */
const Action *smir_action_list_iterator_prev(ActionListIterator *self);

/**
 * Remove and free the memory of the current iteration action for the iterator
 * from the list of actions this iterator belongs to.
 *
 * The current iteration action is the last action returned from the next or
 * prev iterator functions. If NULL was returned last or neither of these two
 * functions have been called yet, then this function does nothing.
 *
 * @param[in] self the action list iterator
 */
void smir_action_list_iterator_remove(ActionListIterator *self);

/**
 * Print the list of actions.
 *
 * @param[in] self   the list of actions
 * @param[in] stream the file stream to print to
 */
void smir_action_list_print(const ActionList *self, FILE *stream);

/* --- Extendable API ------------------------------------------------------- */

/**
 * Set the pre-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] meta the pre-predicate meta data
 *
 * @return the previous pre-predicate meta data (initially NULL)
 */
void *smir_set_pre_meta(StateMachine *self, state_id sid, void *meta);

/**
 * Get the pre-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the pre-predicate meta data
 */
void *smir_get_pre_meta(StateMachine *self, state_id sid);

/**
 * Set the post-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] meta the post-predicate meta data
 *
 * @return the previous post-predicate meta data (initially NULL)
 */
void *smir_set_post_meta(StateMachine *self, state_id sid, void *meta);

/**
 * Get the post-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the post-predicate meta data
 */
void *smir_get_post_meta(StateMachine *self, state_id sid);

/**
 * Compile the state machine to VM instructions, including the meta data.
 *
 * @param[in] self      the state machine
 * @param[in] pre_meta  the compiler for pre-predicate meta data at states
 * @param[in] post_meta the compiler for post-predicate meta data at states
 *
 * @return the compiled program
 */
Program *smir_compile_with_meta(StateMachine *self,
                                compile_f    *pre_meta,
                                compile_f    *post_meta);

/**
 * Reorder the states of the state machine with the given ordering.
 *
 * The order of compilation of the states in the VM will be determined by the
 * ordering parameter. Once this function is called, the state transitions
 * identifiers will be invalidated as they are changed to match the new
 * ordering. Passing in NULL will not change the state machine. This function
 * assumes that the ordering contains unique values from 1 to the number of
 * states in the state machine, and contains every state identifier.
 *
 * @param[in] self         the state machine
 * @param[in] sid_ordering the new order of the states
 */
void smir_reorder_states(StateMachine *self, state_id *sid_ordering);

#endif /* SMIR_H */
