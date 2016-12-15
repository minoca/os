/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

//
// Store a pointer to the shared memory object root path point.
//

PATH_POINT IoSharedMemoryRoot;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IopInitializeSharedMemoryObjectSupport (
    VOID
    )

/*++

Routine Description:

    This routine is called during system initialization to set up support for
    shared memory objects.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    PFILE_OBJECT FileObject;
    PVOID Object;
    PPATH_ENTRY PathEntry;
    FILE_PROPERTIES Properties;
    KSTATUS Status;

    FileObject = NULL;

    //
    // Create the shared memory object directory.
    //

    Object = ObCreateObject(ObjectDirectory,
                            NULL,
                            "SharedMemoryObjects",
                            sizeof("SharedMemoryObjects"),
                            sizeof(OBJECT_HEADER),
                            NULL,
                            OBJECT_FLAG_USE_NAME_DIRECTLY,
                            FI_ALLOCATION_TAG);

    if (Object == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeSharedMemoryObjectSupportEnd;
    }

    //
    // Open a path point to this shared memory object root.
    //

    IopFillOutFilePropertiesForObject(&Properties, Object);
    Status = IopCreateOrLookupFileObject(&Properties,
                                         ObGetRootObject(),
                                         FILE_OBJECT_FLAG_EXTERNAL_IO_STATE,
                                         &FileObject,
                                         &Created);

    if (!KSUCCESS(Status)) {
        goto InitializeSharedMemoryObjectSupportEnd;
    }

    ASSERT(Created != FALSE);

    KeSignalEvent(FileObject->ReadyEvent, SignalOptionSignalAll);
    PathEntry = IopCreatePathEntry(NULL, 0, 0, NULL, FileObject);
    if (PathEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeSharedMemoryObjectSupportEnd;
    }

    ASSERT(IoPathPointRoot.MountPoint != NULL);

    IoSharedMemoryRoot.PathEntry = PathEntry;
    IoSharedMemoryRoot.MountPoint = IoPathPointRoot.MountPoint;
    IoMountPointAddReference(IoSharedMemoryRoot.MountPoint);

InitializeSharedMemoryObjectSupportEnd:
    if (!KSUCCESS(Status)) {

        //
        // Release once for the file properties and once for the create.
        //

        if (Object != NULL) {
            ObReleaseReference(Object);
            ObReleaseReference(Object);
        }

        if (FileObject != NULL) {
            IopFileObjectReleaseReference(FileObject);
        }
    }

    return Status;
}

PPATH_POINT
IopGetSharedMemoryDirectory (
    BOOL FromKernelMode
    )

/*++

Routine Description:

    This routine returns the current process' shared memory directory. This is
    the only place the current process is allowed to create shared memory
    objects.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

Return Value:

    Returns a pointer to the process' shared memory directory.

--*/

{

    PPATH_POINT Directory;
    PKPROCESS Process;

    //
    // The shared memory object directory can only be changed by
    // single-threaded processes. Thus the path lock does not need to be held.
    //

    if (FromKernelMode != FALSE) {
        Process = PsGetKernelProcess();

    } else {
        Process = PsGetCurrentProcess();
    }

    Directory = (PPATH_POINT)&(Process->Paths.SharedMemoryDirectory);
    if (Directory->PathEntry == NULL) {
        Directory = &IoSharedMemoryRoot;
    }

    return Directory;
}

KSTATUS
IopCreateSharedMemoryObject (
    BOOL FromKernelMode,
    PCSTR Name,
    ULONG NameSize,
    ULONG Flags,
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    )

/*++

Routine Description:

    This routine actually creates a new shared memory object.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

    Name - Supplies an optional pointer to the shared memory object name. This
        is only used for shared memory objects created in the shared memory
        directory.

    NameSize - Supplies the size of the name in bytes including the null
        terminator.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Create - Supplies a pointer to the creation parameters.

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
    FILE_ID FileId;
    PSTR FileName;
    ULONG FileNameLength;
    ULONG FileOpenFlags;
    FILE_PROPERTIES FileProperties;
    PIO_HANDLE Handle;
    ULONG MaxFileNameLength;
    PFILE_OBJECT NewFileObject;
    PSHARED_MEMORY_OBJECT NewSharedMemoryObject;
    POBJECT_HEADER ObjectDirectory;
    PSTR Path;
    ULONG PathLength;
    ULONG RetryCount;
    PPATH_POINT SharedMemoryDirectory;
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
    // Get the shared memory object directory for the process.
    //

    SharedMemoryDirectory = IopGetSharedMemoryDirectory(FromKernelMode);

    ASSERT(SharedMemoryDirectory->PathEntry->FileObject->Properties.Type ==
           IoObjectObjectDirectory);

    FileId = SharedMemoryDirectory->PathEntry->FileObject->Properties.FileId;
    ObjectDirectory = (POBJECT_HEADER)(UINTN)FileId;

    //
    // Make sure there is not already an existing shared memory object by the
    // same name. The caller should have the appropriate locks to make the
    // check and create synchronous.
    //

    if (Name != NULL) {
        ExistingObject = ObFindObject(Name, NameSize, ObjectDirectory);
        if (ExistingObject != NULL) {
            ObReleaseReference(ExistingObject);
            Status = STATUS_FILE_EXISTS;
            goto CreateSharedMemoryObjectEnd;
        }
    }

    NewSharedMemoryObject = ObCreateObject(ObjectSharedMemoryObject,
                                           ObjectDirectory,
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

        FileProperties.Permissions = Create->Permissions;
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

    Create->Created = TRUE;
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
    PFILE_OBJECT FileObject,
    ULONGLONG NewSize
    )

/*++

Routine Description:

    This routine truncates a shared memory object. It assumes that the file's
    lock is held exclusively.

Arguments:

    FileObject - Supplies a pointer to the file object that owns the shared
        memory object.

    NewSize - Supplies the new size to set.

Return Value:

    Status code.

--*/

{

    PIO_HANDLE BackingImage;
    PFILE_OBJECT BackingImageObject;
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
    Status = IopModifyFileObjectSize(BackingImageObject,
                                     BackingImage->DeviceContext,
                                     NewSize);

    if (KSUCCESS(Status)) {
        WRITE_INT64_SYNC(&(FileObject->Properties.FileSize), NewSize);
    }

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

