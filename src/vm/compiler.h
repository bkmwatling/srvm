#ifndef COMPILER_H
#define COMPILER_H

#include "../re/parser.h"
#include "program.h"

typedef enum {
    THOMPSON,
    GLUSHKOV,
    FLAT,
} Construction;

typedef enum {
    CS_PCRE,
    CS_RE2,
} CaptureSemantics;

typedef enum {
    MS_NONE,
    MS_CN,
    MS_IN,
    MS_IAR,
} MemoScheme;

typedef struct {
    Construction     construction;      /**< which construction to use        */
    int              only_std_split;    /**< whether to only use `split`      */
    CaptureSemantics capture_semantics; /**< which capture semantics to use   */
    MemoScheme       memo_scheme;       /**< which memoisation scheme to use  */
    int              mark_states; /**< whether to compile state instructions  */
} CompilerOpts;

typedef struct {
    const Parser *parser; /**< the parser to get the regex tree               */
    CompilerOpts  opts;   /**< the options set for the compiler               */
} Compiler;

/**
 * Construct a compiler from a regex parser with specified options.
 *
 * @param[in] parser the regex parser
 * @param[in] opts   the options for the compiler
 *
 * @return the constructed compiler
 */
Compiler *compiler_new(const Parser *parser, const CompilerOpts opts);

/**
 * Construct a compiler from a regex parser with default options.
 *
 * @param[in] parser the regex parser
 *
 * @return the constructed compiler
 */
Compiler *compiler_default(const Parser *parser);

/**
 * Free the memory allocated for the compiler (frees the memory of the parser,
 * excluding the regex string).
 *
 * @param[in] self the compiler to free
 */
void compiler_free(Compiler *self);

/**
 * Compile the regex tree obtained from the parser into a program.
 *
 * @param[in] self the compiler to compile
 *
 * @return the program compiled from the regex tree obtained from the parser
 */
const Program *compiler_compile(const Compiler *self);

#endif /* COMPILER_H */
