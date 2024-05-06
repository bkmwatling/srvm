#include <stdlib.h>
#include <string.h>

#include "benchmark.h"

#define INST_COUNT_LEN               (2 * NBYTECODES)
#define INST_IDX(i)                  (2 * (i))
#define INST_COUNT(self, i)          (self)->inst_counts[INST_IDX(i)]
#define INC_INST_COUNT(self, i)      INST_COUNT(self, i)++
#define INST_FAIL_COUNT(self, i)     (self)->inst_counts[INST_IDX(i) + 1]
#define INC_INST_FAIL_COUNT(self, i) INST_FAIL_COUNT(self, i)++
#define CURR_INST(t)                 *(t)->pc
#define INC_CURR_INST(self, t)       INC_INST_COUNT(self, CURR_INST(t))
#define INC_CURR_INST_FAIL(self, t)  INC_INST_FAIL_COUNT(self, CURR_INST(t))
#define LOG_INSTS(self, i)                                                    \
    fprintf((self)->logfile, #i ": %lu (FAILED: %lu)\n", INST_COUNT(self, i), \
            INST_FAIL_COUNT(self, i))

typedef struct {
    FILE *logfile; /**< the log file stream to print benchmark information to */

    // TODO: use pointers to facilitate shared counting when cloning
    size_t spawn_count;                 /**< the number of spawned threads    */
    size_t inst_counts[INST_COUNT_LEN]; /**< instruction execution counts     */

    ThreadManager *__manager; /**< the thread manager being wrapped           */
} BenchmarkThreadManager;

/* --- BenchmarkThreadManager function prototypes --------------------------- */

static void benchmark_thread_manager_init(void       *impl,
                                          const byte *start_pc,
                                          const char *start_sp);
static void benchmark_thread_manager_reset(void *impl);
static void benchmark_thread_manager_free(void *impl);
static int  benchmark_thread_manager_done_exec(void *impl);

static void    benchmark_thread_manager_schedule_thread(void *impl, Thread *t);
static void    benchmark_thread_manager_schedule_thread_in_order(void   *impl,
                                                                 Thread *t);
static Thread *benchmark_thread_manager_next_thread(void *impl);
static void benchmark_thread_manager_notify_thread_match(void *impl, Thread *t);
static Thread *benchmark_thread_manager_clone_thread(void         *impl,
                                                     const Thread *t);
static void    benchmark_thread_manager_kill_thread(void *impl, Thread *t);
static void    benchmark_thread_manager_init_memoisation(void       *impl,
                                                         size_t      nmemo_insts,
                                                         const char *text);

static const byte *benchmark_thread_pc(void *impl, const Thread *t);
static void benchmark_thread_set_pc(void *impl, Thread *t, const byte *pc);
static const char *benchmark_thread_sp(void *impl, const Thread *t);
static void        benchmark_thread_inc_sp(void *impl, Thread *t);
static int         benchmark_thread_memoise(void *impl, Thread *t, len_t idx);
static cntr_t benchmark_thread_counter(void *impl, const Thread *t, len_t idx);
static void
benchmark_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val);
static void  benchmark_thread_inc_counter(void *impl, Thread *t, len_t idx);
static void *benchmark_thread_memory(void *impl, const Thread *t, len_t idx);
static void  benchmark_thread_set_memory(void       *impl,
                                         Thread     *t,
                                         len_t       idx,
                                         const void *val,
                                         size_t      size);
static const char *const *
benchmark_thread_captures(void *impl, const Thread *t, len_t *ncaptures);
static void benchmark_thread_set_capture(void *impl, Thread *t, len_t idx);

/* --- API function definitions --------------------------------------------- */

ThreadManager *benchmark_thread_manager_new(ThreadManager *thread_manager,
                                            FILE          *logfile)
{
    BenchmarkThreadManager *btm = malloc(sizeof(*btm));
    ThreadManager          *tm  = malloc(sizeof(*tm));

    btm->logfile     = logfile ? logfile : stderr;
    btm->spawn_count = 0;
    memset(btm->inst_counts, 0, sizeof(btm->inst_counts));
    btm->__manager = thread_manager;

    THREAD_MANAGER_SET_ALL_FUNCS(tm, benchmark);
    tm->impl = btm;

    return tm;
}

/* --- BenchmarkThreadManager function defintions --------------------------- */

static void benchmark_thread_manager_init(void       *impl,
                                          const byte *start_pc,
                                          const char *start_sp)
{
    thread_manager_init(((BenchmarkThreadManager *) impl)->__manager, start_pc,
                        start_sp);
}

static void benchmark_thread_manager_reset(void *impl)
{
    BenchmarkThreadManager *self = impl;
    thread_manager_reset(self->__manager);
    // TODO: reset benchmark instrumentation?
    // self->spawn_count = 0;
    // memset(self->inst_counts, 0, sizeof(self->inst_counts));
}

static void benchmark_thread_manager_free(void *impl)
{
    BenchmarkThreadManager *self = impl;

    LOG_INSTS(self, MATCH);
    LOG_INSTS(self, MEMO);
    LOG_INSTS(self, CHAR);
    LOG_INSTS(self, PRED);
    LOG_INSTS(self, STATE);

    thread_manager_free(self->__manager);
    free(impl);
}

static int benchmark_thread_manager_done_exec(void *impl)
{
    return thread_manager_done_exec(
        ((BenchmarkThreadManager *) impl)->__manager);
}

static void benchmark_thread_manager_schedule_thread(void *impl, Thread *t)
{
    thread_manager_schedule_thread(((BenchmarkThreadManager *) impl)->__manager,
                                   t);
}

static void benchmark_thread_manager_schedule_thread_in_order(void   *impl,
                                                              Thread *t)
{
    thread_manager_schedule_thread_in_order(
        ((BenchmarkThreadManager *) impl)->__manager, t);
}

static Thread *benchmark_thread_manager_next_thread(void *impl)
{
    BenchmarkThreadManager *self = impl;
    Thread                 *t    = thread_manager_next_thread(self->__manager);

    if (t) INC_CURR_INST(self, t);

    return t;
}

static void benchmark_thread_manager_notify_thread_match(void *impl, Thread *t)
{
    thread_manager_notify_thread_match(
        ((BenchmarkThreadManager *) impl)->__manager, t);
}

static Thread *benchmark_thread_manager_clone_thread(void         *impl,
                                                     const Thread *t)
{
    return thread_manager_clone_thread(
        ((BenchmarkThreadManager *) impl)->__manager, t);
}

static void benchmark_thread_manager_kill_thread(void *impl, Thread *t)
{
    BenchmarkThreadManager *self = impl;

    INC_CURR_INST_FAIL(self, t);
    thread_manager_kill_thread(self->__manager, t);
}

static const byte *benchmark_thread_pc(void *impl, const Thread *t)
{
    return thread_manager_pc(((BenchmarkThreadManager *) impl)->__manager, t);
}

static void benchmark_thread_set_pc(void *impl, Thread *t, const byte *pc)
{
    thread_manager_set_pc(((BenchmarkThreadManager *) impl)->__manager, t, pc);
}

static const char *benchmark_thread_sp(void *impl, const Thread *t)
{
    return thread_manager_sp(((BenchmarkThreadManager *) impl)->__manager, t);
}

static void benchmark_thread_inc_sp(void *impl, Thread *t)
{
    thread_manager_inc_sp(((BenchmarkThreadManager *) impl)->__manager, t);
}

static void benchmark_thread_manager_init_memoisation(void       *impl,
                                                      size_t      nmemo_insts,
                                                      const char *text)
{
    thread_manager_init_memoisation(
        ((BenchmarkThreadManager *) impl)->__manager, nmemo_insts, text);
}

static int benchmark_thread_memoise(void *impl, Thread *t, len_t idx)
{
    return thread_manager_memoise(((BenchmarkThreadManager *) impl)->__manager,
                                  t, idx);
}

static cntr_t benchmark_thread_counter(void *impl, const Thread *t, len_t idx)
{
    return thread_manager_counter(((BenchmarkThreadManager *) impl)->__manager,
                                  t, idx);
}

static void
benchmark_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val)
{
    thread_manager_set_counter(((BenchmarkThreadManager *) impl)->__manager, t,
                               idx, val);
}

static void benchmark_thread_inc_counter(void *impl, Thread *t, len_t idx)
{
    thread_manager_inc_counter(((BenchmarkThreadManager *) impl)->__manager, t,
                               idx);
}

static void *benchmark_thread_memory(void *impl, const Thread *t, len_t idx)
{
    return thread_manager_memory(((BenchmarkThreadManager *) impl)->__manager,
                                 t, idx);
}

static void benchmark_thread_set_memory(void       *impl,
                                        Thread     *t,
                                        len_t       idx,
                                        const void *val,
                                        size_t      size)
{
    thread_manager_set_memory(((BenchmarkThreadManager *) impl)->__manager, t,
                              idx, val, size);
}

static const char *const *
benchmark_thread_captures(void *impl, const Thread *t, len_t *ncaptures)
{
    return thread_manager_captures(((BenchmarkThreadManager *) impl)->__manager,
                                   t, ncaptures);
}

static void benchmark_thread_set_capture(void *impl, Thread *t, len_t idx)
{
    thread_manager_set_capture(((BenchmarkThreadManager *) impl)->__manager, t,
                               idx);
}

#undef INST_COUNT_LEN
#undef DECL_INST_COUNT
#undef INST_IDX
#undef INST_COUNT
#undef INC_INST_COUNT
#undef INST_FAILURE_COUNT
#undef INC_INST_FAILURE_COUNT
#undef INC_CURR_INST
#undef LOG_INSTS
