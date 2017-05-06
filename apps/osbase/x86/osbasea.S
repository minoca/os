/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osbasea.S

Abstract:

    This module implements assembly support for the OS Base library.

Author:

    Evan Green 25-Feb-2013

Environment:

    User Mode

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/x86.inc>

##
## ---------------------------------------------------------------- Definitions
##

##
## ----------------------------------------------------------------------- Code
##

##
## .text specifies that this code belongs in the executable section.
##
## .code32 specifies that this is 32-bit protected mode code.
##

.text
.code32

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
    movl    4(%esp), %ecx       # Set the system call number.
    movl    8(%esp), %edx       # Set the system call parameter.
    int     $0x2F               # Perform the system call.
    ret                         # Return.

END_FUNCTION(OspSystemCallFull)

##
## INTN
## OspSysenterSystemCall (
##     ULONG SystemCallNumber,
##     PVOID SystemCallParameter
##     )
##

/*++

Routine Description:

    This routine executes a system call using the sysenter instruction, which
    provides a fast jump to kernel mode on mostly-Intel machines.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

FUNCTION(OspSysenterSystemCall)
    pushl   %ebx                # Save EBX.
    CFI_ADJUST_CFA_OFFSET(4)    # Let the debugger know about the push.
    call    OspSysenter         # Perform a call to get the return address.
    popl    %ebx                # Restore EBX.
    CFI_ADJUST_CFA_OFFSET(-4)   # Let the debugger know about the pop.
    ret                         # Return.

##
## This small fake-ish function is used to be able to get the return address
## without generating text relocations. The CFA offset is explicitly set to 8
## because the debugger does not know that this is a function, so the stack
## has the return address and ebx on it.
##

OspSysenter:
    CFI_DEF_CFA_OFFSET(8)
    popl    %ebx                # Put the return address in EBX for sysenter.
    CFI_ADJUST_CFA_OFFSET(-4)   # Tell the debugger about the pop.
    movl    8(%esp), %ecx       # Move the system call number to ECX.
    movl    12(%esp), %edx      # Move the system call parameter to EDX.
    movl    %esp, %eax          # EAX contains the return stack pointer.
    sysenter                    # Jump to kernel mode.
    int $3                      # Execution should never get here.

END_FUNCTION(OspSysenterSystemCall)

##
## VOID
## OspSignalHandler (
##     PSIGNAL_PARAMETERS Parameters,
##     PSIGNAL_CONTEXT Context
##     )
##

/*++

Routine Description:

    This routine is called directly by the kernel when a signal occurs. It
    marshals the parameters and calls the C routine for handling the signal.
    The parameters are stored on the stack with the signal parameters followed
    by the signal context.

Arguments:

    Parameters - Supplies a pointer to the signal parameters. Beyond that on
        the stack is the restore structure.

    Context - Supplies a pointer to the signal context from the kernel.

Return Value:

    None.

--*/

FUNCTION(OspSignalHandler)
    CFI_DEF_CFA_OFFSET(SIGNAL_PARAMETERS_SIZE + SIGNAL_CONTEXT_SIZE)
    CFI_OFFSET(%eax, TRAP_EAX)
    CFI_OFFSET(%ebx, TRAP_EBX)
    CFI_OFFSET(%ecx, TRAP_ECX)
    CFI_OFFSET(%edx, TRAP_EDX)
    CFI_OFFSET(%esi, TRAP_ESI)
    CFI_OFFSET(%edi, TRAP_EDI)
    CFI_OFFSET(%ebp, TRAP_EBP)
    CFI_OFFSET(%eip, TRAP_EIP)
    CFI_OFFSET(%esp, TRAP_ESP)

    ##
    ## Volatile registers are either saved or don't matter. Non-volatiles will
    ## be saved and restored by any C routine called.
    ##

    movl    %esp, %eax          # Get a pointer to the signal parameters.
    leal    SIGNAL_PARAMETERS_SIZE(%eax), %ecx # Get a pointer to the context.
    pushl   %ecx                # Push a pointer to the signal context.
    pushl   %eax                # Push a pointer to the signal parameters.
    CFI_ADJUST_CFA_OFFSET(8)    # Tell the debugger.
    call    OspProcessSignal    # Call out to the C routine to handle it.
    addl    $(SIGNAL_PARAMETERS_SIZE + 8), %esp # Pop arguments + signal params.
    pushl   %esp                # Push system call parameter (signal context).
    pushl   $SystemCallRestoreContext # Push system call number.
    CFI_ADJUST_CFA_OFFSET(-SIGNAL_PARAMETERS_SIZE)
    call    OspSystemCallFull   # Call the full restore handler.
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
    mov   %gs:(0), %eax         # Load the Self pointer.
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
    CFI_ADJUST_CFA_OFFSET(8)    # Image and offset are already pushed.
    pushl   %eax                # Save eax in case of ___tls_get_addr.
    pushl   %ecx                # Save ecx in case of fastcall.
    pushl   %edx                # Save edx in case of fastcall.
    movl    16(%esp), %eax      # Get relocation offset argument.
    pushl   %eax                # Push it.
    movl    16(%esp), %eax      # Get the image pointer.
    pushl   %eax                # Push it.
    CFI_ADJUST_CFA_OFFSET(20)   # Account for the pushes.
    call    OspImResolvePltEntry    # Call the C handler
    movl    %eax, 24(%esp)      # Save the address to fake return to later.
    movl    8(%esp), %edx       # Restore edx.
    movl    12(%esp), %ecx      # Restore ecx.
    movl    16(%esp), %eax      # Restore eax.
    addl    $24, %esp           # Pop nearly everything, including parameters.
    CFI_ADJUST_CFA_OFFSET(-24)  # Account for the pops.
    ret                         # "Return" to the function destination.

END_FUNCTION(OspImArchResolvePltEntry)

