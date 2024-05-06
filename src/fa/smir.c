#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../stc/fatp/vec.h"

#include "../utils.h"
#include "smir.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define trans_id_from_parts(sid, idx) ((((trans_id) sid) << 32) | idx)

#define trans_id_sid(tid) ((state_id) (tid >> 32))

#define trans_id_idx(tid) ((uint32_t) (tid & 0xffffffff))

#define trans_init(trans)                    \
    do {                                     \
        DLL_INIT(trans);                     \
        DLL_INIT((trans)->actions_sentinel); \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

typedef struct trans Trans;

struct action {
    ActionType type;

    union {
        const char      *ch;   /**< type = ACT_CHAR                           */
        const Intervals *pred; /**< type = ACT_PRED                           */
        size_t k; /**< type = ACT_MEMO | ACT_SAVE | ACT_EPSCHK | ACT_EPSSET   */
    };
};

typedef struct {
    ActionList *actions_sentinel;
    size_t      nactions;
    Trans      *out_transitions_sentinel;
    size_t      nout;
    void       *pre_meta;
    void       *post_meta;
} State;

struct trans {
    ActionList *actions_sentinel;
    size_t      nactions;
    state_id    src;
    state_id    dst;
    Trans      *prev;
    Trans      *next;
};

struct action_list {
    const Action *act;
    ActionList   *prev;
    ActionList   *next;
};

struct action_list_iterator {
    const ActionList *sentinel;
    ActionList       *current;
};

struct state_machine {
    const char *regex;
    State      *states;
    Trans      *initial_functions_sentinel;
    size_t      ninits;
};

/* --- Helper functions ----------------------------------------------------- */

static void action_list_free(ActionList *self)
{
    if (!self) return;

    smir_action_free(self->act);
    free(self);
}

static void trans_free(Trans *self)
{
    if (!self) return;

    if (self->actions_sentinel) smir_action_list_free(self->actions_sentinel);
    free(self);
}

static void state_free(State *self)
{
    Trans *elem, *next;

    if (self->actions_sentinel) smir_action_list_free(self->actions_sentinel);
    if (self->out_transitions_sentinel)
        DLL_FREE(self->out_transitions_sentinel, trans_free, elem, next);
}

/* --- API ------------------------------------------------------------------ */

StateMachine *smir_default(const char *regex)
{
    StateMachine *sm = malloc(sizeof(*sm));

    sm->regex  = regex;
    sm->ninits = 0;
    stc_vec_default_init(sm->states);
    DLL_INIT(sm->initial_functions_sentinel);

    return sm;
}

StateMachine *smir_new(const char *regex, uint32_t nstates)
{
    StateMachine *sm = malloc(sizeof(*sm));

    sm->regex  = regex;
    sm->ninits = 0;
    stc_vec_init(sm->states, nstates);
    while (nstates--) smir_add_state(sm);
    DLL_INIT(sm->initial_functions_sentinel);

    return sm;
}

void smir_free(StateMachine *self)
{
    Trans *elem, *next;
    size_t nstates;

    if (!self) return;

    if (self->states) {
        nstates = stc_vec_len_unsafe(self->states);
        while (nstates) state_free(&self->states[--nstates]);
        stc_vec_free(self->states);
    }

    if (self->initial_functions_sentinel)
        DLL_FREE(self->initial_functions_sentinel, trans_free, elem, next);

    free(self);
}

Program *smir_compile(StateMachine *self)
{
    return smir_compile_with_meta(self, NULL, NULL);
}

state_id smir_add_state(StateMachine *self)
{
    State state = { 0 };

    DLL_INIT(state.actions_sentinel);
    DLL_INIT(state.out_transitions_sentinel);
    stc_vec_push_back(self->states, state);

    return stc_vec_len_unsafe(self->states);
}

size_t smir_get_num_states(StateMachine *self)
{
    return stc_vec_len(self->states);
}

const char *smir_get_regex(StateMachine *self) { return self->regex; }

trans_id smir_set_initial(StateMachine *self, state_id sid)
{
    Trans *transition;

    trans_init(transition);
    transition->dst = sid;
    DLL_PUSH_BACK(self->initial_functions_sentinel, transition);

    return trans_id_from_parts(NULL_STATE, self->ninits++);
}

trans_id *smir_get_initial(StateMachine *self, size_t *n)
{
    return smir_get_out_transitions(self, NULL_STATE, n);
}

trans_id smir_set_final(StateMachine *self, state_id sid)
{
    return smir_add_transition(self, sid);
}

trans_id smir_add_transition(StateMachine *self, state_id sid)
{
    Trans  *transition, *transitions;
    size_t *n;

    trans_init(transition);
    transition->src = sid;
    if (sid) {
        transitions = self->states[sid - 1].out_transitions_sentinel;
        n           = &self->states[sid - 1].nout;
    } else {
        transitions = self->initial_functions_sentinel;
        n           = &self->ninits;
    }
    DLL_PUSH_BACK(transitions, transition);

    return trans_id_from_parts(sid, (*n)++);
}

trans_id *smir_get_out_transitions(StateMachine *self, state_id sid, size_t *n)
{
    trans_id *tids;
    size_t    i, m;

    m    = sid ? self->states[sid - 1].nout : self->ninits;
    tids = malloc(m * sizeof(*tids));
    for (i = 0; i < m; i++) tids[i] = trans_id_from_parts(sid, i);
    if (n) *n = m;

    return tids;
}

size_t smir_state_get_num_actions(StateMachine *self, state_id sid)
{
    return sid ? self->states[sid - 1].nactions : 0;
}

const ActionList *smir_state_get_actions(StateMachine *self, state_id sid)
{
    return sid ? self->states[sid - 1].actions_sentinel : NULL;
}

void smir_state_append_action(StateMachine *self,
                              state_id      sid,
                              const Action *act)
{
    ActionList *al;
    if (sid) {
        al      = malloc(sizeof(*al));
        al->act = act;
        DLL_PUSH_BACK(self->states[sid - 1].actions_sentinel, al);
        self->states[sid - 1].nactions++;
    }
}

void smir_state_prepend_action(StateMachine *self,
                               state_id      sid,
                               const Action *act)
{
    ActionList *al;
    if (sid) {
        al      = malloc(sizeof(*al));
        al->act = act;
        DLL_PUSH_FRONT(self->states[sid - 1].actions_sentinel, al);
        self->states[sid - 1].nactions++;
    }
}

void smir_state_set_actions(StateMachine *self, state_id sid, ActionList *acts)
{
    State *state;

    if (!(sid && acts)) return;

    state = &self->states[sid - 1];
    smir_action_list_free(state->actions_sentinel);
    state->actions_sentinel = acts;

    // recalculate the number of actions
    for (state->nactions = 0, acts                                = acts->next;
         acts != state->actions_sentinel; state->nactions++, acts = acts->next)
        ;
}

ActionList *smir_state_clone_actions(StateMachine *self, state_id sid)
{
    return smir_action_list_clone(smir_state_get_actions(self, sid));
}

state_id smir_get_src(StateMachine *self, trans_id tid)
{
    (void) self;
    return trans_id_sid(tid);
}

state_id smir_get_dst(StateMachine *self, trans_id tid)
{
    Trans   *transition, *transitions;
    state_id sid = trans_id_sid(tid);
    uint32_t idx = trans_id_idx(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    DLL_GET(transitions, idx, transition);

    return transition->dst;
}

void smir_set_dst(StateMachine *self, trans_id tid, state_id dst)
{
    Trans   *transition, *transitions;
    state_id sid = trans_id_sid(tid);
    uint32_t idx = trans_id_idx(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    DLL_GET(transitions, idx, transition);
    transition->dst = dst;
}

size_t smir_trans_get_num_actions(StateMachine *self, trans_id tid)
{
    Trans   *transition, *transitions;
    state_id sid = trans_id_sid(tid);
    uint32_t idx = trans_id_idx(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    DLL_GET(transitions, idx, transition);

    return transition->nactions;
}

const ActionList *smir_trans_get_actions(StateMachine *self, trans_id tid)
{
    Trans   *transition, *transitions;
    state_id sid = trans_id_sid(tid);
    uint32_t idx = trans_id_idx(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    DLL_GET(transitions, idx, transition);

    return transition->actions_sentinel;
}

void smir_trans_append_action(StateMachine *self,
                              trans_id      tid,
                              const Action *act)
{
    Trans      *transition, *transitions;
    ActionList *al  = malloc(sizeof(*al));
    state_id    sid = trans_id_sid(tid);
    uint32_t    idx = trans_id_idx(tid);

    al->act     = act;
    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    DLL_GET(transitions, idx, transition);
    DLL_PUSH_BACK(transition->actions_sentinel, al);
    transition->nactions++;
}

void smir_trans_prepend_action(StateMachine *self,
                               trans_id      tid,
                               const Action *act)
{
    Trans      *transition, *transitions;
    ActionList *al  = malloc(sizeof(*al));
    state_id    sid = trans_id_sid(tid);
    uint32_t    idx = trans_id_idx(tid);

    al->act     = act;
    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    DLL_GET(transitions, idx, transition);
    DLL_PUSH_FRONT(transition->actions_sentinel, al);
    transition->nactions++;
}

void smir_trans_set_actions(StateMachine *self, trans_id tid, ActionList *acts)
{
    Trans   *transition, *transitions;
    state_id sid = trans_id_sid(tid);
    uint32_t idx = trans_id_idx(tid);

    if (!acts) return;

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    DLL_GET(transitions, idx, transition);
    smir_action_list_free(transition->actions_sentinel);
    transition->actions_sentinel = acts;

    // recalculate the number of actions
    for (transition->nactions = 0, acts = acts->next;
         acts != transition->actions_sentinel;
         transition->nactions++, acts = acts->next)
        ;
}

ActionList *smir_trans_clone_actions(StateMachine *self, trans_id tid)
{
    return smir_action_list_clone(smir_trans_get_actions(self, tid));
}

void smir_print(StateMachine *self, FILE *stream)
{
    state_id  sid;
    trans_id *out;
    size_t    n, i;

    if (self->ninits) {
        fprintf(stream, "Initialisation:\n");
        out = smir_get_out_transitions(self, INITIAL_STATE_ID, &n);
        for (i = 0; i < n; i++) {
            fprintf(stream, "  %u: ", smir_get_dst(self, out[i]));
            smir_action_list_print(smir_trans_get_actions(self, out[i]),
                                   stream);
            fprintf(stream, "\n");
        }
        free(out);
    }

    for (sid = 1; sid <= smir_get_num_states(self); sid++) {
        fprintf(stream, "State(%u): ", sid);
        smir_action_list_print(smir_state_get_actions(self, sid), stream);
        fprintf(stream, "\n");

        out = smir_get_out_transitions(self, sid, &n);
        for (i = 0; i < n; i++) {
            fprintf(stream, "  %u: ", smir_get_dst(self, out[i]));
            smir_action_list_print(smir_trans_get_actions(self, out[i]),
                                   stream);
            fprintf(stream, "\n");
        }
        free(out);
    }
}

/* --- Action and ActionList functions -------------------------------------- */

const Action *smir_action_zwa(ActionType type)
{
    Action *act = malloc(sizeof(*act));

    assert(type == ACT_BEGIN || type == ACT_END);
    act->type = type;

    return act;
}

const Action *smir_action_char(const char *ch)
{
    Action *act = malloc(sizeof(*act));

    act->type = ACT_CHAR;
    act->ch   = ch;

    return act;
}

const Action *smir_action_predicate(const Intervals *pred)
{
    Action *act = malloc(sizeof(*act));

    act->type = ACT_PRED;
    act->pred = pred;

    return act;
}

const Action *smir_action_num(ActionType type, size_t k)
{
    Action *act = malloc(sizeof(*act));

    act->type = type;
    act->k    = k;

    return act;
}

const Action *smir_action_clone(const Action *self)
{
    const Action *clone;

    if (!self) return NULL;

    switch (self->type) {
        case ACT_BEGIN: /* fallthrough */
        case ACT_END: clone = smir_action_zwa(self->type); break;

        case ACT_CHAR: clone = smir_action_char(self->ch); break;
        case ACT_PRED:
            clone = smir_action_predicate(intervals_clone(self->pred));
            break;

        case ACT_MEMO:   /* fallthrough */
        case ACT_SAVE:   /* fallthrough */
        case ACT_EPSCHK: /* fallthrough */
        case ACT_EPSSET: clone = smir_action_num(self->type, self->k); break;
    }

    return clone;
}

void smir_action_free(const Action *self)
{
    if (!self) return;

    if (self->type == ACT_PRED) intervals_free((Intervals *) self->pred);
    free((Action *) self);
}

ActionType smir_action_type(const Action *self) { return self->type; }

size_t smir_action_get_num(const Action *self)
{
    return ACT_MEMO <= self->type && self->type <= ACT_EPSSET ? self->k : 0;
}

void smir_action_print(const Action *self, FILE *stream)
{
    char *s;

    switch (self->type) {
        case ACT_BEGIN: fprintf(stream, "begin"); break;
        case ACT_END: fprintf(stream, "end"); break;
        case ACT_CHAR:
            fprintf(stream, "char %.*s", stc_utf8_nbytes(self->ch), self->ch);
            break;
        case ACT_PRED:
            s = intervals_to_str(self->pred);
            fprintf(stream, "pred %s", s);
            free(s);
            break;
        case ACT_MEMO: fprintf(stream, "memo %zu", self->k); break;
        case ACT_SAVE: fprintf(stream, "save %zu", self->k); break;
        case ACT_EPSCHK: fprintf(stream, "epschk %zu", self->k); break;
        case ACT_EPSSET: fprintf(stream, "epsset %zu", self->k); break;
    }
}

ActionList *smir_action_list_new(void)
{
    ActionList *action_list;

    DLL_INIT(action_list);

    return action_list;
}

ActionList *smir_action_list_clone(const ActionList *self)
{
    ActionList *clone;

    DLL_INIT(clone);
    smir_action_list_clone_into(self, clone);

    return clone;
}

void smir_action_list_clone_into(const ActionList *self, ActionList *clone)
{
    ActionList       *al;
    const ActionList *tmp;

    for (tmp = self->next; tmp != self; tmp = tmp->next) {
        al      = malloc(sizeof(*al));
        al->act = smir_action_clone(tmp->act);
        DLL_PUSH_BACK(clone, al);
    }
}

void smir_action_list_clear(ActionList *self)
{
    ActionList *next, *elem;

    for (elem = self->next; elem != self; elem = next) {
        next = elem->next;
        action_list_free(elem);
    }
    self->next = self;
    self->prev = self;
}

void smir_action_list_free(ActionList *self)
{
    ActionList *elem, *next;
    DLL_FREE(self, action_list_free, elem, next);
}

size_t smir_action_list_len(const ActionList *self)
{
    size_t            len;
    const ActionList *iter;

    if (!self) return 0;

    for (len = 0, iter = self->next; iter != self; len++, iter = iter->next)
        ;

    return len;
}

void smir_action_list_push_back(ActionList *self, const Action *act)
{
    ActionList *al = malloc(sizeof(*al));

    al->act = act;
    DLL_PUSH_BACK(self, al);
}

void smir_action_list_push_front(ActionList *self, const Action *act)
{
    ActionList *al = malloc(sizeof(*al));

    al->act = act;
    DLL_PUSH_FRONT(self, al);
}

void smir_action_list_append(ActionList *self, ActionList *acts)
{
    if (!acts || acts->next == acts) return;

    self->prev->next = acts->next;
    acts->next->prev = self->prev;
    acts->prev->next = self;
    self->prev       = acts->prev;
    acts->prev = acts->next = acts;
}

void smir_action_list_prepend(ActionList *self, ActionList *acts)
{
    if (!acts || acts->next == acts) return;

    self->next->prev = acts->prev;
    acts->prev->next = self->next;
    acts->next->prev = self;
    self->next       = acts->next;
    acts->next = acts->prev = acts;
}

ActionListIterator *smir_action_list_iter(const ActionList *self)
{
    ActionListIterator *iter = malloc(sizeof(*iter));

    iter->sentinel = self;
    iter->current  = NULL;

    return iter;
}

const Action *smir_action_list_iterator_next(ActionListIterator *self)
{
    if (self->current == self->sentinel) return NULL;
    self->current = self->current ? self->current->next : self->sentinel->next;
    if (self->current == self->sentinel) return NULL;

    return self->current->act;
}

const Action *smir_action_list_iterator_prev(ActionListIterator *self)
{
    if (self->current == self->sentinel) return NULL;
    self->current = self->current ? self->current->prev : self->sentinel->prev;
    if (self->current == self->sentinel) return NULL;

    return self->current->act;
}

void smir_action_list_iterator_remove(ActionListIterator *self)
{
    ActionList *al;

    if (!self->current || self->current == self->sentinel) return;

    al             = self->current;
    al->prev->next = al->next;
    al->next->prev = al->prev;
    self->current  = al->prev == self->sentinel ? NULL : al->prev;

    al->prev = al->next = NULL;
    smir_action_free(al->act);
    free(al);
}

void smir_action_list_print(const ActionList *self, FILE *stream)
{
    const ActionList *al;

    for (al = self->next; al != self; al = al->next) {
        smir_action_print(al->act, stream);
        if (al->next != self) fprintf(stream, ", ");
    }

    if (self->next == self) fprintf(stream, "-|");
}

/* --- Extendable API ------------------------------------------------------- */

void *smir_set_pre_meta(StateMachine *self, state_id sid, void *meta)
{
    void *old_meta;

    if (!sid) return NULL;

    old_meta                       = self->states[sid - 1].pre_meta;
    self->states[sid - 1].pre_meta = meta;

    return old_meta;
}

void *smir_get_pre_meta(StateMachine *self, state_id sid)
{
    return sid ? self->states[sid - 1].pre_meta : NULL;
}

void *smir_set_post_meta(StateMachine *self, state_id sid, void *meta)
{
    void *old_meta;

    if (!sid) return NULL;

    old_meta                        = self->states[sid - 1].post_meta;
    self->states[sid - 1].post_meta = meta;

    return old_meta;
}

void *smir_get_post_meta(StateMachine *self, state_id sid)
{
    return sid ? self->states[sid - 1].post_meta : NULL;
}

void smir_reorder_states(StateMachine *self, state_id *sid_ordering)
{
    State    *states;
    trans_id *out;
    state_id  sid, dst;
    size_t    i, n, nstates;

    if (!sid_ordering) return;

    // update destinations of transitions
    nstates = smir_get_num_states(self);
    for (sid = 0; sid <= nstates; sid++) {
        out = smir_get_out_transitions(self, sid, &n);
        for (i = 0; i < n; i++) {
            dst = smir_get_dst(self, out[i]);
            smir_set_dst(self, out[i], dst ? sid_ordering[dst - 1] : 0);
        }
        free(out);
    }

    // reorder the states in the states array
    stc_vec_init(states, nstates);
    if (nstates) stc_vec_len_unsafe(states) = nstates;
    for (sid = 1; sid <= nstates; sid++)
        states[sid_ordering[sid - 1] - 1] = self->states[sid - 1];

    stc_vec_free(self->states);
    self->states = states;
}

/* --- SMIR compilation ----------------------------------------------------- */

#define RESERVE(bytes, n)               \
    do {                                \
        stc_vec_reserve(bytes, n);      \
        stc_vec_len_unsafe(bytes) += n; \
    } while (0)

#define PC(insts) ((insts) + stc_vec_len_unsafe(insts))

#define SET_OFFSET(insts, offset_idx, idx)     \
    *((offset_t *) ((insts) + (offset_idx))) = \
        (offset_t) (idx) - ((offset_idx) + sizeof(offset_t))

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    offset_t entry;     /**< where the state is compiled in the program       */
    offset_t exit;      /**< the to-be-filled outgoing transition offsets     */
    size_t transitions; /**< where the transitions start                      */
} StateBlock;

typedef struct {
    regex_id rid; /**< the regex identifier for the mapping                   */
    len_t    idx; /**< the index the regex identifier is mapped to            */
} RidToIdx;

typedef struct {
    RidToIdx *thread_map;           /**< stc_vec for thread mapping           */
    RidToIdx *memoisation_map;      /**< stc_vec for memoisation mapping      */
    len_t     next_thread_idx;      /**< next index for thread map            */
    len_t     next_memoisation_idx; /**< next index for memoisation map       */

    // TODO: counter memory
} MemoryMaps; // map RIDs to memory indices

/* --- Helper function definitions ------------------------------------------ */

static size_t count_bytes_actions(const ActionList *acts)
{
    ActionList *n;
    size_t      size;

    if (!acts) return 0;

    for (n = acts->next, size = 0; n != acts; n = n->next) {
        switch (n->act->type) {
            case ACT_BEGIN: size++; break;
            case ACT_END: size++; break;

            case ACT_CHAR:
                size++;
                size += sizeof(const char *);
                break;

            case ACT_PRED:
                size++;
                size += sizeof(len_t);
                break;

            case ACT_MEMO:
                size++;
                size += sizeof(len_t);
                break;

            case ACT_EPSCHK:
                size++;
                size += sizeof(len_t);
                break;

            case ACT_SAVE:
                size++;
                size += sizeof(len_t);
                break;

            case ACT_EPSSET:
                size++;
                size += sizeof(len_t);
                break;
        }
    }

    return size;
}

static byte *compile_actions(byte             *pc,
                             Program          *prog,
                             const ActionList *acts,
                             MemoryMaps       *mmaps)
{
#define GET_IDX(mmap, next_idx, idx_inc, uid)                      \
    do {                                                           \
        for (idx = 0, len = stc_vec_len_unsafe(mmap);              \
             idx < len && (mmap)[idx].rid != (uid); idx++)         \
            ;                                                      \
        if (idx == len) {                                          \
            idx         = (next_idx);                              \
            (next_idx) += (idx_inc);                               \
            stc_vec_push_back(mmap, ((RidToIdx){ (uid), (idx) })); \
        } else {                                                   \
            idx = (mmap)[idx].idx;                                 \
        }                                                          \
    } while (0)

    ActionList *n;
    size_t      idx, len;

    if (!acts) return pc;

    for (n = acts->next; n != acts; n = n->next) {
        switch (n->act->type) {
            case ACT_BEGIN: BCWRITE(pc, BEGIN); break;
            case ACT_END: BCWRITE(pc, END); break;

            case ACT_CHAR:
                BCWRITE(pc, CHAR);
                MEMWRITE(pc, const char *, n->act->ch);
                break;

            case ACT_PRED:
                BCWRITE(pc, PRED);
                MEMWRITE(pc, len_t, stc_vec_len_unsafe(prog->aux));
                MEMCPY(prog->aux, n->act->pred,
                       sizeof(*n->act->pred) +
                           n->act->pred->len *
                               sizeof(*n->act->pred->intervals));
                break;

            case ACT_MEMO:
                BCWRITE(pc, MEMO);
                GET_IDX(mmaps->memoisation_map, mmaps->next_memoisation_idx, 1,
                        n->act->k);
                MEMWRITE(pc, len_t, idx);
                break;

            case ACT_EPSCHK:
                BCWRITE(pc, EPSCHK);
                GET_IDX(mmaps->thread_map, mmaps->next_thread_idx,
                        sizeof(const char *), n->act->k);
                MEMWRITE(pc, len_t, idx);
                break;

            case ACT_SAVE:
                BCWRITE(pc, SAVE);
                MEMWRITE(pc, len_t, n->act->k);
                if ((n->act->k / 2) + 1 > prog->ncaptures)
                    prog->ncaptures = (n->act->k / 2) + 1;
                break;

            case ACT_EPSSET:
                BCWRITE(pc, EPSSET);
                GET_IDX(mmaps->thread_map, mmaps->next_thread_idx,
                        sizeof(const char *), n->act->k);
                MEMWRITE(pc, len_t, idx);
                break;
        }
    }

    return pc;

#undef GET_IDX
}

static size_t
count_bytes_transition(StateMachine *sm, trans_id tid, int count_jmp)
{
    size_t size;

    size = count_bytes_actions(smir_trans_get_actions(sm, tid));
    if (count_jmp) {
        size++;
        size += sizeof(offset_t);
    }

    return size;
}

static byte *compile_transition(StateMachine *sm,
                                byte         *pc,
                                Program      *prog,
                                trans_id      tid,
                                int           compile_jmp,
                                StateBlock   *state_blocks,
                                MemoryMaps   *mmaps)
{
    const ActionList *acts;
    state_id          dst;
    offset_t          jmp_target_idx;
    offset_t          offset_idx;

    acts = smir_trans_get_actions(sm, tid);
    dst  = smir_get_dst(sm, tid);

    pc = compile_actions(pc, prog, acts, mmaps);

    if (compile_jmp) {
        BCWRITE(pc, JMP);
        offset_idx     = pc - prog->insts;
        jmp_target_idx = IS_FINAL_STATE(dst)
                             ? state_blocks[smir_get_num_states(sm) + 1].entry
                             : state_blocks[dst].entry;
        MEMWRITE(pc, offset_t,
                 jmp_target_idx - (offset_idx + sizeof(offset_t)));
    }

    return pc;
}

static size_t
count_bytes_transitions(StateMachine *sm, trans_id *out, size_t n, state_id sid)
{
    size_t i, nstates, size = 0;
    int    count_jmp;

    nstates = smir_get_num_states(sm);
    for (i = 0; i < n - 1; i++)
        if (smir_trans_get_num_actions(sm, out[i]))
            size += count_bytes_transition(sm, out[i], TRUE);

    // count bytes of last transition
    count_jmp = smir_get_dst(sm, out[i]) != ((sid + 1) % (nstates + 1));
    if (n == 1 || smir_trans_get_num_actions(sm, out[i]))
        size += count_bytes_transition(sm, out[i], count_jmp);

    return size;
}

static void compile_transitions(StateMachine *sm,
                                Program      *prog,
                                state_id      sid,
                                StateBlock   *state_blocks,
                                MemoryMaps   *mmaps)
{
    trans_id *out;
    byte     *pc;
    state_id  dst;
    size_t    n, i, nstates;
    offset_t  offset_idx;
    int       compile_jmp;

    nstates = smir_get_num_states(sm);
    out     = smir_get_out_transitions(sm, sid, &n);
    if (!n) goto cleanup;

    offset_idx = state_blocks[sid].exit;
    pc         = prog->insts + state_blocks[sid].transitions;
    for (i = 0; i < n - 1; offset_idx += sizeof(offset_t), i++) {
        if (smir_trans_get_num_actions(sm, out[i])) {
            SET_OFFSET(prog->insts, offset_idx, pc - prog->insts);
            pc = compile_transition(sm, pc, prog, out[i], TRUE, state_blocks,
                                    mmaps);
        } else {
            dst = smir_get_dst(sm, out[i]);
            if (!dst) dst = nstates + 1;
            SET_OFFSET(prog->insts, offset_idx, state_blocks[dst].entry);
        }
    }

    // compile last transition
    compile_jmp = smir_get_dst(sm, out[i]) != ((sid + 1) % (nstates + 1));
    if (n > 1) {
        if (smir_trans_get_num_actions(sm, out[i])) {
            SET_OFFSET(prog->insts, offset_idx, pc - prog->insts);
            compile_transition(sm, pc, prog, out[i], compile_jmp, state_blocks,
                               mmaps);
        } else {
            dst = smir_get_dst(sm, out[i]);
            if (!dst) dst = nstates + 1;
            SET_OFFSET(prog->insts, offset_idx, state_blocks[dst].entry);
        }
    } else if (n == 1) {
        compile_transition(sm, pc, prog, out[i], compile_jmp, state_blocks,
                           mmaps);
    }

cleanup:
    if (out) free(out);
}

static void compile_state(StateMachine *sm,
                          Program      *prog,
                          state_id      sid,
                          compile_f    *pre,
                          compile_f    *post,
                          StateBlock   *state_blocks,
                          MemoryMaps   *mmaps)
{
    trans_id         *out;
    const ActionList *acts;
    size_t            n, size;

    state_blocks[sid].entry = stc_vec_len_unsafe(prog->insts);

    if (pre) pre(smir_get_pre_meta(sm, sid), prog);
    if (smir_state_get_num_actions(sm, sid)) {
        acts = smir_state_get_actions(sm, sid);
        size = count_bytes_actions(acts);
        RESERVE(prog->insts, size);
        compile_actions(prog->insts + stc_vec_len_unsafe(prog->insts) - size,
                        prog, acts, mmaps);
    }
    if (post) post(smir_get_post_meta(sm, sid), prog);

    out = smir_get_out_transitions(sm, sid, &n);
    switch (n) {
        case 0:
            state_blocks[sid].exit        = 0;
            state_blocks[sid].transitions = stc_vec_len_unsafe(prog->insts);
            goto cleanup;

        case 1:
            state_blocks[sid].exit        = stc_vec_len_unsafe(prog->insts);
            state_blocks[sid].transitions = state_blocks[sid].exit;
            break;

        default:
            if (n == 2) {
                BCPUSH(prog->insts, SPLIT);
            } else {
                BCPUSH(prog->insts, TSWITCH);
                MEMPUSH(prog->insts, len_t, n);
            }
            state_blocks[sid].exit = stc_vec_len_unsafe(prog->insts);
            RESERVE(prog->insts, n * sizeof(offset_t));
            state_blocks[sid].transitions = stc_vec_len_unsafe(prog->insts);
            break;
    }

    size = count_bytes_transitions(sm, out, n, sid);
    if (size) RESERVE(prog->insts, size);

cleanup:
    if (out) free(out);
}

static void compile_initial(StateMachine *sm,
                            Program      *prog,
                            StateBlock   *state_blocks,
                            MemoryMaps   *mmaps)
{
    compile_state(sm, prog, INITIAL_STATE_ID, NULL, NULL, state_blocks, mmaps);
}

/* --- API function --------------------------------------------------------- */

Program *
smir_compile_with_meta(StateMachine *sm, compile_f *pre, compile_f *post)
{
    Program    *prog  = program_default(sm->regex);
    MemoryMaps  mmaps = { 0 };
    StateBlock *state_blocks;
    size_t      n, sid;

    stc_vec_default_init(mmaps.thread_map);
    stc_vec_default_init(mmaps.memoisation_map);

    n            = smir_get_num_states(sm);
    state_blocks = malloc((n + 2) * sizeof(*state_blocks));

    // compile `initial .. states .. final` states
    // and store entry, exit, and transitions offsets
    compile_initial(sm, prog, state_blocks, &mmaps);
    for (sid = 1; sid <= n; sid++)
        compile_state(sm, prog, sid, pre, post, state_blocks, &mmaps);
    state_blocks[sid].entry = stc_vec_len_unsafe(prog->insts);
    state_blocks[sid].exit  = 0;
    BCPUSH(prog->insts, MATCH);

    // compile out transitions for each state
    for (sid = 0; sid <= n; sid++)
        compile_transitions(sm, prog, sid, state_blocks, &mmaps);

    prog->thread_mem_len = mmaps.next_thread_idx;
    prog->nmemo_insts    = stc_vec_len_unsafe(mmaps.memoisation_map);

    // cleanup
    stc_vec_free(mmaps.thread_map);
    stc_vec_free(mmaps.memoisation_map);
    free(state_blocks);

    return prog;
}
