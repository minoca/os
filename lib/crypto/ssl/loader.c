/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    loader.c

Abstract:

    This module add a keys to an RSA context.

Author:

    Evan Green 16-Aug-2015

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

#define CY_PEM_PROC_TYPE "Proc-Type:"
#define CY_PEM_ENCRYPTED "4,ENCRYPTED"
#define CY_PEM_ENCRYPTION_AES_128 "DEK-Info: AES-128-CBC,"
#define CY_PEM_ENCRYPTION_AES_256 "DEK-Info: AES-256-CBC,"

#define CY_PEM_SALT_SIZE 8

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CY_PEM_TYPE {
    CyPemTypeRsaPrivateKey,
    CyPemTypeCount
} CY_PEM_TYPE, *PCY_PEM_TYPE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
CypPemDecrypt (
    PSTR PemData,
    UINTN PemDataSize,
    PSTR Password,
    PVOID Data,
    PUINTN DataSize
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR CyPemTypeBeginStrings[CyPemTypeCount] = {
    "-----BEGIN RSA PRIVATE KEY-----",
};

PSTR CyPemTypeEndStrings[CyPemTypeCount] = {
    "-----END RSA PRIVATE KEY-----",
};

//
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
KSTATUS
CyRsaAddPemFile (
    PRSA_CONTEXT RsaContext,
    PVOID PemFile,
    UINTN PemFileLength,
    PSTR Password
    )

/*++

Routine Description:

    This routine attempts to add a private key to the given RSA context.

Arguments:

    RsaContext - Supplies a pointer to the previously initialized RSA context.

    PemFile - Supplies a pointer to the PEM file contents.

    PemFileLength - Supplies the length of the PEM file contents.

    Password - Supplies an optional pointer to a password to decrypt the
        private key if needed.

Return Value:

    Status code.

--*/

{

    PSTR CurrentPem;
    UINTN CurrentPemLength;
    PSTR End;
    UINTN EndLength;
    PSTR FileCopy;
    PSTR Match;
    PUCHAR PemData;
    UINTN PemSize;
    CY_PEM_TYPE PemType;
    BOOL Result;
    PSTR Search;
    PSTR Start;
    UINTN StartLength;
    KSTATUS Status;

    PemData = NULL;

    //
    // Create a null terminated copy of the file.
    //

    FileCopy = RsaContext->BigIntegerContext.AllocateMemory(PemFileLength + 1);
    if (FileCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RsaAddPemFileEnd;
    }

    RtlCopyMemory(FileCopy, PemFile, PemFileLength + 1);
    FileCopy[PemFileLength] = '\0';
    CurrentPem = FileCopy;
    CurrentPemLength = PemFileLength + 1;
    while (CurrentPemLength != 0) {
        for (PemType = 0; PemType < CyPemTypeCount; PemType += 1) {
            Search = CyPemTypeBeginStrings[PemType];
            StartLength = RtlStringLength(Search);
            Start = RtlStringSearch(CurrentPem,
                                    CurrentPemLength,
                                    Search,
                                    StartLength + 1);

            if (Start == NULL) {
                continue;
            }

            Search = CyPemTypeEndStrings[PemType];
            EndLength = RtlStringLength(Search);
            End = RtlStringSearch(CurrentPem,
                                  CurrentPemLength,
                                  Search,
                                  EndLength + 1);

            if (End == NULL) {
                continue;
            }

            CurrentPemLength -= (End - CurrentPem);
            Start += StartLength;
            PemSize = End - Start;
            PemData = RsaContext->BigIntegerContext.AllocateMemory(PemSize);
            if (PemData == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto RsaAddPemFileEnd;
            }

            Match = NULL;
            if (PemType == CyPemTypeRsaPrivateKey) {
                Match = RtlStringSearch(Start,
                                        CurrentPemLength,
                                        CY_PEM_PROC_TYPE,
                                        sizeof(CY_PEM_PROC_TYPE));

                if (Match != NULL) {
                    Match = RtlStringSearch(Start,
                                            CurrentPemLength,
                                            CY_PEM_ENCRYPTED,
                                            sizeof(CY_PEM_ENCRYPTED));

                    if (Match != NULL) {
                        Status = CypPemDecrypt(Start,
                                               PemSize,
                                               Password,
                                               PemData,
                                               &PemSize);

                        if (!KSUCCESS(Status)) {
                            goto RsaAddPemFileEnd;
                        }
                    }
                }
            }

            //
            // If the data didn't end up being decrypted, decode it now.
            //

            if (Match == NULL) {
                Result = CyBase64Decode(Start, PemSize, PemData, &PemSize);
                if (Result == FALSE) {
                    Status = STATUS_INVALID_PARAMETER;
                    goto RsaAddPemFileEnd;
                }
            }

            Status = CypAsn1AddPrivateKey(RsaContext, PemData, PemSize);
            if (!KSUCCESS(Status)) {
                goto RsaAddPemFileEnd;
            }

            End += EndLength;
            CurrentPemLength -= EndLength;
            while ((CurrentPemLength > 0) &&
                   ((*End == '\r') || (*End == '\n'))) {

                End += 1;
                CurrentPemLength -= 1;
            }

            CurrentPem = End;
            break;
        }

        if (PemData != NULL) {
            RsaContext->BigIntegerContext.FreeMemory(PemData);
            PemData = NULL;
        }

        //
        // If nothing was found in the remainder of the string, stop
        // looking for anything.
        //

        if (Start == NULL) {
            break;
        }
    }

    Status = STATUS_SUCCESS;

RsaAddPemFileEnd:
    if (PemData != NULL) {
        RsaContext->BigIntegerContext.FreeMemory(PemData);
    }

    if (FileCopy != NULL) {
        RsaContext->BigIntegerContext.FreeMemory(FileCopy);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
CypPemDecrypt (
    PSTR PemData,
    UINTN PemDataSize,
    PSTR Password,
    PVOID Data,
    PUINTN DataSize
    )

/*++

Routine Description:

    This routine decrypts an encrypted RSA private key.

Arguments:

    PemData - Supplies a pointer to the encrypted PEM data.

    PemDataSize - Supplies the size of the PEM data.

    Password - Supplies the password to use for decryption.

    Data - Supplies the output data buffer.

    DataSize - Supplies a pointer that on input contains the size of the output
        data buffer in bytes. On output, returns the actual size of the output
        data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_PERMISSION_DENIED if no password was supplied.

    STATUS_INVALID_CONFIGURATION if an unknown encryption cipher was used.

    Other error codes on other failures.

--*/

{

    AES_CONTEXT AesContext;
    CHAR Character;
    AES_CIPHER_MODE CipherMode;
    UINTN Index;
    UCHAR InitializationVector[AES_INITIALIZATION_VECTOR_SIZE];
    UCHAR Key[AES_CBC256_KEY_SIZE];
    MD5_CONTEXT Md5Context;
    BOOL Result;
    PSTR Start;

    if ((Password == NULL) || (*Password == '\0')) {
        return STATUS_PERMISSION_DENIED;
    }

    Start = RtlStringSearch(PemData,
                            PemDataSize,
                            CY_PEM_ENCRYPTION_AES_128,
                            sizeof(CY_PEM_ENCRYPTION_AES_128));

    if (Start != NULL) {
        CipherMode = AesModeCbc128;
        Start += sizeof(CY_PEM_ENCRYPTION_AES_128) - 1;

    } else {
        Start = RtlStringSearch(PemData,
                                PemDataSize,
                                CY_PEM_ENCRYPTION_AES_256,
                                sizeof(CY_PEM_ENCRYPTION_AES_256));

        if (Start == NULL) {
            return STATUS_INVALID_CONFIGURATION;
        }

        CipherMode = AesModeCbc256;
        Start += sizeof(CY_PEM_ENCRYPTION_AES_256) - 1;
    }

    PemDataSize -= Start - PemData;
    if (PemDataSize < AES_INITIALIZATION_VECTOR_SIZE * 2) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Grab the initialization vector, which comes right after the cipher type.
    //

    for (Index = 0; Index < AES_INITIALIZATION_VECTOR_SIZE * 2; Index += 1) {
        Character = *Start;
        Start += 1;
        if (RtlIsCharacterDigit(Character)) {
            Character -= '0';

        } else {
            Character = RtlConvertCharacterToLowerCase(Character);
            if ((Character >= 'a') && (Character <= 'f')) {
                Character -= 'a';
                Character += 10;

            } else {
                return STATUS_INVALID_PARAMETER;
            }
        }

        if ((Index & 0x1) != 0) {
            InitializationVector[Index >> 1] |= Character;

        } else {
            InitializationVector[Index >> 1] = Character << 4;
        }
    }

    PemDataSize -= AES_INITIALIZATION_VECTOR_SIZE * 2;
    while ((PemDataSize > 0) &&
           ((*Start == '\r') || (*Start == '\n'))) {

        Start += 1;
        PemDataSize -= 1;
    }

    Result = CyBase64Decode(Start, PemDataSize, Data, DataSize);
    if (Result == FALSE) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Figure out the AES key.
    //

    ASSERT(MD5_HASH_SIZE * 2 == AES_CBC256_KEY_SIZE);

    CyMd5Initialize(&Md5Context);
    CyMd5AddContent(&Md5Context, Password, RtlStringLength(Password));
    CyMd5AddContent(&Md5Context, InitializationVector, CY_PEM_SALT_SIZE);
    CyMd5GetHash(&Md5Context, Key);
    if (CipherMode == AesModeCbc256) {
        CyMd5Initialize(&Md5Context);
        CyMd5AddContent(&Md5Context, Key, MD5_HASH_SIZE);
        CyMd5AddContent(&Md5Context, Password, RtlStringLength(Password));
        CyMd5AddContent(&Md5Context, InitializationVector, CY_PEM_SALT_SIZE);
        CyMd5GetHash(&Md5Context, &(Key[MD5_HASH_SIZE]));
    }

    //
    // Perform the decryption.
    //

    CyAesInitialize(&AesContext, CipherMode, Key, InitializationVector);
    CyAesConvertKeyForDecryption(&AesContext);
    CyAesCbcDecrypt(&AesContext, Data, Data, *DataSize);
    return STATUS_SUCCESS;
}

