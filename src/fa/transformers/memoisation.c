#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "memoisation.h"

/* --- Helper functions ----------------------------------------------------- */

static byte *memoise_in(StateMachine *sm)
{
    size_t    i, n, nstates = smir_get_num_states(sm);
    size_t   *in_degrees = calloc(nstates, sizeof(size_t));
    byte     *memo_sids  = calloc(nstates, sizeof(byte));
    state_id  sid, dst;
    trans_id *out_transitions;

    for (sid = 0; sid <= nstates; sid++) {
        out_transitions = smir_get_out_transitions(sm, sid, &n);
        for (i = 0; i < n; i++) {
            dst = smir_get_dst(sm, out_transitions[i]);
            if (!IS_FINAL_STATE(dst)) in_degrees[dst - 1]++;
        }
        if (out_transitions) free(out_transitions);
    }

    for (sid = 1; sid <= nstates; sid++)
        memo_sids[sid - 1] = in_degrees[sid - 1] > 1;

    if (in_degrees) free(in_degrees);

    return memo_sids;
}

static void
cn_dfs(StateMachine *sm, state_id sid, byte *has_backedge, byte *on_path)
{
    trans_id *out_transitions;
    size_t    i, n;
    state_id  dst;

    if (sid) on_path[sid - 1] = TRUE;
    out_transitions = smir_get_out_transitions(sm, sid, &n);
    for (i = 0; i < n; i++) {
        dst = smir_get_dst(sm, out_transitions[i]);

        if (IS_FINAL_STATE(dst)) continue;

        if (on_path[dst - 1]) {
            has_backedge[dst - 1] = TRUE;
            continue;
        }

        cn_dfs(sm, dst, has_backedge, on_path);
    }

    if (out_transitions) free(out_transitions);
    if (sid) on_path[sid - 1] = FALSE;
}

static byte *memoise_cn(StateMachine *sm)
{
    size_t nstates   = smir_get_num_states(sm);
    byte  *memo_sids = calloc(nstates, sizeof(byte));
    byte  *on_path;

    if (nstates > 0) {
        on_path = calloc(nstates, sizeof(byte));
        cn_dfs(sm, INITIAL_STATE_ID, memo_sids, on_path);
        free(on_path);
    }

    return memo_sids;
}

/**
 * Memoise the states of a state machine in the given set of state identifiers.
 *
 * The set of state identifiers, `sids`, is interpretted as a boolean array
 * indexed by state identifier. A non-zero value in the array for a given state
 * identifier `i` indicates that state should be memoised, while a value of zero
 * indicates it should not be.
 *
 * @param[in] sm      the state machine
 * @param[in] sids    a boolean array indicating if a state is in the set to be
 *                    memoised
 * @param[in] logfile the file for logging output
 */
static void memoise_states(StateMachine *sm, byte *sids, FILE *logfile)
{
    state_id sid;
    size_t   nstates, k = SIZE_MAX;
    // NOTE: `k` can be any value -- it must only be unique amongst MEMO
    // actions. Of course, if there is already memoisation then there could be
    // overlap. It is expected that starting at the maximum possible value for k
    // and decrementing should give the least overlap.

    for (sid = 1, nstates = smir_get_num_states(sm); sid <= nstates; sid++)
        if (sids[sid - 1])
            smir_state_prepend_action(sm, sid, smir_action_num(ACT_MEMO, k--));

    if (logfile)
        fprintf(logfile, "NUMBER OF STATES MEMOISED: %zu\n", SIZE_MAX - k);
}

/* --- API function --------------------------------------------------------- */

StateMachine *
transform_memoise(StateMachine *sm, MemoScheme memo, FILE *logfile)
{
    byte *memo_sids;

    if (!sm) return sm;

    switch (memo) {
        case MS_NONE: return sm;
        case MS_IN: memo_sids = memoise_in(sm); break;
        case MS_CN: memo_sids = memoise_cn(sm); break;
        case MS_IAR: return sm;
    }

    memoise_states(sm, memo_sids, logfile);
    if (memo_sids) free(memo_sids);

    return sm;
}
