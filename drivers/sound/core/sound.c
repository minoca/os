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
SoundpUpdateBufferIoState (
    PSOUND_IO_BUFFER Buffer,
    ULONG Events,
    BOOL SoundCore
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
        (Registration->FunctionTable->GetSetInformation == NULL)) {

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
        if (SoundDevice->RateCount == 0) {
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

        //
        // There is nothing preventing a device from supporting mmap or being
        // manually started (rather than automatically started via read/write).
        // Set them on all devices.
        //

        SoundDevice->Capabilities |= SOUND_CAPABILITY_MMAP |
                                     SOUND_CAPABILITY_MANUAL_ENABLE;

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
    IO_OBJECT_TYPE ObjectType;
    FILE_PERMISSIONS Permissions;
    PFILE_PROPERTIES Properties;
    PSOUND_DEVICE SoundDevice;
    KSTATUS Status;
    SOUND_DEVICE_TYPE Type;
    ULONG TypeIndex;

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

            SoundDevice = Controller->Host.Devices[DeviceIndex];
            if (SoundDevice->Type == FileId) {
                break;
            }
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

    NewHandle = MmAllocatePagedPool(sizeof(SOUND_DEVICE_HANDLE),
                                    SOUND_CORE_ALLOCATION_TAG);

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

    ULONG OldFlags;

    SoundpResetDevice(Handle);
    if (Handle->Device != NULL) {
        OldFlags = RtlAtomicAnd32(&(Handle->Device->Flags),
                                  ~SOUND_DEVICE_FLAG_INTERNAL_BUSY);

        ASSERT((OldFlags & SOUND_DEVICE_FLAG_INTERNAL_BUSY) != 0);
    }

    ASSERT(Handle->Buffer.IoBuffer == NULL);

    SoundpControllerReleaseReference(Handle->Controller);
    if (Handle->Lock != NULL) {
        KeDestroyQueuedLock(Handle->Lock);
    }

    MmFreePagedPool(Handle);
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
    UINTN ControllerOffset;
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
        CoreOffset = Handle->Buffer.CoreOffset;
        ControllerOffset = Handle->Buffer.ControllerOffset;

        //
        // If the core offset is greater than the controller offset, then two
        // I/O's are required. Do the first portion from the core offset to the
        // end of the cyclic buffer.
        //

        if (CoreOffset > ControllerOffset) {
            BytesAvailable = CyclicBufferSize - CoreOffset;
            if (BytesAvailable > BytesRemaining) {
                BytesAvailable = BytesRemaining;
            }

            *CyclicOffset = CoreOffset;
            Status = MmCopyIoBuffer(DestinationBuffer,
                                    DestinationOffset,
                                    SourceBuffer,
                                    SourceOffset,
                                    BytesAvailable);

            if (!KSUCCESS(Status)) {
                goto PerformIoEnd;
            }

            *LinearOffset += BytesAvailable;
            BytesRemaining -= BytesAvailable;
            CoreOffset = 0;
        }

        //
        // If the core offset is less than the controller offset, then perform
        // the rest of the I/O.
        //

        if ((BytesRemaining != 0) && (CoreOffset < ControllerOffset)) {
            BytesAvailable = ControllerOffset - CoreOffset;
            if (BytesAvailable > BytesRemaining) {
                BytesAvailable = BytesRemaining;
            }

            *CyclicOffset = CoreOffset;
            Status = MmCopyIoBuffer(DestinationBuffer,
                                    DestinationOffset,
                                    SourceBuffer,
                                    SourceOffset,
                                    BytesAvailable);

            if (!KSUCCESS(Status)) {
                goto PerformIoEnd;
            }

            *LinearOffset += BytesAvailable;
            BytesRemaining -= BytesAvailable;
            CoreOffset += BytesAvailable;
            if (CoreOffset == CyclicBufferSize) {
                CoreOffset = 0;
            }
        }

        Handle->Buffer.CoreOffset = CoreOffset;
        SoundpUpdateBufferIoState(&(Handle->Buffer), Events, TRUE);
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

    UINTN BufferSize;
    ULONG ChannelCount;
    ULONG ClearFlags;
    UINTN ControllerOffset;
    PVOID CopyOutBuffer;
    UINTN CopySize;
    UINTN CoreOffset;
    UINTN FragmentCount;
    UINTN FragmentSize;
    ULONG IntegerUlong;
    ULONG OldFlags;
    SOUND_QUEUE_INFORMATION QueueInformation;
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

        if (IntegerUlong <= Handle->Device->MaxChannelCount) {
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

        if ((IntegerUlong == 1) &&
            (Handle->Device->MaxChannelCount >= SOUND_STEREO_CHANNEL_COUNT)) {

            ChannelCount = SOUND_STEREO_CHANNEL_COUNT;

        } else {
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

    case SoundGetInputQueueSize:
    case SoundGetOutputQueueSize:
        CopySize = sizeof(SOUND_QUEUE_INFORMATION);
        if (RequestBufferSize < CopySize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        ASSERT(Handle->Buffer.FragmentSize <= MAX_LONG);
        ASSERT(Handle->Buffer.FragmentCount <= MAX_LONG);

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

        BufferSize = Handle->Buffer.Size;
        CoreOffset = Handle->Buffer.CoreOffset;
        ControllerOffset = Handle->Buffer.ControllerOffset;
        if (ControllerOffset >= CoreOffset) {
            QueueInformation.BytesAvailable = ControllerOffset - CoreOffset;

        } else {
            QueueInformation.BytesAvailable = (BufferSize - CoreOffset);
            QueueInformation.BytesAvailable += ControllerOffset;
        }

        QueueInformation.FragmentsAvailable = QueueInformation.BytesAvailable /
                                              Handle->Buffer.FragmentSize;

        QueueInformation.FragmentSize = (LONG)Handle->Buffer.FragmentSize;
        QueueInformation.FragmentCount = (LONG)Handle->Buffer.FragmentCount;
        CopyOutBuffer = &QueueInformation;
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

        FragmentCount = (IntegerUlong &
                         SOUND_BUFFER_SIZE_HINT_FRAGMENT_COUNT_MASK) >>
                        SOUND_BUFFER_SIZE_HINT_FRAGMENT_COUNT_SHIFT;

        FragmentSize = 1 << ((IntegerUlong &
                              SOUND_BUFFER_SIZE_HINT_FRAGMENT_SIZE_MASK) >>
                             SOUND_BUFFER_SIZE_HINT_FRAGMENT_SIZE_SHIFT);

        if (FragmentCount > Handle->Controller->Host.MaxFragmentCount) {
            FragmentCount = Handle->Controller->Host.MaxFragmentCount;
        }

        if (FragmentSize > Handle->Controller->Host.MaxFragmentSize) {
            FragmentSize = Handle->Controller->Host.MaxFragmentSize;

        } else if (FragmentSize < Handle->Controller->Host.MinFragmentSize) {
            FragmentSize = Handle->Controller->Host.MinFragmentSize;
        }

        //
        // The fragment count and size can only be changed before the device
        // is initialized.
        //

        if ((Handle->State < SoundDeviceStateInitialized) &&
            ((FragmentSize * FragmentCount) <
             Handle->Controller->Host.MaxBufferSize)) {

            BufferSize = FragmentCount * FragmentSize;
            KeAcquireQueuedLock(Handle->Lock);
            if (Handle->State < SoundDeviceStateInitialized) {
                Handle->Buffer.Size = BufferSize;
                Handle->Buffer.FragmentCount = FragmentCount;
                Handle->Buffer.FragmentSize = FragmentSize;
            }

            KeReleaseQueuedLock(Handle->Lock);
        }

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
SoundUpdateBufferIoState (
    PSOUND_IO_BUFFER Buffer,
    ULONG Events
    )

/*++

Routine Description:

    This routine updates the given buffer's I/O state in a lock-less way based
    on the current core and controller offsets. If the offsets are the same,
    it will unset the events. If the offsets are different, it set the events.

Arguments:

    Buffer - Supplies a pointer to the sound I/O buffer whose I/O state needs
        to be updated.

    Events - Supplies the events to set/unset. This should really be
        POLL_EVENT_IN or POLL_EVENT_OUT. Errors should be set separately.

Return Value:

    None.

--*/

{

    SoundpUpdateBufferIoState(Buffer, Events, FALSE);
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
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    AllocateDmaBuffer = Controller->Host.FunctionTable->AllocateDmaBuffer;
    if (AllocateDmaBuffer != NULL) {
        Status = AllocateDmaBuffer(Controller->Host.Context,
                                   Device->Context,
                                   FragmentSize,
                                   FragmentCount,
                                   NewIoBuffer);

    } else if ((Device->Capabilities & SOUND_CAPABILITY_MMAP) != 0) {
        BufferSize = FragmentSize * FragmentCount;
        *NewIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                  MAX_ULONGLONG,
                                                  0,
                                                  BufferSize,
                                                  0);

        if (*NewIoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {
        BufferSize = FragmentSize * FragmentCount;
        *NewIoBuffer = MmAllocatePagedIoBuffer(BufferSize, 0);
        if (*NewIoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

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
    UINTN Size;
    KSTATUS Status;

    ASSERT(KeIsQueuedLockHeld(Handle->Lock) != FALSE);
    ASSERT(Handle->State < SoundDeviceStateInitialized);

    //
    // Initialize the buffer offsets so that they appear empty/full. On input,
    // the sound core library is the consumer of written bytes. On output, it
    // is the consumer of read bytes. Equal offsets indicate an empty buffer.
    //

    if (Handle->Device->Type == SoundDeviceInput) {
        Handle->Buffer.CoreOffset = 0;
        Handle->Buffer.ControllerOffset = 0;

    } else {

        ASSERT(Handle->Device->Type == SoundDeviceOutput);

        Handle->Buffer.CoreOffset = 0;
        Handle->Buffer.ControllerOffset = Handle->Buffer.Size - 1;
    }

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

    if (Handle->State == SoundDeviceStateUninitialized) {
        return STATUS_SUCCESS;
    }

    Status = STATUS_SUCCESS;
    KeAcquireQueuedLock(Handle->Lock);
    if (Handle->State == SoundDeviceStateUninitialized) {
        goto UninitializeDeviceEnd;
    }

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
SoundpUpdateBufferIoState (
    PSOUND_IO_BUFFER Buffer,
    ULONG Events,
    BOOL SoundCore
    )

/*++

Routine Description:

    This routine updates the given buffer's I/O state in a lock-less way based
    on the current core and controller offsets.

Arguments:

    Buffer - Supplies a pointer to the sound I/O buffer whose I/O state needs
        to be updated.

    Events - Supplies the events to set/unset. This should really be
        POLL_EVENT_IN or POLL_EVENT_OUT. Errors should be set separately.

    SoundCore - Supplies a boolean indicating whether or not sound core (TRUE)
        of the sound controller (FALSE) is the caller.

Return Value:

    None.

--*/

{

    volatile UINTN *DynamicOffset;
    BOOL Set;
    UINTN SnappedOffset;
    UINTN StaticOffset;

    //
    // One of the buffer offsets may be in flux, the other should not be
    // changing. That is, if the sound core is the caller then it should have
    // some synchronization around the core offset not changing during this
    // call.
    //

    if (SoundCore == FALSE) {
        DynamicOffset = &(Buffer->CoreOffset);
        StaticOffset = Buffer->ControllerOffset;

    } else {
        DynamicOffset = &(Buffer->ControllerOffset);
        StaticOffset = Buffer->CoreOffset;
    }

    //
    // The buffer is empty if the offsets are equal.
    //

    do {
        SnappedOffset = *DynamicOffset;
        if (SnappedOffset == StaticOffset) {
            Set = FALSE;

        } else {
            Set = TRUE;
        }

        IoSetIoObjectState(Buffer->IoState, Events, Set);

    } while (SnappedOffset != *DynamicOffset);

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

    Handle - Supplies a handle to the sound device.

Return Value:

    None.

--*/

{

    PSOUND_DEVICE SoundDevice;
    ULONG SoundFlags;

    Handle->Buffer.FragmentSize = SOUND_FRAGMENT_SIZE_DEFAULT;
    Handle->Buffer.FragmentCount = SOUND_FRAGMENT_COUNT_DEFAULT;
    Handle->Buffer.Size = Handle->Buffer.FragmentSize *
                             Handle->Buffer.FragmentCount;

    ASSERT(Handle->Buffer.Size < Handle->Controller->Host.MaxBufferSize);

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
    return;
}

