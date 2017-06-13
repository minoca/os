/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    od.c

Abstract:

    This module implements the od (octal dump) utility.

Author:

    Evan Green 9-Oct-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OD_VERSION_MAJOR 1
#define OD_VERSION_MINOR 0

#define OD_USAGE                                                               \
    "usage: od [-vw][-A base][-j skip][-N count][-t type]... [file]...\n"      \
    "       od [-bcdosxw] [file] [[+]offset[.][b]]\n"                          \
    "The od utility dumps a given file's contents as a sequence of integers.\n"\
    "Valid options are:\n"                                                     \
    "  -A, --address-radix <base> -- Change the base addresses are printed \n" \
    "      in. Valid values are d (decimal), o (octal), x (hexadecimal) or \n" \
    "      n (do not print addresses).\n"                                      \
    "  -b -- Octal, same as -t o1.\n"                                          \
    "  -c -- Output bytes as characters, same as -t c\n"                       \
    "  -d -- Words, same as -t o2.\n"                                          \
    "  -j, --skip-bytes <count> -- Skip bytes before dumping. A \n"            \
    "      character can be appended for units: b for bytes, k for 1024 \n"    \
    "      bytes, and m for 1048576 bytes. If a hex value is specified, b \n"  \
    "      would be taken to be the last hex digit.\n"                         \
    "  -N --read-bytes <count> -- Read only the given number of bytes.\n"      \
    "  -o -- Octal words, same as -t o2.\n"                                    \
    "  -s -- Signed words, same as -t d2.\n"                                   \
    "  -t, --format <type> -- Specifies the format of how to dump the data. \n"\
    "      Valid values are acdfou and x, for named character, character, \n"  \
    "      signed decimal, float, octal, unsigned decimal, and hexadecimal. \n"\
    "      The values dfou and x can have an optional unsigned decimal \n"     \
    "      integer representing the byte count of the type. The f value \n"    \
    "      can have an optional FDL after it specifying float, double, or \n"  \
    "      long double size. The doux values can have an optional CSI or L \n" \
    "      after them to specify char, short, int or long sizes.\n"            \
    "  -v, --output-duplicates -- Output all lines. Otherwise any number of \n"\
    "        duplicate lines is indicated with a single *.\n"                  \
    "  -x -- Hex words, same as -t x2.\n"                                      \
    "  -w, --width <width> -- Output the given number of bytes per line.\n"    \
    "  file -- Zero or files to dump. If no files are specified, stdin is "    \
    "used.\n"                                                                  \
    "  offset -- Specifies an offset from the file to dump. Interpreted as \n" \
    "      an octal value. With an optional . at the end, it's interpreted \n" \
    "      as a decimal value. With an optional b at the end, it's \n"         \
    "      interpreted as a 512-byte block offset.\n"                          \

#define OD_OPTIONS_STRING "A:bcdj:N:ost:vxw:"

//
// Define od options.
//

//
// Set this option to print all lines, even duplicates.
//

#define OD_OPTION_PRINT_DUPLICATES 0x00000001

//
// Define the default address radix if non was specified.
//

#define OD_DEFAULT_ADDRESS_RADIX 8

//
// Define the default byte width of a line.
//

#define OD_DEFAULT_WIDTH 16

//
// Define the maximum number of output formatters.
//

#define OD_OUTPUT_FORMATTER_COUNT 16

//
// Define the size of the named character array.
//

#define OD_NAMED_CHARACTERS 34

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _OD_OUTPUT_TYPE {
    OdOutputInvalid,
    OdOutputCharacter,
    OdOutputNamedCharacter,
    OdOutputSignedInteger,
    OdOutputUnsignedInteger,
    OdOutputFloat,
    OdOutputDouble,
    OdOutputLongDouble,
} OD_OUTPUT_TYPE, *POD_OUTPUT_TYPE;

/*++

Structure Description:

    This structure defines an od output formatter.

Members:

    OutputType - Stores the type of output to produce.

    Radix - Stores the base to output in for integer types.

    Size - Stores the size to output in for integer types.

    Width - Stores the width of a field.

--*/

typedef struct _OD_OUTPUT_FORMAT {
    OD_OUTPUT_TYPE OutputType;
    INT Radix;
    INT Size;
    INT Width;
} OD_OUTPUT_FORMAT, *POD_OUTPUT_FORMAT;

/*++

Structure Description:

    This structure defines an od input entry.

Members:

    ListEntry - Stores pointers to the next and previous input entries.

    File - Stores a pointer to the open file handle.

    Name - Stores a pointer to the file name.

--*/

typedef struct _OD_INPUT_ENTRY {
    LIST_ENTRY ListEntry;
    FILE *File;
    PSTR Name;
} OD_INPUT_ENTRY, *POD_INPUT_ENTRY;

/*++

Structure Description:

    This structure defines the application context for an instance of the
    octal dump application.

Members:

    Options - Stores a bitfield of application options. See OD_* definitions.

    AddressRadix - Stores the radix of the address to print.

    AddressWidth - Stores the width of the address.

    SkipCount - Stores the number of bytes to skip before dumping the file.

    Count - Stores the number of bytes to dump.

    Width - Stores the width of an od line in bytes of output.

    InputList - Stores the head of the list of input entries.

    CurrentInput - Stores a pointer to the current input entry.

    FormatCount - Stores the number of valid output formats.

    Formats - Stores the array of output formatters.

--*/

typedef struct _OD_CONTEXT {
    ULONG Options;
    INT AddressRadix;
    INT AddressWidth;
    ULONGLONG SkipCount;
    ULONGLONG Count;
    UINTN Width;
    LIST_ENTRY InputList;
    POD_INPUT_ENTRY CurrentInput;
    ULONG FormatCount;
    OD_OUTPUT_FORMAT Formats[OD_OUTPUT_FORMATTER_COUNT];
} OD_CONTEXT, *POD_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
OdParseOutputFormatter (
    POD_CONTEXT Context,
    PSTR Format
    );

INT
OdPerformInitialSeek (
    POD_CONTEXT Context
    );

INT
OdDump (
    POD_CONTEXT Context
    );

INT
OdReadBlock (
    POD_CONTEXT Context,
    PCHAR Buffer,
    PULONG BufferSize
    );

VOID
OdPrintAddress (
    POD_CONTEXT Context,
    ULONGLONG Address
    );

VOID
OdDumpFormat (
    POD_CONTEXT Context,
    PCHAR Line,
    ULONGLONG LineSize,
    POD_OUTPUT_FORMAT Format
    );

//
// -------------------------------------------------------------------- Globals
//

struct option OdLongOptions[] = {
    {"address-radix", required_argument, 0, 'A'},
    {"skip-bytes", required_argument, 0, 'j'},
    {"read-bytes", required_argument, 0, 'N'},
    {"format", required_argument, 0, 't'},
    {"output-duplicates", no_argument, 0, 'v'},
    {"width", required_argument, 0, 'w'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

PSTR OdCharacterNames[OD_NAMED_CHARACTERS] = {
    "nul",
    "soh",
    "stx",
    "etx",
    "eot",
    "enq",
    "ack",
    "bel",
    "bs",
    "ht",
    "nl",
    "vt",
    "ff",
    "cr",
    "so",
    "si",
    "dle",
    "dc1",
    "dc2",
    "dc3",
    "dc4",
    "nak",
    "syn",
    "etb",
    "can",
    "em",
    "sub",
    "esc",
    "fs",
    "gs",
    "rs",
    "us",
    "sp",
    "del",
};

//
// ------------------------------------------------------------------ Functions
//

INT
OdMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the cp utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    PSTR Argument;
    ULONG ArgumentIndex;
    size_t ArgumentLength;
    INT Base;
    OD_CONTEXT Context;
    BOOL CouldHaveOffset;
    POD_INPUT_ENTRY Input;
    CHAR LastCharacter;
    INT Multiplier;
    INT OffsetArgument;
    INT Option;
    int Status;

    memset(&Context, 0, sizeof(OD_CONTEXT));
    INITIALIZE_LIST_HEAD(&(Context.InputList));
    Context.AddressRadix = OD_DEFAULT_ADDRESS_RADIX;
    Context.AddressWidth = 7;
    Context.Count = MAX_ULONGLONG;
    Context.Width = OD_DEFAULT_WIDTH;
    CouldHaveOffset = TRUE;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             OD_OPTIONS_STRING,
                             OdLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'A':
            CouldHaveOffset = FALSE;
            Argument = optarg;
            switch (*Argument) {
            case 'd':
                Context.AddressRadix = 10;
                Context.AddressWidth = 7;
                break;

            case 'o':
                Context.AddressRadix = 8;
                Context.AddressWidth = 7;
                break;

            case 'x':
                Context.AddressRadix = 16;
                Context.AddressWidth = 6;
                break;

            case 'n':
                Context.AddressRadix = 0;
                Context.AddressWidth = 0;
                break;

            default:
                SwPrintError(0, Argument, "Invalid address radix");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'b':
            Status = OdParseOutputFormatter(&Context, "o1");
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'c':
            Status = OdParseOutputFormatter(&Context, "c");
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'd':
            Status = OdParseOutputFormatter(&Context, "u2");
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        //
        // Parse a skip count, which can have a b, k, or m on the end of it for
        // bytes, kilobytes, and megabytes.
        //

        case 'j':
            CouldHaveOffset = FALSE;
            ArgumentLength = strlen(optarg);
            if (ArgumentLength == 0) {
                SwPrintError(0, optarg, "Invalid skip count");
                Status = 1;
                goto MainEnd;
            }

            Argument = strdup(optarg);
            if (Argument == NULL) {
                Status = ENOMEM;
                goto MainEnd;
            }

            LastCharacter = Argument[ArgumentLength - 1];
            if ((LastCharacter == 'k') || (LastCharacter == 'm')) {
                Argument[ArgumentLength - 1] = '\0';

            } else {
                LastCharacter = 0;
            }

            Context.SkipCount = strtoll(Argument, &AfterScan, 0);
            if (AfterScan == Argument) {
                SwPrintError(0, Argument, "Invalid skip count");
                free(Argument);
                Status = 1;
                goto MainEnd;
            }

            free(Argument);
            if (LastCharacter == 'k') {
                Context.SkipCount *= 1024;

            } else if (LastCharacter == 'm') {
                Context.SkipCount *= 1024 * 1024;
            }

            break;

        case 'N':
            CouldHaveOffset = FALSE;
            Context.Count = strtoll(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid byte count");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'o':
            Status = OdParseOutputFormatter(&Context, "o2");
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 's':
            Status = OdParseOutputFormatter(&Context, "d2");
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 't':
            CouldHaveOffset = FALSE;
            Status = OdParseOutputFormatter(&Context, optarg);
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'v':
            Context.Options |= OD_OPTION_PRINT_DUPLICATES;
            break;

        case 'w':
            Context.Count = strtoll(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid width");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'x':
            Status = OdParseOutputFormatter(&Context, "x2");
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'V':
            SwPrintVersion(OD_VERSION_MAJOR, OD_VERSION_MINOR);
            return 1;

        case 'h':
            printf(OD_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // Figure out if there's an offset as an operand. There's an offset if:
    // 1) None of -A -j -N or -t were specified.
    // 2) There are no more than two operands.
    // 3) Either A) The first character of the last operand is a + or B) There
    // are two operands and the first character of the last operand starts with
    // a digit.
    //

    OffsetArgument = 0;
    if ((CouldHaveOffset != FALSE) && (ArgumentCount - ArgumentIndex <= 2) &&
        (ArgumentCount - ArgumentIndex != 0)) {

        OffsetArgument = ArgumentCount - 1;
        Argument = Arguments[OffsetArgument];
        if ((*Argument == '+') ||
            ((ArgumentCount - ArgumentIndex == 2) && (isdigit(*Argument)))) {

            Argument = strdup(Argument);
            if (Argument == NULL) {
                Status = ENOMEM;
                goto MainEnd;
            }

            //
            // Parse an offset.
            //

            Base = 8;
            Multiplier = 1;
            ArgumentLength = strlen(Argument);

            assert(ArgumentLength != 0);

            while ((ArgumentLength != 0) &&
                   ((Argument[ArgumentLength - 1] == '.') ||
                    (Argument[ArgumentLength - 1] == 'b'))) {

                if (Argument[ArgumentLength - 1] == '.') {
                    Base = 10;

                } else {
                    Multiplier = 512;
                }

                Argument[ArgumentLength - 1] = '\0';
                ArgumentLength -= 1;
            }

            Context.SkipCount = strtol(Argument, &AfterScan, Base);
            if (Argument == AfterScan) {
                SwPrintError(0, Argument, "Invalid offset");
                free(Argument);
                Status = EINVAL;
                goto MainEnd;
            }

            Context.SkipCount *= Multiplier;
            free(Argument);

        } else {
            OffsetArgument = 0;
        }
    }

    //
    // If no formats were specified, it's like -t oS was specified.
    //

    if (Context.FormatCount == 0) {
        Status = OdParseOutputFormatter(&Context, "oS");
        if (Status != 0) {
            goto MainEnd;
        }
    }

    //
    // Now add all the arguments as input entries.
    //

    while (ArgumentIndex < ArgumentCount) {
        if (ArgumentIndex == OffsetArgument) {
            ArgumentIndex += 1;
            continue;
        }

        Argument = Arguments[ArgumentIndex];
        Input = malloc(sizeof(OD_INPUT_ENTRY));
        if (Input == NULL) {
            Status = ENOMEM;
            goto MainEnd;
        }

        memset(Input, 0, sizeof(OD_INPUT_ENTRY));
        Input->File = fopen(Argument, "rb");
        if (Input->File == NULL) {
            free(Input);
            Status = errno;
            SwPrintError(Status, Argument, "Failed to open");
            goto MainEnd;
        }

        Input->Name = Argument;
        INSERT_BEFORE(&(Input->ListEntry), &(Context.InputList));
        ArgumentIndex += 1;
    }

    //
    // If no files were processed, dump standard in.
    //

    if (LIST_EMPTY(&(Context.InputList)) != FALSE) {
        Input = malloc(sizeof(OD_INPUT_ENTRY));
        if (Input == NULL) {
            Status = ENOMEM;
            goto MainEnd;
        }

        memset(Input, 0, sizeof(OD_INPUT_ENTRY));
        Input->File = stdin;
        Input->Name = "(stdin)";
        INSERT_BEFORE(&(Input->ListEntry), &(Context.InputList));
    }

    Status = OdPerformInitialSeek(&Context);
    if (Status != 0) {
        goto MainEnd;
    }

    Status = OdDump(&Context);
    if (Status != 0) {
        goto MainEnd;
    }

MainEnd:
    while (LIST_EMPTY(&(Context.InputList)) == FALSE) {
        Input = LIST_VALUE(Context.InputList.Next, OD_INPUT_ENTRY, ListEntry);
        LIST_REMOVE(&(Input->ListEntry));
        if ((Input->File != NULL) && (Input->File != stdin)) {
            fclose(Input->File);
        }

        free(Input);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
OdParseOutputFormatter (
    POD_CONTEXT Context,
    PSTR Format
    )

/*++

Routine Description:

    This routine parses one or more od output formats from the command line.

Arguments:

    Context - Supplies a pointer to the application context.

    Format - Supplies the format string.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    CHAR BaseCharacter;
    POD_OUTPUT_FORMAT Formatter;
    INT Size;
    INT Status;

    Status = 0;
    while (TRUE) {
        if (*Format == 0) {
            break;
        }

        //
        // Fail if there are no more slots left.
        //

        if (Context->FormatCount == OD_OUTPUT_FORMATTER_COUNT) {
            SwPrintError(0, NULL, "Too many output formats");
            Status = EINVAL;
            goto ParseOutputFormatterEnd;
        }

        Formatter = &(Context->Formats[Context->FormatCount]);

        //
        // Get the primary format.
        //

        BaseCharacter = *Format;
        Formatter->Size = sizeof(INT);
        switch (BaseCharacter) {
        case 'a':
            Formatter->OutputType = OdOutputNamedCharacter;
            Formatter->Size = 1;
            break;

        case 'c':
            Formatter->OutputType = OdOutputCharacter;
            Formatter->Size = 1;
            break;

        case 'd':
            Formatter->OutputType = OdOutputSignedInteger;
            Formatter->Radix = 10;
            break;

        case 'f':
            Formatter->OutputType = OdOutputDouble;
            Formatter->Size = sizeof(double);
            break;

        case 'o':
            Formatter->OutputType = OdOutputUnsignedInteger;
            Formatter->Radix = 8;
            break;

        case 'u':
            Formatter->OutputType = OdOutputUnsignedInteger;
            Formatter->Radix = 10;
            break;

        case 'x':
            Formatter->OutputType = OdOutputUnsignedInteger;
            Formatter->Radix = 16;
            break;

        default:
            SwPrintError(0, Format, "Invalid output format");
            Status = EINVAL;
            goto ParseOutputFormatterEnd;
        }

        //
        // The characters dfoux have an optional size integer.
        //

        Format += 1;
        if (((BaseCharacter == 'd') ||
             (BaseCharacter == 'f') ||
             (BaseCharacter == 'o') ||
             (BaseCharacter == 'u') ||
             (BaseCharacter == 'x')) && (isdigit(*Format))) {

            Size = strtoul(Format, &AfterScan, 10);
            if ((Size < 0) || (AfterScan == Format)) {
                SwPrintError(0, Format, "Invalid formatter size");
                Status = EINVAL;
                goto ParseOutputFormatterEnd;
            }

            //
            // Validate the size. Integers can be anything up to 8. Floats
            // can be 4, 8 or 12.
            //

            if (BaseCharacter == 'f') {
                if (Size == 4) {
                    Formatter->OutputType = OdOutputFloat;
                    Formatter->Size = sizeof(float);

                } else if (Size == 8) {
                    Formatter->OutputType = OdOutputDouble;
                    Formatter->Size = sizeof(double);

                } else if (Size == sizeof(long double)) {
                    Formatter->OutputType = OdOutputLongDouble;
                    Formatter->Size = sizeof(long double);

                } else {
                    SwPrintError(0,
                                 NULL,
                                 "Invalid size %d, valid float sizes are 4, "
                                 "8, and %d",
                                 Size,
                                 sizeof(long double));

                    Status = EINVAL;
                    goto ParseOutputFormatterEnd;
                }

            } else {
                if ((Size == 0) || (Size > 8)) {
                    SwPrintError(0,
                                 NULL,
                                 "Invalid size %d, valid float sizes are "
                                 "between 1 and 8",
                                 Size);

                    Status = EINVAL;
                    goto ParseOutputFormatterEnd;
                }

                Formatter->Size = Size;
            }

            Format = AfterScan;

        //
        // The f character could have an FD or L on the end.
        //

        } else if ((BaseCharacter == 'f') &&
                   ((*Format == 'F') || (*Format == 'D') || (*Format == 'L'))) {

            if (*Format == 'F') {
                Formatter->OutputType = OdOutputFloat;

            } else if (*Format == 'D') {
                Formatter->OutputType = OdOutputDouble;

            } else {
                Formatter->OutputType = OdOutputLongDouble;
            }

            Format += 1;

        //
        // The characters dou and x can also have a CSI or L after them for the
        // C types char, short, int, and long.
        //

        } else if (((BaseCharacter == 'd') || (BaseCharacter == 'o') ||
                    (BaseCharacter == 'u') || (BaseCharacter == 'x')) &&
                   ((*Format == 'C') || (*Format == 'S') || (*Format == 'I') ||
                    (*Format == 'L'))) {

            if (*Format == 'C') {
                Formatter->Size = sizeof(CHAR);

            } else if (*Format == 'S') {
                Formatter->Size = sizeof(SHORT);

            } else if (*Format == 'I') {
                Formatter->Size = sizeof(INT);

            } else {
                Formatter->Size = sizeof(LONG);
            }

            Format += 1;
        }

        //
        // Figure out the width of this field.
        //

        switch (Formatter->OutputType) {
        case OdOutputCharacter:
        case OdOutputNamedCharacter:
            Formatter->Width = 3;
            break;

        case OdOutputSignedInteger:
        case OdOutputUnsignedInteger:
            switch (Formatter->Radix) {
            case 8:
                if (Formatter->Size == 1) {
                    Formatter->Width = 3;

                } else if (Formatter->Size == 2) {
                    Formatter->Width = 6;

                } else if (Formatter->Size <= 4) {
                    Formatter->Width = 11;

                } else {
                    Formatter->Width = 22;
                }

                break;

            case 10:
                if (Formatter->Size == 1) {
                    Formatter->Width = 4;

                } else if (Formatter->Size == 2) {
                    Formatter->Width = 5;

                } else if (Formatter->Size <= 4) {
                    Formatter->Width = 11;

                } else {
                    Formatter->Width = 20;
                }

                break;

            case 16:
                Formatter->Width = 2 * Formatter->Size;
                break;

            default:

                assert(FALSE);

                break;
            }

            break;

        case OdOutputFloat:
            Formatter->Width = 14;
            break;

        case OdOutputDouble:
        case OdOutputLongDouble:
            Formatter->Width = 23;
            break;

        default:

            assert(FALSE);

            break;
        }

        Context->FormatCount += 1;
    }

ParseOutputFormatterEnd:
    return Status;
}

INT
OdPerformInitialSeek (
    POD_CONTEXT Context
    )

/*++

Routine Description:

    This routine seeks to the desired offset in the concatenated input stream.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG Count;
    INT Result;

    assert(LIST_EMPTY(&(Context->InputList)) == FALSE);

    Context->CurrentInput = LIST_VALUE(Context->InputList.Next,
                                       OD_INPUT_ENTRY,
                                       ListEntry);

    Count = Context->SkipCount;
    Result = OdReadBlock(Context, NULL, &Count);
    if (Result != 0) {
        return Result;
    }

    if (Count != Context->SkipCount) {
        SwPrintError(0,
                     NULL,
                     "Input stream ended after %lld bytes, but "
                     "requested skip count was %lld bytes.\n",
                     Count,
                     Context->SkipCount);

        Result = ERANGE;
        return Result;
    }

    return 0;
}

INT
OdDump (
    POD_CONTEXT Context
    )

/*++

Routine Description:

    This routine performs the formatted dump of a given input stream.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONGLONG Address;
    ULONGLONG Count;
    BOOL FirstLine;
    ULONG FormatIndex;
    BOOL InDuplicate;
    PCHAR Line;
    ULONG LineSize;
    PCHAR PreviousLine;
    INT Status;

    Address = Context->SkipCount;
    Count = Context->Count;
    Status = 0;

    assert(Context->Width != 0);

    Line = malloc(Context->Width);
    if (Line == NULL) {
        Status = ENOMEM;
        goto DumpFileEnd;
    }

    PreviousLine = malloc(Context->Width);
    if (Line == NULL) {
        Status = ENOMEM;
        goto DumpFileEnd;
    }

    FirstLine = TRUE;
    InDuplicate = FALSE;
    while ((Count != 0) && (Context->CurrentInput != NULL)) {
        LineSize = Context->Width;
        if (LineSize > Count) {
            LineSize = Count;
        }

        Status = OdReadBlock(Context, Line, &LineSize);
        if (Status != 0) {
            goto DumpFileEnd;
        }

        assert(LineSize <= Count);

        if (LineSize == 0) {
            break;
        }

        Count -= LineSize;
        if (FirstLine == FALSE) {

            //
            // If this is the same as the last line, then set the boolean but
            // don't print anything.
            //

            if ((LineSize == Context->Width) &&
                (memcmp(Line, PreviousLine, LineSize) == 0)) {

                InDuplicate = TRUE;
                Address += LineSize;
                continue;

            //
            // It's not the same. If there were duplicates before, print out
            // an asterisk now.
            //

            } else if (InDuplicate != FALSE) {
                InDuplicate = FALSE;
                printf("*\n");
            }
        }

        FirstLine = FALSE;
        memcpy(PreviousLine, Line, LineSize);
        OdPrintAddress(Context, Address);
        Address += LineSize;
        for (FormatIndex = 0;
             FormatIndex < Context->FormatCount;
             FormatIndex += 1) {

            OdDumpFormat(Context,
                         Line,
                         LineSize,
                         &(Context->Formats[FormatIndex]));

            printf("\n");
            if ((FormatIndex != Context->FormatCount - 1) &&
                (Context->AddressWidth != 0)) {

                printf("%*s ", Context->AddressWidth, "");
            }
        }
    }

    OdPrintAddress(Context, Address);
    printf("\n");

DumpFileEnd:
    return Status;
}

INT
OdReadBlock (
    POD_CONTEXT Context,
    PCHAR Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine reads from the input sequence.

Arguments:

    Context - Supplies a pointer to the application context.

    Buffer - Supplies a pointer to the buffer where the data will be returned
        on success.

    BufferSize - Supplies a pointer that on input contains the number of bytes
        to read. On output the number of bytes actually put in the buffer will
        be returned.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    INT Character;
    ULONG Count;
    INT Result;

    Count = 0;
    while (Count < *BufferSize) {
        Character = fgetc(Context->CurrentInput->File);
        if (Character == EOF) {
            if (ferror(Context->CurrentInput->File)) {
                Result = errno;
                SwPrintError(Result,
                             Context->CurrentInput->Name,
                             "Failed to read");

                return Result;

            } else if (feof(Context->CurrentInput->File)) {
                if (Context->CurrentInput->ListEntry.Next ==
                    &(Context->InputList)) {

                    Context->CurrentInput = NULL;
                    break;
                }

                Context->CurrentInput = LIST_VALUE(
                                         Context->CurrentInput->ListEntry.Next,
                                         OD_INPUT_ENTRY,
                                         ListEntry);
            }

        } else {
            if (Buffer != NULL) {
                Buffer[Count] = Character;
            }

            Count += 1;
        }
    }

    *BufferSize = Count;
    Result = 0;
    return Result;
}

VOID
OdPrintAddress (
    POD_CONTEXT Context,
    ULONGLONG Address
    )

/*++

Routine Description:

    This routine prints a file offset.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address to print.

Return Value:

    None.

--*/

{

    if (Context->AddressRadix == 0) {
        return;

    } else if (Context->AddressRadix == 8) {
        printf("%0*llo ", Context->AddressWidth, Address);

    } else if (Context->AddressRadix == 10) {
        printf("%0*lld ", Context->AddressWidth, Address);

    } else if (Context->AddressRadix == 16) {
        printf("%0*llx ", Context->AddressWidth, Address);

    } else {

        assert(FALSE);

    }

    return;
}

VOID
OdDumpFormat (
    POD_CONTEXT Context,
    PCHAR Line,
    ULONGLONG LineSize,
    POD_OUTPUT_FORMAT Format
    )

/*++

Routine Description:

    This routine prints a line of output using the specified format.

Arguments:

    Context - Supplies a pointer to the application context.

    Line - Supplies a pointer to the line buffer.

    LineSize - Supplies the size of the line in bytes.

    Format - Supplies a pointer to the format to use.

Return Value:

    None.

--*/

{

    INT Character;
    double Double;
    float Float;
    ULONGLONG Integer;
    long double LongDouble;
    ULONG ValueSize;

    while (LineSize != 0) {
        if (Format->OutputType == OdOutputFloat) {
            memset(&Float, 0, sizeof(Float));
            ValueSize = sizeof(float);
            if (ValueSize > LineSize) {
                ValueSize = LineSize;
            }

            memcpy(&Float, Line, ValueSize);
            printf("%*.6e ", Format->Width, (double)Float);

        } else if (Format->OutputType == OdOutputDouble) {
            memset(&Double, 0, sizeof(double));
            ValueSize = sizeof(double);
            if (ValueSize > LineSize) {
                ValueSize = LineSize;
            }

            memcpy(&Double, Line, ValueSize);
            printf("%*.15e ", Format->Width, Double);

        } else if (Format->OutputType == OdOutputLongDouble) {
            memset(&LongDouble, 0, sizeof(long double));
            ValueSize = sizeof(long double);
            if (ValueSize > LineSize) {
                ValueSize = LineSize;
            }

            memcpy(&LongDouble, Line, ValueSize);
            printf("%*.15Le ", Format->Width, LongDouble);

        } else if (Format->OutputType == OdOutputCharacter) {
            Character = *Line;
            ValueSize = 1;
            if (Character == '\0') {
                printf("%3s ", "\\0");

            } else if (Character == '\a') {
                printf("%3s ", "\\a");

            } else if (Character == '\b') {
                printf("%3s ", "\\b");

            } else if (Character == '\f') {
                printf("%3s ", "\\f");

            } else if (Character == '\n') {
                printf("%3s ", "\\n");

            } else if (Character == '\r') {
                printf("%3s ", "\\r");

            } else if (Character == '\t') {
                printf("%3s ", "\\t");

            } else if (Character == '\v') {
                printf("%3s ", "\\v");

            } else if (!isprint(Character)) {
                printf("\\%03o", (UCHAR)Character);

            } else {
                printf("%3c ", Character);
            }

        } else if (Format->OutputType == OdOutputNamedCharacter) {
            Character = *Line;
            ValueSize = 1;
            if (Character < OD_NAMED_CHARACTERS) {
                printf("%3s ", OdCharacterNames[Character]);

            } else if (!isprint(Character)) {
                printf("\\%03o", (UCHAR)Character);

            } else {
                printf("%3c ", Character);
            }

        } else {

            assert((Format->OutputType == OdOutputSignedInteger) ||
                   (Format->OutputType == OdOutputUnsignedInteger));

            assert(Format->Size <= sizeof(ULONGLONG));

            Integer = 0;
            ValueSize = Format->Size;
            if (ValueSize > LineSize) {
                ValueSize = LineSize;
            }

            assert(ValueSize != 0);

            memcpy(&Integer, Line, ValueSize);
            switch (Format->Radix) {
            case 8:
                printf("%0*llo ", Format->Width, Integer);
                break;

            case 10:
                if (Format->OutputType == OdOutputSignedInteger) {
                    printf("%*lld ", Format->Width, Integer);

                } else {
                    printf("%*llu ", Format->Width, Integer);
                }

                break;

            case 16:
                printf("%0*llx ", Format->Width, Integer);
                break;

            default:

                assert(FALSE);

                break;
            }
        }

        Line += ValueSize;
        LineSize -= ValueSize;
    }

    return;
}

