/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ucontext.h

Abstract:

    This header contains definitions for manipulating the user machine context.

Author:

    Evan Green 31-Aug-2016

--*/

#ifndef _UCONTEXT_H
#define _UCONTEXT_H

//
// ------------------------------------------------------------------- Includes
//

#include <signal.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

#if defined(__i386)

//
// See the TRAP_FRAME structure for the register definitions.
//

#define NGREG 17

#define REG_DS 0
#define REG_ES 1
#define REG_FS 2
#define REG_GS 3
#define REG_SS 4
#define REG_EAX 5
#define REG_EBX 6
#define REG_ECX 7
#define REG_EDX 8
#define REG_ESI 9
#define REG_EDI 10
#define REG_EBP 11
#define REG_ERR 12
#define REG_EIP 13
#define REG_CS 14
#define REG_EFL 15
#define REG_ESP 16

//
// See the FPU_CONTEXT structure for the register definitions.
//

#define __FPSTATE_SIZE 512
#define __FPSTATE_ALIGNMENT 64

#elif defined(__amd64)

//
// TODO: Define the TRAP_FRAME for x64.
//

#define NGREG 23
#define __FPSTATE_SIZE 512
#define __FPSTATE_ALIGNMENT 64

//
// ARM-specific register state.
//

#elif defined(__arm__)

//
// See the TRAP_FRAME structure for the register definitions.
//

#define NGREG 20

#define REG_R13 1
#define REG_R14 2
#define REG_R0 3

#define REG_R1 5
#define REG_R2 6
#define REG_R3 7
#define REG_R4 8
#define REG_R5 9
#define REG_R6 10
#define REG_R7 11
#define REG_R8 12
#define REG_R9 13
#define REG_R10 14
#define REG_R11 15
#define REG_R12 16

#define REG_R15 18
#define REG_CPSR 19

//
// See the FPU_CONTEXT structure for the register definitions.
//

#define __FPSTATE_SIZE 0x110
#define __FPSTATE_ALIGNMENT 16

#else

#error Unknown architecture.

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the type for a single general purpose register.
//

typedef long int greg_t;

//
// Define the type that contains all the general purpose registers.
//

typedef greg_t gregset_t[NGREG];

//
// Define the type of the floating point registers.
//

typedef struct {
    unsigned char Data[__FPSTATE_SIZE];
} fpregset_t;

/*++

Structure Description:

    This structure stores the entire processor context.

Members:

    gregs - Stores the general registers.

    fpregs - Stores the floating point register context.

--*/

typedef struct {
    gregset_t gregs;
    fpregset_t fpregs;
} mcontext_t;

/*++

Structure Description:

    This structure stores the user mode machine context. This lines up with the
    SIGNAL_CONTEXT_* structures used by the kernel.

Members:

    uc_flags - Stores a bitfield of flags.

    uc_link - Stores a pointer to the context that is resumed when this context
        returns.

    uc_stack - Stores the stack used by this context.

    uc_sigmask - Stores the set of signals that are blocked when this context
        is active.

    uc_mcontext - Stores the machine specific context.

--*/

typedef struct ucontext {
    unsigned long int uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    sigset_t uc_sigmask;
    mcontext_t uc_mcontext;
} ucontext_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
getcontext (
    ucontext_t *Context
    );

/*++

Routine Description:

    This routine saves the current user context into the given structure,
    including the machine registers, signal mask, and execution stack pointer.
    If restored, the returned context will appear to execute at the return from
    this function.

Arguments:

    Context - Supplies a pointer where the current context is saved.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
setcontext (
    const ucontext_t *Context
    );

/*++

Routine Description:

    This routine restores a previous execution context into the current
    processor.

Arguments:

    Context - Supplies a pointer to the previously saved context to restore.

Return Value:

    Does not return on success, as execution continues from the new context.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
void
makecontext (
    ucontext_t *Context,
    void (*StartFunction)(),
    int ArgumentCount,
    ...
    );

/*++

Routine Description:

    This routine modifies an initialized context to call the function provided
    with the given arguments.

Arguments:

    Context - Supplies a pointer to the context.

    StartFunction - Supplies a pointer to the function to call when the
        context is restored.

    ArgumentCount - Supplies the number of int sized arguments supplied.

    ... - Supplies the remaining arguments to pass to the function.

Return Value:

    None.

--*/

LIBC_API
int
swapcontext (
    ucontext_t *OldContext,
    ucontext_t *Context
    );

/*++

Routine Description:

    This routine saves the current context, and sets the given new context
    with a backlink to the original context.

Arguments:

    OldContext - Supplies a pointer where the currently running context will
        be saved on success.

    Context - Supplies a pointer to the new context to apply. A link to the
        context running before this call will be saved in this context.

Return Value:

    0 on success.

    -1 on failure, and errno will be set contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

