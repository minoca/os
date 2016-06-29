/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    shmemobj.c

Abstract:

    This module implements shared memory objects.

Author:

    Chris Stevens 12-Mar-2014

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
// TODO: This doesn't work, as Volume0 is usually a small 10MB boot partition.
// Make shared memory objects work in this case and when the file system is
// all read-only.
//

#define SHARED_MEMORY_OBJECT_DIRECTORY "/Volume/Volume0/temp"
#define SHARED_MEMORY_OBJECT_DIRECTORY_LENGTH \
    (RtlStringLength(SHARED_MEMORY_OBJECT_DIRECTORY) + 1)

#define SHARED_MEMORY_OBJECT_FORMAT_STRING "#ShmObject#%x#%d"

#define MAX_SHARED_MEMORY_OBJECT_CREATE_RETRIES 10

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a shared memory object.

Members:

    Header - Stores the object header.

    FileObject - Stores a pointer to the file object associated with the shared
        memory object.

    BackingImage - Stores a handle to the file that backs the shared memory
        object.

--*/

typedef struct _SHARED_MEMORY_OBJECT {
    OBJECT_HEADER Header;
    PFILE_OBJECT FileObject;
    PIO_HANDLE BackingImage;
} SHARED_MEMORY_OBJECT, *PSHARED_MEMORY_OBJECT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
IopDestroySharedMemoryObject (
    PVOID SharedMemoryObject
    );

KSTATUS
IopAddSharedMemoryObjectPathPrefix (
    PSTR Path,
    ULONG PathLength,
    PSTR *NewPath,
    PULONG NewPathLength
    );

//
// -------------------------------------------------------------------- Globals
//

POBJECT_HEADER IoSharedMemoryObjectDirectory;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IopOpenSharedMemoryObject (
    PSTR Path,
    ULONG PathLength,
    ULONG Access,
    ULONG Flags,
    FILE_PERMISSIONS CreatePermissions,
    PIO_HANDLE *Handle
    )

/*++

Routine Description:

    This routine opens a shared memory objeect.

Arguments:

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

    PSTR NewPath;
    ULONG NewPathLength;
    KSTATUS Status;

    NewPath = NULL;

    //
    // Append the appropriate shared memory object path to the given path
    // unless the path is empty and it's meant to be an anonymous object.
    //

    if (PathLength != 0) {
        Status = IopAddSharedMemoryObjectPathPrefix(Path,
                                                    PathLength,
                                                    &NewPath,
                                                    &NewPathLength);

        if (!KSUCCESS(Status)) {
            goto OpenSharedMemoryObjectEnd;
        }

        Path = NewPath;
        PathLength = NewPathLength;
    }

    //
    // With the correct path in place, open the shared memory object.
    //

    Status = IopOpen(TRUE,
                     NULL,
                     Path,
                     PathLength,
                     Access,
                     Flags,
                     IoObjectSharedMemoryObject,
                     NULL,
                     CreatePermissions,
                     Handle);

    if (!KSUCCESS(Status)) {
        goto OpenSharedMemoryObjectEnd;
    }

OpenSharedMemoryObjectEnd:
    if (NewPath != NULL) {
        MmFreePagedPool(NewPath);
    }

    return Status;
}

KSTATUS
IopDeleteSharedMemoryObject (
    PSTR Path,
    ULONG PathLength
    )

/*++

Routine Description:

    This routine deletes a shared memory object. It does not handle deletion of
    unnamed anonymous shared memory objects.

Arguments:

    Path - Supplies a pointer to the path of the shared memory object within
        the shared memory object namespace.

    PathLength - Supplies the length of the path, in bytes, including the null
        terminator.

Return Value:

    Status code.

--*/

{

    PSTR NewPath;
    ULONG NewPathLength;
    KSTATUS Status;

    NewPath = NULL;

    //
    // If the supplied path is empty, then fail the delete.
    //

    if (PathLength == 0) {
        Status = STATUS_PATH_NOT_FOUND;
        goto DeleteSharedMemroyObjectEnd;
    }

    //
    // Append the appropriate shared memory object path to givn path.
    //

    Status = IopAddSharedMemoryObjectPathPrefix(Path,
                                                PathLength,
                                                &NewPath,
                                                &NewPathLength);

    if (!KSUCCESS(Status)) {
        goto DeleteSharedMemroyObjectEnd;
    }

    //
    // With the correct path in place, delete the shared memory object. This
    // must be marked as a call from kernel mode even though the delete may
    // have come from user mode because the prefix might have created a path
    // outside of the calling processes root.
    //

    Status = IopDelete(TRUE, NULL, NewPath, NewPathLength, 0);
    if (!KSUCCESS(Status)) {
        goto DeleteSharedMemroyObjectEnd;
    }

DeleteSharedMemroyObjectEnd:
    if (NewPath != NULL) {
        MmFreePagedPool(NewPath);
    }

    return Status;
}

POBJECT_HEADER
IopGetSharedMemoryObjectDirectory (
    VOID
    )

/*++

Routine Description:

    This routine returns the shared memory objects' root directory in the
    object manager's system. This is the only place in the object system
    shared memory object creation is allowed.

Arguments:

    None.

Return Value:

    Returns a pointer to the shared memory object directory.

--*/

{

    return IoSharedMemoryObjectDirectory;
}

KSTATUS
IopCreateSharedMemoryObject (
    PSTR Name,
    ULONG NameSize,
    ULONG Flags,
    FILE_PERMISSIONS Permissions,
    PFILE_OBJECT *FileObject
    )

/*++

Routine Description:

    This routine actually creates a new shared memory object.

Arguments:

    Name - Supplies an optional pointer to the shared memory object name. This
        is only used for shared memory objects created in the shared memory
        directory.

    NameSize - Supplies the size of the name in bytes including the null
        terminator.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Permissions - Supplies the permissions to give to the file object.

    FileObject - Supplies a pointer where a pointer to a newly created pipe
        file object will be returned on success.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    ULONG DirectoryAccess;
    PIO_HANDLE DirectoryHandle;
    ULONG DirectoryOpenFlags;
    PSHARED_MEMORY_OBJECT ExistingObject;
    ULONG FileAccess;
    PSTR FileName;
    ULONG FileNameLength;
    ULONG FileOpenFlags;
    FILE_PROPERTIES FileProperties;
    PIO_HANDLE Handle;
    ULONG MaxFileNameLength;
    PFILE_OBJECT NewFileObject;
    PSHARED_MEMORY_OBJECT NewSharedMemoryObject;
    PSTR Path;
    ULONG PathLength;
    ULONG RetryCount;
    KSTATUS Status;

    DirectoryHandle = INVALID_HANDLE;
    FileName = NULL;
    NewFileObject = NULL;
    NewSharedMemoryObject = NULL;
    Path = NULL;

    //
    // Create an object manager object. If it is to be immediately unlinked, do
    // not give it a name.
    //

    if ((Flags & OPEN_FLAG_UNLINK_ON_CREATE) != 0) {
        Name = NULL;
        NameSize = 0;
    }

    //
    // Make sure there is not already an existing shared memory object by the
    // same name. The caller should have the appropriate locks to make the
    // check and create synchronous.
    //

    if (Name != NULL) {
        ExistingObject = ObFindObject(Name,
                                      NameSize,
                                      IoSharedMemoryObjectDirectory);

        if (ExistingObject != NULL) {
            ObReleaseReference(ExistingObject);
            Status = STATUS_FILE_EXISTS;
            goto CreateSharedMemoryObjectEnd;
        }
    }

    NewSharedMemoryObject = ObCreateObject(ObjectSharedMemoryObject,
                                           IoSharedMemoryObjectDirectory,
                                           Name,
                                           NameSize,
                                           sizeof(SHARED_MEMORY_OBJECT),
                                           IopDestroySharedMemoryObject,
                                           0,
                                           IO_ALLOCATION_TAG);

    if (NewSharedMemoryObject == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateSharedMemoryObjectEnd;
    }

    NewSharedMemoryObject->BackingImage = INVALID_HANDLE;

    //
    // Create a file object, if needed.
    //

    if (*FileObject == NULL) {
        RtlZeroMemory(&FileProperties, sizeof(FILE_PROPERTIES));
        IopFillOutFilePropertiesForObject(&FileProperties,
                                          &(NewSharedMemoryObject->Header));

        if ((Flags & OPEN_FLAG_UNLINK_ON_CREATE) != 0) {
            FileProperties.HardLinkCount = 0;
        }

        FileProperties.Permissions = Permissions;
        FileProperties.BlockSize = IoGetCacheEntryDataSize();
        FileProperties.Type = IoObjectSharedMemoryObject;
        Status = IopCreateOrLookupFileObject(&FileProperties,
                                             ObGetRootObject(),
                                             FILE_OBJECT_FLAG_EXTERNAL_IO_STATE,
                                             &NewFileObject,
                                             &Created);

        if (!KSUCCESS(Status)) {

            //
            // Release reference taken when filling out the file properties.
            //

            ObReleaseReference(NewSharedMemoryObject);
            NewSharedMemoryObject = NULL;
            goto CreateSharedMemoryObjectEnd;
        }

        ASSERT(Created != FALSE);

        *FileObject = NewFileObject;
    }

    //
    // Allocate a buffer that will be used to create the shared memory object's
    // backing image's file name.
    //

    MaxFileNameLength = RtlPrintToString(
                                      NULL,
                                      0,
                                      CharacterEncodingDefault,
                                      SHARED_MEMORY_OBJECT_FORMAT_STRING,
                                      NewSharedMemoryObject,
                                      MAX_SHARED_MEMORY_OBJECT_CREATE_RETRIES);

    FileName = MmAllocatePagedPool(MaxFileNameLength, IO_ALLOCATION_TAG);
    if (FileName == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateSharedMemoryObjectEnd;
    }

    //
    // Loop trying to create the backing image.
    //

    DirectoryOpenFlags = OPEN_FLAG_CREATE | OPEN_FLAG_DIRECTORY;
    DirectoryAccess = IO_ACCESS_READ | IO_ACCESS_WRITE;
    FileOpenFlags = OPEN_FLAG_NON_CACHED |
                    OPEN_FLAG_CREATE |
                    OPEN_FLAG_FAIL_IF_EXISTS |
                    OPEN_FLAG_UNLINK_ON_CREATE;

    FileAccess = IO_ACCESS_READ | IO_ACCESS_WRITE;
    RetryCount = 0;
    while (RetryCount < MAX_SHARED_MEMORY_OBJECT_CREATE_RETRIES) {

        //
        // Make sure that the shared memory object directory exists.
        //

        Status = IoOpen(TRUE,
                        NULL,
                        SHARED_MEMORY_OBJECT_DIRECTORY,
                        SHARED_MEMORY_OBJECT_DIRECTORY_LENGTH,
                        DirectoryAccess,
                        DirectoryOpenFlags,
                        FILE_PERMISSION_NONE,
                        &DirectoryHandle);

        if (!KSUCCESS(Status) && (Status != STATUS_FILE_EXISTS)) {
            goto CreateSharedMemoryObjectEnd;
        }

        if (DirectoryHandle != INVALID_HANDLE) {
            IoClose(DirectoryHandle);
        }

        //
        // Create the path to the shared memory object's backing image.
        //

        FileNameLength = RtlPrintToString(FileName,
                                          MaxFileNameLength,
                                          CharacterEncodingDefault,
                                          SHARED_MEMORY_OBJECT_FORMAT_STRING,
                                          NewSharedMemoryObject,
                                          RetryCount);

        if (Path != NULL) {
            MmFreePagedPool(Path);
            Path = NULL;
        }

        Status = IoPathAppend(SHARED_MEMORY_OBJECT_DIRECTORY,
                              SHARED_MEMORY_OBJECT_DIRECTORY_LENGTH,
                              FileName,
                              FileNameLength,
                              IO_ALLOCATION_TAG,
                              &Path,
                              &PathLength);

        if (!KSUCCESS(Status)) {
            goto CreateSharedMemoryObjectEnd;
        }

        Status = IoOpen(TRUE,
                        NULL,
                        Path,
                        PathLength,
                        FileAccess,
                        FileOpenFlags,
                        FILE_PERMISSION_NONE,
                        &Handle);

        //
        // If the file already exists or the directroy got removed since it
        // was created above, try again.
        //

        if ((Status == STATUS_FILE_EXISTS) ||
            (Status == STATUS_PATH_NOT_FOUND)) {

            RetryCount += 1;
            continue;
        }

        if (!KSUCCESS(Status)) {
            goto CreateSharedMemoryObjectEnd;
        }

        break;
    }

    ASSERT(Handle != INVALID_HANDLE);

    NewSharedMemoryObject->BackingImage = Handle;

    //
    // If the shared memory object is named, then it is valid until it is
    // unlinked. So, add a reference to the file object to make sure the create
    // premissions stick around.
    //

    if (Name != NULL) {
        IopFileObjectAddReference(*FileObject);
        NewSharedMemoryObject->FileObject = *FileObject;
    }

    (*FileObject)->SpecialIo = NewSharedMemoryObject;

    ASSERT(Created != FALSE);

    Status = STATUS_SUCCESS;

CreateSharedMemoryObjectEnd:

    //
    // On both success and failure, the file object's ready event needs to be
    // signaled. Other threads may be waiting on the event.
    //

    if (*FileObject != NULL) {
        KeSignalEvent((*FileObject)->ReadyEvent, SignalOptionSignalAll);
    }

    if (!KSUCCESS(Status)) {
        if (NewFileObject != NULL) {
            IopFileObjectReleaseReference(NewFileObject);
            NewFileObject = NULL;
            *FileObject = NULL;
        }

        if (NewSharedMemoryObject != NULL) {
            ObReleaseReference(NewSharedMemoryObject);
        }
    }

    if (Path != NULL) {
        MmFreePagedPool(Path);
    }

    if (FileName != NULL) {
        MmFreePagedPool(FileName);
    }

    return Status;
}

KSTATUS
IopTruncateSharedMemoryObject (
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine truncates a shared memory object. It assumes that the file's
    lock is held exclusively.

Arguments:

    FileObject - Supplies a pointer to the file object that owns the shared
        memory object.

Return Value:

    Status code.

--*/

{

    PIO_HANDLE BackingImage;
    PFILE_OBJECT BackingImageObject;
    ULONGLONG BackingImageSize;
    ULONGLONG FileSize;
    PSHARED_MEMORY_OBJECT SharedMemoryObject;
    KSTATUS Status;

    ASSERT(FileObject->Properties.Type == IoObjectSharedMemoryObject);
    ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock) != FALSE);

    SharedMemoryObject = FileObject->SpecialIo;

    //
    // There must be a backing image as long as the shared object is alive.
    //

    ASSERT(SharedMemoryObject->BackingImage != INVALID_HANDLE);

    //
    // Otherwise modify the backing image's file size to be the same as the
    // shared memory object's file size.
    //

    BackingImage = SharedMemoryObject->BackingImage;
    BackingImageObject = BackingImage->FileObject;
    READ_INT64_SYNC(&(FileObject->Properties.FileSize), &FileSize);
    READ_INT64_SYNC(&(BackingImageObject->Properties.FileSize),
                    &BackingImageSize);

    ASSERT(FileSize < BackingImageSize);

    Status =  IopModifyFileObjectSize(BackingImageObject,
                                      BackingImage->DeviceContext,
                                      FileSize);

    return Status;
}

KSTATUS
IopUnlinkSharedMemoryObject (
    PFILE_OBJECT FileObject,
    PBOOL Unlinked
    )

/*++

Routine Description:

    This routine unlinks a shared memory object from the accessible namespace.

Arguments:

    FileObject - Supplies a pointer to the file object that owns the shared
        memory object.

    Unlinked - Supplies a pointer to a boolean that receives whether or not the
        shared memory object was successfully unlinked.

Return Value:

    Status code.

--*/

{

    PSHARED_MEMORY_OBJECT SharedMemoryObject;
    KSTATUS Status;

    ASSERT(FileObject->Properties.Type == IoObjectSharedMemoryObject);
    ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock) != FALSE);

    SharedMemoryObject = FileObject->SpecialIo;
    *Unlinked = FALSE;
    Status = ObUnlinkObject(SharedMemoryObject);
    if (KSUCCESS(Status)) {

        ASSERT(SharedMemoryObject->FileObject == FileObject);

        IopFileObjectReleaseReference(SharedMemoryObject->FileObject);
        SharedMemoryObject->FileObject = NULL;
        *Unlinked = TRUE;
    }

    return Status;
}

KSTATUS
IopPerformSharedMemoryIoOperation (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine performs a non-cached I/O operation on a shared memory object.
    It is assumed that the file lock is held. This routine will always modify
    the file size in the the file properties and conditionally modify the file
    size in the file object.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code.

--*/

{

    ULONGLONG FileSize;
    PSHARED_MEMORY_OBJECT SharedMemoryObject;
    KSTATUS Status;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT(KeIsSharedExclusiveLockHeld(FileObject->Lock) != FALSE);

    SharedMemoryObject = FileObject->SpecialIo;

    //
    // There must be a backing image as long as the shared memory object is
    // alive.
    //

    ASSERT(SharedMemoryObject->BackingImage != INVALID_HANDLE);

    //
    // If this is a read operation then read from the backing image.
    //

    if (IoContext->Write == FALSE) {
        Status = IoReadAtOffset(SharedMemoryObject->BackingImage,
                                IoContext->IoBuffer,
                                IoContext->Offset,
                                IoContext->SizeInBytes,
                                IoContext->Flags,
                                IoContext->TimeoutInMilliseconds,
                                &(IoContext->BytesCompleted),
                                NULL);

        if (!KSUCCESS(Status)) {
            goto PerformSharedMemoryIoOperationEnd;
        }

    //
    // Write operations get passed onto the file that backs the shared memory
    // object. If there is no such file, then create one, save the handle, and
    // then unlink it.
    //

    } else {
        Status = IoWriteAtOffset(SharedMemoryObject->BackingImage,
                                 IoContext->IoBuffer,
                                 IoContext->Offset,
                                 IoContext->SizeInBytes,
                                 IoContext->Flags,
                                 IoContext->TimeoutInMilliseconds,
                                 &(IoContext->BytesCompleted),
                                 NULL);

        if (!KSUCCESS(Status)) {
            goto PerformSharedMemoryIoOperationEnd;
        }

        //
        // If bytes were written, then update the file size of the shared
        // memory object.
        //

        if (IoContext->BytesCompleted != 0) {
            FileSize = IoContext->Offset + IoContext->BytesCompleted;
            IopUpdateFileObjectFileSize(FileObject, FileSize);
        }
    }

PerformSharedMemoryIoOperationEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions

VOID
IopDestroySharedMemoryObject (
    PVOID Object
    )

/*++

Routine Description:

    This routine destroys the given shared memory object.

Arguments:

    Object - Supplies a pointer to the shared memory object to be destroyed.

Return Value:

    None.

--*/

{

    PSHARED_MEMORY_OBJECT SharedMemoryObject;

    SharedMemoryObject = (PSHARED_MEMORY_OBJECT)Object;
    if (SharedMemoryObject->BackingImage != INVALID_HANDLE) {
        IoIoHandleReleaseReference(SharedMemoryObject->BackingImage);
        SharedMemoryObject->BackingImage = INVALID_HANDLE;
    }

    ASSERT(SharedMemoryObject->FileObject == NULL);

    return;
}

KSTATUS
IopAddSharedMemoryObjectPathPrefix (
    PSTR Path,
    ULONG PathLength,
    PSTR *NewPath,
    PULONG NewPathLength
    )

/*++

Routine Description:

    This routine adds the shared memory object directory's path as a prefix to
    the given path.

Arguments:

    Path - Supplies a pointer to a path to a shared memory object.

    PathLength - Supplies the length of the path.

    NewPath - Supplies a pointer that receives a pointer to the new, complete
        path. The caller is expected to release this memory.

    NewPathLength - Supplies a pointer that receives the length of the newly
        allocated path.

Return Value:

    Status code.

--*/

{

    PSTR SharedMemoryPath;
    ULONG SharedMemoryPathLength;
    KSTATUS Status;

    ASSERT(Path != NULL);
    ASSERT(PathLength != 0);

    SharedMemoryPath = ObGetFullPath(IoSharedMemoryObjectDirectory,
                                     IO_ALLOCATION_TAG);

    if (SharedMemoryPath == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddSharedMemoryObjectPathPrefixEnd;
    }

    SharedMemoryPathLength = RtlStringLength(SharedMemoryPath) + 1;
    Status = IoPathAppend(SharedMemoryPath,
                          SharedMemoryPathLength,
                          Path,
                          PathLength,
                          IO_ALLOCATION_TAG,
                          NewPath,
                          NewPathLength);

    if (!KSUCCESS(Status)) {
        goto AddSharedMemoryObjectPathPrefixEnd;
    }

AddSharedMemoryObjectPathPrefixEnd:
    if (SharedMemoryPath != NULL) {
        MmFreePagedPool(SharedMemoryPath);
    }

    return Status;
}

