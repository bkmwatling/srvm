#include <stdio.h>
#include <stdlib.h>

#include "flatten.h"

/* --- Preprocessor macros -------------------------------------------------- */

#define ACTION_TO_IDX(type)    ((unsigned int) (type))
#define ACTION_TO_BIT(type)    (1 << ACTION_TO_IDX(type))
#define INSERT_TYPE(set, type) (set |= ACTION_TO_BIT(type))

/* --- Data structures ------------------------------------------------------ */

typedef struct {
    StateMachine *origin_sm; /**< the original machine */
    StateMachine *new_sm;    /**< the new machine */
    byte *created; /**< map from original to states to record of creation in new
                      machine */
    state_id *state_map;   /**< map from original states to new states */
    state_id *state_queue; /**< queue of states in original machine that have
                              been added to new machine */
    size_t    eliminated_path_count; /**< the number of transitions eliminated
                                        since they were not useful */
} FlattenGlobals;

/* --- Helper functions ----------------------------------------------------- */

/**
 * Create a signature for a list of actions for efficient comparisons.
 *
 * NOTE: The signature essentially encodes the ZWAs in the action list. It does
 * not account for any other actions. Currently, this means it only encodes
 * ACT_BEGIN and ACT_END.
 *
 * @param[in] actions the list of actions
 *
 * @return the signature
 */
static int action_list_signature(const ActionList *actions)
{
    ActionListIterator *ali;
    const Action       *a;
    int                 signature = 0;

    if (actions) {
        ali = smir_action_list_iter(actions);
        while ((a = smir_action_list_iterator_next(ali))) {
            switch (smir_action_type(a)) {
                case ACT_CHAR:
                case ACT_PRED:
                case ACT_MEMO:
                case ACT_EPSCHK:
                case ACT_EPSSET:
                case ACT_SAVE: break;

                case ACT_BEGIN:
                case ACT_END:
                    INSERT_TYPE(signature, smir_action_type(a));
                    break;
            }
        }
        free(ali);
    }

    return signature;
}

/**
 * Compare two action lists for predicate equivalence.
 *
 * Predicate equivalence means evaluating the list of actions for the same input
 * will always yield the same result (True or False).
 *
 * TODO: this should really be defined somewhere else.
 *
 * @param[in] al1 the first list of actions
 * @param[in] al2 the second list of actions
 *
 * @return TRUE if they are equal, else FALSE
 */
static int action_list_signature_equivalent(int sig1, int sig2)
{
    // NOTE: any actions that, if appear in one list and not the other, means
    // the lists are not equal
    static int DISAMBIGUATING_ACTIONS =
        ACTION_TO_BIT(ACT_END) | ACTION_TO_BIT(ACT_BEGIN);

    return ((sig1 ^ sig2) & DISAMBIGUATING_ACTIONS) == 0;
}

static int is_epsilon_state(const ActionList *actions)
{
    ActionListIterator *ali = smir_action_list_iter(actions);
    const Action       *act;
    byte                is_epsilon = TRUE;

    while ((act = smir_action_list_iterator_next(ali))) {
        switch (smir_action_type(act)) {
            case ACT_CHAR:
            case ACT_PRED: is_epsilon = FALSE; goto done;

            case ACT_BEGIN:
            case ACT_END:
            case ACT_MEMO:
            case ACT_SAVE:
            case ACT_EPSCHK:
            case ACT_EPSSET: break;
        }
    }

done:
    free(ali);

    return is_epsilon;
}

static void remove_unnecessary_actions(const ActionList *actions)
{
    ActionListIterator *ali = smir_action_list_iter(actions);
    const Action       *act;

    while ((act = smir_action_list_iterator_next(ali))) {
        switch (smir_action_type(act)) {
            // remove EPSSET/EPSCHK actions
            case ACT_EPSCHK:
            case ACT_EPSSET: smir_action_list_iterator_remove(ali); break;
            case ACT_BEGIN:
            case ACT_END:
            case ACT_CHAR:
            case ACT_PRED:
            case ACT_MEMO:
            case ACT_SAVE: break;
        }
    }
    free(ali);
}

/**
 * Check if the given sequence of actions is satisfiable with respect to EPSSET
 * and EPSCHK actions.
 *
 * @param[in] actions the sequence of actions
 *
 * @return FALSE if the sequence of actions contains an EPSSET followed at some
 *         point by the corresponding EPSCHK, otherwise TRUE
 */
static int action_list_eps_satisfiable(const ActionList *actions)
{
    ActionListIterator *ali;
    const Action       *act;
    size_t             *epssets, satisfiable = TRUE;
    size_t              idx, num;

    // TODO: use Set instead of Vec
    stc_vec_default_init(epssets);
    ali = smir_action_list_iter(actions);

    while ((act = smir_action_list_iterator_next(ali))) {
        switch (smir_action_type(act)) {
            case ACT_BEGIN:
            case ACT_END:
            case ACT_CHAR:
            case ACT_PRED:
            case ACT_MEMO:
            case ACT_SAVE: break;
            case ACT_EPSCHK:
                num = smir_action_get_num(act);
                for (idx = 0; idx < stc_vec_len(epssets); idx++) {
                    if (epssets[idx] == num) {
                        satisfiable = FALSE;
                        goto done;
                    }
                }
                break;
            case ACT_EPSSET:
                stc_vec_push_back(epssets, smir_action_get_num(act));
                break;
        }
    }

done:
    free(ali);
    stc_vec_free(epssets);

    return satisfiable;
}

/**
 * Check if concatenating the given lists results in a sequence of actions
 * where an EPSSET is executed before the corresponding EPSCHK.
 */
static int can_explore(const ActionList *prefix, const ActionList *suffix)
{
    ActionList *concat, *tmp;
    int         satisfiable;

    concat = smir_action_list_clone(prefix);
    tmp    = smir_action_list_clone(suffix);
    smir_action_list_append(concat, tmp);

    satisfiable = action_list_eps_satisfiable(concat);

    smir_action_list_free(tmp);
    smir_action_list_free(concat);

    return satisfiable;
}

/**
 * Check if adding a transition between the source and destination states with
 * the given actions is useful.
 *
 * In this context, `useful` means the proposed transition can be successfully
 * taken in instanaces where existing transitions cannot.
 *
 * @param[in] actions the sequence of actions for the new transition
 * @param[in] src     the source state identifier
 * @param[in] dst     the destination state identifier
 * @param[in] sm      the state machine
 *
 * @return TRUE if the transition is useful, otherwise FALSE
 */
static int transition_is_useful(const ActionList *actions,
                                state_id          src,
                                state_id          dst,
                                StateMachine     *sm)
{
    trans_id *out_trans;
    size_t    nout, i;
    byte      useful        = TRUE;
    int       new_trans_sig = action_list_signature(actions), old_trans_sig;

    out_trans = smir_get_out_transitions(sm, src, &nout);
    for (i = 0; i < nout; i++) {
        if (smir_get_dst(sm, out_trans[i]) == dst) {
            old_trans_sig =
                action_list_signature(smir_trans_get_actions(sm, out_trans[i]));
            if (old_trans_sig == 0 || action_list_signature_equivalent(
                                          old_trans_sig, new_trans_sig)) {
                useful = FALSE;
                break;
            }
        }
    }
    free(out_trans);

    return useful;
}

/**
 *
 * For every outgoing transition from this state, if it can be explored (does
 * not contain EPSCHK action where corresponding EPSSET action is already on the
 * action path):
 *
 * 1. If the transition contains an EPSCHK and the corresponding EPSSET is on
 * the action path, go to 7.
 * 2. Add the actions on the transition to the action path.
 * 3. If the destination state contains a character-consuming action,
 *     3.1. If this state has not been visited, copy the state to the new
 *           machine, and record the corresponding new state identifier.
 *           Mark as visited.
 *     3.2. Add a transition from the corresponding source state in the
 *            new machine to the corresponding destination state, with a
 *            copy of the current action path. Go to 6.
 * 4. Otherwise, add the state actions to the action path, and recurse on
 *      the destination state.
 * 5. Remove the actions from the state from the action
 *      path.
 * 6. Remove the actions from the transition from the action path.
 * 7. Continue iteration.
 *
 */
static void flatten_dfs(state_id        original_src,
                        state_id        current,
                        ActionList     *path_actions,
                        FlattenGlobals *globals)
{
    ActionListIterator *ali;
    ActionList         *action_list_clone;
    const ActionList   *trans_actions, *original_dst_actions;
    trans_id           *out_trans;
    size_t              nout, idx, count;
    state_id            original_dst, new_src, new_dst;
    trans_id            new_trans;

    out_trans = smir_get_out_transitions(globals->origin_sm, current, &nout);
    for (idx = 0; idx < nout; idx++) {
        trans_actions =
            smir_trans_get_actions(globals->origin_sm, out_trans[idx]);
        if (!can_explore(path_actions, trans_actions)) continue;

        // add transition actions to current path
        action_list_clone = smir_action_list_clone(trans_actions);
        smir_action_list_append(path_actions, action_list_clone);
        smir_action_list_free(action_list_clone);

        original_dst = smir_get_dst(globals->origin_sm, out_trans[idx]);
        original_dst_actions =
            smir_state_get_actions(globals->origin_sm, original_dst);
        if (original_dst != FINAL_STATE_ID &&
            is_epsilon_state(original_dst_actions)) {
            // add state actions to path
            action_list_clone = smir_action_list_clone(original_dst_actions);
            smir_action_list_append(path_actions, action_list_clone);
            smir_action_list_free(action_list_clone);

            // recurse
            flatten_dfs(original_src,
                        smir_get_dst(globals->origin_sm, out_trans[idx]),
                        path_actions, globals);

            // remove state actions from path
            for (ali  = smir_action_list_iter(path_actions),
                count = smir_action_list_len(original_dst_actions);
                 smir_action_list_iterator_prev(ali) && count--;)
                smir_action_list_iterator_remove(ali);
            free(ali);
        } else {
            // insert state in new machine if not created before
            // TODO: possibly remove need for 'created' by having special value
            // in 'state_map' indicating if it has been created or not

            new_src = globals->state_map[original_src];

            if (!globals->created[original_dst]) {
                globals->created[original_dst] = TRUE;
                new_dst = globals->state_map[original_dst] =
                    smir_add_state(globals->new_sm);
                smir_state_set_actions(
                    globals->new_sm, new_dst,
                    smir_action_list_clone(original_dst_actions));
                stc_vec_push_back(globals->state_queue, original_dst);
            } else {
                new_dst = globals->state_map[original_dst];
            }

            // create actions for transition in new machine
            action_list_clone = smir_action_list_clone(path_actions);
            remove_unnecessary_actions(action_list_clone);

            // insert transition from source to new state, if it is
            // useful
            // TODO: consider making this step a separate transform, where we in
            // general eliminate unnecessary transitions from a state machine.
            // This could include the functionality of the `can_explore`
            // function.
            //
            // TODO: Check if transition is useful before cloning the actions?
            if (transition_is_useful(action_list_clone, new_src, new_dst,
                                     globals->new_sm)) {
                new_trans = smir_add_transition(globals->new_sm, new_src);
                smir_set_dst(globals->new_sm, new_trans, new_dst);
                smir_trans_set_actions(globals->new_sm, new_trans,
                                       action_list_clone);
            } else {
                globals->eliminated_path_count++;
                smir_action_list_free(action_list_clone);
            }
        }

        // remove transition actions
        for (ali  = smir_action_list_iter(path_actions),
            count = smir_action_list_len(trans_actions);
             smir_action_list_iterator_prev(ali) && count--;) {
            smir_action_list_iterator_remove(ali);
        }
        free(ali);
    }

    free(out_trans);
}

static void flatten(StateMachine *original, StateMachine *new, FILE *logfile)
{
    ActionList     *path_actions;
    FlattenGlobals *globals;
    size_t          nstates;
    state_id        src;

    if (!original || !new) return;

    path_actions = smir_action_list_new();

    // NOTE: +1 to account for implicit INITIAL_STATE and FINAL_STATE (state 0)
    // that occurs in every SMIR
    nstates            = smir_get_num_states(original) + 1;
    globals            = malloc(sizeof(*globals));
    globals->origin_sm = original;
    globals->new_sm    = new;
    globals->created   = calloc(nstates, sizeof(*(globals->created)));
    globals->state_map = calloc(nstates, sizeof(*(globals->state_map)));
    stc_vec_default_init(globals->state_queue);
    globals->eliminated_path_count = 0;

    globals->created[INITIAL_STATE_ID]   = TRUE;
    globals->state_map[INITIAL_STATE_ID] = INITIAL_STATE_ID;
    globals->created[FINAL_STATE_ID]     = TRUE;
    globals->state_map[FINAL_STATE_ID]   = FINAL_STATE_ID;
    stc_vec_push_front(globals->state_queue, INITIAL_STATE_ID);

    while (!stc_vec_is_empty(globals->state_queue)) {
        src = globals->state_queue[0];
        stc_vec_remove(globals->state_queue, 0);
        flatten_dfs(src, src, path_actions, globals);
    }
    fprintf(logfile, "NUMBER OF TRANSITIONS ELIMINATED FROM FLATTENING: %zu\n",
            globals->eliminated_path_count);

    smir_action_list_free(path_actions);
    stc_vec_free(globals->state_queue);
    free(globals->state_map);
    free(globals->created);
    free(globals);
}

/* --- API function --------------------------------------------------------- */

StateMachine *transform_flatten(StateMachine *sm, FILE *logfile)
{
    StateMachine *out;

    if (!sm) return sm;

    out = smir_default(smir_get_regex(sm));
    flatten(sm, out, logfile);

    return out;
}
