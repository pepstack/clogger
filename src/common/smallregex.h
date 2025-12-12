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
 * smallregex.h
 *
 *   https://gitlab.com/relkom/small-regex
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
#ifndef SMALL_REGEX_H_
#define SMALL_REGEX_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _STDINT
    #include <stdint.h>
#endif

#ifndef _KERNEL
    #include <stdint.h>
#endif

/** only enable below defines if debug
 *
 * #define RE_BUILDWITH_DEBUG
 * #define RE_BUILDWITH_PROBES
 * #define BUILD_WITH_ERRORMSG
 * #define RE_BUILDWITH_TRACEMATCH_PROC
 */

// exporting
#ifdef _KERNEL
    MALLOC_DECLARE(MC_RE);
#endif //_KERNEL

#ifndef DEFAULT_TEMP_LEN
    // the initial buffer allocated for the initial offset calculations (in compiler)
    #define DEFAULT_TEMP_LEN       40UL
#endif

#ifndef MAX_TEMP_REALLOCS
    // maximum stack depth i.e 10 will be 10*DEFAULT_TEMP_LEN(40)*sizeof(struct re_temp i.e 8) = 3.2kB
    #define MAX_TEMP_REALLOCS      10UL
#endif

#ifndef DEFAULT_STATES_DEP
    // the initial stack length
    #define DEFAULT_STATES_DEP     40UL
#endif

#ifndef MAX_STATE_REALLOCS
    // maximum stack depth i.e 10 will be 10*DEFAULT_STACK_DEP(40)*sizeof(struct stack i.e 16) = 6.4kB
    #define MAX_STATE_REALLOCS     10UL
#endif


/*
 * struct regex_objs_t
 *  the regex node
 */
typedef struct regex_objs_t {
    uint8_t  type;          // CHAR, STAR, etc.
    uint32_t trueoffset;    // offset when eval is true
    uint32_t falseoffset;   // offset when eval is false
    union {
        uint8_t  ch;        //the character itself
        uint32_t ccl;       //an offset to characters in class
    };
} regex_objs_t;

/*
 * struct small_regex
 * An instance of the compiled regex
 */
typedef struct small_regex {
    uint32_t objoffset;     // indicates the start of the regex objs
    uint32_t totalsize;     // total size of data[]
    uint32_t pstsize;       // predicted stack size
    uint8_t data[];         // data
} small_regex_t;

/* returns the pointer to the struct regex_objs_t instances */
#define SECTION_OBJECTS(_pat_) \
    (struct regex_objs_t *) &_pat_->data[_pat_->objoffset]

/* pointer to the pattern from offset */
#define OFFSET_TO_PATTERN(_pat_, _off_) \
    &_pat_->data[_off_]

/* calculate nodes */
#define COUNT_NODES(_pat_) \
    (_pat_->totalsize - _pat_->objoffset) / sizeof(struct regex_objs_t)

#define RE_REGEX_SIZE(_pat_) \
    (sizeof(struct small_regex) + _pat_->totalsize)

/*
 * regex_validate
 * Validates the offsets to avoid jump out of the scope.
 *  Takes O(n) time.
 *
 * regex (struct small_regex*) a valid pointer to the compiled regex pattern
 * returns: 0 on success, 1 on error and LOGERR message
 */
int32_t regex_validate(struct small_regex * regex);

/*
 * regex_free
 * Deallocates the regex_instance
 *
 * regex (struct small_regex*) a valid pointer to the compiled regex pattern
 * returns: void
 */
void regex_free(struct small_regex * regex);

/* regex_compile
 *
 * Compiles the regex string pattern to struct regex_objs_t array and stored
 *  in the data section of the struct small_regex.
 * The returned result located in HEAP! use free();
 * The size of the data section is stored in the field totalsize.
 * But, the array can be readed until the instance of struct regex_objs_t field.type == UNUSED
 *
 * pattern (const char*) regex pattern (null terminated string)
 * returns: a valid pointer to the intance or NULL on fail
 */
struct small_regex * regex_compile(const char* pattern);

/* regex_get_size
 *
 * Returns the value of totalsize field.
 *  calculated with formula: totallen - sizeof(struct small_regex);
 *
 * regex (struct small_regex*) a valid pointer to the compiled regex pattern
 * returns: (uint32_t) totallen in bytes exc size of struct small_regex
 */
uint32_t regex_get_size(struct small_regex * regex);

/* regex_matchp
 * Find matches of the compiled pattern inside text.
 *
 * regex (struct small_regex*) a valid pointer to the compiled regex pattern
 * text (const char*) a text on which pattern is applied
 * returns: 0 or larger on success, -1 on error
 */

int32_t regex_matchp(struct small_regex * pattern, const char* text);

/* regex_match
 * Find matches of the txt pattern inside text (will compile automatically first).
 */
int32_t regex_match(const char* pattern, const char* text);


#ifndef BUILD_WITH_ERRORMSG
    #define LOGERR(...)
#else
    #ifdef _KERNEL
        #define LOGERR(...) printf(__VA_ARGS__)
    #else
        #define LOGERR(...) fprintf(stderr, __VA_ARGS__)
    #endif
#endif


#ifdef RE_BUILDWITH_DEBUG
    // debug
    #define DCOLOR_RED     "\x1b[31m"
    #define DCOLOR_GREEN   "\x1b[32m"
    #define DCOLOR_YELLOW  "\x1b[33m"
    #define DCOLOR_BLUE    "\x1b[34m"
    #define DCOLOR_MAGENTA "\x1b[35m"
    #define DCOLOR_CYAN    "\x1b[36m"
    #define DCOLOR_RESET   "\x1b[0m"

    #ifdef RE_BUILDWITH_PROBES
        #define DPROBE(...) fprintf(stdout, __VA_ARGS__)
    #else
        #define DPROBE(...)
    #endif

    struct state;

    void regex_trace(struct small_regex * pattern);
    void regex_print(struct small_regex * pattern);

    #ifdef RE_BUILDWITH_TRACEMATCH_PROC
        void regex_printstrlegend(void);
        void regex_printstr(const char * text, size_t textlen, struct state * state);
    #else
        #define regex_printstr(...)
        #define regex_printstrlegend(...)
    #endif

#else
    #define regex_trace(...)
    #define regex_print(...)
    #define regex_printstr(...)
    #define regex_printstrlegend(...)
    #define DPROBE(...)
#endif

#ifdef __cplusplus
}
#endif
#endif /* SMALL_REGEX_H_ */