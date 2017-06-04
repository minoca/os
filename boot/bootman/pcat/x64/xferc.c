/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    xferc.c

Abstract:

    This module implements the trampoline that transfers control to a 32 or 64
    bit boot application.

Author:

    Evan Green 31-May-2017

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86.h>
#include "firmware.h"
#include "bootlib.h"
#include "../../bootman.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
BmpFwTransferTo64BitApplication (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_APPLICATION_ENTRY EntryPoint,
    ULONG PageDirectory
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
BmpFwTransferToBootApplication (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_APPLICATION_ENTRY EntryPoint
    )

/*++

Routine Description:

    This routine transfers control to another boot application.

Arguments:

    Parameters - Supplies a pointer to the initialization block.

    EntryPoint - Supplies tne address of the entry point routine of the new
        application.

Return Value:

    Returns the integer return value from the application. Often does not
    return on success.

--*/

{

    BOOL CanDoIt;
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    INT Result;

    //
    // If this is a 32-bit application, just call the function directly, no
    // acrobatics needed.
    //

    if ((Parameters->Flags & BOOT_INITIALIZATION_FLAG_64BIT) == 0) {
        return EntryPoint(Parameters);
    }

    //
    // See if cpuid exposes the required leaf.
    //

    CanDoIt = FALSE;
    Eax = X86_CPUID_EXTENDED_IDENTIFICATION;
    ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
    if (Eax >= X86_CPUID_EXTENDED_INFORMATION) {

        //
        // See if long mode is supported.
        //

        Eax = X86_CPUID_EXTENDED_INFORMATION;
        ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
        if ((Edx & X86_CPUID_EXTENDED_INFORMATION_EDX_LONG_MODE) != 0) {
            CanDoIt = TRUE;
        }
    }

    if (CanDoIt == FALSE) {
        FwPrintString(0, 0, "CPU is not 64-bit");
        return STATUS_NOT_SUPPORTED;
    }

    Result = BmpFwTransferTo64BitApplication(
                                           Parameters,
                                           EntryPoint,
                                           (ULONG)(Parameters->PageDirectory));

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

