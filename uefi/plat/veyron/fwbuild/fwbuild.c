/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "uefifw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define VERIFIED_BOOT_PREAMBLE_HEADER_VERSION_MAJOR 2
#define VERIFIED_BOOT_PREAMBLE_HEADER_VERSION_MINOR 0
#define VERIFIED_BOOT_PREAMBLE_IMAGE_VERSION 1

#define VERIFIED_BOOT_IMAGE_ALIGNMENT 0x10000

//
// Define the set of commands and temporary files used to sign data.
//

#define SHA256_DATA_FILE "data.tmp"
#define SHA256_DIGEST_FILE "data.dgst"
#define SHA256_SIGNATURE_FILE "data.sig"

#define SHA256_DIGEST_COMMAND \
    "openssl dgst -sha256 -binary " SHA256_DATA_FILE " >> " SHA256_DIGEST_FILE

#define SHA256_SIGN_COMMAND_FORMAT \
    "openssl rsautl -sign -inkey %s -keyform PEM -in " \
    SHA256_DIGEST_FILE " > " SHA256_SIGNATURE_FILE

#define SHA256_DIGEST_HEADER_LENGTH 19

#define SHA256_SIGNATURE_SIZE 0x100

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

typedef struct _VERIFIED_BOOT_SIGNATURE {
    UINT64 SignatureOffset;
    UINT64 SignatureSize;
    UINT64 DataSize;
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
    UINT64 PreambleSize;
    VERIFIED_BOOT_SIGNATURE PreambleSignature;
    UINT32 HeaderVersionMajor;
    UINT32 HeaderVersionMinor;
    UINT64 ImageVersion;
    UINT64 ImageLoadAddress;
    UINT64 BootLoaderAddress;
    UINT64 BootLoaderSize;
    VERIFIED_BOOT_SIGNATURE ImageSignature;
} PACKED VERIFIED_BOOT_PREAMBLE_HEADER, *PVERIFIED_BOOT_PREAMBLE_HEADER;

//
// ----------------------------------------------- Internal Function Prototypes
//

int
SignData (
    void *Data,
    UINT32 DataSize,
    void **Signature,
    UINT32 *SignatureSize,
    CHAR8 *PrivateKeyFilePath
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the 19 byte header that must be prepended to all SHA256 digests before
// signing.
//

unsigned char Sha256DigestHeader[SHA256_DIGEST_HEADER_LENGTH] = {
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
    CHAR8 **Arguments
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

    CHAR8 *AfterScan;
    void *Buffer;
    size_t BytesDone;
    FILE *FirmwareImage;
    void *FirmwareImageBuffer;
    CHAR8 *FirmwareImagePath;
    void *FirmwareImageSignature;
    UINT32 FirmwareImageSignatureSize;
    UINT64 FirmwareImageSize;
    void *ImageSignature;
    FILE *KeyBlockFile;
    CHAR8 *KeyBlockFilePath;
    UINT32 KeyBlockSize;
    UINT32 LoadAddress;
    FILE *OutputImage;
    CHAR8 *OutputImagePath;
    PVERIFIED_BOOT_PREAMBLE_HEADER PreambleHeader;
    void *PreambleSignature;
    void *PreambleSignatureBuffer;
    UINT32 PreambleSignatureBufferSize;
    UINT64 PreambleSize;
    CHAR8 *PrivateKeyFilePath;
    int Result;
    UINT64 TotalSize;

    Result = 0;
    Buffer = NULL;
    FirmwareImage = NULL;
    FirmwareImageBuffer = NULL;
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
    KeyBlockSize = (UINT32)ftell(KeyBlockFile);
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
    FirmwareImageSize = (UINT32)ftell(FirmwareImage);
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
                   (SHA256_SIGNATURE_SIZE * 2);

    TotalSize = KeyBlockSize + PreambleSize;
    TotalSize = ALIGN_VALUE(TotalSize, VERIFIED_BOOT_IMAGE_ALIGNMENT);
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
    ImageSignature = (CHAR8 *)(PreambleHeader + 1);
    PreambleSignature = (CHAR8 *)(ImageSignature + SHA256_SIGNATURE_SIZE);
    PreambleHeader->PreambleSize = PreambleSize;
    PreambleHeader->HeaderVersionMajor =
                                   VERIFIED_BOOT_PREAMBLE_HEADER_VERSION_MAJOR;

    PreambleHeader->HeaderVersionMajor =
                                   VERIFIED_BOOT_PREAMBLE_HEADER_VERSION_MAJOR;

    PreambleHeader->ImageVersion = VERIFIED_BOOT_PREAMBLE_IMAGE_VERSION;
    PreambleHeader->ImageLoadAddress = LoadAddress;
    PreambleHeader->PreambleSignature.SignatureOffset =
              PreambleSignature - (void *)&(PreambleHeader->PreambleSignature);

    PreambleHeader->PreambleSignature.SignatureSize = SHA256_SIGNATURE_SIZE;
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
    void *Data,
    UINT32 DataSize,
    void **Signature,
    UINT32 *SignatureSize,
    CHAR8 *PrivateKeyFilePath
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
    CHAR8 *CommandBuffer;
    UINT32 CommandBufferSize;
    FILE *DataFile;
    FILE *DigestFile;
    int Result;
    void *SignatureBuffer;
    UINT32 SignatureBufferSize;
    FILE *SignatureFile;
    int Status;

    CommandBuffer = NULL;
    DataFile = NULL;
    DigestFile = NULL;
    SignatureBuffer = NULL;
    SignatureBufferSize = 0;
    SignatureFile = NULL;

    //
    // Take the data and write it to the temporary data file.
    //

    DataFile = fopen(SHA256_DATA_FILE, "wb+");
    if (DataFile == NULL) {
        fprintf(stderr, "Error: Failed to open %s.\n", SHA256_DATA_FILE);
        Result = errno;
        goto SignDataEnd;
    }

    BytesDone = fwrite(Data, 1, DataSize, DataFile);
    if (BytesDone != DataSize) {
        Result = EIO;
        goto SignDataEnd;
    }

    fclose(DataFile);
    DataFile = NULL;

    //
    // Open the digest file and write the header to it.
    //

    DigestFile = fopen(SHA256_DIGEST_FILE, "wb+");
    if (DigestFile == NULL) {
        fprintf(stderr, "Error: Failed to open %s.\n", SHA256_DIGEST_FILE);
        Result = errno;
        goto SignDataEnd;
    }

    BytesDone = fwrite(&(Sha256DigestHeader[0]),
                       1,
                       SHA256_DIGEST_HEADER_LENGTH,
                       DigestFile);

    if (BytesDone != SHA256_DIGEST_HEADER_LENGTH) {
        Result = EIO;
        goto SignDataEnd;
    }

    fclose(DigestFile);
    DigestFile = NULL;

    //
    // Append the data's digest to the digest file.
    //

    Status = system(SHA256_DIGEST_COMMAND);
    if (Status != 0) {
        Result = EAGAIN;
        goto SignDataEnd;
    }

    //
    // Create the command to sign the digest using the private key file.
    //

    CommandBufferSize = strlen(PrivateKeyFilePath) +
                        sizeof(SHA256_SIGN_COMMAND_FORMAT);

    CommandBuffer = malloc(CommandBufferSize);
    if (CommandBuffer == NULL) {
        Result = ENOMEM;
        goto SignDataEnd;
    }

    Result = snprintf(CommandBuffer,
                      CommandBufferSize,
                      SHA256_SIGN_COMMAND_FORMAT,
                      PrivateKeyFilePath);

    if (Result < 0) {
        Result = EINVAL;
        goto SignDataEnd;
    }

    //
    // Sign the digest file.
    //

    Status = system(CommandBuffer);
    if (Status != 0) {
        Result = EAGAIN;
        goto SignDataEnd;
    }

    //
    // Allocate a buffer and read the signature file into the buffer.
    //

    SignatureFile = fopen(SHA256_SIGNATURE_FILE, "rb");
    if (SignatureFile == NULL) {
        fprintf(stderr, "Error: Failed to open %s.\n", SHA256_SIGNATURE_FILE);
        Result = errno;
        goto SignDataEnd;
    }

    fseek(SignatureFile, 0, SEEK_END);
    SignatureBufferSize = (UINT32)ftell(SignatureFile);
    fseek(SignatureFile, 0, SEEK_SET);

    assert(SignatureBufferSize == SHA256_SIGNATURE_SIZE);

    SignatureBuffer = malloc(SignatureBufferSize);
    if (SignatureBuffer == NULL) {
        Result = ENOMEM;
        goto SignDataEnd;
    }

    BytesDone = fread(SignatureBuffer, 1, SignatureBufferSize, SignatureFile);
    if (BytesDone != SignatureBufferSize) {
        Result = EIO;
        goto SignDataEnd;
    }

    Result = 0;

SignDataEnd:
    if (DataFile != NULL) {
        fclose(DataFile);
    }

    if (DigestFile != NULL) {
        fclose(DigestFile);
    }

    if (SignatureFile != NULL) {
        fclose(SignatureFile);
    }

    if (CommandBuffer != NULL) {
        free(CommandBuffer);
    }

    if (Result != 0) {
        if (SignatureBuffer != NULL) {
            free(SignatureBuffer);
            SignatureBuffer = NULL;
            SignatureBufferSize = 0;
        }
    }

    unlink(SHA256_DATA_FILE);
    unlink(SHA256_DIGEST_FILE);
    unlink(SHA256_SIGNATURE_FILE);
    *Signature = SignatureBuffer;
    *SignatureSize = SignatureBufferSize;
    return Result;
}

