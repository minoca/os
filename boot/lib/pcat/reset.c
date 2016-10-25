/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    reset.c

Abstract:

    This module implements support for resettting an PC/AT BIOS machine in the
    boot environment.

Author:

    Evan Green 16-Apr-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86.h>
#include <minoca/kernel/ioport.h>
#include "firmware.h"
#include "bootlib.h"
#include "realmode.h"
#include "bios.h"

//
// ---------------------------------------------------------------- Definitions
//

#define RESET_8042_TRY_COUNT 100000

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

KSTATUS
FwPcatResetSystem (
    SYSTEM_RESET_TYPE ResetType
    )

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

Return Value:

    Does not return on success, the system is reset.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NOT_SUPPORTED if the system cannot be reset.

    STATUS_UNSUCCESSFUL if the system did not reset.

--*/

{

    TABLE_REGISTER IdtTable;
    ULONG Try;
    UCHAR Value;

    //
    // Attempt to reset via the keyboard controller.
    //

    for (Try = 0; Try < RESET_8042_TRY_COUNT; Try += 1) {
        Value = HlIoPortInByte(PC_8042_CONTROL_PORT);
        if ((Value & PC_8042_INPUT_BUFFER_FULL) == 0) {
            break;
        }
    }

    HlIoPortOutByte(PC_8042_CONTROL_PORT, PC_8042_RESET_VALUE);

    //
    // This is a last ditch effort to reset. This triple faults the system by
    // loading a zero-length IDT entry and then causing an interrupt.
    // It's not ideal though as there are folklore systems out that respond to
    // a triple fault by throwing a bus error and hanging rather than resetting.
    // Please make a note of such system here if it is found.
    //

    ArStoreIdtr(&IdtTable);
    IdtTable.Limit = 0;
    ArLoadIdtr(&IdtTable);

    //
    // A debug break is as good an interrupt as any.
    //

    RtlDebugBreak();
    return STATUS_UNSUCCESSFUL;
}

//
// --------------------------------------------------------- Internal Functions
//

