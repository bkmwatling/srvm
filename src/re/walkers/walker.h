/**
 *
 * The Walker struct loosely mimics ANTLR4 visitors and listeners (in one).
 *
 * It maintains a map from Regex node types to walk() functions.
 * These functions control the exploration of the walker (visitor). Note the
 * Walker currently only supports DFS exploration; see TODO at bottom.
 * Altering the parse tree (in-place!) should happen in these functions.
 *
 * The default walker created when calling walker_init() will walk the parse
 * tree in-order, but will trigger all events (pre-order, in-order,
 * post-order) where appropriate.
 *
 * The walk() functions also trigger events for listeners to act upon.
 * Currently supported events are pre-order traversal (i.e. before walking any
 * children), in-order traversal (i.e. after walking the first child), and
 * post-order traversal (i.e. after walking the last child).
 * Note on the default walker:
 *      If a node has 0 children, all three events are triggered consecutively.
 *      If a node has exactly 1 child, the pre-order traversal event is
 *      triggered, the child is walked, and the in-order and post-order events
 *      are triggered consecutively.
 *
 * Listener functions are provided with a reference to the global state of the
 * walker, as well as the node triggering the event.
 *
 * TODO: Add ENTRY and EXIT listener events? Perhaps just remove INORDER.
 *
 * TODO: The Walker currently uses the function call stack for traversal --
 * this may change in the future.
 *
 */

#ifndef WALKER_H
#define WALKER_H

#include "../sre.h"

/* --- data structures ------------------------------------------------------ */

typedef struct walker Walker;
typedef void          (*listener_f)(void *state, RegexNode *curr);

// double pointer to facilitate replacing current node in tree
typedef void (*walker_f)(Walker *w, RegexNode **curr);

typedef enum { PREORDER, INORDER, POSTORDER, NEVENTS } ListenerEvent;

struct walker {
    RegexNode **regex;

    // visitor API
    walker_f walk[NREGEXTYPES];
    walker_f walk_terminal;

    // listener API
    listener_f trigger[NEVENTS][NREGEXTYPES];
    listener_f listen_terminal;

    // global state accessible to walker and listener functions
    void *state;
};

/* --- convenience macros --------------------------------------------------- */

#define WALKER _w
#define REGEX  _r
#define WALKER_F(type) \
    static void __walk_##type(Walker *WALKER, RegexNode **REGEX)

#define SET_WALKER_F(w, type) (w)->walk[type] = __walk_##type
#define WALK(w, r)            (w)->walk[(r)->type]((w), &(r))

#define WALK_TERMINAL_F() \
    static void __walk_terminal(Walker *WALKER, RegexNode **REGEX)

#define SET_WALK_TERMINAL_F(w) (w)->walk_terminal = __walk_terminal
#define WALK_TERMINAL()        WALKER->walk_terminal(WALKER, REGEX)

#define CURRENT (*REGEX)
#define LEFT    CURRENT->left
#define RIGHT   CURRENT->right
#define CHILD   LEFT

#define WALK_LEFT() \
    if (LEFT) WALK(WALKER, LEFT)
#define WALK_RIGHT() \
    if (RIGHT) WALK(WALKER, RIGHT)

#define TRIGGER(event)                         \
    if (WALKER->trigger[event][CURRENT->type]) \
    WALKER->trigger[event][CURRENT->type](WALKER->state, CURRENT)

#define TRIGGER_TERMINAL() \
    if (WALKER->listen_terminal) WALKER->listen_terminal(WALKER->state, CURRENT)

#define SET_CURRENT(exp) *REGEX = exp
#define SET_LEFT(exp)    LEFT = exp
#define SET_RIGHT(exp)   RIGHT = exp
#define SET_CHILD(exp)   CHILD = exp

#define STATE _state
#define LISTENER_F(event, type) \
    static void __listen_##event##_##type(void *STATE, RegexNode *REGEX)

#define GET_STATE(cast, varname) cast varname = (cast) (STATE)

#define REGISTER_LISTENER_F(w, event, type) \
    (w)->trigger[event][type] = __listen_##event##_##type

#define LISTEN_TERMINAL_F() \
    static void __listen_terminal(void *STATE, RegexNode *REGEX)

#define REGISTER_LISTEN_TERMINAL_F(w) (w)->listen_terminal = __listen_terminal

/* --- API ------------------------------------------------------------------ */

/**
 * Creates a generic walker.
 */
Walker *walker_init(void);

/**
 * Walks the given walker from the root node.
 *
 * The return value is for convenience in using the walker consecutively,
 * although in general one should pass a pointer to the variable that is the
 * tree. The walker should then update the variable in place, meaning the
 * return value here is likely not necessary.
 *
 * @param walker The walker
 * @param r      A pointer to the parsed regular expression to walk
 * @returns      NULL if any arguments are NULL (including *r), else the
 *               previously walked regex (NULL on the first call)
 */
RegexNode *walker_walk(Walker *walker, RegexNode **r);

/**
 * Release the memory used by the given walker, returning the underlying regex.
 *
 * @param walker The walker
 */
RegexNode *walker_release(Walker *walker);

#endif /* WALKER_H */
