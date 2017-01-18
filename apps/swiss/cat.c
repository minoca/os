/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cat.c

Abstract:

    This module implements the standard cat (concatenate) tool.

Author:

    Evan Green 6-Jun-2013

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
// This macro determines the output buffer size for the given input buffer.
// At worst each character could expand out to 4 characters, plus a $ at the
// end, plus a line number at the beginning, which is assumed not to hit 100
// million lines.
//

#define CAT_OUTPUT_BLOCK_SIZE(_InputSize) \
    (((_InputSize) * 4) + 10 + 2)

//
// This macro returns non-zero if the output buffer could overflow during
// processing of the next line. This would occur if an expanded character,
// line number, dollar sign, and newline were all printed out.
//

#define CAT_OUTPUT_NEARLY_FULL(_Context) \
    ((_Context)->OutputIndex + 10 + 2 + 4 >= (_Context)->OutputBufferSize)

//
// ---------------------------------------------------------------- Definitions
//

#define CAT_VERSION_MAJOR 1
#define CAT_VERSION_MINOR 0

#define CAT_USAGE                                                              \
    "usage: cat [options] [files]\n\n"                                         \
    "The cat utility concatenates file onto standard output. Options are as\n" \
    "follows:\n\n"                                                             \
    "    -A --show-all -- Equivalent to -vET\n"                                \
    "    -b --number-nonblank -- Number nonempty output lines\n"               \
    "    -e -- Equivalent to -vE\n"                                            \
    "    -E --show-ends -- Display $ at the end of each line.\n"               \
    "    -n --number -- Number all output lines.\n"                            \
    "    -s --squeeze-blank -- Suppress repeated empty output lines.\n"        \
    "    -t -- Equivalent to -vT.\n"                                           \
    "    -T --show-tabs -- display tab characters as ^I.\n"                    \
    "    -u -- Ignored.\n"                                                     \
    "    -v --show-nonprinting -- Use ^ and M- notation, except for line \n"   \
    "       feed and tab characters.\n"                                        \
    "    --help -- Display this help text.\n"                                  \
    "    --version -- Display version information and exit.\n\n"

#define CAT_OPTIONS_STRING "AbeEnstTuv"

//
// Define the block size used in data transfers.
//

#define CAT_INPUT_BLOCK_SIZE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the application context for the cat program.

Members:

    Arguments - Supplies a pointer to the array of command line arguments.

    ArgumentCount - Supplies the number of arguments in the command line.

    FilesGiven - Supplies a boolean indicating if there were files specified
        on the command line.

    NumberNonBlanks - Stores a boolean indicating whether or not to output line
        numbers for non-empty lines.

    ShowLineEnds - Stores a boolean indicating whether or not to print $ after
        each line.

    NumberAllLines - Stores a boolean indicating whether or not to print line
        numbers for all lines.

    RepressRepeatedEmptyLines - Stores a boolean indicating whether more than
        one empty line in the input should be condensed into one line in the
        output.

    ShowTabs - Stores a boolean indicating whether tabs should be printed to
        the output as ^I.

    ShowNonPrinting - Stores a boolean indicating whether or not to use ^ and
        M- notation to display non-printable characters.

    InputBuffer - Stores a pointer to the input buffer.

    InputBufferSize - Stores the total size of the input buffer.

    OutputBuffer - Stores the output buffer.

    OutputBufferSize - Stores the total size of the output buffer.

    OutputIndex - Stores the next free index of the output buffer.

    LineNumber - Stores the next line number to print out.

    LastLineBlank - Stores a boolean indicating if the previous line was
        blank.

    ThisLineBlank - Stores a boolean indicating if the current line is blank.

--*/

typedef struct _CAT_CONTEXT {
    PSTR *Arguments;
    ULONG ArgumentCount;
    BOOL FilesGiven;
    BOOL NumberNonBlanks;
    BOOL ShowLineEnds;
    BOOL NumberAllLines;
    BOOL RepressRepeatedEmptyLines;
    BOOL ShowTabs;
    BOOL ShowNonPrinting;
    PSTR InputBuffer;
    ULONG InputBufferSize;
    PSTR OutputBuffer;
    ULONG OutputBufferSize;
    ULONG OutputIndex;
    ULONG LineNumber;
    BOOL LastLineBlank;
    BOOL ThisLineBlank;
} CAT_CONTEXT, *PCAT_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
CatPrintContents (
    PCAT_CONTEXT Context,
    INT FileDescriptor
    );

BOOL
CatWriteOutputBuffer (
    PCAT_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

struct option CatLongOptions[] = {
    {"show-all", no_argument, 0, 'A'},
    {"number-nonblank", no_argument, 0, 'b'},
    {"show-ends", no_argument, 0, 'E'},
    {"number", no_argument, 0, 'n'},
    {"squeeze-blank", no_argument, 0, 's'},
    {"show-tabs", no_argument, 0, 'T'},
    {"show-nonprinting", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
CatMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the cat program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    CAT_CONTEXT Context;
    BOOL Failed;
    INT FileDescriptor;
    INT OpenFlags;
    INT Option;
    BOOL Result;

    Failed = FALSE;
    memset(&Context, 0, sizeof(CAT_CONTEXT));
    Context.Arguments = Arguments;
    Context.ArgumentCount = ArgumentCount;
    Context.LineNumber = 1;
    Context.ThisLineBlank = TRUE;

    //
    // Count the arguments and get the basic ones out of the way.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             CAT_OPTIONS_STRING,
                             CatLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            return 1;
        }

        switch (Option) {
        case 'A':
            Context.ShowNonPrinting = TRUE;
            Context.ShowLineEnds = TRUE;
            Context.ShowTabs = TRUE;
            break;

        case 'b':
            Context.NumberNonBlanks = TRUE;
            break;

        case 'e':
            Context.ShowNonPrinting = TRUE;
            Context.ShowLineEnds = TRUE;
            break;

        case 'E':
            Context.ShowLineEnds = TRUE;
            break;

        case 'n':
            Context.NumberAllLines = TRUE;
            break;

        case 's':
            Context.RepressRepeatedEmptyLines = TRUE;
            break;

        case 't':
            Context.ShowNonPrinting = TRUE;
            Context.ShowTabs = TRUE;
            break;

        case 'T':
            Context.ShowTabs = TRUE;
            break;

        case 'u':
            break;

        case 'v':
            Context.ShowNonPrinting = TRUE;
            break;

        case 'V':
            SwPrintVersion(CAT_VERSION_MAJOR, CAT_VERSION_MINOR);
            return 1;

        case 'h':
            printf(CAT_USAGE);
            return 1;

        default:

            assert(FALSE);

            return 1;
        }
    }

    //
    // If the caller wants to see non-printing characters, open the file in
    // binary mode. Otherwise, assume it's text in, text out.
    //

    OpenFlags = O_RDONLY;
    if ((Context.ShowNonPrinting != FALSE) || (Context.ShowLineEnds != FALSE)) {
        OpenFlags |= O_BINARY;
    }

    ArgumentIndex = optind;
    if (ArgumentIndex < ArgumentCount) {
        Context.FilesGiven = TRUE;
    }

    if (Context.NumberNonBlanks != FALSE) {
        Context.NumberAllLines = FALSE;
    }

    //
    // Allocate the input and output buffers.
    //

    Context.InputBufferSize = CAT_INPUT_BLOCK_SIZE;
    Context.InputBuffer = malloc(Context.InputBufferSize);
    if (Context.InputBuffer == NULL) {
        SwPrintError(0, NULL, "Failed to allocate memory");
        Failed = TRUE;
        goto mainEnd;
    }

    Context.OutputBufferSize = CAT_OUTPUT_BLOCK_SIZE(Context.InputBufferSize);
    Context.OutputBuffer = malloc(Context.OutputBufferSize);
    if (Context.OutputBuffer == NULL) {
        SwPrintError(0, NULL, "Failed to allocate memory");
        Failed = TRUE;
        goto mainEnd;
    }

    //
    // Now that the arguments have all been squared away, check to see if any
    // files were specified. If they weren't, just use standard in. Otherwise,
    // open up each file and print the contents.
    //

    if (Context.FilesGiven != FALSE) {
        while (ArgumentIndex < ArgumentCount) {
            Argument = Arguments[ArgumentIndex];
            ArgumentIndex += 1;
            if (Argument[0] == '-') {
                if (Argument[1] == '\0') {
                    FileDescriptor = STDIN_FILENO;

                } else {
                    continue;
                }

            } else {
                FileDescriptor = SwOpen(Argument, OpenFlags, 0);
                if (FileDescriptor < 0) {
                    SwPrintError(errno, Argument, "Failed to open file");
                    Failed = TRUE;
                    continue;
                }
            }

            Result = CatPrintContents(&Context, FileDescriptor);
            close(FileDescriptor);
            if (Result == FALSE) {
                Failed = TRUE;
            }
        }

    } else {
        Result = CatPrintContents(&Context, STDIN_FILENO);
        if (Result == FALSE) {
            Failed = TRUE;
        }
    }

mainEnd:
    if (Context.InputBuffer != NULL) {
        free(Context.InputBuffer);
    }

    if (Context.OutputBuffer != NULL) {
        free(Context.OutputBuffer);
    }

    if (Failed != FALSE) {
        return 1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
CatPrintContents (
    PCAT_CONTEXT Context,
    INT FileDescriptor
    )

/*++

Routine Description:

    This routine performs the work of the cat utility, printing out file
    contents.

Arguments:

    Context - Supplies a pointer to the initialized application context.

    FileDescriptor - Supplies the open file descriptor.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ssize_t BytesRead;
    UCHAR Character;
    ULONG InputIndex;
    BOOL LastLineEmpty;
    BOOL Result;
    BOOL ThisLineEmpty;

    //
    // Loop reading and writing bytes.
    //

    while (TRUE) {

        //
        // Read some stuff.
        //

        do {
            BytesRead = read(FileDescriptor,
                             Context->InputBuffer,
                             Context->InputBufferSize);

        } while ((BytesRead < 0) && (errno == EINTR));

        //
        // Stop if the read failed.
        //

        if (BytesRead < 0) {
            Result = FALSE;
            break;
        }

        //
        // If this is the end of the file, stop.
        //

        if (BytesRead == 0) {
            Result = TRUE;
            break;
        }

        //
        // Loop through moving bytes from the input to the output.
        //

        for (InputIndex = 0; InputIndex < BytesRead; InputIndex += 1) {
            Character = Context->InputBuffer[InputIndex];
            if (Character == '\n') {
                LastLineEmpty = Context->LastLineBlank;
                ThisLineEmpty = Context->ThisLineBlank;
                Context->LastLineBlank = Context->ThisLineBlank;
                Context->ThisLineBlank = TRUE;

                //
                // If skipping extra blank lines and the last line was blank,
                // do nothing to the output.
                //

                if ((Context->RepressRepeatedEmptyLines != FALSE) &&
                    (ThisLineEmpty != FALSE) && (LastLineEmpty != FALSE)) {

                    continue;
                }

                //
                // If printing line numbers for all lines and this line is
                // empty, print the line number.
                //

                if (Context->NumberAllLines != FALSE) {
                    if (ThisLineEmpty != FALSE) {
                        Context->OutputIndex += snprintf(
                              Context->OutputBuffer + Context->OutputIndex,
                              Context->OutputBufferSize - Context->OutputIndex,
                              "%9d ",
                              Context->LineNumber);

                        Context->LineNumber += 1;
                    }
                }

                //
                // If printing line ends, add a dollar sign.
                //

                if (Context->ShowLineEnds != FALSE) {
                    Context->OutputBuffer[Context->OutputIndex] = '$';
                    Context->OutputIndex += 1;
                }

                //
                // Add the newline.
                //

                Context->OutputBuffer[Context->OutputIndex] = '\n';
                Context->OutputIndex += 1;

            } else {
                if (Context->ThisLineBlank != FALSE) {

                    //
                    // If line numbers are printed on non-blanks, now's the
                    // time for that.
                    //

                    if ((Context->NumberNonBlanks != FALSE) ||
                        (Context->NumberAllLines != FALSE)) {

                        Context->OutputIndex += snprintf(
                              Context->OutputBuffer + Context->OutputIndex,
                              Context->OutputBufferSize - Context->OutputIndex,
                              "%9d ",
                              Context->LineNumber);

                        Context->LineNumber += 1;
                    }
                }

                Context->ThisLineBlank = FALSE;

                //
                // If the character is a tab and tabs are being shown
                // differently, spit that out.
                //

                if (Character == '\t') {
                    if (Context->ShowTabs != FALSE) {
                        Context->OutputBuffer[Context->OutputIndex] = '^';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] = 'I';
                        Context->OutputIndex += 1;

                    } else {
                        Context->OutputBuffer[Context->OutputIndex] = '\t';
                        Context->OutputIndex += 1;
                    }

                //
                // If the character is non-printable and those are being
                // displayed, then expand that out.
                //

                } else if (((Character < ' ') || (Character > '~')) &&
                           (Context->ShowNonPrinting != FALSE)) {

                    if (Character < ' ') {
                        Context->OutputBuffer[Context->OutputIndex] = '^';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] =
                                                               Character + '@';

                        Context->OutputIndex += 1;

                    } else if (Character == 0x7F) {
                        Context->OutputBuffer[Context->OutputIndex] = '^';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] = '?';
                        Context->OutputIndex += 1;

                    } else if (Character < 0xA0) {
                        Context->OutputBuffer[Context->OutputIndex] = 'M';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] = '-';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] = '^';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] =
                                                        Character - 0x80 + '@';

                        Context->OutputIndex += 1;

                    } else if (Character < 0xFF) {
                        Context->OutputBuffer[Context->OutputIndex] = 'M';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] = '-';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] =
                                                        Character - 0xA0 + ' ';

                        Context->OutputIndex += 1;

                    } else {
                        Context->OutputBuffer[Context->OutputIndex] = 'M';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] = '-';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] = '^';
                        Context->OutputIndex += 1;
                        Context->OutputBuffer[Context->OutputIndex] = '?';
                        Context->OutputIndex += 1;
                    }

                //
                // Spit this out like a normal character.
                //

                } else {
                    Context->OutputBuffer[Context->OutputIndex] = Character;
                    Context->OutputIndex += 1;
                }
            }

            assert(Context->OutputIndex < Context->OutputBufferSize);

            Result = CatWriteOutputBuffer(Context);
            if (Result == FALSE) {
                goto PrintContentsEnd;
            }
        }
    }

PrintContentsEnd:
    return Result;
}

BOOL
CatWriteOutputBuffer (
    PCAT_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes the contents of the output buffer to the output and
    resets the output buffer.

Arguments:

    Context - Supplies a pointer to the initialized application context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ssize_t BytesWritten;
    ULONG OutputSize;
    ssize_t TotalBytesWritten;

    if (Context->OutputIndex == 0) {
        return TRUE;
    }

    OutputSize = Context->OutputIndex;
    Context->OutputIndex = 0;
    TotalBytesWritten = 0;
    while (TotalBytesWritten != OutputSize) {
        do {
            BytesWritten = write(STDOUT_FILENO,
                                 Context->OutputBuffer + TotalBytesWritten,
                                 OutputSize - TotalBytesWritten);

        } while ((BytesWritten < 0) && (errno == EINTR));

        if (BytesWritten <= 0) {
            return FALSE;
        }

        TotalBytesWritten += BytesWritten;
    }

    return TRUE;
}

