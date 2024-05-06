#include "infinite_ambiguity_removal.h"

/* --- API routine --------------------------------------------------------- */

void infinite_ambiguity_removal_thompson(RegexNode **r)
{
    Walker *w;

    if (!r || !(*r)) return;

    w = walker_init();
    // TODO: add walker functions

    walker_walk(w, r);
    walker_release(w);
}
