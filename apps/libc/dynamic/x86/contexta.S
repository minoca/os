/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    contexta.S

Abstract:

    This module implements assembly functionality for working with ucontext
    structure.

Author:

    Evan Green 8-Sep-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x86.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------------------------------- Code
//

//
// .text specifies that this code belongs in the executable section.
//
// .code32 specifies that this is 32-bit protected mode code.
//

.text
.code32

//
// LIBC_API
// int
// getcontext (
//     ucontext_t *Context
//     )
//

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

EXPORTED_FUNCTION(getcontext)
    movl    4(%esp), %eax           # Get the context.
    addl    $SIGNAL_CONTEXT_SIZE, %eax  # Get to the TRAP_FRAME part.
    movl    %ecx, TRAP_ECX(%eax)    # Save ecx.
    xorl    %ecx, %ecx              # Clear a register.
    movw    %cs, %cx                # Get DS.
    movl    %ecx, TRAP_CS(%eax)     # Save CS.
    movw    %ds, %cx                # Get DS.
    movl    %ecx, TRAP_DS(%eax)     # Save DS.
    movw    %es, %cx                # Get ES.
    movl    %ecx, TRAP_ES(%eax)     # Save ES.
    movw    %fs, %cx                # Get FS.
    movl    %ecx, TRAP_FS(%eax)     # Save FS.
    movw    %gs, %cx                # Get GS.
    movl    %ecx, TRAP_GS(%eax)     # Save GS.
    movw    %ss, %cx                # Get SS.
    movl    %ecx, TRAP_SS(%eax)     # Save SS.
    xorl    %ecx, %ecx              # Clear a register.
    movl    %ecx, TRAP_EAX(%eax)    # Save a zeroed eax.
    movl    %ebx, TRAP_EBX(%eax)    # Save ebx.
    movl    %edx, TRAP_EDX(%eax)    # Save edx.
    movl    %esi, TRAP_ESI(%eax)    # Save esi.
    movl    %edi, TRAP_EDI(%eax)    # Save edi.
    movl    %ebp, TRAP_EBP(%eax)    # Save ebp.
    movl    %ecx, TRAP_ERRORCODE(%eax)  # Save zeroed error code.
    movl    (%esp), %ecx            # Get the return address.
    movl    %ecx, TRAP_EIP(%eax)    # Save the instruction pointer.
    pushfl                          # Push eflags.
    popl    %ecx                    # Get eflags.
    movl    %ecx, TRAP_EFLAGS(%eax) # Save eflags.
    leal    4(%esp), %ecx           # Get the stack pointer (w/o return addr).
    movl    %ecx, TRAP_ESP(%eax)    # Save that as esp.
    pushl   %ecx                    # Push stack pointer.
    movl    8(%esp), %eax           # Get the context pointer.
    pushl   %eax                    # Push it as the first argument.
    call    ClpGetContext           # Call the C helper.
    addl    $8, %esp                # Pop the arguments.
    ret                             # Return whatever the C routine returned.

END_FUNCTION(getcontext)

//
// LIBC_API
// int
// setcontext (
//     const ucontext_t *Context
//     )
//

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

EXPORTED_FUNCTION(setcontext)
    movl    4(%esp), %eax           # Get the argument.
    pushl   %eax
    call    ClpSetContext           # Call the C helper.
    movl    8(%esp), %ecx           # Get the argument again.
    addl    $SIGNAL_CONTEXT_SIZE, %ecx  # Get to the TRAP_FRAME part.
    movl    TRAP_DS(%ecx), %eax     # Get DS.
    movw    %ax, %ds                # Restore DS.
    movl    TRAP_ES(%ecx), %eax     # Get ES.
    movw    %ax, %es                # Restore ES.
    movl    TRAP_FS(%ecx), %eax     # Get FS.
    movw    %ax, %fs                # Restore FS.
    movl    TRAP_GS(%ecx), %eax     # Get GS.
    movw    %ax, %gs                # Restore GS.
    movl    TRAP_SS(%ecx), %eax     # Get SS.
    movw    %ax, %ss                # Restore SS.
    movl    TRAP_EBP(%ecx), %ebp    # Restore ebp.
    movl    TRAP_EDI(%ecx), %edi    # Restore edi.
    movl    TRAP_ESI(%ecx), %esi    # Restore esi.
    movl    TRAP_EDX(%ecx), %edx    # Restore edx.
    movl    TRAP_EBX(%ecx), %ebx    # Restore ebx.
    movl    TRAP_EFLAGS(%ecx), %eax # Get eflags.
    movl    %eax, (%esp)            # "Push" eflags into old unpopped argument.
    popfl                           # Pop eflags/old argument off the stack.
    movl    TRAP_EAX(%ecx), %eax    # Restore eax as return value.

    //
    // This last part gets a little fishy depending on where the context
    // structure is. If the new esp is on the same stack but greater than this
    // one, then this code runs the risk of taking a signal, which might
    // clobber the context before restoring EIP can be done. Hopefully that
    // doesn't happen.
    //

    movl    TRAP_ESP(%ecx), %esp    # Restore stack pointer.
    jmp     *TRAP_EIP(%ecx)         # Return.

END_FUNCTION(setcontext)

//
// __NO_RETURN
// void
// ClpContextStart (
//     void (*StartFunction)(),
//     ...
//     )
//

/*++

Routine Description:

    This routine is a small trampoline that calls the function specified in
    makecontext.

Arguments:

    StartFunction - Supplies a pointer to the function to call.

    ... - Supplies the arguments the start function takes.

Return Value:

    This routine does not return.

--*/

FUNCTION(ClpContextStart)
    popl    %eax                    # Get the function to call.
    call    *%eax                   # Make it rain.
    movl    %esi, %esp              # Pop the function and all arguments off.
    call    ClpContextEnd           # Call the C helper to switch contexts.
    hlt                             # Execution should never reach here.

END_FUNCTION(ClpContextStart)

//
// VOID
// ClpFxSave (
//     PFPU_CONTEXT Buffer
//     )
//

/*++

Routine Description:

    This routine saves the current x87 FPU, MMX, XMM, and MXCSR registers to a
    512 byte memory location.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        saved. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

FUNCTION(ClpFxSave)
    movl    4(%esp), %eax   # Get the buffer parameter.
    fxsave  (%eax)          # Save the state into there.
    ret

END_FUNCTION(ClpFxSave)

//
// VOID
// ClpFxRestore (
//     PFPU_CONTEXT Buffer
//     )
//

/*++

Routine Description:

    This routine restores the current x87 FPU, MMX, XMM, and MXCSR registers
    from a 512 byte memory location.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        loaded from. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

FUNCTION(ClpFxRestore)
    movl    4(%esp), %eax   # Get the buffer parameter.
    fxrstor (%eax)          # Load the state from there.
    ret

END_FUNCTION(ClpFxRestore)

//
// VOID
// ClpFSave (
//     PFPU_CONTEXT Buffer
//     )
//

/*++

Routine Description:

    This routine saves the current x87 FPU (floating point unit) state.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        saved. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

FUNCTION(ClpFSave)
    movl    4(%esp), %eax   # Get the buffer parameter.
    fsave  (%eax)           # Save the state into there.
    ret

END_FUNCTION(ClpFSave)

//
// VOID
// ClpFRestore (
//     PFPU_CONTEXT Buffer
//     )
//

/*++

Routine Description:

    This routine restores the x87 FPU (floating point unit) state.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        loaded from. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

FUNCTION(ClpFRestore)
    movl    4(%esp), %eax   # Get the buffer parameter.
    frstor (%eax)           # Load the state from there.
    ret

END_FUNCTION(ClpFRestore)

