#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../types.h"
#include "../utils.h"
#include "parser.h"
#include "sre.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define PARSER_OPTS_DEFAULT ((ParserOpts){ 0, 1, 0, 0, 1, 0, stderr })

#define SET_RID(node, ps) \
    if (node) (node)->rid = (ps)->next_rid++

#define PARSE_RES(code, ch) ((ParseResult){ (code), (ch) })

#define SUCCEEDED(code) ((code) < PARSE_NO_MATCH)

#define FAILED(code) ((code) >= PARSE_NO_MATCH)

#define NOT_MATCHED(code) ((code) == PARSE_NO_MATCH)

#define ERRORED(code) ((code) > PARSE_NO_MATCH)

#define DOT_NINTERVALS 2

#define UNICODE_END "\U0010ffff"

#define INTERVAL_LIST_ITEM_INIT(item, intrvl)          \
    do {                                               \
        (item)           = calloc(1, sizeof(*(item))); \
        (item)->interval = (intrvl);                   \
    } while (0)

#define SKIP_UNTIL(pred, res, parse_state)        \
    do {                                          \
        while (!(pred)) {                         \
            if (*(parse_state)->ch == '\0') {     \
                (res).code = PARSE_END_OF_STRING; \
                break;                            \
            }                                     \
            (parse_state)->ch++;                  \
        }                                         \
    } while (0)

#define FLAG_UNSUPPORTED(code, ps) ((*(ps)->unsupported_features)[code] = TRUE)

/* --- Type definitions ----------------------------------------------------- */

typedef enum {
    UNSUPPORTED_BACKREF,
    UNSUPPORTED_LOOKAHEAD,
    UNSUPPORTED_OCTAL,
    UNSUPPORTED_HEX,
    UNSUPPORTED_UNICODE,
    UNSUPPORTED_POSSESSIVE,
    UNSUPPORTED_CONTROL_VERB,
    UNSUPPORTED_FLAGS,
    UNSUPPORTED_CONTROL_CODE,
    UNSUPPORTED_LOOKBEHIND,
    UNSUPPORTED_NAMED_GROUP,
    UNSUPPORTED_RELATIVE_GROUP,
    UNSUPPORTED_ATOMIC_GROUP,
    UNSUPPORTED_PATTERN_RECURSION,
    UNSUPPORTED_LOOKAHEAD_CONDITIONAL,
    UNSUPPORTED_CALLOUT,
    UNSUPPORTED_GROUP_RESET,
    UNSUPPORTED_GROUP_RECURSION,
    UNSUPPORTED_WORD_BOUNDARY,
    UNSUPPORTED_START_BOUNDARY,
    UNSUPPORTED_END_BOUNDARY,
    UNSUPPORTED_FIRST_MATCH_BOUNDARY,
    UNSUPPORTED_RESET_MATCH_START,
    UNSUPPORTED_QUOTING,
    UNSUPPORTED_UNICODE_PROPERTY,
    UNSUPPORTED_NEWLINE_SEQUENCE,
    NUM_UNSUPPORTED_CODES
} UnsupportedFeatureCode;

typedef struct interval_list_item IntervalListItem;

struct interval_list_item {
    Interval          interval;
    IntervalListItem *prev;
    IntervalListItem *next;
};

typedef struct {
    int               neg;
    size_t            len;
    IntervalListItem *sentinel;
} IntervalList;

typedef struct {
    UnsupportedFeatureCode (*unsupported_features)[NUM_UNSUPPORTED_CODES];
    byte allow_repeated_nullability; /*<< allow expressions like (a?)*        */
    const char *ch;
    int         in_group;
    int         in_lookahead;
    len_t       ncaptures;
    regex_id    next_rid;
} ParseState;

/* --- Helper function prototypes ------------------------------------------- */

static ParseResult parse_alt(const Parser *self,
                             ParseState   *ps,
                             RegexNode   **re /**< out parameter */);

static ParseResult parse_expr(const Parser *self,
                              ParseState   *ps,
                              RegexNode   **re /**< out parameter */);

static ParseResult parse_elem(const Parser *self,
                              ParseState   *ps,
                              RegexNode   **re /**< out parameter */);

static ParseResult parse_atom(const Parser *self,
                              ParseState   *ps,
                              RegexNode   **re /**< out parameter */);

static ParseResult parse_quantifier(const Parser *self,
                                    ParseState   *ps,
                                    RegexNode   **re /**< in,out parameter */);

static ParseResult parse_curly(ParseState *ps,
                               cntr_t     *min /**< out parameter */,
                               cntr_t     *max /**< out parameter */);

static ParseResult parse_paren(const Parser *self,
                               ParseState   *ps,
                               RegexNode   **re /**< out parameter */);

static ParseResult parse_cc(ParseState *ps,
                            RegexNode **re /**< out parameter */);

static ParseResult parse_cc_atom(ParseState   *ps,
                                 IntervalList *list /**< out parameter */);

static ParseResult parse_escape(ParseState *ps,
                                RegexNode **re /**< out parameter */);

static ParseResult parse_escape_char(ParseState  *ps,
                                     const char **ch /**< out parameter */);

static ParseResult parse_escape_cc(ParseState   *ps,
                                   IntervalList *list /**< out parameter */);

static ParseResult parse_posix_cc(ParseState   *ps,
                                  IntervalList *list /**< out parameter */);

static ParseResult skip_comment(ParseState *ps);

static void find_matching_closing_parenthesis(ParseState *ps);

static void print_unsupported_feature(unsigned int feature_idx, FILE *stream);

static RegexNode *parser_regex_counter(RegexNode  *child,
                                       byte        greedy,
                                       cntr_t      min,
                                       cntr_t      max,
                                       int         expand_counters,
                                       ParseState *ps);

/* --- API function definitions --------------------------------------------- */

Parser *parser_new(const char *regex, ParserOpts opts)
{
    Parser *parser = malloc(sizeof(*parser));

    parser->regex = regex;
    parser->opts  = opts;

    return parser;
}

Parser *parser_default(const char *regex)
{
    return parser_new(regex, PARSER_OPTS_DEFAULT);
}

void parser_free(Parser *self) { free(self); }

ParseResult parser_parse(const Parser *self, Regex *re)
{
    RegexNode             *r                                           = NULL;
    UnsupportedFeatureCode unsupported_features[NUM_UNSUPPORTED_CODES] = { 0 };
    ParseState             ps  = { &unsupported_features,
                                   self->opts.allow_repeated_nullability,
                                   self->regex,
                                   0,
                                   0,
                      self->opts.whole_match_capture ? 1 : 0,
                                   0 };
    ParseResult            res = parse_alt(self, &ps, &r);
    unsigned int           i;

    if (SUCCEEDED(res.code)) {
        if (self->opts.whole_match_capture) {
            r      = regex_capture(r, 0);
            r->rid = ps.next_rid++;
        }

        if (re) *re = (Regex){ self->regex, r };
    } else if (r) {
        regex_node_free(r);
    }

    if (self->opts.log_unsupported && res.code == PARSE_UNSUPPORTED) {
        fprintf(self->opts.logfile,
                "------------ UNSUPPORTED FEATURE CODES ------------\n");
        for (i = 0; i < NUM_UNSUPPORTED_CODES; i++)
            if (unsupported_features[i])
                print_unsupported_feature(i, self->opts.logfile);
        fprintf(self->opts.logfile,
                "------------ UNSUPPORTED FEATURE CODES ------------\n");
    }

    return res;
}

/* --- Helper function definitions ------------------------------------------ */

static Intervals *dot(void)
{
    Intervals *dot = intervals_new(TRUE, DOT_NINTERVALS);

    dot->intervals[0] = interval("\0", "\0");
    dot->intervals[1] = interval("\n", "\n");

    return dot;
}

static ParseResult parse_alt(const Parser *self,
                             ParseState   *ps,
                             RegexNode   **re /**< out parameter */)
{
    RegexNode  *r;
    ParseResult res, prev_res;

    res = parse_expr(self, ps, re);
    if (FAILED(res.code)) return res;
    while (*ps->ch == '|') {
        ps->ch++;
        r        = NULL;
        prev_res = res;
        res      = parse_expr(self, ps, &r);
        // NOTE: prev_res must be SUCCESS-ish, hence, this will never
        // overwrite a failure code in res.
        if (prev_res.code > res.code) res.code = prev_res.code;
        if (FAILED(res.code)) {
            if (r) regex_node_free(r);
            return res;
        }
        *re = regex_branch(ALT, *re, r);
        SET_RID(*re, ps);
    }

    return res;
}

static ParseResult parse_expr(const Parser *self,
                              ParseState   *ps,
                              RegexNode   **re /**< out parameter */)
{
    RegexNode  *r;
    ParseResult res, prev_res;

    res = parse_elem(self, ps, re);
    if (NOT_MATCHED(res.code)) {
        *re = regex_new(EPSILON);
        SET_RID(*re, ps);
        return PARSE_RES(PARSE_SUCCESS, ps->ch);
    } else if (ERRORED(res.code)) {
        return res;
    }
    while (*ps->ch) {
        r        = NULL;
        prev_res = res;
        res      = parse_elem(self, ps, &r);

        // NOTE: prev_res must be SUCCESS-ish, hence, this will never
        // overwrite a failure code in res.
        if (NOT_MATCHED(res.code)) {
            if (r) regex_node_free(r);
            res.code = prev_res.code;
            break;
        } else if (ERRORED(res.code)) {
            if (r) regex_node_free(r);
            break;
        } else if (prev_res.code > res.code)
            res.code = prev_res.code;

        *re = regex_branch(CONCAT, *re, r);
        SET_RID(*re, ps);
    }

    return res;
}

static ParseResult parse_elem(const Parser *self,
                              ParseState   *ps,
                              RegexNode   **re /**< out parameter */)
{
    ParseResult     res;
    ParseResultCode prev_code;

    res = parse_atom(self, ps, re);
    if (FAILED(res.code)) return res;
    prev_code = res.code;
    res       = parse_quantifier(self, ps, re);
    if (NOT_MATCHED(res.code) || (SUCCEEDED(res.code) && prev_code > res.code))
        res.code = prev_code;

    return res;
}

static ParseResult parse_atom(const Parser *self,
                              ParseState   *ps,
                              RegexNode   **re /**< out parameter */)
{
    ParseResult     res;
    ParseResultCode prev_code;

    res = skip_comment(ps);
    if (ERRORED(res.code)) return res;

    switch (*ps->ch) {
        case '\\': res = parse_escape(ps, re); break;
        case '(': res = parse_paren(self, ps, re); break;
        case '[': res = parse_cc(ps, re); break;

        case '|': res = PARSE_RES(PARSE_NO_MATCH, ps->ch); break;

        case '^':
            *re = regex_new(CARET);
            SET_RID(*re, ps);
            res = PARSE_RES(PARSE_SUCCESS, ps->ch++);
            break;

        case '$':
            *re = regex_new(DOLLAR);
            SET_RID(*re, ps);
            res = PARSE_RES(PARSE_SUCCESS, ps->ch++);
            break;

        // case '#':
        //     *re = regex_new(MEMOISE);
        //     SET_RID(*re, ps);
        //     res = PARSE_RES(PARSE_SUCCESS, ps->ch++);
        //     break;
        //
        case '.':
            *re = regex_cc(dot());
            SET_RID(*re, ps);
            res = PARSE_RES(PARSE_SUCCESS, ps->ch++);
            break;

        case '\0':
            // *re = regex_new(EPSILON);
            // SET_RID(*re, ps);
            res = PARSE_RES(PARSE_NO_MATCH, ps->ch);
            break;

        case ')':
            if (ps->in_group)
                res = PARSE_RES(PARSE_NO_MATCH, ps->ch);
            else
                res = PARSE_RES(PARSE_UNMATCHED_PAREN, ps->ch);
            break;

        case '*': /* fallthrough */
        case '+': /* fallthrough */
        case '?': res = PARSE_RES(PARSE_UNQUANTIFIABLE, ps->ch); break;

        case '{':
            res = parse_curly(ps, NULL, NULL);
            if (SUCCEEDED(res.code)) {
                res.code = PARSE_UNQUANTIFIABLE;
                break;
            }

        default:
            *re = regex_literal(ps->ch);
            SET_RID(*re, ps);
            res = PARSE_RES(PARSE_SUCCESS, ps->ch++);
            break;
    }

    if (SUCCEEDED(res.code)) {
        prev_code = res.code;
        res       = skip_comment(ps);
        if (SUCCEEDED(res.code)) res.code = prev_code;
    }

    return res;
}

static ParseResult parse_quantifier(const Parser *self,
                                    ParseState   *ps,
                                    RegexNode   **re /**< in,out parameter */)
{
    RegexNode  *tmp;
    ParseResult res = { PARSE_SUCCESS, NULL };
    byte        greedy;
    cntr_t      min, max;

    /* check for quantifier */
    switch (*ps->ch) {
        case '*':
            min = 0;
            max = CNTR_MAX;
            ps->ch++;
            break;
        case '+':
            min = 1;
            max = CNTR_MAX;
            ps->ch++;
            break;
        case '?':
            min = 0;
            max = 1;
            ps->ch++;
            break;
        case '{':
            res = parse_curly(ps, &min, &max);
            if (FAILED(res.code)) return res;
            break;

        default: return PARSE_RES(PARSE_NO_MATCH, ps->ch);
    }

    /* check that child is quantifiable */
    switch ((*re)->type) {
        case EPSILON:
        case CARET:
        /* case MEMOISE: */
        case DOLLAR: return PARSE_RES(PARSE_UNQUANTIFIABLE, ps->ch);

        case LITERAL:
        case CC:
        case ALT:
        case CONCAT:
        case CAPTURE:
        case STAR:
        case PLUS:
        case QUES:
        case COUNTER:
        case LOOKAHEAD:
        case BACKREFERENCE:
            if (!ps->allow_repeated_nullability && max == CNTR_MAX &&
                (*re)->nullable) {
                return PARSE_RES(PARSE_REPEATED_NULLABILITY, ps->ch);
            }
            break;

        case NREGEXTYPES: assert(0 && "unreachable");
    }

    /* check for lazy */
    switch (*ps->ch) {
        case '?':
            greedy = FALSE;
            ps->ch++;
            break;

        /* NOTE: unsupported */
        case '+':
            FLAG_UNSUPPORTED(UNSUPPORTED_POSSESSIVE, ps);
            ps->ch++;
            res.code = PARSE_UNSUPPORTED;
            break;

        default: greedy = TRUE;
    }

    /* check for single or zero repitition */
    if (min == 1 && max == 1) {
        res.ch = ps->ch;
        return res;
    }
    if (min == 0 && max == 0) {
        regex_node_free(*re);
        *re = regex_new(EPSILON);
        SET_RID(*re, ps);
        res.ch = ps->ch;
        return res;
    }

    /* apply quantifier */
    if (self->opts.only_counters) {
        *re = regex_counter(*re, greedy, min, max);
        SET_RID(*re, ps);
    } else if (min == 0 && max == CNTR_MAX) {
        *re = regex_repetition(STAR, *re, greedy);
        SET_RID(*re, ps);
    } else if (min == 1 && max == CNTR_MAX) {
        *re = regex_repetition(PLUS, *re, greedy);
        SET_RID(*re, ps);
    } else if (min == 0 && max == 1) {
        *re = regex_repetition(QUES, *re, greedy);
        SET_RID(*re, ps);
    } else if (self->opts.unbounded_counters || max < CNTR_MAX) {
        *re = parser_regex_counter(*re, greedy, min, max,
                                   self->opts.expand_counters, ps);
    } else {
        tmp = regex_repetition(STAR, *re, greedy);
        *re = regex_branch(CONCAT,
                           parser_regex_counter(regex_clone(*re), greedy, min,
                                                min, self->opts.expand_counters,
                                                ps),
                           tmp);
        SET_RID(tmp, ps);
        SET_RID(*re, ps);
    }

    res.ch = ps->ch;
    return res;
}

static ParseResult parse_curly(ParseState *ps,
                               cntr_t     *min /**< out parameter */,
                               cntr_t     *max /**< out parameter */)
{
    char        ch;
    cntr_t      m, n;
    const char *next_ch = ps->ch;

    if (*next_ch++ != '{' || !isdigit(*next_ch))
        return PARSE_RES(PARSE_NO_MATCH, ps->ch);

    m = 0;
    while ((ch = *next_ch++) != ',') {
        if (isdigit(ch)) {
            m = m * 10 + (ch - '0');
        } else if (ch == '}') {
            n = m;
            goto done;
        } else {
            return PARSE_RES(PARSE_NO_MATCH, ps->ch);
        }
    }

    n = *next_ch == '}' ? CNTR_MAX : 0;
    while ((ch = *next_ch++) != '}')
        if (isdigit(ch))
            n = n * 10 + (ch - '0');
        else
            return PARSE_RES(PARSE_NO_MATCH, ps->ch);

done:
    ps->ch = next_ch;
    if (min) *min = m;
    if (max) *max = n;
    return PARSE_RES(PARSE_SUCCESS, ps->ch);
}

static ParseResult parse_paren(const Parser *self,
                               ParseState   *ps,
                               RegexNode   **re /**< out parameter */)
{
    ParseResult            res;
    ParseState             ps_tmp;
    UnsupportedFeatureCode unsupported_code;
    const char            *ch;
    len_t                  ncaptures;
    int                    is_lookahead = FALSE, pos = FALSE;

    if (*(ch = ps->ch) != '(') return PARSE_RES(PARSE_NO_MATCH, ps->ch);
    ps->ch++;

    switch (*ps->ch) {

        /* control verbs */
        case '*':
            unsupported_code = UNSUPPORTED_CONTROL_VERB;
            goto unsupported_group;
            break;

        case '?':
            switch (*++ps->ch) {
                /* lookbehind */
                case '<':
                    unsupported_code = UNSUPPORTED_LOOKBEHIND;
                    goto unsupported_group;

                /* named groups */
                case 'P':
                case '\'':
                    unsupported_code = UNSUPPORTED_NAMED_GROUP;
                    goto unsupported_group;

                /* relative group back ref */
                case '-':
                case '+':
                    unsupported_code = UNSUPPORTED_RELATIVE_GROUP;
                    goto unsupported_group;

                /* atomic grouping */
                case '>':
                    unsupported_code = UNSUPPORTED_ATOMIC_GROUP;
                    goto unsupported_group;

                /* pattern recursion */
                case 'R':
                    unsupported_code = UNSUPPORTED_PATTERN_RECURSION;
                    goto unsupported_group;

                /* conditional lookahead */
                case '(':
                    unsupported_code = UNSUPPORTED_LOOKAHEAD_CONDITIONAL;
                    goto unsupported_group;

                /* callout */
                case 'C':
                    unsupported_code = UNSUPPORTED_CALLOUT;
                    goto unsupported_group;

                // TODO: support lookaheads
                case '=': // pos = TRUE; /* fallthrough */
                case '!':
                    // is_lookahead = TRUE; break;
                    unsupported_code = UNSUPPORTED_LOOKAHEAD;
                    goto unsupported_group;

                /* reset group numbers in alternations */
                case '|':
                    unsupported_code = UNSUPPORTED_GROUP_RESET;
                    goto unsupported_group;

                /* flags */
                case 'i':
                case 'J':
                case 'm':
                case 's':
                case 'U':
                case 'x':
                    unsupported_code = UNSUPPORTED_FLAGS;
                    goto unsupported_group;

                /* non-capturing */
                case ':': break;

                /* group recursion */
                default:
                    if (!isdigit(*ps->ch))
                        return PARSE_RES(PARSE_INCOMPLETE_GROUP_STRUCTURE, ch);

                    unsupported_code = UNSUPPORTED_GROUP_RECURSION;
                    goto unsupported_group;
            }
            ps->ch++;
            ps_tmp        = (ParseState){ ps->unsupported_features,
                                          ps->allow_repeated_nullability,
                                          ps->ch,
                                          TRUE,
                                          ps->in_lookahead || is_lookahead,
                                          ps->ncaptures,
                                          ps->next_rid };
            res           = parse_alt(self, &ps_tmp, re);
            ps->ch        = ps_tmp.ch;
            ps->ncaptures = ps_tmp.ncaptures;
            ps->next_rid  = ps_tmp.next_rid;
            if (FAILED(res.code)) return res;
            if (*ps->ch != ')')
                return PARSE_RES(PARSE_INCOMPLETE_GROUP_STRUCTURE, ch);

            if (is_lookahead) {
                *re = regex_lookahead(*re, pos);
                SET_RID(*re, ps);
            }
            break;

        /* capture group */
        default:
            if (!ps->in_lookahead) ncaptures = ps->ncaptures++;
            ps_tmp        = (ParseState){ ps->unsupported_features,
                                          ps->allow_repeated_nullability,
                                          ps->ch,
                                          TRUE,
                                          ps->in_lookahead,
                                          ps->ncaptures,
                                          ps->next_rid };
            res           = parse_alt(self, &ps_tmp, re);
            ps->ch        = ps_tmp.ch;
            ps->ncaptures = ps_tmp.ncaptures;
            ps->next_rid  = ps_tmp.next_rid;
            if (FAILED(res.code)) return res;
            if (*ps->ch != ')')
                return PARSE_RES(PARSE_INCOMPLETE_GROUP_STRUCTURE, ch);

            if (!ps->in_lookahead) {
                *re = regex_capture(*re, ncaptures);
                SET_RID(*re, ps);
            }
            break;
    }

done:
    ps->ch++;
    return res;

unsupported_group:
    FLAG_UNSUPPORTED(unsupported_code, ps);
    find_matching_closing_parenthesis(ps);
    if (*ps->ch != ')') return PARSE_RES(PARSE_INCOMPLETE_GROUP_STRUCTURE, ch);
    *re = regex_new(EPSILON);
    SET_RID(*re, ps);
    res = PARSE_RES(PARSE_UNSUPPORTED, ps->ch);
    goto done;
}

static ParseResult parse_cc(ParseState *ps,
                            RegexNode **re /**< out parameter */)
{
    ParseResult       res;
    IntervalList      list = { 0 };
    IntervalListItem *item, *next;
    Intervals        *intervals;
    const char       *ch;
    size_t            i;
    int               neg = FALSE;

    if (*(ch = ps->ch) != '[') return PARSE_RES(PARSE_NO_MATCH, ps->ch);

    if (*++ps->ch == '^') {
        neg = TRUE;
        ps->ch++;
    }

    DLL_INIT(list.sentinel);
    do {
        res = parse_cc_atom(ps, &list);
        if (ERRORED(res.code)) {
            if (res.code == PARSE_MISSING_CLOSING_BRACKET) res.ch = ch;
            goto done;
        }
    } while (*ps->ch != ']');
    ps->ch++;

    intervals = intervals_new(neg, list.len);
    for (i = 0, item = list.sentinel->next; item != list.sentinel;
         i++, item   = item->next) {
        intervals->intervals[i] = item->interval;
    }

    *re = regex_cc(intervals);
    SET_RID(*re, ps);

done:
    DLL_FREE(list.sentinel, free, item, next);
    return res;
}

static ParseResult parse_cc_atom(ParseState   *ps,
                                 IntervalList *list /**< out parameter */)
{
    ParseResult       res;
    IntervalListItem *item;
    const char       *ch, *ch_start = NULL, *ch_end = NULL;

    switch (*ps->ch) {
        case '[':
            res = parse_posix_cc(ps, list);
            if (NOT_MATCHED(res.code)) {
                ch_start = "[";
                res.code = PARSE_SUCCESS;
                ps->ch++;
            }
            break;

        case '\\':
            res = parse_escape_char(ps, &ch_start);
            if (NOT_MATCHED(res.code)) {
                res = parse_escape_cc(ps, list);
                if (NOT_MATCHED(res.code))
                    res = PARSE_RES(PARSE_INVALID_ESCAPE, ps->ch);
            }
            break;

        case '\0':
            res = PARSE_RES(PARSE_MISSING_CLOSING_BRACKET, ps->ch);
            break;

        default: /* NOTE: should handle ']' at beginning of character class */
            ch_start = stc_utf8_str_advance(&ps->ch);
            res      = PARSE_RES(PARSE_SUCCESS, ps->ch);
            break;
    }

    if (ERRORED(res.code)) return res;

    if (*(ch = ps->ch) != '-') {
        if (ch_start) {
            INTERVAL_LIST_ITEM_INIT(item, interval(ch_start, ch_start));
            DLL_PUSH_BACK(list->sentinel, item);
            list->len++;
        }
        return res;
    }

    if (!ch_start && ps->ch[1] != ']')
        return PARSE_RES(PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ, ch);

    switch (*++ps->ch) {
        case '[':
            res = parse_posix_cc(ps, list);
            if (NOT_MATCHED(res.code)) {
                ch_end   = "[";
                res.code = PARSE_SUCCESS;
                ps->ch++;
            } else if (SUCCEEDED(res.code)) {
                res = PARSE_RES(PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ, ch);
            }
            break;

        case '\\':
            res = parse_escape_char(ps, &ch_end);
            if (NOT_MATCHED(res.code)) {
                res = parse_escape_cc(ps, list);
                if (NOT_MATCHED(res.code))
                    res = PARSE_RES(PARSE_INVALID_ESCAPE, ps->ch);
                else if (SUCCEEDED(res.code))
                    res = PARSE_RES(PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ,
                                    ch);
            }
            break;

        case ']':
            if (ch_start) {
                INTERVAL_LIST_ITEM_INIT(item, interval(ch_start, ch_start));
                DLL_PUSH_BACK(list->sentinel, item);
                list->len++;
            }
            INTERVAL_LIST_ITEM_INIT(item, interval("-", "-"));
            DLL_PUSH_BACK(list->sentinel, item);
            list->len++;
            res = PARSE_RES(PARSE_SUCCESS, ps->ch);
            break;

        case '\0':
            res = PARSE_RES(PARSE_MISSING_CLOSING_BRACKET, ps->ch);
            break;

        default:
            ch_end = stc_utf8_str_advance(&ps->ch);
            res    = PARSE_RES(PARSE_SUCCESS, ps->ch);
            break;
    }

    if (ERRORED(res.code) || !ch_end) return res;

    if (stc_utf8_cmp(ch_start, ch_end) <= 0) {
        INTERVAL_LIST_ITEM_INIT(item, interval(ch_start, ch_end));
        DLL_PUSH_BACK(list->sentinel, item);
        list->len++;
    } else {
        res = PARSE_RES(PARSE_CC_RANGE_OUT_OF_ORDER, ch);
    }

    return res;
}

static ParseResult parse_escape(ParseState *ps,
                                RegexNode **re /**< out parameter */)
{
    ParseResult       res;
    IntervalList      list = { 0 };
    IntervalListItem *item, *next;
    Intervals        *intervals;
    const char       *ch;
    size_t            i;
    // TODO: use for backreferences
    // len_t             k;

    if (*ps->ch != '\\') return PARSE_RES(PARSE_NO_MATCH, ps->ch);

    res = parse_escape_char(ps, &ch);
    if (ERRORED(res.code)) return res;
    if (SUCCEEDED(res.code)) {
        *re = regex_literal(ch);
        SET_RID(*re, ps);
        return res;
    }

    DLL_INIT(list.sentinel);
    res = parse_escape_cc(ps, &list);
    if (ERRORED(res.code)) goto done;
    if (SUCCEEDED(res.code)) {
        intervals = intervals_new(FALSE, list.len);
        for (i = 0, item = list.sentinel->next; item != list.sentinel;
             i++, item   = item->next) {
            intervals->intervals[i] = item->interval;
        }

        *re = regex_cc(intervals);
        SET_RID(*re, ps);
        goto done;
    }

    res.code = PARSE_SUCCESS;
    ch       = ps->ch++;
    switch (*ps->ch) {
        case 'B':
        case 'b':
            FLAG_UNSUPPORTED(UNSUPPORTED_WORD_BOUNDARY, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'A':
            FLAG_UNSUPPORTED(UNSUPPORTED_START_BOUNDARY, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'z':
        case 'Z':
            FLAG_UNSUPPORTED(UNSUPPORTED_END_BOUNDARY, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'G':
            FLAG_UNSUPPORTED(UNSUPPORTED_FIRST_MATCH_BOUNDARY, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'g':
            ps->ch++;
            if (*ps->ch == '{') {
                SKIP_UNTIL(*ps->ch == '}', res, ps);
                if (FAILED(res.code)) return res;
            } else {
                SKIP_UNTIL(!isdigit(*ps->ch), res, ps);
                if (SUCCEEDED(res.code)) ps->ch--;
            }
            FLAG_UNSUPPORTED(UNSUPPORTED_BACKREF, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'k':
            switch (ps->ch[1]) {
                case '<': SKIP_UNTIL(*ps->ch == '>', res, ps); break;
                case '\\': SKIP_UNTIL(*ps->ch == '\\', res, ps); break;
                case '{': SKIP_UNTIL(*ps->ch == '}', res, ps); break;
                default: break;
            }
            if (FAILED(res.code)) return res;
            FLAG_UNSUPPORTED(UNSUPPORTED_BACKREF, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'K':
            FLAG_UNSUPPORTED(UNSUPPORTED_RESET_MATCH_START, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;

        // technically will stop at '\\E' as well, but since the semantics of
        // \Q ... \E is to treat characters as is, this is fine (i.e. the second
        // \ is not escaped)
        case 'Q':
            SKIP_UNTIL(*ps->ch == 'E' && ps->ch[-1] == '\\', res, ps);
            if (FAILED(res.code)) return res;
        case 'E': // NOTE: \E only has special meaning if \Q was already seen
            FLAG_UNSUPPORTED(UNSUPPORTED_QUOTING, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'p':
        case 'P':
            ps->ch++;
            if (*ps->ch == '{') {
                SKIP_UNTIL(*ps->ch == '}', res, ps);
                if (FAILED(res.code)) return res;
            }
            FLAG_UNSUPPORTED(UNSUPPORTED_UNICODE_PROPERTY, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'R':
            FLAG_UNSUPPORTED(UNSUPPORTED_NEWLINE_SEQUENCE, ps);
            *re = regex_new(EPSILON);
            SET_RID(*re, ps);
            res.code = PARSE_UNSUPPORTED;
            break;

        default:
            if (isdigit(*ps->ch)) {
                // TODO: support backreferences
                // k = *ps->ch - '0';
                // if (k < ps->ncaptures) {
                //     *re = regex_backreference(k);
                //     SET_RID(*re, ps);
                // } else {
                //     res.code = PARSE_NON_EXISTENT_REF;
                // }
                FLAG_UNSUPPORTED(UNSUPPORTED_BACKREF, ps);
                *re = regex_new(EPSILON);
                SET_RID(*re, ps);
                res.code = PARSE_UNSUPPORTED;
                break;
            } else {
                res.code = PARSE_INVALID_ESCAPE;
            }
            break;
    }

    if (SUCCEEDED(res.code)) ch = ++ps->ch;
    res.ch = ch;

done:
    DLL_FREE(list.sentinel, free, item, next);
    return res;
}

static ParseResult parse_escape_char(ParseState  *ps,
                                     const char **ch /**< out parameter */)
{
    ParseResult res = { PARSE_SUCCESS, NULL };
    const char *next_ch;

    if (*(next_ch = ps->ch) != '\\') return PARSE_RES(PARSE_NO_MATCH, ps->ch);

    switch (*++next_ch) {
        case 't': *ch = "\t"; break;
        case 'f': *ch = "\f"; break;
        case 'n': *ch = "\n"; break;
        case 'r': *ch = "\r"; break;
        case 'v': *ch = "\v"; break;

        case 'a': /* fallthrough */
        case 'e': *ch = "\7"; break;

        case 'c':
            /* XXX: as I understand it is a simple offset on only ascii:
             * @  : 0
             * A-Z: *ps->ch - 'A' + 1
             * a-z: *ps->ch - 'a' + 1
             *  -?: *ps->ch + 64
             * [-`: *ps->ch - 64
             * {-~: *ps->ch - 64 */
            FLAG_UNSUPPORTED(UNSUPPORTED_CONTROL_CODE, ps);
            *ch      = "";
            res.code = PARSE_UNSUPPORTED;
            break;

        case 'o':
            FLAG_UNSUPPORTED(UNSUPPORTED_OCTAL, ps);
            *ch = "";
            if (*next_ch == '{') {
                while (*next_ch != '}') {
                    if (*next_ch == '\0') {
                        res.code = PARSE_END_OF_STRING;
                        break;
                    }
                    next_ch++;
                }
            } else {
                // TODO: technically should be of length 1 <= n <= 3
                while (*next_ch < '8' && isdigit(*next_ch++))
                    ;
                next_ch--;
            }
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'x':
            FLAG_UNSUPPORTED(UNSUPPORTED_HEX, ps);
            *ch = "";
            if (*next_ch == '{') {
                while (*next_ch != '}') {
                    if (*next_ch == '\0') {
                        res.code = PARSE_END_OF_STRING;
                        break;
                    }
                    next_ch++;
                }
            } else {
                // TODO: technically should be of length 1 <= n <= 2
                while (isxdigit(*next_ch++))
                    ;
                next_ch--;
            }
            res.code = PARSE_UNSUPPORTED;
            break;
        case 'u':
            FLAG_UNSUPPORTED(UNSUPPORTED_UNICODE, ps);
            *ch = "";
            if (*next_ch == '{') {
                while (*next_ch != '}') {
                    if (*next_ch == '\0') {
                        res.code = PARSE_END_OF_STRING;
                        break;
                    }
                    next_ch++;
                }
            } else {
                // TODO: technically should be of length 1 <= n <= 4
                while (isxdigit(*next_ch++))
                    ;
                next_ch--;
            }
            res.code = PARSE_UNSUPPORTED;
            break;

        default:
            if (ispunct(*next_ch))
                *ch = next_ch;
            else
                res.code = PARSE_NO_MATCH;
    }

    if (SUCCEEDED(res.code)) ps->ch = ++next_ch;
    res.ch = ps->ch;
    return res;
}

#define PUSH_INTERVAL(start, end)                            \
    INTERVAL_LIST_ITEM_INIT(item, interval((start), (end))); \
    DLL_PUSH_BACK(list->sentinel, item);                     \
    list->len++

#define PUSH_CHAR(ch) PUSH_INTERVAL(ch, ch);

static ParseResult parse_escape_cc(ParseState   *ps,
                                   IntervalList *list /**< out parameter */)
{
    IntervalListItem *item;
    ParseResult       res = { PARSE_SUCCESS, NULL };
    const char       *next_ch;

    if (*(next_ch = ps->ch) != '\\') return PARSE_RES(PARSE_NO_MATCH, ps->ch);

    switch (*++next_ch) {
        case 'D':
            PUSH_INTERVAL("\x00", "\x2f");
            PUSH_INTERVAL("\x3a", UNICODE_END);
            break;

        case 'd': PUSH_INTERVAL("0", "9"); break;

        case 'N':
            PUSH_INTERVAL("\x00", "\x09");
            PUSH_INTERVAL("\x0b", UNICODE_END);
            break;

        case 'H':
            PUSH_INTERVAL("\x00", "\x08");
            PUSH_INTERVAL("\x10", "\x1f");
            PUSH_INTERVAL("\x21", UNICODE_END);
            break;

        case 'h':
            PUSH_CHAR("\t");
            PUSH_CHAR(" ");
            break;

        case 'V':
            PUSH_INTERVAL("\x00", "\x09");
            PUSH_INTERVAL("\x0e", UNICODE_END);
            break;

        case 'v': PUSH_INTERVAL("\x0a", "\x0d"); break;

        case 'S':
            PUSH_INTERVAL("\x00", "\x08");
            PUSH_INTERVAL("\x0e", "\x1f");
            PUSH_INTERVAL("\x21", UNICODE_END);
            break;

        case 's':
            PUSH_CHAR("\t");
            PUSH_INTERVAL("\x0a", "\x0d");
            PUSH_CHAR(" ");
            break;

        case 'W':
            PUSH_INTERVAL("\x00", "\x2f");
            PUSH_INTERVAL("\x3a", "\x40");
            PUSH_INTERVAL("\x5b", "\x5e");
            PUSH_CHAR("\x60");
            PUSH_INTERVAL("\x7b", UNICODE_END);
            break;

        case 'w':
            PUSH_INTERVAL("0", "9");
            PUSH_INTERVAL("A", "Z");
            PUSH_CHAR("_");
            PUSH_INTERVAL("a", "z");
            break;

        case 'C':
        case 'X': res.code = PARSE_INVALID_ESCAPE; break;

        default: res.code = PARSE_NO_MATCH; break;
    }

    if (SUCCEEDED(res.code)) ps->ch = ++next_ch;
    res.ch = ps->ch;
    return res;
}

static ParseResult parse_posix_cc(ParseState   *ps,
                                  IntervalList *list /**< out parameter */)
{
    IntervalListItem *item;
    ParseResult       res = { PARSE_SUCCESS, NULL };

    if (strcmp(ps->ch, "[:alnum:]") == 0) {
        PUSH_INTERVAL("A", "Z");
        PUSH_INTERVAL("a", "z");
        PUSH_INTERVAL("0", "9");
        ps->ch += sizeof("[:alnum:]") - 1;
    } else if (strcmp(ps->ch, "[:alpha:]") == 0) {
        PUSH_INTERVAL("A", "Z");
        PUSH_INTERVAL("a", "z");
        ps->ch += sizeof("[:alpha:]") - 1;
    } else if (strcmp(ps->ch, "[:ascii:]") == 0) {
        PUSH_INTERVAL("\x00", "\x7f");
        ps->ch += sizeof("[:ascii:]") - 1;
    } else if (strcmp(ps->ch, "[:blank:]") == 0) {
        PUSH_CHAR(" ");
        PUSH_CHAR("\t");
        ps->ch += sizeof("[:blank:]") - 1;
    } else if (strcmp(ps->ch, "[:cntrl:]") == 0) {
        PUSH_INTERVAL("\x00", "\x1f");
        PUSH_CHAR("\x7f");
        ps->ch += sizeof("[:cntrl:]") - 1;
    } else if (strcmp(ps->ch, "[:digit:]") == 0) {
        PUSH_INTERVAL("0", "9");
        ps->ch += sizeof("[:digit:]") - 1;
    } else if (strcmp(ps->ch, "[:graph:]") == 0) {
        PUSH_INTERVAL("\x21", "\x7e");
        ps->ch += sizeof("[:graph:]") - 1;
    } else if (strcmp(ps->ch, "[:lower:]") == 0) {
        PUSH_INTERVAL("a", "z");
        ps->ch += sizeof("[:lower:]") - 1;
    } else if (strcmp(ps->ch, "[:print:]") == 0) {
        PUSH_INTERVAL("\x20", "\x7e");
        ps->ch += sizeof("[:print:]") - 1;
    } else if (strcmp(ps->ch, "[:punct:]") == 0) {
        PUSH_INTERVAL("!", "/");
        PUSH_INTERVAL(":", "@");
        PUSH_INTERVAL("[", "`");
        PUSH_INTERVAL("{", "~");
        ps->ch += sizeof("[:punct:]") - 1;
    } else if (strcmp(ps->ch, "[:space:]") == 0) {
        PUSH_CHAR(" ");
        PUSH_CHAR("\t");
        PUSH_CHAR("\f");
        PUSH_CHAR("\n");
        PUSH_CHAR("\r");
        PUSH_CHAR("\v");
        ps->ch += sizeof("[:space:]") - 1;
    } else if (strcmp(ps->ch, "[:upper:]") == 0) {
        PUSH_INTERVAL("A", "Z");
        ps->ch += sizeof("[:upper:]") - 1;
    } else if (strcmp(ps->ch, "[:word:]") == 0) {
        PUSH_INTERVAL("A", "Z");
        PUSH_INTERVAL("a", "z");
        PUSH_INTERVAL("0", "9");
        PUSH_CHAR("_");
        ps->ch += sizeof("[:word:]") - 1;
    } else if (strcmp(ps->ch, "[:xdigit:]") == 0) {
        PUSH_INTERVAL("0", "9");
        PUSH_INTERVAL("A", "F");
        PUSH_INTERVAL("a", "f");
        ps->ch += sizeof("[:xdigit:]") - 1;
    } else {
        res.code = PARSE_NO_MATCH;
    }

    res.ch = ps->ch;
    return res;
}

#undef PUSH_CHAR
#undef PUSH_INTERVAL

static ParseResult skip_comment(ParseState *ps)
{
    const char *ch = ps->ch;

check_for_comment:
    if (*ch++ == '(' && *ch++ == '?' && *ch++ == '#') {
        while (*ch++ != ')')
            if (!*ch)
                return PARSE_RES(PARSE_INCOMPLETE_GROUP_STRUCTURE, ps->ch);
        ps->ch = ch;
        goto check_for_comment;
    }

    return PARSE_RES(PARSE_SUCCESS, ps->ch);
}

static void find_matching_closing_parenthesis(ParseState *ps)
{
    size_t nparen   = 1;
    byte   in_cc    = FALSE;
    byte   in_quote = FALSE;
    char   ch;

    while ((ch = *ps->ch)) {
        switch (ch) {
            case '[':
                if (!in_quote) in_cc = TRUE;
                break;
            case ']':
                if (in_cc) in_cc = FALSE;
                break;
            case ')':
                if (!in_cc && !in_quote) {
                    if (!(--nparen)) return;
                }
                break;
            case '(':
                if (!in_cc && !in_quote) nparen++;
                break;
            case '\\':
                ps->ch++;
                if (in_cc) break;
                if (*ps->ch == 'Q')
                    in_quote = TRUE;
                else if (*ps->ch == 'E' && in_quote)
                    in_quote = FALSE;
                break;
            default: break;
        }
        if (*ps->ch) ps->ch++;
    }
}

static void print_unsupported_feature(unsigned int feature_idx, FILE *stream)
{
    static const char *feature_strings[NUM_UNSUPPORTED_CODES] = {
        "UNSUPPORTED_BACKREF",
        "UNSUPPORTED_LOOKAHEAD",
        "UNSUPPORTED_OCTAL",
        "UNSUPPORTED_HEX",
        "UNSUPPORTED_UNICODE",
        "UNSUPPORTED_POSSESSIVE",
        "UNSUPPORTED_CONTROL_VERB",
        "UNSUPPORTED_FLAGS",
        "UNSUPPORTED_CONTROL_CODE",
        "UNSUPPORTED_LOOKBEHIND",
        "UNSUPPORTED_NAMED_GROUP",
        "UNSUPPORTED_RELATIVE_GROUP",
        "UNSUPPORTED_ATOMIC_GROUP",
        "UNSUPPORTED_PATTERN_RECURSION",
        "UNSUPPORTED_LOOKAHEAD_CONDITIONAL",
        "UNSUPPORTED_CALLOUT",
        "UNSUPPORTED_GROUP_RESET",
        "UNSUPPORTED_GROUP_RECURSION",
        "UNSUPPORTED_WORD_BOUNDARY",
        "UNSUPPORTED_START_BOUNDARY",
        "UNSUPPORTED_END_BOUNDARY",
        "UNSUPPORTED_FIRST_MATCH_BOUNDARY",
        "UNSUPPORTED_RESET_MATCH_START",
        "UNSUPPORTED_QUOTING",
        "UNSUPPORTED_UNICODE_PROPERTY",
        "UNSUPPORTED_NEWLINE_SEQUENCE",
    };

    if (feature_idx >= NUM_UNSUPPORTED_CODES) return;
    fprintf(stream, "%d: %s\n", feature_idx, feature_strings[feature_idx]);
}

static RegexNode *parser_regex_counter(RegexNode  *child,
                                       byte        greedy,
                                       cntr_t      min,
                                       cntr_t      max,
                                       int         expand_counters,
                                       ParseState *ps)
{
    RegexNode *counter, *left, *right, *tmp;
    cntr_t     i;

    if (!expand_counters) {
        counter = regex_counter(child, greedy, min, max);
        SET_RID(counter, ps);
    } else {
        left = min > 0 ? child : NULL;
        for (i = 1; i < min; i++) {
            left = regex_branch(CONCAT, left, regex_clone(child));
            SET_RID(left, ps);
        }

        right = max > min ? regex_repetition(
                                QUES, left ? regex_clone(child) : child, greedy)
                          : NULL;
        SET_RID(right, ps);
        for (i = min + 1; i < max; i++) {
            tmp = regex_branch(CONCAT, regex_clone(child), right);
            SET_RID(tmp, ps);
            right = regex_repetition(QUES, tmp, greedy);
            SET_RID(right, ps);
        }

        if (left && right) {
            counter = regex_branch(CONCAT, left, right);
            SET_RID(counter, ps);
        } else {
            counter = left ? left : right;
        }
    }

    return counter;
}
