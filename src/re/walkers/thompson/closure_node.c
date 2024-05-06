#include "closure_node.h"

/* --- walk routines ------------------------------------------------------- */

WALKER_F(STAR)
{
    WALK_LEFT();
    // insert memoisation instruction behind child of star
    // F* -> (#F)*
    SET_CHILD(regex_branch(CONCAT, regex_new(MEMOISE), CHILD));
}

WALKER_F(PLUS)
{
    WALK_LEFT();
    // insert memoisation instruction behind child of plus
    // F+ -> (#F)+
    SET_CHILD(regex_branch(CONCAT, regex_new(MEMOISE), CHILD));
}

/* --- API routines -------------------------------------------------------- */

void closure_node_thompson(RegexNode **r)
{
    Walker *w;

    if (!r || !(*r)) return;

    w = walker_init();
    SET_WALKER_F(w, STAR);
    SET_WALKER_F(w, PLUS);

    walker_walk(w, r);
    walker_release(w);
}
