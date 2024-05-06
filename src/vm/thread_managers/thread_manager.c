#include "thread_manager.h"
#include "../../utils.h"

/* --- Thread manager NO-OP functions --------------------------------------- */

void thread_manager_init_memoisation_noop(void       *thread_manager_impl,
                                          size_t      nmemo_insts,
                                          const char *text)
{
    UNUSED(thread_manager_impl);
    UNUSED(nmemo_insts);
    UNUSED(text);
}

int thread_manager_memoise_noop(void   *thread_manager_impl,
                                Thread *thread,
                                len_t   idx)
{
    UNUSED(thread_manager_impl);
    UNUSED(thread);
    UNUSED(idx);

    return TRUE;
}

cntr_t thread_manager_counter_noop(void         *thread_manager_impl,
                                   const Thread *thread,
                                   len_t         idx)
{
    UNUSED(thread_manager_impl);
    UNUSED(thread);
    UNUSED(idx);
    return 0;
}

void thread_manager_set_counter_noop(void   *thread_manager_impl,
                                     Thread *thread,
                                     len_t   idx,
                                     cntr_t  val)
{
    UNUSED(thread_manager_impl);
    UNUSED(thread);
    UNUSED(idx);
    UNUSED(val);
}

void thread_manager_inc_counter_noop(void   *thread_manager_impl,
                                     Thread *thread,
                                     len_t   idx)
{
    UNUSED(thread_manager_impl);
    UNUSED(thread);
    UNUSED(idx);
}

void *thread_manager_memory_noop(void         *thread_manager_impl,
                                 const Thread *thread,
                                 len_t         idx)
{
    UNUSED(thread_manager_impl);
    UNUSED(thread);
    UNUSED(idx);
    return NULL;
}

void thread_manager_set_memory_noop(void       *thread_manager_impl,
                                    Thread     *thread,
                                    len_t       idx,
                                    const void *val,
                                    size_t      size)
{
    UNUSED(thread_manager_impl);
    UNUSED(thread);
    UNUSED(idx);
    UNUSED(val);
    UNUSED(size);
}

const char *const *thread_manager_captures_noop(void *thread_manager_impl,
                                                const Thread *thread,
                                                len_t        *ncaptures)
{
    UNUSED(thread_manager_impl);
    UNUSED(thread);
    UNUSED(ncaptures);
    return NULL;
}

void thread_manager_set_capture_noop(void   *thread_manager_impl,
                                     Thread *thread,
                                     len_t   idx)
{
    UNUSED(thread_manager_impl);
    UNUSED(thread);
    UNUSED(idx);
}
