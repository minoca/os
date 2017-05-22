/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    chalkp.h

Abstract:

    This header contains internal definitions for the Chalk interpreter. It
    should not be included outside the interpreter core itself.

Author:

    Evan Green 28-May-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Chalk API functions are all exported from the library.
//

#define CK_API __DLLEXPORT

//
// The parser library is statically linked in.
//

#define YY_API

#include <minoca/lib/types.h>
#include <minoca/lib/chalk.h>

#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "value.h"
#include "core.h"
#include "utils.h"
#include "gc.h"
#include "vm.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros allocate, reallocate, and free memory through the Chalk
// memory manager.
//

#define CkAllocate(_Vm, _Size) CkpReallocate((_Vm), NULL, 0, (_Size))
#define CkFree(_Vm, _Memory) CkpReallocate((_Vm), (_Memory), 0, 0)

//
// These macros allocate, reallocate, and free memory directly using the Chalk
// system function. Most allocations go through the Chalk functions for memory
// management and garbage collection.
//

#define CkRawAllocate(_Vm, _Size) \
    (_Vm)->Configuration.Reallocate(NULL, (_Size))

#define CkRawReallocate(_Vm, _Memory, _NewSize) \
        (_Vm)->Configuration.Reallocate((_Memory), (_NewSize))

#define CkRawFree(_Vm, _Memory) (_Vm)->Configuration.Reallocate((_Memory), 0)

//
// These macros perform basic memory manipulations.
//

#define CkZero(_Memory, _Size) memset((_Memory), 0, (_Size))
#define CkCopy(_Destination, _Source, _Size) \
    memcpy((_Destination), (_Source), (_Size))

#define CkCompareMemory(_Left, _Right, _Size) memcmp(_Left, _Right, _Size)

//
// This macro determines if a configuration flag is set.
//

#define CK_VM_FLAG_SET(_Vm, _Flag) \
    (((_Vm)->Configuration.Flags & (_Flag)) != 0)

//
// These macros manipulate the stack.
//

#define CK_PUSH(_Fiber, _Value) \
    *((_Fiber)->StackTop) = (_Value); \
    (_Fiber)->StackTop += 1

#define CK_POP(_Fiber) ((_Fiber)->StackTop -= 1, *((_Fiber)->StackTop))

//
// ---------------------------------------------------------------- Definitions
//

//
// The lex/parse library is statically linked into this library, so define
// away the API decorator.
//

#define YY_API

//
// Define the maximum number of module-level variables, as limited by the
// bytecode op size.
//

#define CK_MAX_MODULE_VARIABLES 0xFFFF

//
// Define the arbitrary maximum length of a method or variable.
//

#define CK_MAX_NAME 64

//
// Define the maximum number of fields a class can have. This limitation
// exists in the bytecode as well in the form of operand size.
//

#define CK_MAX_FIELDS 255

//
// Define the maximum number of nested functions.
//

#define CK_MAX_NESTED_FUNCTIONS 32

//
// Define the initial number of call frames to allocate for any new fiber. This
// should ideally by a power of two.
//

#define CK_INITIAL_CALL_FRAMES 8

//
// Define the initial size of the stack, in elements.
//

#define CK_INITIAL_STACK 8

//
// Define the minimum number of try frames to allocate. These are allocated
// upon executing the first try block.
//

#define CK_MIN_TRY_STACK 8

//
// Define the maximum size of a method signature string.
//

#define CK_MAX_METHOD_SIGNATURE (CK_MAX_NAME + 8)

//
// Define the maximum value for an integer.
//

#define CK_INT_MAX 0x7FFFFFFFFFFFFFFFLL

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
