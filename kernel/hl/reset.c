/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>
#include "hlp.h"
#include "efi.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to try and acquire the reboot lock, in seconds.
//

#define REBOOT_LOCK_TIMEOUT 5

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context passed to a reset system DPC.

Members:

    ResetType - Stores the reset type to perform.

    Data - Stores the platform-specific reboot data.

    Size - Stores the size fo the platform-specific reboot data in bytes.

    Status - Stores the resulting status code.

--*/

typedef struct _RESET_SYSTEM_DPC_DATA {
    SYSTEM_RESET_TYPE ResetType;
    PVOID Data;
    UINTN Size;
    KSTATUS Status;
} RESET_SYSTEM_DPC_DATA, *PRESET_SYSTEM_DPC_DATA;

/*++

Structure Description:

    This structure is used to describe a reboot controller to the system.
    It is passed from the hardware module to the kernel.

Members:

    ListEntry - Stores pointers to the next and previous reboot controllers in
        the list.

    FunctionTable - Stores the table of pointers to the hardware module's
        functions.

    Context - Stores a pointer's worth of data specific to this cache
        controller instance. This pointer will be passed back to the hardware
        module on each call.

    Identifier - Stores the unique identifier of the reboot controller.

    Properties - Stores a bitfield of flags describing the reboot controller.
        See REBOOT_MODULE_* definitions.

--*/

typedef struct _REBOOT_MODULE {
    LIST_ENTRY ListEntry;
    REBOOT_MODULE_FUNCTION_TABLE FunctionTable;
    PVOID Context;
    ULONG Identifier;
    ULONG Properties;
} REBOOT_MODULE, *PREBOOT_MODULE;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
HlpResetSystemDpc (
    PDPC Dpc
    );

KSTATUS
HlpResetSystem (
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    );

KSTATUS
HlpRebootViaController (
    PREBOOT_MODULE RebootModule,
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the list of registered reboot controllers.
//

LIST_ENTRY HlRebootModules;

//
// Store a spin lock to synchronize access to the reboot controller list.
//

KSPIN_LOCK HlRebootLock;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlResetSystem (
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

    Data - Supplies a pointer to platform-specific reboot data.

    Size - Supplies the size of the platform-specific data in bytes.

Return Value:

    Does not return on success, the system is reset.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NO_INTERFACE if there are no appropriate reboot capababilities
    registered with the system.

    Other status codes on other failures.

--*/

{

    BOOL Acquired;
    PLIST_ENTRY CurrentEntry;
    PDPC Dpc;
    RESET_SYSTEM_DPC_DATA DpcData;
    PREBOOT_PREPARE PrepareFunction;
    PREBOOT_MODULE RebootModule;
    KSTATUS Status;
    ULONGLONG Timeout;

    //
    // If this is being called from a hostile environment, just attempt the
    // reset directly.
    //

    if ((ArAreInterruptsEnabled() == FALSE) ||
        (KeGetRunLevel() != RunLevelLow)) {

        return HlpResetSystem(ResetType, Data, Size);
    }

    //
    // Try to acquire the low level spin lock, but just continue if it can't be
    // acquired in a timely manner.
    //

    Acquired = FALSE;
    Timeout = 0;
    while (TRUE) {
        if (KeTryToAcquireSpinLock(&HlRebootLock) != FALSE) {
            Acquired = TRUE;
            break;
        }

        if (Timeout == 0) {
            Timeout = HlQueryTimeCounter() +
                      (HlQueryTimeCounterFrequency() * REBOOT_LOCK_TIMEOUT);

        } else {
            if (HlQueryTimeCounter() >= Timeout) {
                break;
            }
        }
    }

    //
    // Call the prepare function of all controllers.
    //

    CurrentEntry = HlRebootModules.Next;
    while (CurrentEntry != &HlRebootModules) {
        RebootModule = LIST_VALUE(CurrentEntry, REBOOT_MODULE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (RebootModule->FunctionTable.Prepare != NULL) {
            PrepareFunction = RebootModule->FunctionTable.Prepare;
            Status = PrepareFunction(RebootModule->Context, ResetType);
            if (!KSUCCESS(Status)) {
                RtlDebugPrint("Failed to prepare for reset: %d\n", Status);
            }
        }
    }

    //
    // Loop through and try all the low-level reset controllers.
    //

    Status = STATUS_NO_INTERFACE;
    CurrentEntry = HlRebootModules.Next;
    while (CurrentEntry != &HlRebootModules) {
        RebootModule = LIST_VALUE(CurrentEntry, REBOOT_MODULE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((RebootModule->Properties & REBOOT_MODULE_LOW_LEVEL) == 0) {
            continue;
        }

        Status = HlpRebootViaController(RebootModule, ResetType, Data, Size);
    }

    //
    // Create a DPC so that the reset code runs on processor zero.
    //

    RtlZeroMemory(&DpcData, sizeof(RESET_SYSTEM_DPC_DATA));
    DpcData.ResetType = ResetType;
    DpcData.Status = Status;
    DpcData.Data = Data;
    DpcData.Size = Size;
    Dpc = KeCreateDpc(HlpResetSystemDpc, &DpcData);

    //
    // If DPC creation failed, the system is in a bad way. Skip the niceties
    // go for the reset directly.
    //

    if (Dpc == NULL) {
        return HlpResetSystem(ResetType, Data, Size);
    }

    KeQueueDpcOnProcessor(Dpc, 0);

    //
    // Wait for the DPC to finish.
    //

    KeFlushDpc(Dpc);
    KeDestroyDpc(Dpc);
    if (Acquired != FALSE) {
        KeReleaseSpinLock(&HlRebootLock);
    }

    return DpcData.Status;
}

KSTATUS
HlpInitializeRebootModules (
    VOID
    )

/*++

Routine Description:

    This routine initializes the reboot modules support.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    if (KeGetCurrentProcessorNumber() == 0) {
        KeInitializeSpinLock(&HlRebootLock);
        INITIALIZE_LIST_HEAD(&HlRebootModules);
    }

    return STATUS_SUCCESS;
}

KSTATUS
HlpRebootModuleRegisterHardware (
    PREBOOT_MODULE_DESCRIPTION Description
    )

/*++

Routine Description:

    This routine is called to register a new reboot module with the system.

Arguments:

    Description - Supplies a pointer to a structure describing the new
        reboot module.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PREBOOT_MODULE Module;
    KSTATUS Status;

    Module = NULL;

    //
    // Check the table version.
    //

    if (Description->TableVersion < REBOOT_MODULE_DESCRIPTION_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto RebootModuleRegisterHardwareEnd;
    }

    //
    // Check required function pointers.
    //

    if (Description->FunctionTable.Reboot == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto RebootModuleRegisterHardwareEnd;
    }

    //
    // Allocate the new controller object.
    //

    AllocationSize = sizeof(REBOOT_MODULE);
    Module = MmAllocateNonPagedPool(AllocationSize, HL_POOL_TAG);
    if (Module == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RebootModuleRegisterHardwareEnd;
    }

    RtlZeroMemory(Module, AllocationSize);

    //
    // Initialize the new timer based on the description.
    //

    RtlCopyMemory(&(Module->FunctionTable),
                  &(Description->FunctionTable),
                  sizeof(CALENDAR_TIMER_FUNCTION_TABLE));

    Module->Identifier = Description->Identifier;
    Module->Context = Description->Context;
    Module->Properties = Description->Properties;

    //
    // Insert the controller on the list. The runlevel should be low normally,
    // except perhaps during early boot. It may be dispatch, but the system is
    // running single-threaded at that point.
    //

    KeAcquireSpinLock(&HlRebootLock);
    INSERT_BEFORE(&(Module->ListEntry), &HlRebootModules);
    KeReleaseSpinLock(&HlRebootLock);
    Status = STATUS_SUCCESS;

RebootModuleRegisterHardwareEnd:
    return Status;
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
    Data->Status = HlpResetSystem(Data->ResetType, Data->Data, Data->Size);
    return;
}

KSTATUS
HlpResetSystem (
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

    Data - Supplies a pointer to platform-specific reboot data.

    Size - Supplies the size of the platform-specific data in bytes.

Return Value:

    Does not return on success, the system is reset.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NO_INTERFACE if there was no mechanism available to reset the system.

    Other error codes on other failures.

--*/

{

    PLIST_ENTRY CurrentEntry;
    KSTATUS EfiStatus;
    PREBOOT_MODULE RebootModule;
    KSTATUS Status;

    if ((ResetType == SystemResetInvalid) ||
        (ResetType >= SystemResetTypeCount)) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    // Reboot via any registered controller that doesn't require low level.
    //

    Status = STATUS_NO_INTERFACE;
    CurrentEntry = HlRebootModules.Next;
    while (CurrentEntry != &HlRebootModules) {
        RebootModule = LIST_VALUE(CurrentEntry, REBOOT_MODULE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((RebootModule->Properties & REBOOT_MODULE_LOW_LEVEL) != 0) {
            continue;
        }

        Status = HlpRebootViaController(RebootModule, ResetType, Data, Size);
    }

    //
    // If this is an EFI system, try to use firmware services to shut down.
    //

    EfiStatus = HlpEfiResetSystem(ResetType);
    if (EfiStatus != STATUS_NOT_SUPPORTED) {
        Status = EfiStatus;
    }

    //
    // Try some innate tricks to reset. PCs have several of these tricks, other
    // systems have none.
    //

    HlpArchResetSystem(ResetType);
    return Status;
}

KSTATUS
HlpRebootViaController (
    PREBOOT_MODULE RebootModule,
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine resets the system via a registered reboot controller.

Arguments:

    RebootModule - Supplies a pointer to the reboot module to use to reset
        the system.

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

    Data - Supplies a pointer to the platform specific data.

    Size - Supplies the size of the platform specific data.

Return Value:

    Does not return on success, the system is reset.

    Returns a status code on failure.

--*/

{

    PREBOOT_SYSTEM Reboot;
    KSTATUS Status;

    Reboot = RebootModule->FunctionTable.Reboot;
    Status = Reboot(RebootModule->Context, ResetType, Data, Size);
    HlBusySpin(RESET_SYSTEM_STALL);
    return Status;
}

