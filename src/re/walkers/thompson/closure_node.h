#ifndef _CLOSURE_NODE_THOMPSON_H
#define _CLOSURE_NODE_THOMPSON_H

#include "../walker.h"

/**
 * Apply CN(E) memoisation strategy to the given regular expression.
 *
 * @param r The parsed regular expression.
 */
void closure_node_thompson(RegexNode **r);

#endif
