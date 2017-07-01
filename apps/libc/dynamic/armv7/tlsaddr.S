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

    Evan Green 21-Apr-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/arm.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the offset of the TLS vector within the TCB.
//

.equ TCB_TLS_VECTOR_OFFSET, 0x4

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// LIBC_API
// void *
// __tls_get_addr (
//     PTLS_INDEX Entry
//     )
//

/*++

Routine Description:

    This routine returns the address of a thread-local symbol. References to
    this function are emitted directly by the compiler.

Arguments:

    Entry - Supplies a pointer to the TLS symbol information.

Return Value:

    Returns a pointer to the thread local symbol.

--*/

EXPORTED_FUNCTION __tls_get_addr
    mrc     p15, 0, %r2, c13, c0, 3     @ Get the TPIDRURO register.
    ldr     %r2, [%r2, #TCB_TLS_VECTOR_OFFSET]  @ Get the TLS vector.
    ldr     %r3, [%r2]                  @ Get the generation number.
    ldr     %r1, [%r0]                  @ Get the module ID.
    cmp     %r1, %r3                    @ Compare module ID to generation.
    bgt     __tls_get_addrSlow          @ Go to the slow path if too old.
    ldr     %r2, [%r2, %r1, lsl #2]     @ Get the TLS block for the module.
    cmp     %r2, #0                     @ Compare against zero.
    beq     __tls_get_addrSlow          @ Go to slow path if NULL.
    ldr     %r0, [%r0, #4]              @ Put the offset in R0.
    add     %r0, %r0, %r2               @ Add the TLS block address.
    bx      %lr                         @ Return.

__tls_get_addrSlow:
    b       OsGetTlsAddress             @ Call the OS api.

END_FUNCTION __tls_get_addr

