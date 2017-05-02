/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pipe.c

Abstract:

    This module implements support for pipes.

Author:

    Evan Green 25-Apr-2013

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
// Define pipe flags.
//

//
// This flag is set if the pipe has a name in the object manager directory.
// Note that normal named pipes coming from the file system do not have this
// flag set.
//

#define PIPE_FLAG_OBJECT_NAMED 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a data pipe.

Members:

    Header - Stores the standard object header.

    Flags - Stores the flags used when the pipe was created.

    StreamBuffer - Stores a pointer to the stream buffer backing the pipe.

    ReaderCount - Stores the number of readers that have the pipe open.

    WriterCount - Stores the number of writers that have the pipe open.

--*/

typedef struct _PIPE {
    OBJECT_HEADER Header;
    ULONG Flags;
    PSTREAM_BUFFER StreamBuffer;
    ULONG ReaderCount;
    ULONG WriterCount;
} PIPE, *PPIPE;

/*++

Structure Description:

    This structure defines the parameters needed to create a pipe.

Members:

    BufferSize - Stores the suggested size for the internal stream buffer.
        Supply 0 to use the system default size.

--*/

typedef struct _PIPE_CREATION_PARAMETERS {
    ULONG BufferSize;
} PIPE_CREATION_PARAMETERS, *PPIPE_CREATION_PARAMETERS;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
IopDestroyPipe (
    PVOID PipeObject
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the pipes directory.
//

POBJECT_HEADER IoPipeDirectory;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoCreatePipe (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG OpenFlags,
    FILE_PERMISSIONS CreatePermissions,
    PIO_HANDLE *ReadHandle,
    PIO_HANDLE *WriteHandle
    )

/*++

Routine Description:

    This routine creates and opens a new pipe.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether this request is
        originating from kernel mode (and should use the root path as a base)
        or user mode.

    Directory - Supplies an optional pointer to an open handle to a directory
        for relative paths. Supply NULL to use the current working directory.

    Path - Supplies an optional pointer to the path to open.

    PathLength - Supplies the length of the path buffer in bytes, including the
        null terminator.

    OpenFlags - Supplies the open flags for the pipe. See OPEN_FLAG_*
        definitions. OPEN_FLAG_CREATE and OPEN_FLAG_FAIL_IF_EXISTS are
        automatically applied.

    CreatePermissions - Supplies the permissions to apply to the created pipe.

    ReadHandle - Supplies a pointer where a handle to the read side of the pipe
        will be returned.

    WriteHandle - Supplies a pointer where a handle to the write side of the
        pipe will be returned.

Return Value:

    Status code.

--*/

{

    CREATE_PARAMETERS Create;
    KSTATUS Status;

    *ReadHandle = NULL;
    *WriteHandle = NULL;

    //
    // Create and open the read side.
    //

    Create.Type = IoObjectPipe;
    Create.Context = NULL;
    Create.Permissions = CreatePermissions;
    Create.Created = FALSE;
    Status = IopOpen(FromKernelMode,
                     Directory,
                     Path,
                     PathLength,
                     IO_ACCESS_READ,
                     OpenFlags | OPEN_FLAG_CREATE | OPEN_FLAG_FAIL_IF_EXISTS,
                     &Create,
                     ReadHandle);

    if (!KSUCCESS(Status)) {
        goto CreatePipeEnd;
    }

    //
    // Also open the write side.
    //

    Status = IopOpenPathPoint(&((*ReadHandle)->PathPoint),
                              IO_ACCESS_WRITE,
                              OpenFlags,
                              WriteHandle);

    if (!KSUCCESS(Status)) {
        goto CreatePipeEnd;
    }

CreatePipeEnd:
    if (!KSUCCESS(Status)) {
        if (*ReadHandle != NULL) {
            IoClose(*ReadHandle);
            *ReadHandle = NULL;
        }

        if (*WriteHandle != NULL) {
            IoClose(*WriteHandle);
            *WriteHandle = NULL;
        }
    }

    return Status;
}

POBJECT_HEADER
IopGetPipeDirectory (
    VOID
    )

/*++

Routine Description:

    This routine returns the pipe root directory in the object system. This is
    the only place in the object system pipe creation is allowed.

Arguments:

    None.

Return Value:

    Returns a pointer to the pipe directory.

--*/

{

    return IoPipeDirectory;
}

KSTATUS
IopCreatePipe (
    PCSTR Name,
    ULONG NameSize,
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    )

/*++

Routine Description:

    This routine actually creates a new pipe.

Arguments:

    Name - Supplies an optional pointer to the pipe name. This is only used for
        named pipes created in the pipe directory.

    NameSize - Supplies the size of the name in bytes including the null
        terminator.

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where a pointer to a newly created pipe
        file object will be returned on success.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    PPIPE ExistingPipe;
    FILE_PROPERTIES FileProperties;
    PFILE_OBJECT NewFileObject;
    PPIPE NewPipe;
    KSTATUS Status;
    PKTHREAD Thread;

    NewFileObject = NULL;
    NewPipe = NULL;

    //
    // Make sure there is not already an existing pipe by the same name. The
    // caller should have the appropriate locks to make the check and create
    // synchronous.
    //

    if (Name != NULL) {
        ExistingPipe = ObFindObject(Name, NameSize, IoPipeDirectory);
        if (ExistingPipe != NULL) {
            ObReleaseReference(ExistingPipe);
            Status = STATUS_FILE_EXISTS;
            goto CreatePipeEnd;
        }
    }

    //
    // Create the actual object. This reference is transferred to the file
    // object's special I/O member on success.
    //

    NewPipe = ObCreateObject(ObjectPipe,
                             IoPipeDirectory,
                             Name,
                             NameSize,
                             sizeof(PIPE),
                             IopDestroyPipe,
                             0,
                             IO_ALLOCATION_TAG);

    if (NewPipe == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePipeEnd;
    }

    //
    // Record if the pipe got a name in the pipe directory.
    //

    if (Name != NULL) {
        NewPipe->Flags |= PIPE_FLAG_OBJECT_NAMED;
    }

    //
    // Create a file object if needed.
    //

    if (*FileObject == NULL) {
        Thread = KeGetCurrentThread();
        IopFillOutFilePropertiesForObject(&FileProperties, &(NewPipe->Header));
        FileProperties.Permissions = Create->Permissions;
        FileProperties.Type = IoObjectPipe;
        FileProperties.UserId = Thread->Identity.EffectiveUserId;
        FileProperties.GroupId = Thread->Identity.EffectiveGroupId;
        Status = IopCreateOrLookupFileObject(&FileProperties,
                                             ObGetRootObject(),
                                             0,
                                             0,
                                             &NewFileObject,
                                             &Created);

        if (!KSUCCESS(Status)) {

            //
            // Release the references added by filling out the file properties.
            //

            ObReleaseReference(NewPipe);
            goto CreatePipeEnd;
        }

        ASSERT(Created != FALSE);

        *FileObject = NewFileObject;
    }

    ASSERT((*FileObject)->Properties.Type == IoObjectPipe);

    //
    // Now fill in the pipe with the I/O object state.
    //

    ASSERT((*FileObject)->IoState != NULL);

    NewPipe->StreamBuffer = IoCreateStreamBuffer((*FileObject)->IoState,
                                                 0,
                                                 0,
                                                 PIPE_ATOMIC_WRITE_SIZE);

    if (NewPipe->StreamBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePipeEnd;
    }

    //
    // Now that the pipe's ready, release anyone else who happened to find this
    // file object in the mean time.
    //

    ASSERT(((*FileObject)->SpecialIo == NULL) &&
           ((KeGetEventState((*FileObject)->ReadyEvent) == NotSignaled) ||
            (KeGetEventState((*FileObject)->ReadyEvent) ==
             NotSignaledWithWaiters)));

    (*FileObject)->SpecialIo = NewPipe;
    NewPipe = NULL;
    Create->Created = TRUE;
    Status = STATUS_SUCCESS;

CreatePipeEnd:

    //
    // On both success and failure, the file object's ready event needs to be
    // signaled. Other threads may be waiting on the event.
    //

    if (*FileObject != NULL) {
        KeSignalEvent((*FileObject)->ReadyEvent, SignalOptionSignalAll);
    }

    if (!KSUCCESS(Status)) {
        if (NewFileObject != NULL) {
            *FileObject = NULL;
            IopFileObjectReleaseReference(NewFileObject);
        }

        if (NewPipe != NULL) {
            ObReleaseReference(NewPipe);
            NewPipe = NULL;
        }
    }

    return Status;
}

KSTATUS
IopUnlinkPipe (
    PFILE_OBJECT FileObject,
    PBOOL Unlinked
    )

/*++

Routine Description:

    This routine unlinks a pipe from the accessible namespace.

Arguments:

    FileObject - Supplies a pointer to the pipe's file object.

    Unlinked - Supplies a pointer to a boolean that receives whether or not the
        terminal was successfully unlinked.

Return Value:

    Status code.

--*/

{

    PPIPE Pipe;
    KSTATUS Status;

    ASSERT(FileObject->Properties.Type == IoObjectPipe);
    ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock) != FALSE);

    Pipe = FileObject->SpecialIo;

    ASSERT(Pipe != NULL);
    ASSERT((Pipe->Flags & PIPE_FLAG_OBJECT_NAMED) != 0);

    *Unlinked = FALSE;
    Status = ObUnlinkObject(Pipe);
    if (KSUCCESS(Status)) {
        *Unlinked = TRUE;
    }

    return Status;
}

KSTATUS
IopOpenPipe (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine is called when a pipe is opened.

Arguments:

    IoHandle - Supplies a pointer to the new I/O handle.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PIO_OBJECT_STATE IoState;
    BOOL NonBlocking;
    PPIPE Pipe;
    BOOL PipeOpened;
    ULONG ReturnedEvents;
    KSTATUS Status;

    PipeOpened = FALSE;
    FileObject = IoHandle->FileObject;

    ASSERT(FileObject->Properties.Type == IoObjectPipe);

    KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
    Pipe = FileObject->SpecialIo;
    if (Pipe == NULL) {

        ASSERT(FALSE);

        Status = STATUS_TOO_LATE;
        goto OpenPipeEnd;
    }

    if ((IoHandle->Access & IO_ACCESS_EXECUTE) != 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto OpenPipeEnd;
    }

    IoState = IoStreamBufferGetIoObjectState(Pipe->StreamBuffer);
    if ((IoHandle->Access & IO_ACCESS_READ) != 0) {
        Pipe->ReaderCount += 1;

        //
        // Clear the error event.
        //

        IoSetIoObjectState(IoState, POLL_EVENT_ERROR, FALSE);
    }

    if ((IoHandle->Access & IO_ACCESS_WRITE) != 0) {
        Pipe->WriterCount += 1;

        //
        // Clear the disconnect event.
        //

        IoSetIoObjectState(IoState, POLL_EVENT_DISCONNECTED, FALSE);
    }

    PipeOpened = TRUE;

    //
    // Determine whether this is a blocking or non-blocking open. The initial
    // create/open call is also set non-blocking, and this relies a bit on the
    // fact that the read end is opened first.
    //

    NonBlocking = FALSE;
    if (((IoHandle->OpenFlags & OPEN_FLAG_NON_BLOCKING) != 0) ||
        ((IoHandle->OpenFlags &
          (OPEN_FLAG_CREATE | OPEN_FLAG_FAIL_IF_EXISTS)) ==
         (OPEN_FLAG_CREATE | OPEN_FLAG_FAIL_IF_EXISTS))) {

        NonBlocking = TRUE;
    }

    //
    // In non-blocking mode, open access for write only returns an error if no
    // process currently has the pipe open for reading.
    //

    if (NonBlocking != FALSE) {
        if (((IoHandle->Access & IO_ACCESS_WRITE) != 0) &&
            (Pipe->ReaderCount == 0)) {

            Status = STATUS_NO_SUCH_DEVICE_OR_ADDRESS;
            goto OpenPipeEnd;
        }

    //
    // Handle a blocking open on a pipe, which blocks until the other end
    // connects.
    //

    } else {

        //
        // If there's no jelly for your peanut buffer, wait for some to arrive.
        // Borrow the write event to block on.
        //

        if ((((IoHandle->Access & IO_ACCESS_WRITE) != 0) &&
             (Pipe->ReaderCount == 0)) ||
            (((IoHandle->Access & IO_ACCESS_READ) != 0) &&
             (Pipe->WriterCount == 0))) {

            IoSetIoObjectState(IoState, POLL_EVENT_OUT, FALSE);
            KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
            Status = IoWaitForIoObjectState(IoState,
                                            POLL_EVENT_OUT | POLL_EVENT_ERROR,
                                            TRUE,
                                            WAIT_TIME_INDEFINITE,
                                            &ReturnedEvents);

            KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
            if (!KSUCCESS(Status)) {
                goto OpenPipeEnd;
            }

            if ((ReturnedEvents & POLL_EVENT_OUT) == 0) {
                Status = STATUS_NOT_READY;
                goto OpenPipeEnd;
            }
        }
    }

    //
    // Reset the I/O object state, which sets the in and out poll events
    // properly.
    //

    Status = IoStreamBufferConnect(Pipe->StreamBuffer);
    if (!KSUCCESS(Status)) {
        goto OpenPipeEnd;
    }

    Status = STATUS_SUCCESS;

OpenPipeEnd:
    KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
    if (!KSUCCESS(Status)) {
        if (PipeOpened != FALSE) {
            IopClosePipe(IoHandle);
        }
    }

    return Status;
}

KSTATUS
IopClosePipe (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine is called when a pipe is closed.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle being closed.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PFILE_PROPERTIES FileProperties;
    PIO_OBJECT_STATE IoState;
    BOOL LockHeld;
    PPIPE Pipe;

    FileObject = IoHandle->FileObject;
    FileProperties = &(FileObject->Properties);

    ASSERT(FileProperties->Type == IoObjectPipe);

    KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
    LockHeld = TRUE;
    Pipe = FileObject->SpecialIo;
    IoState = IoStreamBufferGetIoObjectState(Pipe->StreamBuffer);
    if ((IoHandle->Access & IO_ACCESS_READ) != 0) {
        Pipe->ReaderCount -= 1;
        if (Pipe->ReaderCount == 0) {

            //
            // The last reader just closed, so clear the hangup event and the
            // out event. Set the error event.
            //

            IoSetIoObjectState(IoState,
                               POLL_EVENT_OUT | POLL_EVENT_DISCONNECTED,
                               FALSE);

            IoSetIoObjectState(IoState, POLL_EVENT_ERROR, TRUE);
        }
    }

    if ((IoHandle->Access & IO_ACCESS_WRITE) != 0) {
        Pipe->WriterCount -= 1;
        if (Pipe->WriterCount == 0) {

            //
            // Clear the out event, set the hangup event, and set the read
            // event.
            //

            IoSetIoObjectState(IoState, POLL_EVENT_OUT, FALSE);
            IoSetIoObjectState(IoState,
                               POLL_EVENT_DISCONNECTED | POLL_EVENT_IN,
                               TRUE);
        }
    }

    //
    // Pipes that are named in the object directory need to be unlinked on
    // the last close. Check to see if the reader and writer counts are both
    // zero. If so, unlink the object. It may be that another thread is about
    // to open the pipe for read and/or write. This is OK, it's got a reference
    // on the file object and can proceed without concern. When it closes the
    // pipe it will attempt the unlink again, but that's fine. No new lookups
    // can occur after the first unlink attempt.
    //

    if ((Pipe->Flags & PIPE_FLAG_OBJECT_NAMED) != 0) {
        if ((Pipe->WriterCount == 0) && (Pipe->ReaderCount == 0)) {
            KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
            LockHeld = FALSE;
            IopDeleteByHandle(TRUE, IoHandle, 0);
        }
    }

    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
    }

    return STATUS_SUCCESS;
}

KSTATUS
IopPerformPipeIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine reads from or writes to a pipe.

Arguments:

    Handle - Supplies a pointer to the pipe I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    PFILE_OBJECT FileObject;
    BOOL NonBlocking;
    PPIPE Pipe;
    UINTN PipeBytesCompleted;
    KSTATUS Status;

    FileObject = Handle->FileObject;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT(FileObject->Properties.Type == IoObjectPipe);

    Pipe = FileObject->SpecialIo;
    PipeBytesCompleted = 0;
    NonBlocking = FALSE;
    if (IoContext->Write != FALSE) {

        //
        // If there are no readers, send a pipe signal to the calling
        // application.
        //

        if (Pipe->ReaderCount == 0) {
            Status = STATUS_BROKEN_PIPE;

        } else {
            Status = IoWriteStreamBuffer(Pipe->StreamBuffer,
                                         IoContext->IoBuffer,
                                         IoContext->SizeInBytes,
                                         IoContext->TimeoutInMilliseconds,
                                         NonBlocking,
                                         &PipeBytesCompleted);
        }

    } else {
        if (Pipe->WriterCount == 0) {
            NonBlocking = TRUE;
        }

        Status = IoReadStreamBuffer(Pipe->StreamBuffer,
                                    IoContext->IoBuffer,
                                    IoContext->SizeInBytes,
                                    IoContext->TimeoutInMilliseconds,
                                    NonBlocking,
                                    &PipeBytesCompleted);

        if ((Status == STATUS_TRY_AGAIN) && (Pipe->WriterCount == 0)) {

            ASSERT(PipeBytesCompleted == 0);

            Status = STATUS_END_OF_FILE;
        }
    }

    IoContext->BytesCompleted = PipeBytesCompleted;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
IopDestroyPipe (
    PVOID PipeObject
    )

/*++

Routine Description:

    This routine destroys all resources associated with a pipe.

Arguments:

    PipeObject - Supplies a pointer to the pipe object being destroyed.

Return Value:

    None.

--*/

{

    PPIPE Pipe;

    Pipe = (PPIPE)PipeObject;
    if (Pipe->StreamBuffer != NULL) {
        IoDestroyStreamBuffer(Pipe->StreamBuffer);
    }

    return;
}

