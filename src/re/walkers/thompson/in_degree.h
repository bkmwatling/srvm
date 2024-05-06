#ifndef _IN_DEGREE_THOMPSON_H
#define _IN_DEGREE_THOMPSON_H

#include "../walker.h"

/**
 * Memoise the regular expression according to IN(E) for the Thompson
 * construction.
 *
 * @param r A pointer to the parsed regular expression
 */
void in_degree_thompson(RegexNode **r);

#endif
