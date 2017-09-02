/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kernxfr.S

Abstract:

    This module implements the code necessary to transfer execution from the
    loader to the kernel.

Author:

    Evan Green 6-Jun-2017

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x64.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// VOID
// BoEnablePaging (
//     VOID
//     )
//

/*++

Routine Description:

    This routine turns paging on in the processor.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(BoEnablePaging)

    //
    // Enable NX mode. This may not have been done earlier on EFI machines.
    //

    movl    $X86_MSR_EFER, %ecx     # Get EFER as MSR register.
    rdmsr                           # Read it.
    orl     $EFER_NO_EXECUTE_ENABLE, %eax   # OR in the bit.
    wrmsr                           # Write it.

    //
    // Also set the necessary bits in CR4 in case EFI firmware didn't do it.
    //

    movq    %cr4, %rax              # Read CR4.
    orq     $CR4_OR_MASK, %rax      # OR in the PAE bit and others.
    movq    %rax, %cr4              # Set CR4.

    //
    // Set up CR3. Paging may already be on, in which case this switches to the
    // kernel page tables immediately.
    //

    movq    BoPageDirectory@GOTPCREL(%rip), %rax   # Get page directory address.
    movq    (%rax), %rax            # Dereference to get the value.
    movq    %rax, %cr3              # Put it in CR3.

    //
    // Set up CR0, enabling paging, even though by all accounts it really
    // should be on by now.
    //

    movq    %cr0, %rax              # Read CR0.
    orl     $CR0_OR_MASK, %eax      # OR in the right bits.
    andq    $CR0_AND_MASK, %rax     # AND out the right bits.
    movq    %rax, %cr0              # Boom, do it.
    ret                             # Return happily.

END_FUNCTION(BoEnablePaging)

//
// VOID
// BoTransferToKernelAsm (
//     PVOID Parameters,
//     PVOID EntryPoint,
//     PVOID StackAddress
//     )

/*++

Routine Description:

    This routine transfers control of execution to the kernel.

Arguments:

    Parameters - Supplies the parameter block to pass on to the kernel for its
        initialization.

    EntryPoint - Supplies the entry point of the kernel. The function will end
        with a jump to here.

    StackAddress - Supplies the top of the kernel stack. The stack pointer will
        get set to this value just before the kernel is launched.

Return Value:

    None. This function does not return.

--*/

FUNCTION(BoTransferToKernelAsm)
    movq    %rdx, %rsp          # Set up the new stack.
    andq    $~0xF, %rsp         # Align stack down to 16 bytes.
    movl    $0, %ebp            # Zero out the base pointer.
    pushq   %rbp                # Push a dummy return address.
    jmp     *%rsi               # Launch off to the kernel.

    //
    // Jumps are forever, so there's no way to get back here.
    //

LoaderEndLoop:
    int     $3
    jmp     LoaderEndLoop

END_FUNCTION(BoTransferToKernelAsm)

