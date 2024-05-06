#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

/**
 * The thread manager represents an interface for thread manipulation during
 * VM execution.
 *
 * In general, a thread manager should be able to create/kill threads, edit
 * thread memory, and provide a way for iterating over threads.
 *
 * The latter is generally thought to happen through the use of a scheduler.
 * Hence, we define both an interface for scheduling and managing threads here.
 * Each thread manager should have a scheduler it works with as well.
 *
 * The benefits of the below interfaces is it becomes easy to build a thread
 * manager (scheduler) from individual thread managers (schedulers). For
 * example, we do not have to define a memoisation variant for every thread
 * manager. Instead, we define a memoisation thread manager that wraps any
 * underlying thread manager.
 */

#include "../../types.h"
#include "../program.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define thread_manager_init(manager, start_pc, start_sp) \
    (manager)->init((manager)->impl, (start_pc), (start_sp))
#define thread_manager_reset(manager) (manager)->reset((manager)->impl)
#define thread_manager_free(manager)      \
    do {                                  \
        (manager)->free((manager)->impl); \
        free((manager));                  \
    } while (0)
#define thread_manager_done_exec(manager) (manager)->done_exec((manager)->impl)

#define thread_manager_schedule_thread(manager, thread) \
    (manager)->schedule_thread((manager)->impl, (thread))
#define thread_manager_schedule_thread_in_order(manager, thread) \
    (manager)->schedule_thread_in_order((manager)->impl, (thread))
#define thread_manager_next_thread(manager) \
    (manager)->next_thread((manager)->impl)
#define thread_manager_notify_thread_match(manager, thread) \
    (manager)->notify_thread_match((manager)->impl, (thread))
#define thread_manager_clone_thread(manager, thread) \
    (manager)->clone_thread((manager)->impl, (thread))
#define thread_manager_kill_thread(manager, thread) \
    (manager)->kill_thread((manager)->impl, (thread))

#define thread_manager_pc(manager, thread) \
    (manager)->pc((manager)->impl, (thread))
#define thread_manager_set_pc(manager, thread, pc) \
    (manager)->set_pc((manager)->impl, (thread), (pc))
#define thread_manager_sp(manager, thread) \
    (manager)->sp((manager)->impl, (thread))
#define thread_manager_inc_sp(manager, thread) \
    (manager)->inc_sp((manager)->impl, (thread))

#define thread_manager_init_memoisation(manager, nmemo, text_len) \
    (manager)->init_memoisation((manager)->impl, (nmemo), (text_len));
#define thread_manager_memoise(manager, thread, idx) \
    (manager)->memoise((manager)->impl, (thread), (idx))
#define thread_manager_counter(manager, thread, idx) \
    (manager)->counter((manager)->impl, (thread), (idx))
#define thread_manager_set_counter(manager, thread, idx, val) \
    (manager)->set_counter((manager)->impl, (thread), (idx), (val))
#define thread_manager_inc_counter(manager, thread, idx) \
    (manager)->inc_counter((manager)->impl, (thread), (idx))
#define thread_manager_memory(manager, thread, idx) \
    (manager)->memory((manager)->impl, (thread), (idx))
#define thread_manager_set_memory(manager, thread, idx, val, size) \
    (manager)->set_memory((manager)->impl, (thread), (idx), (val), (size))
#define thread_manager_captures(manager, thread, ncaptures) \
    (manager)->captures((manager)->impl, (thread), (ncaptures))
#define thread_manager_set_capture(manager, thread, idx) \
    (manager)->set_capture((manager)->impl, (thread), (idx))

#define THREAD_MANAGER_SET_REQUIRED_FUNCS(manager, prefix)                    \
    do {                                                                      \
        (manager)->init      = prefix##_thread_manager_init;                  \
        (manager)->reset     = prefix##_thread_manager_reset;                 \
        (manager)->free      = prefix##_thread_manager_free;                  \
        (manager)->done_exec = prefix##_thread_manager_done_exec;             \
                                                                              \
        (manager)->schedule_thread = prefix##_thread_manager_schedule_thread; \
        (manager)->schedule_thread_in_order =                                 \
            prefix##_thread_manager_schedule_thread_in_order;                 \
        (manager)->next_thread = prefix##_thread_manager_next_thread;         \
        (manager)->notify_thread_match =                                      \
            prefix##_thread_manager_notify_thread_match;                      \
        (manager)->clone_thread = prefix##_thread_manager_clone_thread;       \
        (manager)->kill_thread  = prefix##_thread_manager_kill_thread;        \
                                                                              \
        (manager)->pc     = prefix##_thread_pc;                               \
        (manager)->set_pc = prefix##_thread_set_pc;                           \
        (manager)->sp     = prefix##_thread_sp;                               \
        (manager)->inc_sp = prefix##_thread_inc_sp;                           \
    } while (0)

#define THREAD_MANAGER_SET_ALL_FUNCS(manager, prefix)         \
    do {                                                      \
        THREAD_MANAGER_SET_REQUIRED_FUNCS(manager, prefix);   \
                                                              \
        (manager)->init_memoisation =                         \
            prefix##_thread_manager_init_memoisation;         \
        (manager)->memoise     = prefix##_thread_memoise;     \
        (manager)->counter     = prefix##_thread_counter;     \
        (manager)->set_counter = prefix##_thread_set_counter; \
        (manager)->inc_counter = prefix##_thread_inc_counter; \
        (manager)->memory      = prefix##_thread_memory;      \
        (manager)->set_memory  = prefix##_thread_set_memory;  \
        (manager)->captures    = prefix##_thread_captures;    \
        (manager)->set_capture = prefix##_thread_set_capture; \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

// the basic thread represent a state in the VM:
// some pointer into the instruction stream, and a pointer into the matching
// string.
//
// Any extensions of this (captures, counters, etc) can happen, but the datatype
// must adhere to this spec.
typedef struct {
    const byte *pc; /**< the program counter of the thread                    */
    const char *sp; /**< the string pointer of the thread                     */
} Thread;

typedef struct thread_manager {
    void (*init)(void       *thread_manager_impl,
                 const byte *start_pc,
                 const char *start_sp);
    void (*reset)(void *thread_manager_impl);
    void (*free)(void *thread_manager_impl); /**< free the thread manager     */
    int  (*done_exec)(void *thread_manager_impl);

    // below functions manipulate thread execution
    void (*schedule_thread)(void *thread_manager_impl, Thread *thread);
    void (*schedule_thread_in_order)(void *thread_manager_impl, Thread *thread);
    Thread *(*next_thread)(void *thread_manager_impl);
    void    (*notify_thread_match)(void *thread_manager_impl, Thread *thread);
    Thread *(*clone_thread)(void *thread_manager_impl, const Thread *thread);
    void    (*kill_thread)(void *thread_manager_impl, Thread *thread);

    // functions that manipulate thread memory
    const byte *(*pc)(void *thread_manager_impl, const Thread *thread);
    void (*set_pc)(void *thread_manager_impl, Thread *thread, const byte *pc);
    const char *(*sp)(void *thread_manager_impl, const Thread *thread);
    void        (*inc_sp)(void *thread_manager_impl, Thread *thread);

    // non-required interface functions
    void   (*init_memoisation)(void       *thread_manager_impl,
                             size_t      nmemo_insts,
                             const char *text);
    int    (*memoise)(void *thread_manager_impl, Thread *thread, len_t idx);
    cntr_t (*counter)(void         *thread_manager_impl,
                      const Thread *thread,
                      len_t         idx);
    void   (*set_counter)(void   *thread_manager_impl,
                        Thread *thread,
                        len_t   idx,
                        cntr_t  val);
    void   (*inc_counter)(void *thread_manager_impl, Thread *thread, len_t idx);
    void *(*memory)(void *thread_manager_impl, const Thread *thread, len_t idx);
    void  (*set_memory)(void       *thread_manager_impl,
                       Thread     *thread,
                       len_t       idx,
                       const void *val,
                       size_t      size);
    const char *const *(*captures)(void         *thread_manager_impl,
                                   const Thread *thread,
                                   len_t        *ncaptures);
    void (*set_capture)(void *thread_manager_impl, Thread *thread, len_t idx);

    void *impl; /**< the underlying implementation                            */
} ThreadManager;

/* --- Thread manager NO-OP functions --------------------------------------- */

// the below functions can be used as placeholders for interface routines where
// nothing should happen. Sensical return values are used -- NULL for pointers,
// truthy values for memoisation, and 0 for counter values.

void thread_manager_init_memoisation_noop(void       *thread_manager_impl,
                                          size_t      nmemo_insts,
                                          const char *text);

int thread_manager_memoise_noop(void   *thread_manager_impl,
                                Thread *thread,
                                len_t   idx);

cntr_t thread_manager_counter_noop(void         *thread_manager_impl,
                                   const Thread *thread,
                                   len_t         idx);

void thread_manager_set_counter_noop(void   *thread_manager_impl,
                                     Thread *thread,
                                     len_t   idx,
                                     cntr_t  val);

void thread_manager_inc_counter_noop(void   *thread_manager_impl,
                                     Thread *thread,
                                     len_t   idx);

void *thread_manager_memory_noop(void         *thread_manager_impl,
                                 const Thread *thread,
                                 len_t         idx);

void thread_manager_set_memory_noop(void       *thread_manager_impl,
                                    Thread     *thread,
                                    len_t       idx,
                                    const void *val,
                                    size_t      size);

const char *const *thread_manager_captures_noop(void *thread_manager_impl,
                                                const Thread *thread,
                                                len_t        *ncaptures);

void thread_manager_set_capture_noop(void   *thread_manager_impl,
                                     Thread *thread,
                                     len_t   idx);

#endif
