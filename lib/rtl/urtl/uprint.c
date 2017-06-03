/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uprint.c

Abstract:

    This module implements debug print in user mode.

Author:

    Evan Green 24-Jul-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the output handle that debug print statements always come out of. This
// happens to be the standard error file number.
//

#define DEBUG_PRINT_OUTPUT_HANDLE (HANDLE)2

//
// Define the size of the printf conversion buffer.
//

#define DEBUG_PRINT_CONVERSION_BUFFER_SIZE 2048

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

RTL_API
VOID
RtlDebugPrint (
    PCSTR Format,
    ...
    )

/*++

Routine Description:

    This routine prints a printf-style string to the debugger.

Arguments:

    Format - Supplies the printf-style format string to print. The contents of
        this string determine the rest of the arguments passed.

    ... - Supplies any arguments needed to convert the Format string.

Return Value:

    None.

--*/

{

    va_list ArgumentList;
    CHAR Buffer[DEBUG_PRINT_CONVERSION_BUFFER_SIZE];
    UINTN BytesCompleted;
    ULONG StringSize;

    va_start(ArgumentList, Format);
    StringSize = RtlFormatString(Buffer,
                                 sizeof(Buffer),
                                 CharacterEncodingDefault,
                                 Format,
                                 ArgumentList);

    if (StringSize != 0) {
        OsPerformIo(DEBUG_PRINT_OUTPUT_HANDLE,
                    IO_OFFSET_NONE,
                    StringSize,
                    SYS_IO_FLAG_WRITE,
                    SYS_WAIT_TIME_INDEFINITE,
                    Buffer,
                    &BytesCompleted);

        OsDebugPrint(Buffer, StringSize);
    }

    va_end(ArgumentList);
    return;
}

