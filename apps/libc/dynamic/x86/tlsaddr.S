/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tlsaddr.S

Abstract:

    This module implements functionality for fast access Thread Local Storage
    symbol lookup.

Author:

    Evan Green 20-Apr-2015

Environment:

    User Mode C Library

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/x86.inc>

##
## ---------------------------------------------------------------- Definitions
##

##
## Define the offset of the TLS vector within the TCB.
##

.equ TCB_TLS_VECTOR_OFFSET, 0x4

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
## LIBC_API
## void *
## ___tls_get_addr (
##     PTLS_INDEX Entry
##     )
##

/*++

Routine Description:

    This routine returns the address of a thread-local symbol. References to
    this function are emitted directly by the compiler.

Arguments:

    Entry - Supplies a pointer to the TLS symbol information. This argument is
        supplied in the eax register.

Return Value:

    Returns a pointer to the thread local symbol.

--*/

EXPORTED_FUNCTION(___tls_get_addr)
    movl    %gs:(TCB_TLS_VECTOR_OFFSET), %ecx    # Get the TLS vector.
    movl    (%eax), %edx        # Get the module ID.
    cmpl    %edx, (%ecx)        # Compare module ID to generation number.
    jb      ___tls_get_addrSlowPath
    movl    (%ecx, %edx, 4), %ecx   # Get the vector[ModuleId].
    cmpl    $0, %ecx            # Compare against NULL.
    je      ___tls_get_addrSlowPath # Do the slow path if it's the first run.
    movl    4(%eax), %eax       # Get the TLS entry offset.
    addl     %ecx, %eax         # Add in the TLS base.
    ret                         # Return the value.

    ##
    ## Call the slow C routine.
    ##

___tls_get_addrSlowPath:
    pushl   %ebx                # Save ebx.
    call    __x86.get_pc_thunk.bx # ebx must be set up correctly for the PLT.
    addl    $_GLOBAL_OFFSET_TABLE_,%ebx # Get GOT address.
    pushl   %eax                # Push the structure parameter.
    call    OsGetTlsAddress@PLT # Call the OS API routine.
    addl    $4, %esp            # Pop the parameter.
    popl    %ebx                # Restore ebx.
    ret                         # Return.

END_FUNCTION(___tls_get_addr)

