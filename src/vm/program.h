#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdio.h>

#include "../stc/fatp/vec.h"

#include "../re/sre.h"
#include "../types.h"

/**
 * Write a bytecode to instruction byte stream at given PC and move PC to after
 * bytecode.
 *
 * @param[in,out] pc       the PC to write the bytecode to
 * @param[in]     bytecode the bytecode to write to PC
 */
#define BCWRITE(pc, bytecode) (*(pc)++ = (bytecode))

/**
 * Push a bytecode to the end of an instruction byte stream.
 *
 * @param[in] insts    the instruction byte stream
 * @param[in] bytecode the bytecode to push onto the instruction byte stream
 */
#define BCPUSH(insts, bytecode) stc_vec_push_back(insts, bytecode)

/**
 * Write a value of given type to PC and advance PC past that value.
 *
 * @param[in,out] pc   the PC to write the value to
 * @param[in]     type the type of the value to write to PC
 * @param[in]     val  the value to write to PC
 */
#define MEMWRITE(pc, type, val)           \
    do {                                  \
        *((type *) (pc))  = (val);        \
        (pc)             += sizeof(type); \
    } while (0)

/**
 * Push a value of a given type to end of a byte stream.
 *
 * @param[in] bytes the byte stream
 * @param[in] type  the type of the value to push onto the byte stream
 * @param[in] val   the value to push onto the byte stream
 */
#define MEMPUSH(bytes, type, val)                                          \
    do {                                                                   \
        stc_vec_reserve(bytes, sizeof(type));                              \
        *((type *) ((bytes) + stc_vec_len_unsafe(bytes)))  = (val);        \
        stc_vec_len_unsafe(bytes)                         += sizeof(type); \
    } while (0)

/**
 * Copy a given number of bytes of memory from a given memory address to the end
 * of a byte stream.
 *
 * @param[in] bytes the byte stream
 * @param[in] src   the memory address to copy from
 * @param[in] size  the number of bytes to copy
 */
#define MEMCPY(bytes, src, size)                                    \
    do {                                                            \
        stc_vec_reserve(bytes, size);                               \
        memcpy((bytes) + stc_vec_len_unsafe(bytes), (src), (size)); \
        stc_vec_len_unsafe(bytes) += (size);                        \
    } while (0)

/**
 * Read the value of the given type from PC into the destination, and move PC
 * past the value in the underlying byte stream.
 *
 * @param[in]     dst  the destination to save the value into
 * @param[in,out] pc   the PC to read the value from
 * @param[in]     type the type of the value to read
 */
#define MEMREAD(dst, pc, type)     \
    do {                           \
        (dst)  = *((type *) (pc)); \
        (pc)  += sizeof(type);     \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

/* Bytecodes */
#define NOOP       0
#define MATCH      1
#define BEGIN      2
#define END        3
#define MEMO       4
#define CHAR       5
#define PRED       6
#define SAVE       7
#define JMP        8
#define SPLIT      9
#define GSPLIT     10
#define LSPLIT     11
#define TSWITCH    12
#define EPSRESET   13
#define EPSSET     14
#define EPSCHK     15
#define RESET      16
#define CMP        17
#define INC        18
#define ZWA        19
#define STATE      20
#define NBYTECODES 21

/* Order for cmp */
#define LT 1
#define LE 2
#define EQ 3
#define NE 4
#define GE 5
#define GT 6

typedef struct {
    const char *regex; /**< the original regular expression string            */

    // VM execution
    byte *insts; /**< stc_vec of the instruction byte stream                  */
    byte *aux;   /**< stc_vec of the auxillary memory for the program         */

    // shared thread memory
    size_t nmemo_insts; /**< the number of memoisation instructions           */

    // thread memory
    cntr_t *counters;       /**< stc_vec of the counter memory default values */
    size_t  thread_mem_len; /**< the number of bytes needed for thread memory */
    size_t  ncaptures;      /**< the number of captures in the program/regex  */
} Program;

/* --- Program function prototypes ------------------------------------------ */

/**
 * Construct a program with preallocated memory and lengths.
 *
 * NOTE: the instruction stream is not populated, but just allocated.
 * NOTE: values of 0 for lengths of memory (including memoisation, counters, and
 * captures) mean those pointers are NULL and shouldn't be used.
 *
 * @param[in] regex          original regex string of the program
 * @param[in] insts_len      number of bytes to allocate for instruction stream
 * @param[in] aux_len        number of bytes to allocate for auxillary memory
 * @param[in] nmemo_insts    number of memory instructions in the program
 * @param[in] ncounters      number of counters in the program
 * @param[in] thread_mem_len number of bytes to allocate for thread memory
 * @param[in] ncaptures      number of captures in the program
 *
 * @return the constructed program with preallocated memory and lengths
 */
Program *program_new(const char *regex,
                     size_t      insts_len,
                     size_t      aux_len,
                     size_t      nmemo_insts,
                     size_t      ncounters,
                     size_t      thread_mem_len,
                     size_t      ncaptures);

/**
 * Construct a default program without preallocating memory and lengths.
 *
 * @param[in] regex the original regex string of the program
 *
 * @return the constructed program without preallocated memory and lengths
 */
Program *program_default(const char *regex);

/**
 * Free the memory allocated for the program (including the memory of the regex
 * string).
 *
 * @param[in] self the program to free
 */
void program_free(Program *self);

/**
 * Print the instruction stream of the program in human readable format.
 *
 * @param[in] self   the program to print instruction stream of
 * @param[in] stream the file stream to print to
 */
void program_print(const Program *self, FILE *stream);

/**
 * Print an instruction to the file stream, with its operands.
 *
 * @param[in] stream the file stream
 * @param[in] pc     the pointer into the instruction stream
 */
void inst_print(FILE *stream, const byte *pc);

#endif /* PROGRAM_H */
