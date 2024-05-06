#include <stdlib.h>

#include "../fa/constructions/glushkov.h"
#include "../fa/constructions/thompson.h"
#include "../fa/transformers/flatten.h"
#include "../fa/transformers/memoisation.h"
#include "../re/sre.h"
#include "../utils.h"
#include "compiler.h"

#define COMPILER_OPTS_DEFAULT \
    ((CompilerOpts){ THOMPSON, FALSE, CS_PCRE, MS_NONE, FALSE })

#define SET_OFFSET(p, pc) (*(p) = pc - (byte *) ((p) + 1))

static void compile_state_markers(void *meta, Program *prog)
{
    UNUSED(meta);
    BCPUSH(prog->insts, STATE);
}

Compiler *compiler_new(const Parser *parser, const CompilerOpts opts)
{
    Compiler *compiler = malloc(sizeof(*compiler));

    compiler->parser = parser;
    compiler->opts   = opts;

    return compiler;
}

Compiler *compiler_default(const Parser *parser)
{
    return compiler_new(parser, COMPILER_OPTS_DEFAULT);
}

void compiler_free(Compiler *self)
{
    parser_free((Parser *) self->parser);
    free(self);
}

const Program *compiler_compile(const Compiler *self)
{
    Regex          re;
    ParseResult    res;
    const Program *prog;
    StateMachine  *sm = NULL, *tmp = NULL;

    res = parser_parse(self->parser, &re);
    if (res.code != PARSE_SUCCESS) return NULL;

    switch (self->opts.construction) {
        case THOMPSON: sm = thompson_construct(re, &self->opts); break;
        case GLUSHKOV: sm = glushkov_construct(re, &self->opts); break;
        case FLAT:
            tmp = thompson_construct(re, &self->opts);
            sm  = transform_flatten(tmp, self->parser->opts.logfile);
            smir_free(tmp);
            break;
    }
    regex_node_free(re.root);

    if (self->opts.memo_scheme != MS_NONE)
        sm = transform_memoise(sm, self->opts.memo_scheme,
                               self->parser->opts.logfile);

#ifdef DEBUG
    smir_print(sm, stderr);
#endif

    prog = smir_compile_with_meta(
        sm, self->opts.mark_states ? compile_state_markers : NULL, NULL);
    smir_free(sm);

    return prog;
}
