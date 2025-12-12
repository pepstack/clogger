/******************************************************************************
 * Copyright (c) 2017-2018 Relkom s.r.o Aleksandr Morozov <admin@relkom.sk>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *****************************************************************************/
/**
 * smallregex.c
 *
 * Supports:
 * ---------
 *   '.'        Dot, matches any character
 *   '^'        Start anchor, matches beginning of string
 *   '$'        End anchor, matches end of string
 *   '*'        Asterisk, match zero or more (greedy)
 *   '+'        Plus, match one or more (greedy)
 *   '?'        Question, match zero or one (non-greedy)
 *   '[abc]'    Character class, match if one of {'a', 'b', 'c'}
 *   '[^abc]'   Inverted class, match if NOT one of {'a', 'b', 'c'} -- NOTE: feature is currently broken!
 *   '[a-zA-Z]' Character ranges, the character set of the ranges { a-z | A-Z }
 *   '\s'       Whitespace, \t \f \r \n \v and spaces
 *   '\S'       Non-whitespace
 *   '\w'       Alphanumeric, [a-zA-Z0-9_]
 *   '\W'       Non-alphanumeric
 *   '\d'       Digits, [0-9]
 *   '\D'       Non-digits
 *   '('        Group start
 *   ')'        Group ends
 *   '|'        Branch
 **/
#ifdef _KERNEL
    #include <sys/cdefs.h>
    #include <sys/param.h>
    #include <sys/kernel.h>
    #include <sys/errno.h>
    #include <sys/types.h>
    #include <sys/malloc.h>
    #include <sys/libkern.h>
#else
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
#endif // _KERNEL


#include "smallregex.h"

// Definitions:
#ifdef _KERNEL
    MALLOC_DEFINE(MC_RE, "mc_re", "Small C regex in kernel");

    #define KMALLOC(_CNT_, _SIZE_) malloc( (_CNT_ * _SIZE_), MC_RE, M_WAITOK|M_ZERO)
    #define KREALLOC(_ADDR_, _SIZE_) realloc(_ADDR_, _SIZE_, MC_RE, M_WAITOK|M_ZERO)
    #define KFREE(_ADDR_) if (_ADDR_ != NULL) free((void*)_ADDR_, MC_RE)

#else

    #define KMALLOC(_CNT_, _SIZE_) calloc(_CNT_, _SIZE_)
    #define KREALLOC(_ADDR_, _SIZE_) realloc(_ADDR_, _SIZE_)
    #define KFREE(_ADDR_) if (_ADDR_ != NULL) free((void*)_ADDR_)

#endif // _KERNEL


#ifndef _KERNEL
    #include <assert.h>
#endif


#ifndef _KERNEL
    #define ASSERT_NULL_R(_VAR_, _RETURN_) assert(_VAR_ != NULL)
#else
    #define ASSERT_NULL_R(_VAR_, _RETURN_) \
        if (_VAR_ == NULL) { \
            printf("ASSERT_NULL:[%s:%u] var: %s\r\n", __func__, __LINE__, #_VAR_); \
            return _RETURN_; \
        }
#endif


#define GENERATE_ENUM_ANY(ENUM, ...) ENUM,
#define GENERATE_VAL_STRINGS_VAL(STRING, VAL) VAL,
#define GENERATE_VAR_NAMES_VAL(STRING, VAL) #STRING,

#define FOREACH_GLOBAL_TYPE(DV)\
        DV(UNUSED,          " "     )\
        DV(DOT,             "."     )\
        DV(BEGIN,           "^"     )\
        DV(END,             "$"     )\
        DV(QUESTIONMARK,    "?"     )\
        DV(STAR,            "*"     )\
        DV(PLUS,            "+"     )\
        DV(CHAR,            "char"  )\
        DV(CHAR_CLASS,      "[]"    )\
        DV(INV_CHAR_CLASS,  "^[]"   )\
        DV(DIGIT,           "\\d"   )\
        DV(NOT_DIGIT,       "\\D"   )\
        DV(ALPHA,           "\\w"   )\
        DV(NOT_ALPHA,       "\\W"   )\
        DV(WHITESPACE,      "\\s"   )\
        DV(NOT_WHITESPACE,  "\\S"   )\
        DV(BRANCH,          "|"     )\
        DV(GROUPSTART,      "("     )\
        DV(GROUPEND,        ")"     )


enum {
    FOREACH_GLOBAL_TYPE(GENERATE_ENUM_ANY)
};


// stack instance
struct sstate_t {
    uint32_t matchcnt;  //for *+ match count
    int32_t offset;     //current offset
    int32_t st0;        //the last st0 accomulator
    int32_t st;         //current st
};

// push stack
#define PUSH()      sstate[sp+1].offset = sstate[sp].offset; sstate[sp+1].st = sstate[sp].st; sp++;

// pop from stack
#define POP()       sp--

// restore (unused)
#define RESTORE()   sp = 0

// the fast access
#define SSTP        sstate[sp]


/**
 * A structure for grouping instancing
 */
struct re_pair_inst {
    uint32_t trueoff;   // offset for eval true
    uint32_t failoff;   // offset for eval false
};


/**
 * Compilation args
 */
struct args_reg_compile {
    struct re_pair_inst *inst;  // start end of groups (offsets)
    uint32_t objs;          // amount of the objects
    uint32_t strln;         // the length of the class length
    uint32_t ecnt;          // the length og the inst
    uint32_t stcnt;         // how large the sstate table may be in the worst case
};


#ifdef RE_BUILDWITH_DEBUG
    static const char* typestr[] = {
        FOREACH_GLOBAL_TYPE(GENERATE_VAL_STRINGS_VAL)
    };

    const char* typenames[] = {
        FOREACH_GLOBAL_TYPE(GENERATE_VAR_NAMES_VAL)
    };
#endif

// Private function declarations:
static int32_t matchpattern(struct small_regex * reg, struct regex_objs_t * pattern, const char* text);
static int32_t matchcharclass(char c, const char* str);
static int32_t matchone(struct small_regex * pattern, struct regex_objs_t p, char c);
static int32_t matchdigit(char c);
static int32_t matchalpha(char c);
static int32_t matchwhitespace(char c);
static int32_t matchmetachar(char c, const char* str);
static int32_t matchrange(char c, const char* str);
static int32_t ismetachar(char c);
static int32_t __regex_compile_count(const char* pattern, struct args_reg_compile * args);

static int32_t __check_state_size(uint32_t sp, uint32_t * reacnt, uint32_t default_dep, void ** ptr, size_t structsize, uint32_t max_reallocs)
{
    // check if the stack pointer (SP) went out of range
    if (( sp >= ( (*reacnt)*default_dep) ) && (max_reallocs != 0)) {
        if  ( (*reacnt) >= max_reallocs) {
            LOGERR("%s can't realloc because limit was reached max: %u\r\n", __func__, max_reallocs);
            return 1;
        } else {
            void * tmp = KREALLOC((*ptr), (*reacnt) * default_dep * structsize);
            if (tmp == NULL) {
                LOGERR("%s realloc failed\r\n", __func__);
                // KFREE(stack);
                return 1;
            }

            DPROBE("reallocating: sp: %u reacnt:%u new size: %lu\r\n", sp, *reacnt, (*reacnt) * default_dep * structsize);

            (*reacnt)++;

            // 2025-11-15: fix warning
            // dereferencing pointer  does break strict-aliasing rules
            *ptr = tmp;  //ERROR: memcpy(*ptr, tmp, sizeof(void *));
        }
    }
    return 0;
}


// Public functions
int32_t regex_validate(struct small_regex * regex)
{
    ASSERT_NULL_R(regex, 1);
    uint32_t i;
    uint32_t count = COUNT_NODES(regex);

    //convert pattern
    struct regex_objs_t * objs = SECTION_OBJECTS(regex);

    for (i = 0; objs[i].type != UNUSED; ++i) {
        if ((count != 0) && (i >= count)) {
            LOGERR("regex_validator: reached end (count: %u) before the UNUSED node was reached\r\n", count);
            return 1;
        }

        if (objs[i].trueoffset >= count) {
            LOGERR("regex_validator: true offset is out of bound has: %u max: %u\r\n", objs[i].trueoffset, count);
            return 1;
        } else if (objs[i].falseoffset >= count) {
            LOGERR("regex_validator: false offset is out of bound has: %u max: %u\r\n", objs[i].falseoffset, count);
            return 1;
        }
    }
    return 0;
}


void regex_free(struct small_regex * regex)
{
    KFREE(regex);
}


int32_t regex_match(const char* pattern, const char* text)
{
    int32_t ret = -1;
    struct small_regex * reg = regex_compile(pattern);
    if (!reg) {
        LOGERR("failed at %s:%u\r\n", __func__, __LINE__);
    } else {
        ret = regex_matchp(reg, text);
        KFREE(reg);
    }
    return ret;
}


int32_t regex_matchp(struct small_regex * regx, const char* text)
{
    ASSERT_NULL_R(regx, -1);
    ASSERT_NULL_R(text, -1);

    struct regex_objs_t * objs = SECTION_OBJECTS(regx);
    int idx = -1;

    if (objs[0].type == BEGIN) {
        // starts from begin ^
        return ( (matchpattern(regx, objs, text)) ? 0 : -1 );
    } else {
        do {
            idx += 1;
            if (matchpattern(regx, objs, text)) {
                return idx;
            }
        } while (*text++ != '\0');

        return -1;
    }
}


static int32_t __regex_compile_count(const char* pattern, struct args_reg_compile * args)
{
    uint32_t i = 0;         // current character index
    unsigned char c = 0;    // character
    int32_t brcnt = 0;      // open brackets counter
    uint32_t realcnt = 1;   // reallocations

    args->ecnt++;
    args->stcnt = 1;


    while (pattern[i] != '\0') {
        c = pattern[i];

        if( c =='\\') {
            if (pattern[i+1] == '\0') {
                LOGERR("the escape charater without character, unexpected EOL\r\n"
                    "--> i=%u value=%s at %s:%u\r\n",
                    i,
                    &pattern[i],
                    __func__,
                    __LINE__);

                return 1;
            }

            i++;
        } else if ( (c == '*') || (c == '+') ) {
            // trying to predict how large the stack will be required
            args->stcnt++;
        } else if (c == '(') {
            brcnt++;
            args->inst[args->ecnt].trueoff = args->objs;
            args->ecnt++;

            // reallocate temp if necessary
            if (__check_state_size(args->ecnt, &realcnt, DEFAULT_TEMP_LEN, (void**)&args->inst, sizeof(struct re_pair_inst), MAX_TEMP_REALLOCS) == 1) {
                return 1;
            }
        } else if (c == ')') {
            args->inst[args->ecnt-(args->ecnt-brcnt)].failoff = args->objs;
            brcnt--;
        } else if (c == '[') {
            // Remember where pattern starts.
            uint32_t pat_begin = 0;
            uint32_t pat_len = 0;

            // Look-ahead to determine if negated
            if (pattern[i+1] == '^') {
                i += 1; // Increment i to avoid including '^' in the char-buffer
            }

            // [test]
            // storing start offset of the pattern
            pat_begin = i + 1;

            /* Copy characters inside [..] to buffer */
            while (pattern[++i] != ']') {
                // check if next is not null
                if (pattern[i] == '\0') {
                    LOGERR("the next element in pattern [] is NULL before reaching ] symbol\r\n"
                        "--> i=%u value=%s at %s:%u\r\n",
                        i,
                        &pattern[pat_begin],
                        __func__,
                        __LINE__);
                    return 1;
                }
            }

            // calculating the patt length
            pat_len = (i - pat_begin);
            args->strln += pat_len + 1; // + NULL
        }

        i++;
        args->objs++;
    }

    if (brcnt != 0) {
        LOGERR("round brackets mismatch has: %d at %s:%u\r\n", brcnt, __func__, __LINE__);
        return 1;
    }

    if (args->stcnt >= DEFAULT_STATES_DEP * MAX_STATE_REALLOCS) {
        LOGERR("it seems that in the worst case there will be required more states than it can handle max: %lu have: %d\r\n", DEFAULT_STATES_DEP * MAX_STATE_REALLOCS, args->stcnt);
    }

    // 'UNUSED' is a sentinel used to indicate end-of-pattern
    args->objs++;

    if (pattern[i-1] == '$') {
        // calculating the end of the main block begin..end
        args->inst[0].failoff = args->objs-2;
    } else {
        // calculating the end of the main block start..unused
        args->inst[0].failoff = args->objs-1;
    }
    return 0;
}


struct small_regex * regex_compile(const char* pattern)
{
    ASSERT_NULL_R(pattern, NULL);

    struct re_pair_inst * inst = NULL;
    struct small_regex * re_compiled = NULL;
    struct regex_objs_t * re_obj = NULL;

    // temp
    inst = (struct re_pair_inst*) KMALLOC(DEFAULT_TEMP_LEN, sizeof(struct re_pair_inst));
    if (inst == NULL) {
        LOGERR("calloc returned NULL at %s:%u\r\n", __func__, __LINE__);
        goto re_temp_error;
    }

    // calculating the length
    struct args_reg_compile args = {inst, 0,0,0,0};

    if (__regex_compile_count(pattern, &args) != 0) {
        // leave
        goto re_temp_error;
    }

    // calculating total size
    size_t totallen = ( args.objs * sizeof(struct regex_objs_t) ) + args.strln + sizeof(struct small_regex);

    re_compiled = (struct small_regex *) KMALLOC(1, totallen);

    if (re_compiled == NULL) {
        LOGERR("calloc returned NULL at %s:%u\r\n", __func__, __LINE__);
        goto re_temp_error;
    }

    // setting the fields
    re_compiled->objoffset = args.strln;
    re_compiled->pstsize = args.stcnt;
    re_compiled->totalsize = (uint32_t)(totallen - sizeof(struct small_regex));

    DPROBE("objs: %u pstsize: %u total data len: %u\r\n", re_compiled->objoffset, re_compiled->pstsize, re_compiled->totalsize);

    char * patrns = (char *) &re_compiled->data[0];

    // objects starts at end of the strln
    re_obj = (struct regex_objs_t *) &re_compiled->data[args.strln];

    char c;			        // current char in pattern
    uint32_t i = 0;         // index into pattern
    uint32_t j = 0;         // index into re_compiled
    uint32_t patoff = 0;    // pattern offset
    uint32_t ecnt = 0;      // grouping depth counter
    uint32_t ecntdip = 0;   // grouping index
    uint32_t resel = 0;     // instance selector i.e resel = ecntdip - (ecntdip - ecnt); or resel = ecntdip

    while (pattern[i] != '\0') {
        c = pattern[i];

        switch (c) {
        // Meta-characters:
        case '^':
            re_obj[j].type = BEGIN;
            re_obj[j].falseoffset = inst[resel].failoff;
            re_obj[j].trueoffset = j+1;
            break;

        case '$':
            re_obj[j].type = END;
            re_obj[j].falseoffset = 0;
            re_obj[j].trueoffset = j+1; // UNUSED
            break;

        case '.':
            re_obj[j].type = DOT;
            re_obj[j].falseoffset = 0;  // can't fail (ANY)
            re_obj[j].trueoffset = j+1;
            break;

        case '*':
            re_obj[j].type = STAR;

            if (re_obj[j-1].type == GROUPEND) {
                re_obj[j].falseoffset = inst[resel].failoff; // can fail because 0 or more
                re_obj[j].trueoffset = inst[resel+1].trueoff;
            } else {
                re_obj[j].falseoffset = inst[resel].failoff; // can't fail (from 0 or more)
                re_obj[j].trueoffset = j-1;
            }
            // ovveride the false of previous object to us
            re_obj[j-1].falseoffset = j;
            break;

        case '+':
            re_obj[j].type = PLUS;
            re_obj[j].falseoffset = inst[resel].failoff; //can fail because 1 or more

            if (re_obj[j-1].type == GROUPEND) {
                re_obj[j].trueoffset = inst[resel+1].trueoff;
            } else {
                re_obj[j].trueoffset = j-1;
            }
            // ovveride the false of previous object to +
            re_obj[j-1].falseoffset = j;
            break;

        case '?':
            re_obj[j].type = QUESTIONMARK;
            re_obj[j].falseoffset = 0; // can't fail because 0 to 1
            re_obj[j].trueoffset = j+1;
            re_obj[j].falseoffset = inst[resel].failoff; // can fail because 1 or more

            // ovveride the false of previous object to us
            re_obj[j-1].falseoffset = j;
            break;

        case '|':
            {
                uint32_t i;

                // we need to modify objects failoffset before the branch
                re_obj[j].type = BRANCH;
                re_obj[j].falseoffset = j+1;                // previous match failed
                re_obj[j].trueoffset = inst[resel].failoff; // previous match successfull

                DPROBE("j: %d F: %d T: %d resel:%d\r\n", j, re_obj[j].falseoffset, re_obj[j].trueoffset, resel);

                for (i = j-1; i >= inst[resel].trueoff; i++) {
                    //trace back and change fail offset
                    if (re_obj[i].type == BRANCH) {
                        break;
                    }

                    DPROBE("-[%d - %d]->%d\r\n", i, j-1, re_obj[i].falseoffset);
                    re_obj[i].falseoffset = j;
                    DPROBE("-->%d\r\n", re_obj[i].falseoffset);
                }
            }
            break;

        case '(':
            ecnt++;
            ecntdip++;
            resel = ecntdip;

            re_obj[j].type = GROUPSTART;
            re_obj[j].falseoffset = inst[resel].failoff;  // offset to the end of the group
            re_obj[j].trueoffset = j+1;
            break;

        case ')':
            DPROBE("ecnt: %d ecntdip: %d recel: %d-->", ecnt, ecntdip, resel);
            ecnt--;
            if (ecnt == 0) {
                resel = 0;
            } else {
                resel = ecntdip - (ecntdip - ecnt);
            }

            DPROBE("ecnt: %d ecntdip: %d recel: %d \r\n", ecnt, ecntdip, resel);

            re_obj[j].type = GROUPEND;
            re_obj[j].falseoffset = inst[resel].failoff;  // offset to the end of the block/group
            re_obj[j].trueoffset = j+1;
            break;

        /* Escaped character-classes (\s \w ...): */
        case '\\':
            if (pattern[i+1] != '\0') {
                /* ... and check the next */
                switch (pattern[i+1]) {
                /* Meta-character: */
                case 'd':
                    re_obj[j].type = DIGIT;
                    re_obj[j].trueoffset = j+1;
                    re_obj[j].falseoffset = inst[ecnt].failoff;
                    break;

                case 'D':
                    re_obj[j].type = NOT_DIGIT;
                    re_obj[j].trueoffset = j+1;
                    re_obj[j].falseoffset = inst[ecnt].failoff;
                    break;

                case 'w':
                    re_obj[j].type = ALPHA;
                    re_obj[j].trueoffset = j+1;
                    re_obj[j].falseoffset = inst[ecnt].failoff;
                    break;

                case 'W':
                    re_obj[j].type = NOT_ALPHA;
                    re_obj[j].trueoffset = j+1;
                    re_obj[j].falseoffset = inst[ecnt].failoff;
                    break;

                case 's':
                    re_obj[j].type = WHITESPACE;
                    re_obj[j].trueoffset = j+1;
                    re_obj[j].falseoffset = inst[ecnt].failoff;
                    break;

                case 'S':
                    re_obj[j].type = NOT_WHITESPACE;
                    re_obj[j].trueoffset = j+1;
                    re_obj[j].falseoffset = inst[ecnt].failoff;
                    break;

                /* Escaped character, e.g. '.' or '$' */
                default:
                    re_obj[j].type = CHAR;
                    re_obj[j].ch = pattern[i+1];
                    re_obj[j].trueoffset = j+1;
                    re_obj[j].falseoffset = inst[ecnt].failoff;
                    break;
                }

                // skip character
                i++;
            } else {
                LOGERR("%s:%u unexpected EOL\r\n", __func__, __LINE__);
                goto re_temp_error;
            }
            break;

        /* Character class: */
        case '[':
            {
                // Remember where pattern starts.
                uint32_t pat_begin = 0;
                uint32_t pat_len = 0;

                // Look-ahead to determine if negated
                if (pattern[i+1] == '^') {
                    re_obj[j].type = INV_CHAR_CLASS;
                    i += 1; // Increment i to avoid including '^' in the char-buffer
                } else {
                    re_obj[j].type = CHAR_CLASS;
                }

                // [test]
                // storing start offset of the pattern
                pat_begin = i + 1;

                /* Copy characters inside [..] to buffer */
                while (pattern[++i] != ']') {
                    // check if next is not null
                    if (pattern[i] == '\0') {
                        LOGERR("the next element in pattern [] is NULL before reaching ] symbol\r\n"
                            "--> i=%u value=%s at %s:%u\r\n",
                            i,
                            &pattern[pat_begin],
                            __func__,
                            __LINE__);

                        goto re_temp_error;
                    }
                }

                // calculating the patt length
                pat_len = (i - pat_begin);
                re_obj[j].ccl = patoff;

                // copy data to the structure, unsafe operation
                memmove((void*)&patrns[patoff], (const void*) &pattern[pat_begin], pat_len);

                patoff += pat_len + 1; //+null

                re_obj[j].trueoffset = j+1;
                re_obj[j].falseoffset = inst[ecnt].failoff;
            }
            break;

        // Other characters:
        default:
            re_obj[j].type = CHAR;
            re_obj[j].ch = c;
            re_obj[j].trueoffset = j+1;
            re_obj[j].falseoffset = inst[ecnt].failoff;
            break;
        }

        i++;
        j++;
    }

    // 'UNUSED' is a sentinel used to indicate end-of-pattern
    re_obj[j].type = UNUSED;

    // move next, j now indicates the amount of the recs from 0..j-1
    j++;

    KFREE(inst);
    return re_compiled;

re_temp_error:
    KFREE(inst);
    KFREE(re_compiled);
    return NULL;
}


uint32_t regex_get_size(struct small_regex * regex)
{
    return (regex? regex->totalsize : 0);
}


#ifdef RE_BUILDWITH_DEBUG
void regex_print(struct small_regex * pattern)
{
    ASSERT_NULL_R(pattern, );

    uint32_t i;
    uint32_t count = COUNT_NODES(pattern);

    //convert pattern
    struct regex_objs_t * objs = SECTION_OBJECTS(pattern);

    for (i = 0; objs[i].type != UNUSED; ++i) {
        if ((count != 0) && (i >= count)) {
            printf("!!!<>>\r\n");
            break;
        }

        switch (objs[i].type) {
        case CHAR_CLASS:
        case INV_CHAR_CLASS:
            printf("[");
            printf("%s", OFFSET_TO_PATTERN(pattern, objs[i].ccl));
            printf("]");
            break;

        case CHAR:
            printf("%c", objs[i].ch);
            break;

        default:
            printf("%s", typestr[objs[i].type]);
            break;
        }
    }

    printf("\r\n");
    return;
}


void regex_trace(small_regex_t * pattern)
{
    ASSERT_NULL_R(pattern, );

    uint32_t count = COUNT_NODES(pattern);

    //convert pattern
    struct regex_objs_t * objs = SECTION_OBJECTS(pattern);
    uint32_t i = 0;

    for ( i = 0; objs[i].type != UNUSED; ++i) {
        if ((count != 0) && (i >= count)) {
            printf("reached end (count: %u) before the UNUSED node was reached\r\n", count);
            break;
        }

        printf("[%u] type: %s  true: %u  false: %u ", i, typenames[objs[i].type], objs[i].trueoffset, objs[i].falseoffset);
        if (objs[i].type == CHAR_CLASS || objs[i].type == INV_CHAR_CLASS) {
            printf(" [");
            printf("%s", OFFSET_TO_PATTERN(pattern, objs[i].ccl));
            printf("]");
        } else if (objs[i].type == CHAR) {
            printf(" '%c'", objs[i].ch);
        }
        printf("\n");
    }

    printf("[%u] type: %s\r\n", i, typenames[objs[i].type]);
    return;
}


void regex_printstrlegend(void)
{
    printf(DCOLOR_RED"RED - st0"DCOLOR_RESET" / "DCOLOR_GREEN"GREEN - matchcnt"DCOLOR_RESET" / "DCOLOR_BLUE" BLUE - < st"DCOLOR_RESET" / "DCOLOR_YELLOW" YELLOW - =st"DCOLOR_RESET " / WHITE - rest\r\n");
}


void regex_printstr(const char * text, size_t textlen, struct sstate_t * sstate)
{
    for (size_t i = 0; i<textlen; i++) {
        if (i == sstate->st0) {
            printf(DCOLOR_RED"%c"DCOLOR_RESET, text[i]);
        } else if ( ((sstate->st - sstate->matchcnt) < i) && (i < sstate->st) && (sstate->matchcnt != 0) ) {
            printf(DCOLOR_GREEN"%c"DCOLOR_RESET, text[i]);
        } else if ( (i != sstate->st0) && (i < sstate->st)) {
            printf(DCOLOR_BLUE"%c"DCOLOR_RESET, text[i]);
        } else if (i == sstate->st) {
            printf(DCOLOR_YELLOW"%c"DCOLOR_RESET, text[i]);
        } else {
            printf("%c", text[i]);
        }
    }
    printf("\r\n");
}

#endif


static int32_t matchdigit(char c)
{
    DPROBE("-->%s \\d with %c\r\n", __func__, c);
    return ((c >= '0') && (c <= '9'));
}


static int32_t matchalpha(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}


static int32_t matchwhitespace(char c)
{
    DPROBE("-->%s \\s with %c\r\n", __func__, c);
    return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\f') || (c == '\v'));
}


static int32_t matchalphanum(char c)
{
    DPROBE("-->%s \\w with %c\r\n", __func__, c);
    return ((c == '_') || matchalpha(c) || matchdigit(c));
}


static int32_t matchrange(char c, const char* str)
{
    DPROBE("-->%s %s with %c\r\n", __func__, str, c);
    return ((c != '-') && (str[0] != '-') && (str[1] == '-') && (str[2] != '\0') && ((c >= str[0]) && (c <= str[2])));
}


static int32_t ismetachar(char c)
{
    return ((c == 's') || (c == 'S') == (c == 'w') || (c == 'W') || (c == 'd') || (c == 'D'));
}


static int32_t matchmetachar(char c, const char* str)
{
    switch (str[0])	{
    case 'd':
        return  matchdigit(c);

    case 'D':
        DPROBE("-->%s not \\D with %c\r\n", __func__, c);
        return !matchdigit(c);

    case 'w':
        return  matchalphanum(c);

    case 'W':
        DPROBE("-->%s not \\W with %c\r\n", __func__, c);
        return !matchalphanum(c);

    case 's':
        return  matchwhitespace(c);

    case 'S':
        DPROBE("-->%s \\S with %c\r\n",  __func__, c);
        return !matchwhitespace(c);

    default:
        DPROBE("-->%s %c with %c\r\n",  __func__, str[0], c);
        return (c == str[0]);
    }

    return (c == str[0]);
}


static int32_t matchcharclass(char c, const char* str)
{
    do {
        if (matchrange(c, str)) {
            return 1;
        } else if (str[0] == '\\') {
            // Escape-char: increment str-ptr and match on next char
            str += 1;

            if (matchmetachar(c, str)) {
                return 1;
            } else if ((c == str[0]) && !ismetachar(c)) {
                return 1;
            }
        } else if (c == str[0]) {
            if (c == '-') {
                return ((str[-1] == '\0') || (str[1] == '\0'));
            } else {
                return 1;
            }
        }
    } while (*str++ != '\0');

    return 0;
}


static int32_t matchone(struct small_regex * pattern, struct regex_objs_t p, char c)
{
    switch (p.type) {
    case DOT:
        DPROBE("-->%s . with %c\r\n", __func__, c);
        if ( (c == '\n') || (c == '\r') ) {
            return 0;
        } else {
            return 1;
        }

    case CHAR_CLASS:
        return matchcharclass(c, (const char*)OFFSET_TO_PATTERN(pattern, p.ccl));

    case INV_CHAR_CLASS:
        return !matchcharclass(c, (const char*)OFFSET_TO_PATTERN(pattern, p.ccl));

    case DIGIT:
        return matchdigit(c);

    case NOT_DIGIT:
        return !matchdigit(c);

    case ALPHA:
        return matchalphanum(c);

    case NOT_ALPHA:
        return !matchalphanum(c);

    case WHITESPACE:
        return matchwhitespace(c);

    case NOT_WHITESPACE:
        return !matchwhitespace(c);

    case CHAR:
        DPROBE("-->%s %c with %c\r\n",__func__,  p.ch, c);
        return (p.ch == c);

    default:
        return -1;
    }

    // SHOULD NOT REACH THIS POINT
    return  (p.ch == c);
}


static int32_t matchpattern(struct small_regex * reg, struct regex_objs_t * pattern, const char* text)
{
    uint32_t stackrealcnt = 1;

    if (reg->pstsize == 0) {
        reg->pstsize = DEFAULT_STATES_DEP;
    }

    struct sstate_t * sstate = (struct sstate_t *) KMALLOC(reg->pstsize, sizeof(struct sstate_t));

    if (sstate == NULL) {
        LOGERR("%s malloc failed\r\n", __func__);
        return 0;
    }

#ifdef RE_BUILDWITH_DEBUG
    size_t textlen = strlen(text);  // length of the string
#endif

    uint32_t sp = 0;                // sstate pointer
    int32_t evalres = 0;            // evaluation result accomulator
    int32_t evalres0 = 0;           // evaluation result accomulator
    int32_t brtxcoff = 0;           // used for group+branch

    // reset first intance
    memset(&SSTP, 0, sizeof(struct sstate_t));
    SSTP.st0 = -1;

    // print legend if compiled with the debug
    regex_printstrlegend();

    for (;;) {
        regex_printstr(text, textlen, &SSTP);

        switch (pattern[SSTP.offset].type) {
        case BEGIN:
            DPROBE("BEGIN\r\n");
            SSTP.offset = pattern[SSTP.offset].trueoffset;
            break;

        case UNUSED:
        case END:
            if ( (evalres == 0) && (sp > 0)) {
                POP();
                DPROBE("returning to [%.4d](%s) sp:%d st:%d st0:%d text[st]:%c\r\n", SSTP.offset, typenames[pattern[SSTP.offset].type], sp, SSTP.st, SSTP.st0, text[SSTP.st]);
            } else {
                DPROBE("EXIT\r\n");
                if (pattern[SSTP.offset].type == END) {
                    uint32_t est = SSTP.st;
                    KFREE(sstate);
                    return (text[est] == '\0') ? evalres : 0;
                } else {
                    KFREE(sstate);
                    return evalres;
                }
            }
            break;

        case QUESTIONMARK:
            SSTP.offset = pattern[SSTP.offset].trueoffset;
            evalres = 1; // anyway it is true
            SSTP.matchcnt = 0;
            break;

        case STAR:
        case PLUS:
            {
                if ( evalres == 1 ) {
                    // increent counter
                    SSTP.matchcnt++;
                }

                if ( ( evalres == 1 ) && (text[SSTP.st] != '\0') ) {
                    int32_t offset0 = SSTP.offset + 1;
                    SSTP.offset = pattern[SSTP.offset].trueoffset;

                    // check further if it matches the next char
                    if ( ( (pattern[offset0].type != END) && (pattern[offset0].type != UNUSED)) && (pattern[offset0].type != BRANCH) && (SSTP.st0 != SSTP.st) ) {
                        DPROBE("PROBING FORWARD: offset0: %d text: %c st: %d\r\n", offset0, text[SSTP.st], SSTP.st);

                        // check next character in order to see how likely the + or * should be left
                        int32_t tr = matchone(reg, pattern[offset0], text[SSTP.st]);

                        switch (tr) {
                        case -1: //unknown operation
                        case 1:
                            DPROBE("PROBING FORWARD: SUCCESS\r\n");

                            // store where we was before the push
                            SSTP.st0 = SSTP.st;

                            DPROBE("PUSH from [%.4d](%s) sp:%d st:%d st0:%d text[st]:%c\r\n", SSTP.offset, typenames[pattern[SSTP.offset].type], sp, SSTP.st, SSTP.st0, text[SSTP.st]);

                            // push to the stack
                            PUSH();

                            // check if stack requires to be extended
                            if (__check_state_size(sp, &stackrealcnt, DEFAULT_STATES_DEP, (void**)&sstate, sizeof(struct sstate_t), MAX_STATE_REALLOCS) == 1) {
                                goto somethingwentwrong;
                            }

                            // reset new instance
                            // SSTP.offset = offset0;
                            SSTP.st0 = -1;

                            SSTP.offset = pattern[offset0].trueoffset;
                            SSTP.st++;// increment current section match

                            DPROBE("PUSH to [%.4d](%s) sp:%d st:%d st0:%d text[st]:%c\r\n", SSTP.offset, typenames[pattern[SSTP.offset].type], sp, SSTP.st, SSTP.st0, text[SSTP.st]);
                            break;
                        case 0:
                            break;
                        }
                    }
                } else {
                    if ( pattern[SSTP.offset].type == PLUS ) {
                        if (SSTP.matchcnt > 0) {
                            // SSTP.txshift++;
                            evalres = 1;
                        } else {
                            evalres = 0;
                        }
                    } else if (pattern[SSTP.offset].type == STAR) {
                        evalres = 1;
                    }

                    if (evalres == 1) {
                        SSTP.offset++;
                    } else {
                        SSTP.offset = pattern[SSTP.offset].falseoffset;
                    }

                    SSTP.matchcnt = 0;
                }
            }
            break;

        case GROUPSTART: // entering group
            DPROBE("ENTERING GROUP from [%.4d](%s) ", SSTP.offset, typenames[pattern[SSTP.offset].type]);

            SSTP.offset++;
            brtxcoff = SSTP.st; // reset current offset

            DPROBE("nextoffset [%.4d](%s) st: %d brtxcoff: %d\r\n", SSTP.offset, typenames[pattern[SSTP.offset].type], SSTP.st, brtxcoff);
            break;

        case GROUPEND: // leaving group T: next F: EXIT or next GROUPEND/BRANCH
            DPROBE("LEAVING GROUP from [%.4d](%s) ", SSTP.offset, typenames[pattern[SSTP.offset].type]);
            if (evalres == 0) {
                // restore the st
                SSTP.st = brtxcoff;
                SSTP.offset = pattern[SSTP.offset].falseoffset;
            } else {
                SSTP.offset = pattern[SSTP.offset].trueoffset;
                brtxcoff = 0; // reset current offset
            }
            DPROBE("nextoffset [%.4d](%s) st: %d brtxcoff: %d\r\n", SSTP.offset, typenames[pattern[SSTP.offset].type], SSTP.st, brtxcoff);
            break;

        case BRANCH: // BRANCHING
            {
                DPROBE("BRANCHING from [%.4d](%s) ", SSTP.offset, typenames[pattern[SSTP.offset].type]);
                if (evalres == 0) {
                    // restore the st
                    SSTP.st = brtxcoff;
                    SSTP.offset = pattern[SSTP.offset].falseoffset;
                } else {
                    SSTP.offset = pattern[SSTP.offset].trueoffset;
                    brtxcoff = 0; // reset current offset
                }

                DPROBE("nextoffset [%.4d](%s) st: %d brtxcoff: %d\r\n", SSTP.offset, typenames[pattern[SSTP.offset].type], SSTP.st, brtxcoff);
            }
            break;

        default:
            // call handler
            if ( /*(evalres == 0) &&*/ (text[SSTP.st] == '\0') ) {
                SSTP.offset = pattern[SSTP.offset].falseoffset;
                evalres = 0;
                continue;
            }

            // store previous sstate of operation evaluation
            evalres0 = evalres;
            evalres = matchone(reg, pattern[SSTP.offset], text[SSTP.st]);

            switch (evalres) {
            case 1:
                SSTP.offset = pattern[SSTP.offset].trueoffset;
                SSTP.st++;// increment current section match
                break;

            case 0:
                SSTP.offset = pattern[SSTP.offset].falseoffset;
                break;

            default:
            case -1: // unknown operation
                evalres = evalres0;
                break;
            }
            break;
        }
    } // while

somethingwentwrong:
    KFREE(sstate);
    return 0;
}
