/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    fwbuild.c

Abstract:

    This module implements a small build utility that adds the header needed
    to make a first stage loader bootable on TI AM335x platforms.

Author:

    Evan Green 1-Apr-2014

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define TI_MLO_OFFSET 0x20000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

unsigned char TiTocHeader[512] = {
    0xA0, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x43, 0x48, 0x53, 0x45, /* 0x10 */
    0x54, 0x54, 0x49, 0x4E, 0x47, 0x53, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* 0x20 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* 0x30 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x40 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x50 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x70 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x80 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x90 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xC1, 0xC0, 0xC0, 0xC0, 0x00, 0x01, 0x00, 0x00, /* 0xA0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
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

    This routine implements the build utility that adds a boot header to a
    firmware image.

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

    int Address;
    char *AfterScan;
    void *Buffer;
    ssize_t BytesDone;
    int Input;
    int Output;
    int Result;
    int Size;
    struct stat Stat;

    Buffer = NULL;
    Input = -1;
    Output = -1;
    if (ArgumentCount != 4) {
        printf("Error: usage: <RAMAddress> <InputFile> <OutputFile>\n");
        Result = EINVAL;
        goto mainEnd;
    }

    Address = strtoul(Arguments[1], &AfterScan, 16);
    if (AfterScan == Arguments[1]) {
        fprintf(stderr, "Error: Invalid RAM Address %s\n", Arguments[1]);
        Result = EINVAL;
        goto mainEnd;
    }

    Result = stat(Arguments[2], &Stat);
    if (Result != 0) {
        fprintf(stderr,
                "Error opening file: %s: %s\n",
                Arguments[2],
                strerror(errno));

        goto mainEnd;
    }

    //
    // Open the destination.
    //

    Output = open(Arguments[3],
                  O_WRONLY | O_BINARY | O_TRUNC | O_CREAT,
                  S_IRUSR | S_IWUSR);

    if (Output < 0) {
        fprintf(stderr,
                "Error opening file: %s: %s\n",
                Arguments[3],
                strerror(errno));

        Result = -1;
        goto mainEnd;
    }

    //
    // Seek to the offset the ROM code searches. The ROM code actually searches
    // a few locations: 0x0, 0x20000 (128KB), 0x40000 (256KB), and 0x60000
    // 384KB. Pick the first one that's not zero.
    // Note: To make a binary that can be downloaded over the UART, simply
    // comment out the following seek and write (so the header is just the
    // address and size).
    //

    Result = lseek(Output, TI_MLO_OFFSET, SEEK_SET);
    if (Result < 0) {
        goto mainEnd;
    }

    //
    // Write the header.
    //

    Result = write(Output, TiTocHeader, sizeof(TiTocHeader));
    if (Result < 0) {
        goto mainEnd;
    }

    //
    // Write the size of the image.
    //

    Size = Stat.st_size;
    Result = write(Output, &Size, sizeof(Size));
    if (Result < 0) {
        goto mainEnd;
    }

    //
    // Write the destination address.
    //

    Result = write(Output, &Address, sizeof(Address));
    if (Result < 0) {
        goto mainEnd;
    }

    Result = -1;
    Input = open(Arguments[2], O_RDONLY | O_BINARY);
    if (Input < 0) {
        fprintf(stderr,
                "Error opening file: %s: %s\n",
                Arguments[2],
                strerror(errno));

        goto mainEnd;
    }

    Buffer = malloc(Size);
    if (Buffer == NULL) {
        goto mainEnd;
    }

    BytesDone = read(Input, Buffer, Size);
    if (BytesDone != Size) {
        goto mainEnd;
    }

    BytesDone = write(Output, Buffer, Size);
    if (BytesDone != Size) {
        goto mainEnd;
    }

    Result = 0;

mainEnd:
    if (Input >= 0) {
        close(Input);
    }

    if (Output >= 0) {
        close(Output);
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

