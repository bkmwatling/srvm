#include "in_degree.h"

/* --- walk routines (Thompson) -------------------------------------------- */

WALKER_F(STAR)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);

    // insert memoisation instruction behind child of star and after star
    // F* -> (#F)*#
    SET_CHILD(regex_branch(CONCAT, regex_new(MEMOISE), CHILD));
    SET_CURRENT(regex_branch(CONCAT, CURRENT, regex_new(MEMOISE)));
}

WALKER_F(ALT)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    WALK_RIGHT();
    TRIGGER(POSTORDER);

    // insert memoisation instruction after alternation
    // (A|B) -> (A|B)#
    SET_CURRENT(regex_branch(CONCAT, CURRENT, regex_new(MEMOISE)));
}

WALKER_F(QUES)
{
    TRIGGER(PREORDER);
    WALK_LEFT();
    TRIGGER(INORDER);
    TRIGGER(POSTORDER);

    // insert memoisation instruction after optional expression
    // E? -> E?#
    SET_CURRENT(regex_branch(CONCAT, CURRENT, regex_new(MEMOISE)));
}

/* --- API routines -------------------------------------------------------- */

void in_degree_thompson(RegexNode **r)
{
    Walker *w;

    if (!r || !(*r)) return;

    w = walker_init();
    SET_WALKER_F(w, STAR);
    SET_WALKER_F(w, ALT);
    SET_WALKER_F(w, QUES);

    walker_walk(w, r);
    walker_release(w);
}
