#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../re/sre.h"
#include "glushkov.h"

#define IS_EMPTY(ppl) ((ppl)->sentinel->next == (ppl)->sentinel)

#define FOREACH(elem, sentinel) \
    for ((elem) = (sentinel)->next; (elem) != (sentinel); (elem) = (elem)->next)

#define FOREACH_REV(elem, sentinel) \
    for ((elem) = (sentinel)->prev; (elem) != (sentinel); (elem) = (elem)->prev)

#define IS_EPS_TRANSITION(act) \
    ((act) != NULL && smir_action_type((act)) == ACT_SAVE)

#define APPEND_GAMMA(ppl)                                    \
    do {                                                     \
        pp_insert_pos_after(ppl->sentinel->prev, GAMMA_POS); \
        ppl->gamma = ppl->sentinel->prev;                    \
        ppl->len++;                                          \
    } while (0)

#define PREPEND_GAMMA(ppl)                             \
    do {                                               \
        pp_insert_pos_after(ppl->sentinel, GAMMA_POS); \
        ppl->gamma = ppl->sentinel->next;              \
        ppl->len++;                                    \
    } while (0)

/* START_POS and GAMMA_POS should be the same since gamma isn't an actual
 * position and the start position/state should not follow from others        */
#define START_POS 0
#define GAMMA_POS 0

#define FIRST(rfa)      ((rfa)->follow[START_POS])
#define NULLABLE(first) ((first)->gamma != NULL)

#define RFA_NEW_FROM(rfa) \
    rfa_new((rfa)->follow, (rfa)->npositions, (rfa)->positions)

typedef struct pos_pair PosPair;

struct pos_pair {
    size_t      pos;     /**< linearised position for Glushkov                */
    ActionList *actions; /**< list of actions on transition                   */
    PosPair    *prev;
    PosPair    *next;
};

typedef struct {
    size_t   len;      /**< length of (number of elements in) the list        */
    PosPair *sentinel; /**< sentinel for circly linked list                   */
    PosPair *gamma;    /**< shortcut to gamma pair in linked list             */
} PosPairList;

typedef struct {
    PosPairList  *last;       /**< last *set* for Glushkov                    */
    PosPairList **follow;     /**< integer map of positions to follow lists   */
    size_t        npositions; /**< number of linearised positions             */
    const Action **positions; /**< integer map of positions to actual info    */
} Rfa;

static void     pp_free(PosPair *self);
static PosPair *pp_clone(const PosPair *self);
static void     pp_insert_after(PosPair *self, PosPair *pp);
static PosPair *pp_insert_pos_after(PosPair *self, size_t pos);
static PosPair *pp_remove(PosPair *self);

static PosPairList *ppl_new(void);
static void         ppl_free(PosPairList *self);
static void         ppl_clone_into(const PosPairList *self, PosPairList *clone);
static void         ppl_clear(PosPairList *self);
static void         ppl_remove(PosPairList *self, size_t pos);
static void         ppl_replace_gamma(PosPairList *self, PosPairList *ppl);
static void         ppl_append(PosPairList *self, PosPairList *ppl);

static Rfa *
rfa_new(PosPairList **follow, size_t npositions, const Action **positions);
static void rfa_free(Rfa *self);
static void rfa_construct(Rfa                *self,
                          const RegexNode    *re,
                          PosPairList        *first,
                          const CompilerOpts *opts);
static void rfa_merge_outgoing(Rfa *rfa, size_t pos, len_t *visited);

static size_t count(const RegexNode *re);
static void   emit(StateMachine *sm, const Rfa *rfa);

#if defined(DEBUG) || defined(DEBUG_GLUSHKOV)
static void ppl_print(PosPairList *self, FILE *stream);
static void rfa_print(Rfa *self, FILE *stream);
#else
#    define ppl_print(...)
#    define rfa_print(...)
#endif

/* --- API function definitions --------------------------------------------- */

StateMachine *glushkov_construct(Regex re, const CompilerOpts *opts)
{
    len_t          npositions;
    size_t         i;
    PosPair       *pp;
    PosPairList  **follow;
    const Action **positions;
    len_t         *visited;
    Rfa           *rfa;
    StateMachine  *sm;

    npositions = count(re.root) + 1; /* + 1 so that 0 is gamma/start */
    // NOLINTBEGIN(bugprone-sizeof-expression)
    positions  = malloc(npositions * sizeof(*positions));
    follow     = malloc(npositions * sizeof(*follow));
    // NOLINTEND(bugprone-sizeof-expression)
    visited    = calloc(npositions, sizeof(*visited));
    for (i = 0; i < npositions; i++) follow[i] = ppl_new();

    positions[START_POS] = NULL;
    rfa                  = rfa_new(follow, 1, positions);
    rfa_construct(rfa, re.root, NULL, opts);
    rfa_print(rfa, stderr);
    rfa_merge_outgoing(rfa, START_POS, visited);
    rfa_print(rfa, stderr);

    // npositions - 1 to remove dummy gamma/start state
    sm = smir_new(re.regex, rfa->npositions - 1);
    emit(sm, rfa);

    for (i = 0; i < rfa->npositions; i++) {
        // manually free just ppl and pp containers not actions
        while (follow[i]->sentinel->next != follow[i]->sentinel) {
            pp                        = follow[i]->sentinel->next;
            follow[i]->sentinel->next = pp->next;
            free(pp);
        }

        pp_free(follow[i]->sentinel);
        free(follow[i]);
    }
    rfa_free(rfa);
    free(follow);
    free(positions);
    free(visited);

    return sm;
}

static PosPair *pos_pair_new(size_t pos)
{
    PosPair *pp = malloc(sizeof(*pp));

    pp->pos     = pos;
    pp->actions = smir_action_list_new();
    pp->prev = pp->next = NULL;

    return pp;
}

static void pp_free(PosPair *self)
{
    smir_action_list_free(self->actions);
    self->prev = self->next = NULL;
    free(self);
}

static PosPair *pp_clone(const PosPair *self)
{
    PosPair *pp = malloc(sizeof(*pp));

    pp->pos     = self->pos;
    pp->actions = smir_action_list_clone(self->actions);
    pp->prev = pp->next = NULL;

    return pp;
}

static void pp_insert_after(PosPair *self, PosPair *pp)
{
    pp->prev       = self;
    pp->next       = self->next;
    self->next     = pp;
    pp->next->prev = pp;
}

static PosPair *pp_insert_pos_after(PosPair *self, size_t pos)
{
    PosPair *pp = pos_pair_new(pos);
    pp_insert_after(self, pp);
    return pp;
}

static PosPair *pp_remove(PosPair *self)
{
    PosPair *next = self->next;

    if (self->prev) self->prev->next = self->next;
    if (self->next) self->next->prev = self->prev;
    self->prev = self->next = NULL;

    pp_free(self);
    return next;
}

static PosPairList *ppl_new(void)
{
    PosPairList *ppl = malloc(sizeof(*ppl));

    ppl->len            = 0;
    ppl->sentinel       = pos_pair_new(START_POS);
    ppl->sentinel->prev = ppl->sentinel->next = ppl->sentinel;
    ppl->gamma                                = NULL;

    return ppl;
}

static void ppl_free(PosPairList *self)
{
    ppl_clear(self);
    pp_free(self->sentinel);
    free(self);
}

static void ppl_clone_into(const PosPairList *self, PosPairList *clone)
{
    PosPair *pp;

    clone->len = self->len;
    FOREACH(pp, self->sentinel) {
        pp_insert_after(clone->sentinel->prev, pp_clone(pp));
        if (pp == self->gamma) clone->gamma = clone->sentinel->prev;
    }
}

static void ppl_clear(PosPairList *self)
{
    PosPair *pp;

    while (self->sentinel->next != self->sentinel) {
        pp                   = self->sentinel->next;
        self->sentinel->next = pp->next;
        pp->prev = pp->next = NULL;
        pp_free(pp);
    }

    self->sentinel->prev = self->sentinel->next = self->sentinel;
    self->len                                   = 0;
    self->gamma                                 = NULL;
}

static void ppl_remove(PosPairList *self, size_t pos)
{
    PosPair *pp;

    if (pos == GAMMA_POS) {
        pp = self->gamma ? self->gamma : self->sentinel;
    } else {
        FOREACH(pp, self->sentinel) {
            if (pp->pos == pos) break;
        }
    }

    if (pp == self->sentinel) return;

    pp_remove(pp);
    self->len--;
    if (pos == GAMMA_POS) self->gamma = NULL;
}

static void ppl_replace_gamma(PosPairList *self, PosPairList *ppl)
{
    if (self->gamma == NULL) return;
    if (IS_EMPTY(ppl)) {
        ppl_remove(self, GAMMA_POS);
        return;
    }

    self->gamma->prev->next   = ppl->sentinel->next;
    ppl->sentinel->next->prev = self->gamma->prev;
    ppl->sentinel->prev->next = self->gamma->next;
    self->gamma->next->prev   = ppl->sentinel->prev;
    self->gamma->prev = self->gamma->next  = NULL;
    self->len                             += ppl->len - 1;
    ppl->sentinel->prev = ppl->sentinel->next = ppl->sentinel;
    ppl->len                                  = 0;

    pp_free(self->gamma);
    self->gamma = ppl->gamma;
    ppl->gamma  = NULL;
}

static void ppl_append(PosPairList *self, PosPairList *ppl)
{
    if (IS_EMPTY(ppl)) return;

    self->sentinel->prev->next  = ppl->sentinel->next;
    ppl->sentinel->next->prev   = self->sentinel->prev;
    ppl->sentinel->prev->next   = self->sentinel;
    self->sentinel->prev        = ppl->sentinel->prev;
    self->len                  += ppl->len;
    ppl->sentinel->prev = ppl->sentinel->next = ppl->sentinel;
    ppl->len                                  = 0;

    if (self->gamma == NULL) self->gamma = ppl->gamma;
    ppl->gamma = NULL;
}

static Rfa *
rfa_new(PosPairList **follow, size_t npositions, const Action **positions)
{
    Rfa *rfa = malloc(sizeof(*rfa));

    rfa->last       = ppl_new();
    rfa->follow     = follow;
    rfa->npositions = npositions;
    rfa->positions  = positions;

    return rfa;
}

static void rfa_free(Rfa *self)
{
    ppl_free(self->last);
    free(self);
}

static void rfa_construct(Rfa                *self,
                          const RegexNode    *re,
                          PosPairList        *first,
                          const CompilerOpts *opts)
{
#define APPEND_POSITION(action)                               \
    do {                                                      \
        self->positions[pos = self->npositions++] = (action); \
        PREPEND_GAMMA(self->follow[pos]);                     \
        pp_insert_pos_after(first->sentinel, pos);            \
        first->len++;                                         \
        pp_insert_pos_after(self->last->sentinel, pos);       \
        self->last->len++;                                    \
    } while (0)

    Rfa         *rfa_r2  = NULL;
    PosPairList *ppl_tmp = NULL, *first_r2 = NULL;
    PosPair     *pp, *pp_tmp;
    ActionList  *al_tmp = NULL;
    size_t       pos    = 0;

    if (first == NULL) first = FIRST(self);
    switch (re->type) {
        case EPSILON: APPEND_GAMMA(first); break;
        case CARET:
            APPEND_GAMMA(first);
            smir_action_list_push_back(first->gamma->actions,
                                       smir_action_zwa(ACT_BEGIN));
            break;
        case DOLLAR:
            APPEND_GAMMA(first);
            smir_action_list_push_back(first->gamma->actions,
                                       smir_action_zwa(ACT_END));
            break;
        // case MEMOISE:
        //     APPEND_GAMMA(first);
        //     smir_action_list_push_back(first->gamma->actions,
        //                                smir_action_num(ACT_MEMO, re->rid));
        //     break;
        //
        case LITERAL: APPEND_POSITION(smir_action_char(re->ch)); break;
        case CC:
            APPEND_POSITION(
                smir_action_predicate(intervals_clone(re->intervals)));
            break;

        case ALT:
            rfa_construct(self, re->left, first, opts);
            rfa_r2   = RFA_NEW_FROM(self);
            first_r2 = ppl_new();
            rfa_construct(rfa_r2, re->right, first_r2, opts);
            self->npositions = rfa_r2->npositions;

            if (NULLABLE(first)) ppl_remove(first_r2, GAMMA_POS);
            ppl_append(first, first_r2);
            ppl_append(self->last, rfa_r2->last);

            goto cleanup;

        case CONCAT:
            rfa_construct(self, re->left, first, opts);
            rfa_r2   = RFA_NEW_FROM(self);
            first_r2 = ppl_new();
            rfa_construct(rfa_r2, re->right, first_r2, opts);
            self->npositions = rfa_r2->npositions;

            al_tmp  = smir_action_list_new();
            ppl_tmp = ppl_new();
            FOREACH(pp, self->last->sentinel) {
                ppl_clone_into(first_r2, ppl_tmp);
                FOREACH(pp_tmp, ppl_tmp->sentinel) {
                    smir_action_list_clone_into(pp->actions, al_tmp);
                    smir_action_list_prepend(pp_tmp->actions, al_tmp);
                }
                ppl_replace_gamma(self->follow[pp->pos], ppl_tmp);
            }

            if (NULLABLE(first)) {
                FOREACH(pp_tmp, first_r2->sentinel) {
                    smir_action_list_clone_into(first->gamma->actions, al_tmp);
                    smir_action_list_prepend(pp_tmp->actions, al_tmp);
                }
                ppl_replace_gamma(first, first_r2);
            }

            if (NULLABLE(first_r2)) {
                FOREACH(pp_tmp, self->last->sentinel) {
                    smir_action_list_clone_into(first_r2->gamma->actions,
                                                al_tmp);
                    smir_action_list_append(pp_tmp->actions, al_tmp);
                }
                ppl_append(self->last, rfa_r2->last);
            } else {
                ppl_free(ppl_tmp);
                ppl_tmp      = self->last;
                self->last   = rfa_r2->last;
                rfa_r2->last = ppl_tmp;
                ppl_tmp      = NULL;
            }

            goto cleanup;

        case CAPTURE:
            rfa_construct(self, re->left, first, opts);

            FOREACH(pp_tmp, first->sentinel) {
                smir_action_list_push_front(
                    pp_tmp->actions,
                    smir_action_num(ACT_SAVE, 2 * re->capture_idx));
            }

            if (NULLABLE(first)) {
                smir_action_list_push_back(
                    first->gamma->actions,
                    smir_action_num(ACT_SAVE, 2 * re->capture_idx + 1));
            }

            FOREACH(pp_tmp, self->last->sentinel) {
                smir_action_list_push_back(
                    pp_tmp->actions,
                    smir_action_num(ACT_SAVE, 2 * re->capture_idx + 1));
                if (self->follow[pp_tmp->pos]->gamma) {
                    smir_action_list_push_back(
                        self->follow[pp_tmp->pos]->gamma->actions,
                        smir_action_num(ACT_SAVE, 2 * re->capture_idx + 1));
                }
            }
            break;

        case STAR:
            rfa_construct(self, re->left, first, opts);
            if (re->greedy) {
                if (!NULLABLE(first)) APPEND_GAMMA(first);
            } else {
                ppl_remove(first, GAMMA_POS);
                PREPEND_GAMMA(first);
            }

            al_tmp  = smir_action_list_new();
            ppl_tmp = ppl_new();
            FOREACH(pp, self->last->sentinel) {
                ppl_clone_into(first, ppl_tmp);
                if (opts->capture_semantics == CS_RE2 && NULLABLE(first))
                    smir_action_list_clear(ppl_tmp->gamma->actions);
                FOREACH(pp_tmp, ppl_tmp->sentinel) {
                    smir_action_list_clone_into(pp->actions, al_tmp);
                    smir_action_list_prepend(pp_tmp->actions, al_tmp);
                }
                ppl_replace_gamma(self->follow[pp->pos], ppl_tmp);
            }

            goto cleanup;

        case PLUS:
            rfa_construct(self, re->left, first, opts);

            ppl_tmp = ppl_new();
            al_tmp  = smir_action_list_new();
            FOREACH(pp, self->last->sentinel) {
                ppl_clone_into(first, ppl_tmp);
                if (re->greedy) {
                    if (!NULLABLE(first)) APPEND_GAMMA(ppl_tmp);
                } else {
                    ppl_remove(ppl_tmp, GAMMA_POS);
                    PREPEND_GAMMA(ppl_tmp);
                }
                FOREACH(pp_tmp, ppl_tmp->sentinel) {
                    if (opts->capture_semantics == CS_RE2 && NULLABLE(first))
                        smir_action_list_clear(ppl_tmp->gamma->actions);
                    smir_action_list_clone_into(pp->actions, al_tmp);
                    smir_action_list_prepend(pp_tmp->actions, al_tmp);
                }
                ppl_replace_gamma(self->follow[pp->pos], ppl_tmp);
            }

            goto cleanup;

        case QUES:
            rfa_construct(self, re->left, first, opts);
            if (re->greedy) {
                if (!NULLABLE(first)) APPEND_GAMMA(first);
            } else {
                ppl_remove(first, GAMMA_POS);
                PREPEND_GAMMA(first);
            }
            break;

        /* TODO: */
        case COUNTER:
            // TODO: PCRE/RE2 semantics
        case LOOKAHEAD:
        case BACKREFERENCE: assert(0 && "TODO");
        case NREGEXTYPES: assert(0 && "unreachable");
    }

cleanup:
    if (rfa_r2) rfa_free(rfa_r2);
    if (first_r2) ppl_free(first_r2);
    if (ppl_tmp) ppl_free(ppl_tmp);
    if (al_tmp) smir_action_list_free(al_tmp);

#undef APPEND_POSITION
}

static void rfa_merge_outgoing(Rfa *rfa, size_t pos, len_t *visited)
{
    len_t       *seen;
    PosPair     *t, *e;
    PosPairList *follow = rfa->follow[pos];

    visited[pos] = TRUE;
    FOREACH(t, follow->sentinel)
        if (!visited[t->pos]) rfa_merge_outgoing(rfa, t->pos, visited);

    e    = follow->sentinel->next;
    seen = calloc(rfa->npositions, sizeof(*seen));
    while (e != follow->sentinel) {
        if (seen[e->pos]) {
            e = pp_remove(e);
            follow->len--;
        } else {
            seen[e->pos] = TRUE;
            e            = e->next;
        }
    }
    free(seen);
    visited[pos] = FALSE;
}

static size_t count(const RegexNode *re)
{
    size_t npos = 0;

    switch (re->type) {
        case CARET:  /* fallthrough */
        case DOLLAR: /* fallthrough */
        // case MEMOISE: /* fallthrough */
        case EPSILON: break;

        case LITERAL: /* fallthrough */
        case CC:      /* fallthrough */
        case BACKREFERENCE: npos = 1; break;

        case ALT: /* fallthrough */
        case CONCAT: npos = count(re->left) + count(re->right); break;

        case CAPTURE: npos = /* 2 + */ count(re->left); break;

        case STAR: /* fallthrough */
        case PLUS: /* fallthrough */
        case QUES: npos = count(re->left); break;

        /* TODO: neither work yet */
        case COUNTER: npos = count(re->left); break;
        case LOOKAHEAD: npos = 1 + count(re->left); break;

        case NREGEXTYPES: assert(0 && "unreachable");
    }

    return npos;
}

static void emit(StateMachine *sm, const Rfa *rfa)
{
    PosPair  *pp;
    trans_id  tid;
    size_t    i;
    state_id *pos_map  = calloc(rfa->npositions, sizeof(*pos_map));
    state_id  next_sid = 1;

    pos_map[START_POS] = INITIAL_STATE_ID;
    FOREACH(pp, FIRST(rfa)->sentinel) {
        if (pp->pos == GAMMA_POS) {
            tid = smir_set_final(sm, pos_map[START_POS]);
        } else {
            if (!pos_map[pp->pos]) pos_map[pp->pos] = next_sid++;
            tid = smir_set_initial(sm, pos_map[pp->pos]);
            smir_set_dst(sm, tid, pos_map[pp->pos]);
        }
        smir_trans_set_actions(sm, tid, pp->actions);
    }

    for (i = 1; i < rfa->npositions; i++) {
        if (smir_action_type(rfa->positions[i]) == ACT_SAVE) continue;

        if (!pos_map[i]) pos_map[i] = next_sid++;
        smir_state_append_action(sm, pos_map[i], rfa->positions[i]);
        FOREACH(pp, rfa->follow[i]->sentinel) {
            if (pp->pos == GAMMA_POS) {
                tid = smir_set_final(sm, pos_map[i]);
            } else {
                tid = smir_add_transition(sm, pos_map[i]);
                if (!pos_map[pp->pos]) pos_map[pp->pos] = next_sid++;
                smir_set_dst(sm, tid, pos_map[pp->pos]);
            }
            smir_trans_set_actions(sm, tid, pp->actions);
        }
    }

    free(pos_map);
}

/* --- Debug functions ------------------------------------------------------ */

#if defined(DEBUG) || defined(DEBUG_GLUSHKOV)
static void ppl_print(PosPairList *self, FILE *stream)
{
    PosPair            *pp;
    const Action       *act;
    ActionListIterator *iter;
    size_t              act_idx;

    FOREACH(pp, self->sentinel) {
        fprintf(stream, "%lu (", pp->pos);
        iter = smir_action_list_iter(pp->actions);
        while ((act = smir_action_list_iterator_next(iter))) {
            act_idx = smir_action_get_num(act);
            switch (smir_action_type(act)) {
                case ACT_BEGIN: fprintf(stream, "^"); break;
                case ACT_END: fprintf(stream, "$"); break;
                case ACT_MEMO: fprintf(stream, "#"); break;

                case ACT_SAVE:
                    fprintf(stream, "%c_%lu", act_idx % 2 == 0 ? '[' : ']',
                            act_idx / 2);
                    break;

                default:
                    fprintf(stream, "action type = %d\n",
                            smir_action_type(act));
                    assert(0 && "unreachable");
            }
            fprintf(stream, ",");
        }
        fprintf(stream, ") ");
        free(iter);
    }
}

static void rfa_print(Rfa *rfa, FILE *stream)
{
    size_t i;

    fprintf(stream, "\nRFA FOLLOW SET:\n");
    for (i = 0; i < rfa->npositions; i++) {
        fprintf(stream, "%lu: ", i);
        ppl_print(rfa->follow[i], stream);
        fprintf(stream, "\n");
    }
    fprintf(stream, "LAST: ");
    ppl_print(rfa->last, stream);
    fprintf(stream, "\n");
}
#endif
