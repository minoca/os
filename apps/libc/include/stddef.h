/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    stddef.h

Abstract:

    This header contains essential C definitions. This header is often
    overridden by a compiler internal version.

Author:

    Evan Green 4-Mar-2013

--*/

#ifndef _STDDEF_H
#define _STDDEF_H

//
// --------------------------------------------------------------------- Macros
//

//
// This macro gets the offset, in bytes, from the beginning of the given
// structure type to the given structure member.
//

#define offsetof(_Type, _Member) __builtin_offsetof(_Type, _Member)

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define NULL if it hasn't already been defined.
//

#ifndef NULL

#ifdef __cplusplus

#define NULL 0

#else

#define NULL ((void *)0)

#endif

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the type returned by the compiler when two pointers are subtracted.
//

typedef __PTRDIFF_TYPE__ ptrdiff_t;

//
// Define the widest character the compiler supports.
//

#ifndef __cplusplus

typedef __WCHAR_TYPE__ wchar_t;

#endif

//
// The int is to char as wint_t is to wchar_t.
//

typedef __WINT_TYPE__ wint_t;

//
// Define the type the compiler returns from the sizeof() operator.
//

typedef __SIZE_TYPE__ size_t;

#ifdef __cplusplus

}

#endif
#endif

