#include <stdlib.h>

#include "thread_pool.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct thread_list ThreadList;

struct thread_list {
    Thread     *thread;
    ThreadList *next;
};

struct thread_pool {
    ThreadList *pool;

    FILE *logfile;
};

/* --- API function definitions --------------------------------------------- */

ThreadPool *thread_pool_new(FILE *logfile)
{
    ThreadPool *pool = malloc(sizeof(*pool));

    pool->pool    = NULL;
    pool->logfile = logfile;

    return pool;
}

void thread_pool_free(ThreadPool *self, void (*thread_free)(Thread *t))
{
    size_t      nthreads = 0;
    ThreadList *p;

    while ((p = self->pool)) {
        self->pool = p->next;
        nthreads++;
        thread_free(p->thread);
        free(p);
    }
    fprintf(self->logfile, "TOTAL THREADS IN POOL: %zu\n", nthreads);

    free(self);
}

Thread *thread_pool_get_thread(ThreadPool *self)
{
    Thread     *t;
    ThreadList *p;

    if ((p = self->pool)) {
        self->pool = p->next;
        t          = p->thread;
        free(p);
    } else {
        t = NULL;
    }

    return t;
}

void thread_pool_add_thread(ThreadPool *self, Thread *t)
{
    ThreadList *p;

    if (t) {
        p          = malloc(sizeof(*p));
        p->thread  = t;
        p->next    = self->pool;
        self->pool = p;
    }
}
