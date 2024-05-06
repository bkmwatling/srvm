#include <stdlib.h>
#include <string.h>

#include "memoisation.h"

typedef struct {
    byte       *memoisation_memory; /**< memory used for memoisation          */
    const char *text;               /**< input string being matched against   */
    size_t      text_len;           /**< length of the input string           */

    ThreadManager *__manager; /**< the thread manager being wrapped           */
} MemoisedThreadManager;

/* --- MemoisedThreadManager function prototypes ---------------------------- */

static void memoised_thread_manager_init(void       *impl,
                                         const byte *start_pc,
                                         const char *start_sp);
static void memoised_thread_manager_reset(void *impl);
static void memoised_thread_manager_free(void *impl);
static int  memoised_thread_manager_done_exec(void *impl);

static void    memoised_thread_manager_schedule_thread(void *impl, Thread *t);
static void    memoised_thread_manager_schedule_thread_in_order(void   *impl,
                                                                Thread *t);
static Thread *memoised_thread_manager_next_thread(void *impl);
static void memoised_thread_manager_notify_thread_match(void *impl, Thread *t);
static Thread *memoised_thread_manager_clone_thread(void         *impl,
                                                    const Thread *t);
static void    memoised_thread_manager_kill_thread(void *impl, Thread *t);
static void    memoised_thread_manager_init_memoisation(void       *impl,
                                                        size_t      nmemo_insts,
                                                        const char *text);

static const byte *memoised_thread_pc(void *impl, const Thread *t);
static void memoised_thread_set_pc(void *impl, Thread *t, const byte *pc);
static const char *memoised_thread_sp(void *impl, const Thread *t);
static void        memoised_thread_inc_sp(void *impl, Thread *t);
static int         memoised_thread_memoise(void *impl, Thread *t, len_t idx);
static cntr_t memoised_thread_counter(void *impl, const Thread *t, len_t idx);
static void
memoised_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val);
static void  memoised_thread_inc_counter(void *impl, Thread *t, len_t idx);
static void *memoised_thread_memory(void *impl, const Thread *t, len_t idx);
static void  memoised_thread_set_memory(void       *impl,
                                        Thread     *t,
                                        len_t       idx,
                                        const void *val,
                                        size_t      size);
static const char *const *
memoised_thread_captures(void *impl, const Thread *t, len_t *ncaptures);
static void memoised_thread_set_capture(void *impl, Thread *t, len_t idx);

/* --- API function definitions --------------------------------------------- */

ThreadManager *memoised_thread_manager_new(ThreadManager *thread_manager)
{
    MemoisedThreadManager *mtm = malloc(sizeof(*mtm));
    ThreadManager         *tm  = malloc(sizeof(*tm));

    mtm->memoisation_memory = NULL;
    mtm->text_len           = 0;
    mtm->text_len           = 0;
    mtm->__manager          = thread_manager;

    THREAD_MANAGER_SET_ALL_FUNCS(tm, memoised);
    tm->impl = mtm;

    return tm;
}

/* --- MemoisedThreadManager function definitions --------------------------- */

static void memoised_thread_manager_init(void       *impl,
                                         const byte *start_pc,
                                         const char *start_sp)
{
    thread_manager_init(((MemoisedThreadManager *) impl)->__manager, start_pc,
                        start_sp);
}

static void memoised_thread_manager_reset(void *impl)
{
    MemoisedThreadManager *self = impl;
    thread_manager_reset(self->__manager);
    if (self->memoisation_memory)
        memset(self->memoisation_memory, 0,
               self->text_len * self->text_len *
                   sizeof(*self->memoisation_memory));
}

static void memoised_thread_manager_free(void *impl)
{
    free(((MemoisedThreadManager *) impl)->memoisation_memory);
    thread_manager_free(((MemoisedThreadManager *) impl)->__manager);
    free(impl);
}

static int memoised_thread_manager_done_exec(void *impl)
{
    return thread_manager_done_exec(
        ((MemoisedThreadManager *) impl)->__manager);
}

static void memoised_thread_manager_schedule_thread(void *impl, Thread *t)
{
    thread_manager_schedule_thread(((MemoisedThreadManager *) impl)->__manager,
                                   t);
}

static void memoised_thread_manager_schedule_thread_in_order(void   *impl,
                                                             Thread *t)
{
    thread_manager_schedule_thread_in_order(
        ((MemoisedThreadManager *) impl)->__manager, t);
}

static Thread *memoised_thread_manager_next_thread(void *impl)
{
    return thread_manager_next_thread(
        ((MemoisedThreadManager *) impl)->__manager);
}

static void memoised_thread_manager_notify_thread_match(void *impl, Thread *t)
{
    thread_manager_notify_thread_match(
        ((MemoisedThreadManager *) impl)->__manager, t);
}

static Thread *memoised_thread_manager_clone_thread(void *impl, const Thread *t)
{
    return thread_manager_clone_thread(
        ((MemoisedThreadManager *) impl)->__manager, t);
}

static void memoised_thread_manager_kill_thread(void *impl, Thread *t)
{
    thread_manager_kill_thread(((MemoisedThreadManager *) impl)->__manager, t);
}

static const byte *memoised_thread_pc(void *impl, const Thread *t)
{
    return thread_manager_pc(((MemoisedThreadManager *) impl)->__manager, t);
}

static void memoised_thread_set_pc(void *impl, Thread *t, const byte *pc)
{
    thread_manager_set_pc(((MemoisedThreadManager *) impl)->__manager, t, pc);
}

static const char *memoised_thread_sp(void *impl, const Thread *t)
{
    return thread_manager_sp(((MemoisedThreadManager *) impl)->__manager, t);
}

static void memoised_thread_inc_sp(void *impl, Thread *t)
{
    thread_manager_inc_sp(((MemoisedThreadManager *) impl)->__manager, t);
}

static void memoised_thread_manager_init_memoisation(void       *impl,
                                                     size_t      nmemo_insts,
                                                     const char *text)
{
    MemoisedThreadManager *self = impl;

    if (self->memoisation_memory) free(self->memoisation_memory);

    self->text               = text;
    self->text_len           = strlen(text) + 1;
    self->memoisation_memory = malloc(nmemo_insts * self->text_len *
                                      sizeof(*self->memoisation_memory));
    memset(self->memoisation_memory, FALSE,
           nmemo_insts * self->text_len * sizeof(*self->memoisation_memory));
}

static int memoised_thread_memoise(void *impl, Thread *t, len_t idx)
{
    MemoisedThreadManager *self = impl;
    size_t                 i    = idx * self->text_len + (t->sp - self->text);

    if (self->memoisation_memory[i]) return FALSE;
    self->memoisation_memory[i] = TRUE;

    return TRUE;
}

static cntr_t memoised_thread_counter(void *impl, const Thread *t, len_t idx)
{
    return thread_manager_counter(((MemoisedThreadManager *) impl)->__manager,
                                  t, idx);
}

static void
memoised_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val)
{
    thread_manager_set_counter(((MemoisedThreadManager *) impl)->__manager, t,
                               idx, val);
}

static void memoised_thread_inc_counter(void *impl, Thread *t, len_t idx)
{
    thread_manager_inc_counter(((MemoisedThreadManager *) impl)->__manager, t,
                               idx);
}

static void *memoised_thread_memory(void *impl, const Thread *t, len_t idx)
{
    return thread_manager_memory(((MemoisedThreadManager *) impl)->__manager, t,
                                 idx);
}

static void memoised_thread_set_memory(void       *impl,
                                       Thread     *t,
                                       len_t       idx,
                                       const void *val,
                                       size_t      size)
{
    thread_manager_set_memory(((MemoisedThreadManager *) impl)->__manager, t,
                              idx, val, size);
}

static const char *const *
memoised_thread_captures(void *impl, const Thread *t, len_t *ncaptures)
{
    return thread_manager_captures(((MemoisedThreadManager *) impl)->__manager,
                                   t, ncaptures);
}

static void memoised_thread_set_capture(void *impl, Thread *t, len_t idx)
{
    thread_manager_set_capture(((MemoisedThreadManager *) impl)->__manager, t,
                               idx);
}
