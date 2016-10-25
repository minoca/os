/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.c

Abstract:

    This module implements initialization for the UEFI core. It is called by
    platform-specific portions of the firmware.

Author:

    Evan Green 26-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/kernel/hmod.h>
#include <minoca/kernel/kdebug.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum size of the firmware image name.
//

#define EFI_FIRMWARE_BINARY_NAME_MAX_SIZE 25

//
// Define the size of the EFI loaded module buffer.
//

#define EFI_MODULE_BUFFER_SIZE \
    (sizeof(DEBUG_MODULE) + EFI_FIRMWARE_BINARY_NAME_MAX_SIZE)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_CORE_PROTOCOL_NOTIFY_ENTRY {
    EFI_GUID *ProtocolGuid;
    VOID **Protocol;
    EFI_EVENT Event;
    VOID *Registration;
    BOOLEAN Present;
} EFI_CORE_PROTOCOL_NOTIFY_ENTRY, *PEFI_CORE_PROTOCOL_NOTIFY_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfiCoreExitBootServices (
    EFI_HANDLE ImageHandle,
    UINTN MapKey
    );

EFI_STATUS
EfipCoreRegisterForInterestingNotifies (
    VOID
    );

EFIAPI
VOID
EfipCoreRuntimeArchProtocolNotify (
    EFI_EVENT Event,
    VOID *Context
    );

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable1 (
    UINTN Argument1
    );

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable2 (
    UINTN Argument1,
    UINTN Argument2
    );

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable3 (
    UINTN Argument1,
    UINTN Argument2,
    UINTN Argument3
    );

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable4 (
    UINTN Argument1,
    UINTN Argument2,
    UINTN Argument3,
    UINTN Argument4
    );

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable5 (
    UINTN Argument1,
    UINTN Argument2,
    UINTN Argument3,
    UINTN Argument4,
    UINTN Argument5
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to enable debugging throughout the firmware.
//

BOOL EfiDebugFirmware = FALSE;

//
// Carve out some space for the loaded module structure reported to the
// debugger.
//

UINT8 EfiModuleBuffer[EFI_MODULE_BUFFER_SIZE];

//
// Store the EFI debug device description.
//

extern DEBUG_DEVICE_DESCRIPTION EfiDebugDevice;

//
// Store the runtime handoff information.
//

EFI_RUNTIME_ARCH_PROTOCOL EfiRuntimeProtocolTemplate;
EFI_RUNTIME_ARCH_PROTOCOL *EfiRuntimeProtocol = &EfiRuntimeProtocolTemplate;

//
// Store the image handle of the firmware itself.
//

EFI_HANDLE EfiFirmwareImageHandle;

//
// Define a template for the EFI services.
//

EFI_BOOT_SERVICES EfiBootServicesTemplate = {
    {
        EFI_BOOT_SERVICES_SIGNATURE,
        EFI_BOOT_SERVICES_REVISION,
        sizeof(EFI_BOOT_SERVICES),
        0,
        0
    },

    EfiCoreRaiseTpl,
    EfiCoreRestoreTpl,
    EfiCoreAllocatePages,
    EfiCoreFreePages,
    EfiCoreGetMemoryMap,
    EfiCoreAllocatePool,
    EfiCoreFreePool,
    EfiCoreCreateEvent,
    EfiCoreSetTimer,
    EfiCoreWaitForEvent,
    EfiCoreSignalEvent,
    EfiCoreCloseEvent,
    EfiCoreCheckEvent,
    EfiCoreInstallProtocolInterface,
    EfiCoreReinstallProtocolInterface,
    EfiCoreUninstallProtocolInterface,
    EfiCoreHandleProtocol,
    NULL,
    EfiCoreRegisterProtocolNotify,
    EfiCoreLocateHandle,
    EfiCoreLocateDevicePath,
    EfiCoreInstallConfigurationTable,
    EfiCoreLoadImage,
    EfiCoreStartImage,
    EfiCoreExit,
    EfiCoreUnloadImage,
    EfiCoreExitBootServices,
    EfiCoreGetNextMonotonicCount,
    EfiCoreStall,
    EfiCoreSetWatchdogTimer,
    EfiCoreConnectController,
    EfiCoreDisconnectController,
    EfiCoreOpenProtocol,
    EfiCoreCloseProtocol,
    EfiCoreOpenProtocolInformation,
    EfiCoreProtocolsPerHandle,
    EfiCoreLocateHandleBuffer,
    EfiCoreLocateProtocol,
    EfiCoreInstallMultipleProtocolInterfaces,
    EfiCoreUninstallMultipleProtocolInterfaces,
    (EFI_CALCULATE_CRC32)EfiCoreNotYetAvailable3,
    EfiCoreCopyMemory,
    EfiCoreSetMemory,
    EfiCoreCreateEventEx
};

EFI_RUNTIME_SERVICES EfiRuntimeServicesTemplate = {
    {
        EFI_RUNTIME_SERVICES_SIGNATURE,
        EFI_RUNTIME_SERVICES_REVISION,
        sizeof(EFI_RUNTIME_SERVICES),
        0,
        0
    },

    (EFI_GET_TIME)EfiCoreNotYetAvailable2,
    (EFI_SET_TIME)EfiCoreNotYetAvailable1,
    (EFI_GET_WAKEUP_TIME)EfiCoreNotYetAvailable3,
    (EFI_SET_WAKEUP_TIME)EfiCoreNotYetAvailable2,
    (EFI_SET_VIRTUAL_ADDRESS_MAP)EfiCoreNotYetAvailable4,
    (EFI_CONVERT_POINTER)EfiCoreNotYetAvailable2,
    (EFI_GET_VARIABLE)EfiCoreNotYetAvailable5,
    (EFI_GET_NEXT_VARIABLE_NAME)EfiCoreNotYetAvailable3,
    (EFI_SET_VARIABLE)EfiCoreNotYetAvailable5,
    (EFI_GET_NEXT_HIGH_MONO_COUNT)EfiCoreNotYetAvailable1,
    (EFI_RESET_SYSTEM)EfiCoreNotYetAvailable4,
    (EFI_UPDATE_CAPSULE)EfiCoreNotYetAvailable3,
    (EFI_QUERY_CAPSULE_CAPABILITIES)EfiCoreNotYetAvailable4,
    (EFI_QUERY_VARIABLE_INFO)EfiCoreNotYetAvailable4
};

//
// Define pointers to the true system table and firmware services.
//

EFI_SYSTEM_TABLE *EfiSystemTable;
EFI_BOOT_SERVICES *EfiBootServices = &EfiBootServicesTemplate;
EFI_RUNTIME_SERVICES *EfiRuntimeServices = &EfiRuntimeServicesTemplate;

//
// Define the protocols the core is interested in hearing about.
//

EFI_GUID EfiRuntimeArchProtocolGuid = EFI_RUNTIME_ARCH_PROTOCOL_GUID;
EFI_CORE_PROTOCOL_NOTIFY_ENTRY EfiRuntimeProtocolNotifyEntry = {
    &EfiRuntimeArchProtocolGuid,
    (VOID **)&EfiRuntimeProtocol,
    NULL,
    NULL,
    FALSE
};

//
// ------------------------------------------------------------------ Functions
//

VOID
EfiCoreMain (
    VOID *FirmwareBaseAddress,
    VOID *FirmwareLowestAddress,
    UINTN FirmwareSize,
    CHAR8 *FirmwareBinaryName,
    VOID *StackBase,
    UINTN StackSize
    )

/*++

Routine Description:

    This routine implements the entry point into the UEFI firmware. This
    routine is called by the platform firmware, and should be called as early
    as possible. It will perform callouts to allow the platform to initialize
    further.

Arguments:

    FirmwareBaseAddress - Supplies the base address where the firmware was
        loaded into memory. Supply -1 to indicate that the image is loaded at
        its preferred base address and was not relocated.

    FirmwareLowestAddress - Supplies the lowest address where the firmware was
        loaded into memory.

    FirmwareSize - Supplies the size of the firmware image in memory, in bytes.

    FirmwareBinaryName - Supplies the name of the binary that's loaded, which
        is reported to the debugger for symbol loading.

    StackBase - Supplies the base (lowest) address of the stack.

    StackSize - Supplies the size in bytes of the stack. This should be at
        least 0x4000 bytes (16kB).

Return Value:

    This routine does not return.

--*/

{

    PDEBUG_MODULE DebugModule;
    EFI_STATUS EfiStatus;
    KSTATUS KStatus;
    UINTN ModuleNameLength;
    ULONG OriginalTimeout;
    UINTN Step;
    EFI_TIME Time;

    EfiStatus = EFI_SUCCESS;
    OriginalTimeout = 0;
    KStatus = STATUS_SUCCESS;
    Step = 0;

    //
    // Perform very basic processor initialization, preparing it to take
    // exceptions and use the serial port.
    //

    EfipInitializeProcessor();
    Step += 1;
    DebugModule = (PDEBUG_MODULE)EfiModuleBuffer;

    //
    // Initialize the debugging subsystem.
    //

    RtlZeroMemory(&EfiModuleBuffer, sizeof(EfiModuleBuffer));
    ModuleNameLength = RtlStringLength(FirmwareBinaryName) + 1;
    if (ModuleNameLength > EFI_FIRMWARE_BINARY_NAME_MAX_SIZE) {
        ModuleNameLength = EFI_FIRMWARE_BINARY_NAME_MAX_SIZE;
    }

    DebugModule->StructureSize = sizeof(DEBUG_MODULE) + ModuleNameLength -
                                 (ANYSIZE_ARRAY * sizeof(CHAR));

    RtlStringCopy(DebugModule->BinaryName,
                  FirmwareBinaryName,
                  ModuleNameLength);

    DebugModule->LowestAddress = FirmwareLowestAddress;
    DebugModule->Size = FirmwareSize;
    if (EfiDebugFirmware != FALSE) {

        //
        // Stall does not work this early, so prevent KD from using it.
        //

        OriginalTimeout = KdSetConnectionTimeout(MAX_ULONG);
        KStatus = KdInitialize(&EfiDebugDevice, DebugModule);
        if (!KSUCCESS(KStatus)) {
            goto InitializeEnd;
        }
    }

    //
    // Initialize the runtime protocol template.
    //

    Step += 1;
    INITIALIZE_LIST_HEAD(&(EfiRuntimeProtocol->ImageListHead));
    INITIALIZE_LIST_HEAD(&(EfiRuntimeProtocol->EventListHead));
    EfiRuntimeProtocol->MemoryDescriptorSize =
                              sizeof(EFI_MEMORY_DESCRIPTOR) + sizeof(UINT64) -
                              (sizeof(EFI_MEMORY_DESCRIPTOR) % sizeof(UINT64));

    EfiRuntimeProtocol->MemoryDescriptorVersion = EFI_MEMORY_DESCRIPTOR_VERSION;

    //
    // Allow the platform to do some initialization now that code is
    // debuggable.
    //

    EfiStatus = EfiPlatformInitialize(0);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiCoreInitializeHandleDatabase();
    EfiStatus = EfiCoreInitializeEventServices(0);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiCoreInitializeMemoryServices(FirmwareLowestAddress,
                                                FirmwareSize,
                                                StackBase,
                                                StackSize);

    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiCoreInitializeEventServices(1);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiCoreInitializeInterruptServices();
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiCoreInitializeTimerServices();
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    //
    // Create the runtime services table.
    //

    Step += 1;
    EfiBootServices = &EfiBootServicesTemplate;
    EfiRuntimeServices =
                      EfiCoreAllocateRuntimePool(sizeof(EFI_RUNTIME_SERVICES));

    if (EfiRuntimeServices == NULL) {
        goto InitializeEnd;
    }

    EfiCoreCopyMemory(EfiRuntimeServices,
                      &EfiRuntimeServicesTemplate,
                      sizeof(EFI_RUNTIME_SERVICES));

    //
    // Create the system table.
    //

    Step += 1;
    EfiSystemTable = EfiCoreAllocateRuntimePool(sizeof(EFI_SYSTEM_TABLE));
    if (EfiSystemTable == NULL) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiCoreSetMemory(EfiSystemTable, sizeof(EFI_SYSTEM_TABLE), 0);
    EfiSystemTable->Hdr.Signature = EFI_SYSTEM_TABLE_SIGNATURE;
    EfiSystemTable->Hdr.Revision = EFI_SYSTEM_TABLE_REVISION;
    EfiSystemTable->Hdr.HeaderSize = sizeof(EFI_SYSTEM_TABLE);
    EfiSystemTable->Hdr.CRC32 = 0;
    EfiSystemTable->Hdr.Reserved = 0;
    EfiSystemTable->BootServices = EfiBootServices;
    EfiSystemTable->RuntimeServices = EfiRuntimeServices;

    //
    // Allow KD to use stall now that timer services are set up.
    //

    if (EfiDebugFirmware != FALSE) {
        KdSetConnectionTimeout(OriginalTimeout);
    }

    EfiStatus = EfiCoreInitializeImageServices(FirmwareBaseAddress,
                                               FirmwareLowestAddress,
                                               FirmwareSize);

    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfipCoreRegisterForInterestingNotifies();
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiFvInitializeSectionExtraction(EfiFirmwareImageHandle,
                                                 EfiSystemTable);

    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiFvInitializeBlockSupport(EfiFirmwareImageHandle,
                                            EfiSystemTable);

    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiPlatformInitialize(1);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiFvDriverInit(EfiFirmwareImageHandle, EfiSystemTable);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    //
    // Initialize builtin drivers.
    //

    Step += 1;
    EfiStatus = EfiDiskIoDriverEntry(NULL, EfiSystemTable);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiPartitionDriverEntry(NULL, EfiSystemTable);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiFatDriverEntry(NULL, EfiSystemTable);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiGraphicsTextDriverEntry(NULL, EfiSystemTable);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    //
    // The EFI core is up, tell the platform to enumerate any firmware volumes,
    // followed by any devices.
    //

    Step += 1;
    EfiStatus = EfiPlatformEnumerateFirmwareVolumes();
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    EfiCoreInitializeDispatcher();
    EfiCoreDispatcher();

    //
    // Now that the firmware volumes are up, install any ACPI tables found in
    // them.
    //

    Step += 1;
    EfiStatus = EfiAcpiDriverEntry(NULL, EfiSystemTable);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiSmbiosDriverEntry(NULL, EfiSystemTable);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    //
    // Ask the platform to enumerate any builtin devices it knows about.
    //

    Step += 1;
    EfiStatus = EfiPlatformEnumerateDevices();
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    Step += 1;
    EfiStatus = EfiPlatformInitialize(2);
    if (EFI_ERROR(EfiStatus)) {
        goto InitializeEnd;
    }

    //
    // Let's get the time, just for kicks.
    //

    EfiStatus = EfiGetTime(&Time, NULL);
    if (!EFI_ERROR(EfiStatus)) {
        RtlDebugPrint("%d/%d/%d %02d:%02d:%02d\n",
                      Time.Month,
                      Time.Day,
                      Time.Year,
                      Time.Hour,
                      Time.Minute,
                      Time.Second);
    }

    //
    // Here we go, let's boot this thing.
    //

    Step += 1;
    EfiBdsEntry();

InitializeEnd:

    //
    // Never return.
    //

    RtlDebugPrint("EFI firmware failed. KStatus %d, EFI Status 0x%x, Step %d\n",
                  KStatus,
                  EfiStatus,
                  Step);

    while (TRUE) {
        RtlDebugBreak();
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfiCoreExitBootServices (
    EFI_HANDLE ImageHandle,
    UINTN MapKey
    )

/*++

Routine Description:

    This routine terminates all boot services.

Arguments:

    ImageHandle - Supplies the handle that identifies the exiting image.

    MapKey - Supplies the latest memory map key.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the map key is incorrect.

--*/

{

    EFI_STATUS Status;

    Status = EfiCoreTerminateMemoryServices(MapKey);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiSetWatchdogTimer(0, 0, 0, NULL);
    EfiCoreTerminateTimerServices();
    EfiCoreTerminateInterruptServices();
    EfipCoreNotifySignalList(&EfiEventExitBootServicesGuid);
    EfiDisableInterrupts();

    //
    // Remove the boot services from the system table and recalculate the CRC.
    //

    EfiSystemTable->BootServices = NULL;
    EfiSystemTable->ConIn = NULL;
    EfiSystemTable->ConsoleInHandle = NULL;
    EfiSystemTable->ConOut = NULL;
    EfiSystemTable->ConsoleOutHandle = NULL;
    EfiSystemTable->StdErr = NULL;
    EfiSystemTable->StandardErrorHandle = NULL;
    EfiCoreCalculateTableCrc32(&(EfiSystemTable->Hdr));
    EfiSetMem(EfiBootServices, sizeof(EFI_BOOT_SERVICES), 0);
    EfiBootServices = NULL;
    EfiRuntimeProtocol->AtRuntime = TRUE;
    return Status;
}

EFI_STATUS
EfipCoreRegisterForInterestingNotifies (
    VOID
    )

/*++

Routine Description:

    This routine signs up for registration of notifications for protocols the
    UEFI core is interested in.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    EFI_STATUS Status;

    Status = EfiCoreCreateEvent(EVT_NOTIFY_SIGNAL,
                                TPL_CALLBACK,
                                EfipCoreRuntimeArchProtocolNotify,
                                &EfiRuntimeProtocolNotifyEntry,
                                &(EfiRuntimeProtocolNotifyEntry.Event));

    if (EFI_ERROR(Status)) {

        ASSERT(FALSE);

        return Status;
    }

    Status = EfiCoreRegisterProtocolNotify(
                                EfiRuntimeProtocolNotifyEntry.ProtocolGuid,
                                EfiRuntimeProtocolNotifyEntry.Event,
                                &(EfiRuntimeProtocolNotifyEntry.Registration));

    if (EFI_ERROR(Status)) {

        ASSERT(FALSE);

        return Status;
    }

    return Status;
}

EFIAPI
VOID
EfipCoreRuntimeArchProtocolNotify (
    EFI_EVENT Event,
    VOID *Context
    )

/*++

Routine Description:

    This routine is called when the runtime driver produces the runtime
    architectural protocol.

Arguments:

    Event - Supplies a pointer to the event that fired.

    Context - Supplies a context pointer containing in this case a pointer to
        the core protocl notify entry.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_CORE_PROTOCOL_NOTIFY_ENTRY Entry;
    VOID *Protocol;
    EFI_STATUS Status;

    Entry = (PEFI_CORE_PROTOCOL_NOTIFY_ENTRY)Context;
    Status = EfiCoreLocateProtocol(Entry->ProtocolGuid,
                                   Entry->Registration,
                                   &Protocol);

    if (EFI_ERROR(Status)) {
        return;
    }

    //
    // Mark the entry as present, and update the global variable if one exists.
    //

    Entry->Present = TRUE;
    if (Entry->Protocol != NULL) {
        *(Entry->Protocol) = Protocol;
    }

    if (EfiCoreCompareGuids(Entry->ProtocolGuid, &EfiRuntimeArchProtocolGuid) !=
        FALSE) {

        //
        // Move all the images and events from the temporary template over to
        // the new list.
        //

        while (LIST_EMPTY(&(EfiRuntimeProtocolTemplate.ImageListHead)) ==
               FALSE) {

            CurrentEntry = EfiRuntimeProtocolTemplate.ImageListHead.Next;
            LIST_REMOVE(CurrentEntry);
            INSERT_AFTER(CurrentEntry, &(EfiRuntimeProtocol->ImageListHead));
        }

        while (LIST_EMPTY(&(EfiRuntimeProtocolTemplate.EventListHead)) ==
               FALSE) {

            CurrentEntry = EfiRuntimeProtocolTemplate.EventListHead.Next;
            LIST_REMOVE(CurrentEntry);
            INSERT_AFTER(CurrentEntry, &(EfiRuntimeProtocol->EventListHead));
        }
    }

    //
    // Recalculate the CRCs of the major tables.
    //

    EfiCoreCalculateTableCrc32(&(EfiRuntimeServices->Hdr));
    EfiCoreCalculateTableCrc32(&(EfiBootServices->Hdr));
    EfiCoreCalculateTableCrc32(&(EfiSystemTable->Hdr));
    return;
}

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable1 (
    UINTN Argument1
    )

/*++

Routine Description:

    This routine implements an EFI service stub that simply returns failure.
    This is better than jumping off and executing NULL.

Arguments:

    Argument1 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

Return Value:

    EFI_UNSUPPORTED always.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable2 (
    UINTN Argument1,
    UINTN Argument2
    )

/*++

Routine Description:

    This routine implements an EFI service stub that simply returns failure.
    This is better than jumping off and executing NULL.

Arguments:

    Argument1 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument2 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

Return Value:

    EFI_UNSUPPORTED always.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable3 (
    UINTN Argument1,
    UINTN Argument2,
    UINTN Argument3
    )

/*++

Routine Description:

    This routine implements an EFI service stub that simply returns failure.
    This is better than jumping off and executing NULL.

Arguments:

    Argument1 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument2 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument3 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

Return Value:

    EFI_UNSUPPORTED always.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable4 (
    UINTN Argument1,
    UINTN Argument2,
    UINTN Argument3,
    UINTN Argument4
    )

/*++

Routine Description:

    This routine implements an EFI service stub that simply returns failure.
    This is better than jumping off and executing NULL.

Arguments:

    Argument1 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument2 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument3 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument4 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

Return Value:

    EFI_UNSUPPORTED always.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiCoreNotYetAvailable5 (
    UINTN Argument1,
    UINTN Argument2,
    UINTN Argument3,
    UINTN Argument4,
    UINTN Argument5
    )

/*++

Routine Description:

    This routine implements an EFI service stub that simply returns failure.
    This is better than jumping off and executing NULL.

Arguments:

    Argument1 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument2 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument3 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument4 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

    Argument5 - Supplies an unsed argument to satisfy the prototype of the
        function this routine is stubbing out.

Return Value:

    EFI_UNSUPPORTED always.

--*/

{

    return EFI_UNSUPPORTED;
}

