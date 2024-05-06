#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../utils.h"
#include "regex_to_string.h"

/* --- data structures ----------------------------------------------------- */

#define SB_ENSURE_SPACE(sb, n) \
    ENSURE_SPACE((sb)->s, (sb)->len, (sb)->cap, sizeof(char))

#define DEFAULT_CAP 32

typedef struct {
    char  *s;
    size_t len;
    size_t cap;
} StringBuilder;

static void appendn(StringBuilder *sb, const char *s, size_t n)
{
    SB_ENSURE_SPACE(sb, n);
    strncpy(sb->s + sb->len, s, n);
    sb->len += n;
}

static void append(StringBuilder *sb, const char *s)
{
    appendn(sb, s, strlen(s));
}

static void appendf(StringBuilder *sb, const char *fmt, ...)
{
    char   *s;
    size_t  len;
    va_list args;

    va_start(args, fmt);
    // includes terminating '\0'
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    printf("append: %s\n", fmt);
    s = malloc(sizeof(char) * len);
    if (!s) return;

    va_start(args, fmt);
    vsnprintf(s, len, fmt, args);
    va_end(args);

    SB_ENSURE_SPACE(sb, len);
    strncpy(sb->s + sb->len, s, sb->cap - sb->len);
    sb->len += len - 1;

    free(s);
}

static StringBuilder *string_builder(void)
{
    StringBuilder *sb = malloc(sizeof(StringBuilder));

    if (!sb) return NULL;

    sb->s   = malloc(sizeof(char) * DEFAULT_CAP);
    sb->cap = DEFAULT_CAP;
    sb->len = 0;

    return sb;
}

/* --- listener routines --------------------------------------------------- */

LISTEN_TERMINAL_F()
{
    GET_STATE(StringBuilder *, sb);
    switch (REGEX->type) {
        case CARET: append(sb, "^"); break;
        case DOLLAR: append(sb, "$"); break;
        case MEMOISE: append(sb, "#"); break;
        case LITERAL: appendn(sb, REGEX->ch, stc_utf8_nbytes(REGEX->ch)); break;

        case CC: printf("CC printer not supported"); // TODO
        default: break;
    }
}

/* --- ALT ----------------------------------------------------------------- */

LISTENER_F(INORDER, ALT)
{
    GET_STATE(StringBuilder *, sb);
    append(sb, "|");
    if (REGEX->right->type == ALT) {
        // assoiativity was overridden -- insert (?:
        append(sb, "(?:");
    }
}

LISTENER_F(POSTORDER, ALT)
{
    GET_STATE(StringBuilder *, sb);
    if (REGEX->right->type == ALT) {
        // assoiativity was overridden -- insert closing )
        append(sb, ")");
    }
}

/* --- CONCAT -------------------------------------------------------------- */

LISTENER_F(PREORDER, CONCAT)
{
    GET_STATE(StringBuilder *, sb);

    // lower priority operators as child must have parenthesis
    if (REGEX->left->type == ALT) append(sb, "(?:");
}

LISTENER_F(INORDER, CONCAT)
{
    GET_STATE(StringBuilder *, sb);

    // lower priority operators as child must have parenthesis
    if (REGEX->left->type == ALT) append(sb, ")");

    if (IS_BINARY_OP(REGEX->right->type)) {
        // ALT => as above
        // CONCAT => associativity was overriden
        append(sb, "(?:");
    }
}

LISTENER_F(POSTORDER, CONCAT)
{
    GET_STATE(StringBuilder *, sb);

    if (IS_BINARY_OP(REGEX->right->type)) append(sb, ")");
}

/* --- CAPTURE ------------------------------------------------------------- */

LISTENER_F(PREORDER, CAPTURE)
{
    GET_STATE(StringBuilder *, sb);
    append(sb, "(");

    (void) REGEX;
}

LISTENER_F(POSTORDER, CAPTURE)
{
    GET_STATE(StringBuilder *, sb);
    append(sb, ")");

    (void) REGEX;
}

/* --- STAR ---------------------------------------------------------------- */

LISTENER_F(PREORDER, STAR)
{
    GET_STATE(StringBuilder *, sb);

    // not terminal or parenthetical
    if (IS_OP(REGEX->left->type)) append(sb, "(?:");
}

LISTENER_F(POSTORDER, STAR)
{
    GET_STATE(StringBuilder *, sb);

    // see above
    if (IS_OP(REGEX->left->type)) append(sb, ")");

    append(sb, "*");
    if (!REGEX->greedy) append(sb, "?"); // lazy
}

/* --- PLUS ---------------------------------------------------------------- */

LISTENER_F(PREORDER, PLUS)
{
    GET_STATE(StringBuilder *, sb);

    // not terminal or parenthetical
    if (IS_OP(REGEX->left->type)) append(sb, "(?:");
}

LISTENER_F(POSTORDER, PLUS)
{
    GET_STATE(StringBuilder *, sb);

    // see above
    if (IS_OP(REGEX->left->type)) append(sb, ")");

    append(sb, "+");
    if (!REGEX->greedy) append(sb, "?"); // lazy
}

/* --- QUES ---------------------------------------------------------------- */

LISTENER_F(PREORDER, QUES)
{
    GET_STATE(StringBuilder *, sb);

    // not terminal or parenthetical
    if (IS_OP(REGEX->left->type)) append(sb, "(?:");
}

LISTENER_F(POSTORDER, QUES)
{
    GET_STATE(StringBuilder *, sb);

    // see above
    if (IS_OP(REGEX->left->type)) append(sb, ")");

    append(sb, "?");
    if (!REGEX->greedy) append(sb, "?"); // lazy
}

/* --- COUNTER ------------------------------------------------------------- */

LISTENER_F(PREORDER, COUNTER)
{
    GET_STATE(StringBuilder *, sb);

    // not terminal or parenthetical
    if (IS_OP(REGEX->left->type)) append(sb, "(?:");
}

LISTENER_F(POSTORDER, COUNTER)
{
    GET_STATE(StringBuilder *, sb);

    // see above
    if (IS_OP(REGEX->left->type)) append(sb, ")");

    appendf(sb, "{" CNTR_FMT ", " CNTR_FMT "}", REGEX->min, REGEX->max);
    if (!REGEX->greedy) append(sb, "?"); // lazy
}

/* --- LOOKAHEAD ----------------------------------------------------------- */

LISTENER_F(PREORDER, LOOKAHEAD)
{
    GET_STATE(StringBuilder *, sb);

    append(sb, "(?");
    if (REGEX->positive)
        append(sb, "=");
    else
        append(sb, "!");
}

LISTENER_F(POSTORDER, LOOKAHEAD)
{
    GET_STATE(StringBuilder *, sb);

    append(sb, ")");
    (void) REGEX;
}

/* --- API routine --------------------------------------------------------- */

char *regex_to_string(RegexNode *re)
{
    Walker        *w;
    StringBuilder *sb;
    char          *str;

    if (!re) return NULL;

    w = walker_init();
    REGISTER_LISTEN_TERMINAL_F(w);
    REGISTER_LISTENER_F(w, INORDER, ALT);
    REGISTER_LISTENER_F(w, POSTORDER, ALT);

    REGISTER_LISTENER_F(w, PREORDER, CONCAT);
    REGISTER_LISTENER_F(w, INORDER, CONCAT);
    REGISTER_LISTENER_F(w, POSTORDER, CONCAT);

    REGISTER_LISTENER_F(w, PREORDER, CAPTURE);
    REGISTER_LISTENER_F(w, POSTORDER, CAPTURE);

    REGISTER_LISTENER_F(w, PREORDER, STAR);
    REGISTER_LISTENER_F(w, POSTORDER, STAR);

    REGISTER_LISTENER_F(w, PREORDER, PLUS);
    REGISTER_LISTENER_F(w, POSTORDER, PLUS);

    REGISTER_LISTENER_F(w, PREORDER, QUES);
    REGISTER_LISTENER_F(w, POSTORDER, QUES);

    REGISTER_LISTENER_F(w, PREORDER, COUNTER);
    REGISTER_LISTENER_F(w, POSTORDER, COUNTER);

    REGISTER_LISTENER_F(w, PREORDER, LOOKAHEAD);
    REGISTER_LISTENER_F(w, POSTORDER, LOOKAHEAD);

    w->state = (void *) string_builder();
    walker_walk(w, &re);

    sb = (StringBuilder *) w->state;
    SB_ENSURE_SPACE(sb, 1);
    sb->s[sb->len] = '\0';

    str = sb->s;

    // clean up
    walker_release(w);
    free(sb);

    return str;
}
