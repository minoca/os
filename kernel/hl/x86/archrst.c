/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    archrst.c

Abstract:

    This module implements architecture specific system reset support.

Author:

    Evan Green 16-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/ioport.h>
#include <minoca/kernel/x86.h>
#include "../hlp.h"

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
HlpArchResetSystem (
    SYSTEM_RESET_TYPE ResetType
    )

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

Return Value:

    Does not return on success, the system resets.

    STATUS_NOT_SUPPORTED if the system cannot be reset.

    STATUS_UNSUCCESSFUL if the system did not reset.

--*/

{

    PFADT Fadt;
    TABLE_REGISTER IdtTable;
    BOOL ResetRegisterSupported;
    ULONG Try;
    UCHAR Value;

    Fadt = AcpiFindTable(FADT_SIGNATURE, NULL);

    //
    // If there's an FADT, attempt to use the ACPI reset mechanism.
    //

    if (Fadt != NULL) {

        //
        // If the reset register is supported, use that. The ACPI spec says
        // that though the reset register is a generic address, the address
        // width must be 8. If the FADT is new enough, use the flag to
        // indicate support. Otherwise, use the table size and reset value to
        // determine support.
        //

        ResetRegisterSupported = FALSE;
        if ((Fadt->Header.Revision >= 3) &&
            ((Fadt->Flags & FADT_FLAG_RESET_REGISTER_SUPPORTED) != 0)) {

            ResetRegisterSupported = TRUE;

        } else if ((Fadt->Header.Revision == 1) &&
                   (Fadt->Header.Length > FIELD_OFFSET(FADT, ResetValue)) &&
                   (Fadt->ResetValue != 0)) {

            ResetRegisterSupported = TRUE;
        }

        if (ResetRegisterSupported != FALSE) {
            if (Fadt->ResetRegister.AddressSpaceId == AddressSpaceIo) {
                HlIoPortOutByte((USHORT)(Fadt->ResetRegister.Address),
                                Fadt->ResetValue);

                HlBusySpin(RESET_SYSTEM_STALL);
            }
        }
    }

    //
    // Attempt to reset via the keyboard controller, unless ACPI says there
    // is none.
    //

    if ((Fadt == NULL) ||
        (Fadt->Header.Revision <= 1) ||
        ((Fadt->IaBootFlags & FADT_IA_FLAG_8042_PRESENT) != 0)) {

        for (Try = 0; Try < RESET_8042_TRY_COUNT; Try += 1) {
            Value = HlIoPortInByte(PC_8042_CONTROL_PORT);
            if ((Value & PC_8042_INPUT_BUFFER_FULL) == 0) {
                break;
            }
        }

        HlIoPortOutByte(PC_8042_CONTROL_PORT, PC_8042_RESET_VALUE);
        HlBusySpin(RESET_SYSTEM_STALL);
    }

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

    //
    // Execution should really never get here.
    //

    HlBusySpin(RESET_SYSTEM_STALL);
    return STATUS_UNSUCCESSFUL;
}

//
// --------------------------------------------------------- Internal Functions
//

