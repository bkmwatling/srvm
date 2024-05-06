#ifndef GLUSHKOV_H
#define GLUSHKOV_H

#include "../../vm/compiler.h"
#include "../smir.h"

/**
 * Construct a state machine from a regex tree using the Glushkov construction.
 *
 * @param[in] re   the regex string and tree
 * @param[in] opts the compiler options for construction
 *
 * @return the Glushkov constructed state machine
 */
StateMachine *glushkov_construct(Regex re, const CompilerOpts *opts);

#endif /* GLUSHKOV_H */
