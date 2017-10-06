/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    stdint.h

Abstract:

    This header contains definitions for integer types.

Author:

    Evan Green 3-May-2013

--*/

#ifndef _STDINT_H
#define _STDINT_H

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define builtin compiler macros if they're not supplied.
//

#ifndef __INT8_TYPE__
#define __INT8_TYPE__ char
#define __INT8_MAX__ 0x7F
#define __INT8_C(_Value) ((int8_t) + (_Value))
#endif

#ifndef __INT16_TYPE__
#define __INT16_TYPE__ short
#define __INT16_MAX__ 0x7FFF
#define __INT16_C(_Value) ((int16_t) + (_Value))
#endif

#ifndef __INT32_TYPE__
#define __INT32_TYPE__ int
#define __INT32_MAX__ 0x7FFFFFFF
#define __INT32_C(_Value) _Value##L
#endif

#ifndef __INT64_TYPE__
#define __INT64_TYPE__ long long
#define __INT64_MAX__ 0x7FFFFFFFFFFFFFFFLL
#define __INT64_C(_Value) _Value##LL
#endif

#ifndef __UINT8_TYPE__
#define __UINT8_TYPE__ unsigned char
#define __UINT8_MAX__ 0xFF
#define __UINT8_C(_Value) ((uint8_t) + (_Value##U))
#endif

#ifndef __UINT16_TYPE__
#define __UINT16_TYPE__ unsigned short
#define __UINT16_MAX__ 0xFFFF
#define __UINT16_C(_Value) ((uint16_t) + (_Value##U))
#endif

#ifndef __UINT32_TYPE__
#define __UINT32_TYPE__ unsigned int
#define __UINT32_MAX__ 0xFFFFFFFF
#define __UINT32_C(_Value) _Value##UL
#endif

#ifndef __UINT64_TYPE__
#define __UINT64_TYPE__ unsigned long long
#define __UINT64_MAX__ 0xFFFFFFFFFFFFFFFFULL
#define __UINT64_C(_Value) _Value##ULL
#endif

#ifndef __INT_LEAST8_TYPE__
#define __INT_LEAST8_TYPE__ __INT8_TYPE__
#define __INT_LEAST8_MAX__ __INT8_MAX__
#endif

#ifndef __INT_LEAST16_TYPE__
#define __INT_LEAST16_TYPE__ __INT16_TYPE__
#define __INT_LEAST16_MAX__ __INT16_MAX__
#endif

#ifndef __INT_LEAST32_TYPE__
#define __INT_LEAST32_TYPE__ __INT32_TYPE__
#define __INT_LEAST32_MAX__ __INT32_MAX__
#endif

#ifndef __INT_LEAST64_TYPE__
#define __INT_LEAST64_TYPE__ __INT64_TYPE__
#define __INT_LEAST64_MAX__ __INT64_MAX__
#endif

#ifndef __UINT_LEAST8_TYPE__
#define __UINT_LEAST8_TYPE__ __UINT8_TYPE__
#define __UINT_LEAST8_MAX__ __UINT8_MAX__
#endif

#ifndef __UINT_LEAST16_TYPE__
#define __UINT_LEAST16_TYPE__ __UINT16_TYPE__
#define __UINT_LEAST16_MAX__ __UINT16_MAX__
#endif

#ifndef __UINT_LEAST32_TYPE__
#define __UINT_LEAST32_TYPE__ __UINT32_TYPE__
#define __UINT_LEAST32_MAX__ __UINT32_MAX__
#endif

#ifndef __UINT_LEAST64_TYPE__
#define __UINT_LEAST64_TYPE__ __UINT64_TYPE__
#define __UINT_LEAST64_MAX__ __UINT64_MAX__
#endif

#ifndef __INT_FAST8_TYPE__
#define __INT_FAST8_TYPE__ __INT8_TYPE__
#define __INT_FAST8_MAX__ __INT8_MAX__
#endif

#ifndef __INT_FAST16_TYPE__
#define __INT_FAST16_TYPE__ __INT16_TYPE__
#define __INT_FAST16_MAX__ __INT16_MAX__
#endif

#ifndef __INT_FAST32_TYPE__
#define __INT_FAST32_TYPE__ __INT32_TYPE__
#define __INT_FAST32_MAX__ __INT32_MAX__
#endif

#ifndef __INT_FAST64_TYPE__
#define __INT_FAST64_TYPE__ __INT64_TYPE__
#define __INT_FAST64_MAX__ __INT64_MAX__
#endif

#ifndef __UINT_FAST8_TYPE__
#define __UINT_FAST8_TYPE__ __UINT8_TYPE__
#define __UINT_FAST8_MAX__ __UINT8_MAX__
#endif

#ifndef __UINT_FAST16_TYPE__
#define __UINT_FAST16_TYPE__ __UINT16_TYPE__
#define __UINT_FAST16_MAX__ __UINT16_MAX__
#endif

#ifndef __UINT_FAST32_TYPE__
#define __UINT_FAST32_TYPE__ __UINT32_TYPE__
#define __UINT_FAST32_MAX__ __UINT32_MAX__
#endif

#ifndef __UINT_FAST64_TYPE__
#define __UINT_FAST64_TYPE__ __UINT64_TYPE__
#define __UINT_FAST64_MAX__ __UINT64_MAX__
#endif

#ifndef __INTPTR_TYPE__
#if (__SIZEOF_POINTER__ == 8)
#define __INTPTR_TYPE__ long
#define __INTPTR_MAX__ __LONG_MAX__
#else
#define __INTPTR_TYPE__ int
#define __INTPTR_MAX__ __INT_MAX__
#endif
#endif

#ifndef __UINTPTR_TYPE__
#if (__SIZEOF_POINTER__ == 8)
#define __UINTPTR_TYPE__ unsigned long
#define __UINTPTR_MAX__ ((2 * __LONG_MAX__) + 1)
#else
#define __UINTPTR_TYPE__ unsigned int
#define __UINTPTR_MAX__ ((2 * __INT_MAX__) + 1)
#endif
#endif

#ifndef __INTMAX_C
#define __INTMAX_C(_Value)  INT64_C(_Value)
#endif

#ifndef __UINTMAX_C
#define __UINTMAX_C(_Value) UINT64_C(_Value)
#endif

//
// Define the boundaries of the intN_t types.
//

#define INT8_MIN   (-INT8_MAX - 1)
#define INT8_MAX   __INT8_MAX__
#define INT16_MIN  (-INT16_MAX - 1)
#define INT16_MAX  __INT16_MAX__
#define INT32_MIN  (-INT32_MAX - 1)
#define INT32_MAX  __INT32_MAX__
#define INT64_MIN  (-INT64_MAX - 1)
#define INT64_MAX  __INT64_MAX__

#define UINT8_MAX  __UINT8_MAX__
#define UINT16_MAX __UINT16_MAX__
#define UINT32_MAX __UINT32_MAX__
#define UINT64_MAX __UINT64_MAX__

//
// Define the boundaries of the int_leastN_t types.
//

#define INT_LEAST8_MIN   (-INT_LEAST8_MAX - 1)
#define INT_LEAST8_MAX   __INT_LEAST8_MAX__
#define INT_LEAST16_MIN  (-INT_LEAST16_MAX - 1)
#define INT_LEAST16_MAX  __INT_LEAST16_MAX__
#define INT_LEAST32_MIN  (-INT_LEAST32_MAX - 1)
#define INT_LEAST32_MAX  __INT_LEAST32_MAX__
#define INT_LEAST64_MIN  (-INT_LEAST64_MAX - 1)
#define INT_LEAST64_MAX  __INT_LEAST64_MAX__

#define UINT_LEAST8_MAX  __UINT_LEAST8_MAX__
#define UINT_LEAST16_MAX __UINT_LEAST16_MAX__
#define UINT_LEAST32_MAX __UINT_LEAST32_MAX__
#define UINT_LEAST64_MAX __UINT_LEAST64_MAX__

//
// Define the boundaries of the int_fastN_t types.
//

#define INT_FAST8_MIN   (-INT_FAST8_MAX - 1)
#define INT_FAST8_MAX   __INT_FAST8_MAX__
#define INT_FAST16_MIN  (-INT_FAST16_MAX - 1)
#define INT_FAST16_MAX  __INT_FAST16_MAX__
#define INT_FAST32_MIN  (-INT_FAST32_MAX - 1)
#define INT_FAST32_MAX  __INT_FAST32_MAX__
#define INT_FAST64_MIN  (-INT_FAST64_MAX - 1)
#define INT_FAST64_MAX  __INT_FAST64_MAX__

#define UINT_FAST8_MAX  __UINT_FAST8_MAX__
#define UINT_FAST16_MAX __UINT_FAST16_MAX__
#define UINT_FAST32_MAX __UINT_FAST32_MAX__
#define UINT_FAST64_MAX __UINT_FAST64_MAX__

//
// Define the boundaries of the pointer types.
//

#define INTPTR_MIN  (-INTPTR_MAX - 1)
#define INTPTR_MAX  __INTPTR_MAX__
#define UINTPTR_MAX __UINTPTR_MAX__

//
// Define the boundaries of the max int types.
//

#define INTMAX_MIN  (-INTMAX_MAX - 1)
#define INTMAX_MAX  __INTMAX_MAX__
#define UINTMAX_MAX __UINTMAX_MAX__

//
// Define the boundaries of some types not defined here.
//

#define PTRDIFF_MIN (-PTRDIFF_MAX - 1)
#define PTRDIFF_MAX __PTRDIFF_MAX__

#define SIG_ATOMIC_MIN (-SIG_ATOMIC_MAX - 1)
#define SIG_ATOMIC_MAX __SIG_ATOMIC_MAX__

#define SIZE_MAX __SIZE_MAX__

#define WCHAR_MIN __WCHAR_MIN__
#define WCHAR_MAX __WCHAR_MAX__

#define WINT_MIN __WINT_MIN__
#define WINT_MAX __WINT_MAX__

//
// Define the macros that expand to constant expressions.
//

#define INT8_C(_Value)    __INT8_C(_Value)
#define UINT8_C(_Value)   __UINT8_C(_Value)
#define INT16_C(_Value)   __INT16_C(_Value)
#define UINT16_C(_Value)  __UINT16_C(_Value)
#define INT32_C(_Value)   __INT32_C(_Value)
#define UINT32_C(_Value)  __UINT32_C(_Value)
#define INT64_C(_Value)   __INT64_C(_Value)
#define UINT64_C(_Value)  __UINT64_C(_Value)
#define INTMAX_C(_Value)  __INTMAX_C(_Value)
#define UINTMAX_C(_Value) __UINTMAX_C(_Value)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;

typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;

#ifndef uint64_t

typedef __UINT64_TYPE__ uint64_t;

#endif

typedef __INT_LEAST8_TYPE__ int_least8_t;
typedef __INT_LEAST16_TYPE__ int_least16_t;
typedef __INT_LEAST32_TYPE__ int_least32_t;
typedef __INT_LEAST64_TYPE__ int_least64_t;

typedef __UINT_LEAST8_TYPE__ uint_least8_t;
typedef __UINT_LEAST16_TYPE__ uint_least16_t;
typedef __UINT_LEAST32_TYPE__ uint_least32_t;
typedef __UINT_LEAST64_TYPE__ uint_least64_t;

typedef __INT_FAST8_TYPE__ int_fast8_t;
typedef __INT_FAST16_TYPE__ int_fast16_t;
typedef __INT_FAST32_TYPE__ int_fast32_t;
typedef __INT_FAST64_TYPE__ int_fast64_t;

typedef __UINT_FAST8_TYPE__ uint_fast8_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
typedef __UINT_FAST64_TYPE__ uint_fast64_t;

typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;

typedef __INTMAX_TYPE__ intmax_t;

#ifndef uintmax_t

typedef __UINTMAX_TYPE__ uintmax_t;

#endif

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#ifdef __cplusplus

}

#endif
#endif

