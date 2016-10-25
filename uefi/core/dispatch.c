/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dispatch.c

Abstract:

    This module implements the driver dispatcher.

Author:

    Evan Green 12-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "fwvolp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_CORE_DRIVER_ENTRY_MAGIC 0x76697244 // 'virD'

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_KNOWN_HANDLE {
    LIST_ENTRY ListEntry;
    EFI_HANDLE Handle;
    EFI_GUID NameGuid;
} EFI_KNOWN_HANDLE, *PEFI_KNOWN_HANDLE;

typedef struct _EFI_CORE_DRIVER_ENTRY {
    UINTN Magic;
    LIST_ENTRY DriverListEntry;
    LIST_ENTRY SchedulerListEntry;
    EFI_HANDLE VolumeHandle;
    EFI_GUID FileName;
    EFI_DEVICE_PATH_PROTOCOL *FileDevicePath;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Volume;
    EFI_HANDLE ImageHandle;
    BOOLEAN IsFirmwareVolumeImage;
    BOOLEAN Untrusted;
    BOOLEAN Initialized;
    BOOLEAN Scheduled;
    BOOLEAN Dependent;
} EFI_CORE_DRIVER_ENTRY, *PEFI_CORE_DRIVER_ENTRY;

typedef struct _EFI_FIRMWARE_VOLUME_FILE_DEVICE_PATH {
    MEDIA_FW_VOL_FILEPATH_DEVICE_PATH File;
    EFI_DEVICE_PATH_PROTOCOL End;
} EFI_FIRMWARE_VOLUME_FILE_DEVICE_PATH, *PEFI_FIRMWARE_VOLUME_FILE_DEVICE_PATH;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
VOID
EfipFirmwareVolumeEventProtocolNotify (
    EFI_EVENT Event,
    VOID *Context
    );

BOOLEAN
EfipFirmwareVolumeHasBeenProcessed (
    EFI_HANDLE Handle
    );

PEFI_KNOWN_HANDLE
EfipMarkFirmwareVolumeProcessed (
    EFI_HANDLE Handle
    );

EFI_STATUS
EfipCoreAddDriverToList (
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Volume,
    EFI_HANDLE VolumeHandle,
    EFI_GUID *DriverName,
    EFI_FV_FILETYPE Type
    );

EFI_DEVICE_PATH_PROTOCOL *
EfipCoreConvertFirmwareVolumeFileToDevicePath (
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Volume,
    EFI_HANDLE VolumeHandle,
    EFI_GUID *DriverName
    );

VOID
EfipCoreInsertOnScheduledQueue (
    PEFI_CORE_DRIVER_ENTRY DriverEntry
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the list of known firmware volume handles.
//

LIST_ENTRY EfiFirmwareVolumeList;

//
// Define the variables used to register for firmware volume arrivals.
//

EFI_EVENT EfiFirmwareVolumeEvent;
VOID *EfiFirmwareVolumeEventRegistration;

//
// Define the list of file types supported by the dispatcher.
//

EFI_FV_FILETYPE EfiDispatcherFileTypes[] = {
    EFI_FV_FILETYPE_DRIVER,
    EFI_FV_FILETYPE_COMBINED_PEIM_DRIVER,
};

EFI_LOCK EfiDispatcherLock;
LIST_ENTRY EfiDiscoveredList;
LIST_ENTRY EfiScheduledQueue;

BOOLEAN EfiDispatcherRunning;

//
// ------------------------------------------------------------------ Functions
//

VOID
EfiCoreInitializeDispatcher (
    VOID
    )

/*++

Routine Description:

    This routine initializes the driver dispatcher.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INITIALIZE_LIST_HEAD(&EfiFirmwareVolumeList);
    INITIALIZE_LIST_HEAD(&EfiDiscoveredList);
    INITIALIZE_LIST_HEAD(&EfiScheduledQueue);
    EfiCoreInitializeLock(&EfiDispatcherLock, TPL_HIGH_LEVEL);
    EfiFirmwareVolumeEvent = EfiCoreCreateProtocolNotifyEvent(
                                         &EfiFirmwareVolume2ProtocolGuid,
                                         TPL_CALLBACK,
                                         EfipFirmwareVolumeEventProtocolNotify,
                                         NULL,
                                         &EfiFirmwareVolumeEventRegistration);

    ASSERT(EfiFirmwareVolumeEvent != NULL);

    return;
}

EFIAPI
EFI_STATUS
EfiCoreDispatcher (
    VOID
    )

/*++

Routine Description:

    This routine runs the driver dispatcher. It drains the scheduled queue
    loading and starting drivers until there are no more drivers to run.

Arguments:

    None.

Return Value:

    EFI_SUCCESS if one or more drivers were loaded.

    EFI_NOT_FOUND if no drivers were loaded.

    EFI_ALREADY_STARTED if the dispatcher is already running.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_CORE_DRIVER_ENTRY DriverEntry;
    BOOLEAN ReadyToRun;
    EFI_STATUS ReturnStatus;
    EFI_STATUS Status;

    if (EfiDispatcherRunning != FALSE) {
        return EFI_ALREADY_STARTED;
    }

    EfiDispatcherRunning = TRUE;
    ReturnStatus = EFI_NOT_FOUND;
    do {

        //
        // Drain the scheduled queue.
        //

        while (LIST_EMPTY(&EfiScheduledQueue) == FALSE) {
            DriverEntry = LIST_VALUE(EfiScheduledQueue.Next,
                                     EFI_CORE_DRIVER_ENTRY,
                                     SchedulerListEntry);

            ASSERT(DriverEntry->Magic == EFI_CORE_DRIVER_ENTRY_MAGIC);

            //
            // Load the driver into memory if needed.
            //

            if ((DriverEntry->ImageHandle == NULL) &&
                (DriverEntry->IsFirmwareVolumeImage == FALSE)) {

                Status = EfiCoreLoadImage(FALSE,
                                          EfiFirmwareImageHandle,
                                          DriverEntry->FileDevicePath,
                                          NULL,
                                          0,
                                          &(DriverEntry->ImageHandle));

                if (EFI_ERROR(Status)) {
                    RtlDebugPrint("Warning: Driver failed load with status "
                                  "0x%x.\n",
                                  Status);

                    EfiCoreAcquireLock(&EfiDispatcherLock);
                    if (Status == EFI_SECURITY_VIOLATION) {
                        DriverEntry->Untrusted = TRUE;

                    } else {
                        DriverEntry->Initialized = TRUE;
                    }

                    DriverEntry->Scheduled = FALSE;
                    LIST_REMOVE(&(DriverEntry->SchedulerListEntry));
                    EfiCoreReleaseLock(&EfiDispatcherLock);

                    //
                    // Don't try to start this image, it failed to load.
                    //

                    continue;
                }
            }

            EfiCoreAcquireLock(&EfiDispatcherLock);
            DriverEntry->Scheduled = FALSE;
            DriverEntry->Initialized = TRUE;
            LIST_REMOVE(&(DriverEntry->SchedulerListEntry));
            EfiCoreReleaseLock(&EfiDispatcherLock);
            if (DriverEntry->IsFirmwareVolumeImage == FALSE) {

                ASSERT(DriverEntry->ImageHandle != NULL);

                Status = EfiCoreStartImage(DriverEntry->ImageHandle,
                                           NULL,
                                           NULL);

                if (EFI_ERROR(Status)) {
                    RtlDebugPrint("Warning: Driver start failed with "
                                  "status 0x%x.\n",
                                  Status);
                }
            }

            ReturnStatus = EFI_SUCCESS;
        }

        //
        // Search the discovered list for items to place on the scheduled
        // queue.
        //

        ReadyToRun = FALSE;
        CurrentEntry = EfiDiscoveredList.Next;
        while (CurrentEntry != &EfiDiscoveredList) {
            DriverEntry = LIST_VALUE(CurrentEntry,
                                     EFI_CORE_DRIVER_ENTRY,
                                     DriverListEntry);

            CurrentEntry = CurrentEntry->Next;
            if (DriverEntry->Dependent != FALSE) {
                EfipCoreInsertOnScheduledQueue(DriverEntry);
                ReadyToRun = TRUE;
            }
        }

    } while (ReadyToRun != FALSE);

    EfiDispatcherRunning = FALSE;
    return ReturnStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
VOID
EfipFirmwareVolumeEventProtocolNotify (
    EFI_EVENT Event,
    VOID *Context
    )

/*++

Routine Description:

    This routine is called when a new firmware volume protocol appears in the
    system.

Arguments:

    Event - Supplies a pointer to the event that fired.

    Context - Supplies an unused context pointer.

Return Value:

    None.

--*/

{

    EFI_FV_FILE_ATTRIBUTES Attributes;
    UINTN BufferSize;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINTN FileTypeCount;
    EFI_STATUS GetNextFileStatus;
    UINTN Index;
    UINTN Key;
    PEFI_KNOWN_HANDLE KnownHandle;
    EFI_GUID NameGuid;
    UINTN Size;
    EFI_STATUS Status;
    EFI_FV_FILETYPE Type;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Volume;
    EFI_HANDLE VolumeHandle;

    //
    // Loop through all the new firmware volumes.
    //

    while (TRUE) {
        BufferSize = sizeof(EFI_HANDLE);
        Status = EfiCoreLocateHandle(ByRegisterNotify,
                                     NULL,
                                     EfiFirmwareVolumeEventRegistration,
                                     &BufferSize,
                                     &VolumeHandle);

        if (EFI_ERROR(Status)) {
            break;
        }

        if (EfipFirmwareVolumeHasBeenProcessed(VolumeHandle) != FALSE) {
            continue;
        }

        KnownHandle = EfipMarkFirmwareVolumeProcessed(VolumeHandle);
        if (KnownHandle == NULL) {
            continue;
        }

        Status = EfiCoreHandleProtocol(VolumeHandle,
                                       &EfiFirmwareVolume2ProtocolGuid,
                                       (VOID **)&Volume);

        if ((EFI_ERROR(Status)) || (Volume == NULL)) {

            ASSERT(FALSE);

            continue;
        }

        Status = EfiCoreHandleProtocol(VolumeHandle,
                                       &EfiDevicePathProtocolGuid,
                                       (VOID **)&DevicePath);

        if (EFI_ERROR(Status)) {
            continue;
        }

        //
        // Discover drivers in the firmware volume and add them to the
        // discovered driver list.
        //

        FileTypeCount = sizeof(EfiDispatcherFileTypes) /
                        sizeof(EfiDispatcherFileTypes[0]);

        for (Index = 0; Index < FileTypeCount; Index += 1) {
            Key = 0;
            while (TRUE) {
                Type = EfiDispatcherFileTypes[Index];
                GetNextFileStatus = Volume->GetNextFile(Volume,
                                                        &Key,
                                                        &Type,
                                                        &NameGuid,
                                                        &Attributes,
                                                        &Size);

                if (EFI_ERROR(GetNextFileStatus)) {
                    break;
                }

                EfipCoreAddDriverToList(Volume, VolumeHandle, &NameGuid, Type);
            }
        }
    }

    return;
}

BOOLEAN
EfipFirmwareVolumeHasBeenProcessed (
    EFI_HANDLE Handle
    )

/*++

Routine Description:

    This routine determines if the given firmware volume has been processed.

Arguments:

    Handle - Supplies the handle to the volume.

Return Value:

    TRUE if the handle has been processed.

    FALSE if the handle has not been processed.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_KNOWN_HANDLE KnownHandle;

    CurrentEntry = EfiFirmwareVolumeList.Next;
    while (CurrentEntry != &EfiFirmwareVolumeList) {
        KnownHandle = LIST_VALUE(CurrentEntry, EFI_KNOWN_HANDLE, ListEntry);
        if (KnownHandle->Handle == Handle) {
            return TRUE;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return FALSE;
}

PEFI_KNOWN_HANDLE
EfipMarkFirmwareVolumeProcessed (
    EFI_HANDLE Handle
    )

/*++

Routine Description:

    This routine marks a firmware volume handle as having been processed. This
    function adds entries on the firmware volume list if the new entry is
    different from the one in the handle list by checking the firmware volume
    image GUID. Items are never removed/free from the firmware volume list.

Arguments:

    Handle - Supplies the handle to the volume.

Return Value:

    Returns a pointer to the new known handle structure.

    NULL if a firmware volume with the same GUID was already processed.

--*/

{

    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *BlockIo;
    EFI_FV_BLOCK_MAP_ENTRY *BlockMap;
    PLIST_ENTRY CurrentEntry;
    UINT32 ExtHeaderOffset;
    UINTN Index;
    PEFI_KNOWN_HANDLE KnownHandle;
    EFI_LBA LbaIndex;
    UINTN LbaOffset;
    EFI_GUID NameGuid;
    BOOLEAN NameGuidFound;
    EFI_STATUS Status;
    EFI_FIRMWARE_VOLUME_HEADER *VolumeHeader;

    NameGuidFound = FALSE;

    //
    // Get the firmware volume block protocol on the handle. Going rogue.
    //

    Status = EfiCoreHandleProtocol(Handle,
                                   &EfiFirmwareVolumeBlockProtocolGuid,
                                   (VOID **)&BlockIo);

    if (!EFI_ERROR(Status)) {

        //
        // Get the full volume header using the block I/O protocol.
        //

        ASSERT(BlockIo != NULL);

        Status = EfiFvGetVolumeHeader(BlockIo, &VolumeHeader);
        if (!EFI_ERROR(Status)) {

            ASSERT(VolumeHeader != NULL);

            if ((EfiFvVerifyHeaderChecksum(VolumeHeader) != FALSE) &&
                (VolumeHeader->ExtHeaderOffset != 0)) {

                ExtHeaderOffset = VolumeHeader->ExtHeaderOffset;
                BlockMap = VolumeHeader->BlockMap;
                LbaIndex = 0;
                LbaOffset = 0;

                //
                // Find the LBA index and offset for the volume extension
                // header using the block map.
                //

                while ((BlockMap->BlockCount != 0) ||
                       (BlockMap->BlockLength != 0)) {

                    for (Index = 0;
                         Index < BlockMap->BlockCount;
                         Index += 1) {

                        if (ExtHeaderOffset < BlockMap->BlockLength) {
                            break;
                        }

                        ExtHeaderOffset -= BlockMap->BlockLength;
                        LbaIndex += 1;
                    }

                    if (Index < BlockMap->BlockCount) {
                        LbaOffset = ExtHeaderOffset;
                        break;
                    }

                    BlockMap += 1;
                }

                Status = EfiFvReadData(BlockIo,
                                       &LbaIndex,
                                       &LbaOffset,
                                       sizeof(NameGuid),
                                       (UINT8 *)&NameGuid);

                if (!EFI_ERROR(Status)) {
                    NameGuidFound = TRUE;
                }
            }

            EfiCoreFreePool(VolumeHeader);
        }
    }

    //
    // If a name GUID for this volume was found, compare it with all the other
    // known volumes.
    //

    if (NameGuidFound != FALSE) {
        CurrentEntry = EfiFirmwareVolumeList.Next;
        while (CurrentEntry != &EfiFirmwareVolumeList) {
            KnownHandle = LIST_VALUE(CurrentEntry, EFI_KNOWN_HANDLE, ListEntry);
            if (EfiCoreCompareGuids(&NameGuid, &(KnownHandle->NameGuid)) !=
                FALSE) {

                RtlDebugPrint("Found two firmware volumes with the same GUID. "
                              "Skipping one!\n");

                return NULL;
            }
        }
    }

    KnownHandle = EfiCoreAllocateBootPool(sizeof(EFI_KNOWN_HANDLE));
    if (KnownHandle == NULL) {

        ASSERT(FALSE);

        return NULL;
    }

    EfiCoreSetMemory(KnownHandle, sizeof(EFI_KNOWN_HANDLE), 0);
    KnownHandle->Handle = Handle;
    if (NameGuidFound != FALSE) {
        EfiCoreCopyMemory(&(KnownHandle->NameGuid),
                          &NameGuid,
                          sizeof(EFI_GUID));
    }

    INSERT_BEFORE(&(KnownHandle->ListEntry), &EfiFirmwareVolumeList);
    return KnownHandle;
}

EFI_STATUS
EfipCoreAddDriverToList (
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Volume,
    EFI_HANDLE VolumeHandle,
    EFI_GUID *DriverName,
    EFI_FV_FILETYPE Type
    )

/*++

Routine Description:

    This routine adds a driver entry to the discovered list.

Arguments:

    Volume - Supplies a pointer to the firmware volume.

    VolumeHandle - Supplies the firmware volume handle.

    DriverName - Supplies a pointer to the GUID of the driver's name.

    Type - Supplies the file type.

Return Value:

    EFI_SUCCESS on success.

    EFI_ALREADY_STARTED if the driver has already been started.

--*/

{

    EFI_CORE_DRIVER_ENTRY *DriverEntry;

    DriverEntry = EfiCoreAllocateBootPool(sizeof(EFI_CORE_DRIVER_ENTRY));
    if (DriverEntry == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    EfiCoreSetMemory(DriverEntry, sizeof(EFI_CORE_DRIVER_ENTRY), 0);
    if (Type == EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE) {
        DriverEntry->IsFirmwareVolumeImage = TRUE;
    }

    DriverEntry->Magic = EFI_CORE_DRIVER_ENTRY_MAGIC;
    EfiCoreCopyMemory(&(DriverEntry->FileName), DriverName, sizeof(EFI_GUID));
    DriverEntry->VolumeHandle = VolumeHandle;
    DriverEntry->Volume = Volume;
    DriverEntry->FileDevicePath =
                    EfipCoreConvertFirmwareVolumeFileToDevicePath(Volume,
                                                                  VolumeHandle,
                                                                  DriverName);

    DriverEntry->Dependent = TRUE;
    EfiCoreAcquireLock(&EfiDispatcherLock);
    INSERT_BEFORE(&(DriverEntry->DriverListEntry), &EfiDiscoveredList);
    EfiCoreReleaseLock(&EfiDispatcherLock);
    return EFI_SUCCESS;
}

EFI_DEVICE_PATH_PROTOCOL *
EfipCoreConvertFirmwareVolumeFileToDevicePath (
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Volume,
    EFI_HANDLE VolumeHandle,
    EFI_GUID *DriverName
    )

/*++

Routine Description:

    This routine converts a firmware volume and driver name into an EFI
    device path.

Arguments:

    Volume - Supplies a pointer to the firmware volume.

    VolumeHandle - Supplies the firmware volume handle.

    DriverName - Supplies a pointer to the GUID of the driver's name.

Return Value:

    Returns a pointer to the file device path.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *FileDevicePath;
    EFI_FIRMWARE_VOLUME_FILE_DEVICE_PATH FileNameDevicePath;
    EFI_STATUS Status;
    EFI_DEVICE_PATH_PROTOCOL *VolumeDevicePath;

    Status = EfiCoreHandleProtocol(VolumeHandle,
                                   &EfiDevicePathProtocolGuid,
                                   (VOID **)&VolumeDevicePath);

    if (EFI_ERROR(Status)) {
        return NULL;
    }

    //
    // Build a device path to the file in the volume.
    //

    EfiCoreInitializeFirmwareVolumeDevicePathNode(&(FileNameDevicePath.File),
                                                  DriverName);

    EfiCoreSetDevicePathEndNode(&(FileNameDevicePath.End));
    FileDevicePath = EfiCoreAppendDevicePath(
                              VolumeDevicePath,
                              (EFI_DEVICE_PATH_PROTOCOL *)&FileNameDevicePath);

    return FileDevicePath;
}

VOID
EfipCoreInsertOnScheduledQueue (
    PEFI_CORE_DRIVER_ENTRY DriverEntry
    )

/*++

Routine Description:

    This routine inserts a driver entry onto the scheduled queue.

Arguments:

    DriverEntry - Supplies a pointer to the driver entry to schedule.

Return Value:

    None.

--*/

{

    EfiCoreAcquireLock(&EfiDispatcherLock);
    DriverEntry->Dependent = FALSE;
    DriverEntry->Scheduled = TRUE;
    INSERT_BEFORE(&(DriverEntry->SchedulerListEntry), &EfiScheduledQueue);
    EfiCoreReleaseLock(&EfiDispatcherLock);
    return;
}

