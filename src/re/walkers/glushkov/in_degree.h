#ifndef _IN_DEGREE_GLUSHKOV_H
#define _IN_DEGREE_GLUSHKOV_H

#include "../walker.h"

/**
 * Memoise the regular expression according to IN(E) for the Glushkov
 * construction.
 *
 * @param r A pointer to the parsed regular expression
 */
void in_degree_glushkov(RegexNode **r);

#endif
