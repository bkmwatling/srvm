#ifndef _IAR_THOMPSON_H
#define _IAR_THOMPSON_H

#include "../walker.h"

/**
 * Memoise the given regex according to IAR(E).
 *
 * @param r A pointer to the parsed regular expression
 */
void infinite_ambiguity_removal_thompson(RegexNode **r);

#endif
