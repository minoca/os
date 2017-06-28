/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    xfera.S

Abstract:

    This module implements the assembly trampoline that transfers between
    a 32-bit boot application and a 64-bit boot application. This is where
    long mode and paging are enabled.

Author:

    Evan Green 31-May-2017

Environment:

    Boot

--*/

##
## ------------------------------------------------------------------ Includes
##

//
// Include the 32-bit header since this is really only compiled in 32-bit mode.
//

#include <minoca/kernel/x86.inc>

##
## ---------------------------------------------------------------------- Code
##

.text
.code32

##
## INT
## BmpFwTransferTo64BitApplication (
##     PBOOT_INITIALIZATION_BLOCK Parameters,
##     PBOOT_APPLICATION_ENTRY EntryPoint,
##     ULONG PageDirectory
##     )
##

/*++

Routine Description:

    This routine enables paging, enables long mode, and jumps to the
    application entry point. Upon return, it returns to 32-bit mode and disables
    paging. This function starts and ends in 32-bit mode.

Arguments:

    Parameters - Supplies the parameters to pass to the application entry
        point. This is a 32-bit pointer.

    EntryPoint - Supplies a pointer to the function to call. This is a 32-bit
        pointer.

    PageDirectory - Supplies the physical address of the value to set in CR3.
        This is a 32-bit value.

Return Value:

    Returns the value returned from the entry point function. Usually on
    success, this does not return, but launches the OS instead.

--*/

FUNCTION(BmpFwTransferTo64BitApplication)
    pushl   %ebp                    # Save previous frame pointer.
    movl    %esp, %ebp              # Set new frame pointer.
    CFI_DEF_CFA(%ebp, 8)            # Return address is counted in this frame.
    andl    $0xFFFFFFF0, %esp       # Align the stack.
    pushl   %edi                    # Save EDI, first 64-bit parameter register.
    pushl   %esi                    # Save ESI, volatile in 64-bit land.
    movl    8(%ebp), %edi           # Load the parameters parameter.
    movl    12(%ebp), %esi          # Load the function pointer.
    movl    16(%ebp), %eax          # Load up the page directory.
    movl    %cr3, %ecx              # Get the old CR3.
    pushl   %ecx                    # Save it, probably not necessary.
    movl    %eax, %cr3              # Set the new CR3.
    subl    $4, %esp                # Dummy push for 16-byte stack alignment.
    pushl   $0                      # Zero high word of selector.
    pushl   $KERNEL_CS              # Push CS selector.
    pushl   $0                      # Zero high word of return RIP.
    pushl   $ProtectedModeReturn    # Push "return" address to protected mode.

    ##
    ## Enable long mode in EFER. Do this before turning paging on so that
    ## 4-level paging is used rather than 32-bit PAE 3-level paging.
    ##

    movl    $X86_MSR_EFER, %ecx     # Get EFER as MSR register.
    rdmsr                           # Read it.
    orl     $(EFER_LONG_MODE_ENABLE | EFER_NO_EXECUTE_ENABLE), %eax
    wrmsr                           # Write it.

    ##
    ## Enable paging.
    ##

    movl    %cr4, %eax              # Read CR4.
    orl     $CR4_OR_MASK, %eax      # OR in the PAE bit and others.
    movl    %eax, %cr4              # Set CR4.
    movl    %cr0, %eax              # Read CR0.
    orl     $CR0_OR_MASK, %eax      # OR in the right bits.
    andl    $CR0_AND_MASK, %eax     # AND out the right bits.
    movl    %eax, %cr0              # Boom, paging enabled.

    ##
    ## Perform a long jump to get to long mode. A long mode GDT entry was set
    ## up for this purpose. The next boot application will set up their own
    ## GDT, so use of this selector is short lived.
    ##

    ljmp    $KERNEL64_TRANSITION_CS, $LongModeCode

LongModeCode:
.code64

    ##
    ## Wow, long mode. Save the GDT and IDT because who knows what sort of mess
    ## the next application will get into.
    ##

    subq    $32, %rsp               # Make (aligned) space on the stack.
    sidt    16(%rsp)                # Save the IDT.
    sgdt    (%rsp)                  # Save the GDT.

    ##
    ## Off to adventure time with the next boot application.
    ##

    callq   *%rsi                   # Away we go.

    ##
    ## Back from the adventure. Restore the old structures.
    ##

    lgdt    (%rsp)                  # Restore the GDT.
    lidt    16(%rsp)                # Restore the IDT.
    addq    $32, %rsp               # Pop that stuff off the stack.

    ##
    ## Perform a far return, which pops a return RIP and CS. This will switch
    ## things back to 32-bit mode.
    ##

    retfq

.code32
ProtectedModeReturn:

    ##
    ## Disable paging.
    ##

    movl    %cr0, %ecx              # Get CR0.
    andl    $~CR0_PAGING_ENABLE, %ecx   # Remove paging enable bit.
    movl    %ecx, %cr0              # Save to disable paging.

    ##
    ## Disable long mode.
    ##

    movl    %eax, %edi              # Save return value from boot application.
    movl    $X86_MSR_EFER, %ecx     # Get EFER as MSR register.
    rdmsr                           # Read it.
    andl    $~EFER_LONG_MODE_ENABLE, %eax    # Disable long mode.
    wrmsr                           # Write it.
    movl    %edi, %eax              # Restore return value to eax.

    ##
    ## Restore things and return.
    ##

    addl    $4, %esp                # Remove the dummy alignment value.
    popl    %edi                    # Pop the old CR3.
    movl    %edi, %cr3              # Restore it.
    popl    %esi                    # Restore esi.
    popl    %edi                    # Restore the original edi.
    leave                           # Set esp to ebp, and pop ebp.
    ret

END_FUNCTION(BmpFwTransferTo64BitApplication)

##
## --------------------------------------------------------- Internal Functions
##

