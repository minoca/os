/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    iobase.c

Abstract:

    This module implements the base I/O API (open, close, read, write).

Author:

    Evan Green 25-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/intrface/disk.h>
#include "iop.h"
#include "pagecach.h"

//
// ---------------------------------------------------------------- Definitions
//

#define IO_RENAME_ATTEMPTS_MAX 10000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopOpenPagingDevice (
    PDEVICE Device,
    ULONG Access,
    ULONG Flags,
    PPAGING_IO_HANDLE *Handle,
    PULONG IoOffsetAlignment,
    PULONG IoSizeAlignment,
    PULONGLONG IoCapacity
    );

KSTATUS
IopClosePagingObject (
    PPAGING_IO_HANDLE Handle
    );

KSTATUS
IopCreateAnonymousObject (
    BOOL FromKernelMode,
    ULONG Access,
    ULONG Flags,
    PCREATE_PARAMETERS Create,
    PPATH_POINT PathPoint
    );

KSTATUS
IopPerformIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT Context
    );

KSTATUS
IopPerformPagingIoOperation (
    PPAGING_IO_HANDLE Handle,
    PIO_CONTEXT Context,
    PIRP Irp
    );

KSTATUS
IopPerformCharacterDeviceIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT Context
    );

KSTATUS
IopPerformDirectoryIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT Context
    );

KSTATUS
IopAddRelativeDirectoryEntries (
    PIO_HANDLE Handle,
    PIO_OFFSET Offset,
    PIO_BUFFER IoBuffer,
    UINTN BufferSize,
    PUINTN BytesConsumed
    );

VOID
IopFixMountPointDirectoryEntries (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    UINTN BufferSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global I/O statistics.
//

IO_GLOBAL_STATISTICS IoGlobalStatistics;

//
// Set this boolean to print all open calls.
//

BOOL IoDebugPrintOpens;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoOpen (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG Access,
    ULONG Flags,
    FILE_PERMISSIONS CreatePermissions,
    PIO_HANDLE *Handle
    )

/*++

Routine Description:

    This routine opens a file, device, pipe, or other I/O object.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Directory - Supplies an optional pointer to an open handle to a directory
        for relative paths. Supply NULL to use the current working directory.

    Path - Supplies a pointer to the path to open.

    PathLength - Supplies the length of the path buffer in bytes, including the
        null terminator.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    CreatePermissions - Supplies the permissions to apply for a created file.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

{

    CREATE_PARAMETERS Create;
    PSTR Separator;
    KSTATUS Status;

    //
    // Do not allow shared memory object names with more than a leading slash.
    //

    if ((Flags & OPEN_FLAG_SHARED_MEMORY) != 0) {
        Separator = RtlStringFindCharacterRight(Path,
                                                PATH_SEPARATOR,
                                                PathLength);

        if ((Separator != NULL) && (Separator != Path)) {
            Status = STATUS_INVALID_PARAMETER;
            goto OpenEnd;
        }
    }

    if ((Flags & OPEN_FLAG_CREATE) != 0) {
        Create.Type = IoObjectInvalid;
        Create.Context = NULL;
        Create.Permissions = CreatePermissions;
        Create.Created = FALSE;
    }

    Status = IopOpen(FromKernelMode,
                     Directory,
                     Path,
                     PathLength,
                     Access,
                     Flags,
                     &Create,
                     Handle);

OpenEnd:
    if (IoDebugPrintOpens != FALSE) {
        RtlDebugPrint("Open %s: %d\n", Path, Status);
    }

    return Status;
}

KERNEL_API
KSTATUS
IoOpenDevice (
    PDEVICE Device,
    ULONG Access,
    ULONG Flags,
    PIO_HANDLE *Handle,
    PULONG IoOffsetAlignment,
    PULONG IoSizeAlignment,
    PULONGLONG IoCapacity
    )

/*++

Routine Description:

    This routine opens a device. If the given device is the device meant to
    hold the page file, this routine does not prepare the returned I/O handle
    for paging operations.

Arguments:

    Device - Supplies a pointer to a device to open.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer that receives the open I/O handle.

    IoOffsetAlignment - Supplies a pointer where the alignment requirement in
        bytes will be returned for all I/O offsets.

    IoSizeAlignment - Supplies a pointer where the alignment requirement for
        the size of all transfers (the block size) will be returned for all
        I/O requests.

    IoCapacity - Supplies the device's total size, in bytes.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PIO_HANDLE IoHandle;
    ULONGLONG LocalFileSize;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    IoHandle = NULL;
    if ((Flags & OPEN_FLAG_PAGING_DEVICE) != 0) {
        Status = IopOpenPagingDevice(Device,
                                     Access,
                                     Flags,
                                     (PPAGING_IO_HANDLE *)&IoHandle,
                                     IoOffsetAlignment,
                                     IoSizeAlignment,
                                     IoCapacity);

    } else {

        //
        // Open the device normally.
        //

        Status = IopOpenDevice(Device, Access, Flags, &IoHandle);
        if (!KSUCCESS(Status)) {
            goto OpenDeviceEnd;
        }

        //
        // Return the requested data.
        //

        FileObject = IoHandle->FileObject;
        LocalFileSize = FileObject->Properties.Size;
        if (IoOffsetAlignment != NULL) {
            *IoOffsetAlignment = FileObject->Properties.BlockSize;
        }

        if (IoSizeAlignment != NULL) {
            *IoSizeAlignment = FileObject->Properties.BlockSize;
        }

        if (IoCapacity != NULL) {
            *IoCapacity = LocalFileSize;
        }

        Status = STATUS_SUCCESS;
    }

OpenDeviceEnd:

    ASSERT((KSUCCESS(Status)) || (IoHandle == NULL));

    *Handle = IoHandle;
    return Status;
}

KERNEL_API
BOOL
IoIsPagingDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine determines whether or not paging is enabled on the given
    device.

Arguments:

    Device - Supplies a pointer to a device.

Return Value:

    Returns TRUE if paging is enabled on the device, or FALSE otherwise.

--*/

{

    if ((Device->Flags & DEVICE_FLAG_PAGING_DEVICE) != 0) {
        return TRUE;
    }

    return FALSE;
}

KERNEL_API
KSTATUS
IoClose (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine closes a file or device.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle returned when the file was
        opened.

Return Value:

    Status code. Close operations can fail if their associated flushes to
    the file system fail.

--*/

{

    KSTATUS Status;

    if (IoHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (IoHandle->HandleType) {
    case IoHandleTypeDefault:
        Status = IoIoHandleReleaseReference(IoHandle);
        break;

    case IoHandleTypePaging:
        Status = IopClosePagingObject((PPAGING_IO_HANDLE)IoHandle);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_HANDLE;
        break;
    }

    return Status;
}

KERNEL_API
KSTATUS
IoRead (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine reads from an I/O object.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer where the read data will be
        returned on success.

    SizeInBytes - Supplies the number of bytes to read.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        read will be returned.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    IO_CONTEXT Context;
    PIO_HANDLE ReadHandle;
    KSTATUS Status;

    //
    // No-allocate code paths should not be calling I/O read. They should use
    // the offset-based read routine.
    //

    ASSERT((Flags & IO_FLAG_NO_ALLOCATE) == 0);

    //
    // The special page file no-allocate read operation is not supported by
    // this routine. An offset must be supplied for said reads.
    //

    if ((Flags & IO_FLAG_NO_ALLOCATE) != 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReadEnd;
    }

    //
    // Find the correct I/O handle.
    //

    if (Handle->HandleType == IoHandleTypePaging) {
        ReadHandle = ((PPAGING_IO_HANDLE)Handle)->IoHandle;

    } else {
        ReadHandle = Handle;
    }

    Context.IoBuffer = IoBuffer;
    Context.Offset = IO_OFFSET_NONE;
    Context.SizeInBytes = SizeInBytes;
    Context.BytesCompleted = 0;
    Context.Flags = Flags;
    Context.TimeoutInMilliseconds = TimeoutInMilliseconds;
    Context.Write = FALSE;
    Status = IopPerformIoOperation(ReadHandle, &Context);
    *BytesCompleted = Context.BytesCompleted;

ReadEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoWrite (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine writes to an I/O object.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer containing the data to write.

    SizeInBytes - Supplies the number of bytes to write.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        written will be returned.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    IO_CONTEXT Context;
    KSTATUS Status;
    PIO_HANDLE WriteHandle;

    //
    // No-allocate code paths should not be calling I/O write. They should use
    // the offset-based write routine.
    //

    ASSERT((Flags & IO_FLAG_NO_ALLOCATE) == 0);

    //
    // The special page file no-allocate write operation is not supported by
    // this routine. An offset must be supplied for said writes.
    //

    if ((Flags & IO_FLAG_NO_ALLOCATE) != 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto WriteEnd;
    }

    //
    // Find the correct I/O handle.
    //

    if (Handle->HandleType == IoHandleTypePaging) {
        WriteHandle = ((PPAGING_IO_HANDLE)Handle)->IoHandle;

    } else {
        WriteHandle = Handle;
    }

    Context.IoBuffer = IoBuffer;
    Context.Offset = IO_OFFSET_NONE;
    Context.SizeInBytes = SizeInBytes;
    Context.BytesCompleted = 0;
    Context.Flags = Flags;
    Context.TimeoutInMilliseconds = TimeoutInMilliseconds;
    Context.Write = TRUE;
    Status = IopPerformIoOperation(WriteHandle, &Context);
    *BytesCompleted = Context.BytesCompleted;

WriteEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoReadAtOffset (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    IO_OFFSET Offset,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted,
    PIRP Irp
    )

/*++

Routine Description:

    This routine reads from an I/O object at a specific offset.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer where the read data will be
        returned on success.

    Offset - Supplies the offset from the beginning of the file or device where
        the I/O should be done.

    SizeInBytes - Supplies the number of bytes to read.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        read will be returned.

    Irp - Supplies a pointer to the IRP to use for this I/O. This is required
        when doing operations on the page file.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    IO_CONTEXT Context;
    PIO_HANDLE ReadHandle;
    KSTATUS Status;

    //
    // Determine the correct read handle. Only perform paging I/O operations
    // when operating on the page file. It is not enough to look at the I/O
    // handle's open flags. There could be reads from the page file or paging
    // device that are not in no-allocate code paths. The caller must dictate
    // the no-allocate code path.
    //

    if ((Handle->HandleType == IoHandleTypePaging) &&
        ((Flags & IO_FLAG_NO_ALLOCATE) == 0)) {

        ReadHandle = ((PPAGING_IO_HANDLE)Handle)->IoHandle;

    } else {
        ReadHandle = Handle;
    }

    Context.IoBuffer = IoBuffer;
    Context.Offset = Offset;
    Context.SizeInBytes = SizeInBytes;
    Context.BytesCompleted = 0;
    Context.Flags = Flags;
    Context.TimeoutInMilliseconds = TimeoutInMilliseconds;
    Context.Write = FALSE;

    //
    // Perform the read operation based on the read handle.
    //

    switch (ReadHandle->HandleType) {
    case IoHandleTypeDefault:
        Status = IopPerformIoOperation(ReadHandle, &Context);
        break;

    case IoHandleTypePaging:
        if ((Irp == NULL) || (TimeoutInMilliseconds != WAIT_TIME_INDEFINITE)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        Status = IopPerformPagingIoOperation((PPAGING_IO_HANDLE)ReadHandle,
                                             &Context,
                                             Irp);

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_HANDLE;
        break;
    }

    *BytesCompleted = Context.BytesCompleted;
    return Status;
}

KERNEL_API
KSTATUS
IoWriteAtOffset (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    IO_OFFSET Offset,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted,
    PIRP Irp
    )

/*++

Routine Description:

    This routine writes to an I/O object at a specific offset.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer containing the data to write.

    Offset - Supplies the offset from the beginning of the file or device where
        the I/O should be done.

    SizeInBytes - Supplies the number of bytes to write.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        written will be returned.

    Irp - Supplies a pointer to the IRP to use for this I/O. This is required
        when doing operations on the page file.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    IO_CONTEXT Context;
    KSTATUS Status;
    PIO_HANDLE WriteHandle;

    //
    // Determine the correct write handle. Only perform paging I/O operations
    // when operating on the page file. It is not enough to look at the I/O
    // handle's open flags. There could be writes to the page file or paging
    // device that are not in no-allocate code paths. The caller must dictate
    // the no-allocate code path.
    //

    if ((Handle->HandleType == IoHandleTypePaging) &&
        ((Flags & IO_FLAG_NO_ALLOCATE) == 0)) {

        WriteHandle = ((PPAGING_IO_HANDLE)Handle)->IoHandle;

    } else {
        WriteHandle = Handle;
    }

    Context.IoBuffer = IoBuffer;
    Context.Offset = Offset;
    Context.SizeInBytes = SizeInBytes;
    Context.BytesCompleted = 0;
    Context.Flags = Flags;
    Context.TimeoutInMilliseconds = TimeoutInMilliseconds;
    Context.Write = TRUE;
    switch (WriteHandle->HandleType) {
    case IoHandleTypeDefault:
        Status = IopPerformIoOperation(WriteHandle, &Context);
        break;

    case IoHandleTypePaging:
        if ((Irp == NULL) || (TimeoutInMilliseconds != WAIT_TIME_INDEFINITE)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        Status = IopPerformPagingIoOperation((PPAGING_IO_HANDLE)WriteHandle,
                                             &Context,
                                             Irp);

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_HANDLE;
        break;
    }

    *BytesCompleted = Context.BytesCompleted;
    return Status;
}

KERNEL_API
KSTATUS
IoFlush (
    PIO_HANDLE Handle,
    IO_OFFSET Offset,
    ULONGLONG Size,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes I/O data to its appropriate backing device.

Arguments:

    Handle - Supplies an open I/O handle. This parameters is not required if
        the FLUSH_FLAG_ALL flag is set.

    Offset - Supplies the offset from the beginning of the file or device where
        the flush should be done.

    Size - Supplies the size, in bytes, of the region to flush. Supply a value
        of -1 to flush from the given offset to the end of the file.

    Flags - Supplies flags regarding the flush operation. See FLUSH_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    ULONG IoFlags;
    KSTATUS Status;

    IoFlags = IO_FLAG_DATA_SYNCHRONIZED | IO_FLAG_METADATA_SYNCHRONIZED;

    //
    // Handle the flush-all synchronous case, where this routine will not
    // return until all dirty data made it out to disk.
    //

    if ((Flags & FLUSH_FLAG_ALL_SYNCHRONOUS) != 0) {
        if (Handle != INVALID_HANDLE) {
            Status = STATUS_INVALID_PARAMETER;
            goto FlushEnd;
        }

        //
        // Flushing synchronously will get all dirty file data and meta-data to
        // its underlying block device. It will also flush any dirty block
        // device data that has no association with the file layer.
        //

        Status = IopFlushFileObjects(0, IoFlags, NULL);
        goto FlushEnd;

    //
    // Handle the flush-all case. Just notify the page cache worker to run and
    // exit. This does not need to wait until the writes complete.
    //

    } else if ((Flags & FLUSH_FLAG_ALL) != 0) {

        //
        // If a handle was provided, something isn't right.
        //

        if (Handle != INVALID_HANDLE) {
            Status = STATUS_INVALID_PARAMETER;
            goto FlushEnd;
        }

        IopSchedulePageCacheThread();
        Status = STATUS_SUCCESS;
        goto FlushEnd;
    }

    //
    // Otherwise, flush the data for the specific handle. If the data for the
    // handle is not in the cache because it's not cacheable, exit successfully.
    //

    FileObject = Handle->FileObject;
    if ((FileObject->Properties.Type == IoObjectTerminalMaster) ||
        (FileObject->Properties.Type == IoObjectTerminalSlave)) {

        Status = IopTerminalFlush(FileObject, Flags);
        if (!KSUCCESS(Status)) {
            goto FlushEnd;
        }

    } else if (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE) {
        if ((Flags &
             (FLUSH_FLAG_READ | FLUSH_FLAG_WRITE | FLUSH_FLAG_DISCARD)) != 0) {

            Status = STATUS_INVALID_PARAMETER;
            goto FlushEnd;
        }

        Status = IopFlushFileObject(FileObject,
                                    Offset,
                                    Size,
                                    IoFlags,
                                    TRUE,
                                    NULL);

        if (!KSUCCESS(Status)) {
            goto FlushEnd;
        }

    } else {
        Status = STATUS_SUCCESS;
        goto FlushEnd;
    }

FlushEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoSeek (
    PIO_HANDLE Handle,
    SEEK_COMMAND SeekCommand,
    IO_OFFSET Offset,
    PIO_OFFSET NewOffset
    )

/*++

Routine Description:

    This routine seeks to the given position in a file. This routine is only
    relevant for normal file or block based devices.

Arguments:

    Handle - Supplies the open I/O handle.

    SeekCommand - Supplies the reference point for the seek offset. Usual
        reference points are the beginning of the file, current file position,
        and the end of the file.

    Offset - Supplies the offset from the reference point to move in bytes.

    NewOffset - Supplies an optional pointer where the file position after the
        move will be returned on success.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    IO_OFFSET FileSize;
    IO_OFFSET LocalNewOffset;
    IO_OFFSET OldOffset;
    IO_OFFSET PreviousOffset;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    FileObject = Handle->FileObject;
    switch (FileObject->Properties.Type) {
    case IoObjectRegularFile:
    case IoObjectRegularDirectory:
    case IoObjectBlockDevice:
    case IoObjectObjectDirectory:
    case IoObjectSharedMemoryObject:
        break;

    default:
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Loop trying to perform the update atomically.
    //

    while (TRUE) {
        OldOffset = RtlAtomicOr64((PULONGLONG)&(Handle->CurrentOffset), 0);
        switch (SeekCommand) {
        case SeekCommandNop:
            LocalNewOffset = OldOffset;
            Status = STATUS_SUCCESS;
            goto SeekEnd;

        case SeekCommandFromCurrentOffset:
            LocalNewOffset = OldOffset + Offset;
            break;

        //
        // Add the file size to the offset and then fall through to handle
        // seeking from the end the same as seeking from the beginning.
        //

        case SeekCommandFromEnd:
            FileSize = FileObject->Properties.Size;
            LocalNewOffset = Offset + FileSize;
            break;

        case SeekCommandFromBeginning:
            LocalNewOffset = Offset;
            break;

        default:
            LocalNewOffset = 0;
            Status = STATUS_INVALID_PARAMETER;
            goto SeekEnd;
        }

        if (LocalNewOffset < 0) {
            Status = STATUS_INVALID_PARAMETER;
            goto SeekEnd;
        }

        PreviousOffset = RtlAtomicCompareExchange64(
                                          (PULONGLONG)&(Handle->CurrentOffset),
                                          (ULONGLONG)LocalNewOffset,
                                          (ULONGLONG)OldOffset);

        if (PreviousOffset == OldOffset) {
            break;
        }
    }

    Status = STATUS_SUCCESS;

SeekEnd:
    if (NewOffset != NULL) {
        *NewOffset = LocalNewOffset;
    }

    return Status;
}

KERNEL_API
KSTATUS
IoGetFileSize (
    PIO_HANDLE Handle,
    PULONGLONG FileSize
    )

/*++

Routine Description:

    This routine returns the current size of the given file or block device.

Arguments:

    Handle - Supplies the open file handle.

    FileSize - Supplies a pointer where the file size will be returned on
        success.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    ULONGLONG LocalFileSize;
    PPAGING_IO_HANDLE PagingHandle;
    KSTATUS Status;

    if (Handle->HandleType == IoHandleTypePaging) {
        PagingHandle = (PPAGING_IO_HANDLE)Handle;
        Handle = PagingHandle->IoHandle;
    }

    FileObject = Handle->FileObject;
    LocalFileSize = FileObject->Properties.Size;
    *FileSize = LocalFileSize;
    Status = STATUS_SUCCESS;
    return Status;
}

KERNEL_API
KSTATUS
IoGetFileInformation (
    PIO_HANDLE Handle,
    PFILE_PROPERTIES FileProperties
    )

/*++

Routine Description:

    This routine gets the file properties for the given I/O handle.

Arguments:

    Handle - Supplies the open file handle.

    FileProperties - Supplies a pointer where the file properties will be
        returned on success.

Return Value:

    Status code.

--*/

{

    SET_FILE_INFORMATION Request;
    KSTATUS Status;

    Request.FieldsToSet = 0;
    Request.FileProperties = FileProperties;
    Status = IoSetFileInformation(TRUE, Handle, &Request);
    return Status;
}

KERNEL_API
KSTATUS
IoSetFileInformation (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PSET_FILE_INFORMATION Request
    )

/*++

Routine Description:

    This routine sets the file properties for the given I/O handle.
    Only some properties can be set by this routine.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request
        originated from user mode (FALSE) or kernel mode (TRUE). Kernel mode
        requests bypass permission checks.

    Handle - Supplies the open file handle.

    Request - Supplies a pointer to the get/set information request.

Return Value:

    Status code.

--*/

{

    ULONG FieldsToSet;
    PFILE_OBJECT FileObject;
    BOOL FileOwner;
    PFILE_PROPERTIES FileProperties;
    BOOL HasChownPermission;
    FILE_PROPERTIES LocalProperties;
    BOOL LockHeldExclusive;
    BOOL LockHeldShared;
    BOOL ModifyFileSize;
    IO_OFFSET NewFileSize;
    KSTATUS Status;
    BOOL StatusChanged;
    PKTHREAD Thread;
    BOOL Updated;

    LockHeldExclusive = FALSE;
    LockHeldShared = FALSE;
    Thread = KeGetCurrentThread();
    FieldsToSet = Request->FieldsToSet;
    if (FromKernelMode != FALSE) {
        FileProperties = Request->FileProperties;

    } else {
        FileProperties = &LocalProperties;
    }

    if (FieldsToSet == 0) {
        RtlZeroMemory(FileProperties, sizeof(FILE_PROPERTIES));
    }

    Updated = FALSE;
    StatusChanged = FALSE;

    //
    // Operate on the file object that was actually opened, not the file object
    // doing all the I/O.
    //

    FileObject = Handle->PathPoint.PathEntry->FileObject;
    if (FromKernelMode == FALSE) {

        //
        // Copy the properties from the user mode buffer.
        //

        if (FieldsToSet != 0) {
            Status = MmCopyFromUserMode(FileProperties,
                                        Request->FileProperties,
                                        sizeof(FILE_PROPERTIES));

            if (!KSUCCESS(Status)) {
                goto SetFileInformationEnd;
            }
        }

        FileOwner = FALSE;
        if ((FileObject->Properties.UserId ==
             Thread->Identity.EffectiveUserId) ||
            (KSUCCESS(PsCheckPermission(PERMISSION_FILE_OWNER)))) {

            FileOwner = TRUE;
        }

        //
        // Perform permission checking. Only a privileged user can change the
        // file owner.
        //

        HasChownPermission = FALSE;
        Status = PsCheckPermission(PERMISSION_CHOWN);
        if (KSUCCESS(Status)) {
            HasChownPermission = TRUE;
        }

        Status = STATUS_PERMISSION_DENIED;
        if ((FieldsToSet & FILE_PROPERTY_FIELD_USER_ID) != 0) {
            if (HasChownPermission == FALSE) {

                //
                // Succeed a "non-change" for a file already owned by the user.
                //

                if ((FileOwner != FALSE) &&
                    (FileObject->Properties.UserId ==
                     FileProperties->UserId)) {

                    FieldsToSet &= ~FILE_PROPERTY_FIELD_USER_ID;

                } else {
                    goto SetFileInformationEnd;
                }
            }
        }

        //
        // An unprivileged user can change the group of a file they own to any
        // group of which they are also a member (ie Mickey can change the file
        // to any of his mouseketeer clubs).
        //

        if ((FieldsToSet & FILE_PROPERTY_FIELD_GROUP_ID) != 0) {
            if (HasChownPermission == FALSE) {
                if (FileOwner == FALSE) {
                    goto SetFileInformationEnd;
                }

                if (PsIsUserInGroup(FileObject->Properties.GroupId) == FALSE) {
                    goto SetFileInformationEnd;
                }
            }
        }

        //
        // Only the owner of the file may change the permissions and times on
        // it.
        //

        if ((FieldsToSet & FILE_PROPERTY_OWNER_OWNED_FIELDS) != 0) {
            if (FileOwner == FALSE) {
                goto SetFileInformationEnd;
            }
        }

    } else {
        HasChownPermission = TRUE;
        FileOwner = TRUE;
    }

    //
    // Truncating a file requires the caller to be able to write to it.
    //

    if ((FieldsToSet & FILE_PROPERTY_FIELD_FILE_SIZE) != 0) {
        Status = IopCheckPermissions(FromKernelMode,
                                     &(Handle->PathPoint),
                                     IO_ACCESS_WRITE);

        if (!KSUCCESS(Status)) {
            goto SetFileInformationEnd;
        }
    }

    ModifyFileSize = FALSE;
    NewFileSize = 0;
    if (FieldsToSet != 0) {
        KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
        LockHeldExclusive = TRUE;

    } else {
        KeAcquireSharedExclusiveLockShared(FileObject->Lock);
        LockHeldShared = TRUE;
    }

    //
    // Not all attributes can be set for symbolic links.
    //

    if (FileObject->Properties.Type == IoObjectSymbolicLink) {
        FieldsToSet &= FILE_PROPERTY_FIELD_USER_ID |
                       FILE_PROPERTY_FIELD_GROUP_ID |
                       FILE_PROPERTY_FIELD_ACCESS_TIME |
                       FILE_PROPERTY_FIELD_MODIFIED_TIME |
                       FILE_PROPERTY_FIELD_STATUS_CHANGE_TIME;
    }

    if (FieldsToSet != 0) {

        //
        // Object directories cannot be altered.
        //

        if (FileObject->Properties.Type == IoObjectObjectDirectory) {
            Status = STATUS_NOT_SUPPORTED;
            goto SetFileInformationEnd;
        }

        //
        // If the owner or group are changed by an unprivileged user, the
        // setuid and setgid bits are cleared from the file.
        //

        if (((FieldsToSet &
              (FILE_PROPERTY_FIELD_USER_ID |
               FILE_PROPERTY_FIELD_GROUP_ID)) != 0) &&
            (HasChownPermission == FALSE)) {

            FileObject->Properties.Permissions &=
                                           ~(FILE_PERMISSION_SET_USER_ID |
                                             FILE_PERMISSION_SET_GROUP_ID);

            Updated = TRUE;
            StatusChanged = TRUE;
        }

        if ((FieldsToSet & FILE_PROPERTY_FIELD_USER_ID) != 0) {
            FileObject->Properties.UserId = FileProperties->UserId;
            Updated = TRUE;
            StatusChanged = TRUE;
        }

        if ((FieldsToSet & FILE_PROPERTY_FIELD_GROUP_ID) != 0) {
            FileObject->Properties.GroupId = FileProperties->GroupId;
            Updated = TRUE;
            StatusChanged = TRUE;
        }

        if ((FieldsToSet & FILE_PROPERTY_FIELD_PERMISSIONS) != 0) {
            FileObject->Properties.Permissions =
                        FileProperties->Permissions & FILE_PERMISSION_MASK;

            Updated = TRUE;
            StatusChanged = TRUE;

            //
            // If the permissions are being changed by an unprivileged
            // owner, and the caller is not a member of the file group, the
            // setgid permission is removed.
            //

            if ((FromKernelMode == FALSE) &&
                (!KSUCCESS(PsCheckPermission(PERMISSION_FILE_OWNER))) &&
                (PsIsUserInGroup(FileObject->Properties.GroupId) ==
                 FALSE)) {

                FileObject->Properties.Permissions &=
                                             ~FILE_PERMISSION_SET_GROUP_ID;
            }
        }

        if ((FieldsToSet & FILE_PROPERTY_FIELD_ACCESS_TIME) != 0) {
            FileObject->Properties.AccessTime = FileProperties->AccessTime;
            Updated = TRUE;
            StatusChanged = TRUE;
        }

        if ((FieldsToSet & FILE_PROPERTY_FIELD_MODIFIED_TIME) != 0) {
            FileObject->Properties.ModifiedTime =
                                              FileProperties->ModifiedTime;

            Updated = TRUE;
            StatusChanged = TRUE;
        }

        if ((FieldsToSet & FILE_PROPERTY_FIELD_STATUS_CHANGE_TIME) != 0) {
            FileObject->Properties.StatusChangeTime =
                                          FileProperties->StatusChangeTime;

            Updated = TRUE;
        }

        if ((FieldsToSet & FILE_PROPERTY_FIELD_FILE_SIZE) != 0) {

            //
            // Some types cannot have their file sizes modified.
            //

            switch (FileObject->Properties.Type) {
            case IoObjectRegularFile:
            case IoObjectSharedMemoryObject:
                break;

            default:
                Status = STATUS_PERMISSION_DENIED;
                goto SetFileInformationEnd;
            }

            ModifyFileSize = TRUE;
            NewFileSize = FileProperties->Size;
        }

    } else {
        RtlCopyMemory(FileProperties,
                      &(FileObject->Properties),
                      sizeof(FILE_PROPERTIES));
    }

    //
    // If the file status was changed, update the file status change time.
    // Don't do this if the caller explicitly changed the status change
    // time field.
    //

    if ((StatusChanged != FALSE) &&
        ((FieldsToSet & FILE_PROPERTY_FIELD_STATUS_CHANGE_TIME) == 0)) {

        KeGetSystemTime(&(FileObject->Properties.StatusChangeTime));
    }

    if (LockHeldExclusive != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
        LockHeldExclusive = FALSE;

    } else {
        KeReleaseSharedExclusiveLockShared(FileObject->Lock);
        LockHeldShared = FALSE;
    }

    //
    // With the spin lock released, go ahead and modify the file size if
    // requested.
    //

    if (ModifyFileSize != FALSE) {
        Status = IopModifyFileObjectSize(FileObject,
                                         Handle->DeviceContext,
                                         NewFileSize);

        if (!KSUCCESS(Status)) {
            goto SetFileInformationEnd;
        }
    }

    if (Updated != FALSE) {
        IopMarkFileObjectPropertiesDirty(FileObject);
    }

    Status = STATUS_SUCCESS;

SetFileInformationEnd:
    if (LockHeldExclusive != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);

    } else if (LockHeldShared != FALSE) {
        KeReleaseSharedExclusiveLockShared(FileObject->Lock);
    }

    //
    // Copy the buffer back to user mode if this is a successful get request.
    //

    if ((KSUCCESS(Status)) && (FromKernelMode == FALSE) && (FieldsToSet == 0)) {
        Status = MmCopyToUserMode(Request->FileProperties,
                                  FileProperties,
                                  sizeof(FILE_PROPERTIES));
    }

    return Status;
}

KERNEL_API
KSTATUS
IoDelete (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR Path,
    ULONG PathSize,
    ULONG Flags
    )

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path points to
    a file object or shared memory object, its hard link count is decremented.
    If the hard link count reaches zero and no processes have the object open,
    the contents of the object are destroyed. If processes have open handles to
    the object, the destruction of the object contents are deferred until the
    last handle to the old file is closed. If the path points to a symbolic
    link, the link itself is removed and not the destination. The removal of
    the entry from the directory is immediate.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Directory - Supplies an optional pointer to an open handle to a directory
        for relative paths. Supply NULL to use the current working directory.

    Path - Supplies a pointer to the path to delete.

    PathSize - Supplies the length of the path buffer in bytes, including the
        null terminator.

    Flags - Supplies a bitfield of flags. See DELETE_FLAG_* definitions.

Return Value:

    Status code.

--*/

{

    PPATH_ENTRY DirectoryEntry;
    PFILE_OBJECT DirectoryFileObject;
    PPATH_POINT DirectoryPathPoint;
    ULONG OpenFlags;
    PATH_POINT PathPoint;
    KSTATUS Status;

    DirectoryPathPoint = NULL;
    PathPoint.PathEntry = NULL;

    //
    // If the caller specified a directory, validate that it is a directory.
    // Search permission checking for the directory is done in the path walk
    // code.
    //

    if (Directory != NULL) {
        DirectoryPathPoint = &(Directory->PathPoint);
        DirectoryEntry = DirectoryPathPoint->PathEntry;
        DirectoryFileObject = DirectoryEntry->FileObject;
        if (DirectoryFileObject->Properties.Type != IoObjectRegularDirectory) {
            Status = STATUS_NOT_A_DIRECTORY;
            goto DeleteEnd;
        }

        ASSERT(Directory->FileObject == DirectoryFileObject);
    }

    OpenFlags = OPEN_FLAG_SYMBOLIC_LINK | OPEN_FLAG_NO_MOUNT_POINT;
    if ((Flags & DELETE_FLAG_SHARED_MEMORY) != 0) {
        OpenFlags |= OPEN_FLAG_SHARED_MEMORY;
    }

    Status = IopPathWalk(FromKernelMode,
                         DirectoryPathPoint,
                         &Path,
                         &PathSize,
                         OpenFlags,
                         NULL,
                         &PathPoint);

    if (!KSUCCESS(Status)) {
        goto DeleteEnd;
    }

    Status = IopDeletePathPoint(FromKernelMode, &PathPoint, Flags);
    if (!KSUCCESS(Status)) {
        goto DeleteEnd;
    }

DeleteEnd:
    if (PathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&PathPoint);
    }

    return Status;
}

KERNEL_API
KSTATUS
IoRename (
    BOOL FromKernelMode,
    PIO_HANDLE SourceStartDirectory,
    PCSTR SourcePath,
    ULONG SourcePathSize,
    PIO_HANDLE DestinationStartDirectory,
    PCSTR DestinationPath,
    ULONG DestinationPathSize
    )

/*++

Routine Description:

    This routine attempts to rename the object at the given path. This routine
    operates on symbolic links themselves, not the destinations of symbolic
    links. If the source and destination paths are equal, this routine will do
    nothing and return successfully. If the source path is not a directory, the
    destination path must not be a directory. If the destination file exists,
    it will be deleted. The caller must have write access in both the old and
    new directories. If the source path is a directory, the destination path
    must not exist or be an empty directory. The destination path must not have
    a path prefix of the source (ie it's illegal to move /my/path into
    /my/path/stuff).

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    SourceStartDirectory - Supplies an optional pointer to a handle to the
        directory to start at for relative source paths. If the source path is
        not relative, this parameter is ignored. If this is not supplied, then
        the current working directory of the process is used.

    SourcePath - Supplies a pointer to the path of the file to rename.

    SourcePathSize - Supplies the length of the source path buffer in bytes,
        including the null terminator.

    DestinationStartDirectory - Supplies an optional pointer to the directory
        to start at for relative destination paths. If the destination path is
        not relative, this parameter is ignored. If this is not supplied, then
        the current working directory of the process is used.

    DestinationPath - Supplies a pointer to the path to rename the file to.

    DestinationPathSize - Supplies the size of the destination path buffer in
        bytes, including the null terminator.

Return Value:

    Status code.

--*/

{

    ULONG Attempts;
    BOOL DescendantPath;
    PSTR DestinationDirectory;
    PFILE_OBJECT DestinationDirectoryFileObject;
    PATH_POINT DestinationDirectoryPathPoint;
    ULONG DestinationDirectorySize;
    PSTR DestinationFile;
    PFILE_OBJECT DestinationFileObject;
    ULONG DestinationFileSize;
    PATH_POINT DestinationPathPoint;
    PPATH_POINT DestinationStartPathPoint;
    PDEVICE Device;
    PATH_POINT FoundPathPoint;
    PCSTR LocalDestinationPath;
    ULONG LocalDestinationPathSize;
    PCSTR LocalSourcePath;
    ULONG LocalSourcePathSize;
    BOOL LocksHeld;
    ULONG NameHash;
    PPATH_ENTRY NewPathEntry;
    SYSTEM_CONTROL_RENAME RenameRequest;
    PFILE_OBJECT SourceDirectoryFileObject;
    PATH_POINT SourceDirectoryPathPoint;
    PFILE_OBJECT SourceFileObject;
    PATH_POINT SourcePathPoint;
    PPATH_POINT SourceStartPathPoint;
    KSTATUS Status;

    DestinationDirectory = NULL;
    DestinationDirectoryPathPoint.PathEntry = NULL;
    DestinationPathPoint.PathEntry = NULL;
    DestinationFile = NULL;
    DestinationFileObject = NULL;
    DestinationStartPathPoint = NULL;
    FoundPathPoint.PathEntry = NULL;
    LocksHeld = FALSE;
    NewPathEntry = NULL;
    SourceDirectoryPathPoint.PathEntry = NULL;
    SourceFileObject = NULL;
    SourcePathPoint.PathEntry = NULL;
    SourceStartPathPoint = NULL;
    if ((SourcePathSize <= 1) || (DestinationPathSize <= 1)) {
        Status = STATUS_PATH_NOT_FOUND;
        goto RenameEnd;
    }

    if (SourceStartDirectory != NULL) {
        SourceStartPathPoint = &(SourceStartDirectory->PathPoint);
        if (SourceStartPathPoint->PathEntry->FileObject->Properties.Type !=
            IoObjectRegularDirectory) {

            Status = STATUS_NOT_A_DIRECTORY;
            goto RenameEnd;
        }

        ASSERT(SourceStartDirectory->FileObject ==
               SourceStartPathPoint->PathEntry->FileObject);
    }

    if (DestinationStartDirectory != NULL) {
        DestinationStartPathPoint = &(DestinationStartDirectory->PathPoint);
        if (DestinationStartPathPoint->PathEntry->FileObject->Properties.Type !=
            IoObjectRegularDirectory) {

            Status = STATUS_NOT_A_DIRECTORY;
            goto RenameEnd;
        }

        ASSERT(DestinationStartDirectory->FileObject ==
               DestinationStartPathPoint->PathEntry->FileObject);
    }

    //
    // Loop trying to rename the source to the destination. The loop is
    // necessary because things may change before the appropriate locks are
    // acquired. Once the locks are acquired, the state is checked and if it is
    // not good enough to proceed, the whole process gets restarted.
    //

    Attempts = 0;
    Status = STATUS_TRY_AGAIN;
    while (Attempts < IO_RENAME_ATTEMPTS_MAX) {

        //
        // Get the source file, which must exist.
        //

        LocalSourcePath = SourcePath;
        LocalSourcePathSize = SourcePathSize;
        Status = IopPathWalk(FromKernelMode,
                             SourceStartPathPoint,
                             &LocalSourcePath,
                             &LocalSourcePathSize,
                             OPEN_FLAG_SYMBOLIC_LINK | OPEN_FLAG_NO_MOUNT_POINT,
                             NULL,
                             &SourcePathPoint);

        if (!KSUCCESS(Status)) {
            goto RenameEnd;
        }

        //
        // Rename is not allowed if the source is mounted anywhere.
        //

        if (SourcePathPoint.PathEntry->MountCount != 0) {
            Status = STATUS_RESOURCE_IN_USE;
            goto RenameEnd;
        }

        //
        // Get the source directory entry and file object.
        //

        IopGetParentPathPoint(NULL,
                              &SourcePathPoint,
                              &SourceDirectoryPathPoint);

        SourceDirectoryFileObject =
                                SourceDirectoryPathPoint.PathEntry->FileObject;

        ASSERT(SourceDirectoryFileObject != NULL);
        ASSERT(SourcePathPoint.MountPoint ==
               SourceDirectoryPathPoint.MountPoint);

        //
        // Check to see that the caller has permission to delete something from
        // the source directory.
        //

        if (FromKernelMode == FALSE) {
            Status = IopCheckDeletePermission(FromKernelMode,
                                              &SourceDirectoryPathPoint,
                                              &SourcePathPoint);

            if (!KSUCCESS(Status)) {
                goto RenameEnd;
            }
        }

        SourceFileObject = SourcePathPoint.PathEntry->FileObject;

        ASSERT(SourceFileObject->Properties.DeviceId ==
               SourceDirectoryFileObject->Properties.DeviceId);

        //
        // Split the destination path into a file part and a directory part.
        //

        Status = IopPathSplit(DestinationPath,
                              DestinationPathSize,
                              &DestinationDirectory,
                              &DestinationDirectorySize,
                              &DestinationFile,
                              &DestinationFileSize);

        if (!KSUCCESS(Status)) {
            goto RenameEnd;
        }

        //
        // Get the destination file, which may or may not exist.
        //

        LocalDestinationPath = DestinationPath;
        LocalDestinationPathSize = DestinationPathSize;
        Status = IopPathWalk(FromKernelMode,
                             DestinationStartPathPoint,
                             &LocalDestinationPath,
                             &LocalDestinationPathSize,
                             OPEN_FLAG_SYMBOLIC_LINK | OPEN_FLAG_NO_MOUNT_POINT,
                             NULL,
                             &DestinationPathPoint);

        if (!KSUCCESS(Status)) {
            if (Status != STATUS_PATH_NOT_FOUND) {
                goto RenameEnd;
            }

            ASSERT(DestinationPathPoint.PathEntry == NULL);

            //
            // Try to find the destination file's directory.
            //

            LocalDestinationPath = DestinationDirectory;
            LocalDestinationPathSize = DestinationDirectorySize;
            if ((LocalDestinationPathSize == 0) ||
                ((LocalDestinationPathSize == 1) &&
                 (LocalDestinationPath[0] == '\0'))) {

                LocalDestinationPath = ".";
                LocalDestinationPathSize = sizeof(".");
            }

            Status = IopPathWalk(FromKernelMode,
                                 DestinationStartPathPoint,
                                 &LocalDestinationPath,
                                 &LocalDestinationPathSize,
                                 OPEN_FLAG_SYMBOLIC_LINK,
                                 NULL,
                                 &DestinationDirectoryPathPoint);

            if (!KSUCCESS(Status)) {
                goto RenameEnd;
            }

            DestinationDirectoryFileObject =
                           DestinationDirectoryPathPoint.PathEntry->FileObject;

            //
            // Require write permission on the directory since the destination
            // does not exist.
            //

            Status = IopCheckPermissions(FromKernelMode,
                                         &DestinationDirectoryPathPoint,
                                         IO_ACCESS_WRITE);

            if (!KSUCCESS(Status)) {
                goto RenameEnd;
            }

        //
        // The destination file exists.
        //

        } else {
            DestinationFileObject = DestinationPathPoint.PathEntry->FileObject;

            //
            // If the destination is the same as the source, then it's a no-op.
            //

            if (SourceFileObject == DestinationFileObject) {
                Status = STATUS_SUCCESS;
                goto RenameEnd;
            }

            //
            // If the source is not a directory, the destination cannot be a
            // directory.
            //

            if ((SourceFileObject->Properties.Type !=
                 IoObjectRegularDirectory) &&
                (DestinationFileObject->Properties.Type ==
                 IoObjectRegularDirectory)) {

                Status = STATUS_FILE_IS_DIRECTORY;
                goto RenameEnd;
            }

            //
            // If the source is a directory, the destination must be a
            // directory. The check for that destination to be empty will be
            // done in the file system.
            //

            if ((SourceFileObject->Properties.Type ==
                 IoObjectRegularDirectory) &&
                (DestinationFileObject->Properties.Type !=
                 IoObjectRegularDirectory)) {

                Status = STATUS_NOT_A_DIRECTORY;
                goto RenameEnd;
            }

            //
            // Rename is not allowed when the destination is mounted. It does
            // not matter where.
            //

            if (DestinationPathPoint.PathEntry->MountCount != 0) {
                Status = STATUS_RESOURCE_IN_USE;
                goto RenameEnd;
            }

            IopGetParentPathPoint(NULL,
                                  &DestinationPathPoint,
                                  &DestinationDirectoryPathPoint);

            DestinationDirectoryFileObject =
                           DestinationDirectoryPathPoint.PathEntry->FileObject;

            ASSERT(DestinationPathPoint.PathEntry->FileObject->Device ==
                   DestinationDirectoryPathPoint.PathEntry->FileObject->Device);

            ASSERT(DestinationPathPoint.MountPoint ==
                   DestinationDirectoryPathPoint.MountPoint);

            ASSERT(DestinationFileObject->Properties.DeviceId ==
                   DestinationDirectoryFileObject->Properties.DeviceId);

            //
            // Since there is a destination file, it needs to be deleted.
            // Ensure the caller has that authority.
            //

            Status = IopCheckDeletePermission(FromKernelMode,
                                              &DestinationDirectoryPathPoint,
                                              &DestinationPathPoint);

            if (!KSUCCESS(Status)) {
                goto RenameEnd;
            }
        }

        //
        // The destination directory should not have a path prefix of the
        // source file. Ignore mount points for this check and only look at
        // the path entries.
        //

        if (SourceFileObject->Properties.Type == IoObjectRegularDirectory) {
            DescendantPath = IopIsDescendantPath(
                                      SourcePathPoint.PathEntry,
                                      DestinationDirectoryPathPoint.PathEntry);

            if (DescendantPath != FALSE) {
                Status = STATUS_INVALID_PARAMETER;
                goto RenameEnd;
            }
        }

        //
        // Renames don't work across file systems.
        //

        if (SourcePathPoint.PathEntry->FileObject->Device !=
            DestinationDirectoryPathPoint.PathEntry->FileObject->Device) {

            Status = STATUS_CROSS_DEVICE;
            goto RenameEnd;
        }

        //
        // The object file system does not allow renaming, only devices and
        // volumes can handle it.
        //

        Device = SourcePathPoint.PathEntry->FileObject->Device;
        if ((Device->Header.Type != ObjectDevice) &&
            (Device->Header.Type != ObjectVolume)) {

            Status = STATUS_ACCESS_DENIED;
            goto RenameEnd;
        }

        //
        // Prepare the rename request.
        //

        RenameRequest.Name = DestinationFile;
        RenameRequest.NameSize = DestinationFileSize;
        RenameRequest.DestinationFileUnlinked = FALSE;
        RenameRequest.DestinationDirectorySize = 0;
        RenameRequest.SourceFileProperties = &(SourceFileObject->Properties);
        RenameRequest.SourceDirectoryProperties =
                                      &(SourceDirectoryFileObject->Properties);

        RenameRequest.DestinationFileProperties = NULL;
        RenameRequest.DestinationDirectoryProperties =
                                 &(DestinationDirectoryFileObject->Properties);

        //
        // For a rename operation, the source file, the source directory and
        // the destination directory need to be locked. Additionally, if a
        // destination file exists, it needs to be locked to synchronize the
        // unlink operation and write file properties. The source file is
        // locked to synchronize with file property writes. Because the FAT
        // file system writes properties to the parent directory, file property
        // writes always need to be able to find a valid parent directory.
        // Directories are always locked before files.
        //

        IopAcquireFileObjectLocksExclusive(SourceDirectoryFileObject,
                                           DestinationDirectoryFileObject);

        if (DestinationFileObject != NULL) {
            IopAcquireFileObjectLocksExclusive(SourceFileObject,
                                               DestinationFileObject);

        } else {
            KeAcquireSharedExclusiveLockExclusive(SourceFileObject->Lock);
        }

        LocksHeld = TRUE;

        //
        // If the source file or destination directory have been unlinked, act
        // like the paths were not found. It's okay if the destination
        // directory has no siblings if it's a mount point, as mount points
        // cannot be unlinked without first being unmounted, and some mounts
        // are just floating path entries without siblings.
        //

        if ((SourcePathPoint.PathEntry->SiblingListEntry.Next == NULL) ||
            ((DestinationDirectoryPathPoint.PathEntry->SiblingListEntry.Next ==
              NULL) &&
             (!IO_IS_MOUNT_POINT(&DestinationDirectoryPathPoint)))) {

            Status = STATUS_PATH_NOT_FOUND;
            break;
        }

        //
        // If the source is still there, the source directory better still be
        // there too.
        //

        ASSERT((SourceDirectoryPathPoint.PathEntry->SiblingListEntry.Next !=
                NULL) ||
               (IO_IS_MOUNT_POINT(&SourceDirectoryPathPoint)));

        //
        // If the destination file was present above and is still in the path
        // hierarchy, then the rename can proceed.
        //

        if ((DestinationPathPoint.PathEntry != NULL) &&
            (DestinationPathPoint.PathEntry->SiblingListEntry.Next != NULL)) {

            ASSERT(DestinationFileObject != NULL);
            ASSERT(DestinationPathPoint.PathEntry->Negative == FALSE);
            ASSERT(DestinationFileObject->Properties.HardLinkCount != 0);

            RenameRequest.DestinationFileProperties =
                                          &(DestinationFileObject->Properties);

            Status = STATUS_SUCCESS;
            break;
        }

        //
        // Otherwise, now that the destination directory's lock is held, if
        // there is still no file at the destination, then the rename can
        // proceed.
        //

        Status = IopPathLookup(FromKernelMode,
                               DestinationStartPathPoint,
                               &DestinationDirectoryPathPoint,
                               TRUE,
                               DestinationFile,
                               DestinationFileSize,
                               OPEN_FLAG_NO_MOUNT_POINT,
                               NULL,
                               &FoundPathPoint);

        //
        // If no path is found, then either a negative path entry was found or
        // no file path exists. It is then safe to proceed with the rename.
        //

        if (Status == STATUS_PATH_NOT_FOUND) {

            ASSERT(RenameRequest.DestinationFileProperties == NULL);

            Status = STATUS_SUCCESS;

            ASSERT((FoundPathPoint.PathEntry == NULL) ||
                   (FoundPathPoint.PathEntry->Negative != FALSE));

            //
            // If there's a negative path entry there, unlink it. The reference
            // will be released when the locks can be dropped.
            //

            if (FoundPathPoint.PathEntry != NULL) {
                IopPathUnlink(FoundPathPoint.PathEntry);
            }

            break;

        //
        // For any other error, just break and fail.
        //

        } else if (!KSUCCESS(Status)) {
            break;
        }

        //
        // If a destination file was found, then the rename must loop back and
        // make another attempt. Due to lock ordering it is not possible to
        // simply acquire this entry's lock now. And once the locks are
        // released, no guarantees can be made about the state of the source
        // or directory.
        //

        ASSERT(FoundPathPoint.PathEntry != NULL);
        ASSERT(FoundPathPoint.PathEntry->SiblingListEntry.Next != NULL);

        //
        // A destination entry was found or it was unlinked after the
        // destination directory lock was acquired. The rename needs to be
        // tried again. Release the locks and any references taken.
        //

        KeReleaseSharedExclusiveLockExclusive(SourceFileObject->Lock);
        if (DestinationFileObject != NULL) {
            KeReleaseSharedExclusiveLockExclusive(DestinationFileObject->Lock);
        }

        KeReleaseSharedExclusiveLockExclusive(SourceDirectoryFileObject->Lock);
        if (DestinationDirectoryFileObject != SourceDirectoryFileObject) {
            KeReleaseSharedExclusiveLockExclusive(
                                         DestinationDirectoryFileObject->Lock);
        }

        LocksHeld = FALSE;
        IO_PATH_POINT_RELEASE_REFERENCE(&SourcePathPoint);
        SourcePathPoint.PathEntry = NULL;
        IO_PATH_POINT_RELEASE_REFERENCE(&SourceDirectoryPathPoint);
        SourceDirectoryPathPoint.PathEntry = NULL;
        IO_PATH_POINT_RELEASE_REFERENCE(&DestinationDirectoryPathPoint);
        DestinationDirectoryPathPoint.PathEntry = NULL;
        if (DestinationPathPoint.PathEntry != NULL) {
            IO_PATH_POINT_RELEASE_REFERENCE(&DestinationPathPoint);
            DestinationPathPoint.PathEntry = NULL;
        }

        if (FoundPathPoint.PathEntry != NULL) {
            IO_PATH_POINT_RELEASE_REFERENCE(&FoundPathPoint);
            FoundPathPoint.PathEntry = NULL;
        }

        MmFreePagedPool(DestinationDirectory);
        DestinationDirectory = NULL;
        MmFreePagedPool(DestinationFile);
        DestinationFile = NULL;
        DestinationFileObject = NULL;
        Attempts += 1;
    }

    if (!KSUCCESS(Status)) {
        goto RenameEnd;
    }

    //
    // Check to make sure that the source and destination file objects did not
    // become mount points since the checks above. A path entry's file object's
    // lock is acquired in shared mode when the mount count is increment,
    // synchronizing with the rename call.
    //

    if ((SourcePathPoint.PathEntry->MountCount != 0) ||
        ((DestinationPathPoint.PathEntry != NULL) &&
         (DestinationPathPoint.PathEntry->MountCount != 0))) {

         Status = STATUS_RESOURCE_IN_USE;
         goto RenameEnd;
    }

    Status = IopSendSystemControlIrp(Device,
                                     IrpMinorSystemControlRename,
                                     &RenameRequest);

    //
    // Even if the rename failed, the destination file (if it existed and had
    // not already been unlinked) could have been unlinked. If so, decrement
    // it's hard link count and unlink it from this path tree. This must happen
    // while the locks are held.
    //

    if (RenameRequest.DestinationFileUnlinked != FALSE) {

        ASSERT(DestinationPathPoint.PathEntry != NULL);
        ASSERT(DestinationFileObject != NULL);

        IopFileObjectDecrementHardLinkCount(DestinationFileObject);
        IopPathUnlink(DestinationPathPoint.PathEntry);

    //
    // If there's a negative destination entry, remove it. The rename moved
    // the source onto the destination, in which case the file object
    // pointer is incorrectly null.
    //

    } else if ((DestinationPathPoint.PathEntry != NULL) &&
               (DestinationPathPoint.PathEntry->Negative != FALSE)) {

        IopPathUnlink(DestinationPathPoint.PathEntry);
    }

    //
    // If the source file's hard link count changed, then it could now either
    // be in two directories, or in no directories.
    //

    if (RenameRequest.SourceFileHardLinkDelta != 0) {

        //
        // If the delta is 1, then it got added to the destination directory,
        // but was never deleted from the source. Increment the hard link count.
        //

        if (RenameRequest.SourceFileHardLinkDelta == 1) {
            IopFileObjectIncrementHardLinkCount(SourceFileObject);
            IopUpdateFileObjectTime(DestinationDirectoryFileObject,
                                    FileObjectModifiedTime);

        //
        // Otherwise, the delta is -1. Decrement the hard link count and unlink
        // it from the source path entry. Unfortunately, this rename turned
        // into a delete.
        //

        } else {

            ASSERT(RenameRequest.SourceFileHardLinkDelta == (ULONG)-1);

            IopFileObjectDecrementHardLinkCount(SourceFileObject);
            IopPathUnlink(SourcePathPoint.PathEntry);
            IopUpdateFileObjectTime(SourceDirectoryFileObject,
                                    FileObjectModifiedTime);
        }

    //
    // Rename succeeded.
    //

    } else if (KSUCCESS(Status)) {

        //
        // Create a path entry at the destination to avoid the painful
        // penalty of having to do a file system lookup on this object next
        // time.
        //

        if (SourcePathPoint.PathEntry->DoNotCache == FALSE) {
            NameHash = IopHashPathString(DestinationFile, DestinationFileSize);
            NewPathEntry = IopCreatePathEntry(
                                       DestinationFile,
                                       DestinationFileSize,
                                       NameHash,
                                       DestinationDirectoryPathPoint.PathEntry,
                                       SourceFileObject);

            if (NewPathEntry != NULL) {
                INSERT_BEFORE(
                        &(NewPathEntry->SiblingListEntry),
                        &(DestinationDirectoryPathPoint.PathEntry->ChildList));

                IopFileObjectAddReference(SourceFileObject);
            }
        }

        //
        // Unlink the source file path from its parent so new paths walks will
        // not find it and so that delete will see that it's too late.
        //

        IopPathUnlink(SourcePathPoint.PathEntry);

        //
        // Also update the size of the destination directory.
        //

        IopUpdateFileObjectFileSize(DestinationDirectoryFileObject,
                                    RenameRequest.DestinationDirectorySize);

        IopUpdateFileObjectTime(DestinationDirectoryFileObject,
                                FileObjectModifiedTime);

        IopUpdateFileObjectTime(SourceDirectoryFileObject,
                                FileObjectModifiedTime);
    }

    IopUpdateFileObjectTime(SourceFileObject, FileObjectStatusTime);

RenameEnd:
    if (LocksHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(SourceFileObject->Lock);
        if (DestinationFileObject != NULL) {
            KeReleaseSharedExclusiveLockExclusive(DestinationFileObject->Lock);
        }

        KeReleaseSharedExclusiveLockExclusive(SourceDirectoryFileObject->Lock);
        if (DestinationDirectoryFileObject != SourceDirectoryFileObject) {
            KeReleaseSharedExclusiveLockExclusive(
                                         DestinationDirectoryFileObject->Lock);
        }
    }

    if ((KSUCCESS(Status)) && (SourceFileObject != DestinationFileObject)) {
        IopPathCleanCache(SourcePathPoint.PathEntry);
    }

    if (SourcePathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&SourcePathPoint);
    }

    if (SourceDirectoryPathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&SourceDirectoryPathPoint);
    }

    if (DestinationPathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&DestinationPathPoint);
    }

    if (DestinationDirectoryPathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&DestinationDirectoryPathPoint);
    }

    if (FoundPathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&FoundPathPoint);
    }

    if (NewPathEntry != NULL) {
        IoPathEntryReleaseReference(NewPathEntry);
    }

    if (DestinationDirectory != NULL) {
        MmFreePagedPool(DestinationDirectory);
    }

    if (DestinationFile != NULL) {
        MmFreePagedPool(DestinationFile);
    }

    return Status;
}

KERNEL_API
KSTATUS
IoCreateSymbolicLink (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR LinkName,
    ULONG LinkNameSize,
    PSTR LinkTarget,
    ULONG LinkTargetSize
    )

/*++

Routine Description:

    This routine attempts to create a new symbolic link at the given path.
    The target of the symbolic link is not required to exist. The link path
    must not already exist.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Directory - Supplies an optional pointer to an open handle to a directory
        for relative paths. Supply NULL to use the current working directory.

    LinkName - Supplies a pointer to the path of the new link to create.

    LinkNameSize - Supplies the length of the link name buffer in bytes,
        including the null terminator.

    LinkTarget - Supplies a pointer to the target of the link, the location the
        link points to.

    LinkTargetSize - Supplies the size of the link target buffer in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

{

    UINTN BytesCompleted;
    CREATE_PARAMETERS Create;
    ULONG Flags;
    PIO_HANDLE Handle;
    IO_BUFFER IoBuffer;
    KSTATUS Status;

    Handle = NULL;
    Flags = OPEN_FLAG_CREATE | OPEN_FLAG_FAIL_IF_EXISTS | OPEN_FLAG_TRUNCATE |
            OPEN_FLAG_SYMBOLIC_LINK;

    Create.Type = IoObjectSymbolicLink;
    Create.Context = NULL;
    Create.Permissions = FILE_PERMISSION_ALL;
    Create.Created = FALSE;
    Status = IopOpen(FromKernelMode,
                     Directory,
                     LinkName,
                     LinkNameSize,
                     IO_ACCESS_WRITE,
                     Flags,
                     &Create,
                     &Handle);

    if (!KSUCCESS(Status)) {
        goto CreateSymbolicLinkEnd;
    }

    Status = MmInitializeIoBuffer(&IoBuffer,
                                  LinkTarget,
                                  INVALID_PHYSICAL_ADDRESS,
                                  LinkTargetSize,
                                  IO_BUFFER_FLAG_KERNEL_MODE_DATA);

    if (!KSUCCESS(Status)) {
        goto CreateSymbolicLinkEnd;
    }

    Status = IoWriteAtOffset(Handle,
                             &IoBuffer,
                             0,
                             LinkTargetSize,
                             0,
                             WAIT_TIME_INDEFINITE,
                             &BytesCompleted,
                             NULL);

    if (!KSUCCESS(Status)) {
        goto CreateSymbolicLinkEnd;
    }

CreateSymbolicLinkEnd:
    if (Handle != NULL) {
        IoClose(Handle);
    }

    return Status;
}

KERNEL_API
KSTATUS
IoReadSymbolicLink (
    PIO_HANDLE Handle,
    ULONG AllocationTag,
    PSTR *LinkTarget,
    PULONG LinkTargetSize
    )

/*++

Routine Description:

    This routine reads the destination of a given open symbolic link, and
    returns the information in a newly allocated buffer. It is the caller's
    responsibility to free this memory from paged pool.

Arguments:

    Handle - Supplies the open file handle to the symbolic link itself.

    AllocationTag - Supplies the paged pool tag to use when creating the
        allocation.

    LinkTarget - Supplies a pointer where a newly allocated string will be
        returned on success containing the target the link is pointing at.

    LinkTargetSize - Supplies a pointer where the size of the link target in
        bytes (including the null terminator) will be returned.

Return Value:

    STATUS_SUCCESS if the link target was successfully returned.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_NOT_READY if the contents of the symbolic link are not valid.

    Other status codes on other failures.

--*/

{

    UINTN BytesCompleted;
    FILE_PROPERTIES FileProperties;
    IO_BUFFER IoBuffer;
    ULONGLONG Size;
    KSTATUS Status;
    PSTR TargetBuffer;
    UINTN TargetBufferSize;

    TargetBuffer = NULL;

    //
    // Reading the symbolic link is pretty much just reading the entire
    // contents of the file into paged pool.
    //

    Status = IoGetFileInformation(Handle, &FileProperties);
    if (!KSUCCESS(Status)) {
        goto ReadSymbolicLinkEnd;
    }

    if (FileProperties.Type != IoObjectSymbolicLink) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReadSymbolicLinkEnd;
    }

    Size = FileProperties.Size;
    TargetBufferSize = Size;
    if (Size != TargetBufferSize) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReadSymbolicLinkEnd;
    }

    if (Size == 0) {
        Status = STATUS_NOT_READY;
        goto ReadSymbolicLinkEnd;
    }

    TargetBuffer = MmAllocatePagedPool(TargetBufferSize + 1, AllocationTag);
    if (TargetBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReadSymbolicLinkEnd;
    }

    Status = MmInitializeIoBuffer(&IoBuffer,
                                  TargetBuffer,
                                  INVALID_PHYSICAL_ADDRESS,
                                  TargetBufferSize,
                                  IO_BUFFER_FLAG_KERNEL_MODE_DATA);

    if (!KSUCCESS(Status)) {
        goto ReadSymbolicLinkEnd;
    }

    Status = IoReadAtOffset(Handle,
                            &IoBuffer,
                            0,
                            TargetBufferSize,
                            0,
                            WAIT_TIME_INDEFINITE,
                            &BytesCompleted,
                            NULL);

    if (!KSUCCESS(Status)) {
        goto ReadSymbolicLinkEnd;
    }

    if (BytesCompleted != TargetBufferSize) {
        Status = STATUS_NOT_READY;
        goto ReadSymbolicLinkEnd;
    }

    TargetBuffer[TargetBufferSize] = '\0';
    TargetBufferSize += 1;
    Status = STATUS_SUCCESS;

ReadSymbolicLinkEnd:
    if (!KSUCCESS(Status)) {
        if (TargetBuffer != NULL) {
            MmFreePagedPool(TargetBuffer);
            TargetBuffer = NULL;
        }

        TargetBufferSize = 0;
    }

    *LinkTarget = TargetBuffer;
    *LinkTargetSize = TargetBufferSize;
    return Status;
}

KERNEL_API
KSTATUS
IoUserControl (
    PIO_HANDLE Handle,
    ULONG MinorCode,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    )

/*++

Routine Description:

    This routine performs a user control operation.

Arguments:

    Handle - Supplies the open file handle.

    MinorCode - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    ContextBuffer - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextBufferSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    switch (Handle->FileObject->Properties.Type) {
    case IoObjectBlockDevice:
    case IoObjectCharacterDevice:
        Status = IopSendUserControlIrp(Handle,
                                       MinorCode,
                                       FromKernelMode,
                                       ContextBuffer,
                                       ContextBufferSize);

        break;

    case IoObjectTerminalMaster:
    case IoObjectTerminalSlave:
        Status = IopTerminalUserControl(Handle,
                                        MinorCode,
                                        FromKernelMode,
                                        ContextBuffer,
                                        ContextBufferSize);

        break;

    case IoObjectSocket:
        Status = IoSocketUserControl(Handle,
                                     MinorCode,
                                     FromKernelMode,
                                     ContextBuffer,
                                     ContextBufferSize);

        break;

    case IoObjectSharedMemoryObject:
        Status = IopSharedMemoryUserControl(Handle,
                                            MinorCode,
                                            FromKernelMode,
                                            ContextBuffer,
                                            ContextBufferSize);

        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

KERNEL_API
KSTATUS
IoGetDevice (
    PIO_HANDLE Handle,
    PDEVICE *Device
    )

/*++

Routine Description:

    This routine returns the actual device backing the given I/O object. Not
    all I/O objects are actually backed by a single device. For file and
    directory objects, this routine will return a pointer to the volume.

Arguments:

    Handle - Supplies the open file handle.

    Device - Supplies a pointer where the underlying I/O device will be
        returned.

Return Value:

    Status code.

--*/

{

    PDEVICE FileDevice;
    PFILE_OBJECT FileObject;
    PPAGING_IO_HANDLE PagingIoHandle;

    //
    // For paging I/O handles, this routine is called during page in (so it
    // can't fault). Get the device directly out of the paging I/O handle.
    //

    if (Handle->HandleType == IoHandleTypePaging) {
        PagingIoHandle = (PPAGING_IO_HANDLE)Handle;
        *Device = PagingIoHandle->Device;
        if (PagingIoHandle->Device != NULL) {
            return STATUS_SUCCESS;
        }

        return STATUS_INVALID_CONFIGURATION;
    }

    FileObject = Handle->FileObject;
    FileDevice = FileObject->Device;
    if (IS_DEVICE_OR_VOLUME(FileDevice)) {
        *Device = FileDevice;
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_CONFIGURATION;
}

KERNEL_API
BOOL
IoIsPageFileAccessSupported (
    PIO_HANDLE Handle
    )

/*++

Routine Description:

    This routine determines whether or not page file access is supported on the
    given handle.

Arguments:

    Handle - Supplies a pointer to the I/O handle.

Return Value:

    Returns TRUE if the handle supports page file I/O, or FALSE otherwise.

--*/

{

    if (Handle->HandleType == IoHandleTypePaging) {
        return TRUE;
    }

    return FALSE;
}

KERNEL_API
KSTATUS
IoGetGlobalStatistics (
    PIO_GLOBAL_STATISTICS Statistics
    )

/*++

Routine Description:

    This routine returns a snap of the global I/O statistics counters.

Arguments:

    Statistics - Supplies a pointer to the global I/O statistics.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the version is less than
    IO_GLOBAL_STATISTICS_VERSION.

--*/

{

    if ((Statistics->Version < IO_GLOBAL_STATISTICS_VERSION) ||
        (Statistics->Version > IO_GLOBAL_STATISTICS_MAX_VERSION)) {

        return STATUS_INVALID_PARAMETER;
    }

    Statistics->BytesRead = RtlAtomicOr64(&(IoGlobalStatistics.BytesRead), 0);
    Statistics->BytesWritten = RtlAtomicOr64(&(IoGlobalStatistics.BytesWritten),
                                             0);

    Statistics->PagingBytesRead =
                       RtlAtomicOr64(&(IoGlobalStatistics.PagingBytesRead), 0);

    Statistics->PagingBytesWritten =
                    RtlAtomicOr64(&(IoGlobalStatistics.PagingBytesWritten), 0);

    return STATUS_SUCCESS;
}

KSTATUS
IoNotifyFileMapping (
    PIO_HANDLE Handle,
    BOOL Mapping
    )

/*++

Routine Description:

    This routine is called to notify a file object that it is being mapped
    into memory or unmapped.

Arguments:

    Handle - Supplies the handle being mapped.

    Mapping - Supplies a boolean indicating if a new mapping is being created
        (TRUE) or an old mapping is being destroyed (FALSE).

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    KSTATUS Status;

    FileObject = Handle->FileObject;
    switch (FileObject->Properties.Type) {
    case IoObjectSharedMemoryObject:
        Status = IopSharedMemoryNotifyFileMapping(FileObject, Mapping);
        break;

    default:
        Status = STATUS_SUCCESS;
        break;
    }

    return Status;
}

KSTATUS
IoOpenPageFile (
    PCSTR Path,
    ULONG PathSize,
    ULONG Access,
    ULONG Flags,
    PIO_HANDLE *Handle,
    PULONGLONG FileSize
    )

/*++

Routine Description:

    This routine opens a page file. This routine is to be used only
    internally by MM.

Arguments:

    Path - Supplies a pointer to the string containing the file path to open.

    PathSize - Supplies the length of the path buffer in bytes, including
        the null terminator.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

    FileSize - Supplies a pointer where the file size in bytes will be returned
        on success.

Return Value:

    Status code.

--*/

{

    PDEVICE Device;
    PFILE_OBJECT FileObject;
    PIO_HANDLE IoHandle;
    ULONGLONG LocalFileSize;
    PPAGING_IO_HANDLE NewHandle;
    KSTATUS Status;

    *FileSize = 0;
    *Handle = NULL;
    IoHandle = NULL;
    NewHandle = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Allocate the basic structure.
    //

    NewHandle = MmAllocateNonPagedPool(sizeof(PAGING_IO_HANDLE),
                                       IO_ALLOCATION_TAG);

    if (NewHandle == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenPageFileEnd;
    }

    RtlZeroMemory(NewHandle, sizeof(PAGING_IO_HANDLE));
    NewHandle->HandleType = IoHandleTypePaging;

    //
    // Open the file normally, but with the page file and non-cached flags set.
    //

    Flags |= OPEN_FLAG_PAGE_FILE | OPEN_FLAG_NO_PAGE_CACHE;
    Status = IopOpen(TRUE,
                     NULL,
                     Path,
                     PathSize,
                     Access,
                     Flags,
                     NULL,
                     &IoHandle);

    if (!KSUCCESS(Status)) {
        goto OpenPageFileEnd;
    }

    //
    // Even if a page file exists on this device, it might not be intended for
    // use on this sytem. If the device is not an intended paging device, then
    // return failure.
    //

    FileObject = IoHandle->FileObject;
    Device = FileObject->Device;
    if (!IS_DEVICE_OR_VOLUME(Device)) {
        Status = STATUS_NOT_SUPPORTED;
        goto OpenPageFileEnd;
    }

    if ((Device->Flags & DEVICE_FLAG_PAGING_DEVICE) == 0) {
        Status = STATUS_NO_SUCH_FILE;
        goto OpenPageFileEnd;
    }

    NewHandle->DeviceContext = IoHandle->DeviceContext;
    NewHandle->Device = Device;
    LocalFileSize = FileObject->Properties.Size;
    NewHandle->Capacity = LocalFileSize;
    NewHandle->IoHandle = IoHandle;
    NewHandle->OffsetAlignment = FileObject->Properties.BlockSize;
    NewHandle->SizeAlignment = FileObject->Properties.BlockSize;
    *FileSize = NewHandle->Capacity;
    Status = STATUS_SUCCESS;

OpenPageFileEnd:
    if (!KSUCCESS(Status)) {
        if (IoHandle != NULL) {
            IoClose(IoHandle);
        }

        if (NewHandle != NULL) {
            MmFreeNonPagedPool(NewHandle);
        }

        NewHandle = NULL;
    }

    *Handle = (PIO_HANDLE)NewHandle;
    return Status;
}

KSTATUS
IopOpen (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG Access,
    ULONG Flags,
    PCREATE_PARAMETERS Create,
    PIO_HANDLE *Handle
    )

/*++

Routine Description:

    This routine opens a file, device, pipe, or other I/O object.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Directory - Supplies an optional pointer to a handle to a directory to use
        if the given path is relative. Supply NULL to use the current working
        directory.

    Path - Supplies a pointer to the path to open.

    PathLength - Supplies the length of the path buffer in bytes, including the
        null terminator.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Create - Supplies an optional pointer to the creation information. If the
        OPEN_FLAG_CREATE is supplied in the flags, then this field is required.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT DirectoryFileObject;
    PPATH_POINT DirectoryPathPoint;
    PFILE_OBJECT FileObject;
    PATH_POINT PathPoint;
    PKPROCESS Process;
    KSTATUS Status;

    DirectoryPathPoint = NULL;
    PathPoint.PathEntry = NULL;
    PathPoint.MountPoint = NULL;

    //
    // If the caller specified a directory, validate that it is a directory,
    // and perform permission checking if search permissions were not granted
    // upon open.
    //

    if (Directory != NULL) {
        DirectoryFileObject = Directory->FileObject;
        DirectoryPathPoint = &(Directory->PathPoint);
        if (DirectoryFileObject->Properties.Type != IoObjectRegularDirectory) {
            Status = STATUS_NOT_A_DIRECTORY;
            goto OpenEnd;
        }

        ASSERT(DirectoryFileObject ==
               DirectoryPathPoint->PathEntry->FileObject);
    }

    //
    // Apply the umask.
    //

    if ((Flags & OPEN_FLAG_CREATE) != 0) {
        if (Create == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto OpenEnd;
        }

        Process = PsGetCurrentProcess();
        Create->Permissions &= ~(Process->Umask);

        //
        // Change the override if the create flag is on.
        //

        if ((Flags & OPEN_FLAG_DIRECTORY) != 0) {

            ASSERT(Create->Type == IoObjectInvalid);

            Create->Type = IoObjectRegularDirectory;

        } else if ((Flags & OPEN_FLAG_SHARED_MEMORY) != 0) {

            ASSERT(Create->Type == IoObjectInvalid);

            Create->Type = IoObjectSharedMemoryObject;

        } else if (Create->Type == IoObjectInvalid) {
            Create->Type = IoObjectRegularFile;
        }

    } else {
        Create = NULL;
    }

    //
    // If there is no path, create an anonymous object.
    //

    if (PathLength == 0) {

        ASSERT((Flags & OPEN_FLAG_CREATE) != 0);

        Status = IopCreateAnonymousObject(FromKernelMode,
                                          Access,
                                          Flags,
                                          Create,
                                          &PathPoint);

    //
    // There is a path, so walk it to create or open your destiny.
    //

    } else {
        Status = IopPathWalk(FromKernelMode,
                             DirectoryPathPoint,
                             &Path,
                             &PathLength,
                             Flags,
                             Create,
                             &PathPoint);
    }

    if (!KSUCCESS(Status)) {
        goto OpenEnd;
    }

    //
    // Check the directory flag against the type.
    //

    FileObject = PathPoint.PathEntry->FileObject;

    //
    // If the directory flag is set, the resulting file object is required to
    // be a directory.
    //

    if ((Flags & OPEN_FLAG_DIRECTORY) != 0) {
        if ((FileObject->Properties.Type != IoObjectRegularDirectory) &&
            (FileObject->Properties.Type != IoObjectObjectDirectory)) {

            Status = STATUS_NOT_A_DIRECTORY;
            goto OpenEnd;
        }

    //
    // Sockets can only be opened if they're being created or just opened for
    // information.
    //

    } else if (FileObject->Properties.Type == IoObjectSocket) {
        if (((Create == NULL) && (Access != 0)) ||
            ((Create != NULL) && (Create->Created == FALSE))) {

            Status = STATUS_NO_SUCH_DEVICE_OR_ADDRESS;
            goto OpenEnd;
        }

    //
    // If the directory flag is not set, then check the override against the
    // object.
    //

    } else {

        //
        // If the object is a directory, then fail if either an override was
        // specified (meaning a create is trying to occur) or the open is for
        // anything other than read. Turns out opening a directory for read is
        // allowed, it's just that no I/O can be performed on it.
        //

        if ((FileObject->Properties.Type == IoObjectRegularDirectory) ||
            (FileObject->Properties.Type == IoObjectObjectDirectory)) {

            if (((Access & (IO_ACCESS_WRITE | IO_ACCESS_EXECUTE)) != 0) ||
                (Create != NULL)) {

                if ((Create != NULL) &&
                    (Create->Type == IoObjectSymbolicLink)) {

                    Status = STATUS_FILE_EXISTS;

                } else {
                    Status = STATUS_FILE_IS_DIRECTORY;
                }

                goto OpenEnd;
            }
        }
    }

    //
    // Check permissions on path entry. If this call successfully created the
    // object, then open it no matter what. This supports calls like creating
    // a file with read/write access on that file but fewer permissions in the
    // create mask.
    //

    if (FromKernelMode == FALSE) {
        if ((Create == NULL) || (Create->Created == FALSE)) {
            Status = IopCheckPermissions(FromKernelMode, &PathPoint, Access);
            if (!KSUCCESS(Status)) {
                goto OpenEnd;
            }
        }
    }

    //
    // Open the path point, which upon success takes another reference on the
    // path point.
    //

    Status = IopOpenPathPoint(&PathPoint, Access, Flags, Handle);
    if (!KSUCCESS(Status)) {
        goto OpenEnd;
    }

    Status = STATUS_SUCCESS;

OpenEnd:

    //
    // Do not use the path point release reference macro here, the mount point
    // may be null if an anonymous object was created.
    //

    if (PathPoint.PathEntry != NULL) {
        IoPathEntryReleaseReference(PathPoint.PathEntry);
        if (PathPoint.MountPoint != NULL) {
            IoMountPointReleaseReference(PathPoint.MountPoint);
        }
    }

    return Status;
}

KSTATUS
IopOpenPathPoint (
    PPATH_POINT PathPoint,
    ULONG Access,
    ULONG Flags,
    PIO_HANDLE *Handle
    )

/*++

Routine Description:

    This routine opens a path entry object. This routine must be called
    carefully by internal functions, as it skips all permission checks.

Arguments:

    PathPoint - Supplies a pointer to the path point to open. Upon success this
        routine will add a reference to the path point's path entry and mount
        point.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

{

    IRP_CLOSE CloseIrp;
    PDEVICE Device;
    PFILE_OBJECT FileObject;
    PIO_HANDLE NewHandle;
    PVOID OldDeviceContext;
    ULONG OldFileObjectFlags;
    IRP_OPEN OpenIrp;
    BOOL OpenIrpSent;
    KSTATUS Status;

    Device = NULL;
    NewHandle = NULL;
    OpenIrpSent = FALSE;

    //
    // Create an I/O handle.
    //

    Status = IopCreateIoHandle(&NewHandle);
    if (!KSUCCESS(Status)) {
        goto OpenPathEntryEnd;
    }

    IO_COPY_PATH_POINT(&(NewHandle->PathPoint), PathPoint);
    NewHandle->OpenFlags = Flags;
    NewHandle->Access = Access;
    FileObject = PathPoint->PathEntry->FileObject;
    NewHandle->FileObject = FileObject;
    switch (FileObject->Properties.Type) {
    case IoObjectRegularFile:
    case IoObjectSymbolicLink:
    case IoObjectBlockDevice:
        RtlZeroMemory(&OpenIrp, sizeof(IRP_OPEN));
        OpenIrp.FileProperties = &(FileObject->Properties);
        OpenIrp.IoState = FileObject->IoState;
        Device = FileObject->Device;

        ASSERT(IS_DEVICE_OR_VOLUME(Device));

        //
        // If the file object is cacheable and has not been opened, call the
        // driver to get a context with full access.
        //

        if ((IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE) &&
            ((FileObject->Flags & FILE_OBJECT_FLAG_OPEN) == 0)) {

            OpenIrp.DesiredAccess = IO_ACCESS_READ | IO_ACCESS_WRITE;
            OpenIrp.OpenFlags = Flags;
            OpenIrp.IoHandle = NewHandle;
            Status = IopSendOpenIrp(Device, &OpenIrp);
            if (!KSUCCESS(Status)) {
                goto OpenPathEntryEnd;
            }

            //
            // Now try to insert the device context into the file object. First
            // exchange the device context pointer. It is not safe to mark it
            // open until the context is set.
            //

            OldDeviceContext = (PVOID)RtlAtomicCompareExchange(
                                         (PVOID)&(FileObject->DeviceContext),
                                         (UINTN)OpenIrp.DeviceContext,
                                         (UINTN)NULL);

            //
            // If the old context was NULL, then this caller might have won the
            // race to set it. That said, some devices return a NULL context.
            // So additionally try to set the open status. If this race is lost
            // then send the close IRP. The other open won.
            //

            if (OldDeviceContext == NULL) {
                OldFileObjectFlags = RtlAtomicOr32(&(FileObject->Flags),
                                                   FILE_OBJECT_FLAG_OPEN);

                if ((OldFileObjectFlags & FILE_OBJECT_FLAG_OPEN) != 0) {
                    CloseIrp.DeviceContext = OpenIrp.DeviceContext;
                    IopSendCloseIrp(Device, &CloseIrp);
                }

            //
            // Otherwise, this caller lost the race. It should destroy its
            // context before continuing. It is not safe, however, to assert
            // that the file object is open. The winner of the context race may
            // not have set the open flag yet.
            //

            } else {
                CloseIrp.DeviceContext = OpenIrp.DeviceContext;
                IopSendCloseIrp(Device, &CloseIrp);
            }
        }

        //
        // If the file object is going to be used in the paging path or is not
        // cacheable, open up a device context that will be stored in the I/O
        // handle.
        //

        if ((IO_IS_FILE_OBJECT_CACHEABLE(FileObject) == FALSE) ||
            ((Flags & OPEN_FLAG_PAGE_FILE) != 0) ||
            ((Flags & OPEN_FLAG_PAGING_DEVICE) != 0)) {

            OpenIrp.DesiredAccess = Access;
            OpenIrp.OpenFlags = Flags;
            OpenIrp.IoHandle = NewHandle;
            Status = IopSendOpenIrp(Device, &OpenIrp);
            if (!KSUCCESS(Status)) {
                goto OpenPathEntryEnd;
            }

            OpenIrpSent = TRUE;
            NewHandle->DeviceContext = OpenIrp.DeviceContext;
        }

        //
        // If the caller requested a truncate operation and it is allowed on
        // this object type, modify the file object's size.
        //

        if (((Flags & OPEN_FLAG_TRUNCATE) != 0) &&
            ((Flags & OPEN_FLAG_PAGE_FILE) == 0)) {

            Status = IopModifyFileObjectSize(FileObject,
                                             NewHandle->DeviceContext,
                                             0);

            if (!KSUCCESS(Status)) {
                goto OpenPathEntryEnd;
            }
        }

        Status = STATUS_SUCCESS;
        break;

    case IoObjectCharacterDevice:
    case IoObjectRegularDirectory:
        RtlZeroMemory(&OpenIrp, sizeof(IRP_OPEN));
        OpenIrp.FileProperties = &(FileObject->Properties);
        OpenIrp.IoState = FileObject->IoState;
        OpenIrp.DesiredAccess = Access;
        OpenIrp.OpenFlags = Flags;
        OpenIrp.IoHandle = NewHandle;
        Device = FileObject->Device;

        ASSERT(IS_DEVICE_OR_VOLUME(Device));

        Status = IopSendOpenIrp(Device, &OpenIrp);
        if (!KSUCCESS(Status)) {
            goto OpenPathEntryEnd;
        }

        OpenIrpSent = TRUE;
        NewHandle->DeviceContext = OpenIrp.DeviceContext;
        break;

    case IoObjectPipe:
        Status = IopOpenPipe(NewHandle);
        break;

    //
    // Object directories don't need anything to be opened.
    //

    case IoObjectObjectDirectory:
        Status = STATUS_SUCCESS;
        break;

    case IoObjectSocket:
        Status = IopOpenSocket(NewHandle);
        break;

    case IoObjectTerminalMaster:
        Status = IopTerminalOpenMaster(NewHandle);
        break;

    case IoObjectTerminalSlave:
        Status = IopTerminalOpenSlave(NewHandle);
        break;

    case IoObjectSharedMemoryObject:
        if ((Flags & OPEN_FLAG_TRUNCATE) != 0) {
            Status = IopModifyFileObjectSize(FileObject, NULL, 0);
            if (!KSUCCESS(Status)) {
                goto OpenPathEntryEnd;
            }
        }

        Status = STATUS_SUCCESS;
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_CONFIGURATION;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto OpenPathEntryEnd;
    }

    //
    // Do not use the default path point add reference macro. An anonymous
    // object does not have a mount point.
    //

    IoPathEntryAddReference(PathPoint->PathEntry);
    if (PathPoint->MountPoint != NULL) {
        IoMountPointAddReference(PathPoint->MountPoint);
    }

    Status = STATUS_SUCCESS;

OpenPathEntryEnd:
    if (!KSUCCESS(Status)) {
        if (OpenIrpSent != FALSE) {
            CloseIrp.DeviceContext = NewHandle->DeviceContext;
            IopSendCloseIrp(Device, &CloseIrp);
        }

        if (NewHandle != NULL) {
            NewHandle->PathPoint.PathEntry = NULL;
            IoIoHandleReleaseReference(NewHandle);
            NewHandle = NULL;
        }
    }

    ASSERT((NewHandle == NULL) || (NewHandle->PathPoint.PathEntry != NULL));

    *Handle = NewHandle;
    return Status;
}

KSTATUS
IopOpenDevice (
    PDEVICE Device,
    ULONG Access,
    ULONG Flags,
    PIO_HANDLE *Handle
    )

/*++

Routine Description:

    This routine opens a device or volume.

Arguments:

    Device - Supplies a pointer to a device to open.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

{

    PIO_HANDLE NewHandle;
    PCHAR ObjectPath;
    KSTATUS Status;

    ASSERT((Device->Header.Type == ObjectDevice) ||
           (Device->Header.Type == ObjectVolume));

    NewHandle = NULL;
    ObjectPath = ObGetFullPath(Device, DEVICE_ALLOCATION_TAG);
    if (ObjectPath == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenDeviceEnd;
    }

    //
    // Open the device from kernel mode.
    //

    Status = IopOpen(TRUE,
                     NULL,
                     ObjectPath,
                     RtlStringLength(ObjectPath) + 1,
                     Access,
                     Flags,
                     NULL,
                     &NewHandle);

    if (!KSUCCESS(Status)) {
        goto OpenDeviceEnd;
    }

OpenDeviceEnd:
    if (ObjectPath != NULL) {
        MmFreePagedPool(ObjectPath);
    }

    *Handle = NewHandle;
    return Status;
}

KSTATUS
IopCreateSpecialIoObject (
    BOOL FromKernelMode,
    ULONG Flags,
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    )

/*++

Routine Description:

    This routine creates a special file object.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where a pointer to the new file object
        will be returned on success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    ASSERT(Create != NULL);

    switch (Create->Type) {
    case IoObjectPipe:
        Status = IopCreatePipe(NULL, 0, Create, FileObject);
        break;

    case IoObjectSocket:
        Status = IopCreateSocket(Create, FileObject);
        break;

    case IoObjectTerminalMaster:
    case IoObjectTerminalSlave:
        Status = IopCreateTerminal(Create, FileObject);
        break;

    case IoObjectSharedMemoryObject:
        Status = IopCreateSharedMemoryObject(FromKernelMode,
                                             NULL,
                                             0,
                                             Flags,
                                             Create,
                                             FileObject);

        break;

    default:

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    return Status;
}

KSTATUS
IopClose (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine shuts down an I/O handle that is about to be destroyed.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle returned when the file was
        opened.

Return Value:

    Status code.

--*/

{

    IRP_CLOSE CloseIrp;
    PDEVICE Device;
    PFILE_OBJECT FileObject;
    KSTATUS Status;

    FileObject = NULL;
    if (IoHandle->PathPoint.PathEntry != NULL) {
        FileObject = IoHandle->FileObject;
        switch (FileObject->Properties.Type) {
        case IoObjectRegularFile:
        case IoObjectRegularDirectory:
        case IoObjectSymbolicLink:
        case IoObjectBlockDevice:
        case IoObjectCharacterDevice:

            //
            // If the handle received a device context on open, close it.
            //

            if ((IO_IS_FILE_OBJECT_CACHEABLE(FileObject) == FALSE) ||
                ((IoHandle->OpenFlags & OPEN_FLAG_PAGE_FILE) != 0) ||
                ((IoHandle->OpenFlags & OPEN_FLAG_PAGING_DEVICE) != 0)) {

                CloseIrp.DeviceContext = IoHandle->DeviceContext;
                Device = FileObject->Device;

                ASSERT(IS_DEVICE_OR_VOLUME(Device));

                Status = IopSendCloseIrp(Device, &CloseIrp);

            //
            // Otherwise, just report success.
            //

            } else {
                Status = STATUS_SUCCESS;
            }

            break;

        case IoObjectPipe:
            Status = IopClosePipe(IoHandle);
            break;

        case IoObjectSocket:
            Status = IopCloseSocket(IoHandle);
            break;

        case IoObjectTerminalMaster:
            Status = IopTerminalCloseMaster(IoHandle);
            break;

        case IoObjectTerminalSlave:
            Status = IopTerminalCloseSlave(IoHandle);
            break;

        default:
            Status = STATUS_SUCCESS;
            break;
        }

        if (!KSUCCESS(Status)) {
            goto CloseEnd;
        }
    }

    //
    // Clear the asynchronous receiver information from this handle.
    //

    if (IoHandle->Async != NULL) {
        IoSetHandleAsynchronous(IoHandle, 0, FALSE);
        MmFreePagedPool(IoHandle->Async);
        IoHandle->Async = NULL;
    }

    //
    // Let go of the path point, and slide gently into the night. Be careful,
    // as anonymous objects do not have a mount point. Also handles that failed
    // to open do not have a path entry.
    //

    if (IoHandle->PathPoint.PathEntry != NULL) {

        //
        // If the file object in the handle is not the same as the one in the
        // path entry, release the reference on the one in the handle.
        //

        if (FileObject != IoHandle->FileObject) {
            IopFileObjectReleaseReference(IoHandle->FileObject);
        }

        IoPathEntryReleaseReference(IoHandle->PathPoint.PathEntry);
        if (IoHandle->PathPoint.MountPoint != NULL) {
            IoMountPointReleaseReference(IoHandle->PathPoint.MountPoint);
        }
    }

    Status = STATUS_SUCCESS;

CloseEnd:
    return Status;
}

KSTATUS
IopDeleteByHandle (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    ULONG Flags
    )

/*++

Routine Description:

    This routine attempts to delete the the object open by the given I/O
    handle. This does not close or invalidate the handle, but it does attempt
    to unlink the object so future path walks will not find it at that location.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Handle - Supplies the open handle to the device.

    Flags - Supplies a bitfield of flags. See DELETE_FLAG_* definitions.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Fail for anonymous path entries.
    //

    if (Handle->PathPoint.PathEntry->NameSize == 0) {
        return STATUS_PATH_NOT_FOUND;
    }

    Status = IopDeletePathPoint(FromKernelMode, &(Handle->PathPoint), Flags);
    return Status;
}

KSTATUS
IopDeletePathPoint (
    BOOL FromKernelMode,
    PPATH_POINT PathPoint,
    ULONG Flags
    )

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path point is
    a file object or shared memory object, its hard link count is decremented.
    If the hard link count reaches zero and no processes have the object open,
    the contents of the object are destroyed. If processes have open handles to
    the object, the destruction of the object contents are deferred until the
    last handle to the old file is closed.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    PathPoint - Supplies a pointer to the path point to delete. The caller
        should already have a reference on this path point, which will need to
        be released by the caller when finished.

    Flags - Supplies a bitfield of flags. See DELETE_FLAG_* definitions.

Return Value:

    Status code.

--*/

{

    PDEVICE Device;
    PFILE_OBJECT DirectoryFileObject;
    PFILE_OBJECT FileObject;
    BOOL LocksHeld;
    PATH_POINT ParentPathPoint;
    BOOL SendUnlinkRequest;
    KSTATUS Status;
    BOOL Unlinked;

    LocksHeld = FALSE;
    ParentPathPoint.PathEntry = NULL;

    //
    // Delete is not allowed if the path entry is mounted. Doesn't matter
    // where.
    //

    if (PathPoint->PathEntry->MountCount != 0) {
        Status = STATUS_RESOURCE_IN_USE;
        goto DeletePathPointEnd;
    }

    //
    // Get the file object for the file to delete, and the path point for the
    // containing directory.
    //

    FileObject = PathPoint->PathEntry->FileObject;
    IopGetParentPathPoint(NULL,
                          PathPoint,
                          &ParentPathPoint);

    ASSERT(PathPoint->MountPoint == ParentPathPoint.MountPoint);

    //
    // Perform permission checking on the directory in preparation for the
    // directory write operation.
    //

    if (FromKernelMode == FALSE) {
        Status = IopCheckDeletePermission(FromKernelMode,
                                          &ParentPathPoint,
                                          PathPoint);

        if (!KSUCCESS(Status)) {
            goto DeletePathPointEnd;
        }
    }

    //
    // The root object cannot be deleted. This is detected by the parent
    // equaling the child.
    //

    if (IO_ARE_PATH_POINTS_EQUAL(PathPoint, &ParentPathPoint) != FALSE) {
        Status = STATUS_NOT_SUPPORTED;
        goto DeletePathPointEnd;
    }

    //
    // Square up with the directory flag.
    //

    if ((Flags & DELETE_FLAG_DIRECTORY) != 0) {
        if (FileObject->Properties.Type != IoObjectRegularDirectory) {
            Status = STATUS_NOT_A_DIRECTORY;
            goto DeletePathPointEnd;
        }

    } else {
        if (FileObject->Properties.Type == IoObjectRegularDirectory) {
            Status = STATUS_FILE_IS_DIRECTORY;
            goto DeletePathPointEnd;
        }
    }

    //
    // The object file system only allows kernel mode to unlink pipes and
    // terminals. Shared memory objects can be unlinked by both kernel and user
    // mode.
    //

    Device = PathPoint->PathEntry->FileObject->Device;
    SendUnlinkRequest = FALSE;
    if (Device == ObGetRootObject()) {
        if ((FileObject->Properties.Type != IoObjectSharedMemoryObject) &&
            ((FromKernelMode == FALSE) ||
             ((FileObject->Properties.Type != IoObjectTerminalMaster) &&
              (FileObject->Properties.Type != IoObjectTerminalSlave) &&
              (FileObject->Properties.Type != IoObjectPipe)))) {

            Status = STATUS_ACCESS_DENIED;
            goto DeletePathPointEnd;
        }

    //
    // Otherwise deletes can only be from devices or volumes.
    //

    } else {
        if ((Device->Header.Type != ObjectDevice) &&
            (Device->Header.Type != ObjectVolume)) {

            Status = STATUS_ACCESS_DENIED;
            goto DeletePathPointEnd;
        }

        SendUnlinkRequest = TRUE;
    }

    DirectoryFileObject = ParentPathPoint.PathEntry->FileObject;

    //
    // The unlink operation needs to modify the parent directory and the file
    // properties of the child. Hold both locks exclusively. Directories are
    // always acquired first.
    //

    ASSERT(DirectoryFileObject != FileObject);

    KeAcquireSharedExclusiveLockExclusive(DirectoryFileObject->Lock);
    KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
    LocksHeld = TRUE;

    //
    // With the appropriate locks acquired, check to make sure the file can
    // still be unlinked. If it cannot, act like it was not found.
    //

    if (PathPoint->PathEntry->SiblingListEntry.Next == NULL) {
        Status = STATUS_PATH_NOT_FOUND;
        goto DeletePathPointEnd;
    }

    //
    // Check again to make sure that the path entry did not get mounted on.
    // Mount creation synchronizes with the path entry's file object lock.
    //

    if (PathPoint->PathEntry->MountCount != 0) {
        Status = STATUS_RESOURCE_IN_USE;
        goto DeletePathPointEnd;
    }

    ASSERT(FileObject->Properties.HardLinkCount != 0);

    //
    // If unlink request needs to be sent to a driver, then send it now.
    //

    if (SendUnlinkRequest != FALSE) {
        Status = IopSendUnlinkRequest(Device,
                                      FileObject,
                                      DirectoryFileObject,
                                      PathPoint->PathEntry->Name,
                                      PathPoint->PathEntry->NameSize,
                                      &Unlinked);

    //
    // Otherwise just handle the unlink by calling the type specific unlink
    // routine, decrementing the object's hard link count and updating the
    // directory's access time.
    //

    } else {
        if (FileObject->Properties.Type == IoObjectSharedMemoryObject) {
            Status = IopUnlinkSharedMemoryObject(FileObject, &Unlinked);

        } else if (FileObject->Properties.Type == IoObjectPipe) {
            Status = IopUnlinkPipe(FileObject, &Unlinked);

        } else {

            ASSERT((FileObject->Properties.Type == IoObjectTerminalMaster) ||
                   (FileObject->Properties.Type == IoObjectTerminalSlave));

            Status = IopUnlinkTerminal(FileObject, &Unlinked);
        }

        if (Unlinked != FALSE) {
            IopFileObjectDecrementHardLinkCount(FileObject);
            IopUpdateFileObjectTime(DirectoryFileObject,
                                    FileObjectModifiedTime);
        }
    }

    //
    // If the object was successfully unlinked, finish the job even if the call
    // failed. Unlink the path entry from the system's path hierarchy. This
    // needs to be done while the parent's file object I/O lock is held
    // exclusively.
    //

    if (Unlinked != FALSE) {
        IopPathUnlink(PathPoint->PathEntry);
    }

    KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
    KeReleaseSharedExclusiveLockExclusive(DirectoryFileObject->Lock);
    LocksHeld = FALSE;

    //
    // Clean the cached path entries if the path point was successfully
    // unlinked from its parent. The only things that should be there now are
    // negative path entries with a reference count of zero.
    //

    if (Unlinked != FALSE) {
        IopPathCleanCache(PathPoint->PathEntry);
    }

    if (!KSUCCESS(Status)) {
        goto DeletePathPointEnd;
    }

DeletePathPointEnd:
    if (LocksHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
        KeReleaseSharedExclusiveLockExclusive(DirectoryFileObject->Lock);
    }

    if (ParentPathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&ParentPathPoint);
    }

    return Status;
}

KSTATUS
IopSendFileOperationIrp (
    IRP_MINOR_CODE MinorCode,
    PFILE_OBJECT FileObject,
    PVOID DeviceContext,
    ULONG Flags
    )

/*++

Routine Description:

    This routine sends a file operation IRP.

Arguments:

    MinorCode - Supplies the minor code of the IRP to send.

    FileObject - Supplies a pointer to the file object of the file being
        operated on.

    DeviceContext - Supplies a pointer to the device context to send down.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    PDEVICE Device;
    SYSTEM_CONTROL_FILE_OPERATION Request;
    KSTATUS Status;

    if ((FileObject->Properties.Type != IoObjectRegularFile) &&
        (FileObject->Properties.Type != IoObjectRegularDirectory) &&
        (FileObject->Properties.Type != IoObjectSymbolicLink) &&
        (FileObject->Properties.Type != IoObjectBlockDevice) &&
        (FileObject->Properties.Type != IoObjectCharacterDevice)) {

        return STATUS_SUCCESS;
    }

    Request.FileProperties = &(FileObject->Properties);
    Request.DeviceContext = DeviceContext;
    Request.Flags = Flags;
    Device = FileObject->Device;

    ASSERT(IS_DEVICE_OR_VOLUME(Device));

    Status = IopSendSystemControlIrp(Device, MinorCode, &Request);
    return Status;
}

KSTATUS
IopSendLookupRequest (
    PDEVICE Device,
    PFILE_OBJECT Directory,
    PCSTR FileName,
    ULONG FileNameSize,
    PFILE_PROPERTIES Properties,
    PULONG Flags,
    PULONG MapFlags
    )

/*++

Routine Description:

    This routine sends a lookup request IRP. This routine assumes that the
    directory's lock is held exclusively.

Arguments:

    Device - Supplies a pointer to the device to send the request to.

    Directory - Supplies a pointer to the file object of the directory to
        search in.

    FileName - Supplies a pointer to the name of the file, which may not be
        null terminated.

    FileNameSize - Supplies the size of the file name buffer including space
        for a null terminator (which may be a null terminator or may be a
        garbage character). Supply 0 to perform a root lookup request.

    Properties - Supplies a pointer where the file properties will be returned
        if the file was found.

    Flags - Supplies a pointer where the translated file object flags will be
        returned. See FILE_OBJECT_FLAG_* definitions.

    MapFlags - Supplies a pointer where the required map flags associated with
        this file object will be returned. See MAP_FLAG_* definitions.

Return Value:

    Status code.

--*/

{

    SYSTEM_CONTROL_LOOKUP Request;
    KSTATUS Status;

    RtlZeroMemory(Properties, sizeof(FILE_PROPERTIES));
    Request.Root = FALSE;
    if (FileNameSize == 0) {
        Request.Root = TRUE;

        ASSERT(Directory == NULL);
    }

    Request.Flags = 0;
    Request.MapFlags = 0;
    Request.DirectoryProperties = NULL;
    if (Directory != NULL) {

        ASSERT(KeIsSharedExclusiveLockHeldExclusive(Directory->Lock) != FALSE);
        ASSERT(Directory->Properties.HardLinkCount != 0);
        ASSERT(FileNameSize != 0);

        Request.DirectoryProperties = &(Directory->Properties);
    }

    Request.FileName = FileName;
    Request.FileNameSize = FileNameSize;
    Request.Properties = Properties;
    Status = IopSendSystemControlIrp(Device,
                                     IrpMinorSystemControlLookup,
                                     &Request);

    *Flags = 0;
    if ((Request.Flags & LOOKUP_FLAG_NO_PAGE_CACHE) != 0) {
        *Flags |= FILE_OBJECT_FLAG_NO_PAGE_CACHE;
    }

    if ((Request.Flags & LOOKUP_FLAG_NON_PAGED_IO_STATE) != 0) {
        *Flags |= FILE_OBJECT_FLAG_NON_PAGED_IO_STATE;
    }

    *MapFlags = Request.MapFlags;
    return Status;
}

KSTATUS
IopSendCreateRequest (
    PDEVICE Device,
    PFILE_OBJECT Directory,
    PCSTR Name,
    ULONG NameSize,
    PFILE_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine sends a creation request to the device. This routine assumes
    that the directory's lock is held exclusively.

Arguments:

    Device - Supplies a pointer to the device to send the request to.

    Directory - Supplies a pointer to the file object of the directory to
        create the file in.

    Name - Supplies a pointer to the name of the file or directory to create,
        which may not be null terminated.

    NameSize - Supplies the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a  garbage
        character).

    Properties - Supplies a pointer to the file properties of the created file
        on success. The permissions, object type, user ID, group ID, and access
        times are all valid from the system.

Return Value:

    Status code.

--*/

{

    SYSTEM_CONTROL_CREATE Request;
    KSTATUS Status;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(Directory->Lock) != FALSE);
    ASSERT(Directory->Properties.HardLinkCount != 0);

    RtlZeroMemory(&Request, sizeof(SYSTEM_CONTROL_CREATE));
    Request.DirectoryProperties = &(Directory->Properties);
    Request.Name = Name;
    Request.NameSize = NameSize;
    RtlCopyMemory(&(Request.FileProperties),
                  Properties,
                  sizeof(FILE_PROPERTIES));

    Status = IopSendSystemControlIrp(Device,
                                     IrpMinorSystemControlCreate,
                                     &Request);

    RtlCopyMemory(Properties,
                  &(Request.FileProperties),
                  sizeof(FILE_PROPERTIES));

    //
    // Update the access time and modified time if file was created.
    //

    if (KSUCCESS(Status)) {
        IopUpdateFileObjectTime(Directory, FileObjectModifiedTime);
        IopUpdateFileObjectFileSize(Directory, Request.DirectorySize);
    }

    return Status;
}

KSTATUS
IopSendUnlinkRequest (
    PDEVICE Device,
    PFILE_OBJECT FileObject,
    PFILE_OBJECT DirectoryObject,
    PCSTR Name,
    ULONG NameSize,
    PBOOL Unlinked
    )

/*++

Routine Description:

    This routine sends an unlink request to the device. This routine assumes
    that the directory's lock is held exclusively.

Arguments:

    Device - Supplies a pointer to the device to send the request to.

    FileObject - Supplies a pointer to the file object of the file that is to
        be unlinked.

    DirectoryObject - Supplies a pointer to the file object of the directory
        from which the file will be unlinked.

    Name - Supplies a pointer to the name of the file or directory to create,
        which may not be null terminated.

    NameSize - Supplies the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a  garbage
        character).

    Unlinked - Supplies a boolean that receives whether or not the file was
        unlinked. The file may be unlinked even if the call fails.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    SYSTEM_CONTROL_UNLINK UnlinkRequest;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(DirectoryObject->Lock));
    ASSERT(DirectoryObject->Properties.HardLinkCount != 0);

    UnlinkRequest.DirectoryProperties = &(DirectoryObject->Properties);
    UnlinkRequest.FileProperties = &(FileObject->Properties);
    UnlinkRequest.Name = Name;
    UnlinkRequest.NameSize = NameSize;
    UnlinkRequest.Unlinked = FALSE;
    Status = IopSendSystemControlIrp(Device,
                                     IrpMinorSystemControlUnlink,
                                     &UnlinkRequest);

    //
    // If the file was successfully unlinked, finish the job even if the IRP
    // failed.
    //

    if (UnlinkRequest.Unlinked != FALSE) {

        //
        // Decrement the hard link count of the file being deleted.
        //

        IopFileObjectDecrementHardLinkCount(FileObject);

        //
        // The directory was modified, update its times.
        //

        IopUpdateFileObjectTime(DirectoryObject, FileObjectModifiedTime);
    }

    *Unlinked = UnlinkRequest.Unlinked;
    return Status;
}

KERNEL_API
KSTATUS
IoGetFileBlockInformation (
    PIO_HANDLE Handle,
    PFILE_BLOCK_INFORMATION *FileBlockInformation
    )

/*++

Routine Description:

    This routine gets a list of logical block offsets for the given file or
    partition, comprising the runs of contiguous disk space taken up by the
    file or partition.

Arguments:

    Handle - Supplies an I/O handle for the file or partition.

    FileBlockInformation - Supplies a pointer that receives a pointer to the
        block information for the file or partition. If this is non-null and a
        partition is queried, then the partition will update the offsets in the
        block information to be logical block offsets for the parent disk.

Return Value:

    Status code.

--*/

{

    SYSTEM_CONTROL_GET_BLOCK_INFORMATION BlockInformation;
    PDEVICE Device;
    PFILE_OBJECT FileObject;
    PIRP Irp;
    PPAGING_IO_HANDLE PagingHandle;
    KSTATUS Status;

    Irp = NULL;
    Status = IoGetDevice(Handle, &Device);
    if (!KSUCCESS(Status)) {
        goto GetBlockInformationEnd;
    }

    if (Handle->HandleType == IoHandleTypePaging) {
        PagingHandle = (PPAGING_IO_HANDLE)Handle;
        Handle = PagingHandle->IoHandle;
    }

    FileObject = Handle->FileObject;
    Irp = IoCreateIrp(Device, IrpMajorSystemControl, 0);
    if (Irp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetBlockInformationEnd;
    }

    BlockInformation.FileProperties = &(FileObject->Properties);
    BlockInformation.FileBlockInformation = *FileBlockInformation;
    Irp->MinorCode = IrpMinorSystemControlGetBlockInformation;
    Irp->U.SystemControl.SystemContext = &BlockInformation;
    Status = IoSendSynchronousIrp(Irp);
    if (!KSUCCESS(Status)) {
        goto GetBlockInformationEnd;
    }

    Status = IoGetIrpStatus(Irp);
    if (!KSUCCESS(Status)) {
        goto GetBlockInformationEnd;
    }

    *FileBlockInformation = BlockInformation.FileBlockInformation;

GetBlockInformationEnd:
    if (Irp != NULL) {
        IoDestroyIrp(Irp);
    }

    return Status;
}

KERNEL_API
VOID
IoDestroyFileBlockInformation (
    PFILE_BLOCK_INFORMATION FileBlockInformation
    )

/*++

Routine Description:

    This routine destroys file block information for a file or partition.

Arguments:

    FileBlockInformation - Supplies a pointer to file block information to be
        destroyed.

Return Value:

    Status code.

--*/

{

    PFILE_BLOCK_ENTRY BlockEntry;

    while (LIST_EMPTY(&(FileBlockInformation->BlockList)) == FALSE) {
        BlockEntry = LIST_VALUE(FileBlockInformation->BlockList.Next,
                                FILE_BLOCK_ENTRY,
                                ListEntry);

        LIST_REMOVE(&(BlockEntry->ListEntry));
        MmFreeNonPagedPool(BlockEntry);
    }

    MmFreeNonPagedPool(FileBlockInformation);
    return;
}

KSTATUS
IoWriteFileBlocks (
    PFILE_BLOCK_IO_CONTEXT FileContext,
    PIO_BUFFER IoBuffer,
    ULONGLONG Offset,
    UINTN SizeInBytes,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine writes data directly to a file's disk blocks, bypassing the
    filesystem. It is meant for critical code paths, such as writing out the
    crash dump file during a system failure.

Arguments:

    FileContext - Supplies a pointer to the file block context, including the
        file's block information, the device's block level I/O routines and
        block information.

    IoBuffer - Supplies a pointer to an I/O buffer with the data to write.

    Offset - Supplies the offset, in bytes, into the file where the data is to
        be written.

    SizeInBytes - Supplies the size of the data to write, in bytes.

    BytesCompleted - Supplies a pointer that receives the total number of bytes
        written to the disk.

Return Value:

    Status code.

--*/

{

    UINTN AlignedSize;
    ULONGLONG BlockCount;
    PLIST_ENTRY BlockList;
    ULONGLONG BlockOffset;
    PFILE_BLOCK_ENTRY BlockRun;
    ULONGLONG BlockRunOffset;
    UINTN BlocksCompleted;
    ULONG BlockShift;
    UINTN BlocksRemaining;
    ULONGLONG BlocksThisRound;
    PDISK_BLOCK_IO_WRITE BlockWrite;
    PLIST_ENTRY CurrentEntry;
    KSTATUS Status;

    ASSERT(POWER_OF_2(FileContext->BlockSize) != FALSE);

    BlockWrite = (PDISK_BLOCK_IO_WRITE)FileContext->BlockIoWrite;

    //
    // Align the size up to full blocks. The I/O buffer should be able to
    // handle it.
    //

    AlignedSize = ALIGN_RANGE_UP(SizeInBytes, FileContext->BlockSize);

    ASSERT(MmGetIoBufferSize(IoBuffer) >= AlignedSize);

    //
    // The I/O buffer should be mapped.
    //

    ASSERT(KSUCCESS(MmMapIoBuffer(IoBuffer, FALSE, FALSE, FALSE)));

    //
    // TODO: Support partial block writes to crash dump files.
    //

    ASSERT(IS_ALIGNED(Offset, FileContext->BlockSize) != FALSE);

    BlockList = &(FileContext->FileBlockInformation->BlockList);
    BlockShift = RtlCountTrailingZeros32(FileContext->BlockSize);
    BlockOffset = Offset >> BlockShift;

    //
    // Find the first block run that this write is targeting.
    //

    BlockCount = 0;
    BlockRun = NULL;
    BlockRunOffset = 0;
    CurrentEntry = BlockList->Next;
    while (CurrentEntry != BlockList) {
        BlockRun = LIST_VALUE(CurrentEntry, FILE_BLOCK_ENTRY, ListEntry);
        if (BlockOffset < (BlockCount + BlockRun->Count)) {

            ASSERT(BlockOffset >= BlockCount);

            BlockRunOffset = BlockOffset - BlockCount;
            break;
        }

        BlockCount += BlockRun->Count;
        CurrentEntry = CurrentEntry->Next;
        BlockRun = NULL;
    }

    //
    // Trusted callers really shouldn't be going beyond the end of the file or
    // the buffer.
    //

    ASSERT(BlockRun != NULL);

    //
    // Loop writing each fragment of the I/O buffer to as many contiguous
    // blocks as possible.
    //

    BlocksRemaining = AlignedSize >> BlockShift;
    while (BlocksRemaining != 0) {

        //
        // Determine how many contiguous blocks can be written this round.
        //

        BlocksThisRound = BlockRun->Count - BlockRunOffset;
        if (BlocksRemaining < BlocksThisRound) {
            BlocksThisRound = BlocksRemaining;
        }

        ASSERT(BlocksThisRound != 0);

        //
        // Send this write down to the disk.
        //

        Status = BlockWrite(FileContext->DiskToken,
                            IoBuffer,
                            BlockRun->Address + BlockRunOffset,
                            (UINTN)BlocksThisRound,
                            &BlocksCompleted);

        if (!KSUCCESS(Status)) {
            goto WriteFileBlocksEnd;
        }

        //
        // Update the I/O buffer offset so the next run starts where this left
        // off.
        //

        MmIoBufferIncrementOffset(IoBuffer, BlocksCompleted << BlockShift);
        BlocksRemaining -= BlocksCompleted;
        if (BlocksCompleted != BlocksThisRound) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto WriteFileBlocksEnd;
        }

        //
        // Move to the next block run if this run is exhausted.
        //

        BlockRunOffset += BlocksThisRound;
        if (BlockRunOffset == BlockRun->Count) {
            CurrentEntry = CurrentEntry->Next;
            if (CurrentEntry == BlockList) {
                break;
            }

            BlockRun = LIST_VALUE(CurrentEntry, FILE_BLOCK_ENTRY, ListEntry);
            BlockRunOffset = 0;
        }
    }

    if (BlocksRemaining != 0) {
        Status = STATUS_END_OF_FILE;
        goto WriteFileBlocksEnd;
    }

    Status = STATUS_SUCCESS;

WriteFileBlocksEnd:
    BlocksCompleted = (AlignedSize >> BlockShift) - BlocksRemaining;
    *BytesCompleted = BlocksCompleted << BlockShift;
    if (*BytesCompleted > SizeInBytes) {
        *BytesCompleted = SizeInBytes;
    }

    if (*BytesCompleted != 0) {
        MmIoBufferDecrementOffset(IoBuffer, *BytesCompleted);
    }

    return Status;
}

KSTATUS
IopSynchronizeBlockDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine sends a sync request to a block device to ensure all data is
    written out to permanent storage.

Arguments:

    Device - Supplies a pointer to the device to send the synchronize request
        to.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = IopSendSystemControlIrp(Device,
                                     IrpMinorSystemControlSynchronize,
                                     NULL);

    return Status;
}

KERNEL_API
KSTATUS
IoLoadFile (
    PCSTR Path,
    ULONG PathLength,
    PLOAD_FILE_COMPLETION_ROUTINE CompletionRoutine,
    PVOID CompletionContext
    )

/*++

Routine Description:

    This routine asynchronously loads the file at the given path. The path can
    either be absolute or relative. For the kernel process, relative paths are
    in relative to the system volume's drivers directory. The supplied
    completion routine is invoked when the load finishes.

Arguments:

    Path - Supplies a pointer to the path to the file. It can either be an
        absolute or relative path. Relative paths for the kernel process are
        relative to the system partition's drivers directory.

    PathLength - Supplies the length of the path buffer in bytes, including the
        null terminator.

    CompletionRoutine - Supplies a pointer to the callback routine to notify
        when the load is complete.

    CompletionContext - Supplies a pointer to an opaque context that will be
        passed to the completion routine along with the loaded file.

Return Value:

    Status code.

--*/

{

    UINTN BytesCompleted;
    ULONGLONG FileSize;
    PLOADED_FILE NewFile;
    KSTATUS Status;

    NewFile = NULL;

    //
    // Fail if the path is NULL or has no length.
    //

    if ((Path == NULL) || (PathLength < 2)) {
        Status = STATUS_INVALID_PARAMETER;
        goto LoadFileEnd;
    }

    //
    // Allocate a new file structure to store the loaded file information.
    //

    NewFile = MmAllocatePagedPool(sizeof(LOADED_FILE), IO_ALLOCATION_TAG);
    if (NewFile == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadFileEnd;
    }

    RtlZeroMemory(NewFile, sizeof(LOADED_FILE));
    NewFile->Version = LOADED_FILE_VERSION;

    //
    // Open the file using the given path. If it is a relative path, then it
    // will search in the process's current directory. For the kernel, that
    // is the drivers directory on the system partition.
    //

    Status = IoOpen(TRUE,
                    NULL,
                    Path,
                    PathLength,
                    IO_ACCESS_READ | IO_ACCESS_EXECUTE,
                    0,
                    FILE_PERMISSION_NONE,
                    &(NewFile->IoHandle));

    if (!KSUCCESS(Status)) {
        goto LoadFileEnd;
    }

    //
    // Get the file size and allocate an I/O buffer to contain it.
    //

    Status = IoGetFileSize(NewFile->IoHandle, &FileSize);
    if (!KSUCCESS(Status)) {
        goto LoadFileEnd;
    }

    if (FileSize > MAX_UINTN) {
        Status = STATUS_NOT_SUPPORTED;
        goto LoadFileEnd;
    }

    NewFile->Length = (UINTN)FileSize;

    //
    // Create an I/O buffer that can support the read.
    //

    NewFile->IoBuffer = MmAllocateUninitializedIoBuffer(NewFile->Length, 0);
    if (NewFile->IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadFileEnd;
    }

    //
    // TODO: Convert file load reads to asynchronous I/O.
    //

    Status = IoReadAtOffset(NewFile->IoHandle,
                            NewFile->IoBuffer,
                            0,
                            NewFile->Length,
                            0,
                            WAIT_TIME_INDEFINITE,
                            &BytesCompleted,
                            NULL);

    if (!KSUCCESS(Status)) {
        goto LoadFileEnd;
    }

    if (BytesCompleted != NewFile->Length) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto LoadFileEnd;
    }

    //
    // With success on the horizon, call the callback to signal completion.
    //

    CompletionRoutine(CompletionContext, NewFile);

LoadFileEnd:
    if (!KSUCCESS(Status)) {
        if (NewFile != NULL) {
            IoUnloadFile(NewFile);
            NewFile = NULL;
        }
    }

    return Status;
}

KERNEL_API
VOID
IoUnloadFile (
    PLOADED_FILE File
    )

/*++

Routine Description:

    This routine unloads the given file.

Arguments:

    File - Supplies a pointer to the file to unload.

Return Value:

    None.

--*/

{

    if (File->IoBuffer != NULL) {
        MmFreeIoBuffer(File->IoBuffer);
    }

    if (File->IoHandle != NULL) {
        IoClose(File->IoHandle);
    }

    MmFreePagedPool(File);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopOpenPagingDevice (
    PDEVICE Device,
    ULONG Access,
    ULONG Flags,
    PPAGING_IO_HANDLE *Handle,
    PULONG IoOffsetAlignment,
    PULONG IoSizeAlignment,
    PULONGLONG IoCapacity
    )

/*++

Routine Description:

    This routine opens a block device for paging.

Arguments:

    Device - Supplies a pointer to the device to open.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

    IoOffsetAlignment - Supplies a pointer where the alignment requirement in
        bytes will be returned for all I/O offsets.

    IoSizeAlignment - Supplies a pointer where the alignment requirement for
        the size of all transfers (the block size) will be returned for all
        I/O requests.

    IoCapacity - Supplies the device's total size, in bytes.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PIO_HANDLE IoHandle;
    PPAGING_IO_HANDLE PagingHandle;
    KSTATUS Status;

    IoHandle = NULL;
    PagingHandle = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Allocate the basic structure.
    //

    PagingHandle = MmAllocateNonPagedPool(sizeof(PAGING_IO_HANDLE),
                                          IO_ALLOCATION_TAG);

    if (PagingHandle == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenPagingDeviceEnd;
    }

    RtlZeroMemory(PagingHandle, sizeof(PAGING_IO_HANDLE));
    PagingHandle->HandleType = IoHandleTypePaging;

    //
    // Open the device normally.
    //

    Status = IopOpenDevice(Device, Access, Flags, &IoHandle);
    if (!KSUCCESS(Status)) {
        goto OpenPagingDeviceEnd;
    }

    //
    // Grab some needed parameters from the paged file object structure.
    //

    FileObject = IoHandle->FileObject;
    PagingHandle->IoHandle = IoHandle;
    PagingHandle->Device = FileObject->Device;
    PagingHandle->DeviceContext = IoHandle->DeviceContext;
    PagingHandle->Capacity = FileObject->Properties.Size;
    PagingHandle->OffsetAlignment = FileObject->Properties.BlockSize;
    PagingHandle->SizeAlignment = PagingHandle->OffsetAlignment;
    *IoOffsetAlignment = PagingHandle->OffsetAlignment;
    *IoSizeAlignment = PagingHandle->SizeAlignment;
    *IoCapacity = PagingHandle->Capacity;
    Status = STATUS_SUCCESS;

OpenPagingDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (IoHandle != NULL) {
            IoClose(IoHandle);
        }

        if (PagingHandle != NULL) {
            MmFreeNonPagedPool(PagingHandle);
        }

        PagingHandle = NULL;
    }

    *Handle = PagingHandle;
    return Status;
}

KSTATUS
IopClosePagingObject (
    PPAGING_IO_HANDLE Handle
    )

/*++

Routine Description:

    This routine closes a page file or device.

Arguments:

    Handle - Supplies the handle returned upon opening the page file or device.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // This routine is called from IoClose, but assert here that it will not
    // recurse more than once.
    //

    ASSERT(Handle->HandleType == IoHandleTypePaging);
    ASSERT(Handle->IoHandle->HandleType == IoHandleTypeDefault);

    Status = IoClose(Handle->IoHandle);
    if (!KSUCCESS(Status)) {
        goto ClosePagingObjectEnd;
    }

    MmFreeNonPagedPool(Handle);
    Status = STATUS_SUCCESS;

ClosePagingObjectEnd:
    return Status;
}

KSTATUS
IopCreateAnonymousObject (
    BOOL FromKernelMode,
    ULONG Access,
    ULONG Flags,
    PCREATE_PARAMETERS Create,
    PPATH_POINT PathPoint
    )

/*++

Routine Description:

    This routine creates an anonymous I/O object, one that is not visible
    in the global path system.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Create - Supplies a pointer to the creation parameters.

    PathPoint - Supplies a pointer that receives the path entry and mount point
        of the newly minted path point.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PPATH_ENTRY PathEntry;
    KSTATUS Status;

    FileObject = NULL;
    PathEntry = NULL;
    Status = IopCreateSpecialIoObject(FromKernelMode,
                                      Flags,
                                      Create,
                                      &FileObject);

    if (!KSUCCESS(Status)) {
        goto CreateAnonymousObjectEnd;
    }

    //
    // Create an anonymous path entry for this object.
    //

    PathEntry = IopCreateAnonymousPathEntry(FileObject);
    if (PathEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateAnonymousObjectEnd;
    }

    FileObject = NULL;

CreateAnonymousObjectEnd:
    if (!KSUCCESS(Status)) {
        if (PathEntry != NULL) {
            IoPathEntryReleaseReference(PathEntry);
            PathEntry = NULL;
        }
    }

    if (FileObject != NULL) {
        IopFileObjectReleaseReference(FileObject);
    }

    PathPoint->PathEntry = PathEntry;
    PathPoint->MountPoint = NULL;
    return Status;
}

KSTATUS
IopPerformIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT Context
    )

/*++

Routine Description:

    This routine reads from or writes to a file or device.

Arguments:

    Handle - Supplies the open I/O handle.

    Context - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    PFILE_OBJECT FileObject;
    KSTATUS Status;

    ASSERT((Context->Flags & IO_FLAG_NO_ALLOCATE) == 0);
    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Context->BytesCompleted == 0);
    ASSERT(Context->IoBuffer != NULL);

    FileObject = Handle->FileObject;
    if (FileObject == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto PerformIoOperationEnd;
    }

    //
    // Non-blocking handles always have a timeout of zero.
    //

    if ((Handle->OpenFlags & OPEN_FLAG_NON_BLOCKING) != 0) {
        Context->TimeoutInMilliseconds = 0;
    }

    if ((Handle->OpenFlags & OPEN_FLAG_SYNCHRONIZED) != 0) {
        Context->Flags |= IO_FLAG_DATA_SYNCHRONIZED;
    }

    //
    // Fail if the caller hadn't opened the file with the correct access.
    //

    if (Context->Write != FALSE) {
        if ((Handle->Access & IO_ACCESS_WRITE) == 0) {
            Status = STATUS_INVALID_HANDLE;
            goto PerformIoOperationEnd;
        }

    } else {
        if ((Handle->Access & (IO_ACCESS_READ | IO_ACCESS_EXECUTE)) == 0) {
            Status = STATUS_INVALID_HANDLE;
            goto PerformIoOperationEnd;
        }
    }

    //
    // Perform the operation based on the file object type.
    //

    switch (FileObject->Properties.Type) {
    case IoObjectBlockDevice:
    case IoObjectRegularFile:
    case IoObjectSharedMemoryObject:
    case IoObjectSymbolicLink:
        Status = IopPerformCacheableIoOperation(Handle, Context);
        break;

    case IoObjectCharacterDevice:
        Status = IopPerformCharacterDeviceIoOperation(Handle, Context);
        break;

    case IoObjectRegularDirectory:
        Status = IopPerformDirectoryIoOperation(Handle, Context);
        break;

    case IoObjectPipe:
        Status = IopPerformPipeIoOperation(Handle, Context);
        break;

    case IoObjectSocket:
        Status = IopPerformSocketIoOperation(Handle, Context);
        break;

    case IoObjectTerminalMaster:
        Status = IopPerformTerminalMasterIoOperation(Handle, Context);
        break;

    case IoObjectTerminalSlave:
        Status = IopPerformTerminalSlaveIoOperation(Handle, Context);
        break;

    case IoObjectObjectDirectory:
        Status = IopPerformObjectIoOperation(Handle, Context);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        goto PerformIoOperationEnd;
    }

PerformIoOperationEnd:

    ASSERT(Context->BytesCompleted <= Context->SizeInBytes);

    return Status;
}

KSTATUS
IopPerformPagingIoOperation (
    PPAGING_IO_HANDLE Handle,
    PIO_CONTEXT Context,
    PIRP Irp
    )

/*++

Routine Description:

    This routine reads from or writes to a file or device.

Arguments:

    Handle - Supplies the open I/O handle.

    Context - Supplies a pointer to the paging I/O context.

    Irp - Supplies a pointer to the IRP to use for this I/O.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    KSTATUS Status;

    //
    // Reset the paging IRP. The IRP should never be null.
    //

    ASSERT(Irp != NULL);
    ASSERT(Context->BytesCompleted == 0);
    ASSERT(Handle->HandleType == IoHandleTypePaging);
    ASSERT(Context->IoBuffer != NULL);
    ASSERT(IS_ALIGNED(Context->SizeInBytes, MmPageSize()) != FALSE);

    IoInitializeIrp(Irp);

    ASSERT(Irp->MajorCode == IrpMajorIo);

    if (Context->Write != FALSE) {
        Irp->MinorCode = IrpMinorIoWrite;

    } else {
        Irp->MinorCode = IrpMinorIoRead;
    }

    ASSERT((Context->Offset + Context->SizeInBytes) <= Handle->Capacity);

    //
    // Use the supplied I/O buffer directly. This code path should only be
    // reached by trusted callers. The buffer should be properly aligned, etc.
    //

    Irp->U.ReadWrite.IoBuffer = Context->IoBuffer;
    Irp->U.ReadWrite.DeviceContext = Handle->DeviceContext;
    Irp->U.ReadWrite.IoFlags = Context->Flags;
    Irp->U.ReadWrite.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    Irp->U.ReadWrite.IoOffset = Context->Offset;
    Irp->U.ReadWrite.IoSizeInBytes = Context->SizeInBytes;
    Irp->U.ReadWrite.IoBytesCompleted = 0;
    Status = IoSendSynchronousIrp(Irp);
    if (!KSUCCESS(Status)) {
        goto PerformPageFileIoOperationEnd;
    }

    //
    // Update the global statistics.
    //

    if (Handle->Device->Header.Type == ObjectDevice) {
        if (Context->Write != FALSE) {
            RtlAtomicAdd64(&(IoGlobalStatistics.PagingBytesWritten),
                           Irp->U.ReadWrite.IoBytesCompleted);

        } else {
            RtlAtomicAdd64(&(IoGlobalStatistics.PagingBytesRead),
                           Irp->U.ReadWrite.IoBytesCompleted);
        }
    }

    if (Irp->U.ReadWrite.IoBytesCompleted != Irp->U.ReadWrite.IoSizeInBytes) {

        ASSERT(FALSE);

        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto PerformPageFileIoOperationEnd;
    }

    Status = IoGetIrpStatus(Irp);

PerformPageFileIoOperationEnd:
    Context->BytesCompleted = Irp->U.ReadWrite.IoBytesCompleted;
    return Status;
}

KSTATUS
IopPerformCharacterDeviceIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT Context
    )

/*++

Routine Description:

    This routine performeds read and write I/O to a character device.

Arguments:

    Handle - Supplies a pointer to the I/O handle.

    Context - Supplies a pointer to the I/O context.

Return Value:

    Status code.

--*/

{

    PDEVICE Device;
    PFILE_OBJECT FileObject;
    IRP_MINOR_CODE MinorCode;
    IRP_READ_WRITE Parameters;
    KSTATUS Status;

    ASSERT(Context->IoBuffer != NULL);

    FileObject = Handle->FileObject;

    ASSERT(FileObject->Properties.Type == IoObjectCharacterDevice);

    //
    // Initialize the parameters for the I/O IRP. The offset does not matter
    // for character devices. Set it to 0, always.
    //

    Parameters.IoBuffer = Context->IoBuffer;
    Parameters.DeviceContext = Handle->DeviceContext;
    Parameters.IoFlags = Context->Flags;
    Parameters.TimeoutInMilliseconds = Context->TimeoutInMilliseconds;
    Parameters.IoSizeInBytes = Context->SizeInBytes;
    Parameters.IoBytesCompleted = 0;
    Parameters.IoOffset = Context->Offset;
    Parameters.FileProperties = &(FileObject->Properties);
    if (Context->Offset == IO_OFFSET_NONE) {
        Parameters.IoOffset =
                        RtlAtomicOr64((PULONGLONG)&(Handle->CurrentOffset), 0);
    }

    Parameters.NewIoOffset = Context->Offset;
    Device = FileObject->Device;

    ASSERT(IS_DEVICE_OR_VOLUME(Device));

    if (Context->Write != FALSE) {
        MinorCode = IrpMinorIoWrite;

    } else {
        MinorCode = IrpMinorIoRead;
    }

    //
    // Fire off the I/O.
    //

    Status = IopSendIoIrp(Device, MinorCode, &Parameters);
    Context->BytesCompleted = Parameters.IoBytesCompleted;
    if (Context->Offset == IO_OFFSET_NONE) {
        RtlAtomicExchange64((PULONGLONG)&(Handle->CurrentOffset),
                            Parameters.NewIoOffset);
    }

    return Status;
}

KSTATUS
IopPerformDirectoryIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT Context
    )

/*++

Routine Description:

    This routine performs I/O operations on regular directory handles. Only
    read operations should be requested from a directory handle. It is
    important to note that the supplied offset is a directory entry offset and
    not a byte offset.

Arguments:

    Handle - Supplies a pointer to the I/O handle.

    Context - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    PDEVICE Device;
    PFILE_OBJECT FileObject;
    BOOL LockHeldExclusive;
    IRP_READ_WRITE Parameters;
    KSTATUS Status;

    ASSERT(Context->IoBuffer != NULL);
    ASSERT((Context->Write == FALSE) && (Context->Flags == 0));

    Context->BytesCompleted = 0;
    Parameters.IoBytesCompleted = Context->BytesCompleted;
    FileObject = Handle->FileObject;

    ASSERT(FileObject != NULL);

    KeAcquireSharedExclusiveLockShared(FileObject->Lock);
    LockHeldExclusive = FALSE;
    if (Context->Offset != IO_OFFSET_NONE) {
        Parameters.IoOffset = Context->Offset;

    } else {
        Parameters.IoOffset =
                        RtlAtomicOr64((PULONGLONG)&(Handle->CurrentOffset), 0);
    }

    Parameters.NewIoOffset = Parameters.IoOffset;
    if ((Handle->OpenFlags & OPEN_FLAG_DIRECTORY) == 0) {
        Status = STATUS_FILE_IS_DIRECTORY;
        goto PerformDirectoryIoOperationEnd;
    }

    //
    // If this was a directory, add the relative entries.
    //

    Status = IopAddRelativeDirectoryEntries(Handle,
                                            &(Parameters.IoOffset),
                                            Context->IoBuffer,
                                            Context->SizeInBytes,
                                            &(Parameters.IoBytesCompleted));

    Parameters.NewIoOffset = Parameters.IoOffset;
    if (!KSUCCESS(Status)) {
        goto PerformDirectoryIoOperationEnd;
    }

    //
    // On success, both relative directory entries were added. Now off to the
    // driver to add more.
    //

    ASSERT(Parameters.IoOffset >= DIRECTORY_CONTENTS_OFFSET);

    //
    // This I/O buffer does not need to be locked in memory at the moment.
    // If some future file system requires use of the physical addresses,
    // then it needs to be locked in memory.
    //

    Parameters.IoBuffer = Context->IoBuffer;
    Parameters.DeviceContext = Handle->DeviceContext;
    Parameters.IoFlags = Context->Flags;
    Parameters.TimeoutInMilliseconds = Context->TimeoutInMilliseconds;
    Parameters.IoSizeInBytes = Context->SizeInBytes;
    Parameters.FileProperties = &(FileObject->Properties);

    //
    // Acquire the file lock in shared mode and fire off the I/O!
    //

    Device = FileObject->Device;

    ASSERT(IS_DEVICE_OR_VOLUME(Device));

    Status = IopSendIoIrp(Device, IrpMinorIoRead, &Parameters);
    if (KSUCCESS(Status) || (Status == STATUS_END_OF_FILE)) {
        if ((Handle->OpenFlags & OPEN_FLAG_NO_ACCESS_TIME) == 0) {

            ASSERT(LockHeldExclusive == FALSE);

            KeSharedExclusiveLockConvertToExclusive(FileObject->Lock);
            LockHeldExclusive = TRUE;
            IopUpdateFileObjectTime(FileObject, FileObjectAccessTime);
        }
    }

PerformDirectoryIoOperationEnd:

    //
    // Adjust the current offset.
    //

    if (Context->Offset == IO_OFFSET_NONE) {
        RtlAtomicExchange64((PULONGLONG)&(Handle->CurrentOffset),
                            Parameters.NewIoOffset);
    }

    if (LockHeldExclusive != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);

    } else {
        KeReleaseSharedExclusiveLockShared(FileObject->Lock);
    }

    //
    // Modify the file IDs of any directory entries that are mount points.
    // This needs to happen for any directory entries read from disk.
    //

    if (Parameters.IoBytesCompleted != 0) {
        IopFixMountPointDirectoryEntries(Handle,
                                         Context->IoBuffer,
                                         Parameters.IoBytesCompleted);
    }

    Context->BytesCompleted = Parameters.IoBytesCompleted;
    return Status;
}

KSTATUS
IopAddRelativeDirectoryEntries (
    PIO_HANDLE Handle,
    PIO_OFFSET Offset,
    PIO_BUFFER IoBuffer,
    UINTN BufferSize,
    PUINTN BytesConsumed
    )

/*++

Routine Description:

    This routine adds the relative . and .. directory entries to a directory
    read operation.

Arguments:

    Handle - Supplies the open I/O handle.

    Offset - Supplies a pointer to the offset to read from. On return contains
        the new offset.

    IoBuffer - Supplies a pointer to the I/O buffer that will contain the added
        relative directory entries on output.

    BufferSize - Supplies the size of the I/O buffer, in bytes.

    BytesConsumed - Supplies the a pointer that on input contains the number
        of bytes in the buffer that have already been used. On output, it will
        contain the updated number of bytes used.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    UINTN BytesAvailable;
    PDIRECTORY_ENTRY Entry;
    ULONG EntrySize;
    PFILE_OBJECT FileObject;
    IO_OFFSET FileOffset;
    UCHAR LocalBuffer[sizeof(DIRECTORY_ENTRY) + sizeof("..") + 8];
    PATH_POINT Parent;
    PKPROCESS Process;
    PPATH_POINT Root;
    KSTATUS Status;

    ASSERT(BufferSize >= *BytesConsumed);

    BytesAvailable = BufferSize - *BytesConsumed;
    FileOffset = *Offset;
    Entry = (PDIRECTORY_ENTRY)LocalBuffer;
    Status = STATUS_MORE_PROCESSING_REQUIRED;

    //
    // Tack on a . and a .. entry. Use reserved file offsets to remember
    // which directory entries were reported.
    //

    if (FileOffset == DIRECTORY_OFFSET_DOT) {
        EntrySize = ALIGN_RANGE_UP(sizeof(DIRECTORY_ENTRY) + sizeof("."), 8);
        if (BytesAvailable > EntrySize) {
            Entry->Size = EntrySize;
            Entry->Type = IoObjectRegularDirectory;
            Entry->NextOffset = DIRECTORY_OFFSET_DOT_DOT;
            FileObject = Handle->FileObject;

            ASSERT(FileObject == Handle->PathPoint.PathEntry->FileObject);

            Entry->FileId = FileObject->Properties.FileId;
            RtlCopyMemory(Entry + 1, ".", sizeof("."));
            Status = MmCopyIoBufferData(IoBuffer,
                                        Entry,
                                        *BytesConsumed,
                                        EntrySize,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                goto AddRelativeDirectoryEntriesEnd;
            }

            *BytesConsumed += EntrySize;
            BytesAvailable -= EntrySize;
            FileOffset = Entry->NextOffset;
        }
    }

    if (FileOffset == DIRECTORY_OFFSET_DOT_DOT) {
        EntrySize = ALIGN_RANGE_UP(sizeof(DIRECTORY_ENTRY) + sizeof(".."), 8);
        if (BytesAvailable > EntrySize) {
            Entry->Size = EntrySize;
            Entry->Type = IoObjectRegularDirectory;
            Entry->NextOffset = DIRECTORY_CONTENTS_OFFSET;

            //
            // Get the parent path point. Provide the process root to prevent
            // leaking a file ID outside of the root. This does not need to
            // hold the process' path locks because changing roots is required
            // to be a single-threaded operation.
            //

            Root = NULL;
            Process = PsGetCurrentProcess();
            if (Process->Paths.Root.PathEntry != NULL) {
                Root = (PPATH_POINT)&(Process->Paths.Root);
            }

            IopGetParentPathPoint(Root, &(Handle->PathPoint), &Parent);
            FileObject = Parent.PathEntry->FileObject;
            IO_PATH_POINT_RELEASE_REFERENCE(&Parent);
            Entry->FileId = FileObject->Properties.FileId;
            RtlCopyMemory(Entry + 1, "..", sizeof(".."));
            Status = MmCopyIoBufferData(IoBuffer,
                                        Entry,
                                        *BytesConsumed,
                                        EntrySize,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                goto AddRelativeDirectoryEntriesEnd;
            }

            *BytesConsumed += EntrySize;
            BytesAvailable -= EntrySize;
            FileOffset = Entry->NextOffset;
        }
    }

    if (FileOffset >= DIRECTORY_CONTENTS_OFFSET) {
        Status = STATUS_SUCCESS;
    }

AddRelativeDirectoryEntriesEnd:
    *Offset = FileOffset;
    return Status;
}

VOID
IopFixMountPointDirectoryEntries (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    UINTN BufferSize
    )

/*++

Routine Description:

    This routine searches for mount points within the provided directory and
    patches up the directory entries in the buffer to reflect those mount
    points.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to the buffer filled with directory entries.

    BufferSize - Supplies the size of the directory entries buffer.

Return Value:

    None.

--*/

{

    UINTN BytesRemaining;
    PLIST_ENTRY CurrentEntry;
    DIRECTORY_ENTRY DirectoryEntry;
    BOOL FixRequired;
    PMOUNT_POINT MountPoint;
    UINTN Offset;
    FILE_ID OriginalFileId;
    PFILE_OBJECT OriginalFileObject;
    PPATH_POINT PathPoint;
    KSTATUS Status;
    FILE_ID TargetFileId;
    PFILE_OBJECT TargetFileObject;

    PathPoint = &(Handle->PathPoint);

    //
    // If the current mount point has no children, then there is nothing to
    // patch.
    //

    if (LIST_EMPTY(&(PathPoint->MountPoint->ChildListHead)) != FALSE) {
        return;
    }

    //
    // The current mount point has child mounts, but their root path entries
    // are not necessarily children of the given current path entry. Check to
    // make sure at least one fix up is required.
    //

    FixRequired = FALSE;
    KeAcquireSharedExclusiveLockShared(IoMountLock);
    CurrentEntry = PathPoint->MountPoint->ChildListHead.Next;
    while (CurrentEntry != &(PathPoint->MountPoint->ChildListHead)) {
        MountPoint = LIST_VALUE(CurrentEntry, MOUNT_POINT, SiblingListEntry);
        if (MountPoint->MountEntry->Parent == PathPoint->PathEntry) {
            FixRequired = TRUE;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // If no mount points were direct children of the current path, then exit.
    //

    if (FixRequired == FALSE) {
        goto FixMountPointDirectoryEntriesEnd;
    }

    //
    // Otherwise, bite the bullet and iterate over the whole directory. Keep in
    // mind that the mount point's child list may contain multiple entries that
    // mount on top of the same file, so it should not be the primary loop.
    //

    Offset = 0;
    BytesRemaining = BufferSize;
    while (BytesRemaining >= sizeof(DIRECTORY_ENTRY)) {
        Status = MmCopyIoBufferData(IoBuffer,
                                    &DirectoryEntry,
                                    Offset,
                                    sizeof(DIRECTORY_ENTRY),
                                    FALSE);

        if (!KSUCCESS(Status)) {
            goto FixMountPointDirectoryEntriesEnd;
        }

        OriginalFileId = DirectoryEntry.FileId;
        TargetFileObject = NULL;
        CurrentEntry = PathPoint->MountPoint->ChildListHead.Next;
        while (CurrentEntry != &(PathPoint->MountPoint->ChildListHead)) {
            MountPoint = LIST_VALUE(CurrentEntry,
                                    MOUNT_POINT,
                                    SiblingListEntry);

            OriginalFileObject = MountPoint->MountEntry->FileObject;
            if (OriginalFileObject->Properties.FileId == OriginalFileId) {
                TargetFileObject = MountPoint->TargetEntry->FileObject;
                TargetFileId = TargetFileObject->Properties.FileId;
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        if (TargetFileObject != NULL) {
            DirectoryEntry.FileId = TargetFileId;
            MmCopyIoBufferData(IoBuffer,
                               &DirectoryEntry,
                               Offset,
                               sizeof(DIRECTORY_ENTRY),
                               TRUE);
        }

        Offset += DirectoryEntry.Size;

        //
        // Assert that the subtraction will not underflow.
        //

        ASSERT((BufferSize - Offset) <= BytesRemaining);

        BytesRemaining = BufferSize - Offset;
    }

FixMountPointDirectoryEntriesEnd:
    KeReleaseSharedExclusiveLockShared(IoMountLock);
    return;
}

