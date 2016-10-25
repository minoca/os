/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cryptop.h

Abstract:

    This header contains internal definitions for the Cryptographic Library.

Author:

    Evan Green 13-Jan-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#define CRYPTO_API __DLLEXPORT
#define RTL_API __DLLPROTECTED

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>
#include <minoca/lib/crypto.h>

//
// ---------------------------------------------------------------- Definitions
//

#define BIG_INTEGER_RADIX 0x100000000ULL

//
// Define the modulo indices.
//

#define BIG_INTEGER_M_OFFSET 0
#define BIG_INTEGER_P_OFFSET 1
#define BIG_INTEGER_Q_OFFSET 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Big integer functions
//

KSTATUS
CypBiInitializeContext (
    PBIG_INTEGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes a big integer context.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the context was not partially filled
        in correctly.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failed.

--*/

VOID
CypBiDestroyContext (
    PBIG_INTEGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys a big integer context.

Arguments:

    Context - Supplies a pointer to the context to tear down.

Return Value:

    None.

--*/

VOID
CypBiClearCache (
    PBIG_INTEGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys all big integers on the free list for the given
    context.

Arguments:

    Context - Supplies a pointer to the context to clear.
        for.

Return Value:

    None.

--*/

KSTATUS
CypBiCalculateModuli (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    INTN ModOffset
    );

/*++

Routine Description:

    This routine performs some pre-calculations used in modulo reduction
    optimizations.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the modulus that will be used. This value
        will be made permanent.

    ModOffset - Supplies an offset to the moduli that can be used: the standard
        moduli, or the primes p and q.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failed.

--*/

VOID
CypBiReleaseModuli (
    PBIG_INTEGER_CONTEXT Context,
    INTN ModOffset
    );

/*++

Routine Description:

    This routine frees memory associated with moduli for the given offset.

Arguments:

    Context - Supplies a pointer to the big integer context.

    ModOffset - Supplies the index of the moduli to free: the standard
        moduli, or the primes p and q.

Return Value:

    None.

--*/

PBIG_INTEGER
CypBiExponentiateModulo (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    PBIG_INTEGER Exponent
    );

/*++

Routine Description:

    This routine performs exponentiation, modulo a value.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the value to reduce. A reference on this
        value will be released on success.

    Exponent - Supplies the exponent to raise the value to. A reference on this
        value will be released on success.

Return Value:

    Returns a pointer to the exponentiated value on success.

    NULL on allocation failure.

--*/

PBIG_INTEGER
CypBiChineseRemainderTheorem (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    PBIG_INTEGER DpValue,
    PBIG_INTEGER DqValue,
    PBIG_INTEGER PValue,
    PBIG_INTEGER QValue,
    PBIG_INTEGER QInverse
    );

/*++

Routine Description:

    This routine uses the Chinese Remainder Theorem as an aide to quickly
    decrypting RSA values.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the value to perform the exponentiation on. A
        reference on this value will be released on success.

    DpValue - Supplies a pointer to the dP value. A reference on this will be
        released on success.

    DqValue - Supplies a pointer to the dQ value. A reference on this will be
        released on success.

    PValue - Supplies a pointer to the p prime. A reference on this value will
        be released on success.

    QValue - Supplies a pointer to the q prime. A reference on this value will
        be released on success.

    QInverse - Supplies a pointer to the Q inverse. A reference on this will be
        released on success.

Return Value:

    Returns a pointer to the result of the Chinese Remainder Theorem on success.

    NULL on allocation failure.

--*/

PBIG_INTEGER
CypBiImport (
    PBIG_INTEGER_CONTEXT Context,
    PVOID Data,
    UINTN Size
    );

/*++

Routine Description:

    This routine creates a big integer from a set of raw binary bytes.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Data - Supplies a pointer to the data to import.

    Size - Supplies the number of bytes in the data.

Return Value:

    Returns a pointer to the newly created value on success.

    NULL on allocation failure.

--*/

KSTATUS
CypBiExport (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    PVOID Data,
    UINTN Size
    );

/*++

Routine Description:

    This routine exports a big integer to a byte stream.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the big integer to export. A reference is
        released on this value by this function on success.

    Data - Supplies a pointer to the data buffer.

    Size - Supplies the number of bytes in the data buffer.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the given buffer was not big enough to hold the
    entire integer.

--*/

VOID
CypBiDebugPrint (
    PBIG_INTEGER Value
    );

/*++

Routine Description:

    This routine debug prints the contents of a big integer.

Arguments:

    Value - Supplies a pointer to the value to print.

Return Value:

    None.

--*/

VOID
CypBiAddReference (
    PBIG_INTEGER Integer
    );

/*++

Routine Description:

    This routine adds a reference to the given big integer.

Arguments:

    Integer - Supplies a pointer to the big integer.

Return Value:

    None.

--*/

VOID
CypBiReleaseReference (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Integer
    );

/*++

Routine Description:

    This routine releases resources associated with a big integer.

Arguments:

    Context - Supplies a pointer to the context that owns the integer.

    Integer - Supplies a pointer to the big integer.

    ComponentCount - Supplies the number of components to allocate
        for.

Return Value:

    None.

--*/

VOID
CypBiMakePermanent (
    PBIG_INTEGER Integer
    );

/*++

Routine Description:

    This routine makes a big integer "permanent", causing add and release
    references to be ignored.

Arguments:

    Integer - Supplies a pointer to the big integer.

Return Value:

    None.

--*/

VOID
CypBiMakeNonPermanent (
    PBIG_INTEGER Integer
    );

/*++

Routine Description:

    This routine undoes the effects of making a big integer permanent,
    instead giving it a reference count of 1.

Arguments:

    Integer - Supplies a pointer to the big integer.

Return Value:

    None.

--*/

KSTATUS
CypAsn1AddPrivateKey (
    PRSA_CONTEXT RsaContext,
    PVOID PemData,
    UINTN PemDataSize
    );

/*++

Routine Description:

    This routine extracts a private key given ASN.1 data.

Arguments:

    RsaContext - Supplies a pointer to an initialized RSA context where the
        private key should be placed.

    PemData - Supplies a pointer to the ASN.1 data, which must have already
        been Base64 decoded and decrypted if necessary prior to calling this
        function.

    PemDataSize - Supplies the size of the ASN data in bytes.

Return Value:

    Status code.

--*/

