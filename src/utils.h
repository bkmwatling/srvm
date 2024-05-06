#ifndef UTILS_H
#define UTILS_H

#define UNUSED(var) ((void) &var)

#define ENSURE_SPACE(arr, len, alloc, size)           \
    do {                                              \
        if ((len) >= (alloc)) {                       \
            do {                                      \
                (alloc) <<= 1;                        \
            } while ((len) >= (alloc));               \
            (arr) = realloc((arr), (alloc) * (size)); \
        }                                             \
    } while (0)

#define PUSH(arr, len, alloc, item)                        \
    do {                                                   \
        ENSURE_SPACE(arr, (len) + 1, alloc, sizeof(item)); \
        (arr)[(len)++] = (item);                           \
    } while (0)

#define STR_PUSH(s, len, alloc, str)                               \
    do {                                                           \
        ENSURE_SPACE(s, (len) + sizeof(str), alloc, sizeof(char)); \
        strncpy((s) + (len), (str), (alloc) - (len));              \
        (len) += sizeof(str) - 1;                                  \
    } while (0)

#define DLL_INIT(dll)                            \
    do {                                         \
        (dll)       = calloc(1, sizeof(*(dll))); \
        (dll)->prev = (dll)->next = (dll);       \
    } while (0)

#define DLL_FREE(dll, elem_free, elem, next)                           \
    do {                                                               \
        for ((elem) = (dll)->next; (elem) != (dll); (elem) = (next)) { \
            (next) = (elem)->next;                                     \
            (elem_free)((elem));                                       \
        }                                                              \
        free((dll));                                                   \
    } while (0)

#define DLL_PUSH_FRONT(dll, elem)         \
    do {                                  \
        (elem)->prev       = (dll);       \
        (elem)->next       = (dll)->next; \
        (elem)->next->prev = (elem);      \
        (dll)->next        = (elem);      \
    } while (0)

#define DLL_PUSH_BACK(dll, elem)          \
    do {                                  \
        (elem)->prev       = (dll)->prev; \
        (elem)->next       = (dll);       \
        (dll)->prev        = (elem);      \
        (elem)->prev->next = (elem);      \
    } while (0)

#define DLL_GET(dll, idx, elem)                              \
    for ((elem) = (dll)->next; (idx) > 0 && (elem) != (dll); \
         (idx)--, (elem) = (elem)->next)

#endif /* UTILS_H */
