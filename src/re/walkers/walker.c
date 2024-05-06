#include <stdlib.h>
#include <string.h>

#include "walker.h"

/* --- identity walker ----------------------------------------------------- */

WALK_TERMINAL_F() { TRIGGER_TERMINAL(); }

// one for each RegexType defined in sre.h
WALKER_F(CARET) { WALK_TERMINAL(); }

WALKER_F(DOLLAR) { WALK_TERMINAL(); }

WALKER_F(MEMOISE) { WALK_TERMINAL(); }

WALKER_F(LITERAL) { WALK_TERMINAL(); }

WALKER_F(CC) { WALK_TERMINAL(); }

WALKER_F(ALT)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    WALK_RIGHT();
    TRIGGER(POSTORDER);
}

WALKER_F(CONCAT)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    WALK_RIGHT();
    TRIGGER(POSTORDER);
}

WALKER_F(CAPTURE)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);
}

WALKER_F(STAR)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);
}

WALKER_F(PLUS)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);
}

WALKER_F(QUES)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);
}

WALKER_F(COUNTER)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);
}

WALKER_F(LOOKAHEAD)
{
    TRIGGER(PREORDER);
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);
}

/* --- API routines -------------------------------------------------------- */

Walker *walker_init(void)
{
    Walker *w;

    w = malloc(sizeof(Walker));
    if (!w) return NULL;

    w->regex           = NULL;
    w->state           = NULL;
    w->listen_terminal = NULL;
    memset(w->walk, 0, sizeof(w->walk));
    memset(w->trigger, 0, sizeof(w->trigger));

    SET_WALK_TERMINAL_F(w);
    SET_WALKER_F(w, CARET);
    SET_WALKER_F(w, DOLLAR);
    SET_WALKER_F(w, MEMOISE);
    SET_WALKER_F(w, LITERAL);
    SET_WALKER_F(w, CC);
    SET_WALKER_F(w, ALT);
    SET_WALKER_F(w, CONCAT);
    SET_WALKER_F(w, CAPTURE);
    SET_WALKER_F(w, STAR);
    SET_WALKER_F(w, PLUS);
    SET_WALKER_F(w, QUES);
    SET_WALKER_F(w, COUNTER);
    SET_WALKER_F(w, LOOKAHEAD);

    return w;
}

RegexNode *walker_walk(Walker *walker, RegexNode **regex)
{
    RegexNode *prev = NULL;

    if (!walker || !regex || !(*regex)) return NULL;

    if (walker->regex) prev = *walker->regex;

    walker->regex = regex;
    WALK(walker, *(walker->regex));

    return prev;
}

RegexNode *walker_release(Walker *walker)
{
    RegexNode *out = NULL;

    if (walker) {
        out = *walker->regex;
        free(walker);
    }

    return out;
}
