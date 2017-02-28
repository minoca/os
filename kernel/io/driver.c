/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    driver.c

Abstract:

    This module implements routines that interact with drivers.

Author:

    Evan Green 16-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopInitializeImages (
    PKPROCESS Process
    );

KSTATUS
IopAddDeviceDatabaseEntry (
    PCSTR DeviceOrClassId,
    PCSTR DriverName,
    PLIST_ENTRY DatabaseListHead
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store list heads to the device databases. These list entries are of type
// DEVICE_DATABASE_ENTRY, and store the mappings between devices and drivers
// or device classes and drivers. All memory in these databases is paged.
//

LIST_ENTRY IoDeviceDatabaseHead;
LIST_ENTRY IoDeviceClassDatabaseHead;
PQUEUED_LOCK IoDeviceDatabaseLock;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoRegisterDriverFunctions (
    PDRIVER Driver,
    PDRIVER_FUNCTION_TABLE FunctionTable
    )

/*++

Routine Description:

    This routine is called by a driver to register its function pointers with
    the system. Drivers cannot be attached to the system until this is
    complete. This routine is usually called by a driver in its entry point.
    This routine should only be called once during the lifetime of a driver.

Arguments:

    Driver - Supplies a pointer to the driver whose routines are being
        registered.

    FunctionTable - Supplies a pointer to the function pointer table containing
        the drivers dispatch routines.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if a required parameter or function was not
        supplied.

--*/

{

    KSTATUS Status;

    Status = STATUS_INVALID_PARAMETER;
    if ((Driver == NULL) || (FunctionTable == NULL)) {
        goto RegisterDriverFunctionsEnd;
    }

    if (FunctionTable->Version == 0) {
        goto RegisterDriverFunctionsEnd;
    }

    //
    // The driver seems to have filled out the correct fields. Save the
    // function table in the driver structure.
    //

    RtlCopyMemory(&(Driver->FunctionTable),
                  FunctionTable,
                  sizeof(DRIVER_FUNCTION_TABLE));

    Status = STATUS_SUCCESS;

RegisterDriverFunctionsEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoAttachDriverToDevice (
    PDRIVER Driver,
    PDEVICE Device,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called by a driver to attach itself to a device. Once
    attached, the driver will participate in all IRPs that go through to the
    device. This routine can only be called during a driver's AddDevice routine.

Arguments:

    Driver - Supplies a pointer to the driver attaching to the device.

    Device - Supplies a pointer to the device to attach to.

    Context - Supplies an optional context pointer that will be passed to the
        driver each time it is called in relation to this device.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_EARLY or STATUS_TOO_LATE if the routine was called outside of a
    driver's AddDevice routine.

    STATUS_INSUFFICIENT_RESOURCES if allocations failed.

--*/

{

    PDRIVER_STACK_ENTRY StackEntry;
    KSTATUS Status;

    //
    // Only allow drivers to attach during the Unreported and Initialized
    // states.
    //

    if ((Device->State != DeviceUnreported) &&
        (Device->State != DeviceInitialized)) {

        Status = STATUS_TOO_LATE;
        goto AttachDriverToDeviceEnd;
    }

    //
    // Allocate and initialize the driver stack entry.
    //

    StackEntry = MmAllocateNonPagedPool(sizeof(DRIVER_STACK_ENTRY),
                                        DEVICE_ALLOCATION_TAG);

    if (StackEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AttachDriverToDeviceEnd;
    }

    RtlZeroMemory(StackEntry, sizeof(DRIVER_STACK_ENTRY));
    StackEntry->Driver = Driver;
    StackEntry->DriverContext = Context;

    //
    // Add the driver to the top of the stack.
    //

    INSERT_AFTER(&(StackEntry->ListEntry), &(Device->DriverStackHead));
    Device->DriverStackSize += 1;

    //
    // Increase the reference count on the driver so it cannot be unloaded
    // while the device is in use.
    //

    IoDriverAddReference(Driver);
    Status = STATUS_SUCCESS;

AttachDriverToDeviceEnd:
    return Status;
}

KERNEL_API
VOID
IoDriverAddReference (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine increments the reference count on a driver.

Arguments:

    Driver - Supplies a pointer to the driver.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);

    ImImageAddReference(Driver->Image);
    return;
}

KERNEL_API
VOID
IoDriverReleaseReference (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine decrements the reference count on a driver. This routine
    must be balanced by a previous call to add a reference on the driver.

Arguments:

    Driver - Supplies a pointer to the driver.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(IoDeviceDatabaseLock);
    ImImageReleaseReference(Driver->Image);
    KeReleaseQueuedLock(IoDeviceDatabaseLock);
    return;
}

INTN
IoSysLoadDriver (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine loads a driver into the kernel's address space.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PDRIVER Driver;
    PSTR DriverName;
    PSYSTEM_CALL_LOAD_DRIVER Parameters;
    KSTATUS Status;

    DriverName = NULL;
    Parameters = SystemCallParameter;
    Status = PsCheckPermission(PERMISSION_DRIVER_LOAD);
    if (!KSUCCESS(Status)) {
        goto SysLoadDriverEnd;
    }

    Status = MmCreateCopyOfUserModeString(Parameters->DriverName,
                                          Parameters->DriverNameSize,
                                          IO_ALLOCATION_TAG,
                                          &DriverName);

    if (!KSUCCESS(Status)) {
        goto SysLoadDriverEnd;
    }

    Status = IoLoadDriver(DriverName, &Driver);
    if (!KSUCCESS(Status)) {
        goto SysLoadDriverEnd;
    }

    //
    // Immediately release the reference taken on the driver.
    //

    IoDriverReleaseReference(Driver);

SysLoadDriverEnd:
    if (DriverName != NULL) {
        MmFreePagedPool(DriverName);
    }

    return Status;
}

KSTATUS
IoLoadDriver (
    PCSTR DriverName,
    PDRIVER *DriverOut
    )

/*++

Routine Description:

    This routine loads a driver into memory. This routine must be called at low
    level. The returned driver will come with an incremented reference count
    that must be released by the caller.

Arguments:

    DriverName - Supplies the name of the driver to load.

    DriverOut - Supplies a pointer where the pointer to the driver will be
        returned on success. The driver will be returned with an incremented
        reference count, it's up to the caller to release that reference when
        finished.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE DriverImage;
    PKPROCESS KernelProcess;
    ULONG LoadFlags;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    LoadFlags = IMAGE_LOAD_FLAG_IGNORE_INTERPRETER |
                IMAGE_LOAD_FLAG_NO_STATIC_CONSTRUCTORS |
                IMAGE_LOAD_FLAG_BIND_NOW |
                IMAGE_LOAD_FLAG_GLOBAL;

    KernelProcess = PsGetKernelProcess();
    *DriverOut = NULL;

    //
    // The driver image list is guarded by the device database lock since
    // acquiring the kernel process lock is too heavy (prevents the creation of
    // threads).
    //

    KeAcquireQueuedLock(IoDeviceDatabaseLock);
    Status = ImLoad(&(KernelProcess->ImageListHead),
                    DriverName,
                    NULL,
                    NULL,
                    KernelProcess,
                    LoadFlags,
                    &DriverImage,
                    NULL);

    if (KSUCCESS(Status)) {
        Status = IopInitializeImages(KernelProcess);
        if (!KSUCCESS(Status)) {
            ImImageReleaseReference(DriverImage);
            DriverImage = NULL;
        }
    }

    KeReleaseQueuedLock(IoDeviceDatabaseLock);
    if (!KSUCCESS(Status)) {
        goto LoadDriverEnd;
    }

    *DriverOut = DriverImage->SystemExtension;

LoadDriverEnd:
    return Status;
}

KSTATUS
IoAddDeviceDatabaseEntry (
    PCSTR DeviceId,
    PCSTR DriverName
    )

/*++

Routine Description:

    This routine adds a mapping between a device and a driver. Only one device
    to driver mapping can exist in the database at once.

Arguments:

    DeviceId - Supplies the device ID of the device to associate with a driver.
        This memory does not need to be retained, a copy of this string will
        be created.

    DriverName - Supplies the name of the driver corresponding to the device.
        This memory does not need to be retained, a copy of this string will be
        created.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if a required parameter or function was not
        supplied.

    STATUS_INSUFFICIENT_RESOURCE on allocation failure.

    STATUS_DUPLICATE_ENTRY if the device ID already exists in the database.

--*/

{

    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = IopAddDeviceDatabaseEntry(DeviceId,
                                       DriverName,
                                       &IoDeviceDatabaseHead);

    return Status;
}

KSTATUS
IoAddDeviceClassDatabaseEntry (
    PCSTR ClassId,
    PCSTR DriverName
    )

/*++

Routine Description:

    This routine adds a mapping between a device class and a driver. Only one
    device class to driver mapping can exist in the database at once.

Arguments:

    ClassId - Supplies the device class identifier of the device to associate
        with a driver. This memory does not need to be retained, a copy of this
        string will be created.

    DriverName - Supplies the name of the driver corresponding to the device
        class. This memory does not need to be retained, a copy of this string
        will be created.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if a required parameter or function was not
        supplied.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_DUPLICATE_ENTRY if the device ID already exists in the database.

--*/

{

    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = IopAddDeviceDatabaseEntry(ClassId,
                                       DriverName,
                                       &IoDeviceClassDatabaseHead);

    return Status;
}

KSTATUS
IoCreateDriverStructure (
    PVOID LoadedImage
    )

/*++

Routine Description:

    This routine is called to create a new driver structure for a loaded image.
    This routine should only be called internally by the system.

Arguments:

    LoadedImage - Supplies a pointer to the image associated with the driver.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE Image;
    PDRIVER NewDriver;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(IoDeviceDatabaseLock) != FALSE);

    Image = LoadedImage;
    NewDriver = MmAllocateNonPagedPool(sizeof(DRIVER), IO_ALLOCATION_TAG);
    if (NewDriver == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDriverStructureEnd;
    }

    RtlZeroMemory(NewDriver, sizeof(DRIVER));
    Image->SystemExtension = NewDriver;
    NewDriver->Image = Image;
    Status = STATUS_SUCCESS;

CreateDriverStructureEnd:
    if (!KSUCCESS(Status)) {
        if (NewDriver != NULL) {
            MmFreeNonPagedPool(NewDriver);
            NewDriver = NULL;
            Image->SystemExtension = NULL;
        }
    }

    return Status;
}

VOID
IoDestroyDriverStructure (
    PVOID LoadedImage
    )

/*++

Routine Description:

    This routine is called to destroy a driver structure in association with
    a driver being torn down. This routine should only be called internally by
    the system.

Arguments:

    LoadedImage - Supplies a pointer to the image being destroyed.

Return Value:

    None.

--*/

{

    PDRIVER Driver;
    PLOADED_IMAGE Image;
    PDRIVER_UNLOAD UnloadRoutine;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(IoDeviceDatabaseLock) != FALSE);

    Image = LoadedImage;
    Driver = Image->SystemExtension;
    if (Driver != NULL) {

        //
        // Call the unload routine if supplied.
        //

        UnloadRoutine = Driver->FunctionTable.Unload;
        if (UnloadRoutine != NULL) {
            UnloadRoutine(Driver);
        }

        Image->SystemExtension = NULL;
        Driver->Image = NULL;
        MmFreeNonPagedPool(Driver);
    }

    return;
}

KSTATUS
IopInitializeDriver (
    PVOID LoadedImage
    )

/*++

Routine Description:

    This routine is called to initialize a newly loaded driver. This routine
    should only be called internally by the system.

Arguments:

    LoadedImage - Supplies a pointer to the image associated with the driver.

Return Value:

    Status code.

--*/

{

    PDRIVER Driver;
    PDRIVER_ENTRY DriverEntry;
    PLOADED_IMAGE Image;
    KSTATUS Status;

    Image = LoadedImage;

    ASSERT(KeIsQueuedLockHeld(IoDeviceDatabaseLock) != FALSE);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    Driver = Image->SystemExtension;
    Status = STATUS_SUCCESS;
    if ((Driver->Flags & DRIVER_FLAG_ENTRY_CALLED) == 0) {

        //
        // Call the driver's entry point.
        //

        DriverEntry = (PDRIVER_ENTRY)Image->EntryPoint;
        if (DriverEntry != NULL) {
            Status = DriverEntry(Driver);
            Driver->Flags |= DRIVER_FLAG_ENTRY_CALLED;
            if (!KSUCCESS(Status)) {
                Driver->Flags |= DRIVER_FLAG_FAILED_DRIVER_ENTRY;
            }
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopInitializeImages (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine initializes any newly loaded images. This routine assumes the
    image list queued lock is already held.

Arguments:

    Process - Supplies a pointer to the process whose images should be
        initialized.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE Image;
    KSTATUS Status;
    KSTATUS TotalStatus;

    ASSERT(Process == PsGetKernelProcess());

    //
    // Iterate backwards to initialize dependency modules first.
    //

    TotalStatus = STATUS_SUCCESS;
    CurrentEntry = Process->ImageListHead.Previous;
    while (CurrentEntry != &(Process->ImageListHead)) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        CurrentEntry = CurrentEntry->Previous;
        if ((Image->Flags & IMAGE_FLAG_INITIALIZED) == 0) {
            Status = IopInitializeDriver(Image);
            if (KSUCCESS(Status)) {
                Image->Flags |= IMAGE_FLAG_INITIALIZED;

            } else {
                TotalStatus = Status;
            }
        }
    }

    return TotalStatus;
}

KSTATUS
IopAddDeviceDatabaseEntry (
    PCSTR DeviceOrClassId,
    PCSTR DriverName,
    PLIST_ENTRY DatabaseListHead
    )

/*++

Routine Description:

    This routine adds a mapping between a device and a driver or a device class
    and a driver. Only one device (or device class) to driver mapping can exist
    in the database at once. This routine must be called at low level.

Arguments:

    DeviceOrClassId - Supplies the device ID or class ID to associate with a
        driver. This memory does not need to be retained, a copy of this string
        will be created.

    DriverName - Supplies the name of the driver corresponding to the device.
        This memory does not need to be retained, a copy of this string will be
        created.

    DatabaseListHead - Supplies the list head of the database to insert this
        mapping in.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on an allocation failure.

    STATUS_DUPLICATE_ENTRY if the device ID already exists in the database.

--*/

{

    ULONG AllocationSize;
    PLIST_ENTRY CurrentEntry;
    PDEVICE_DATABASE_ENTRY DatabaseEntry;
    BOOL Equal;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(IoDeviceDatabaseLock);

    //
    // Loop through all mappings looking for an existing one, and fail if one
    // is found.
    //

    CurrentEntry = DatabaseListHead->Next;
    while (CurrentEntry != DatabaseListHead) {
        DatabaseEntry = LIST_VALUE(CurrentEntry,
                                   DEVICE_DATABASE_ENTRY,
                                   ListEntry);

        CurrentEntry = CurrentEntry->Next;
        Equal = RtlAreStringsEqual(DatabaseEntry->U.DeviceId,
                                   DeviceOrClassId,
                                   MAX_DEVICE_ID);

        if (Equal != FALSE) {
            Status = STATUS_DUPLICATE_ENTRY;
            goto AddDeviceDatabaseEntryEnd;
        }
    }

    //
    // Allocate space for the entry including both strings.
    //

    AllocationSize = sizeof(DEVICE_DATABASE_ENTRY);
    AllocationSize += RtlStringLength(DeviceOrClassId) + 1;
    AllocationSize += RtlStringLength(DriverName) + 1;
    DatabaseEntry = MmAllocatePagedPool(AllocationSize, IO_ALLOCATION_TAG);
    if (DatabaseEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceDatabaseEntryEnd;
    }

    RtlZeroMemory(DatabaseEntry, sizeof(DEVICE_DATABASE_ENTRY));

    //
    // Copy the strings into the extra space in the allocation.
    //

    DatabaseEntry->U.DeviceId = (PSTR)(DatabaseEntry + 1);
    RtlStringCopy(DatabaseEntry->U.DeviceId,
                  DeviceOrClassId,
                  RtlStringLength(DeviceOrClassId) + 1);

    DatabaseEntry->DriverName = DatabaseEntry->U.DeviceId +
                                RtlStringLength(DatabaseEntry->U.DeviceId) + 1;

    RtlStringCopy(DatabaseEntry->DriverName,
                  DriverName,
                  RtlStringLength(DriverName) + 1);

    INSERT_AFTER(&(DatabaseEntry->ListEntry), DatabaseListHead);
    Status = STATUS_SUCCESS;

AddDeviceDatabaseEntryEnd:
    KeReleaseQueuedLock(IoDeviceDatabaseLock);
    return Status;
}

