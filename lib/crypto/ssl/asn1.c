/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    asn1.c

Abstract:

    This module implements limited support for extracting SSL-related data in
    ASN.1 format.

Author:

    Evan Green 17-Aug-2015

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../cryptop.h"

//
// ---------------------------------------------------------------- Definitions
//

#define ASN1_INTEGER 0x02
#define ASN1_SEQUENCE 0x30

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
CypAsn1GetInteger (
    PUCHAR Buffer,
    UINTN BufferSize,
    PUINTN Offset,
    PVOID *Integer,
    PUINTN IntegerLength
    );

KSTATUS
CypAsn1GetObject (
    PUCHAR Buffer,
    UINTN BufferSize,
    PUINTN Offset,
    ULONG ObjectType,
    PUINTN ObjectLength
    );

KSTATUS
CypAsn1GetLength (
    PUCHAR Buffer,
    UINTN BufferSize,
    PUINTN Offset,
    PUINTN ObjectLength
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
CypAsn1AddPrivateKey (
    PRSA_CONTEXT RsaContext,
    PVOID PemData,
    UINTN PemDataSize
    )

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

{

    PUCHAR Bytes;
    RSA_PRIVATE_KEY_COMPONENTS Components;
    UINTN Offset;
    KSTATUS Status;

    Bytes = PemData;
    if (*Bytes != ASN1_SEQUENCE) {
        return STATUS_UNKNOWN_IMAGE_FORMAT;
    }

    Offset = 7;
    RtlZeroMemory(&Components, sizeof(RSA_PRIVATE_KEY_COMPONENTS));
    Status = CypAsn1GetInteger(Bytes,
                               PemDataSize,
                               &Offset,
                               &(Components.PublicKey.Modulus),
                               &(Components.PublicKey.ModulusLength));

    if (!KSUCCESS(Status)) {
        goto Asn1AddPrivateKeyEnd;
    }

    Status = CypAsn1GetInteger(Bytes,
                               PemDataSize,
                               &Offset,
                               &(Components.PublicKey.PublicExponent),
                               &(Components.PublicKey.PublicExponentLength));

    if (!KSUCCESS(Status)) {
        goto Asn1AddPrivateKeyEnd;
    }

    Status = CypAsn1GetInteger(Bytes,
                               PemDataSize,
                               &Offset,
                               &(Components.PrivateExponent),
                               &(Components.PrivateExponentLength));

    if (!KSUCCESS(Status)) {
        goto Asn1AddPrivateKeyEnd;
    }

    Status = CypAsn1GetInteger(Bytes,
                               PemDataSize,
                               &Offset,
                               &(Components.PValue),
                               &(Components.PValueLength));

    if (!KSUCCESS(Status)) {
        goto Asn1AddPrivateKeyEnd;
    }

    Status = CypAsn1GetInteger(Bytes,
                               PemDataSize,
                               &Offset,
                               &(Components.QValue),
                               &(Components.QValueLength));

    if (!KSUCCESS(Status)) {
        goto Asn1AddPrivateKeyEnd;
    }

    Status = CypAsn1GetInteger(Bytes,
                               PemDataSize,
                               &Offset,
                               &(Components.DpValue),
                               &(Components.DpValueLength));

    if (!KSUCCESS(Status)) {
        goto Asn1AddPrivateKeyEnd;
    }

    Status = CypAsn1GetInteger(Bytes,
                               PemDataSize,
                               &Offset,
                               &(Components.DqValue),
                               &(Components.DqValueLength));

    if (!KSUCCESS(Status)) {
        goto Asn1AddPrivateKeyEnd;
    }

    Status = CypAsn1GetInteger(Bytes,
                               PemDataSize,
                               &Offset,
                               &(Components.QInverse),
                               &(Components.QInverseLength));

    if (!KSUCCESS(Status)) {
        goto Asn1AddPrivateKeyEnd;
    }

    Status = CyRsaLoadPrivateKey(RsaContext, &Components);
    if (!KSUCCESS(Status)) {
        goto Asn1AddPrivateKeyEnd;
    }

Asn1AddPrivateKeyEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
CypAsn1GetInteger (
    PUCHAR Buffer,
    UINTN BufferSize,
    PUINTN Offset,
    PVOID *Integer,
    PUINTN IntegerLength
    )

/*++

Routine Description:

    This routine parses an integer out of an ASN.1 sequence.

Arguments:

    Buffer - Supplies a pointer to the data buffer.

    BufferSize - Supplies the total size of the buffer in bytes.

    Offset - Supplies a pointer that on input contains the offset to the data
        buffer. On output, this offset will be updated beyond the integer.

    Integer - Supplies a pointer where a pointer to the integer data will be
        returned on success.

    IntegerLength - Supplies a pointer where the length of the integer in bytes
        will be returned.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = CypAsn1GetObject(Buffer,
                              BufferSize,
                              Offset,
                              ASN1_INTEGER,
                              IntegerLength);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (*Offset + *IntegerLength > BufferSize) {
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    //
    // Potentially ignore the negative byte.
    //

    if ((*IntegerLength > 1) && (Buffer[*Offset] == 0x00)) {
        *IntegerLength -= 1;
        *Offset += 1;
    }

    *Integer = Buffer + *Offset;
    *Offset += *IntegerLength;
    return Status;
}

KSTATUS
CypAsn1GetObject (
    PUCHAR Buffer,
    UINTN BufferSize,
    PUINTN Offset,
    ULONG ObjectType,
    PUINTN ObjectLength
    )

/*++

Routine Description:

    This routine parses the next object type and length out of an ASN.1
    sequence.

Arguments:

    Buffer - Supplies a pointer to the data buffer.

    BufferSize - Supplies the total size of the buffer in bytes.

    Offset - Supplies a pointer that on input contains the offset to the data
        buffer. On output, this offset will be updated beyond the object type
        and length.

    ObjectType - Supplies the expected object type.

    ObjectLength - Supplies a pointer where the length of the object will be
        returned.

Return Value:

    Status code.

--*/

{

    if (*Offset >= BufferSize) {
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    if (Buffer[*Offset] != ObjectType) {
        return STATUS_UNEXPECTED_TYPE;
    }

    *Offset += 1;
    return CypAsn1GetLength(Buffer, BufferSize, Offset, ObjectLength);
}

KSTATUS
CypAsn1GetLength (
    PUCHAR Buffer,
    UINTN BufferSize,
    PUINTN Offset,
    PUINTN ObjectLength
    )

/*++

Routine Description:

    This routine parses a length field from an ASN.1 sequence.

Arguments:

    Buffer - Supplies a pointer to the data buffer.

    BufferSize - Supplies the total size of the buffer in bytes.

    Offset - Supplies a pointer that on input contains the offset to the data
        buffer. On output, this offset will be updated beyond the length.

    ObjectType - Supplies the expected object type.

    ObjectLength - Supplies a pointer where the length of the object will be
        returned.

Return Value:

    Status code.

--*/

{

    UCHAR Byte;
    ULONG ByteCount;
    ULONG ByteIndex;
    ULONG Length;

    if (*Offset >= BufferSize) {
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    Byte = Buffer[*Offset];
    *Offset += 1;
    if ((Byte & 0x80) == 0) {
        *ObjectLength = Byte;

    } else {
        ByteCount = Byte & 0x7F;
        if (*Offset + ByteCount > BufferSize) {
            return STATUS_DATA_LENGTH_MISMATCH;
        }

        Length = 0;
        for (ByteIndex = 0; ByteIndex < ByteCount; ByteIndex += 1) {
            Length <<= 8;
            Length += Buffer[*Offset];
            *Offset += 1;
        }

        *ObjectLength = Length;
    }

    return STATUS_SUCCESS;
}

