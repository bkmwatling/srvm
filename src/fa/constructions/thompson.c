#include <assert.h>

#include "../../re/sre.h"
#include "thompson.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define SET_TRANS_PRIORITY(sm, re, sid, enter, leave)   \
    do {                                                \
        if ((re)->greedy) {                             \
            (enter) = smir_add_transition((sm), (sid)); \
            (leave) = smir_add_transition((sm), (sid)); \
        } else {                                        \
            (leave) = smir_add_transition((sm), (sid)); \
            (enter) = smir_add_transition((sm), (sid)); \
        }                                               \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    state_id initial;
    state_id final;
} StateIdPair;

/* --- Helper function prototypes ------------------------------------------- */

static StateIdPair
emit(StateMachine *sm, const RegexNode *re, const CompilerOpts *opts);

/* --- API function --------------------------------------------------------- */

StateMachine *thompson_construct(Regex re, const CompilerOpts *opts)
{
    StateMachine *sm;
    StateIdPair   child;

    sm    = smir_default(re.regex);
    child = emit(sm, re.root, opts);
    smir_set_initial(sm, child.initial);
    smir_set_final(sm, child.final);

    return sm;
}

/* --- Helper functions ----------------------------------------------------- */

static StateIdPair
emit(StateMachine *sm, const RegexNode *re, const CompilerOpts *opts)
{
    StateIdPair state_ids, child_state_ids;
    trans_id    out, enter, leave;
    state_id    sid;

    switch (re->type) {
        case EPSILON:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            break;

        case CARET:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            smir_state_append_action(sm, state_ids.final,
                                     smir_action_zwa(ACT_BEGIN));
            break;

        case DOLLAR:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            smir_state_append_action(sm, state_ids.final,
                                     smir_action_zwa(ACT_END));
            break;

        // case MEMOISE:
        //     state_ids.initial = state_ids.final = smir_add_state(sm);
        //     smir_state_append_action(sm, state_ids.final,
        //                              smir_action_num(ACT_MEMO, re->rid));
        //     break;
        //
        case LITERAL:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            smir_state_append_action(sm, state_ids.final,
                                     smir_action_char(re->ch));
            break;

        case CC:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            smir_state_append_action(
                sm, state_ids.final,
                smir_action_predicate(intervals_clone(re->intervals)));
            break;

        case ALT:
            state_ids.initial = smir_add_state(sm);

            child_state_ids = emit(sm, re->left, opts);
            out             = smir_add_transition(sm, state_ids.initial);
            smir_set_dst(sm, out, child_state_ids.initial);
            out = smir_add_transition(sm, child_state_ids.final);

            child_state_ids = emit(sm, re->right, opts);
            leave           = smir_add_transition(sm, state_ids.initial);
            smir_set_dst(sm, leave, child_state_ids.initial);
            leave = smir_add_transition(sm, child_state_ids.final);

            state_ids.final = smir_add_state(sm);
            smir_set_dst(sm, out, state_ids.final);
            smir_set_dst(sm, leave, state_ids.final);
            break;

        case CONCAT:
            state_ids       = emit(sm, re->left, opts);
            child_state_ids = emit(sm, re->right, opts);

            out = smir_add_transition(sm, state_ids.final);
            smir_set_dst(sm, out, child_state_ids.initial);

            state_ids.final = child_state_ids.final;
            break;

        case CAPTURE:
            state_ids.initial = smir_add_state(sm);
            child_state_ids   = emit(sm, re->left, opts);
            state_ids.final   = smir_add_state(sm);

            out = smir_add_transition(sm, state_ids.initial);
            smir_set_dst(sm, out, child_state_ids.initial);
            smir_trans_append_action(
                sm, out, smir_action_num(ACT_SAVE, 2 * re->capture_idx));

            out = smir_add_transition(sm, child_state_ids.final);
            smir_set_dst(sm, out, state_ids.final);
            smir_trans_append_action(
                sm, out, smir_action_num(ACT_SAVE, 2 * re->capture_idx + 1));
            break;

        case STAR:
            state_ids.initial = smir_add_state(sm);
            if (opts->capture_semantics == CS_PCRE) sid = smir_add_state(sm);
            child_state_ids = emit(sm, re->left, opts);
            if (opts->capture_semantics == CS_RE2)
                sid = child_state_ids.initial;
            state_ids.final = smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, state_ids.initial, enter, leave);
            smir_set_dst(sm, enter, sid);
            smir_set_dst(sm, leave, state_ids.final);
            if (opts->capture_semantics == CS_PCRE) {
                enter = smir_add_transition(sm, sid);
                smir_set_dst(sm, enter, child_state_ids.initial);
                smir_trans_append_action(sm, enter,
                                         smir_action_num(ACT_EPSSET, re->rid));
            }

            SET_TRANS_PRIORITY(sm, re, child_state_ids.final, enter, leave);
            smir_set_dst(sm, enter, sid);
            smir_set_dst(sm, leave, state_ids.final);
            if (opts->capture_semantics == CS_PCRE) {
                smir_trans_append_action(sm, enter,
                                         smir_action_num(ACT_EPSCHK, re->rid));
            } else if (opts->capture_semantics == CS_RE2) {
                smir_state_append_action(sm, child_state_ids.final,
                                         smir_action_num(ACT_EPSCHK, re->rid));
                smir_trans_append_action(sm, enter,
                                         smir_action_num(ACT_EPSSET, re->rid));
            }
            break;

        case PLUS:
            if (opts->capture_semantics == CS_PCRE) {
                state_ids.initial = smir_add_state(sm);
                child_state_ids   = emit(sm, re->left, opts);

                out = smir_add_transition(sm, state_ids.initial);
                smir_set_dst(sm, out, child_state_ids.initial);
                smir_trans_append_action(sm, out,
                                         smir_action_num(ACT_EPSSET, re->rid));
            } else if (opts->capture_semantics == CS_RE2) {
                state_ids = child_state_ids = emit(sm, re->left, opts);
            }
            state_ids.final = smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, child_state_ids.final, enter, leave);
            smir_set_dst(sm, enter, state_ids.initial);
            smir_set_dst(sm, leave, state_ids.final);
            if (opts->capture_semantics == CS_PCRE) {
                smir_trans_append_action(sm, enter,
                                         smir_action_num(ACT_EPSCHK, re->rid));
            } else if (opts->capture_semantics == CS_RE2) {
                smir_state_append_action(sm, child_state_ids.final,
                                         smir_action_num(ACT_EPSCHK, re->rid));
                smir_trans_append_action(sm, enter,
                                         smir_action_num(ACT_EPSSET, re->rid));
            }
            break;

        case QUES:
            state_ids.initial = smir_add_state(sm);
            child_state_ids   = emit(sm, re->left, opts);
            state_ids.final   = smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, state_ids.initial, enter, leave);
            smir_set_dst(sm, enter, child_state_ids.initial);
            smir_set_dst(sm, leave, state_ids.final);

            out = smir_add_transition(sm, child_state_ids.final);
            smir_set_dst(sm, out, state_ids.final);
            break;

        /* TODO: */
        case COUNTER:
        case LOOKAHEAD:
        case BACKREFERENCE: assert(0 && "TODO"); break;
        case NREGEXTYPES: assert(0 && "unreachable"); break;
    }

    return state_ids;
}
