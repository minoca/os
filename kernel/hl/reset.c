/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    reset.c

Abstract:

    This module implements support for rebooting the system.

Author:

    Evan Green 16-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "hlp.h"
#include "efi.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context passed to a reset system DPC.

Members:

    ResetType - Stores the reset type to perform.

    Status - Stores the resulting status code.

--*/

typedef struct _RESET_SYSTEM_DPC_DATA {
    SYSTEM_RESET_TYPE ResetType;
    KSTATUS Status;
} RESET_SYSTEM_DPC_DATA, *PRESET_SYSTEM_DPC_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
HlpResetSystemDpc (
    PDPC Dpc
    );

KSTATUS
HlpResetSystem (
    SYSTEM_RESET_TYPE ResetType
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlResetSystem (
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

    PDPC Dpc;
    RESET_SYSTEM_DPC_DATA DpcData;

    //
    // If this is being called from a hostile environment, just attempt the
    // reset directly.
    //

    if ((ArAreInterruptsEnabled() == FALSE) ||
        (KeGetRunLevel() != RunLevelLow)) {

        return HlpResetSystem(ResetType);
    }

    //
    // Create a DPC so that the reset code runs on processor zero.
    //

    RtlZeroMemory(&DpcData, sizeof(RESET_SYSTEM_DPC_DATA));
    DpcData.ResetType = ResetType;
    DpcData.Status = STATUS_NOT_STARTED;
    Dpc = KeCreateDpc(HlpResetSystemDpc, &DpcData);

    //
    // If DPC creation failed, the system is in a bad way. Skip the niceties
    // go for the reset directly.
    //

    if (Dpc == NULL) {
        return HlpResetSystem(ResetType);
    }

    KeQueueDpcOnProcessor(Dpc, 0);

    //
    // Wait for the DPC to finish.
    //

    KeFlushDpc(Dpc);
    KeDestroyDpc(Dpc);
    return DpcData.Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
HlpResetSystemDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the reset system DPC that is run on processor zero.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PRESET_SYSTEM_DPC_DATA Data;

    ASSERT((KeGetRunLevel() == RunLevelDispatch) &&
           (KeGetCurrentProcessorNumber() == 0));

    Data = Dpc->UserData;
    Data->Status = HlpResetSystem(Data->ResetType);
    return;
}

KSTATUS
HlpResetSystem (
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

    KSTATUS ArchStatus;
    KSTATUS Status;

    if ((ResetType == SystemResetInvalid) ||
        (ResetType >= SystemResetTypeCount)) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    // If this is an EFI system, try to use firmware services to shut down.
    //

    Status = HlpEfiResetSystem(ResetType);

    //
    // Try some innate tricks to reset. PCs have several of these tricks, other
    // systems have none.
    //

    ArchStatus = HlpArchResetSystem(ResetType);
    if (Status == STATUS_NOT_SUPPORTED) {
        Status = ArchStatus;
    }

    return Status;
}
