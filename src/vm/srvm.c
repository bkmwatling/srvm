#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "program.h"
#include "srvm.h"

/* --- Type definitions ----------------------------------------------------- */

struct srvm {
    ThreadManager *thread_manager;    /**< the thread manager to execute with */
    const Program *program;           /**< the program of the SRVM to execute */
    const char    *curr_sp;           /**< the SP to generate threads from    */
    int            matching_finished; /**< flag to indicate matching is done  */
    len_t          ncaptures; /**< the number of captures in the program      */
    const char   **captures;  /**< the array of (start, end) capture pairs    */
};

/* --- Private function prototypes ------------------------------------------ */

static int srvm_run(SRVM *self, const char *text);

/* --- API function definitions --------------------------------------------- */

SRVM *srvm_new(ThreadManager *thread_manager, const Program *prog)
{
    SRVM *srvm = malloc(sizeof(*srvm));

    srvm->thread_manager    = thread_manager;
    srvm->program           = prog;
    srvm->curr_sp           = NULL;
    srvm->matching_finished = FALSE;
    srvm->ncaptures         = prog->ncaptures;
    srvm->captures          = malloc(2 * srvm->ncaptures * sizeof(char *));
    memset(srvm->captures, 0, 2 * srvm->ncaptures * sizeof(char *));

    return srvm;
}

void srvm_free(SRVM *self)
{
    thread_manager_free(self->thread_manager);
    free(self->captures);
    free(self);
}

int srvm_match(SRVM *self, const char *text)
{
    if (text == NULL) return 0;

    self->curr_sp           = text;
    self->matching_finished = FALSE;
    memset(self->captures, 0, 2 * self->ncaptures * sizeof(char *));
    thread_manager_reset(self->thread_manager);

    return srvm_run(self, text);
}

int srvm_find(SRVM *self, const char *text)
{
    if (text == NULL) return 0;

    if (self->curr_sp == NULL) {
        self->curr_sp           = text;
        self->matching_finished = FALSE;
        thread_manager_reset(self->thread_manager);
    }
    memset(self->captures, 0, 2 * self->ncaptures * sizeof(char *));

    return srvm_run(self, text);
}

StcStringView srvm_capture(SRVM *self, len_t idx)
{
    if (idx >= self->ncaptures) return (StcStringView){ 0, NULL };

    return stc_sv_from_range(self->captures[2 * idx],
                             self->captures[2 * idx + 1]);
}

StcStringView *srvm_captures(SRVM *self, len_t *ncaptures)
{
    len_t          i;
    StcStringView *captures = malloc(self->ncaptures * sizeof(StcStringView));

    if (ncaptures) *ncaptures = self->ncaptures;
    for (i = 0; i < self->ncaptures; i++)
        captures[i] =
            stc_sv_from_range(self->captures[2 * i], self->captures[2 * i + 1]);

    return captures;
}

int srvm_matches(ThreadManager *thread_manager,
                 const Program *prog,
                 const char    *text)
{
    SRVM *srvm    = srvm_new(thread_manager, prog);
    int   matches = srvm_match(srvm, text);
    srvm_free(srvm);

    return matches;
}

/* --- Private function definitions ----------------------------------------- */

static int srvm_run(SRVM *self, const char *text)
{
    void          *null    = NULL;
    int            matched = NO_MATCH, cond;
    const Program *prog    = self->program;
    ThreadManager *tm      = self->thread_manager;
    void          *thread, *t;
    const byte    *pc;
    const char    *sp, *codepoint, *matched_sp;
    len_t          ncaptures, k;
    offset_t       x, y;
    cntr_t         cval, n;
    Intervals     *intervals;

    if (self->matching_finished) return MATCHES_EXHAUSTED;

    thread_manager_init_memoisation(self->thread_manager,
                                    self->program->nmemo_insts, text);
    do {
        thread_manager_init(tm, self->program->insts, self->curr_sp);
        while ((thread = thread_manager_next_thread(tm))) {
            if ((sp = thread_manager_sp(tm, thread)) > text && sp[-1] == '\0') {
                thread_manager_kill_thread(tm, thread);
                continue;
            }

            pc = thread_manager_pc(tm, thread);
            switch (*pc++) {
                case NOOP:
                    thread_manager_set_pc(tm, thread, pc);
                    thread_manager_schedule_thread(tm, thread);
                    break;

                case MATCH:
                    matched_sp = sp;
                    matched    = 1;
                    if (self->captures)
                        memcpy(self->captures,
                               thread_manager_captures(tm, thread, &ncaptures),
                               2 * self->ncaptures * sizeof(char *));
                    thread_manager_notify_thread_match(tm, thread);
                    break;

                case BEGIN:
                    if (sp == text) {
                        thread_manager_set_pc(tm, thread, pc);
                        thread_manager_schedule_thread(tm, thread);
                    } else {
                        thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case END:
                    if (*sp == '\0') {
                        thread_manager_set_pc(tm, thread, pc);
                        thread_manager_schedule_thread(tm, thread);
                    } else {
                        thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case MEMO:
                    MEMREAD(k, pc, len_t);
                    if (thread_manager_memoise(tm, thread, k)) {
                        thread_manager_set_pc(tm, thread, pc);
                        thread_manager_schedule_thread(tm, thread);
                    } else {
                        thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case CHAR:
                    MEMREAD(codepoint, pc, const char *);
                    if (*sp && stc_utf8_cmp(codepoint, sp) == 0) {
                        thread_manager_set_pc(tm, thread, pc);
                        thread_manager_inc_sp(tm, thread);
                        thread_manager_schedule_thread(tm, thread);
                    } else {
                        thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case PRED:
                    MEMREAD(k, pc, len_t);
                    intervals = (Intervals *) (prog->aux + k);
                    if (*sp && intervals_predicate(intervals, sp)) {
                        thread_manager_set_pc(tm, thread, pc);
                        thread_manager_inc_sp(tm, thread);
                        thread_manager_schedule_thread(tm, thread);
                    } else {
                        thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case SAVE:
                    MEMREAD(k, pc, len_t);
                    thread_manager_set_pc(tm, thread, pc);
                    thread_manager_set_capture(tm, thread, k);
                    thread_manager_schedule_thread(tm, thread);
                    break;

                case JMP:
                    MEMREAD(x, pc, offset_t);
                    thread_manager_set_pc(tm, thread, pc + x);
                    thread_manager_schedule_thread(tm, thread);
                    break;

                case SPLIT:
                    t = thread_manager_clone_thread(tm, thread);
                    MEMREAD(x, pc, offset_t);
                    thread_manager_set_pc(tm, thread, pc + x);
                    MEMREAD(y, pc, offset_t);
                    thread_manager_set_pc(tm, t, pc + y);
                    thread_manager_schedule_thread(tm, thread);
                    thread_manager_schedule_thread(tm, t);
                    break;

                /* TODO: */
                case GSPLIT: break;
                case LSPLIT: break;

                case TSWITCH:
                    MEMREAD(k, pc, len_t);
                    // k > 1 to reuse current thread for last offset
                    for (; k > 1; k--) {
                        MEMREAD(x, pc, offset_t);
                        t = thread_manager_clone_thread(tm, thread);
                        thread_manager_set_pc(tm, t, pc + x);
                        thread_manager_schedule_thread_in_order(tm, t);
                    }
                    // reuse current thread
                    MEMREAD(x, pc, offset_t);
                    thread_manager_set_pc(tm, thread, pc + x);
                    thread_manager_schedule_thread_in_order(tm, thread);
                    break;

                case EPSRESET:
                    MEMREAD(k, pc, len_t);
                    thread_manager_set_pc(tm, thread, pc);
                    thread_manager_set_memory(tm, thread, k, &null,
                                              sizeof(null));
                    thread_manager_schedule_thread(tm, thread);
                    break;

                case EPSSET:
                    MEMREAD(k, pc, len_t);
                    thread_manager_set_pc(tm, thread, pc);
                    thread_manager_set_memory(tm, thread, k, &sp, sizeof(sp));
                    thread_manager_schedule_thread(tm, thread);
                    break;

                case EPSCHK:
                    MEMREAD(k, pc, len_t);
                    if (*(char **) thread_manager_memory(tm, thread, k) < sp) {
                        thread_manager_set_pc(tm, thread, pc);
                        thread_manager_schedule_thread(tm, thread);
                    } else {
                        thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case RESET:
                    MEMREAD(k, pc, len_t);
                    MEMREAD(cval, pc, cntr_t);
                    thread_manager_set_pc(tm, thread, pc);
                    thread_manager_set_counter(tm, thread, k, cval);
                    thread_manager_schedule_thread(tm, thread);
                    break;

                case CMP:
                    MEMREAD(k, pc, len_t);
                    MEMREAD(n, pc, cntr_t);
                    cval = thread_manager_counter(tm, thread, k);
                    switch (*pc++) {
                        case LT: cond = (cval < n); break;
                        case LE: cond = (cval <= n); break;
                        case EQ: cond = (cval == n); break;
                        case NE: cond = (cval != n); break;
                        case GE: cond = (cval >= n); break;
                        case GT: cond = (cval > n); break;
                        default: cond = 0; break;
                    }

                    if (cond) {
                        thread_manager_set_pc(tm, thread, pc);
                        thread_manager_schedule_thread(tm, thread);
                    } else {
                        thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case INC:
                    MEMREAD(k, pc, len_t);
                    thread_manager_set_pc(tm, thread, pc);
                    thread_manager_inc_counter(tm, thread, k);
                    thread_manager_schedule_thread(tm, thread);
                    break;

                case ZWA:
                    t = thread_manager_clone_thread(tm, thread);
                    MEMREAD(x, pc, offset_t);
                    thread_manager_set_pc(tm, t, pc + x);
                    MEMREAD(y, pc, offset_t);
                    thread_manager_set_pc(tm, thread, pc + y);
                    // TODO:
                    // s = malloc(sizeof(Scheduler));
                    // scheduler_copy_with(s, scheduler, t);
                    //
                    // if (srvm_run(text, thread_manager, s, NULL) == *pc)
                    //     scheduler_schedule(scheduler, thread);
                    // else
                    //     scheduler_kill(scheduler, thread);
                    // scheduler_free(s);
                    break;

                case STATE:
                    thread_manager_set_pc(tm, thread, pc);
                    thread_manager_schedule_thread(tm, thread);
                    break;

                case NBYTECODES: assert(0 && "unreachable");
            }
        }

        if (thread_manager_done_exec(tm)) {
            self->matching_finished = TRUE;
            break;
        }
        self->curr_sp = matched && matched_sp != self->curr_sp
                            ? matched_sp
                            : stc_utf8_str_next(self->curr_sp);
    } while (!matched);

    return matched;
}
