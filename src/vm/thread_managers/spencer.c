#include <stdlib.h>
#include <string.h>

#include "../../stc/fatp/slice.h"
#include "../../stc/fatp/vec.h"

#include "../../utils.h"
#include "spencer.h"
#include "thread_manager.h"
#include "thread_pool.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct spencer_thread {
    const byte  *pc;
    const char  *sp;
    cntr_t      *counters; /**< stc_slice of counter values                   */
    byte        *memory;   /**< stc_slice for general memory                  */
    const char **captures; /**< stc_slice of capture SPs                      */
} SpencerThread;

typedef struct spencer_scheduler {
    size_t   in_order_idx; /**< the index for inserting threads when in-order */
    Thread  *active;       /**< the active thread for the scheduler           */
    Thread **stack;        /**< stc_vec for thread stack for DFS scheduling   */
} SpencerScheduler;

typedef struct spencer_thread_manager {
    SpencerScheduler *scheduler; /**< the Spencer scheduler for scheduling    */
    ThreadPool       *pool;      /**< the pool of threads                     */
    const char       *sp;        /**< the string pointer for the manager      */

    // for spawning threads
    len_t ncounters;  /**< number of counter values to spawn threads with     */
    len_t memory_len; /**< number bytes allocated for thread general memory   */
    len_t ncaptures;  /**< number of captures to allocate memory in threads   */
} SpencerThreadManager;

/* --- SpencerThreadManager function prototypes ----------------------------- */

static void spencer_thread_manager_init(void       *impl,
                                        const byte *start_pc,
                                        const char *start_sp);
static void spencer_thread_manager_reset(void *impl);
static void spencer_thread_manager_free(void *impl);
static int  spencer_thread_manager_done_exec(void *impl);

static void    spencer_thread_manager_schedule_thread(void *impl, Thread *t);
static void    spencer_thread_manager_schedule_thread_in_order(void   *impl,
                                                               Thread *t);
static Thread *spencer_thread_manager_next_thread(void *impl);
static void spencer_thread_manager_notify_thread_match(void *impl, Thread *t);
static Thread *spencer_thread_manager_clone_thread(void *impl, const Thread *t);
static void    spencer_thread_manager_kill_thread(void *impl, Thread *t);
static const byte *spencer_thread_pc(void *impl, const Thread *t);
static void        spencer_thread_set_pc(void *impl, Thread *t, const byte *pc);
static const char *spencer_thread_sp(void *impl, const Thread *t);
static void        spencer_thread_inc_sp(void *impl, Thread *t);
static cntr_t spencer_thread_counter(void *impl, const Thread *t, len_t idx);
static void
spencer_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val);
static void  spencer_thread_inc_counter(void *impl, Thread *t, len_t idx);
static void *spencer_thread_memory(void *impl, const Thread *t, len_t idx);
static void  spencer_thread_set_memory(void       *impl,
                                       Thread     *t,
                                       len_t       idx,
                                       const void *val,
                                       size_t      size);
static const char *const *
spencer_thread_captures(void *impl, const Thread *t, len_t *ncaptures);
static void spencer_thread_set_capture(void *impl, Thread *t, len_t idx);

/* --- Scheduler function prototypes ---------------------------------------- */

static SpencerScheduler *spencer_scheduler_new(void);
static void spencer_scheduler_schedule(SpencerScheduler *self, Thread *thread);
static void spencer_scheduler_schedule_in_order(SpencerScheduler *self,
                                                Thread           *thread);
static Thread *spencer_scheduler_next(SpencerScheduler *self);
static void    spencer_scheduler_free(SpencerScheduler *self);

/* --- Helper function prototypes ------------------------------------------- */

static SpencerThread             *
spencer_thread_manager_new_thread(SpencerThreadManager *self);
static void spencer_thread_manager_copy_thread(SpencerThreadManager *self,
                                               SpencerThread        *dst,
                                               const SpencerThread  *src);
static SpencerThread             *
spencer_thread_manager_get_thread(SpencerThreadManager *self);
static void spencer_thread_free(Thread *t);

/* --- SpencerThreadManager function definitions ---------------------------- */

ThreadManager *spencer_thread_manager_new(len_t ncounters,
                                          len_t memory_len,
                                          len_t ncaptures,
                                          FILE *logfile)
{
    ThreadManager        *tm  = malloc(sizeof(*tm));
    SpencerThreadManager *stm = malloc(sizeof(*stm));

    stm->scheduler  = spencer_scheduler_new();
    stm->pool       = thread_pool_new(logfile);
    stm->ncounters  = ncounters;
    stm->memory_len = memory_len;
    stm->ncaptures  = ncaptures;

    THREAD_MANAGER_SET_REQUIRED_FUNCS(tm, spencer);

    // TODO: create thread managers for below optional implementations
    tm->counter     = spencer_thread_counter;
    tm->set_counter = spencer_thread_set_counter;
    tm->inc_counter = spencer_thread_inc_counter;
    tm->memory      = spencer_thread_memory;
    tm->set_memory  = spencer_thread_set_memory;
    tm->captures    = spencer_thread_captures;
    tm->set_capture = spencer_thread_set_capture;

    // opt out of memoisation
    tm->init_memoisation = thread_manager_init_memoisation_noop;
    tm->memoise          = thread_manager_memoise_noop;

    tm->impl = stm;

    return tm;
}

static void spencer_thread_manager_init(void       *impl,
                                        const byte *start_pc,
                                        const char *start_sp)
{
    SpencerThreadManager *self = impl;
    SpencerThread        *st   = spencer_thread_manager_get_thread(self);

    self->sp = start_sp;

    st->pc = start_pc;
    st->sp = start_sp;

    memset(st->counters, 0, sizeof(*st->counters) * self->ncounters);
    memset(st->memory, 0, sizeof(*st->memory) * self->memory_len);
    memset(st->captures, 0, sizeof(*st->captures) * 2 * self->ncaptures);

    spencer_thread_manager_schedule_thread(impl, (Thread *) st);
}

static void spencer_thread_manager_reset(void *impl)
{
    Thread           *t;
    SpencerScheduler *ss = ((SpencerThreadManager *) impl)->scheduler;

    ss->in_order_idx = 0;
    while ((t = spencer_scheduler_next(ss)))
        spencer_thread_manager_kill_thread(impl, t);
}

static void spencer_thread_manager_free(void *impl)
{
    SpencerThreadManager *self = impl;

    spencer_thread_manager_reset(impl);
    spencer_scheduler_free(self->scheduler);
    thread_pool_free(self->pool, spencer_thread_free);
    free(impl);
}

static int spencer_thread_manager_done_exec(void *impl)
{
    return *((SpencerThreadManager *) impl)->sp == '\0';
}

static void spencer_thread_manager_schedule_thread(void *impl, Thread *t)
{
    spencer_scheduler_schedule(((SpencerThreadManager *) impl)->scheduler, t);
}

static void spencer_thread_manager_schedule_thread_in_order(void   *impl,
                                                            Thread *t)
{
    spencer_scheduler_schedule_in_order(
        ((SpencerThreadManager *) impl)->scheduler, t);
}

static Thread *spencer_thread_manager_next_thread(void *impl)
{
    return spencer_scheduler_next(((SpencerThreadManager *) impl)->scheduler);
}

static void spencer_thread_manager_notify_thread_match(void *impl, Thread *t)
{
    // empty the scheduler
    spencer_thread_manager_kill_thread(impl, t);
    spencer_thread_manager_reset(impl);
}

static Thread *spencer_thread_manager_clone_thread(void *impl, const Thread *t)
{
    SpencerThreadManager *self = impl;
    SpencerThread        *st   = spencer_thread_manager_get_thread(self);

    spencer_thread_manager_copy_thread(self, st, (SpencerThread *) t);

    return (Thread *) st;
}

static void spencer_thread_manager_kill_thread(void *impl, Thread *t)
{
    thread_pool_add_thread(((SpencerThreadManager *) impl)->pool, t);
}

static const byte *spencer_thread_pc(void *impl, const Thread *t)
{
    UNUSED(impl);
    return t->pc;
}

static void spencer_thread_set_pc(void *impl, Thread *t, const byte *pc)
{
    UNUSED(impl);
    t->pc = pc;
}

static const char *spencer_thread_sp(void *impl, const Thread *t)
{
    UNUSED(impl);
    return t->sp;
}

static void spencer_thread_inc_sp(void *impl, Thread *t)
{
    UNUSED(impl);
    t->sp = stc_utf8_str_next(t->sp);
}

static cntr_t spencer_thread_counter(void *impl, const Thread *t, len_t idx)
{
    UNUSED(impl);
    return ((SpencerThread *) t)->counters[idx];
}

static void
spencer_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val)
{
    UNUSED(impl);
    ((SpencerThread *) t)->counters[idx] = val;
}

static void spencer_thread_inc_counter(void *impl, Thread *t, len_t idx)
{
    UNUSED(impl);
    ((SpencerThread *) t)->counters[idx]++;
}

static void *spencer_thread_memory(void *impl, const Thread *t, len_t idx)
{
    UNUSED(impl);
    return ((SpencerThread *) t)->memory + idx;
}

static void spencer_thread_set_memory(void       *self,
                                      Thread     *t,
                                      len_t       idx,
                                      const void *val,
                                      size_t      size)
{
    UNUSED(self);
    memcpy(((SpencerThread *) t)->memory + idx, val, size);
}

static const char *const *
spencer_thread_captures(void *impl, const Thread *t, len_t *ncaptures)
{
    UNUSED(impl);
    if (ncaptures)
        *ncaptures = stc_slice_len(((SpencerThread *) t)->captures) / 2;
    return ((SpencerThread *) t)->captures;
}

static void spencer_thread_set_capture(void *impl, Thread *t, len_t idx)
{
    UNUSED(impl);
    ((SpencerThread *) t)->captures[idx] = ((SpencerThread *) t)->sp;
}

/* --- SpencerScheduler function definitions -------------------------------- */

static SpencerScheduler *spencer_scheduler_new(void)
{
    SpencerScheduler *ss = malloc(sizeof(*ss));

    ss->in_order_idx = 0;
    ss->active       = NULL;
    stc_vec_default_init(ss->stack); // NOLINT(bugprone-sizeof-expression)

    return ss;
}

static void spencer_scheduler_schedule(SpencerScheduler *self, Thread *thread)
{
    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    if (self->active)
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(self->stack, thread);
    else
        self->active = thread;
}

static void spencer_scheduler_schedule_in_order(SpencerScheduler *self,
                                                Thread           *thread)
{
    size_t len = stc_vec_len_unsafe(self->stack);

    if (self->in_order_idx > len) {
        spencer_scheduler_schedule(self, thread);
        self->in_order_idx = len;
    } else if (self->in_order_idx == len) {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(self->stack, thread);
    } else {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_insert(self->stack, self->in_order_idx, thread);
    }
}

static Thread *spencer_scheduler_next(SpencerScheduler *self)
{
    SpencerThread *thread = (SpencerThread *) self->active;

    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    self->active       = NULL;
    if (thread == NULL && !stc_vec_is_empty(self->stack))
        thread = (SpencerThread *) stc_vec_pop(self->stack);

    return (Thread *) thread;
}

static void spencer_scheduler_free(SpencerScheduler *self)
{
    stc_vec_free(self->stack);
    free(self);
}

/* --- Helper functions ----------------------------------------------------- */

static SpencerThread *
spencer_thread_manager_new_thread(SpencerThreadManager *self)
{
    SpencerThread *st = malloc(sizeof(*st));

    stc_slice_init(st->memory, self->memory_len);
    stc_slice_init(st->captures, 2 * self->ncaptures);
    stc_slice_init(st->counters, self->ncounters);

    return st;
}

static void spencer_thread_manager_copy_thread(SpencerThreadManager *self,
                                               SpencerThread        *dst,
                                               const SpencerThread  *src)
{
    dst->pc = src->pc;
    dst->sp = src->sp;
    memcpy(dst->memory, src->memory, sizeof(*dst->memory) * self->memory_len);
    memcpy(dst->captures, src->captures,
           sizeof(*dst->captures) * 2 * self->ncaptures);
    memcpy(dst->counters, src->counters,
           sizeof(*dst->counters) * self->ncounters);
}

static SpencerThread *
spencer_thread_manager_get_thread(SpencerThreadManager *self)
{
    SpencerThread *st = (SpencerThread *) thread_pool_get_thread(self->pool);

    if (!st) st = spencer_thread_manager_new_thread(self);

    return st;
}

static void spencer_thread_free(Thread *t)
{
    SpencerThread *st = (SpencerThread *) t;

    stc_slice_free(st->memory);
    stc_slice_free(st->counters);
    stc_slice_free(st->captures);
    free(st);
}
