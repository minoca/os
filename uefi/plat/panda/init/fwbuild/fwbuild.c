/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fwbuild.c

Abstract:

    This module implements a small build utility that adds the header needed
    to make a first stage loader bootable on TI OMAP4 platforms.

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
    0x40, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x43, 0x48, 0x53, 0x45,
    0x54, 0x54, 0x49, 0x4E, 0x47, 0x53, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xC1, 0xC0, 0xC0, 0xC0, 0x00, 0x01, 0x00, 0x00,
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
    FILE *Input;
    FILE *Output;
    size_t Read;
    int Result;
    int Size;
    struct stat Stat;
    size_t Written;

    Buffer = NULL;
    Input = NULL;
    Output = NULL;
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

    Output = fopen(Arguments[3], "wb");
    if (Output == NULL) {
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
    //

    Result = fseek(Output, TI_MLO_OFFSET, SEEK_SET);
    if (Result < 0) {
        goto mainEnd;
    }

    //
    // Write the header.
    //

    Result = -1;
    Written = fwrite(TiTocHeader, 1, sizeof(TiTocHeader), Output);
    if (Written != sizeof(TiTocHeader)) {
        goto mainEnd;
    }

    //
    // Write the size of the image.
    //

    Size = Stat.st_size;
    Written = fwrite(&Size, 1, sizeof(Size), Output);
    if (Written != sizeof(Size)) {
        goto mainEnd;
    }

    //
    // Write the destination address.
    //

    Written = fwrite(&Address, 1, sizeof(Address), Output);
    if (Written != sizeof(Address)) {
        goto mainEnd;
    }

    Input = fopen(Arguments[2], "rb");
    if (Input == NULL) {
        fprintf(stderr,
                "Error opening file: %s: %s\n",
                Arguments[2],
                strerror(errno));

        goto mainEnd;
    }

    Buffer = malloc(Size);
    if (Buffer == NULL) {
        errno = ENOMEM;
        goto mainEnd;
    }

    Read = fread(Buffer, 1, Size, Input);
    if (Read != Size) {
        goto mainEnd;
    }

    Written = fwrite(Buffer, 1, Size, Output);
    if (Written!= Size) {
        goto mainEnd;
    }

    Result = 0;

mainEnd:
    if (Input != NULL) {
        fclose(Input);
    }

    if (Output != NULL) {
        fclose(Output);
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

