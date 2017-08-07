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

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x64.inc>

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

    Entry - Supplies a pointer to the TLS symbol information. This argument is
        supplied in the eax register.

Return Value:

    Returns a pointer to the thread local symbol.

--*/

EXPORTED_FUNCTION(__tls_get_addr)

    //
    // TODO: Implement ___tls_get_addr for x64.
    //

    int $3

END_FUNCTION(__tls_get_addr)

