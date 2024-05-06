#ifndef THOMPSON_H
#define THOMPSON_H

#include "../../vm/compiler.h"
#include "../smir.h"

/**
 * Construct a state machine from a regex tree using the Thompson construction.
 *
 * @param[in] re   the regex string and tree
 * @param[in] opts the compiler options for construction
 *
 * @return the Thompson constructed state machine
 */
StateMachine *thompson_construct(Regex re, const CompilerOpts *opts);

#endif /* THOMPSON_H */
