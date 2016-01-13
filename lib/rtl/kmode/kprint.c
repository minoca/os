/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    kprint.c

Abstract:

    This module implements common printf-like routines in the kernel.

Author:

    Evan Green 24-Jul-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../rtlp.h"
#include <minoca/kdebug.h>

//
// ---------------------------------------------------------------- Definitions
//

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
    PSTR Format,
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

    //
    // Simply pass the data on to the debugger's print function.
    //

    va_start(ArgumentList, Format);
    KdPrintWithArgumentList(Format, ArgumentList);
    va_end(ArgumentList);
    return;
}

