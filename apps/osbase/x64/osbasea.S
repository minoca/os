/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osbasea.S

Abstract:

    This module implements assembly support for the OS Base library.

Author:

    Evan Green 17-Jan-2015

Environment:

    User Mode

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
## INTN
## OspSystemCallFull (
##     ULONG SystemCallNumber,
##     PVOID SystemCallParameter
##     )
##

/*++

Routine Description:

    This routine executes a system call using the traditional "int x" method.
    This method is highly compatible, but slow.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

FUNCTION(OspSystemCallFull)
    syscall                     # Just do that system call, params in rdi, rsi.
    ret                         # Return.

END_FUNCTION(OspSystemCallFull)

##
## VOID
## OspSignalHandler (
##     )
##

/*++

Routine Description:

    This routine is called directly by the kernel when a signal occurs. It
    marshals the parameters and calls the C routine for handling the signal.
    The parameters are stored on the stack with the signal parameters followed
    by the signal context.

Arguments:

    None. The parameters are stored in registers, but do not conform to any C
        calling convention.

Return Value:

    None.

--*/

FUNCTION(OspSignalHandler)
    int     $3                  # TODO: Check this, it's never been run!
    call    OspProcessSignal    # Call the processing routine.
    int     $3                  # Execution should never get back here.

END_FUNCTION(OspSignalHandler)

##
## PTHREAD_CONTROL_BLOCK
## OspGetThreadControlBlock (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns a pointer to the thread control block, a structure
    unique to each thread.

Arguments:

    None.

Return Value:

    Returns a pointer to the current thread's control block.

--*/

FUNCTION(OspGetThreadControlBlock)
    int     $3                  # TODO: Check this, it's never been run!
    movq    %gs:(0), %rax       # Load the Self pointer.
    ret                         # Return.

END_FUNCTION(OspGetThreadControlBlock)

##
## VOID
## OspImArchResolvePltEntry (
##     PLOADED_IMAGE Image,
##     ULONG RelocationOffset
##     )
##

/*++

Routine Description:

    This routine implements the slow path for a Procedure Linkable Table entry
    that has not yet been resolved to its target function address. This routine
    is only called once for each PLT entry, as subsequent calls jump directly
    to the destination function address.

Arguments:

    Image - Supplies a pointer to the loaded image whose PLT needs resolution.
        This is really whatever pointer is in GOT + 4.

    RelocationOffset - Supplies the byte offset from the start of the
        relocation section where the relocation for this PLT entry resides.

Return Value:

    None. Control jumps directly to the destination function, rather than
    returning.

--*/

FUNCTION(OspImArchResolvePltEntry)
    int     $3                  # TODO: Check this, it's never been run!
    CFI_ADJUST_CFA_OFFSET(16)   # Image and offset are already pushed.
    pushq   %rax                # Save eax in case of ___tls_get_addr.
    pushq   %rcx                # Save ecx in case of fastcall.
    pushq   %rdx                # Save edx in case of fastcall.
    movq    32(%rsp), %rax      # Get relocation offset argument.
    pushq   %rax                # Push it.
    movq    32(%esp), %rax      # Get the image pointer.
    pushq   %rax                # Push it.
    CFI_ADJUST_CFA_OFFSET(40)   # Account for the pushes.
    call    OspImResolvePltEntry    # Call the C handler
    movq    %rax, 48(%rsp)      # Save the address to fake return to later.
    movq    16(%rsp), %rdx       # Restore edx.
    movq    24(%rsp), %rcx      # Restore ecx.
    movq    32(%rsp), %rax      # Restore eax.
    addq    $48, %rsp           # Pop nearly everything, including parameters.
    CFI_ADJUST_CFA_OFFSET(-48)  # Account for the pops.
    ret                         # "Return" to the function destination.

END_FUNCTION(OspImArchResolvePltEntry)
