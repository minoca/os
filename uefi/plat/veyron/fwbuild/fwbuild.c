/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fwbuild.c

Abstract:

    This module implements a small build utility that adds the keyblock and
    preamble to a firmware image in order to boot on the RK3288 Veyron SoC.

Author:

    Chris Stevens 7-Jul-2015

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#define CRYPTO_API __DLLEXPORT

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/crypto.h>

//
// ---------------------------------------------------------------- Definitions
//

#define VERIFIED_BOOT_PREAMBLE_HEADER_VERSION_MAJOR 2
#define VERIFIED_BOOT_PREAMBLE_HEADER_VERSION_MINOR 0
#define VERIFIED_BOOT_PREAMBLE_IMAGE_VERSION 1

#define VERIFIED_BOOT_IMAGE_ALIGNMENT 0x10000

#define VERIFIED_BOOT_MAX_SIGNATURE_SIZE 256

#define VERIFIED_BOOT_SHA_HEADER_LENGTH 19

//
// The signature is large enough for a 2048-bit RSA key.
//

#define VERIFIED_BOOT_SIGNATURE_SIZE 0x100

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a verified boot signature as it is stored in the
    verified boot preamble header.

Members:

    SignatureOffset - Stores the offset to the signature from the beginning of
        this signature structure.

    SignatureSize - Stores the size of the signature, in bytes.

    DataSize - Stores the size of the signed data, in bytes.

--*/

#pragma pack(push, 1)

typedef struct _VERIFIED_BOOT_SIGNATURE {
    ULONGLONG SignatureOffset;
    ULONGLONG SignatureSize;
    ULONGLONG DataSize;
} PACKED VERIFIED_BOOT_SIGNATURE, *PVERIFIED_BOOT_SIGNATURE;

/*++

Structure Description:

    This structure defines the verified boot preamble header that is to be
    appended to the key block.

Members:

    PreambleSize - Stores the size of the preamble, in bytes.

    PreambleSignature - Stores the signature of the preamble header, including
        the image signature.

    HeaderVersionMajor - Stores the header's major version number.

    HeaderVersionMinor - Stores the header's minor version number.

    ImageVersion - Stores the image's version.

    ImageLoadAddress - Stores the load address of the image.

    BootLoaderAddress - Stores the boot loader's address.

    BootLoaderSize - Stores the size of the boot loader, in bytes.

    ImageSignature - Stores the signature of the image that is appended to the
        preamble.

--*/

typedef struct _VERIFIED_BOOT_PREAMBLE_HEADER {
    ULONGLONG PreambleSize;
    VERIFIED_BOOT_SIGNATURE PreambleSignature;
    ULONG HeaderVersionMajor;
    ULONG HeaderVersionMinor;
    ULONGLONG ImageVersion;
    ULONGLONG ImageLoadAddress;
    ULONGLONG BootLoaderAddress;
    ULONGLONG BootLoaderSize;
    VERIFIED_BOOT_SIGNATURE ImageSignature;
} PACKED VERIFIED_BOOT_PREAMBLE_HEADER, *PVERIFIED_BOOT_PREAMBLE_HEADER;

#pragma pack(pop)

//
// ----------------------------------------------- Internal Function Prototypes
//

int
SignData (
    void *Data,
    ULONG DataSize,
    PVOID *Signature,
    PULONG SignatureSize,
    PSTR PrivateKeyFilePath
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the 19 byte header that must be prepended to all SHA256 digests before
// signing.
//

UCHAR VerifiedBootShaHeader[VERIFIED_BOOT_SHA_HEADER_LENGTH] = {
    0x30, 0x31, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
    0x00, 0x04, 0x20,
};

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the build utility that adds the keyblock and
    preamble to the firmware image.

Arguments:

    ArgumentCount - Supplies the number of arguments specified on the command
        line.

    Arguments - Supplies an array of strings containing the command line
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    PVOID Buffer;
    size_t BytesDone;
    FILE *FirmwareImage;
    PVOID FirmwareImageBuffer;
    PSTR FirmwareImagePath;
    PVOID FirmwareImageSignature;
    ULONG FirmwareImageSignatureSize;
    ULONGLONG FirmwareImageSize;
    PVOID ImageSignature;
    FILE *KeyBlockFile;
    PSTR KeyBlockFilePath;
    ULONG KeyBlockSize;
    ULONG LoadAddress;
    FILE *OutputImage;
    PSTR OutputImagePath;
    PVERIFIED_BOOT_PREAMBLE_HEADER PreambleHeader;
    PVOID PreambleSignature;
    PVOID PreambleSignatureBuffer;
    ULONG PreambleSignatureBufferSize;
    ULONGLONG PreambleSize;
    PSTR PrivateKeyFilePath;
    int Result;
    ULONGLONG TotalSize;

    Result = 0;
    Buffer = NULL;
    FirmwareImage = NULL;
    FirmwareImageBuffer = NULL;
    FirmwareImageSignature = NULL;
    OutputImage = NULL;
    PreambleSignatureBuffer = NULL;
    KeyBlockFile = NULL;
    if (ArgumentCount != 6) {
        printf("Error: usage: %s <LoadAddress> <KeyBlockFile> "
               "<PrivateKeyFile> <FirmwareImage> <OutputImage>\n",
               Arguments[0]);

        Result = EINVAL;
        goto mainEnd;
    }

    //
    // Get the load address.
    //

    LoadAddress = strtoul(Arguments[1], &AfterScan, 16);
    if (AfterScan == Arguments[1]) {
        fprintf(stderr, "Error: Invalid load address %s.\n", Arguments[1]);
        Result = EINVAL;
        goto mainEnd;
    }

    KeyBlockFilePath = Arguments[2];
    PrivateKeyFilePath = Arguments[3];
    FirmwareImagePath = Arguments[4];
    OutputImagePath = Arguments[5];

    //
    // Open the keyblock, firmware image, and output files.
    //

    KeyBlockFile = fopen(KeyBlockFilePath, "rb");
    if (KeyBlockFile == NULL) {
        fprintf(stderr, "Error: Failed to open %s.\n", KeyBlockFilePath);
        Result = errno;
        goto mainEnd;
    }

    FirmwareImage = fopen(FirmwareImagePath, "rb");
    if (FirmwareImage == NULL) {
        fprintf(stderr, "Error: Failed to open %s.\n", FirmwareImagePath);
        Result = errno;
        goto mainEnd;
    }

    OutputImage = fopen(OutputImagePath, "wb+");
    if (OutputImage == NULL) {
        fprintf(stderr, "Error: Failed to open %s.\n", OutputImagePath);
        Result = errno;
        goto mainEnd;
    }

    //
    // Write the keyblock to the output file.
    //

    fseek(KeyBlockFile, 0, SEEK_END);
    KeyBlockSize = ftell(KeyBlockFile);
    fseek(KeyBlockFile, 0, SEEK_SET);
    Buffer = malloc(KeyBlockSize);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto mainEnd;
    }

    BytesDone = fread(Buffer, 1, KeyBlockSize, KeyBlockFile);
    if (BytesDone != KeyBlockSize) {
        Result = EIO;
        goto mainEnd;
    }

    BytesDone = fwrite(Buffer, 1, KeyBlockSize, OutputImage);
    if (BytesDone != KeyBlockSize) {
        Result = EIO;
        goto mainEnd;
    }

    free(Buffer);
    Buffer = NULL;

    //
    // Get the firmware image size and read in the firmware image data.
    //

    fseek(FirmwareImage, 0, SEEK_END);
    FirmwareImageSize = ftell(FirmwareImage);
    fseek(FirmwareImage, 0, SEEK_SET);
    FirmwareImageBuffer = malloc(FirmwareImageSize);
    if (FirmwareImageBuffer == NULL) {
        Result = ENOMEM;
        goto mainEnd;
    }

    BytesDone = fread(FirmwareImageBuffer, 1, FirmwareImageSize, FirmwareImage);
    if (BytesDone != FirmwareImageSize) {
        Result = EIO;
        goto mainEnd;
    }

    //
    // Sign the firmware image.
    //

    Result = SignData(FirmwareImageBuffer,
                      FirmwareImageSize,
                      &FirmwareImageSignature,
                      &FirmwareImageSignatureSize,
                      PrivateKeyFilePath);

    if (Result != 0) {
        fprintf(stderr, "Error: Failed to sign %s.\n", FirmwareImagePath);
        goto mainEnd;
    }

    //
    // Determine the preamble size. The firmware image is appended to the
    // output file at the end of the preamble and must be aligned on a 64K
    // boundary.
    //

    PreambleSize = sizeof(VERIFIED_BOOT_PREAMBLE_HEADER) +
                   (VERIFIED_BOOT_SIGNATURE_SIZE * 2);

    TotalSize = KeyBlockSize + PreambleSize;
    TotalSize = ALIGN_RANGE_UP(TotalSize, VERIFIED_BOOT_IMAGE_ALIGNMENT);
    PreambleSize = TotalSize - KeyBlockSize;

    //
    // Initialize the preamble header and add the firmare image's signature to
    // the end.
    //

    Buffer = malloc(PreambleSize);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto mainEnd;
    }

    memset(Buffer, 0, PreambleSize);
    PreambleHeader = (PVERIFIED_BOOT_PREAMBLE_HEADER)Buffer;
    ImageSignature = (PSTR)(PreambleHeader + 1);
    PreambleSignature = (PSTR)(ImageSignature + VERIFIED_BOOT_SIGNATURE_SIZE);
    PreambleHeader->PreambleSize = PreambleSize;
    PreambleHeader->HeaderVersionMajor =
                                   VERIFIED_BOOT_PREAMBLE_HEADER_VERSION_MAJOR;

    PreambleHeader->HeaderVersionMinor =
                                   VERIFIED_BOOT_PREAMBLE_HEADER_VERSION_MINOR;

    PreambleHeader->ImageVersion = VERIFIED_BOOT_PREAMBLE_IMAGE_VERSION;
    PreambleHeader->ImageLoadAddress = LoadAddress;
    PreambleHeader->PreambleSignature.SignatureOffset =
              PreambleSignature - (void *)&(PreambleHeader->PreambleSignature);

    PreambleHeader->PreambleSignature.SignatureSize =
                                                  VERIFIED_BOOT_SIGNATURE_SIZE;

    PreambleHeader->PreambleSignature.DataSize =
                                        sizeof(VERIFIED_BOOT_PREAMBLE_HEADER) +
                                        FirmwareImageSignatureSize;

    PreambleHeader->ImageSignature.SignatureOffset =
                    ImageSignature - (void *)&(PreambleHeader->ImageSignature);

    PreambleHeader->ImageSignature.SignatureSize = FirmwareImageSignatureSize;
    PreambleHeader->ImageSignature.DataSize = FirmwareImageSize;

    //
    // Write the firmware image signature into the preamble.
    //

    memcpy(ImageSignature, FirmwareImageSignature, FirmwareImageSignatureSize);

    //
    // Sign the preamble header and firmware image signature.
    //

    Result = SignData(PreambleHeader,
                      PreambleHeader->PreambleSignature.DataSize,
                      &PreambleSignatureBuffer,
                      &PreambleSignatureBufferSize,
                      PrivateKeyFilePath);

    if (Result != 0) {
        fprintf(stderr, "Error: Failed to sign preamble header.\n");
        goto mainEnd;
    }

    assert(PreambleSignatureBufferSize ==
           PreambleHeader->PreambleSignature.SignatureSize);

    //
    // Write the preamble signature to the end of the preamble.
    //

    memcpy(PreambleSignature,
           PreambleSignatureBuffer,
           PreambleSignatureBufferSize);

    //
    // Append the preamble to the output file.
    //

    BytesDone = fwrite(Buffer, 1, PreambleSize, OutputImage);
    if (BytesDone != PreambleSize) {
        Result = EIO;
        goto mainEnd;
    }

    free(Buffer);
    Buffer = NULL;

    //
    // Append the firwmare image to the output file.
    //

    BytesDone = fwrite(FirmwareImageBuffer, 1, FirmwareImageSize, OutputImage);
    if (BytesDone != FirmwareImageSize) {
        Result = EIO;
        goto mainEnd;
    }

mainEnd:
    if (FirmwareImage != NULL) {
        fclose(FirmwareImage);
    }

    if (KeyBlockFile != NULL) {
        fclose(KeyBlockFile);
    }

    if (FirmwareImageBuffer != NULL) {
        free(FirmwareImageBuffer);
    }

    if (FirmwareImageSignature != NULL) {
        free(FirmwareImageSignature);
    }

    if (PreambleSignatureBuffer != NULL) {
        free(PreambleSignatureBuffer);
    }

    if (OutputImage != NULL) {
        fclose(OutputImage);
    }

    if (Buffer != NULL) {
        free(Buffer);
    }

    if (Result != 0) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

int
SignData (
    PVOID Data,
    ULONG DataSize,
    PVOID *Signature,
    PULONG SignatureSize,
    PSTR PrivateKeyFilePath
    )

/*++

Routine Description:

    This routine signs a data buffer and returns the signature in a buffer that
    the caller is expected to free.

Arguments:

    Data - Supplies a pointer to the data to sign.

    DataSize - Supplies the size of the data to sign.

    Signature - Supplies a pointer that receives the data's signature. The
        caller is expected to free this buffer.

    SignatureSize - Supplies a pointer that receives the size of the data's
        signature.

    PrivateKeyFilePath - Supplies the path to the private key file to use for
        the signing.

Return Value:

    0 on success. Non-zero error value on failure.

--*/

{

    size_t BytesDone;
    PUCHAR HashBuffer;
    FILE *KeyFile;
    PUCHAR KeyFileBuffer;
    ULONG KeyFileSize;
    KSTATUS KStatus;
    int Result;
    RSA_CONTEXT RsaContext;
    SHA256_CONTEXT ShaContext;
    PVOID SignatureData;
    INTN SignatureDataSize;
    struct stat Stat;
    int Status;

    KeyFile = NULL;
    KeyFileBuffer = NULL;
    HashBuffer = NULL;
    SignatureData = NULL;
    SignatureDataSize = 0;
    memset(&RsaContext, 0, sizeof(RSA_CONTEXT));
    RsaContext.BigIntegerContext.AllocateMemory = (PCY_ALLOCATE_MEMORY)malloc;
    RsaContext.BigIntegerContext.ReallocateMemory =
                                               (PCY_REALLOCATE_MEMORY)realloc;

    RsaContext.BigIntegerContext.FreeMemory = free;
    KStatus = CyRsaInitializeContext(&RsaContext);
    if (!KSUCCESS(KStatus)) {
        Result = EINVAL;
        goto SignDataEnd;
    }

    //
    // Allocate space for the header and the SHA256 hash.
    //

    HashBuffer = malloc(VERIFIED_BOOT_SHA_HEADER_LENGTH + SHA256_HASH_SIZE);
    if (HashBuffer == NULL) {
        Result = ENOMEM;
        goto SignDataEnd;
    }

    //
    // Copy in the fixed header.
    //

    memcpy(HashBuffer, VerifiedBootShaHeader, VERIFIED_BOOT_SHA_HEADER_LENGTH);

    //
    // Create a SHA-256 hash of the data.
    //

    CySha256Initialize(&ShaContext);
    CySha256AddContent(&ShaContext, Data, DataSize);
    CySha256GetHash(&ShaContext,
                    &(HashBuffer[VERIFIED_BOOT_SHA_HEADER_LENGTH]));

    //
    // Read in the private key in PEM format. It's assumed there's no password
    // on it.
    //

    KeyFile = fopen(PrivateKeyFilePath, "rb");
    if (KeyFile == NULL) {
        Result = errno;
        fprintf(stderr,
                "Cannot open private key file %s.\n",
                PrivateKeyFilePath);

        goto SignDataEnd;
    }

    Status = stat(PrivateKeyFilePath, &Stat);
    if (Status != 0) {
        Result = errno;
        fprintf(stderr, "Cannot stat %s.\n", PrivateKeyFilePath);
        goto SignDataEnd;
    }

    KeyFileSize = Stat.st_size;
    KeyFileBuffer = malloc(KeyFileSize + 1);
    if (KeyFileBuffer == NULL) {
        Result = ENOMEM;
        goto SignDataEnd;
    }

    BytesDone = fread(KeyFileBuffer, 1, KeyFileSize, KeyFile);
    if (BytesDone != KeyFileSize) {
        Result = EIO;
        goto SignDataEnd;
    }

    KeyFileBuffer[KeyFileSize] = '\0';

    //
    // Load the private key into the RSA context.
    //

    KStatus = CyRsaAddPemFile(&RsaContext, KeyFileBuffer, KeyFileSize, NULL);
    if (!KSUCCESS(KStatus)) {
        fprintf(stderr,
                "Failed to load PEM: %s: %d\n",
                PrivateKeyFilePath,
                KStatus);

        Result = EINVAL;
        goto SignDataEnd;
    }

    if (RsaContext.ModulusSize > VERIFIED_BOOT_MAX_SIGNATURE_SIZE) {
        Result = EINVAL;
        goto SignDataEnd;
    }

    SignatureData = malloc(VERIFIED_BOOT_MAX_SIGNATURE_SIZE);
    if (SignatureData == NULL) {
        Result = ENOMEM;
        goto SignDataEnd;
    }

    //
    // Sign the header + hash.
    //

    SignatureDataSize = CyRsaEncrypt(
                            &RsaContext,
                            HashBuffer,
                            VERIFIED_BOOT_SHA_HEADER_LENGTH + SHA256_HASH_SIZE,
                            SignatureData,
                            TRUE);

    if (SignatureDataSize == -1) {
        Result = ENOMEM;
        fprintf(stderr, "Failed to sign data: %d\n", KStatus);
        goto SignDataEnd;
    }

    Result = 0;

SignDataEnd:
    if (Result != 0) {
        if (SignatureData != NULL) {
            free(SignatureData);
            SignatureData = NULL;
            SignatureDataSize = 0;
        }
    }

    CyRsaDestroyContext(&RsaContext);
    if (KeyFile != NULL) {
        fclose(KeyFile);
    }

    if (KeyFileBuffer != NULL) {
        free(KeyFileBuffer);
    }

    if (HashBuffer != NULL) {
        free(HashBuffer);
    }

    *Signature = SignatureData;
    *SignatureSize = SignatureDataSize;
    return Result;
}

