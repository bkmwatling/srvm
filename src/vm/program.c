#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../stc/fatp/vec.h"
#include "../stc/util/utf.h"

#include "../types.h"
#include "../utils.h"
#include "program.h"

#define BUFSIZE 512

/**< function type for printing an offset to a file stream */
typedef void(offset_print_f)(FILE       *stream,
                             offset_t    offset,
                             const byte *pc,
                             const byte *insts);

/**< function type for printing a predicate */
typedef void(predicate_print_f)(FILE *stream, len_t idx, const byte *aux);

/* --- Helper function prototypes ------------------------------------------- */

static const byte *inst_print_formatted(FILE              *stream,
                                        const byte        *pc,
                                        const Program     *prog,
                                        predicate_print_f *print_predicate,
                                        offset_print_f    *print_offset);
static void print_predicate_as_string(FILE *stream, len_t idx, const byte *aux);
static void print_predicate_as_index(FILE *stream, len_t idx, const byte *aux);
static void print_offset_as_absolute_index(FILE       *stream,
                                           offset_t    x,
                                           const byte *pc,
                                           const byte *insts);
static void print_offset_as_offset(FILE       *stream,
                                   offset_t    offset,
                                   const byte *pc,
                                   const byte *insts);

/* --- API function definitions --------------------------------------------- */

Program *program_new(const char *regex,
                     size_t      insts_len,
                     size_t      aux_len,
                     size_t      nmemo_insts,
                     size_t      ncounters,
                     size_t      thread_mem_len,
                     size_t      ncaptures)
{
    Program *prog = calloc(1, sizeof(*prog));

    prog->regex = regex;
    stc_vec_init(prog->insts, insts_len);
    stc_vec_init(prog->aux, aux_len);
    prog->nmemo_insts = nmemo_insts;
    stc_vec_init(prog->counters, ncounters);
    prog->thread_mem_len = thread_mem_len;
    prog->ncaptures      = ncaptures;

    memset(prog->insts, 0, insts_len * sizeof(byte));
    memset(prog->aux, 0, aux_len * sizeof(byte));
    memset(prog->counters, 0, ncounters * sizeof(cntr_t));

    return prog;
}

Program *program_default(const char *regex)
{
    Program *prog = calloc(1, sizeof(*prog));

    prog->regex = regex;
    stc_vec_default_init(prog->insts);
    stc_vec_default_init(prog->aux);
    stc_vec_default_init(prog->counters);

    memset(prog->insts, 0, STC_VEC_DEFAULT_CAP * sizeof(byte));
    memset(prog->aux, 0, STC_VEC_DEFAULT_CAP * sizeof(byte));
    memset(prog->counters, 0, STC_VEC_DEFAULT_CAP * sizeof(cntr_t));

    return prog;
}

void program_free(Program *self)
{
    free((void *) self->regex);
    stc_vec_free(self->insts);
    stc_vec_free(self->aux);
    stc_vec_free(self->counters);
    free(self);
}

void program_print(const Program *self, FILE *stream)
{
    len_t       i         = 0;
    const byte *pc        = self->insts,
               *insts_end = pc + stc_vec_len_unsafe(self->insts);

    while (pc < insts_end) {
        fprintf(stream, "%4d: ", i++);
        pc = inst_print_formatted(stream, pc, self, print_predicate_as_string,
                                  print_offset_as_absolute_index);
        fputc('\n', stream);
    }
}

void inst_print(FILE *stream, const byte *pc)
{
    inst_print_formatted(stream, pc, NULL, print_predicate_as_index,
                         print_offset_as_offset);
}

/* --- Helper function definitions ------------------------------------------ */

static const byte *inst_print_formatted(FILE              *stream,
                                        const byte        *pc,
                                        const Program     *prog,
                                        predicate_print_f *print_predicate,
                                        offset_print_f    *print_offset)
{
    char       *p;
    const byte *insts = prog ? prog->insts : NULL;
    const byte *aux   = prog ? prog->aux : NULL;
    cntr_t      c;
    offset_t    x, y;
    len_t       i, n;

    if (pc == NULL) return NULL;

    switch (*pc++) {
        case NOOP: fputs("noop", stream); break;
        case MATCH: fputs("match", stream); break;
        case BEGIN: fputs("begin", stream); break;
        case END: fputs("end", stream); break;

        case MEMO:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "memo " LEN_FMT, n);
            break;

        case CHAR:
            MEMREAD(p, pc, char *);
            fprintf(stream, "char %.*s", stc_utf8_nbytes(p), p);
            break;

        case PRED:
            MEMREAD(i, pc, len_t);
            fputs("pred ", stream);
            print_predicate(stream, i, aux);
            break;

        case SAVE:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "save " LEN_FMT, n);
            break;

        case JMP:
            MEMREAD(x, pc, offset_t);
            fputs("jmp ", stream);
            print_offset(stream, x, pc, insts);
            break;

        case SPLIT:
            MEMREAD(x, pc, offset_t);
            MEMREAD(y, pc, offset_t);
            fputs("split ", stream);
            print_offset(stream, x, pc - sizeof(offset_t), insts);
            fputs(", ", stream);
            print_offset(stream, y, pc, insts);
            break;

        case GSPLIT:
            MEMREAD(x, pc, offset_t);
            fputs("gsplit ", stream);
            print_offset(stream, x, pc, insts);
            break;

        case LSPLIT:
            MEMREAD(x, pc, offset_t);
            fputs("lsplit ", stream);
            print_offset(stream, x, pc, insts);
            break;

        case TSWITCH:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "tswitch " LEN_FMT, n);
            for (i = 0; i < n; i++) {
                MEMREAD(x, pc, offset_t);
                fputs(", ", stream);
                print_offset(stream, x, pc, insts);
            }
            break;

        case EPSRESET:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "epsreset " LEN_FMT, n);
            break;

        case EPSSET:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "epsset " LEN_FMT, n);
            break;

        case EPSCHK:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "epschk " LEN_FMT, n);
            break;

        case RESET:
            MEMREAD(i, pc, len_t);
            MEMREAD(c, pc, cntr_t);
            fprintf(stream, "reset " LEN_FMT ", " CNTR_FMT, i, c);
            break;

        case CMP:
            MEMREAD(i, pc, len_t);
            MEMREAD(c, pc, cntr_t);

            switch (*pc++) {
                case LT: fputs("cmplt ", stream); break;
                case LE: fputs("cmple ", stream); break;
                case EQ: fputs("cmpeq ", stream); break;
                case NE: fputs("cmpne ", stream); break;
                case GE: fputs("cmpge ", stream); break;
                case GT: fputs("cmpgt ", stream); break;
            }

            fprintf(stream, LEN_FMT ", " CNTR_FMT, i, c);
            break;

        case INC:
            MEMREAD(i, pc, len_t);
            fprintf(stream, "inc " LEN_FMT, i);
            break;

        case ZWA:
            MEMREAD(x, pc, offset_t);
            MEMREAD(y, pc, offset_t);
            fputs("zwa ", stream);
            print_offset(stream, x, pc - sizeof(offset_t), insts);
            fputs(", ", stream);
            print_offset(stream, y, pc, insts);
            fprintf(stream, ", %d", *pc);
            pc++;
            break;

        case STATE: fputs("state", stream); break;

        default:
            fprintf(stderr, "bytecode = %d\n", pc[-1]);
            assert(0 && "unreachable");
    }

    return pc;
}

static void print_predicate_as_string(FILE *stream, len_t idx, const byte *aux)
{
    char *p = intervals_to_str((const Intervals *) (aux + idx));
    fputs(p, stream);
    free(p);
}

static void print_predicate_as_index(FILE *stream, len_t idx, const byte *aux)
{
    UNUSED(aux);
    fprintf(stream, LEN_FMT, idx);
}

static void print_offset_as_absolute_index(FILE       *stream,
                                           offset_t    x,
                                           const byte *pc,
                                           const byte *insts)
{
    len_t idx, len;

    pc += x;
    for (idx = 0; insts < pc; idx++) {
        switch (*insts++) {
            case NOOP:  /* fallthrough */
            case MATCH: /* fallthrough */
            case BEGIN: /* fallthrough */
            case END: break;
            case MEMO: insts += sizeof(len_t); break;
            case CHAR: insts += sizeof(char *); break;
            case PRED: /* fallthrough */
            case SAVE: insts += sizeof(len_t); break;
            case JMP:    /* fallthrough */
            case GSPLIT: /* fallthrough */
            case LSPLIT: insts += sizeof(offset_t); break;
            case SPLIT: insts += 2 * sizeof(offset_t); break;
            case TSWITCH:
                MEMREAD(len, insts, len_t);
                insts += len * sizeof(offset_t);
                break;
            case EPSRESET: /* fallthrough */
            case EPSSET:   /* fallthrough */
            case EPSCHK: insts += sizeof(len_t); break;
            case RESET: insts += sizeof(len_t) + sizeof(cntr_t); break;
            case CMP: insts += sizeof(len_t) + sizeof(cntr_t) + 1; break;
            case INC: insts += sizeof(len_t); break;
            case ZWA: insts += 2 * sizeof(offset_t) + 1; break;
            case STATE: break;
            default:
                fprintf(stderr, "bytecode = %d\n", insts[-1]);
                assert(0 && "unreachable");
        }
    }

    fprintf(stream, LEN_FMT, idx);
}

static void print_offset_as_offset(FILE       *stream,
                                   offset_t    offset,
                                   const byte *pc,
                                   const byte *insts)
{
    UNUSED(pc);
    UNUSED(insts);
    fprintf(stream, OFFSET_FMT, offset);
}
