#include <stdlib.h>

#include "../../stc/fatp/vec.h"

#include "transformer.h"

/* --- API functions -------------------------------------------------------- */

StateMachine *transform_from_states(StateMachine *old_sm, state_id *states)
{
    StateMachine *new_sm;
    trans_id     *out_transitions, old_tid, new_tid;
    state_id      old_sid, new_src, new_dst, old_dst;
    size_t        n, i;

    new_sm = smir_default(smir_get_regex(old_sm));

    // build map from old states to new
    for (old_sid = 1; old_sid <= smir_get_num_states(old_sm); old_sid++) {
        if (!states[old_sid - 1]) continue;
        states[old_sid - 1] = smir_add_state(new_sm);

        // add state actions
        if (smir_state_get_num_actions(old_sm, old_sid))
            smir_state_set_actions(new_sm, states[old_sid - 1],
                                   smir_state_clone_actions(old_sm, old_sid));
    }

    // add initial transitions
    out_transitions = smir_get_initial(old_sm, &n);
    for (i = 0; i < n; i++) {
        old_tid = out_transitions[i];

        if ((new_dst = states[smir_get_dst(old_sm, old_tid) - 1])) {
            new_tid = smir_set_initial(new_sm, new_dst);
            if (smir_trans_get_num_actions(old_sm, old_tid)) {
                smir_trans_set_actions(
                    new_sm, new_tid, smir_trans_clone_actions(old_sm, old_tid));
            }
        }
    }
    if (out_transitions) free(out_transitions);

    // add state transitions
    for (old_sid = 1; old_sid <= smir_get_num_states(old_sm); old_sid++) {
        if (!(new_src = states[old_sid - 1])) continue;
        out_transitions = smir_get_out_transitions(old_sm, old_sid, &n);

        for (i = 0; i < n; i++) {
            old_tid = out_transitions[i];

            old_dst = smir_get_dst(old_sm, old_tid);

            if (IS_FINAL_STATE(old_dst)) {
                new_tid = smir_set_final(new_sm, new_src);
            } else if ((new_dst = states[old_dst - 1])) {
                new_tid = smir_add_transition(new_sm, new_src);
                smir_set_dst(new_sm, new_tid, new_dst);
            } else {
                continue;
            }

            // add transition actions
            if (smir_trans_get_num_actions(old_sm, old_tid)) {
                smir_state_set_actions(
                    new_sm, new_src, smir_trans_clone_actions(old_sm, old_tid));
            }
        }

        if (out_transitions) free(out_transitions);
    }

    return new_sm;
}

StateMachine *transform_from_transitions(StateMachine *old_sm,
                                         trans_id     *transitions)
{
    StateMachine *new_sm;
    state_id     *states;
    trans_id      old_tid, new_tid;
    state_id      src, dst;
    size_t        i;

    // TODO: handle initial and final transitions

    new_sm = smir_default(smir_get_regex(old_sm));

    if (!stc_vec_len_unsafe(transitions)) return new_sm;

    states = calloc(smir_get_num_states(old_sm), sizeof(state_id));
    for (i = 0; i < stc_vec_len_unsafe(transitions); i++) {
        old_tid = transitions[i];

        // for each transition, compute the new state identifiers
        src = smir_get_src(old_sm, old_tid);
        dst = smir_get_dst(old_sm, old_tid);

        if (!IS_INITIAL_STATE(src) && !states[src - 1]) {
            states[src - 1] = smir_add_state(new_sm);
            if (smir_state_get_num_actions(old_sm, src)) {
                smir_state_set_actions(new_sm, states[src - 1],
                                       smir_state_clone_actions(old_sm, src));
            }
        }

        if (!IS_FINAL_STATE(dst) && !states[dst - 1]) {
            states[dst - 1] = smir_add_state(new_sm);
            if (smir_state_get_num_actions(old_sm, dst)) {
                smir_state_set_actions(new_sm, states[dst - 1],
                                       smir_state_clone_actions(old_sm, dst));
            }
        }

        // insert transition
        if (IS_INITIAL_STATE(src)) {
            new_tid = smir_set_initial(new_sm, states[dst - 1]);
        } else if (IS_FINAL_STATE(dst)) {
            new_tid = smir_set_final(new_sm, states[src - 1]);
        } else {
            new_tid = smir_add_transition(new_sm, states[src - 1]);
            smir_set_dst(new_sm, new_tid, states[dst - 1]);
        }

        // copy over actions
        if (smir_trans_get_num_actions(old_sm, old_tid)) {
            smir_trans_set_actions(new_sm, new_tid,
                                   smir_trans_clone_actions(old_sm, old_tid));
        }
    }

    if (states) free(states);
    return new_sm;
}

StateMachine *transform_with_states(StateMachine *sm, state_predicate_f *spf)
{
    unsigned int *states;
    state_id      sid;

    if (!spf || !sm) return sm;

    states = malloc(sizeof(unsigned int) * smir_get_num_states(sm));
    for (sid = 1; sid <= smir_get_num_states(sm); sid++)
        states[sid - 1] = spf(sm, sid);
    sm = transform_from_states(sm, states);

    free(states);
    return transform_from_states(sm, states);
}

StateMachine *transform_with_trans(StateMachine *sm, trans_predicate_f *tpf)
{
    trans_id *transitions, *out_trans;
    state_id  sid;
    size_t    n, i;

    if (!tpf || !sm) return sm;

    stc_vec_default_init(transitions);

    for (sid = 1; sid <= smir_get_num_states(sm); sid++) {
        out_trans = smir_get_out_transitions(sm, sid, &n);
        for (i = 0; i < n; i++)
            if (tpf(sm, out_trans[i]))
                stc_vec_push_back(transitions, out_trans[i]);
        if (out_trans) free(out_trans);
    }

    sm = transform_from_transitions(sm, transitions);
    stc_vec_free(transitions);
    return sm;
}
