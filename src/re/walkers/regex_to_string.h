#ifndef _REGEX_TO_STR_H
#define _REGEX_TO_STR_H

#include "walker.h"

/**
 * Convert the given regex tree to a string.
 *
 * @param[in] re The regular expression parse tree
 */
char *regex_to_string(RegexNode *re);

#endif
