/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sound.c

Abstract:

    This module implements the sound library driver.

Author:

    Chris Stevens 18-Apr-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/devinfo/sound.h>
#include "sound.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum length of a device name including the null terminator.
//

#define SOUND_MAX_DEVICE_NAME_SIZE 20

//
// Define the minimum allowed low water signal threshold, in bytes.
//

#define SOUND_CORE_LOW_TRESHOLD_MINIMUM 1

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
SoundpAllocateIoBuffer (
    PSOUND_CONTROLLER Controller,
    PSOUND_DEVICE Device,
    UINTN FragmentSize,
    UINTN FragmentCount,
    PIO_BUFFER *NewIoBuffer
    );

VOID
SoundpFreeIoBuffer (
    PSOUND_CONTROLLER Controller,
    PSOUND_DEVICE Device,
    PIO_BUFFER IoBuffer
    );

KSTATUS
SoundpCopyBufferData (
    PSOUND_DEVICE_HANDLE Handle,
    PIO_BUFFER Destination,
    UINTN DestinationOffset,
    PIO_BUFFER Source,
    UINTN SourceOffset,
    UINTN Size
    );

KSTATUS
SoundpInitializeDevice (
    PSOUND_DEVICE_HANDLE Handle
    );

KSTATUS
SoundpResetDevice (
    PSOUND_DEVICE_HANDLE Handle
    );

KSTATUS
SoundpStartDevice (
    PSOUND_DEVICE_HANDLE Handle
    );

KSTATUS
SoundpSetVolume (
    PSOUND_DEVICE_HANDLE Handle,
    ULONG Volume
    );

VOID
SoundpControllerAddReference (
    PSOUND_CONTROLLER Controller
    );

VOID
SoundpControllerReleaseReference (
    PSOUND_CONTROLLER Controller
    );

VOID
SoundpDestroyController (
    PSOUND_CONTROLLER Controller
    );

KSTATUS
SoundpEnumerateDirectory (
    PSOUND_CONTROLLER Controller,
    PIO_BUFFER IoBuffer,
    PIO_OFFSET EntryOffset,
    UINTN SizeInBytes,
    PUINTN BytesCompleted
    );

ULONG
SoundpFindNearestRate (
    PSOUND_DEVICE SoundDevice,
    ULONG DesiredRate
    );

VOID
SoundpUpdateBufferState (
    PSOUND_IO_BUFFER Buffer,
    SOUND_DEVICE_TYPE Type,
    UINTN Offset,
    UINTN BytesAvailable,
    BOOL SoundCore
    );

VOID
SoundpSetBufferSize (
    PSOUND_DEVICE_HANDLE Handle,
    UINTN FragmentCount,
    UINTN FragmentSize
    );

VOID
SoundpSetHandleDefaults (
    PSOUND_DEVICE_HANDLE Handle
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER SoundDriver = NULL;

UUID SoundDeviceInformationUuid = SOUND_DEVICE_INFORMATION_UUID;

PCSTR SoundGenericDeviceNames[SoundDeviceTypeCount] = {
    "input",
    "output"
};

PCSTR SoundSpecificDeviceFormats[SoundDeviceTypeCount] = {
    "input%d",
    "output%d"
};

PCSTR SoundRouteNames[SoundDeviceRouteTypeCount] = {
    "Unknown",
    "LineOut",
    "Speaker",
    "Headphone",
    "CD",
    "SpdifOut",
    "DigitalOut",
    "ModemLineSide",
    "ModemHandsetSide",
    "LineIn",
    "AUX",
    "Microphone",
    "Telephony",
    "SpdifIn",
    "DigitalIn",
};

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the null driver. It registers its other
    dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    SoundDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

SOUND_API
KSTATUS
SoundCreateController (
    PSOUND_CONTROLLER_INFORMATION Registration,
    PSOUND_CONTROLLER *Controller
    )

/*++

Routine Description:

    This routine creates a sound core controller object.

Arguments:

    Registration - Supplies a pointer to the host registration information.
        This information will be copied, allowing it to be stack allocated by
        the caller.

    Controller - Supplies a pointer where a pointer to the new controller will
        be returned on success.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PSOUND_FUNCTION_TABLE FunctionTable;
    ULONG Index;
    PSOUND_CONTROLLER NewController;
    PSOUND_DEVICE SoundDevice;
    KSTATUS Status;
    UINTN TotalDeviceSize;

    NewController = NULL;

    //
    // Make sure the bare minimum was supplied.
    //

    if ((Registration->DeviceCount == 0) ||
        (Registration->Devices == NULL) ||
        (Registration->OsDevice == NULL) ||
        (Registration->MinFragmentCount < SOUND_FRAGMENT_COUNT_MINIMUM) ||
        (Registration->FunctionTable->GetSetInformation == NULL) ||
        (POWER_OF_2(Registration->MinFragmentCount) == FALSE) ||
        (POWER_OF_2(Registration->MaxFragmentCount) == FALSE) ||
        (POWER_OF_2(Registration->MinFragmentSize) == FALSE) ||
        (POWER_OF_2(Registration->MaxFragmentSize) == FALSE) ||
        (POWER_OF_2(Registration->MaxBufferSize) == FALSE)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CreateControllerEnd;
    }

    //
    // Determine the size of the allocation, accounting for the sound devices
    // and the function table.
    //

    TotalDeviceSize = 0;
    for (Index = 0; Index < Registration->DeviceCount; Index += 1) {
        SoundDevice = Registration->Devices[Index];
        if ((SoundDevice->RateCount == 0) || (SoundDevice->RouteCount == 0)) {
            Status = STATUS_INVALID_PARAMETER;
            goto CreateControllerEnd;
        }

        TotalDeviceSize += SoundDevice->StructureSize;
    }

    AllocationSize = sizeof(SOUND_CONTROLLER) +
                     sizeof(SOUND_FUNCTION_TABLE) +
                     (sizeof(PSOUND_DEVICE) * Registration->DeviceCount) +
                     TotalDeviceSize;

    NewController = MmAllocatePagedPool(AllocationSize,
                                        SOUND_CORE_ALLOCATION_TAG);

    if (NewController == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateControllerEnd;
    }

    //
    // Copy over the function table and sound devices so that the sound core
    // library has its own copies.
    //

    RtlZeroMemory(NewController, AllocationSize);
    NewController->ReferenceCount = 1;
    RtlCopyMemory(&(NewController->Host),
                  Registration,
                  sizeof(SOUND_CONTROLLER_INFORMATION));

    FunctionTable = (PSOUND_FUNCTION_TABLE)(NewController + 1);
    RtlCopyMemory(FunctionTable,
                  Registration->FunctionTable,
                  sizeof(SOUND_FUNCTION_TABLE));

    NewController->Host.FunctionTable = FunctionTable;
    NewController->Host.Devices = (PSOUND_DEVICE *)(FunctionTable + 1);
    SoundDevice = (PVOID)NewController->Host.Devices +
                  (sizeof(PSOUND_DEVICE) * NewController->Host.DeviceCount);

    for (Index = 0; Index < NewController->Host.DeviceCount; Index += 1) {
        NewController->Host.Devices[Index] = SoundDevice;
        RtlCopyMemory(SoundDevice,
                      Registration->Devices[Index],
                      Registration->Devices[Index]->StructureSize);

        SoundDevice->Flags &= SOUND_DEVICE_FLAG_PUBLIC_MASK;
        SoundDevice = (PVOID)SoundDevice + SoundDevice->StructureSize;
    }

    //
    // Take a reference on the host device so that the sound controller does
    // not disappear while the sound core controller still lives.
    //

    IoDeviceAddReference(NewController->Host.OsDevice);

    //
    // Notify the system that there is a new sound controller in town.
    //

    Status = IoRegisterDeviceInformation(NewController->Host.OsDevice,
                                         &SoundDeviceInformationUuid,
                                         TRUE);

    if (!KSUCCESS(Status)) {
        goto CreateControllerEnd;
    }

    KeGetSystemTime(&(NewController->CreationTime));

CreateControllerEnd:
    if (!KSUCCESS(Status)) {
        if (NewController != NULL) {
            SoundDestroyController(NewController);
            NewController = NULL;
        }
    }

    *Controller = NewController;
    return Status;
}

SOUND_API
VOID
SoundDestroyController (
    PSOUND_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys a sound controller.

Arguments:

    Controller - Supplies a pointer to the controller to tear down.

Return Value:

    None.

--*/

{

    SoundpControllerReleaseReference(Controller);
    return;
}

SOUND_API
KSTATUS
SoundLookupDevice (
    PSOUND_CONTROLLER Controller,
    PSYSTEM_CONTROL_LOOKUP Lookup
    )

/*++

Routine Description:

    This routine looks for a sound device underneath the given controller.

Arguments:

    Controller - Supplies a pointer tot he sound core library's controller.

    Lookup - Supplies a pointer to the lookup information.

Return Value:

    Status code.

--*/

{

    ULONG DeviceIndex;
    BOOL Equal;
    FILE_ID FileId;
    PCSTR Format;
    ULONG FoundTypeIndex;
    ULONG ItemsScanned;
    ULONG LookupFlags;
    ULONG MapFlags;
    IO_OBJECT_TYPE ObjectType;
    FILE_PERMISSIONS Permissions;
    PFILE_PROPERTIES Properties;
    PSOUND_DEVICE SoundDevice;
    KSTATUS Status;
    SOUND_DEVICE_TYPE Type;
    ULONG TypeIndex;

    LookupFlags = 0;
    MapFlags = 0;
    if (Controller == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto LookupDeviceEnd;
    }

    //
    // If this is the root lookup, just return a handle to the controller.
    //

    if (Lookup->Root != FALSE) {

        //
        // Enable opening of the controller as a directory.
        //

        FileId = (FILE_ID)(UINTN)Controller;
        ObjectType = IoObjectRegularDirectory;
        Status = STATUS_SUCCESS;
        goto LookupDeviceEnd;
    }

    //
    // If the controller claims to have non-cached buffers, then report the
    // map flags on lookup so that mmap knows to map buffers non-cached.
    //

    if ((Controller->Host.Flags &
         SOUND_CONTROLLER_FLAG_NON_CACHED_DMA_BUFFER) != 0) {

        MapFlags |= MAP_FLAG_CACHE_DISABLE;
    }

    //
    // If the controller needs non-paged sound buffer state, then make sure
    // a non-paged I/O state is created for the file.
    //

    if ((Controller->Host.Flags &
         SOUND_CONTROLLER_FLAG_NON_PAGED_SOUND_BUFFER) != 0) {

        LookupFlags |= LOOKUP_FLAG_NON_PAGED_IO_STATE;
    }

    //
    // If the name matches a generic name, the file ID get set to the type.
    // An appropriate device will be found on open.
    //

    ObjectType = IoObjectCharacterDevice;
    for (Type = 0; Type < SoundDeviceTypeCount; Type += 1) {
        Equal = RtlAreStringsEqual(SoundGenericDeviceNames[Type],
                                   Lookup->FileName,
                                   Lookup->FileNameSize - 1);

        if (Equal != FALSE) {
            FileId = Type;
            Status = STATUS_SUCCESS;
            goto LookupDeviceEnd;
        }
    }

    //
    // Perhaps a specific name was specified.
    //

    for (Type = 0; Type < SoundDeviceTypeCount; Type += 1) {
        Format = SoundSpecificDeviceFormats[Type];
        Status = RtlStringScan(Lookup->FileName,
                               Lookup->FileNameSize - 1,
                               Format,
                               RtlStringLength(Format) + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               &FoundTypeIndex);

        if (KSUCCESS(Status) && (ItemsScanned == 1)) {
            break;
        }
    }

    if (Type != SoundDeviceTypeCount) {
        TypeIndex = 0;
        for (DeviceIndex = 0;
             DeviceIndex < Controller->Host.DeviceCount;
             DeviceIndex += 1) {

            SoundDevice = Controller->Host.Devices[DeviceIndex];
            if (SoundDevice->Type == Type) {
                if (TypeIndex == FoundTypeIndex) {
                    FileId = (FILE_ID)(UINTN)SoundDevice;
                    Status = STATUS_SUCCESS;
                    goto LookupDeviceEnd;
                }

                TypeIndex += 1;
            }
        }
    }

    Status = STATUS_PATH_NOT_FOUND;

LookupDeviceEnd:
    if (KSUCCESS(Status)) {
        Properties = Lookup->Properties;
        Properties->FileId = FileId;
        Properties->Type = ObjectType;
        Properties->HardLinkCount = 1;
        Properties->BlockSize = 1;
        Properties->BlockCount = 0;
        Properties->UserId = 0;
        Properties->GroupId = 0;
        Properties->StatusChangeTime = Controller->CreationTime;
        Properties->ModifiedTime = Properties->StatusChangeTime;
        Properties->AccessTime = Properties->StatusChangeTime;

        //
        // Set the permissions based on the device type.
        //

        if (ObjectType == IoObjectRegularDirectory)  {
            Permissions = FILE_PERMISSION_USER_READ |
                          FILE_PERMISSION_USER_EXECUTE |
                          FILE_PERMISSION_GROUP_READ |
                          FILE_PERMISSION_GROUP_EXECUTE |
                          FILE_PERMISSION_OTHER_READ |
                          FILE_PERMISSION_OTHER_EXECUTE;

        } else {
            if (Type == SoundDeviceInput) {
                Permissions = FILE_PERMISSION_USER_READ |
                              FILE_PERMISSION_GROUP_READ |
                              FILE_PERMISSION_OTHER_READ;

            //
            // Output devices are read/write to allow mmap to work.
            //

            } else {

                ASSERT(Type == SoundDeviceOutput);

                Permissions = FILE_PERMISSION_USER_WRITE |
                              FILE_PERMISSION_GROUP_WRITE |
                              FILE_PERMISSION_OTHER_WRITE |
                              FILE_PERMISSION_USER_READ |
                              FILE_PERMISSION_GROUP_READ |
                              FILE_PERMISSION_OTHER_READ;
            }
        }

        Properties->Permissions = Permissions;
        Properties->Size = 0;
        Lookup->MapFlags = MapFlags;
        Lookup->Flags = LookupFlags;
    }

    return Status;
}

SOUND_API
KSTATUS
SoundOpenDevice (
    PSOUND_CONTROLLER Controller,
    PFILE_PROPERTIES FileProperties,
    ULONG AccessFlags,
    ULONG OpenFlags,
    PIO_OBJECT_STATE IoState,
    PSOUND_DEVICE_HANDLE *Handle
    )

/*++

Routine Description:

    This routine opens a sound device. This helps a sound driver coordinate the
    sharing of its resources and may even select which physical device to open.

Arguments:

    Controller - Supplies a pointer to the sound core library's controller.

    FileProperties - Supplies a pointer to the file properties that indicate
        which device is being opened.

    AccessFlags - Supplies a bitmask of access flags. See IO_ACCESS_* for
        definitions.

    OpenFlags - Supplies a bitmask of open flags. See OPEN_FLAG_* for
        definitions.

    IoState - Supplies a pointer I/O state to signal for this device.

    Handle - Supplies a pointer that receives an opaque handle to the opened
        sound device.

Return Value:

    Status code.

--*/

{

    PSOUND_DEVICE Device;
    ULONG DeviceIndex;
    FILE_ID FileId;
    PSOUND_DEVICE_HANDLE NewHandle;
    ULONG OldFlags;
    PSOUND_DEVICE SoundDevice;
    KSTATUS Status;

    NewHandle = NULL;
    SoundDevice = NULL;

    //
    // Make sure the requested device can support the requested access.
    //

    FileId = FileProperties->FileId;
    if (FileProperties->Type == IoObjectRegularDirectory) {
        if ((PSOUND_CONTROLLER)(UINTN)FileId != Controller) {
            Status = STATUS_INVALID_PARAMETER;
            goto OpenDeviceEnd;
        }

    //
    // If the file ID is small (i.e. just a device type) pick a suitable device
    // now that the caller is actually opening it.
    //

    } else if (FileId < SoundDeviceTypeCount) {
        for (DeviceIndex = 0;
             DeviceIndex < Controller->Host.DeviceCount;
             DeviceIndex += 1) {

            Device = Controller->Host.Devices[DeviceIndex];
            if (Device->Type == FileId) {
                if ((Device->Flags & SOUND_DEVICE_FLAG_PRIMARY) != 0) {
                    SoundDevice = Device;
                    break;

                } else if (SoundDevice == NULL) {
                    SoundDevice = Device;
                }
            }
        }

        if (SoundDevice == NULL) {
            Status = STATUS_NO_SUCH_DEVICE;
            goto OpenDeviceEnd;
        }

    } else {
        SoundDevice = (PSOUND_DEVICE)(UINTN)FileProperties->FileId;
    }

    //
    // Attempt to gain exclusive access to the device.
    //

    if (SoundDevice != NULL) {
        OldFlags = RtlAtomicOr32(&(SoundDevice->Flags),
                                 SOUND_DEVICE_FLAG_INTERNAL_BUSY);

        if ((OldFlags & SOUND_DEVICE_FLAG_INTERNAL_BUSY) != 0) {
            Status = STATUS_RESOURCE_IN_USE;
            goto OpenDeviceEnd;
        }
    }

    //
    // If the controller needs to the buffer to be non-paged, allocate the
    // whole handle as non-paged, since the buffer is embedded in the handle.
    //

    if ((Controller->Host.Flags &
         SOUND_CONTROLLER_FLAG_NON_PAGED_SOUND_BUFFER) != 0) {

        NewHandle = MmAllocateNonPagedPool(sizeof(SOUND_DEVICE_HANDLE),
                                           SOUND_CORE_ALLOCATION_TAG);

    } else {
        NewHandle = MmAllocatePagedPool(sizeof(SOUND_DEVICE_HANDLE),
                                        SOUND_CORE_ALLOCATION_TAG);
    }

    if (NewHandle == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenDeviceEnd;
    }

    RtlZeroMemory(NewHandle, sizeof(SOUND_DEVICE_HANDLE));
    SoundpControllerAddReference(Controller);
    NewHandle->Controller = Controller;
    NewHandle->Device = SoundDevice;
    NewHandle->Buffer.IoState = IoState;
    NewHandle->State = SoundDeviceStateUninitialized;
    NewHandle->Lock = KeCreateQueuedLock();
    if (NewHandle->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenDeviceEnd;
    }

    //
    // Set some default information in case the user does not.
    //

    SoundpSetHandleDefaults(NewHandle);
    Status = STATUS_SUCCESS;

OpenDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (NewHandle != NULL) {
            SoundCloseDevice(NewHandle);
            NewHandle = NULL;
        }
    }

    *Handle = NewHandle;
    return Status;
}

SOUND_API
VOID
SoundCloseDevice (
    PSOUND_DEVICE_HANDLE Handle
    )

/*++

Routine Description:

    This routine closes a sound device, releasing any resources allocated for
    the device.

Arguments:

    Handle - Supplies a pointer to the sound device handle to close.

Return Value:

    None.

--*/

{

    BOOL NonPaged;
    ULONG OldFlags;

    SoundpResetDevice(Handle);
    if (Handle->Device != NULL) {
        OldFlags = RtlAtomicAnd32(&(Handle->Device->Flags),
                                  ~SOUND_DEVICE_FLAG_INTERNAL_BUSY);

        ASSERT((OldFlags & SOUND_DEVICE_FLAG_INTERNAL_BUSY) != 0);
    }

    //
    // The buffer is typically freed in reset, but if reset fails, still
    // release the buffer resources.
    //

    if (Handle->Buffer.IoBuffer != NULL) {

        ASSERT(Handle->Device != NULL);

        SoundpFreeIoBuffer(Handle->Controller,
                           Handle->Device,
                           Handle->Buffer.IoBuffer);

        Handle->Buffer.IoBuffer = NULL;
    }

    NonPaged = FALSE;
    if ((Handle->Controller->Host.Flags &
         SOUND_CONTROLLER_FLAG_NON_PAGED_SOUND_BUFFER) != 0) {

        NonPaged = TRUE;
    }

    SoundpControllerReleaseReference(Handle->Controller);
    if (Handle->Lock != NULL) {
        KeDestroyQueuedLock(Handle->Lock);
    }

    if (NonPaged != FALSE) {
        MmFreeNonPagedPool(Handle);

    } else {
        MmFreePagedPool(Handle);
    }

    return;
}

SOUND_API
KSTATUS
SoundPerformIo (
    PSOUND_DEVICE_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    PIO_OFFSET IoOffset,
    UINTN SizeInBytes,
    ULONG IoFlags,
    ULONG TimeoutInMilliseconds,
    BOOL Write,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine will play or record sound on the given device.

Arguments:

    Handle - Supplies a pointer to the sound device handle to use for I/O.

    IoBuffer - Supplies a pointer to I/O buffer with the data to play or the
        where the recorded data should end up.

    IoOffset - Supplies a pointer to the offset where the I/O should start on
        input. On output, it stores the I/O offset at the end of the I/O. This
        is only relevant to the controller "directory".

    SizeInBytes - Supplies the amount of data to play or record.

    IoFlags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    Write - Supplies a boolean indicating whether this is a write (TRUE) or
        read (FALSE) request.

    BytesCompleted - Supplies a pointer that receives the number of bytes
        played or recorded.

Return Value:

    Status code.

--*/

{

    UINTN BytesAvailable;
    UINTN BytesRemaining;
    UINTN BytesThisRound;
    UINTN CopySize;
    UINTN CoreOffset;
    ULONGLONG CurrentTime;
    UINTN CyclicBufferSize;
    PUINTN CyclicOffset;
    PIO_BUFFER DestinationBuffer;
    UINTN DestinationOffset;
    ULONGLONG EndTime;
    ULONG Events;
    PUINTN LinearOffset;
    BOOL LockHeld;
    ULONG ReturnedEvents;
    PIO_BUFFER SourceBuffer;
    UINTN SourceOffset;
    KSTATUS Status;
    ULONGLONG TimeCounterFrequency;
    ULONG WaitTime;

    BytesRemaining = SizeInBytes;
    EndTime = 0;
    LockHeld = FALSE;
    TimeCounterFrequency = 0;

    //
    // If the handle has no sound device, then it is a handle to the
    // controller. Writes are not allowed and a read request returns the
    // available devices as directory entries.
    //

    if (Handle->Device == NULL) {
        if (Write != FALSE) {
            return STATUS_ACCESS_DENIED;
        }

        Status = SoundpEnumerateDirectory(Handle->Controller,
                                          IoBuffer,
                                          IoOffset,
                                          SizeInBytes,
                                          BytesCompleted);

        return Status;
    }

    //
    // If the handle is non-blocking, then set the timeout to 0. This overrides
    // the behavior of the I/O handle's open flags.
    //

    if ((Handle->Flags & SOUND_DEVICE_HANDLE_FLAG_NON_BLOCKING) != 0) {
        TimeoutInMilliseconds = 0;
    }

    //
    // If the device isn't already in the running state, then check to make
    // sure it is initialized.
    //

    *BytesCompleted = 0;
    if (Handle->State != SoundDeviceStateRunning) {
        KeAcquireQueuedLock(Handle->Lock);
        LockHeld = TRUE;

        //
        // If this is the first I/O on the device, then allocate the buffer.
        //

        if (Handle->Buffer.IoBuffer == NULL) {
            Status = SoundpAllocateIoBuffer(Handle->Controller,
                                            Handle->Device,
                                            Handle->Buffer.FragmentSize,
                                            Handle->Buffer.FragmentCount,
                                            &(Handle->Buffer.IoBuffer));

            if (!KSUCCESS(Status)) {
                goto PerformIoEnd;
            }
        }

        //
        // If the device is uninitialized, make sure it is ready to start the
        // I/O.
        //

        if (Handle->State == SoundDeviceStateUninitialized) {
            Status = SoundpInitializeDevice(Handle);
            if (!KSUCCESS(Status)) {
                goto PerformIoEnd;
            }
        }

        KeReleaseQueuedLock(Handle->Lock);
        LockHeld = FALSE;
    }

    //
    // Determine which event to wait on and don't allow the wrong I/O on the
    // device.
    //

    if (Write != FALSE) {
        if (Handle->Device->Type == SoundDeviceInput) {
            Status = STATUS_ACCESS_DENIED;
            goto PerformIoEnd;
        }

        Events = POLL_EVENT_OUT;
        SourceBuffer = IoBuffer;
        LinearOffset = &SourceOffset;
        DestinationBuffer = Handle->Buffer.IoBuffer;
        CyclicOffset = &DestinationOffset;

    } else {

        //
        // If an I/O buffer is empty and mmap is supported, then just return
        // the device's buffer directly.
        //

        if (IoBuffer->FragmentCount == 0) {
            if ((Handle->Device->Capabilities & SOUND_CAPABILITY_MMAP) == 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto PerformIoEnd;
            }

            if (*IoOffset >= Handle->Buffer.Size) {
                Status = STATUS_END_OF_FILE;
                goto PerformIoEnd;
            }

            if (((*IoOffset + SizeInBytes) < *IoOffset) ||
                ((*IoOffset + SizeInBytes) > Handle->Buffer.Size)) {

                SizeInBytes = Handle->Buffer.Size - *IoOffset;
                BytesRemaining = SizeInBytes;
            }

            Status = MmAppendIoBuffer(IoBuffer,
                                      Handle->Buffer.IoBuffer,
                                      *IoOffset,
                                      SizeInBytes);

            if (KSUCCESS(Status)) {
                BytesRemaining = 0;
            }

            goto PerformIoEnd;
        }

        if (Handle->Device->Type == SoundDeviceOutput) {
            Status = STATUS_ACCESS_DENIED;
            goto PerformIoEnd;
        }

        Events = POLL_EVENT_IN;
        SourceBuffer = Handle->Buffer.IoBuffer;
        CyclicOffset = &SourceOffset;
        DestinationBuffer = IoBuffer;
        LinearOffset = &DestinationOffset;

        //
        // If the input device is not yet running, then fire it up.
        //

        if (Handle->State < SoundDeviceStateRunning) {
            Status = SoundpStartDevice(Handle);
            if (!KSUCCESS(Status)) {
                goto PerformIoEnd;
            }
        }
    }

    if ((TimeoutInMilliseconds != 0) &&
        (TimeoutInMilliseconds != WAIT_TIME_INDEFINITE)) {

        EndTime = KeGetRecentTimeCounter();
        EndTime += KeConvertMicrosecondsToTimeTicks(
                         TimeoutInMilliseconds * MICROSECONDS_PER_MILLISECOND);

        TimeCounterFrequency = HlQueryTimeCounterFrequency();
    }

    //
    // Wait until there is space and then either write into the buffer or read
    // from it.
    //

    *LinearOffset = 0;
    CyclicBufferSize = Handle->Buffer.Size;
    do {
        if (TimeoutInMilliseconds == 0) {
            WaitTime = 0;

        } else if (TimeoutInMilliseconds != WAIT_TIME_INDEFINITE) {
            CurrentTime = KeGetRecentTimeCounter();
            WaitTime = (EndTime - CurrentTime) * MILLISECONDS_PER_SECOND /
                       TimeCounterFrequency;

        } else {
            WaitTime = WAIT_TIME_INDEFINITE;
        }

        Status = IoWaitForIoObjectState(Handle->Buffer.IoState,
                                        Events,
                                        TRUE,
                                        WaitTime,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            goto PerformIoEnd;
        }

        if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
            Status = STATUS_DEVICE_IO_ERROR;
            goto PerformIoEnd;
        }

        //
        // Multiple references may be taken on the I/O handle due to a fork.
        // This needs to synchronize between multiple readers/writers.
        //

        KeAcquireQueuedLock(Handle->Lock);
        LockHeld = TRUE;

        //
        // Immediately consume all of the available bytes. If there are more
        // than necessary, they get put back when the buffer state is updated.
        //

        BytesAvailable = 0;
        BytesThisRound = RtlAtomicExchange(&(Handle->Buffer.BytesAvailable), 0);
        if (BytesThisRound > BytesRemaining) {
            BytesAvailable = BytesThisRound - BytesRemaining;
            BytesThisRound = BytesRemaining;
        }

        ASSERT(BytesThisRound <= CyclicBufferSize);

        //
        // Copy from the core offset to the end of the buffer until there are
        // no bytes remaining for this round.
        //

        CoreOffset = Handle->Buffer.CoreOffset;
        while (BytesThisRound != 0) {
            CopySize = CyclicBufferSize - CoreOffset;
            if (CopySize > BytesThisRound) {
                CopySize = BytesThisRound;
            }

            *CyclicOffset = CoreOffset;
            Status = SoundpCopyBufferData(Handle,
                                          DestinationBuffer,
                                          DestinationOffset,
                                          SourceBuffer,
                                          SourceOffset,
                                          CopySize);

            if (!KSUCCESS(Status)) {
                goto PerformIoEnd;
            }

            BytesThisRound -= CopySize;
            BytesRemaining -= CopySize;
            *LinearOffset += CopySize;

            ASSERT(POWER_OF_2(CyclicBufferSize) != FALSE);

            CoreOffset += CopySize;
            CoreOffset = REMAINDER(CoreOffset, CyclicBufferSize);
        }

        SoundpUpdateBufferState(&(Handle->Buffer),
                                Handle->Device->Type,
                                CoreOffset,
                                BytesAvailable,
                                TRUE);

        KeReleaseQueuedLock(Handle->Lock);
        LockHeld = FALSE;

        //
        // If this is a write and the device is not started, fire it up now
        // that there is data in the buffer.
        //

        if (Handle->State < SoundDeviceStateRunning) {
            Status = SoundpStartDevice(Handle);
            if (!KSUCCESS(Status)) {
                goto PerformIoEnd;
            }
        }

    } while (BytesRemaining != 0);

PerformIoEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Handle->Lock);
    }

    *BytesCompleted = SizeInBytes - BytesRemaining;
    return Status;
}

SOUND_API
KSTATUS
SoundUserControl (
    PSOUND_DEVICE_HANDLE Handle,
    BOOL FromKernelMode,
    ULONG RequestCode,
    PVOID RequestBuffer,
    UINTN RequestBufferSize
    )

/*++

Routine Description:

    This routine handles user control requests that get or set the state of the
    given sound device.

Arguments:

    Handle - Supplies a pointer to the sound device handle for the request.

    FromKernelMode - Supplies a boolean indicating whether or not the request
        came from kernel mode (TRUE) or user mode (FALSE). If it came from user
        mode, then special MM routines must be used when accessing the request
        buffer.

    RequestCode - Supplies the request code for the user control.

    RequestBuffer - Supplies a pointer to the buffer containing context for the
        user control request. Treat this with suspicion if the request came
        from user mode.

    RequestBufferSize - Supplies the size of the request buffer. If the request
        is from user mode, this must be treated with suspicion.

Return Value:

    Status code.

--*/

{

    ULONG BytesRemaining;
    ULONG ChannelCount;
    ULONG ClearFlags;
    UINTN ControllerOffset;
    PVOID CopyOutBuffer;
    UINTN CopySize;
    UINTN FragmentCount;
    UINTN FragmentsCompleted;
    ULONG FragmentShift;
    UINTN FragmentSize;
    ULONG Index;
    ULONG IntegerUlong;
    BOOL LockHeld;
    PCSTR Name;
    ULONG NameIndex;
    ULONG NameSize;
    ULONG OldFlags;
    SOUND_POSITION_INFORMATION Position;
    SOUND_QUEUE_INFORMATION QueueInformation;
    ULONG RouteCount;
    SOUND_DEVICE_ROUTE_INFORMATION RouteInformation;
    PSOUND_DEVICE_ROUTE Routes;
    ULONG SetFlags;
    INT Shift;
    PSOUND_DEVICE SoundDevice;
    KSTATUS Status;

    SoundDevice = Handle->Device;

    //
    // No user control requests are supported for the controller itself.
    //

    if (SoundDevice == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        goto UserControlEnd;
    }

    Status = STATUS_SUCCESS;
    CopyOutBuffer = NULL;
    switch (RequestCode) {
    case SoundGetSupportedFormats:
        CopyOutBuffer = &(SoundDevice->Formats);
        CopySize = sizeof(ULONG);
        break;

    case SoundSetFormat:
        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        //
        // If a valid format was supplied, make sure there is only 1.
        //

        if ((IntegerUlong & SoundDevice->Formats) != 0) {
            Shift = RtlCountTrailingZeros32(IntegerUlong);
            Handle->Format = (1 << Shift);
        }

        //
        // Always return the current format.
        //

        CopyOutBuffer = &(Handle->Format);
        break;

    case SoundSetChannelCount:
        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        if ((IntegerUlong >= SoundDevice->MinChannelCount) &&
            (IntegerUlong <= SoundDevice->MaxChannelCount)) {

            Handle->ChannelCount = IntegerUlong;
        }

        CopyOutBuffer = &(Handle->ChannelCount);
        break;

    case SoundSetStereo:
        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        ChannelCount = Handle->ChannelCount;
        if ((IntegerUlong == 1) &&
            (SoundDevice->MaxChannelCount >= SOUND_STEREO_CHANNEL_COUNT)) {

            ChannelCount = SOUND_STEREO_CHANNEL_COUNT;

        } else if ((IntegerUlong == 0) &&
                   (SoundDevice->MinChannelCount <= SOUND_MONO_CHANNEL_COUNT)) {

            ChannelCount = SOUND_MONO_CHANNEL_COUNT;
            IntegerUlong = 0;
        }

        Handle->ChannelCount = ChannelCount;
        CopyOutBuffer = &IntegerUlong;
        break;

    case SoundSetSampleRate:
        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        //
        // Find the closest supported sample rate.
        //

        Handle->SampleRate = SoundpFindNearestRate(SoundDevice, IntegerUlong);
        CopyOutBuffer = &(Handle->SampleRate);
        break;

    case SoundGetCurrentInputPosition:
    case SoundGetCurrentOutputPosition:
        CopySize = sizeof(SOUND_POSITION_INFORMATION);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        CopyOutBuffer = &Position;
        if (Handle->Device->Type == SoundDeviceInput) {
            if (RequestCode == SoundGetCurrentOutputPosition) {
                RtlZeroMemory(&Position, sizeof(Position));
                break;
            }

        } else {
            if (RequestCode == SoundGetCurrentInputPosition) {
                RtlZeroMemory(&Position, sizeof(Position));
                break;
            }
        }

        //
        // This must be synchronized in order to update the fragment count and
        // the buffer state.
        //

        KeAcquireQueuedLock(Handle->Lock);
        ControllerOffset = Handle->Buffer.ControllerOffset;
        Position.TotalBytes = (ULONG)Handle->Buffer.BytesCompleted;
        FragmentsCompleted = Position.TotalBytes >>
                             Handle->Buffer.FragmentShift;

        Position.FragmentCount = (LONG)(FragmentsCompleted -
                                        Handle->Buffer.FragmentsCompleted);

        Handle->Buffer.FragmentsCompleted = FragmentsCompleted;
        Position.Offset = (LONG)ControllerOffset;

        //
        // This IOCTL is used in conjunction with mmap. As user mode will not
        // make any official reads/writes, use this as an opportunity to move
        // the core's offset forward to match the controller offset, eating
        // through all of the available bytes.
        //

        RtlAtomicExchange(&(Handle->Buffer.BytesAvailable), 0);
        SoundpUpdateBufferState(&(Handle->Buffer),
                                Handle->Device->Type,
                                ControllerOffset,
                                0,
                                TRUE);

        KeReleaseQueuedLock(Handle->Lock);
        break;

    case SoundGetInputQueueSize:
    case SoundGetOutputQueueSize:
        CopySize = sizeof(SOUND_QUEUE_INFORMATION);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        ASSERT(Handle->Buffer.FragmentSize <= MAX_LONG);
        ASSERT(Handle->Buffer.FragmentCount <= MAX_LONG);

        CopyOutBuffer = &QueueInformation;
        if (Handle->Device->Type == SoundDeviceInput) {
            if (RequestCode == SoundGetOutputQueueSize) {
                RtlZeroMemory(&QueueInformation, sizeof(QueueInformation));
                break;
            }

        } else {
            if (RequestCode == SoundGetInputQueueSize) {
                RtlZeroMemory(&QueueInformation, sizeof(QueueInformation));
                break;
            }
        }

        QueueInformation.BytesAvailable = Handle->Buffer.BytesAvailable;
        QueueInformation.FragmentsAvailable = QueueInformation.BytesAvailable >>
                                              Handle->Buffer.FragmentShift;

        QueueInformation.FragmentSize = (LONG)Handle->Buffer.FragmentSize;
        QueueInformation.FragmentCount = (LONG)Handle->Buffer.FragmentCount;
        break;

    case SoundSetTimingPolicy:
        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        if (IntegerUlong > SOUND_TIMING_POLICY_MAX) {
            IntegerUlong = SOUND_TIMING_POLICY_MAX;
        }

        if (IntegerUlong <= (SOUND_TIMING_POLICY_MAX / 2)) {
            FragmentShift = (SOUND_TIMING_POLICY_MAX / 2) - IntegerUlong;

            ASSERT(SOUND_FRAGMENT_SIZE_DEFAULT_SHIFT > FragmentShift);

            FragmentShift = SOUND_FRAGMENT_SIZE_DEFAULT_SHIFT - FragmentShift;

        } else {
            FragmentShift = IntegerUlong - (SOUND_TIMING_POLICY_MAX / 2);
            FragmentShift += SOUND_FRAGMENT_SIZE_DEFAULT_SHIFT;
        }

        FragmentSize = 1 << FragmentShift;

        //
        // The fragment count and size can only be changed before the device
        // is initialized.
        //

        LockHeld = FALSE;
        if (Handle->State < SoundDeviceStateInitialized) {
            KeAcquireQueuedLock(Handle->Lock);
            LockHeld = TRUE;
            SoundpSetBufferSize(Handle,
                                Handle->Buffer.FragmentCount,
                                FragmentSize);
        }

        if (Handle->Buffer.FragmentShift <= SOUND_FRAGMENT_SIZE_DEFAULT_SHIFT) {
            IntegerUlong = SOUND_FRAGMENT_SIZE_DEFAULT_SHIFT -
                           Handle->Buffer.FragmentShift;

            IntegerUlong = (SOUND_TIMING_POLICY_MAX / 2) - IntegerUlong;
            if (IntegerUlong > (SOUND_TIMING_POLICY_MAX / 2)) {
                IntegerUlong = 0;
            }

        } else {
            IntegerUlong = Handle->Buffer.FragmentShift -
                           SOUND_FRAGMENT_SIZE_DEFAULT_SHIFT;

            IntegerUlong += (SOUND_TIMING_POLICY_MAX / 2);
            if (IntegerUlong > SOUND_TIMING_POLICY_MAX) {
                IntegerUlong = SOUND_TIMING_POLICY_MAX;
            }
        }

        if (LockHeld != FALSE) {
            KeReleaseQueuedLock(Handle->Lock);
            LockHeld = FALSE;
        }

        CopyOutBuffer = &IntegerUlong;
        break;

    case SoundSetBufferSizeHint:
        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        //
        // The fragment count and size can only be changed before the device
        // is initialized.
        //

        LockHeld = FALSE;
        if (Handle->State < SoundDeviceStateInitialized) {
            FragmentCount = (IntegerUlong &
                             SOUND_BUFFER_SIZE_HINT_FRAGMENT_COUNT_MASK) >>
                            SOUND_BUFFER_SIZE_HINT_FRAGMENT_COUNT_SHIFT;

            FragmentSize = 1 << ((IntegerUlong &
                                  SOUND_BUFFER_SIZE_HINT_FRAGMENT_SIZE_MASK) >>
                                 SOUND_BUFFER_SIZE_HINT_FRAGMENT_SIZE_SHIFT);

            KeAcquireQueuedLock(Handle->Lock);
            LockHeld = TRUE;
            SoundpSetBufferSize(Handle, FragmentCount, FragmentSize);
        }

        IntegerUlong = ((Handle->Buffer.FragmentCount <<
                         SOUND_BUFFER_SIZE_HINT_FRAGMENT_COUNT_SHIFT) &
                        SOUND_BUFFER_SIZE_HINT_FRAGMENT_COUNT_MASK) |
                       ((Handle->Buffer.FragmentShift <<
                         SOUND_BUFFER_SIZE_HINT_FRAGMENT_SIZE_SHIFT) &
                        SOUND_BUFFER_SIZE_HINT_FRAGMENT_SIZE_MASK);

        if (LockHeld != FALSE) {
            KeReleaseQueuedLock(Handle->Lock);
            LockHeld = FALSE;
        }

        CopyOutBuffer = &IntegerUlong;
        break;

    case SoundStopInput:
        if (Handle->Device->Type != SoundDeviceInput) {
            break;
        }

        Status = SoundpResetDevice(Handle);
        break;

    case SoundStopOutput:
        if (Handle->Device->Type != SoundDeviceOutput) {
            break;
        }

        Status = SoundpResetDevice(Handle);
        break;

    case SoundStopAll:
        Status = SoundpResetDevice(Handle);
        break;

    case SoundGetDeviceCapabilities:
        CopyOutBuffer = &(SoundDevice->Capabilities);
        CopySize = sizeof(ULONG);
        break;

    case SoundEnableDevice:
        if ((Handle->Device->Capabilities &
             SOUND_CAPABILITY_MANUAL_ENABLE) == 0) {

            Status = STATUS_NOT_SUPPORTED;
            break;
        }

        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        //
        // Figure out which flags need to be set vs. cleared.
        //

        SetFlags = SOUND_DEVICE_FLAG_INTERNAL_ENABLE_INPUT;
        if (Handle->Device->Type == SoundDeviceOutput) {
            SetFlags = SOUND_DEVICE_FLAG_INTERNAL_ENABLE_OUTPUT;
        }

        ClearFlags = 0;
        if ((IntegerUlong & SOUND_ENABLE_INPUT) == 0) {
            ClearFlags |= SOUND_DEVICE_FLAG_INTERNAL_ENABLE_INPUT;
        }

        if ((IntegerUlong & SOUND_ENABLE_OUTPUT) == 0) {
            ClearFlags |= SOUND_DEVICE_FLAG_INTERNAL_ENABLE_OUTPUT;
        }

        if (ClearFlags != 0) {
            SetFlags &= ~ClearFlags;
            RtlAtomicAnd32(&(Handle->Device->Flags), ~ClearFlags);
        }

        //
        // If a flags actually gets set, then try to start the device.
        //

        if (SetFlags != 0) {
            OldFlags = RtlAtomicOr32(&(Handle->Device->Flags), SetFlags);
            if ((OldFlags & SetFlags) != SetFlags) {
                SoundpStartDevice(Handle);
            }

            IntegerUlong = 0;
            if ((SetFlags & SOUND_DEVICE_FLAG_INTERNAL_ENABLE_INPUT) != 0) {
                IntegerUlong |= SOUND_ENABLE_INPUT;
            }

            if ((SetFlags & SOUND_DEVICE_FLAG_INTERNAL_ENABLE_OUTPUT) != 0) {
                IntegerUlong |= SOUND_ENABLE_OUTPUT;
            }
        }

        CopyOutBuffer = &IntegerUlong;
        break;

    case SoundGetOutputVolume:
    case SoundGetInputVolume:
        IntegerUlong = Handle->Volume;
        if (Handle->Device->Type == SoundDeviceInput) {
            if (RequestCode == SoundGetOutputVolume) {
                IntegerUlong = 0;
            }

        } else {
            if (RequestCode == SoundGetInputVolume) {
                IntegerUlong = 0;
            }
        }

        CopyOutBuffer = &IntegerUlong;
        CopySize = sizeof(ULONG);
        break;

    case SoundSetOutputVolume:
    case SoundSetInputVolume:
        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        CopyOutBuffer = &IntegerUlong;
        if (Handle->Device->Type == SoundDeviceInput) {
            if (RequestCode == SoundSetOutputVolume) {
                IntegerUlong = 0;
                break;
            }

        } else {
            if (RequestCode == SoundSetInputVolume) {
                IntegerUlong = 0;
                break;
            }
        }

        Status = SoundpSetVolume(Handle, IntegerUlong);
        IntegerUlong = Handle->Volume;
        break;

    case SoundSetNonBlock:
        RtlAtomicOr32(&(Handle->Flags), SOUND_DEVICE_HANDLE_FLAG_NON_BLOCKING);
        break;

    case SoundSetLowThreshold:
        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        if (IntegerUlong < SOUND_CORE_LOW_TRESHOLD_MINIMUM) {
            IntegerUlong = SOUND_CORE_LOW_TRESHOLD_MINIMUM;
        }

        //
        // Synchronize with the buffer size changing. It's bad if the low
        // water mark is greater than the buffer size.
        //

        KeAcquireQueuedLock(Handle->Lock);
        if (IntegerUlong > Handle->Buffer.Size) {
            IntegerUlong = Handle->Buffer.Size;
        }

        Handle->Buffer.LowThreshold = IntegerUlong;
        KeReleaseQueuedLock(Handle->Lock);
        CopyOutBuffer = &(Handle->Buffer.LowThreshold);
        RtlAtomicOr32(&(Handle->Flags), SOUND_DEVICE_HANDLE_FLAG_LOW_WATER_SET);
        break;

    case SoundGetSupportedOutputRoutes:
    case SoundGetSupportedInputRoutes:
        CopySize = sizeof(SOUND_DEVICE_ROUTE_INFORMATION);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        //
        // Consider increasing the maximum if a device has a lot of routes.
        // This routine will currently truncate them.
        //

        RouteCount = SoundDevice->RouteCount;
        if (RouteCount > SOUND_ROUTE_COUNT_MAX) {
            RtlDebugPrint("SNDCORE: Truncating route report: %d\n", RouteCount);
            RouteCount = SOUND_ROUTE_COUNT_MAX;
        }

        RtlZeroMemory(&RouteInformation, CopySize);
        RouteInformation.RouteCount = RouteCount;
        NameIndex = 0;
        BytesRemaining = SOUND_ROUTE_NAME_SIZE;
        Routes = (PVOID)SoundDevice + SoundDevice->RoutesOffset;
        for (Index = 0; Index < RouteCount; Index += 1) {
            Name = SoundRouteNames[Routes[Index].Type];
            NameSize = RtlStringLength(Name) + 1;
            if (NameSize > BytesRemaining) {
                break;
            }

            RouteInformation.RouteIndex[Index] = NameIndex;
            RtlStringCopy(&(RouteInformation.RouteName[NameIndex]),
                          Name,
                          NameSize);

            NameIndex += NameSize;
        }

        CopyOutBuffer = &RouteInformation;
        break;

    case SoundGetOutputRoute:
    case SoundGetInputRoute:
        IntegerUlong = Handle->Route;
        if (Handle->Device->Type == SoundDeviceInput) {
            if (RequestCode == SoundGetOutputVolume) {
                IntegerUlong = 0;
            }

        } else {
            if (RequestCode == SoundGetInputVolume) {
                IntegerUlong = 0;
            }
        }

        CopyOutBuffer = &IntegerUlong;
        CopySize = sizeof(ULONG);
        break;

    case SoundSetOutputRoute:
    case SoundSetInputRoute:
        CopySize = sizeof(ULONG);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        CopyOutBuffer = &IntegerUlong;
        if (Handle->Device->Type == SoundDeviceInput) {
            if (RequestCode == SoundSetOutputVolume) {
                IntegerUlong = 0;
                break;
            }

        } else {
            if (RequestCode == SoundSetInputVolume) {
                IntegerUlong = 0;
                break;
            }
        }

        if (FromKernelMode != FALSE) {
            IntegerUlong = *((PULONG)RequestBuffer);

        } else {
            Status = MmCopyFromUserMode(&IntegerUlong, RequestBuffer, CopySize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        //
        // The route can only be set while in the uninitialized state.
        //

        if ((IntegerUlong < SoundDevice->RouteCount) &&
            (Handle->State < SoundDeviceStateInitialized)) {

            KeAcquireQueuedLock(Handle->Lock);
            if (Handle->State < SoundDeviceStateInitialized) {
                Handle->Route = IntegerUlong;
            }

            KeReleaseQueuedLock(Handle->Lock);
        }

        IntegerUlong = Handle->Route;
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto UserControlEnd;
    }

    if (CopyOutBuffer != NULL) {
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto UserControlEnd;
        }

        if (FromKernelMode != FALSE) {
            RtlCopyMemory(RequestBuffer, CopyOutBuffer, CopySize);

        } else {
            Status = MmCopyToUserMode(RequestBuffer,
                                      CopyOutBuffer,
                                      CopySize);

            if (!KSUCCESS(Status)) {
                goto UserControlEnd;
            }
        }

        goto UserControlEnd;
    }

UserControlEnd:
    return Status;
}

SOUND_API
KSTATUS
SoundGetSetDeviceInformation (
    PSOUND_CONTROLLER Controller,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets device information for a sound controller.

Arguments:

    Controller - Supplies a pointer to the sound core library's controller.

    Uuid - Supplies a pointer to the information identifier.

    Data - Supplies a pointer to the data buffer.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer in bytes. On output, returns the needed size of the data buffer,
        even if the supplied buffer was nonexistant or too small.

    Set - Supplies a boolean indicating whether to get the information (FALSE)
        or set the information (TRUE).

Return Value:

    Status code.

--*/

{

    ULONG DeviceTypeCount[SoundDeviceTypeCount];
    ULONG Index;
    PSOUND_DEVICE_INFORMATION Information;
    KSTATUS Status;

    if (Controller == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto GetSetDeviceInformationEnd;
    }

    Status = STATUS_NOT_HANDLED;
    if (RtlAreUuidsEqual(Uuid, &SoundDeviceInformationUuid) != FALSE) {
        if (*DataSize < sizeof(SOUND_DEVICE_INFORMATION)) {
            *DataSize = sizeof(SOUND_DEVICE_INFORMATION);
            Status = STATUS_BUFFER_TOO_SMALL;
            goto GetSetDeviceInformationEnd;
        }

        *DataSize = sizeof(SOUND_DEVICE_INFORMATION);
        if (Set != FALSE) {
            Status = STATUS_NOT_SUPPORTED;
            goto GetSetDeviceInformationEnd;
        }

        Information = (PSOUND_DEVICE_INFORMATION)Data;
        if (Information->Version < SOUND_DEVICE_INFORMATION_VERSION) {
            Status = STATUS_INVALID_PARAMETER;
            goto GetSetDeviceInformationEnd;
        }

        RtlZeroMemory(DeviceTypeCount, sizeof(DeviceTypeCount));
        for (Index = 0; Index < Controller->Host.DeviceCount; Index += 1) {
            DeviceTypeCount[Controller->Host.Devices[Index]->Type] += 1;
        }

        //
        // No sound device flags are defined yet.
        //

        Information->Flags = 0;
        Information->InputDeviceCount = DeviceTypeCount[SoundDeviceInput];
        Information->OutputDeviceCount = DeviceTypeCount[SoundDeviceOutput];
        Status = STATUS_SUCCESS;
        goto GetSetDeviceInformationEnd;
    }

GetSetDeviceInformationEnd:
    return Status;
}

SOUND_API
VOID
SoundUpdateBufferState (
    PSOUND_IO_BUFFER Buffer,
    SOUND_DEVICE_TYPE Type,
    UINTN Offset
    )

/*++

Routine Description:

    This routine updates the given buffer's state in a lock-less way. It will
    increment the total bytes processes and signal the I/O state if necessary.
    It assumes, however, that the sound controller has some sort of
    synchronization to prevent this routine from being called simultaneously
    for the same buffer.

Arguments:

    Buffer - Supplies a pointer to the sound I/O buffer whose state needs to be
        updated.

    Type - Supplies the type of sound device to which the buffer belongs.

    Offset - Supplies the hardware's updated offset within the I/O buffer.

Return Value:

    None.

--*/

{

    UINTN BytesCompleted;
    UINTN OldOffset;

    //
    // Update the buffer's total bytes completed by the hardware before
    // updating the controller offset. It's assumed the controller has some
    // synchronization to protect this update.
    //

    OldOffset = Buffer->ControllerOffset;
    if (OldOffset < Offset) {
        BytesCompleted = Offset - OldOffset;

    } else {
        BytesCompleted = (Buffer->Size - OldOffset) + Offset;
    }

    Buffer->BytesCompleted += BytesCompleted;
    SoundpUpdateBufferState(Buffer, Type, Offset, BytesCompleted, FALSE);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
SoundpAllocateIoBuffer (
    PSOUND_CONTROLLER Controller,
    PSOUND_DEVICE Device,
    UINTN FragmentSize,
    UINTN FragmentCount,
    PIO_BUFFER *NewIoBuffer
    )

/*++

Routine Description:

    This routine allocates an I/O buffer that will be passed to the host
    controller during I/O. If the host controller supports DMA, it should have
    provided an allocation routine to use, as there may be device specific
    alignment and mapping requirements. Otherwise, just allocate a generic
    I/O buffer on behalf of device that can only do polled I/O.

Arguments:

    Controller - Supplies a pointer to a sound controller.

    Device - Supplies a pointer to a sound device.

    FragmentSize - Supplies the size of a fragment, in bytes.

    FragmentCount - Supplies the desired number of fragments.

    NewIoBuffer - Supplies a pointer that receives a pointer to the newly
        allocated buffer.

Return Value:

    Status code.

--*/

{

    PSOUND_ALLOCATE_DMA_BUFFER AllocateDmaBuffer;
    UINTN BufferSize;
    PIO_BUFFER IoBuffer;
    KSTATUS Status;

    IoBuffer = NULL;
    Status = STATUS_SUCCESS;
    BufferSize = FragmentSize * FragmentCount;
    AllocateDmaBuffer = Controller->Host.FunctionTable->AllocateDmaBuffer;
    if (AllocateDmaBuffer != NULL) {
        Status = AllocateDmaBuffer(Controller->Host.Context,
                                   Device->Context,
                                   FragmentSize,
                                   FragmentCount,
                                   &IoBuffer);

        if (!KSUCCESS(Status)) {
            goto AllocateIoBufferEnd;
        }

    } else if ((Device->Capabilities & SOUND_CAPABILITY_MMAP) != 0) {
        IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                              MAX_ULONGLONG,
                                              0,
                                              BufferSize,
                                              0);

        if (IoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AllocateIoBufferEnd;
        }

    } else {
        IoBuffer = MmAllocatePagedIoBuffer(BufferSize, 0);
        if (IoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AllocateIoBufferEnd;
        }
    }

    //
    // Zero the entire I/O buffer so that any unused portions produce no sound
    // even if they are played by the hardware.
    //

    Status = MmZeroIoBuffer(IoBuffer, 0, BufferSize);
    if (!KSUCCESS(Status)) {
        goto AllocateIoBufferEnd;
    }

AllocateIoBufferEnd:
    if (!KSUCCESS(Status)) {
        if (IoBuffer != NULL) {
            SoundpFreeIoBuffer(Controller, Device, IoBuffer);
            IoBuffer = NULL;
        }
    }

    *NewIoBuffer = IoBuffer;
    return Status;
}

VOID
SoundpFreeIoBuffer (
    PSOUND_CONTROLLER Controller,
    PSOUND_DEVICE Device,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine destroys an I/O buffer allocated for a device.

Arguments:

    Controller - Supplies a pointer to the sound controller for which the
        buffer was allocated.

    Device - Supplies a pointer to the sound device for which the buffer was
        allocated.

    IoBuffer - Supplies a pointer to the I/O buffer to free.

Return Value:

    None.

--*/

{

    PSOUND_FREE_DMA_BUFFER FreeDmaBuffer;

    FreeDmaBuffer = Controller->Host.FunctionTable->FreeDmaBuffer;
    if (FreeDmaBuffer != NULL) {
        FreeDmaBuffer(Controller->Host.Context, Device->Context, IoBuffer);

    } else {
        MmFreeIoBuffer(IoBuffer);
    }

    return;
}

KSTATUS
SoundpCopyBufferData (
    PSOUND_DEVICE_HANDLE Handle,
    PIO_BUFFER Destination,
    UINTN DestinationOffset,
    PIO_BUFFER Source,
    UINTN SourceOffset,
    UINTN Size
    )

/*++

Routine Description:

    This routine copies sound data from one I/O buffer to another. This gives
    the sound controller an opportunity to do any conversions if its audio
    format does not conform to one of sound core's formats. One of the two
    buffers will be the buffer supplied to the sound controller when the
    device was put in the initialized state. Which one it is depends on the
    direction of the audio.

Arguments:

    Handle - Supplies a pointer to a handle to the device to which to copy
        to/from.

    Destination - Supplies a pointer to the destination I/O buffer that is to
        be copied into.

    DestinationOffset - Supplies the offset into the destination I/O buffer
        where the copy should begin.

    Source - Supplies a pointer to the source I/O buffer whose contexts will be
        copied to the destination.

    SourceOffset - Supplies the offset into the source I/O buffer where the
        copy should begin.

    Size - Supplies the size of the copy, in bytes.

Return Value:

    Status code.

--*/

{

    PSOUND_COPY_BUFFER_DATA CopyBufferData;
    KSTATUS Status;

    CopyBufferData = Handle->Controller->Host.FunctionTable->CopyBufferData;
    if (CopyBufferData != NULL) {
        Status = CopyBufferData(Handle->Controller->Host.Context,
                                Handle->Device->Context,
                                Destination,
                                DestinationOffset,
                                Source,
                                SourceOffset,
                                Size);

    } else {
        Status = MmCopyIoBuffer(Destination,
                                DestinationOffset,
                                Source,
                                SourceOffset,
                                Size);
    }

    return Status;
}

KSTATUS
SoundpInitializeDevice (
    PSOUND_DEVICE_HANDLE Handle
    )

/*++

Routine Description:

    This routine initializes a sound device, preparing it to input or outpu
    sound data. This assumes the handle's queued lock is held.

Arguments:

    Handle - Supplies a pointer to a handle to the device to initialize.

Return Value:

    Status code.

--*/

{

    PSOUND_CONTROLLER Controller;
    PSOUND_GET_SET_INFORMATION GetSetInformation;
    SOUND_DEVICE_STATE_INFORMATION Information;
    PSOUND_DEVICE_ROUTE Routes;
    UINTN Size;
    KSTATUS Status;

    ASSERT(KeIsQueuedLockHeld(Handle->Lock) != FALSE);
    ASSERT(Handle->State < SoundDeviceStateInitialized);

    //
    // Initialize the sound controller device.
    //

    Information.Version = SOUND_DEVICE_STATE_INFORMATION_VERSION;
    Information.State = SoundDeviceStateInitialized;
    Information.U.Initialize.Buffer = &(Handle->Buffer);
    Information.U.Initialize.Format = Handle->Format;
    Information.U.Initialize.ChannelCount = Handle->ChannelCount;
    Information.U.Initialize.SampleRate = Handle->SampleRate;
    Information.U.Initialize.Volume = Handle->Volume;
    Routes = (PVOID)Handle->Device + Handle->Device->RoutesOffset;
    Information.U.Initialize.RouteContext = Routes[Handle->Route].Context;
    Controller = Handle->Controller;
    Size = sizeof(SOUND_DEVICE_STATE_INFORMATION);
    GetSetInformation = Controller->Host.FunctionTable->GetSetInformation;
    Status = GetSetInformation(Controller->Host.Context,
                               Handle->Device->Context,
                               SoundDeviceInformationState,
                               &Information,
                               &Size,
                               TRUE);

    if (!KSUCCESS(Status)) {
        goto InitializeDeviceEnd;
    }

    Handle->State = SoundDeviceStateInitialized;

    //
    // If this is an output stream, signal that it is ready for writes into
    // the whole buffer.
    //

    if (Handle->Device->Type == SoundDeviceOutput) {
        SoundpUpdateBufferState(&(Handle->Buffer),
                                Handle->Device->Type,
                                0,
                                Handle->Buffer.Size,
                                TRUE);
    }

InitializeDeviceEnd:
    return Status;
}

KSTATUS
SoundpResetDevice (
    PSOUND_DEVICE_HANDLE Handle
    )

/*++

Routine Description:

    This routine resets a device, releasing it to operate on behalf of another
    handle.

Arguments:

    Handle - Supplies a pointer to a handle to the device to reset.

Return Value:

    Status code.

--*/

{

    PSOUND_CONTROLLER Controller;
    PSOUND_GET_SET_INFORMATION GetSetInformation;
    SOUND_DEVICE_STATE_INFORMATION Information;
    UINTN Size;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    KeAcquireQueuedLock(Handle->Lock);
    if (Handle->State != SoundDeviceStateUninitialized) {
        Information.Version = SOUND_DEVICE_STATE_INFORMATION_VERSION;
        Information.State = SoundDeviceStateUninitialized;
        Controller = Handle->Controller;
        Size = sizeof(SOUND_DEVICE_STATE_INFORMATION);
        GetSetInformation = Controller->Host.FunctionTable->GetSetInformation;
        Status = GetSetInformation(Controller->Host.Context,
                                   Handle->Device->Context,
                                   SoundDeviceInformationState,
                                   &Information,
                                   &Size,
                                   TRUE);

        if (!KSUCCESS(Status)) {
            goto UninitializeDeviceEnd;
        }
    }

    //
    // The buffer was allocated based on the current fragment size and count,
    // which are about to be reset.
    //

    if (Handle->Buffer.IoBuffer != NULL) {

        ASSERT(Handle->Device != NULL);

        SoundpFreeIoBuffer(Handle->Controller,
                           Handle->Device,
                           Handle->Buffer.IoBuffer);

        Handle->Buffer.IoBuffer = NULL;
    }

    //
    // Reinitialize the default values.
    //

    SoundpSetHandleDefaults(Handle);
    Handle->State = SoundDeviceStateUninitialized;

UninitializeDeviceEnd:
    KeReleaseQueuedLock(Handle->Lock);
    return Status;
}

KSTATUS
SoundpStartDevice (
    PSOUND_DEVICE_HANDLE Handle
    )

/*++

Routine Description:

    This routine starts a sound device so that it starts playing or recording
    sound.

Arguments:

    Handle - Supplies a pointer to a handle to the device to start.

Return Value:

    Status code.

--*/

{

    PSOUND_CONTROLLER Controller;
    PSOUND_GET_SET_INFORMATION GetSetInformation;
    SOUND_DEVICE_STATE_INFORMATION Information;
    UINTN Size;
    ULONG SoundFlags;
    KSTATUS Status;

    ASSERT(Handle->State > SoundDeviceStateUninitialized);

    //
    // If input/output is not enabled, do not start the device. An unsuccessful
    // start request because the device is not enabled should not be fatal.
    //

    SoundFlags = SOUND_DEVICE_FLAG_INTERNAL_ENABLE_INPUT;
    if (Handle->Device->Type == SoundDeviceOutput) {
        SoundFlags = SOUND_DEVICE_FLAG_INTERNAL_ENABLE_OUTPUT;
    }

    if ((Handle->Device->Flags & SoundFlags) != SoundFlags) {
        return STATUS_SUCCESS;
    }

    Status = STATUS_SUCCESS;
    KeAcquireQueuedLock(Handle->Lock);
    if (Handle->State != SoundDeviceStateRunning) {
        Information.Version = SOUND_DEVICE_STATE_INFORMATION_VERSION;
        Information.State = SoundDeviceStateRunning;
        Controller = Handle->Controller;
        Size = sizeof(SOUND_DEVICE_STATE_INFORMATION);
        GetSetInformation = Controller->Host.FunctionTable->GetSetInformation;
        Status = GetSetInformation(Controller->Host.Context,
                                   Handle->Device->Context,
                                   SoundDeviceInformationState,
                                   &Information,
                                   &Size,
                                   TRUE);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Handle->State = SoundDeviceStateRunning;
    }

StartDeviceEnd:
    KeReleaseQueuedLock(Handle->Lock);
    return Status;
}

KSTATUS
SoundpSetVolume (
    PSOUND_DEVICE_HANDLE Handle,
    ULONG Volume
    )

/*++

Routine Description:

    This routine sets the volume for the given sound core device. It calls
    down to the lower level sound controller in case it needs to modify start
    or hardware settings.

Arguments:

    Handle - Supplies a pointer to the handle of the sound device whose volume
        is to be changed.

    Volume - Supplies the volume to set. See SOUND_VOLUME_* for definitions.

Return Value:

    Status code.

--*/

{

    PSOUND_CONTROLLER Controller;
    PSOUND_GET_SET_INFORMATION GetSetInformation;
    ULONG LeftVolume;
    ULONG RightVolume;
    UINTN Size;
    ULONG Status;

    LeftVolume = (Volume & SOUND_VOLUME_LEFT_CHANNEL_MASK) >>
                 SOUND_VOLUME_LEFT_CHANNEL_SHIFT;

    if (LeftVolume > SOUND_VOLUME_MAXIMUM) {
        LeftVolume = SOUND_VOLUME_MAXIMUM;
    }

    RightVolume = (Volume & SOUND_VOLUME_RIGHT_CHANNEL_MASK) >>
                  SOUND_VOLUME_RIGHT_CHANNEL_SHIFT;

    if (RightVolume > SOUND_VOLUME_MAXIMUM) {
        RightVolume = SOUND_VOLUME_MAXIMUM;
    }

    Volume = (LeftVolume << SOUND_VOLUME_LEFT_CHANNEL_SHIFT) |
             (RightVolume << SOUND_VOLUME_RIGHT_CHANNEL_SHIFT);

    //
    // Synchronize attempts to change the hardware's volume. This prevents each
    // controller's driver from having to implement protection for this call.
    //

    KeAcquireQueuedLock(Handle->Lock);
    Size = sizeof(ULONG);
    Controller = Handle->Controller;
    GetSetInformation = Controller->Host.FunctionTable->GetSetInformation;
    Status = GetSetInformation(Controller->Host.Context,
                               Handle->Device->Context,
                               SoundDeviceInformationVolume,
                               &Volume,
                               &Size,
                               TRUE);

    if (!KSUCCESS(Status)) {
        goto SetVolumeEnd;
    }

    Handle->Volume = Volume;

SetVolumeEnd:
    KeReleaseQueuedLock(Handle->Lock);
    return Status;
}

VOID
SoundpControllerAddReference (
    PSOUND_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine adds a reference on a sound core controller.

Arguments:

    Controller - Supplies a pointer to a sound core controller.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Controller->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x10000000);

    return;
}

VOID
SoundpControllerReleaseReference (
    PSOUND_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine releases a reference on a sound core controller.

Arguments:

    Controller - Supplies a pointer to a sound core controller.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Controller->ReferenceCount),
                                       (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        SoundpDestroyController(Controller);
    }

    return;
}

VOID
SoundpDestroyController (
    PSOUND_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys a sound core controller and all of its resources.

Arguments:

    Controller - Supplies a pointer to a sound core controler to destroy.

Return Value:

    None.

--*/

{

    IoRegisterDeviceInformation(Controller->Host.OsDevice,
                                &SoundDeviceInformationUuid,
                                FALSE);

    IoDeviceReleaseReference(Controller->Host.OsDevice);
    MmFreePagedPool(Controller);
    return;
}

KSTATUS
SoundpEnumerateDirectory(
    PSOUND_CONTROLLER Controller,
    PIO_BUFFER IoBuffer,
    PIO_OFFSET EntryOffset,
    UINTN SizeInBytes,
    PUINTN BytesRead
    )

/*++

Routine Description:

    This routine reports the controller's devices as directory entries.

Arguments:

    Controller - Supplies a pointer to the sound controller.

    IoBuffer - Supplies a pointer to I/O buffer that receives the device
        entries.

    EntryOffset - Supplies an offset into the "directory", in terms of entries,
        where the enumerate should begin. On output, this stores the offset of
        the first unread entry.

    SizeInBytes - Supplies the number of bytes to read from the "directory".

    BytesRead - Supplies a pointer that on input contains the number of bytes
        already read into the buffer. On output, accumulates any additional
        bytes read into the buffer.

Return Value:

    Status code.

--*/

{

    UINTN BytesWritten;
    PSOUND_DEVICE Device;
    ULONG DeviceIndex;
    ULONG EntriesRead;
    DIRECTORY_ENTRY Entry;
    UINTN EntrySize;
    ULONG FinalNameSize;
    PCSTR Format;
    PCSTR GenericName;
    CHAR Name[SOUND_MAX_DEVICE_NAME_SIZE];
    ULONG NameSize;
    IO_OFFSET NextOffset;
    UINTN SpaceLeft;
    ULONG StartIndex;
    KSTATUS Status;
    ULONG TypeIndex;
    ULONG TypeIndices[SoundDeviceTypeCount];

    BytesWritten = *BytesRead;
    SpaceLeft = SizeInBytes - BytesWritten;
    EntriesRead = 0;
    NextOffset = *EntryOffset;

    ASSERT(*EntryOffset >= DIRECTORY_CONTENTS_OFFSET);
    ASSERT(*EntryOffset < (LONGLONG)MAX_LONG);

    //
    // Shave off the . and .. directories to get to the device index.
    //

    StartIndex = (ULONG)(*EntryOffset - DIRECTORY_CONTENTS_OFFSET);

    //
    // Iterate through the devices. Determine the name for each based on the
    // type.
    //

    DeviceIndex = StartIndex;
    while (DeviceIndex < Controller->Host.DeviceCount) {

        //
        // If this is the first time looking through the devices, initializes
        // the type indices, up to the starting index.
        //

        if (DeviceIndex == StartIndex) {
            RtlZeroMemory(TypeIndices, sizeof(TypeIndices));
            for (DeviceIndex = 0; DeviceIndex < StartIndex; DeviceIndex += 1) {
                Device = Controller->Host.Devices[DeviceIndex];
                TypeIndices[Device->Type] += 1;
            }
        }

        Device = Controller->Host.Devices[DeviceIndex];
        TypeIndex = TypeIndices[Device->Type];
        TypeIndices[Device->Type] += 1;
        Format = SoundSpecificDeviceFormats[Device->Type];
        NameSize = RtlPrintToString(NULL,
                                    0,
                                    CharacterEncodingDefault,
                                    Format,
                                    TypeIndex);

        EntrySize = ALIGN_RANGE_UP(sizeof(DIRECTORY_ENTRY) + NameSize, 8);
        if (EntrySize > SpaceLeft) {
            Status = STATUS_MORE_PROCESSING_REQUIRED;
            goto EnumerateDirectoryEnd;
        }

        NextOffset += 1;
        Entry.Size = EntrySize;
        Entry.FileId = (FILE_ID)(UINTN)Device;
        Entry.NextOffset = NextOffset;
        Entry.Type = IoObjectCharacterDevice;
        Status = MmCopyIoBufferData(IoBuffer,
                                    &Entry,
                                    BytesWritten,
                                    sizeof(DIRECTORY_ENTRY),
                                    TRUE);

        if (!KSUCCESS(Status)) {
            goto EnumerateDirectoryEnd;
        }

        FinalNameSize = RtlPrintToString(Name,
                                         sizeof(Name),
                                         CharacterEncodingDefault,
                                         Format,
                                         TypeIndex);

        ASSERT(FinalNameSize == NameSize);

        Status = MmCopyIoBufferData(IoBuffer,
                                    Name,
                                    BytesWritten + sizeof(DIRECTORY_ENTRY),
                                    NameSize,
                                    TRUE);

        if (!KSUCCESS(Status)) {
            goto EnumerateDirectoryEnd;
        }

        BytesWritten += EntrySize;
        SpaceLeft -= EntrySize;
        EntriesRead += 1;
        DeviceIndex += 1;
    }

    //
    // Add the generic pseudo-devices. Subtract off the device count to get
    // the right offset.
    //

    DeviceIndex -= Controller->Host.DeviceCount;
    while (DeviceIndex < SoundDeviceTypeCount) {
        GenericName = SoundGenericDeviceNames[DeviceIndex];
        NameSize = RtlStringLength(GenericName) + 1;
        EntrySize = ALIGN_RANGE_UP(sizeof(DIRECTORY_ENTRY) + NameSize, 8);
        if (EntrySize > SpaceLeft) {
            Status = STATUS_MORE_PROCESSING_REQUIRED;
            goto EnumerateDirectoryEnd;
        }

        NextOffset += 1;
        Entry.Size = EntrySize;
        Entry.FileId = DeviceIndex;
        Entry.NextOffset = NextOffset;
        Entry.Type = IoObjectCharacterDevice;
        Status = MmCopyIoBufferData(IoBuffer,
                                    &Entry,
                                    BytesWritten,
                                    sizeof(DIRECTORY_ENTRY),
                                    TRUE);

        if (!KSUCCESS(Status)) {
            goto EnumerateDirectoryEnd;
        }

        Status = MmCopyIoBufferData(IoBuffer,
                                    (PSTR)GenericName,
                                    BytesWritten + sizeof(DIRECTORY_ENTRY),
                                    NameSize,
                                    TRUE);

        if (!KSUCCESS(Status)) {
            goto EnumerateDirectoryEnd;
        }

        BytesWritten += EntrySize;
        SpaceLeft -= EntrySize;
        EntriesRead += 1;
        DeviceIndex += 1;
    }

    if (EntriesRead == 0) {
        Status = STATUS_END_OF_FILE;
        goto EnumerateDirectoryEnd;
    }

    Status = STATUS_SUCCESS;

EnumerateDirectoryEnd:
    *BytesRead = BytesWritten;
    *EntryOffset += EntriesRead;
    return Status;
}

ULONG
SoundpFindNearestRate (
    PSOUND_DEVICE SoundDevice,
    ULONG DesiredRate
    )

/*++

Routine Description:

    This routine returns the rate closest to the desired rate that is still
    supported by the device.

Arguments:

    SoundDevice - Supplies a pointer to the sound device whose rates are to be
        searched.

    DesiredRate - Supplies the desired rate, in Hz.

Return Value:

    Returns the nearest rate supported by the given device.

--*/

{

    ULONG HigherRate;
    ULONG Index;
    ULONG LowerRate;
    PULONG Rates;
    ULONG SelectedRate;

    Rates = (PVOID)SoundDevice + SoundDevice->RatesOffset;
    LowerRate = Rates[0];
    HigherRate = LowerRate;
    for (Index = 0; Index < SoundDevice->RateCount; Index += 1) {
        HigherRate = Rates[Index];
        if (DesiredRate <= HigherRate) {
            break;
        }

        LowerRate = HigherRate;
    }

    if (LowerRate == HigherRate) {
        SelectedRate = LowerRate;

    } else {
        if ((HigherRate - DesiredRate) < (DesiredRate - LowerRate)) {
            SelectedRate = HigherRate;

        } else {
            SelectedRate = LowerRate;
        }
    }

    return SelectedRate;
}

VOID
SoundpUpdateBufferState (
    PSOUND_IO_BUFFER Buffer,
    SOUND_DEVICE_TYPE Type,
    UINTN Offset,
    UINTN BytesAvailable,
    BOOL SoundCore
    )

/*++

Routine Description:

    This routine updates the given buffer's state in a lock-less way. It
    updates either the sound core or controller offset and updates the total
    number of bytes processed by the controller a new controller offset is
    supplied.

Arguments:

    Buffer - Supplies a pointer to the sound I/O buffer whose state needs to be
        updated.

    Type - Supplies the type of sound device to which the buffer belongs.

    Offset - Supplies the sound core or controller's updated offset within the
        I/O buffer.

    BytesAvailable - Supplies the number of bytes available that need to be
        added to the buffer's count.

    SoundCore - Supplies a boolean indicating whether or not the offset is for
        the sound core (TRUE) of the sound controller (FALSE).

Return Value:

    None.

--*/

{

    UINTN CompareBytes;
    ULONG Events;
    UINTN NewBytes;
    UINTN OldBytes;
    BOOL Set;

    if (SoundCore == FALSE) {
        Buffer->ControllerOffset = Offset;

    } else {
        Buffer->CoreOffset = Offset;
    }

    //
    // If bytes became available, add them to the available bytes. If the count
    // of available bytes is greater than the buffer size, that means sound
    // core is behind. Readjust the available bytes such that the missed bytes
    // are no longer available. These bytes may be coming from sound core, if
    // sound core "over consumed" when it atomically zero'd the available
    // bytes.
    //

    if (BytesAvailable != 0) {
        OldBytes = Buffer->BytesAvailable;
        do {
            NewBytes = OldBytes + BytesAvailable;
            if (NewBytes > Buffer->Size) {
                NewBytes = REMAINDER(NewBytes, Buffer->Size);

                //
                // If the core is behind by multiple whole buffers, report that
                // the last one is available.
                //

                if (NewBytes == 0) {
                    NewBytes = Buffer->Size;
                }
            }

            CompareBytes = OldBytes;
            OldBytes = RtlAtomicCompareExchange(&(Buffer->BytesAvailable),
                                                NewBytes,
                                                CompareBytes);

        } while (OldBytes != CompareBytes);
    }

    //
    // Pick the correct events based on the device type.
    //

    Events = POLL_EVENT_IN;
    if (Type == SoundDeviceOutput) {
        Events = POLL_EVENT_OUT;
    }

    //
    // As long as there are more than the low threshold of bytes available,
    // signal the event. Do this in a loop as sound core and the controller
    // can race to set the object state.
    //

    do {
        Set = FALSE;
        OldBytes = Buffer->BytesAvailable;
        if (OldBytes >= Buffer->LowThreshold) {
            Set = TRUE;
        }

        IoSetIoObjectState(Buffer->IoState, Events, Set);

    } while (OldBytes != Buffer->BytesAvailable);

    return;
}

VOID
SoundpSetBufferSize (
    PSOUND_DEVICE_HANDLE Handle,
    UINTN FragmentCount,
    UINTN FragmentSize
    )

/*++

Routine Description:

    This routine attempts to set the buffer size for the given handle. It
    assumes that the caller has the appropriate protection - either
    initializing the handle or holds the handle's lock.

Arguments:

    Handle - Supplies a pointer to the sound device handle.

    FragmentCount - Supplies the desired number of fragments.

    FragmentSize - Supplies the desired fragment size, in bytes.

Return Value:

    None.

--*/

{

    UINTN BufferSize;
    UINTN Shift;

    if (Handle->State < SoundDeviceStateInitialized) {

        //
        // Find the closest power of 2 fragment count that is less than the
        // supplied value.
        //

        if (FragmentCount != 0) {
            Shift = RtlCountLeadingZeros(FragmentCount);
            Shift = (sizeof(FragmentCount) * BITS_PER_BYTE) - 1 - Shift;
            FragmentCount = 1 << Shift;
        }

        if (FragmentCount < Handle->Controller->Host.MinFragmentCount) {
            FragmentCount = Handle->Controller->Host.MinFragmentCount;

        } else if (FragmentCount > Handle->Controller->Host.MaxFragmentCount) {
            FragmentCount = Handle->Controller->Host.MaxFragmentCount;
        }

        if (FragmentSize > Handle->Controller->Host.MaxFragmentSize) {
            FragmentSize = Handle->Controller->Host.MaxFragmentSize;

        } else if (FragmentSize < Handle->Controller->Host.MinFragmentSize) {
            FragmentSize = Handle->Controller->Host.MinFragmentSize;
        }

        //
        // If this fails, something isn't quite right. The driver's maximum
        // fragment count and maximumg fragment size multiply to a value larger
        // than the maximum buffer size.
        //

        BufferSize = FragmentSize * FragmentCount;
        if (BufferSize > Handle->Controller->Host.MaxBufferSize) {

            ASSERT(BufferSize <= Handle->Controller->Host.MaxBufferSize);

            return;
        }

        ASSERT(POWER_OF_2(FragmentCount) != FALSE);
        ASSERT(POWER_OF_2(FragmentSize) != FALSE);
        ASSERT(POWER_OF_2(BufferSize) != FALSE);

        Handle->Buffer.Size = BufferSize;
        Handle->Buffer.FragmentCount = FragmentCount;
        Handle->Buffer.FragmentSize = FragmentSize;
        Handle->Buffer.FragmentShift = RtlCountTrailingZeros(FragmentSize);

        //
        // If the low water mark was not manually set, adjust it to the
        // fragment size. If it was set, assume the handle owner got it right
        // for their latency needs.
        //

        if ((Handle->Flags & SOUND_DEVICE_HANDLE_FLAG_LOW_WATER_SET) == 0) {
            Handle->Buffer.LowThreshold = FragmentSize;
        }
    }

    return;
}

VOID
SoundpSetHandleDefaults (
    PSOUND_DEVICE_HANDLE Handle
    )

/*++

Routine Description:

    This routine sets default values in the given sound device handle.

Arguments:

    Handle - Supplies a pointer to the sound device handle.

Return Value:

    None.

--*/

{

    PSOUND_DEVICE SoundDevice;
    ULONG SoundFlags;

    Handle->Buffer.CoreOffset = 0;
    Handle->Buffer.ControllerOffset = 0;
    Handle->Buffer.BytesAvailable = 0;
    Handle->Buffer.BytesCompleted = 0;
    Handle->Buffer.FragmentsCompleted = 0;
    SoundpSetBufferSize(Handle,
                        SOUND_FRAGMENT_COUNT_DEFAULT,
                        SOUND_FRAGMENT_SIZE_DEFAULT);

    //
    // By default, the low byte signal threshold is equal to a fragment size.
    //

    Handle->Buffer.LowThreshold = Handle->Buffer.FragmentSize;
    RtlAtomicAnd32(&(Handle->Flags), ~SOUND_DEVICE_HANDLE_FLAG_LOW_WATER_SET);
    SoundDevice = Handle->Device;
    if (SoundDevice != NULL) {
        Handle->Format = 1 << RtlCountTrailingZeros32(SoundDevice->Formats);
        Handle->ChannelCount = SoundDevice->MaxChannelCount;
        Handle->SampleRate = SoundpFindNearestRate(SoundDevice,
                                                   SOUND_SAMPLE_RATE_DEFAULT);

        //
        // Reset the device to automatically start on the first read/write.
        //

        SoundFlags = SOUND_DEVICE_FLAG_INTERNAL_ENABLE_INPUT;
        if (Handle->Device->Type == SoundDeviceOutput) {
            SoundFlags = SOUND_DEVICE_FLAG_INTERNAL_ENABLE_OUTPUT;
        }

        RtlAtomicOr32(&(SoundDevice->Flags), SoundFlags);
    }

    Handle->Volume = SOUND_VOLUME_DEFAULT;
    Handle->Route = 0;
    return;
}

