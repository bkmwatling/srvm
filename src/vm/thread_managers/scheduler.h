#ifndef SCHEDULER_H
#define SCHEDULER_H

/**
 * The Scheduler interface specifies routines for manipulating execution order
 * of threads in the SRVM.
 *
 * It has since been deprecated in favour of using the thread manager interface.
 */

#include "thread_manager.h"

#define scheduler_schedule(scheduler, thread) \
    ((scheduler)->schedule((scheduler)->impl, (thread)))
#define scheduler_schedule_in_order(scheduler, thread) \
    ((scheduler)->schedule_in_order((scheduler)->impl, (thread)))
#define scheduler_has_next(scheduler) ((scheduler)->has_next((scheduler)->impl))
#define scheduler_next(scheduler)     ((scheduler)->next((scheduler)->impl))
#define scheduler_clear(scheduler)    ((scheduler)->clear((scheduler)->impl))
#define scheduler_notify_match(scheduler) \
    ((scheduler)->notify_match((scheduler)->impl))
#define scheduler_free(scheduler)             \
    do {                                      \
        (scheduler)->free((scheduler)->impl); \
        free((scheduler));                    \
    } while (0)

typedef struct {
    void    (*schedule)(void *scheduler_impl, Thread *thread);
    void    (*schedule_in_order)(void *scheduler_impl, Thread *thread);
    int     (*has_next)(const void *scheduler_impl);
    Thread *(*next)(void *scheduler_impl);
    void    (*clear)(void *scheduler_impl);
    void    (*free)(void *scheduler_impl);

    void *impl; /**< the underlying implementation of the scheduler           */
} Scheduler;

#endif
