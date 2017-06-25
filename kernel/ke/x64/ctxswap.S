/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ctxswap.S

Abstract:

    This module implements context switching on AMD64.

Author:

    Evan Green 11-Jun-2017

Environment:

    Kernel mode

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
## VOID
## KepContextSwap (
##     PVOID *SavedStackLocation,
##     PVOID NewStack,
##     PVOID NewThreadPointer,
##     BOOL FirstTime
##     )
##

/*++

Routine Description:

    This routine switches context to the given thread.

Arguments:

    SavedStackLocation - Supplies a pointer where the old stack pointer will
        be saved.

    NewStack - Supplies the new stack address.

    NewThreadPointer - Supplies the new thread pointer data.

    FirstTime - Supplies a boolean indicating whether the thread has never been
        run before.

Return Value:

    None.

--*/

FUNCTION(KepContextSwap)
    pushq   %rbp                    # Function prologue (also saves rbp).
    movq    %rsp, %rbp              #
    pushq   %rbx                    # Save nonvolatile registers.
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    pushfq
    subq    $16, %rsp               # Grab some extra stack space, aligned.
    movl    $CONTEXT_SWAP_MAGIC, (%rsp)     # Save a sentinal on the stack.

    ##
    ## Load the new thread pointer GDT entry.
    ##

    wrgsbase    %rdx                # Write the new GS base.

    ##
    ## Save the current thread's stack pointer. This effectively freezes the
    ## current thread. When this thread is swapped back in, the stack pointer
    ## will be restored, and execution of this function will continue. It's
    ## crucial that the stack pointer not change between a normal context swap
    ## and a first-time context swap, otherwise the thread that created the
    ## context swap will have an incorrect stack next time it is swapped in.
    ##

    movq    %rsp, (%rdi)            # Save stack pointer.

    ##
    ## Switch to the new stack and perform work than can only be done once off
    ## of the old stack.
    ##

    movq    %rsi, %rsp              # Switch to the new stack.
    xorq    %rbp, %rbp              # Zero out rbp so the call stack stops here.

    ##
    ## Perform any post-stack switch work needed on the old thread.
    ##

    call    KepPostContextSwapWork  # Perform post swap work.

    ##
    ## Determine whether to do a first-time return or a normal one. The only
    ## difference is that a first-time execution has been set up to do an iret,
    ## and a normal context swap doesn't need that because the stack is already
    ## set up correctly.
    ##

    cmpl    $FALSE, %ecx            # Compare FirstTime to FALSE.
    je      KepContextSwapRestore   # Special case a first run.

KepContextSwapFirstTime:
    movq    %rsp, %rdi              # 1st argument is the trap frame pointer.
    call    KepPreThreadStartWork   # Do any thread setup needed.
    call    ArRestoreTrapFrame      # Set up CPU context.
    iretq                           # Return from the artificial exception.

KepContextSwapRestore:
    movl    (%rsp), %eax            # Check the sentinal value.
    cmpl    $CONTEXT_SWAP_MAGIC, %eax   # Compare.
    jne     KepContextSwapBadMagic  # Jump out of line if it's bad.

KepContextSwapReturn:
    addq    $16, %rsp               # Undo previous alignment adjustment.
    popfq                           # Restore registers.
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    leave
    ret

KepContextSwapBadMagic:
    int     $0x3                    # Break on bad context.
    jmp     KepContextSwapReturn    # If we break here, WATCH OUT.

END_FUNCTION(KepContextSwap)

