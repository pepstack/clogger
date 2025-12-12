/*******************************************************************************
 * uthash                                                                      *
 *                                                                             *
 * Copyright (c) 2003-2020, Troy D. Hanson                                     *
 * http://troydhanson.github.com/uthash/                                       *
 * All rights reserved.                                                        *
 *                                                                             *
 * Redistribution and use in source and binary forms, with or without          *
 * modification, are permitted provided that the following conditions are met: *
 *                                                                             *
 * Redistributions of source code must retain the above copyright notice, this *
 * list of conditions and the following disclaimer.                            *
 *                                                                             *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" *
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE   *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE  *
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE    *
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR         *
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF        *
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    *
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN     *
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)     *
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE  *
 * POSSIBILITY OF SUCH DAMAGE.                                                 *
 *******************************************************************************/
/**
 * @file uthash_incl.h
 * @brief uthash api
 *
 * @author cheungmine@qq.com
 * @copyright © 2024-2025 anomura.cc All Rights Reserved.
 * @version 1.0.20
 *
 * @since 2020-04-30 13:50:10
 * @date 2025-12-06 22:02:59
 */
#ifndef UTHASH_INCL_H__
#define UTHASH_INCL_H__

#include "uthash/uthash.h"
#include "uthash/utarray.h"

///////////////////////////////////////////////////////////////////////
// uthash extension
///////////////////////////////////////////////////////////////////////

#define HASH_FIND_STR_LEN(head,findstr,findstrlen,out)                           \
do {                                                                             \
    unsigned _uthash_hfstr_keylen = (unsigned)(findstrlen);                      \
    HASH_FIND(hh, head, findstr, _uthash_hfstr_keylen, out);                     \
} while (0)

#define HASH_ADD_STR_LEN(head,strfield,strfieldlen,add)                          \
do {                                                                             \
    unsigned _uthash_hastr_keylen = (unsigned)(strfieldlen);                     \
    HASH_ADD(hh, head, strfield[0], _uthash_hastr_keylen, add);                  \
} while (0)

#define HASH_REPLACE_STR_LEN(head,strfield,strfieldlen,add,replaced)             \
do {                                                                             \
    unsigned _uthash_hrstr_keylen = (unsigned)(strfieldlen);                     \
    HASH_REPLACE(hh, head, strfield[0], _uthash_hrstr_keylen, add, replaced);    \
} while (0)


///////////////////////////////////////////////////////////////////////
// utarray extension
///////////////////////////////////////////////////////////////////////
typedef UT_array   UTArray;
typedef UTArray* UTArrayPtr;


#define UTArrayInit(arr, icd)              utarray_init(((UTArrayPtr)(&(arr))), &icd)
#define UTArrayUninit(arr)                 utarray_done(((UTArrayPtr)(&(arr))))

#define UTArrayReserve(arr, elts)          utarray_reserve(((UTArrayPtr)(&(arr))), ((unsigned)(elts)))

#define UTArrayAdd(arr, elt)               utarray_push_back(((UTArrayPtr)(&(arr))), &(elt))

#define UTArrayLen(arr)                    ((int) utarray_len(((UTArrayPtr)(&(arr)))))

#define UTArrayFront(arr)                  utarray_front(((UTArrayPtr)(&(arr))))
#define UTArrayFrontElt(arr, EltType)      ((EltType *) utarray_front(((UTArrayPtr)(&(arr)))))

// do check idx
#define UTArrayGet(arr, idx)               utarray_eltptr(((UTArrayPtr)(&(arr))), ((unsigned)(idx)))
#define UTArrayGetElt(arr, idx, EltType)   ((EltType *) utarray_eltptr(((UTArrayPtr)(&(arr))), ((unsigned)(idx))))

// no check idx
#define UTArrayElt(arr, idx)               _utarray_eltptr(((UTArrayPtr)(&(arr))), ((unsigned)(idx)))
#define UTArrayEltType(arr, idx, EltType)  ((EltType *)_utarray_eltptr(((UTArrayPtr)(&(arr))), ((unsigned)(idx))))

#define UTArrayNext(arr, eltp)             utarray_next(((UTArrayPtr)(&(arr))), (eltp))
#define UTArrayNextElt(arr, eltp, EltType) ((EltType *) utarray_next(((UTArrayPtr)(&(arr))), (eltp)))
#define UTArrayClear(arr)                  utarray_clear(((UTArrayPtr)(&(arr))))

#define UTArrayBack(arr)                   utarray_back(((UTArrayPtr)(&(arr))))
#define UTArrayBackElt(arr, EltType)       ((EltType *) utarray_back(((UTArrayPtr)(&(arr)))))

#define UTArraySort(arr, sortcmp)          utarray_sort(((UTArrayPtr)(&(arr))), &sortcmp)

#define UTArrayPtrNew(parr, icd)           utarray_new((parr), &icd)
#define UTArrayPtrAdd(parr, elt)           utarray_push_back((parr), &(elt))
#define UTArrayPtrLen(parr)                ((int) utarray_len((parr)))


#endif /* UTHASH_INCL_H__ */
