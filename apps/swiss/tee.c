/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tee.c

Abstract:

    This module implements the tee utility.

Author:

    Evan Green 1-Apr-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TEE_VERSION_MAJOR 1
#define TEE_VERSION_MINOR 0

#define TEE_USAGE                                                              \
    "usage: tee [options] [files]\n\n"                                         \
    "The tee utility copies standard input to standard output, and also \n"    \
    "writes to the given files. Options are:\n"                                \
    "    -a, --append -- Open the output files with O_APPEND.\n"               \
    "    -i, --ignore-interrupts -- Ignore interrupt signals.\n"               \
    "    --help -- Display this help text.\n"                                  \
    "    --version -- Display version information and exit.\n\n"

#define TEE_OPTIONS_STRING "aihV"

//
// Define the block size used in data transfers.
//

#define TEE_BUFFER_SIZE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option TeeLongOptions[] = {
    {"append", no_argument, 0, 'a'},
    {"ignore-interrupts", no_argument, 0, 'i'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
TeeMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the tee program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    PVOID Buffer;
    ssize_t BytesRead;
    ssize_t BytesWritten;
    int Descriptor;
    UINTN DescriptorCount;
    UINTN DescriptorIndex;
    int *Descriptors;
    INT OpenFlags;
    INT Option;
    void *OriginalAction;
    PSTR Path;
    BOOL RestoreSignal;
    INT Status;
    ssize_t TotalBytesWritten;
    INT TotalStatus;

    Buffer = NULL;
    DescriptorCount = 0;
    Descriptors = NULL;
    OpenFlags = O_CREAT | O_WRONLY | O_TRUNC;;
    RestoreSignal = FALSE;
    TotalStatus = 0;

    //
    // Count the arguments and get the basic ones out of the way.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             TEE_OPTIONS_STRING,
                             TeeLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            return 1;
        }

        switch (Option) {
        case 'a':
            OpenFlags |= O_APPEND;
            OpenFlags &= ~O_TRUNC;
            break;

        case 'i':
            if (RestoreSignal == FALSE) {
                OriginalAction = signal(SIGINT, SIG_IGN);
                RestoreSignal = TRUE;
            }

            break;

        case 'V':
            SwPrintVersion(TEE_VERSION_MAJOR, TEE_VERSION_MINOR);
            return 1;

        case 'h':
            printf(TEE_USAGE);
            return 1;

        default:

            assert(FALSE);

            return 1;
        }
    }

    //
    // Allocate the buffer.
    //

    Buffer = malloc(TEE_BUFFER_SIZE);
    if (Buffer == NULL) {
        Status = 1;
        goto mainEnd;
    }

    //
    // Allocate the array of descriptors.
    //

    ArgumentIndex = optind;
    DescriptorCount = ArgumentCount - ArgumentIndex + 1;
    Descriptors = malloc(sizeof(int) * DescriptorCount);
    if (Descriptors == NULL) {
        Status = 1;
        goto mainEnd;
    }

    memset(Descriptors, -1, sizeof(int) * DescriptorCount);

    //
    // Open all the files.
    //

    Descriptors[0] = STDOUT_FILENO;
    for (DescriptorIndex = 1;
         DescriptorIndex < DescriptorCount;
         DescriptorIndex += 1) {

        Path = Arguments[ArgumentIndex + DescriptorIndex - 1];
        Descriptors[DescriptorIndex] = SwOpen(Path, OpenFlags, 0777);
        if (Descriptors[DescriptorIndex] < 0) {
            SwPrintError(errno, Path, "Cannot open");
            TotalStatus = 1;
        }
    }

    //
    // Loop reading input and writing output.
    //

    Status = 0;
    while (TRUE) {
        do {
            BytesRead = read(STDIN_FILENO, Buffer, TEE_BUFFER_SIZE);

        } while ((BytesRead <= 0) && (errno == EINTR));

        if (BytesRead < 0) {
            SwPrintError(errno, NULL, "Cannot read standard in");
            TotalStatus = 1;
            break;

        } else if (BytesRead == 0) {
            break;
        }

        //
        // Write to all output descriptors.
        //

        for (DescriptorIndex = 0;
             DescriptorIndex < DescriptorCount;
             DescriptorIndex += 1) {

            Descriptor = Descriptors[DescriptorIndex];
            if (Descriptor < 0) {
                continue;
            }

            TotalBytesWritten = 0;

            //
            // Loop until all bytes are written to this file descriptor.
            //

            while (TotalBytesWritten < BytesRead) {
                do {
                    BytesWritten = write(Descriptor,
                                         Buffer + TotalBytesWritten,
                                         BytesRead - TotalBytesWritten);

                } while ((BytesWritten <= 0) && (errno == EINTR));

                if (BytesWritten <= 0) {
                    if (DescriptorIndex == 0) {
                        Path = "(stdout)";

                    } else {
                        Path = Arguments[ArgumentIndex + DescriptorIndex - 1];
                    }

                    SwPrintError(errno, Path, "Cannot write to");
                    TotalStatus = 1;
                    break;
                }

                TotalBytesWritten += BytesWritten;
            }
        }
    }

mainEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    if (Descriptors != NULL) {
        for (DescriptorIndex = 1;
             DescriptorIndex < DescriptorCount;
             DescriptorIndex += 1) {

            Descriptor = Descriptors[DescriptorIndex];
            if (Descriptor >= 0) {

                assert(Descriptor != STDOUT_FILENO);

                close(Descriptor);
            }
        }

        free(Descriptors);
    }

    if (RestoreSignal != FALSE) {
        signal(SIGINT, OriginalAction);
    }

    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

