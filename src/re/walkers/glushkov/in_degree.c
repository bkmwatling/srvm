#include "in_degree.h"

#define NULLABLE 0x1
#define LAST1    0x2

#define HAS &
#define AND &
#define OR  |
#define NOT ~

#define SET_NULLABLE(x, f) SET_FLAG(x, f)
#define SET_LAST1(x, f)    SET_FLAG(x, f)
#define SET_FLAG(x, f)     x |= (f)
#define CLEAR_FLAG(x, f)   x &= ~(f)

/* --- walk routines (Glushkov) -------------------------------------------- */

WALKER_F(ALT)
{
    byte *state = WALKER->state;
    byte  left;

    TRIGGER(PREORDER);
    WALK_LEFT();

    // save info
    left = *state;

    TRIGGER(INORDER);
    WALK_RIGHT();
    TRIGGER(POSTORDER);

    SET_NULLABLE(*state, (left OR * state) HAS NULLABLE);
    SET_FLAG(*state, LAST1);
}

WALKER_F(CONCAT)
{
    byte *state = WALKER->state;
    byte  left;

    TRIGGER(PREORDER);
    WALK_LEFT();

    // save left state
    left = *state;

    if (left HAS LAST1)
        SET_LEFT(regex_branch(CONCAT, LEFT, regex_new(MEMOISE)));

    TRIGGER(INORDER);
    WALK_RIGHT();
    TRIGGER(POSTORDER);

    SET_NULLABLE(*state, (left AND * state)
                             HAS NULLABLE AND NOT((left HAS LAST1) >> 1));
    SET_LAST1(*state, (*state HAS NULLABLE) << 1);
}

WALKER_F(CAPTURE)
{
    byte *state = WALKER->state;

    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);

    if (*state HAS LAST1)
        SET_CHILD(regex_branch(CONCAT, CHILD, regex_new(MEMOISE)));

    CLEAR_FLAG(*state, NULLABLE OR LAST1);
}

WALKER_F(STAR)
{
    byte *state = WALKER->state;

    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);

    SET_CHILD(regex_branch(CONCAT, regex_new(MEMOISE), CHILD));
    SET_FLAG(*state, NULLABLE);
}

WALKER_F(PLUS)
{
    byte *state = WALKER->state;

    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);

    SET_CHILD(regex_branch(CONCAT, regex_new(MEMOISE), CHILD));
    // child no longer nullable thanks to memoisation
    CLEAR_FLAG(*state, NULLABLE);
}

WALKER_F(QUES)
{
    byte *state = WALKER->state;

    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);

    SET_FLAG(*state, NULLABLE);
}

WALKER_F(COUNTER)
{
    byte *state = WALKER->state;

    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);

    // if we can match more than once, then we can enter
    // the subexpression through multiple paths -> prepend
    // memoisation instruction
    // (ab{0,5} => a(?:#b){0,5})

    if (CURRENT->max > 1) {
        // can match multiple times; requires memoisation
        SET_CHILD(regex_branch(CONCAT, regex_new(MEMOISE), CHILD));

        // child is no longer nullable
        CLEAR_FLAG(*state, NULLABLE);
    }

    if (CURRENT->min == 0) {
        // nullable
        SET_FLAG(*state, NULLABLE);
    }
}

WALKER_F(LOOKAHEAD)
{
    // TODO: no idea how lookahead is implemented
    TRIGGER(PREORDER);
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);
}

LISTEN_TERMINAL_F()
{
    GET_STATE(byte *, state);
    CLEAR_FLAG(*state, NULLABLE OR LAST1);

    (void) REGEX;
}

void in_degree_glushkov(RegexNode **r)
{
    Walker *w;
    byte    last;

    if (!r || !(*r)) return;

    w        = walker_init();
    w->state = &last;

    SET_WALKER_F(w, ALT);
    SET_WALKER_F(w, CONCAT);
    SET_WALKER_F(w, CAPTURE);
    SET_WALKER_F(w, STAR);
    SET_WALKER_F(w, PLUS);
    SET_WALKER_F(w, QUES);
    SET_WALKER_F(w, COUNTER);
    SET_WALKER_F(w, LOOKAHEAD);

    REGISTER_LISTEN_TERMINAL_F(w);

    walker_walk(w, r);
    walker_release(w);
}
