/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sum.c

Abstract:

    This module implements the sum utility, which implements primitive
    checksumming of files.

Author:

    Evan Green 12-May-2015

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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SUM_VERSION_MAJOR 1
#define SUM_VERSION_MINOR 0

#define SUM_USAGE                                                              \
    "usage: sum [options] [files...]\n"                                        \
    "The sum utility implements primitive checksumming of input files.\n"      \
    "Options are:\n"                                                           \
    "  -r, -- Use the BSD sum algorithm, and use 1K blocks.\n"                 \
    "  -s, --sysv -- Use the SYSV sum algorithm, and use 512 byte blocks.\n"   \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define SUM_OPTIONS_STRING "rs"

//
// Define the default buffer size.
//

#define SUM_BLOCK_SIZE 4096

//
// Define sum application options.
//

//
// Set this option to use the SYSV algorithm. If not set, the default is to
// use the BSD algorithm.
//

#define SUM_OPTION_SYSV 0x00000001

//
// Set this option to print file names.
//

#define SUM_OPTION_PRINT_NAMES 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SumChecksumFile (
    PSTR FileName,
    ULONG Options
    );

//
// -------------------------------------------------------------------- Globals
//

struct option SumLongOptions[] = {
    {"sysv", no_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
SumMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the sum utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    INT Option;
    ULONG Options;
    int Status;
    int TotalStatus;

    Options = 0;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SUM_OPTIONS_STRING,
                             SumLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'r':
            Options &= ~SUM_OPTION_SYSV;
            break;

        case 's':
            Options |= SUM_OPTION_SYSV | SUM_OPTION_PRINT_NAMES;
            break;

        case 'V':
            SwPrintVersion(SUM_VERSION_MAJOR, SUM_VERSION_MINOR);
            return 1;

        case 'h':
            printf(SUM_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    Status = 0;
    ArgumentIndex = optind;
    if (ArgumentIndex == ArgumentCount) {
        Status = SumChecksumFile("-", Options);

    } else {
        if (ArgumentIndex + 1 < ArgumentCount) {
            Options |= SUM_OPTION_PRINT_NAMES;
        }

        while (ArgumentIndex < ArgumentCount) {
            Status = SumChecksumFile(Arguments[ArgumentIndex], Options);
            if (Status != 0) {
                TotalStatus = Status;
            }

            ArgumentIndex += 1;
        }
    }

MainEnd:
    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SumChecksumFile (
    PSTR FileName,
    ULONG Options
    )

/*++

Routine Description:

    This routine checksums a file.

Arguments:

    FileName - Supplies a pointer to the path of the file to checksum, or "-"
        for standard in.

    Options - Supplies application options. See SUM_OPTION_* definitions.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    UCHAR Buffer[SUM_BLOCK_SIZE];
    ssize_t BytesRead;
    int File;
    UINTN Index;
    INT Status;
    ULONG Sum;
    ULONGLONG TotalSize;

    if (strcmp(FileName, "-") == 0) {
        File = STDIN_FILENO;

    } else {
        File = SwOpen(FileName, O_RDONLY | O_BINARY | O_NOCTTY, 0);
        if (File < 0) {
            Status = errno;
            SwPrintError(Status, FileName, "Cannot open");
            return Status;
        }
    }

    Sum = 0;
    TotalSize = 0;
    while (TRUE) {
        do {
            BytesRead = read(File, Buffer, SUM_BLOCK_SIZE);

        } while ((BytesRead < 0) && (errno == EINTR));

        if (BytesRead == 0) {
            break;

        } else if (BytesRead < 0) {
            Status = errno;
            SwPrintError(Status, FileName, "Read error");
            goto ChecksumFileEnd;
        }

        if ((Options & SUM_OPTION_SYSV) != 0) {
            for (Index = 0; Index < BytesRead; Index += 1) {
                Sum += Buffer[Index];
            }

        } else {

            //
            // The BSD algorithm rotates left by one bit each time, avoiding
            // some common problems with simple summing (like swapped bytes).
            //

            for (Index = 0; Index < BytesRead; Index += 1) {
                Sum = (Sum >> 1) + ((Sum & 1) << 15);
                Sum = (Sum + Buffer[Index]) & 0xFFFF;
            }
        }

        TotalSize += BytesRead;
    }

    //
    // Fold the result down to 16 bits.
    //

    if ((Options & SUM_OPTION_SYSV) != 0) {
        Sum = (Sum & 0xFFFF) + (Sum >> 16);
        Sum = (Sum & 0xFFFF) + (Sum >> 16);
        TotalSize = (TotalSize + 511ULL) / 512ULL;

    } else {
        TotalSize = (TotalSize + 1023ULL) / 1024ULL;
    }

    if ((Options & SUM_OPTION_PRINT_NAMES) != 0) {
        if ((Options & SUM_OPTION_SYSV) != 0) {
            printf("%d %llu %s\n", Sum, TotalSize, FileName);

        } else {
            printf("%05d %5llu %s\n", Sum, TotalSize, FileName);
        }

    } else {

        //
        // The sysv option always turns on the print names flag.
        //

        assert((Options & SUM_OPTION_SYSV) == 0);

        printf("%05d %5llu %s\n", Sum, TotalSize, FileName);
    }

    Status = 0;

ChecksumFileEnd:
    if (File > 0) {

        assert(STDIN_FILENO == 0);

        close(File);
    }

    return Status;
}

