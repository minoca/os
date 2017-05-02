/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    filesys.c

Abstract:

    This module implements support for file system drivers.

Author:

    Evan Green 25-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/devinfo/part.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum length of a volume name string, including the terminator.
//

#define VOLUME_NAME_LENGTH 11

//
// Define the number of times create or lookup volume is allowed to kick-start
// a failed device.
//

#define VOLUME_START_RETRY_MAX 1

//
// Define the maximum number of supported volumes in the system.
//

#define MAX_VOLUMES 10000

//
// Define the location of the drivers directory, relative to the system root.
//

#define SYSTEM_DRIVERS_DIRECTORY "drivers"

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a registered file system.

Members:

    ListEntry - Stores pointers to the previous and next registered file
        systems.

    Driver - Supplies a pointer to the driver object.

--*/

typedef struct _FILE_SYSTEM {
    LIST_ENTRY ListEntry;
    PDRIVER Driver;
} FILE_SYSTEM, *PFILE_SYSTEM;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
IopDestroyVolume (
    PVOLUME Volume
    );

PSTR
IopGetNewVolumeName (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Keep the list of registered file systems.
//

LIST_ENTRY IoFileSystemList;

//
// This lock synchronizes access to the list of file systems.
//

PQUEUED_LOCK IoFileSystemListLock = NULL;

//
// Store a pointer to the volumes directory and the number of volumes in the
// system.
//

POBJECT_HEADER IoVolumeDirectory = NULL;

//
// Define the path from the system volume to the system directory. Set it to a
// default in case there is no boot entry (which there should really always be).
//

PSTR IoSystemDirectoryPath = "minoca";

//
// Store a pointer to the system volumes.
//

PVOLUME IoSystemVolume = NULL;

UUID IoPartitionDeviceInformationUuid = PARTITION_DEVICE_INFORMATION_UUID;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoRegisterFileSystem (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine registers the given driver as a file system driver.

Arguments:

    Driver - Supplies a pointer to the driver registering the file system
        support.

Return Value:

    Status code.

--*/

{

    PFILE_SYSTEM NewFileSystem;
    KSTATUS Status;

    //
    // Allocate and initialize the new file system.
    //

    NewFileSystem = MmAllocatePagedPool(sizeof(FILE_SYSTEM), FI_ALLOCATION_TAG);
    if (NewFileSystem == NULL) {
        Status = STATUS_NO_MEMORY;
        goto RegisterFileSystemEnd;
    }

    RtlZeroMemory(NewFileSystem, sizeof(FILE_SYSTEM));
    NewFileSystem->Driver = Driver;

    //
    // Add it to the list.
    //

    KeAcquireQueuedLock(IoFileSystemListLock);
    INSERT_AFTER(&(NewFileSystem->ListEntry), &IoFileSystemList);
    KeReleaseQueuedLock(IoFileSystemListLock);
    Status = STATUS_SUCCESS;

RegisterFileSystemEnd:
    return Status;
}

KSTATUS
IopAddFileSystem (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine adds a file system to the given volume.

Arguments:

    Device - Supplies a pointer to the volume to attach a file system to.

Return Value:

    Status code.

--*/

{

    PDRIVER_ADD_DEVICE AddDevice;
    PLIST_ENTRY CurrentEntry;
    PFILE_SYSTEM CurrentFileSystem;
    PDRIVER Driver;
    ULONG OriginalStackSize;
    KSTATUS Status;

    ASSERT(Device->Header.Type == ObjectVolume);

    OriginalStackSize = Device->DriverStackSize;
    Status = STATUS_NO_DRIVERS;

    //
    // Loop through all file systems, calling AddDevice until a driver
    // attaches.
    //

    KeAcquireQueuedLock(IoFileSystemListLock);
    CurrentEntry = IoFileSystemList.Next;
    while (CurrentEntry != &IoFileSystemList) {
        CurrentFileSystem = LIST_VALUE(CurrentEntry, FILE_SYSTEM, ListEntry);
        Driver = CurrentFileSystem->Driver;

        //
        // Call the driver's AddDevice. The return value of AddDevice is
        // ignored, success is implied if the driver attached itself. Note that
        // the file system list lock is held as Add Device is called. Thus
        // a file system driver's Add Device routine cannot depend on any other
        // volume enumerations to complete, otherwise a deadlock would occur.
        //

        if ((Driver->Flags & DRIVER_FLAG_FAILED_DRIVER_ENTRY) == 0) {
            if (Driver->FunctionTable.AddDevice != NULL) {
                AddDevice = Driver->FunctionTable.AddDevice;
                AddDevice(Driver,
                          IoGetDeviceId(Device),
                          Device->ClassId,
                          Device->CompatibleIds,
                          Device);

                if (Device->DriverStackSize != OriginalStackSize) {
                    Status = STATUS_SUCCESS;
                    break;
                }

            } else {
                Status = STATUS_DRIVER_FUNCTION_MISSING;
                IopSetDeviceProblem(Device, DeviceProblemNoAddDevice, Status);
                goto AddFileSystemEnd;
            }
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (!KSUCCESS(Status)) {
        IopSetDeviceProblem(Device, DeviceProblemNoFileSystem, Status);
        goto AddFileSystemEnd;
    }

AddFileSystemEnd:
    KeReleaseQueuedLock(IoFileSystemListLock);
    return Status;
}

KSTATUS
IoCreateVolume (
    PDEVICE Device,
    PVOLUME *Volume
    )

/*++

Routine Description:

    This routine creates a new volume to be mounted by a file system.

Arguments:

    Device - Supplies a pointer to the physical device upon which the file
        system should be mounted.

    Volume - Supplies a pointer that receives a pointer to the newly created
        volume.

Return Value:

    Status code.

--*/

{

    BOOL LockHeld;
    PSTR NewName;
    PVOLUME NewVolume;
    KSTATUS Status;
    BOOL TargetAttached;

    ASSERT((Device->Flags & DEVICE_FLAG_MOUNTABLE) != 0);

    LockHeld = FALSE;
    TargetAttached = FALSE;

    //
    // Allocate the next available name for the volume.
    //

    NewName = IopGetNewVolumeName();
    if (NewName == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateVolumeEnd;
    }

    //
    // Create the volume.
    //

    Status = IopCreateDevice(NULL,
                             NULL,
                             (PDEVICE)IoVolumeDirectory,
                             NewName,
                             NULL,
                             NULL,
                             ObjectVolume,
                             sizeof(VOLUME),
                             (PDEVICE *)&NewVolume);

    if (!KSUCCESS(Status)) {
        goto CreateVolumeEnd;
    }

    //
    // Now acquire the physical device's lock exclusively and attach the volume
    // to it. If the physical device is awaiting removal or removed, abort the
    // process.
    //

    KeAcquireSharedExclusiveLockExclusive(Device->Lock);
    LockHeld = TRUE;
    if ((Device->State == DeviceAwaitingRemoval) ||
        (Device->State == DeviceRemoved)) {

        Status = STATUS_PARENT_AWAITING_REMOVAL;
        goto CreateVolumeEnd;
    }

    //
    // Only allow one volume to be mounted per device.
    //

    if ((Device->Flags & DEVICE_FLAG_MOUNTED) != 0) {
        Status = STATUS_TOO_LATE;
        goto CreateVolumeEnd;
    }

    //
    // Reference the backing device, attach it to the volume and add the volume
    // to the device's active child list.
    //

    ObAddReference(Device);
    NewVolume->Device.TargetDevice = Device;
    INSERT_BEFORE(&(NewVolume->Device.ActiveListEntry),
                  &(Device->ActiveChildListHead));

    //
    // Set the volume specific referencee count to 1 or 2 depending on whether
    // or not the caller wants a pointer to the volume. Also add an object
    // manager reference that will be released when the volume reference drops
    // to 0.
    //

    ObAddReference(NewVolume);
    if (Volume == NULL) {
        NewVolume->ReferenceCount = 1;

    } else {
        NewVolume->ReferenceCount = 2;
    }

    TargetAttached = TRUE;
    Device->Flags |= DEVICE_FLAG_MOUNTED;
    KeReleaseSharedExclusiveLockExclusive(Device->Lock);
    LockHeld = FALSE;

    //
    // TODO: Determine if this volume should contain the page file.
    //

    NewVolume->Device.Flags |= DEVICE_FLAG_PAGING_DEVICE;

    //
    // Queue the work item to start the volume.
    //

    Status = IopQueueDeviceWork((PDEVICE)NewVolume, DeviceActionStart, NULL, 0);
    if (!KSUCCESS(Status)) {
        IopSetDeviceProblem((PDEVICE)NewVolume,
                            DeviceProblemFailedToQueueStart,
                            Status);

        goto CreateVolumeEnd;
    }

CreateVolumeEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(Device->Lock);
    }

    if (!KSUCCESS(Status)) {

        //
        // If the volume failed to attach, but was created, then release it.
        //

        if (TargetAttached == FALSE) {
            if (NewVolume != NULL) {
                ObReleaseReference(NewVolume);
            }

        //
        // Otherwise if the caller requested the new volume, release the second
        // reference taken. This will actually attempt to destroy the volume.
        // It may succeed, but it may not. Not much to do about this.
        //

        } else if (Volume != NULL) {

            ASSERT(TargetAttached != FALSE);

            IoVolumeReleaseReference(NewVolume);
        }

    } else {

        //
        // If the caller wanted a pointer to the volume, send it off.
        //

        if (Volume != NULL) {
            *Volume = NewVolume;
        }
    }

    if (NewName != NULL) {
        MmFreePagedPool(NewName);
    }

    return Status;
}

KSTATUS
IopCreateOrLookupVolume (
    PDEVICE Device,
    PVOLUME *Volume
    )

/*++

Routine Description:

    This routine returns the volume associated with the given device, if such
    a volume exists. A reference is taken on the volume, which the caller is
    expected to release.

Arguments:

    Device - Supplies a pointer to the device whose volume is to be returned.

    Volume - Supplies a pointer that receives a pointer to the created or found
        volume.

    VolumeCreated - Supplies a pointer that receives a boolean indicating
        whether or not the volume was created.

Return Value:

    Status code.

--*/

{

    PDEVICE Child;
    PLIST_ENTRY CurrentEntry;
    PVOLUME FoundVolume;
    PVOLUME NewVolume;
    ULONG RetryCount;
    KSTATUS Status;

    ASSERT(Device != NULL);
    ASSERT(Volume != NULL);
    ASSERT((Device->Flags & DEVICE_FLAG_MOUNTABLE) != 0);

    FoundVolume = NULL;

    //
    // Loop until a volume is found or created.
    //

    while (TRUE) {

        //
        // If the OS has not already mounted a volume on the device, then try
        // to create a volume.
        //

        if ((Device->Flags & DEVICE_FLAG_MOUNTED) == 0) {

            //
            // Create a volume on the device. If this successfully creates a
            // volume, then it takes a reference on it. If it finds out that
            // someone else beat it to the punch, it returns a "too late"
            // status. If it fails outright, just exit.
            //

            Status = IoCreateVolume(Device, &NewVolume);
            if (!KSUCCESS(Status) && (Status != STATUS_TOO_LATE)) {
                goto CreateOrLookupVolumeEnd;
            }

            //
            // If a volume was successfully created, wait for the volume to
            // signal on ready or on failure.
            //

            if (KSUCCESS(Status)) {
                ObWaitOnObject(NewVolume, 0, WAIT_TIME_INDEFINITE);

                //
                // After the signal, if the volume is in the started state,
                // then this is a success. If the volume is not started, then
                // either there was a problem initializing the volume or it
                // was removed because of user interaction. Either way, fail.
                //

                FoundVolume = NewVolume;
                if (NewVolume->Device.State == DeviceStarted) {
                    Status = STATUS_SUCCESS;

                } else {
                    Status = STATUS_UNSUCCESSFUL;
                }

                goto CreateOrLookupVolumeEnd;
            }
        }

        //
        // A volume was already mounted when this routine was called or someone
        // else beat this routine to the punch. Lookup the volume.
        //

        FoundVolume = NULL;
        KeAcquireSharedExclusiveLockShared(Device->Lock);

        //
        // If the volume still remains, then search for it. If it has been
        // unmounted since the check above, there are a few options: (1) the
        // device is in the middle of removal - the next volume create will
        // fail; (2) the volume got removed - the next volume create should
        // succeed. This routine loops to try again either way.
        //

        if ((Device->Flags & DEVICE_FLAG_MOUNTED) != 0) {
            CurrentEntry = Device->ActiveChildListHead.Next;

            //
            // Search through the active children for the first volume. There
            // should only be one volume per device. Add a reference to this
            // volume and return it.
            //

            while (CurrentEntry != &(Device->ActiveChildListHead)) {
                Child = LIST_VALUE(CurrentEntry, DEVICE, ActiveListEntry);
                if (Child->Header.Type == ObjectVolume) {
                    FoundVolume = (PVOLUME)Child;
                    IoVolumeAddReference(FoundVolume);
                    break;
                }

                CurrentEntry = CurrentEntry->Next;
            }
        }

        KeReleaseSharedExclusiveLockShared(Device->Lock);

        //
        // If a volume was found, wait on it. If it signals from the start
        // state, proceed. If it signals from the removed state, then try
        // again. If it signals from any other state, try to kick-start it
        // once before giving up.
        //

        if (FoundVolume != NULL) {
            RetryCount = 0;
            while (TRUE) {
                ObWaitOnObject(FoundVolume, 0, WAIT_TIME_INDEFINITE);
                if (FoundVolume->Device.State == DeviceStarted) {
                    Status = STATUS_SUCCESS;
                    goto CreateOrLookupVolumeEnd;
                }

                //
                // Try to find or create the volume again if the volume has
                // been removed.
                //

                if (FoundVolume->Device.State == DeviceRemoved) {
                    IoVolumeReleaseReference(FoundVolume);
                    FoundVolume = NULL;
                    break;
                }

                if (RetryCount >= VOLUME_START_RETRY_MAX) {
                    Status = STATUS_UNSUCCESSFUL;
                    goto CreateOrLookupVolumeEnd;
                }

                //
                // Otherwise, kick it to see if it will come up.
                //

                ObSignalObject(FoundVolume, SignalOptionUnsignal);
                Status = IopQueueDeviceWork((PDEVICE)FoundVolume,
                                            DeviceActionStart,
                                            NULL,
                                            0);

                if (!KSUCCESS(Status)) {
                    IopSetDeviceProblem((PDEVICE)NewVolume,
                                        DeviceProblemFailedToQueueStart,
                                        Status);

                    goto CreateOrLookupVolumeEnd;
                }

                RetryCount += 1;
            }
        }
    }

CreateOrLookupVolumeEnd:
    if (!KSUCCESS(Status)) {
        if (FoundVolume != NULL) {
            IoVolumeReleaseReference(FoundVolume);
        }

    } else {
        *Volume = FoundVolume;
    }

    return Status;
}

VOID
IopVolumeArrival (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine performs work associated with a new volume coming online.

Arguments:

    Parameter - Supplies a pointer to the arriving volume.

Return Value:

    None.

--*/

{

    IO_BOOT_INFORMATION BootInformation;
    UINTN BootInformationSize;
    BOOL Created;
    PSTR DeviceName;
    ULONG DeviceNameLength;
    PIO_HANDLE DriversDirectoryHandle;
    PFILE_OBJECT FileObject;
    ULONG FileObjectFlags;
    PKPROCESS KernelProcess;
    ULONG MapFlags;
    BOOL Match;
    PARTITION_DEVICE_INFORMATION PartitionInformation;
    UINTN PartitionInformationSize;
    PPATH_POINT PathPoint;
    FILE_PROPERTIES Properties;
    KSTATUS Status;
    PIO_HANDLE SystemDirectoryHandle;
    BOOL SystemVolume;
    PDEVICE TargetDevice;
    PVOLUME Volume;
    PIO_HANDLE VolumeHandle;
    PSTR VolumeName;
    ULONG VolumeNameLength;

    DeviceName = NULL;
    DriversDirectoryHandle = NULL;
    FileObject = NULL;
    SystemDirectoryHandle = NULL;
    SystemVolume = FALSE;
    Volume = (PVOLUME)Parameter;
    VolumeHandle = NULL;

    ASSERT(Volume != NULL);
    ASSERT(Volume->Device.Header.Type == ObjectVolume);

    //
    // Get the partition information for the volume.
    //

    TargetDevice = IoGetTargetDevice((PDEVICE)Volume);

    ASSERT(TargetDevice != NULL);

    VolumeName = ObGetFullPath(Volume, IO_ALLOCATION_TAG);
    if (VolumeName == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto VolumeArrivalEnd;
    }

    DeviceName = ObGetFullPath(TargetDevice, IO_ALLOCATION_TAG);
    if (DeviceName == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto VolumeArrivalEnd;
    }

    DeviceNameLength = RtlStringLength(DeviceName) + 1;
    VolumeNameLength = RtlStringLength(VolumeName) + 1;

    //
    // Get the root path entry for the volume. Start by sending a root lookup
    // request to the volume. If it does not succeed, then the volume isn't
    // participating in the file system and there is nothing to do, really.
    //

    Status = IopSendLookupRequest(&(Volume->Device),
                                  NULL,
                                  NULL,
                                  0,
                                  &Properties,
                                  &FileObjectFlags,
                                  &MapFlags);

    if (!KSUCCESS(Status)) {
        goto VolumeArrivalEnd;
    }

    Properties.DeviceId = Volume->Device.DeviceId;

    //
    // Create or lookup a file object for the volume.
    //

    Status = IopCreateOrLookupFileObject(&Properties,
                                         &(Volume->Device),
                                         FileObjectFlags,
                                         MapFlags,
                                         &FileObject,
                                         &Created);

    if (!KSUCCESS(Status)) {
        goto VolumeArrivalEnd;
    }

    ASSERT(Created != FALSE);
    ASSERT(Volume->PathEntry == NULL);

    //
    // Make a path entry with the found file object. This does not take an
    // additional reference on the file object.
    //

    Volume->PathEntry = IopCreateAnonymousPathEntry(FileObject);
    if (Volume->PathEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto VolumeArrivalEnd;
    }

    FileObject = NULL;

    //
    // The volume is completely set up now, so signal it as ready. This can
    // potentially race with the device removal sequence unsignalling the
    // volume. The worst is that something sneaks through with a short-lived
    // reference to the device. It won't be very useful once the remove IRP
    // is sent.
    //

    ObSignalObject(Volume, SignalOptionSignalAll);

    //
    // Mount the device on the volume. During this call, the mount code should
    // look up and find this volume as an active child of the given device.
    //

    Status = IoMount(TRUE,
                     VolumeName,
                     VolumeNameLength,
                     DeviceName,
                     DeviceNameLength,
                     MOUNT_FLAG_LINKED,
                     IO_ACCESS_READ | IO_ACCESS_WRITE);

    if (!KSUCCESS(Status)) {
        goto VolumeArrivalEnd;
    }

    //
    // Determine whether or not this is the system volume.
    //

    PartitionInformationSize = sizeof(PARTITION_DEVICE_INFORMATION);
    Status = IoGetSetDeviceInformation(TargetDevice->DeviceId,
                                       &IoPartitionDeviceInformationUuid,
                                       &PartitionInformation,
                                       &PartitionInformationSize,
                                       FALSE);

    if ((KSUCCESS(Status)) &&
        (PartitionInformationSize == sizeof(PARTITION_DEVICE_INFORMATION))) {

        //
        // Get the boot partition identifiers.
        //

        BootInformationSize = sizeof(IO_BOOT_INFORMATION);
        Status = KeGetSetSystemInformation(SystemInformationIo,
                                           IoInformationBoot,
                                           &BootInformation,
                                           &BootInformationSize,
                                           FALSE);

        if ((KSUCCESS(Status)) &&
            (BootInformationSize == sizeof(IO_BOOT_INFORMATION))) {

            ASSERT(sizeof(BootInformation.SystemPartitionIdentifier) ==
                   sizeof(PartitionInformation.PartitionId));

            Match = RtlCompareMemory(
                            BootInformation.SystemPartitionIdentifier,
                            PartitionInformation.PartitionId,
                            sizeof(BootInformation.SystemPartitionIdentifier));

            if ((Match != FALSE) && (IoSystemVolume == NULL)) {
                IoSystemVolume = Volume;
                SystemVolume = TRUE;
            }
        }
    }

    //
    // If this is the system volume, then open the drivers directory and change
    // the kernel's current directory to the driver's directory.
    //

    if (SystemVolume != FALSE) {

        //
        // Copy the system volume path. Synchronization would be needed if this
        // path changes.
        //

        ASSERT(VolumeNameLength != 0);

        Status = IoOpen(TRUE,
                        NULL,
                        VolumeName,
                        VolumeNameLength,
                        IO_ACCESS_READ,
                        OPEN_FLAG_DIRECTORY,
                        0,
                        &VolumeHandle);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to open system volume: %d\n", Status);
            goto VolumeArrivalEnd;
        }

        //
        // Attempt to open the system directory.
        //

        Status = IoOpen(TRUE,
                        VolumeHandle,
                        IoSystemDirectoryPath,
                        RtlStringLength(IoSystemDirectoryPath) + 1,
                        IO_ACCESS_READ,
                        OPEN_FLAG_DIRECTORY,
                        0,
                        &SystemDirectoryHandle);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to open system directory '%s': %d\n",
                          IoSystemDirectoryPath,
                          Status);

            goto VolumeArrivalEnd;
        }

        //
        // Attempt to open the driver directory.
        //

        Status = IoOpen(TRUE,
                        SystemDirectoryHandle,
                        SYSTEM_DRIVERS_DIRECTORY,
                        sizeof(SYSTEM_DRIVERS_DIRECTORY),
                        IO_ACCESS_READ,
                        OPEN_FLAG_DIRECTORY,
                        0,
                        &DriversDirectoryHandle);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to open driver directory '%s/%s': %d\n",
                          IoSystemDirectoryPath,
                          SYSTEM_DRIVERS_DIRECTORY,
                          Status);

            goto VolumeArrivalEnd;
        }

        //
        // Now set the kernel's current working directory to the drivers
        // directory.
        //

        KernelProcess = PsGetKernelProcess();

        ASSERT(KernelProcess == PsGetCurrentProcess());

        PathPoint = &(DriversDirectoryHandle->PathPoint);
        IO_PATH_POINT_ADD_REFERENCE(PathPoint);
        KeAcquireQueuedLock(KernelProcess->Paths.Lock);

        ASSERT(KernelProcess->Paths.CurrentDirectory.PathEntry == NULL);
        ASSERT(KernelProcess->Paths.CurrentDirectory.MountPoint == NULL);

        IO_COPY_PATH_POINT(&(KernelProcess->Paths.CurrentDirectory), PathPoint);
        KeReleaseQueuedLock(KernelProcess->Paths.Lock);
    }

    //
    // Tell the memory manager about volumes that can contain page files.
    //

    if ((Volume->Device.Flags & DEVICE_FLAG_PAGING_DEVICE) != 0) {
        MmVolumeArrival(VolumeName, VolumeNameLength, SystemVolume);
    }

    //
    // Tell the process library about the new volume.
    //

    PsVolumeArrival(VolumeName, VolumeNameLength, SystemVolume);

    //
    // Attempt to start any devices that had previously failed as a volume with
    // more drivers is potentially here.
    //

    if (SystemVolume != FALSE) {
        IopQueueDeviceWork(IoRootDevice,
                           DeviceActionStart,
                           NULL,
                           DEVICE_ACTION_SEND_TO_SUBTREE);
    }

    Status = STATUS_SUCCESS;

VolumeArrivalEnd:
    if (VolumeName != NULL) {
        MmFreePagedPool(VolumeName);
    }

    if (DeviceName != NULL) {
        MmFreePagedPool(DeviceName);
    }

    if (FileObject != NULL) {
        IopFileObjectReleaseReference(FileObject);
    }

    if (VolumeHandle != NULL) {
        IoClose(VolumeHandle);
    }

    if (SystemDirectoryHandle != NULL) {
        IoClose(SystemDirectoryHandle);
    }

    if (DriversDirectoryHandle != NULL) {
        IoClose(DriversDirectoryHandle);
    }

    if (!KSUCCESS(Status)) {
        IopSetDeviceProblem((PDEVICE)Volume,
                            DeviceProblemFailedVolumeArrival,
                            Status);
    }

    //
    // Relase the reference on the volume taken when this work item was
    // scheduled.
    //

    ObReleaseReference(Volume);
    return;
}

VOID
IoVolumeAddReference (
    PVOLUME Volume
    )

/*++

Routine Description:

    This routine increments a volume's reference count.

Arguments:

    Volume - Supplies a pointer to a volume device.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Volume->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x10000000);

    return;
}

VOID
IoVolumeReleaseReference (
    PVOLUME Volume
    )

/*++

Routine Description:

    This routine decrements a volume's reference count.

Arguments:

    Volume - Supplies a pointer to a volume device.

Return Value:

    None.

--*/

{

    BOOL DestroyVolume;
    ULONG OldReferenceCount;
    PDEVICE TargetDevice;

    TargetDevice = Volume->Device.TargetDevice;
    KeAcquireSharedExclusiveLockExclusive(TargetDevice->Lock);
    OldReferenceCount = RtlAtomicAdd32(&(Volume->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 2) {
        DestroyVolume = TRUE;
        KeAcquireSharedExclusiveLockExclusive(Volume->Device.Lock);

        //
        // If the volume is already removed or in the process of being
        // unmounted there is no work to do. It's too late.
        //

        if ((Volume->Device.State == DeviceRemoved) ||
            ((Volume->Flags & VOLUME_FLAG_UNMOUNTING) != 0)) {

            DestroyVolume = FALSE;

        //
        // Prepare the volume for the destruction path.
        //

        } else {

            //
            // Mark that the volume is in the middle of the unmounting process
            // in order to prevent new path walks from succeeding.
            //

            Volume->Flags |= VOLUME_FLAG_UNMOUNTING;

            //
            // Before proceeding through the removal process, unsignal the
            // volume. The volume lookup routine waits on the device for its
            // state to settle.
            //

            ObSignalObject(Volume, SignalOptionUnsignal);

            //
            // Take a object manager reference on the volume. As soon as the
            // locks are released, another thread could come through and
            // release the last volume reference and, in turn, the last object
            // reference.
            //

            ObAddReference(Volume);
        }

        KeReleaseSharedExclusiveLockExclusive(Volume->Device.Lock);
        KeReleaseSharedExclusiveLockExclusive(TargetDevice->Lock);
        if (DestroyVolume != FALSE) {
            IopDestroyVolume(Volume);
            ObReleaseReference(Volume);
        }

    } else if (OldReferenceCount == 1) {
        KeReleaseSharedExclusiveLockExclusive(TargetDevice->Lock);

        //
        // Release the volume path entry if the volume is about to be taken out
        // of comission.
        //

        if (Volume->PathEntry != NULL) {

            ASSERT(Volume->PathEntry->Parent == NULL);

            IoPathEntryReleaseReference(Volume->PathEntry);
        }

        ObReleaseReference(Volume);

    } else {
        KeReleaseSharedExclusiveLockExclusive(TargetDevice->Lock);
    }

    return;
}

KSTATUS
IopRemoveDevicePaths (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine takes the device's paths offline.

Arguments:

    Device - Supplies a pointer to the departing device.

Return Value:

    Status code.

--*/

{

    PSTR DevicePath;
    PCSTR Path;
    ULONG PathSize;
    PATH_POINT RootPathPoint;
    KSTATUS Status;

    ASSERT(IS_DEVICE_OR_VOLUME(Device));

    //
    // If it's a volume, it should be unmounting.
    //

    ASSERT((Device->Header.Type != ObjectVolume) ||
           ((((PVOLUME)Device)->Flags & VOLUME_FLAG_UNMOUNTING) != 0));

    ASSERT((Device->State == DeviceAwaitingRemoval) ||
           (Device->State == DeviceRemoved));

    DevicePath = NULL;
    RootPathPoint.PathEntry = NULL;

    //
    // If the device is a volume, it might have contained a page file, notify
    // the memory is volume is being removed.
    //

    if ((Device->Flags & DEVICE_FLAG_PAGING_DEVICE) != 0) {
        Status = MmVolumeRemoval(Device);
        if (!KSUCCESS(Status)) {
            goto RemoveDevicePathsEnd;
        }
    }

    //
    // Retrieve a path to the device's root. If this fails, then the
    // removal process needs to be rolled back. The system cannot close any
    // opens paths or remove mount points correctly.
    //

    DevicePath = ObGetFullPath(Device, IO_ALLOCATION_TAG);
    if (DevicePath == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RemoveDevicePathsEnd;
    }

    //
    // Open a path to the device root. If this fails, it should be because
    // the parent path is marked closing, or the root lookup call never
    // went through because the volume is set as "unmounting". In either
    // case, there are no paths or mount points to destroy. Count it as
    // success.
    //

    Path = DevicePath;
    PathSize = RtlStringLength(Path) + 1;
    Status = IopPathWalk(TRUE,
                         NULL,
                         &Path,
                         &PathSize,
                         OPEN_FLAG_DIRECTORY,
                         NULL,
                         &RootPathPoint);

    if (!KSUCCESS(Status)) {

        ASSERT((Status == STATUS_PATH_NOT_FOUND) ||
               (Status == STATUS_DEVICE_NOT_CONNECTED));

        Status = STATUS_SUCCESS;
        goto RemoveDevicePathsEnd;
    }

    //
    // Forcefully remove all mount points that exist under the root.
    //

    IopRemoveMountPoints(&RootPathPoint);

    //
    // Clean the cached path entries. Do this after removing mount points as
    // the work above closed a bunch of path entries.
    //

    IopPathCleanCache(RootPathPoint.PathEntry);
    Status = STATUS_SUCCESS;

RemoveDevicePathsEnd:
    if (DevicePath != NULL) {
        MmFreePagedPool(DevicePath);
    }

    if (RootPathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&RootPathPoint);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
IopDestroyVolume (
    PVOLUME Volume
    )

/*++

Routine Description:

    This routine attempts to destroy the given volume by queuing its removal.
    Remove is not queued if the volume is busy.

Arguments:

    Volume - Supplies a pointer to the volume to be destroyed.

Return Value:

    Status code.

--*/

{

    ULONG Flags;
    KSTATUS Status;
    PDEVICE TargetDevice;

    ASSERT(Volume->Device.Header.Type == ObjectVolume);
    ASSERT((Volume->Flags & VOLUME_FLAG_UNMOUNTING) != 0);

    TargetDevice = Volume->Device.TargetDevice;

    //
    // Flush the volume. This does not need to be synchronized, because the
    // underlying device is explicitly flushed after in hope of batching writes
    // to the device.
    //

    Status = IopFlushFileObjects(Volume->Device.DeviceId, 0, NULL);
    if (!KSUCCESS(Status)) {
        Volume->Flags &= ~VOLUME_FLAG_UNMOUNTING;
        IopSetDeviceProblem(&(Volume->Device),
                            DeviceProblemFailedVolumeRemoval,
                            Status);

        goto DestroyVolumeEnd;
    }

    //
    // Since volumes and their target devices are 1:1, flush the device's
    // cache entries now that the volume has been closed and flushed. In the
    // future, the partition manager will have to trigger the device cache
    // flush once all the volumes are unmounted.
    //

    Status = IopFlushFileObjects(TargetDevice->DeviceId, 0, NULL);
    if (!KSUCCESS(Status)) {
        Volume->Flags &= ~VOLUME_FLAG_UNMOUNTING;
        IopSetDeviceProblem(&(Volume->Device),
                            DeviceProblemFailedVolumeRemoval,
                            Status);

        goto DestroyVolumeEnd;
    }

    //
    // TODO: Notify the user that the device is now safe to remove.
    //

    //
    // Remove any cached path entries that are below the volume root.
    //

    if (Volume->PathEntry != NULL) {
        IopPathCleanCache(Volume->PathEntry);
    }

    //
    // Start the removal process for this volume. There isn't much recourse if
    // this fails other than to roll it back and let the caller know.
    //

    Flags = DEVICE_ACTION_SEND_TO_SUBTREE | DEVICE_ACTION_OPEN_QUEUE;
    Status = IopQueueDeviceWork(&(Volume->Device),
                                DeviceActionPrepareRemove,
                                NULL,
                                Flags);

    //
    // If there was a queue failure, set the problem state. Do not call the
    // queue failure handler as that might incorrectly roll back the device
    // tree state. Just assume that no parent is waiting on this device's state
    // and that is is safe to ignore the failure.
    //

    if (!KSUCCESS(Status) && (Status != STATUS_DEVICE_QUEUE_CLOSING)) {
        Volume->Flags &= ~VOLUME_FLAG_UNMOUNTING;
        IopSetDeviceProblem(&(Volume->Device),
                            DeviceProblemFailedToQueuePrepareRemove,
                            Status);

        goto DestroyVolumeEnd;
    }

    //
    // If this was the system volume, unset the global variable.
    //

    if (Volume == IoSystemVolume) {
        IoSystemVolume = NULL;
    }

DestroyVolumeEnd:
    return;
}

PSTR
IopGetNewVolumeName (
    VOID
    )

/*++

Routine Description:

    This routine returns a name for a volume that does not collide with any
    existing volume names.

Arguments:

    None.

Return Value:

    Returns a new volume name on success, allocated from paged pool.

    NULL on failure.

--*/

{

    PVOID ExistingVolume;
    PSTR NewName;
    ULONG NewNameLength;
    ULONG VolumeIndex;

    NewName = MmAllocatePagedPool(VOLUME_NAME_LENGTH, FI_ALLOCATION_TAG);
    if (NewName == NULL) {
        goto GetNewVolumeNameEnd;
    }

    //
    // Iterate through possible volume names. If the volume doesn't exist,
    // return it.
    //

    for (VolumeIndex = 0; VolumeIndex < MAX_VOLUMES; VolumeIndex += 1) {
        NewNameLength = RtlPrintToString(NewName,
                                         VOLUME_NAME_LENGTH,
                                         CharacterEncodingDefault,
                                         "Volume%d",
                                         VolumeIndex);

        if (NewNameLength > VOLUME_NAME_LENGTH) {
            NewNameLength = VOLUME_NAME_LENGTH;
        }

        ExistingVolume = ObFindObject(NewName,
                                      NewNameLength,
                                      IoVolumeDirectory);

        if (ExistingVolume == NULL) {
            goto GetNewVolumeNameEnd;
        }

        //
        // The object exists, release the extra reference added by "finding" it.
        //

        ObReleaseReference(ExistingVolume);
        ExistingVolume = NULL;
    }

    //
    // There are too many volumes in the system! Give up.
    //

    MmFreePagedPool(NewName);
    NewName = NULL;

GetNewVolumeNameEnd:
    return NewName;
}

