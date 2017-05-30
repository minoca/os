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
    structures.

Author:

    Evan Green 9-Sep-2016

Environment:

    User Mode C Library

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/x64.inc>

##
## ---------------------------------------------------------------- Definitions
##

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## LIBC_API
## int
## getcontext (
##     ucontext_t *Context
##     )
##

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
    int     $3                      # TODO: Check this, it's never been run!
    movq    %rdi, %rax              # Get the context.
    addq    $SIGNAL_CONTEXT_SIZE, %rax  # Get to the TRAP_FRAME part.
    movq    %rcx, TRAP_RCX(%rax)    # Save rcx.
    xorq    %rcx, %rcx              # Clear a register.
    movw    %cs, %cx                # Get DS.
    movq    %rcx, TRAP_CS(%rax)     # Save CS.
    movw    %ds, %cx                # Get DS.
    movq    %rcx, TRAP_DS(%rax)     # Save DS.
    movw    %es, %cx                # Get ES.
    movq    %rcx, TRAP_ES(%rax)     # Save ES.
    movw    %fs, %cx                # Get FS.
    movq    %rcx, TRAP_FS(%rax)     # Save FS.
    movw    %gs, %cx                # Get GS.
    movq    %rcx, TRAP_GS(%rax)     # Save GS.
    movw    %ss, %cx                # Get SS.
    movq    %rcx, TRAP_SS(%rax)     # Save SS.
    xorq    %rcx, %rcx              # Clear a register.
    movq    %rcx, TRAP_RAX(%rax)    # Save a zeroed rax.
    movq    %rbx, TRAP_RBX(%rax)    # Save rbx.
    movq    %rdx, TRAP_RDX(%rax)    # Save rdx.
    movq    %rsi, TRAP_RSI(%rax)    # Save rsi.
    movq    %rdi, TRAP_RDI(%rax)    # Save rdi.
    movq    %rbp, TRAP_RBP(%rax)    # Save rbp.
    movq    %r8, TRAP_R8(%rax)      # Save r8.
    movq    %r9, TRAP_R9(%rax)      # Save r9.
    movq    %r10, TRAP_R10(%rax)    # Save r10.
    movq    %r11, TRAP_R11(%rax)    # Save r11.
    movq    %r12, TRAP_R12(%rax)    # Save r12.
    movq    %r13, TRAP_R13(%rax)    # Save r13.
    movq    %r14, TRAP_R14(%rax)    # Save r14.
    movq    %r15, TRAP_R15(%rax)    # Save r15.
    movq    %rcx, TRAP_ERRORCODE(%rax)  # Save zeroed error code.
    movq    (%rsp), %rcx            # Get the return address.
    movq    %rcx, TRAP_RIP(%rax)    # Save the instruction pointer.
    pushfq                          # Push eflags.
    popq    %rcx                    # Get eflags.
    movq    %rcx, TRAP_RFLAGS(%rax) # Save eflags.
    leaq    4(%rsp), %rcx           # Get the stack pointer (w/o return addr).
    movq    %rcx, TRAP_RSP(%rax)    # Save that as rsp.
    movq    %rsp, %rsi              # Set stack pointer as second arg.
    call    ClpGetContext           # Call the C helper.
    ret                             # Return whatever the C routine returned.

END_FUNCTION(getcontext)

##
## LIBC_API
## int
## setcontext (
##     const ucontext_t *Context
##     )
##

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
    int     $3                      # TODO: Check this, it's never been run!
    pushq   %rdi                    # Save the argument.
    call    ClpSetContext           # Call the C helper.
    popq    %rcx                    # Restore the argument.
    addq    $SIGNAL_CONTEXT_SIZE, %rcx  # Get to the TRAP_FRAME part.
    movq    TRAP_DS(%rcx), %rax     # Get DS.
    movw    %ax, %ds                # Restore DS.
    movq    TRAP_ES(%rcx), %rax     # Get ES.
    movw    %ax, %es                # Restore ES.
    movq    TRAP_FS(%rcx), %rax     # Get FS.
    movw    %ax, %fs                # Restore FS.
    movq    TRAP_GS(%rcx), %rax     # Get GS.
    movw    %ax, %gs                # Restore GS.
    movq    TRAP_SS(%rcx), %rax     # Get SS.
    movw    %ax, %ss                # Restore SS.
    movq    TRAP_R15(%rcx), %r15    # Restore r15.
    movq    TRAP_R14(%rcx), %r14    # Restore r14.
    movq    TRAP_R13(%rcx), %r13    # Restore r13.
    movq    TRAP_R12(%rcx), %r12    # Restore r12.
    movq    TRAP_R11(%rcx), %r11    # Restore r11.
    movq    TRAP_R10(%rcx), %r10    # Restore r10.
    movq    TRAP_R9(%rcx), %r9      # Restore r9.
    movq    TRAP_R8(%rcx), %r8      # Restore r8.
    movq    TRAP_RBP(%rcx), %rbp    # Restore rbp.
    movq    TRAP_RDI(%rcx), %rdi    # Restore rdi.
    movq    TRAP_RSI(%rcx), %rsi    # Restore rsi.
    movq    TRAP_RDX(%rcx), %rdx    # Restore rdx.
    movq    TRAP_RBX(%rcx), %rbx    # Restore rbx.
    movq    TRAP_RFLAGS(%rcx), %rax # Get eflags.
    pushq   %rax                    # Push eflags.
    popfq                           # Pop eflags off the stack.
    movq    TRAP_RAX(%rcx), %rax    # Restore rax as return value.

    ##
    ## This last part gets a little fishy depending on where the context
    ## structure is. If the new rsp is on the same stack but greater than this
    ## one, then this code runs the risk of taking a signal, which might
    ## clobber the context before restoring RIP can be done. Hopefully that
    ## doesn't happen.
    ##

    movq    TRAP_RSP(%rcx), %rsp    # Restore stack pointer.
    jmp     *TRAP_RIP(%rcx)         # Return.

END_FUNCTION(setcontext)

##
## __NO_RETURN
## void
## ClpContextStart (
##     void (*StartFunction)(),
##     ...
##     )
##

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
    int     $3                      # TODO: Check this, it's never been run!
    popq    %rax                    # Get the function to call.
    popq    %rdi                    # Pop argument 1.
    popq    %rsi                    # Pop argument 2.
    popq    %rdx                    # Pop argument 3.
    popq    %rcx                    # Pop argument 4.
    popq    %r8                     # Pop argument 5.
    popq    %r9                     # Pop argument 6.
    callq   *%rax                   # Make it rain.
    movq    %r12, %rsp              # Pop the function and all arguments off.
    call    ClpContextEnd           # Call the C helper to switch contexts.
    hlt                             # Execution should never reach here.

END_FUNCTION(ClpContextStart)

##
## VOID
## ClpFxSave (
##     PFPU_CONTEXT Buffer
##     )
##

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
    fxsave  (%rdi)          # Save the state into there.
    ret

END_FUNCTION(ClpFxSave)

##
## VOID
## ClpFxRestore (
##     PFPU_CONTEXT Buffer
##     )
##

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
    fxrstor (%rdi)          # Load the state from there.
    ret

END_FUNCTION(ClpFxRestore)

