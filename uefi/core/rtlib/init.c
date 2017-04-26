/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.c

Abstract:

    This module implements the main entry point for the UEFI runtime core.

Author:

    Evan Green 18-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
VOID
EfipRuntimeExitBootServicesNotify (
    EFI_EVENT Event,
    VOID *Context
    );

EFIAPI
VOID
EfipRuntimeVirtualAddressChangeNotify (
    EFI_EVENT Event,
    VOID *Context
    );

EFIAPI
EFI_STATUS
EfipStubGetNextHighMonotonicCount (
    UINT32 *HighCount
    );

EFIAPI
EFI_STATUS
EfipStubUpdateCapsule (
    EFI_CAPSULE_HEADER **CapsuleHeaderArray,
    UINTN CapsuleCount,
    EFI_PHYSICAL_ADDRESS ScatterGatherList
    );

EFIAPI
EFI_STATUS
EfipStubQueryCapsuleCapabilities (
    EFI_CAPSULE_HEADER **CapsuleHeaderArray,
    UINTN CapsuleCount,
    UINT64 *MaximumCapsuleSize,
    EFI_RESET_TYPE *ResetType
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_SYSTEM_TABLE *EfiSystemTable;
EFI_BOOT_SERVICES *EfiBootServices;
EFI_RUNTIME_SERVICES *EfiRuntimeServices;
EFI_HANDLE EfiRuntimeImageHandle;

//
// Store a boolean indicating whether or not the system is in the runtime
// phase.
//

BOOLEAN EfiAtRuntime;

//
// Store information about where and when an assert might have happened, for
// debugging.
//

CONST CHAR8 *EfiRuntimeAssertExpression;
CONST CHAR8 *EfiRuntimeAssertFile;
UINTN EfiRuntimeAssertLine;

//
// Keep the virtual address change and exit boot services events.
//

EFI_EVENT EfiRuntimeExitBootServicesEvent;
EFI_EVENT EfiRuntimeVirtualAddressChangeEvent;

//
// ------------------------------------------------------------------ Functions
//

__USED
EFIAPI
EFI_STATUS
EfiRuntimeCoreEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine implements the entry point into the runtime services core
    driver.

Arguments:

    ImageHandle - Supplies the handle associated with this image.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI_SUCCESS if the driver initialized successfully.

    Other status codes on failure.

--*/

{

    UINT32 Crc;
    EFI_STATUS Status;

    //
    // Save the important data structures globally.
    //

    EfiRuntimeImageHandle = ImageHandle;
    EfiSystemTable = SystemTable;
    EfiBootServices = SystemTable->BootServices;
    EfiRuntimeServices = SystemTable->RuntimeServices;

    //
    // Populate the runtime services handled by the runtime core. Set them
    // before calling platform initialize in case the platform wants to
    // override them.
    //

    EfiRuntimeServices->GetVariable = EfiCoreGetVariable;
    EfiRuntimeServices->SetVariable = EfiCoreSetVariable;
    EfiRuntimeServices->GetNextVariableName = EfiCoreGetNextVariableName;
    EfiRuntimeServices->QueryVariableInfo = EfiCoreQueryVariableInfo;
    EfiRuntimeServices->GetNextHighMonotonicCount =
                                             EfipStubGetNextHighMonotonicCount;

    EfiRuntimeServices->UpdateCapsule = EfipStubUpdateCapsule;
    EfiRuntimeServices->QueryCapsuleCapabilities =
                                              EfipStubQueryCapsuleCapabilities;

    Status = EfiPlatformRuntimeInitialize();
    if (EFI_ERROR(Status)) {
        goto RuntimeCoreEntryEnd;
    }

    //
    // Recompute the table CRC.
    //

    EfiRuntimeServices->Hdr.CRC32 = 0;
    Crc = 0;
    EfiCalculateCrc32(EfiRuntimeServices,
                      EfiRuntimeServices->Hdr.HeaderSize,
                      &Crc);

    EfiRuntimeServices->Hdr.CRC32 = Crc;
    Status = EfipCoreInitializeVariableServices();
    if (EFI_ERROR(Status)) {
        goto RuntimeCoreEntryEnd;
    }

    Status = EfiCreateEvent(EVT_SIGNAL_EXIT_BOOT_SERVICES,
                            TPL_NOTIFY,
                            EfipRuntimeExitBootServicesNotify,
                            NULL,
                            &EfiRuntimeExitBootServicesEvent);

    if (EFI_ERROR(Status)) {
        goto RuntimeCoreEntryEnd;
    }

    Status = EfiCreateEvent(EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                            TPL_NOTIFY,
                            EfipRuntimeVirtualAddressChangeNotify,
                            NULL,
                            &EfiRuntimeVirtualAddressChangeEvent);

    if (EFI_ERROR(Status)) {
        goto RuntimeCoreEntryEnd;
    }

    Status = EFI_SUCCESS;

RuntimeCoreEntryEnd:
    return Status;
}

BOOLEAN
EfiIsAtRuntime (
    VOID
    )

/*++

Routine Description:

    This routine determines whether or not the system has gone through
    ExitBootServices.

Arguments:

    None.

Return Value:

    TRUE if the system has gone past ExitBootServices and is now in the
    Runtime phase.

    FALSE if the system is currently in the Boot phase.

--*/

{

    return EfiAtRuntime;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
VOID
EfipRuntimeExitBootServicesNotify (
    EFI_EVENT Event,
    VOID *Context
    )

/*++

Routine Description:

    This routine does nothing but return. It conforms to the event notification
    function prototype.

Arguments:

    Event - Supplies an unused event.

    Context - Supplies an unused context pointer.

Return Value:

    None.

--*/

{

    EfiPlatformRuntimeExitBootServices();
    EfipCoreVariableHandleExitBootServices();
    EfiAtRuntime = TRUE;
    EfiBootServices = NULL;
    return;
}

EFIAPI
VOID
EfipRuntimeVirtualAddressChangeNotify (
    EFI_EVENT Event,
    VOID *Context
    )

/*++

Routine Description:

    This routine does nothing but return. It conforms to the event notification
    function prototype.

Arguments:

    Event - Supplies an unused event.

    Context - Supplies an unused context pointer.

Return Value:

    None.

--*/

{

    EfiPlatformRuntimeVirtualAddressChange();
    EfipCoreVariableHandleVirtualAddressChange();
    EfiConvertPointer(0, (VOID **)&EfiSystemTable);
    EfiConvertPointer(0, (VOID **)&EfiRuntimeServices);
    return;
}

EFIAPI
EFI_STATUS
EfipStubGetNextHighMonotonicCount (
    UINT32 *HighCount
    )

/*++

Routine Description:

    This routine returns the next high 32 bits of the platform's monotonic
    counter.

Arguments:

    HighCount - Supplies a pointer where the value is returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the count is NULL.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfipStubUpdateCapsule (
    EFI_CAPSULE_HEADER **CapsuleHeaderArray,
    UINTN CapsuleCount,
    EFI_PHYSICAL_ADDRESS ScatterGatherList
    )

/*++

Routine Description:

    This routine passes capsules to the firmware with both virtual and physical
    mapping. Depending on the intended consumption, the firmware may process
    the capsule immediately. If the payload should persist across a system
    reset, the reset value returned from EFI_QueryCapsuleCapabilities must be
    passed into ResetSystem and will cause the capsule to be processed by the
    firmware as part of the reset process.

Arguments:

    CapsuleHeaderArray - Supplies a virtual pointer to an array of virtual
        pointers to the capsules being passed into update capsule.

    CapsuleCount - Supplies the number of pointers to EFI_CAPSULE_HEADERs in
        the capsule header array.

    ScatterGatherList - Supplies an optional physical pointer to a set of
        EFI_CAPSULE_BLOCK_DESCRIPTOR that describes the location in physical
        memory of a set of capsules.

Return Value:

    EFI_SUCCESS if a valid capsule was passed. If
    CAPSULE_FLAGS_PERSIT_ACROSS_RESET is not set, the capsule has been
    successfully processed by the firmware.

    EFI_INVALID_PARAMETER if the capsule size is NULL, the capsule count is
    zero, or an incompatible set of flags were set in the capsule header.

    EFI_DEVICE_ERROR if the capsule update was started, but failed due to a
    device error.

    EFI_UNSUPPORTED if the capsule type is not supported on this platform.

    EFI_OUT_OF_RESOURCES if resources could not be allocated. If this call
    originated during runtime, this error is returned if the caller must retry
    the call during boot services.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfipStubQueryCapsuleCapabilities (
    EFI_CAPSULE_HEADER **CapsuleHeaderArray,
    UINTN CapsuleCount,
    UINT64 *MaximumCapsuleSize,
    EFI_RESET_TYPE *ResetType
    )

/*++

Routine Description:

    This routine returns whether or not the capsule is supported via the
    UpdateCapsule routine.

Arguments:

    CapsuleHeaderArray - Supplies a virtual pointer to an array of virtual
        pointers to the capsules being passed into update capsule.

    CapsuleCount - Supplies the number of pointers to EFI_CAPSULE_HEADERs in
        the capsule header array.

    MaximumCapsuleSize - Supplies a pointer that on output contains the maximum
        size that the update capsule routine can support as an argument to
        the update capsule routine.

    ResetType - Supplies a pointer where the reset type required to perform the
        capsule update is returned.

Return Value:

    EFI_SUCCESS if a valid answer was returned.

    EFI_UNSUPPORTED if the capsule type is not supported on this platform.

    EFI_DEVICE_ERROR if the capsule update was started, but failed due to a
    device error.

    EFI_INVALID_PARAMETER if the maximum capsule size is NULL.

    EFI_OUT_OF_RESOURCES if resources could not be allocated. If this call
    originated during runtime, this error is returned if the caller must retry
    the call during boot services.

--*/

{

    return EFI_UNSUPPORTED;
}

VOID
RtlRaiseAssertion (
    CONST CHAR8 *Expression,
    CONST CHAR8 *SourceFile,
    UINTN SourceLine
    )

/*++

Routine Description:

    This routine raises an assertion failure exception. If a debugger is
    connected, it will attempt to connect to the debugger.

Arguments:

    Expression - Supplies the string containing the expression that failed.

    SourceFile - Supplies the string describing the source file of the failure.

    SourceLine - Supplies the source line number of the failure.

Return Value:

    None.

--*/

{

    //
    // The RTL functions are not linked in here, but this one is referenced by
    // various macros. Mark the assert location for some poor soul trying to
    // debug, but just keep going.
    //

    EfiRuntimeAssertExpression = Expression;
    EfiRuntimeAssertFile = SourceFile;
    EfiRuntimeAssertLine = SourceLine;
    return;
}

VOID
RtlDebugBreak (
    VOID
    )

/*++

Routine Description:

    This routine causes a break into the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

