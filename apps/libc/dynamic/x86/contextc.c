/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    contextc.c

Abstract:

    This module implements C support for working with ucontext structures in
    the C library.

Author:

    Evan Green 8-Sep-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"
#include <alloca.h>
#include <errno.h>
#include <signal.h>
#include <ucontext.h>
#include <minoca/kernel/x86.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

__NO_RETURN
VOID
ClpContextEnd (
    ucontext_t *Context
    );

__NO_RETURN
void
ClpContextStart (
    void (*StartFunction)(),
    ...
    );

VOID
ClpFxSave (
    PFPU_CONTEXT Buffer
    );

VOID
ClpFxRestore (
    PFPU_CONTEXT Buffer
    );

VOID
ClpFSave (
    PFPU_CONTEXT Buffer
    );

VOID
ClpFRestore (
    PFPU_CONTEXT Buffer
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
void
makecontext (
    ucontext_t *Context,
    void (*StartFunction)(),
    int ArgumentCount,
    ...
    )

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

{

    PUINTN Argument;
    va_list ArgumentList;
    UINTN Index;
    PVOID StackTop;
    PTRAP_FRAME TrapFrame;

    if (Context == NULL) {
        return;
    }

    //
    // Create a stack that looks like this (starting with the last pushed):
    // ClpContextStart
    // StartFunction
    // Argument1 (16 byte aligned)
    // ...
    // ArgumentN
    // Context.
    //

    StackTop = (PVOID)(Context->uc_stack.ss_sp + Context->uc_stack.ss_size -
                       sizeof(UINTN));

    StackTop -= (ArgumentCount + 1) * sizeof(UINTN);
    StackTop = ALIGN_POINTER_DOWN(StackTop, 16);
    StackTop -= 2 * sizeof(UINTN);
    Argument = (PUINTN)StackTop;
    *Argument = (UINTN)ClpContextStart;
    Argument += 1;
    *Argument = (UINTN)StartFunction;
    Argument += 1;
    va_start(ArgumentList, ArgumentCount);
    for (Index = 0; Index < ArgumentCount; Index += 1) {
        *Argument = va_arg(ArgumentList, UINTN);
        Argument += 1;
    }

    va_end(ArgumentList);
    *Argument = (UINTN)Context;

    //
    // Set the registers to point at the top of the stack.
    //

    TrapFrame = (PTRAP_FRAME)&(Context->uc_mcontext.gregs);
    TrapFrame->Esi = (UINTN)Argument;
    TrapFrame->Ebp = 0;
    TrapFrame->Esp = (UINTN)StackTop + sizeof(PVOID);
    TrapFrame->Eip = (UINTN)ClpContextStart;
    return;
}

int
ClpGetContext (
    ucontext_t *Context,
    void *StackPointer
    )

/*++

Routine Description:

    This routine stores the current FPU and general context into the given
    structure. The assembly code that calls this routine is responsible for
    saving the general registers.

Arguments:

    Context - Supplies a pointer to the context.

    StackPointer - Supplies the current stack pointer.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    BOOL Aligned;
    int Error;
    PFPU_CONTEXT FpuContext;
    void *StackBase;
    size_t StackSize;
    pthread_attr_t ThreadAttribute;

    Error = pthread_getattr_np(pthread_self(), &ThreadAttribute);
    if (Error != 0) {
        errno = Error;
        return -1;
    }

    Error = pthread_attr_getstack(&ThreadAttribute, &StackBase, &StackSize);
    if (Error != 0) {
        errno = Error;
        return -1;
    }

    if (StackBase == NULL) {
        StackBase = StackPointer;
    }

    Context->uc_flags = SIGNAL_CONTEXT_FLAG_FPU_VALID;
    Context->uc_stack.ss_sp = StackPointer;
    Context->uc_stack.ss_flags = 0;
    Context->uc_stack.ss_size = StackSize;

    //
    // TODO: Enable this when sigaltstack is implemented.
    //

#if 0

    //
    // If currently on the signal stack, then the thread parameters aren't
    // correct.
    //

    if (sigaltstack(NULL, &SignalStack) == 0) {
        if ((SignalStack.ss_flags & SS_ONSTACK) != 0) {
            Context->uc_stack = SignalStack;
        }
    }

#endif

    sigprocmask(0, NULL, &(Context->uc_sigmask));

    //
    // Get the FPU context buffer. If it's not aligned, it will have to be
    // saved into an aligned buffer and then copied.
    //

    Aligned = IS_POINTER_ALIGNED(&(Context->uc_mcontext.fpregs),
                                 __FPSTATE_ALIGNMENT);

    if (Aligned != FALSE) {
        FpuContext = (PFPU_CONTEXT)&(Context->uc_mcontext.fpregs);

    } else {
        FpuContext = alloca(__FPSTATE_SIZE + __FPSTATE_ALIGNMENT);
        FpuContext = ALIGN_POINTER_UP(FpuContext, __FPSTATE_ALIGNMENT);
    }

    //
    // Save the floating point state.
    //

    if (OsTestProcessorFeature(OsX86FxSave) != FALSE) {
        ClpFxSave(FpuContext);

    } else {
        ClpFSave(FpuContext);
    }

    if (FpuContext != (PFPU_CONTEXT)&(Context->uc_mcontext.fpregs)) {
        memcpy(&(Context->uc_mcontext.fpregs),
               FpuContext,
               sizeof(Context->uc_mcontext.fpregs));
    }

    pthread_attr_destroy(&ThreadAttribute);
    return 0;
}

void
ClpSetContext (
    const ucontext_t *Context
    )

/*++

Routine Description:

    This routine restores the user context set in the given structure.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    None.

--*/

{

    PFPU_CONTEXT FpuContext;

    //
    // Restore the floating point context using the appropriate mechanism.
    //

    if ((Context->uc_flags & SIGNAL_CONTEXT_FLAG_FPU_VALID) != 0) {

        //
        // If the structure causes the floating point context not to be aligned,
        // allocate a temporary structure, align it, and copy the data in.
        //

        FpuContext = (PFPU_CONTEXT)&(Context->uc_mcontext.fpregs);
        if (!IS_POINTER_ALIGNED(FpuContext, __FPSTATE_ALIGNMENT)) {
            FpuContext = alloca(__FPSTATE_SIZE + __FPSTATE_ALIGNMENT);
            FpuContext = ALIGN_POINTER_UP(FpuContext, __FPSTATE_ALIGNMENT);
            memcpy(FpuContext,
                   &(Context->uc_mcontext.fpregs),
                   sizeof(FPU_CONTEXT));
        }

        if (OsTestProcessorFeature(OsX86FxSave) != FALSE) {
            ClpFxRestore(FpuContext);

        } else {
            ClpFRestore(FpuContext);
        }
    }

    sigprocmask(SIG_SETMASK, &(Context->uc_sigmask), NULL);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

