#ifndef PARSER_H
#define PARSER_H

#include "sre.h"

typedef struct {
    int only_counters;              /**< convert *, +, and ? to counters      */
    int unbounded_counters;         /**< allow counters to be unbounded       */
    int expand_counters;            /**< expand counters                      */
    int whole_match_capture;        /**< save entire match into capture 0     */
    int log_unsupported;            /**< log unsupported features in the expr */
    int allow_repeated_nullability; /**< allow expressions like (a?)*         */
    FILE *logfile;                  /**< the file for logging                 */
} ParserOpts;

typedef struct {
    const char *regex; /**< the regex string to parse                         */
    ParserOpts  opts;  /**< the options for parsing                           */
} Parser;

typedef enum {
    PARSE_SUCCESS,
    PARSE_UNSUPPORTED,
    PARSE_NO_MATCH,
    // errors
    PARSE_UNMATCHED_PAREN,
    PARSE_UNQUANTIFIABLE,
    PARSE_INCOMPLETE_GROUP_STRUCTURE,
    PARSE_INVALID_ESCAPE,
    PARSE_MISSING_CLOSING_BRACKET,
    PARSE_CC_RANGE_OUT_OF_ORDER,
    PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ,
    PARSE_NON_EXISTENT_REF,
    PARSE_END_OF_STRING,
    PARSE_REPEATED_NULLABILITY,
} ParseResultCode;

typedef struct {
    ParseResultCode code; /**< the result code for this result                */
    const char *ch; /**< the pointer into the regex that caused this result   */
} ParseResult;

/**
 * Construct a parser from a regex string with specified options.
 *
 * @param[in] regex the regex string
 * @param[in] opts  the options for the parser
 *
 * @return the constructed parser
 */
Parser *parser_new(const char *regex, ParserOpts opts);

/**
 * Construct a parser from a regex string with default options.
 *
 * @param[in] regex the regex string
 *
 * @return the constructed parser
 */
Parser *parser_default(const char *regex);

/**
 * Free the memory allocated for the parser (does not free the regex string).
 *
 * @param[in] self the parser to free
 */
void parser_free(Parser *self);

/**
 * Parse the regex stored in the parser into a regex abstract tree.
 *
 * @param[in]  self the parser to parse
 * @param[out] re   the parsed regex tree with the regex string
 *
 * @return the result of the parsing
 */
ParseResult parser_parse(const Parser *self, Regex *re);

#endif /* PARSER_H */
