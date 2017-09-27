/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ctxswap.S

Abstract:

    This module implements context switching on x86

Author:

    Evan Green 6-Aug-2012

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x86.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the offset into the GDT where the thread area entry starts, in bytes.
//

#define GDT_THREAD_OFFSET (GDT_THREAD & ~0x7)

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
// VOID
// KepContextSwap (
//     PVOID *SavedStackLocation,
//     PVOID NewStack,
//     PVOID NewThreadPointer,
//     BOOL FirstTime
//     )
//

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

//
// Parameters
//

.equ SavedStackLocation, 8
.equ NewStack, 12
.equ NewThreadPointerLowerGdt, 16
.equ NewThreadPointerUpperGdt, 20
.equ FirstTime, 24

FUNCTION(KepContextSwap)
    pushl   %ebp                    # Function prologue.
    movl    %esp, %ebp              #
    pushl   %ebx                    # Save nonvolatile registers.
    pushl   %esi
    pushl   %edi
    pushl   %ebp
    pushfl
    pushl   $CONTEXT_SWAP_MAGIC

    //
    // Load the new thread pointer GDT entry.
    //

    movl    NewThreadPointerLowerGdt(%ebp), %eax    # Get lower GDT half.
    movl    NewThreadPointerUpperGdt(%ebp), %edx    # Get upper GDT half.
    movl    %fs:(PROCESSOR_BLOCK_GDT), %ecx         # Get current GDT.
    movl    %eax, GDT_THREAD_OFFSET(%ecx)           # Load lower half.
    movl    %edx, GDT_THREAD_OFFSET+4(%ecx)         # Load upper half.
    movw    $GDT_THREAD, %ax                        # Load the GS value.
    movw    %ax, %gs                                # Reload GS to take effect.

    //
    // Save the parameters before the stack switch is initiated.
    //

    movl    FirstTime(%ebp), %esi   # Get FirstTime parameter.

    //
    // Save the current thread's stack pointer. This effectively freezes the
    // current thread. When this thread is swapped back in, the stack pointer
    // will be restored, and execution of this function will continue. It's
    // crucial that the stack pointer not change between a normal context swap
    // and a first-time context swap, otherwise the thread that created the
    // context swap will have an incorrect stack next time it is swapped in.
    //

    movl    SavedStackLocation(%ebp), %ecx          # Get location to save in.
    movl    %esp, (%ecx)            # Save stack pointer.

    //
    // Switch to the new stack and perform work than can only be done once off
    // of the old stack. Touch the stack before switching to it to trigger any
    // page directory updates needed to see the new stack from the old CR3.
    //

    movl    NewStack(%ebp), %eax    # Get the new stack.
    movl    (%eax), %ecx            # Poke the new stack to trigger page faults.
    movl    %eax, %esp              # Switch to the new stack.
    xor     %ebp, %ebp              # Zero out ebp so the call stack stops here.

    //
    // Perform any post-stack switch work needed on the old thread.
    //

    call    KepPostContextSwapWork  # Perform post swap work.

    //
    // Determine whether to do a first-time return or a normal one. The only
    // difference is that a first-time execution has been set up to do an iret,
    // and a normal context swap doesn't need that because the stack is already
    // set up correctly.
    //

    cmpl    $FALSE, %esi            # Compare FirstTime to FALSE.
    je      KepContextSwapRestore   # Special case a first run.

KepContextSwapFirstTime:
    pushl   %esp                    # Push a pointer to the trap frame.
    call    KepPreThreadStartWork   # Do any thread setup needed.
    addl    $4, %esp                # Pop the parameter.
    mov     %esp, %ebx              # Set restore trap frame parameter.
    call    ArRestoreTrapFrame      # Set up CPU context.
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the artificial exception.

KepContextSwapRestore:
    popl    %eax                    # Pop the magic value.
    cmp     $CONTEXT_SWAP_MAGIC, %eax   # Compare.
    jne     KepContextSwapBadMagic  # Jump out of line if it's bad.

KepContextSwapReturn:
    popfl                           # Restore registers.
    popl    %ebp
    popl    %edi
    popl    %esi
    popl    %ebx
    leave
    ret

KepContextSwapBadMagic:
    int     $0x3                    # Break on bad context.
    jmp     KepContextSwapReturn    # If we break here, WATCH OUT.

END_FUNCTION(KepContextSwap)

