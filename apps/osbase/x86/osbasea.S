/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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
## VOID
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

    None.

--*/

FUNCTION(OspSystemCallFull)
    movl    4(%esp), %ecx       # Set the system call number.
    movl    8(%esp), %edx       # Set the system call parameter.
    int     $0x2F               # Perform the system call.
    ret                         # Return.

END_FUNCTION(OspSystemCallFull)

##
## VOID
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

    None.

--*/

FUNCTION(OspSysenterSystemCall)
    pushl   %ebx                # Save EBX.
    call    OspSysenter         # Perform a call to get the return address.
    popl    %ebx                # Restore EBX.
    ret                         # Return.

##
## This small fake-ish function is used to be able to get the return address
## without generating text relocations.
##

OspSysenter:
    popl    %edx                # Put the return address in EDX for sysenter.
    movl    8(%esp), %eax       # Move the system call number to EAX.
    movl    12(%esp), %ebx      # Move the system call parameter to EBX.
    movl    %esp, %ecx          # ECX contains the return stack pointer.
    sysenter                    # Jump to kernel mode.

END_FUNCTION(OspSysenterSystemCall)

##
## VOID
## OspSignalHandler (
##     )
##

/*++

Routine Description:

    This routine is called directly by the kernel when a signal occurs. It
    marshals the parameters and calls the C routine for handling the signal.

Arguments:

    None. The parameters are stored in registers, but do not conform to any C
        calling convention.

Return Value:

    None.

--*/

FUNCTION(OspSignalHandler)
    pushl   %esi                # Push ExitStatus/ValueParameter.
    pushl   %ebx                # Push Sending User ID.
    pushl   %eax                # Push FaultAddress/BandEvent/SnedingProcess.
    pushl   %ebp                # Push the error number.
    xorl    %ebp, %ebp          # Zero out that funky base pointer.
    pushl   %edi                # Push the signal number and code.
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
    pushl   %eax                # Save eax in case of ___tls_get_addr.
    pushl   %ecx                # Save ecx in case of fastcall.
    pushl   %edx                # Save edx in case of fastcall.
    movl    16(%esp), %eax      # Get relocation offset argument.
    pushl   %eax                # Push it.
    movl    16(%esp), %eax      # Get the image pointer.
    pushl   %eax                # Push it.
    call    OspImResolvePltEntry    # Call the C handler
    movl    %eax, 24(%esp)      # Save the address to fake return to later.
    movl    8(%esp), %edx       # Restore edx.
    movl    12(%esp), %ecx      # Restore ecx.
    movl    16(%esp), %eax      # Restore eax.
    addl    $24, %esp           # Pop nearly everything, including parameters.
    ret                         # "Return" to the function destination.

END_FUNCTION(OspImArchResolvePltEntry)

