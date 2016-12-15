/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    obfs.c

Abstract:

    This module implements support for certain file I/O operations on object
    manager objects.

Author:

    Evan Green 26-Apr-2013

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

#define INITIAL_OBJECT_NAME_BUFFER_SIZE 128

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IopPerformObjectIoOperation (
    PIO_HANDLE IoHandle,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine reads from an object directory.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    LONGLONG AdvanceCount;
    UINTN BytesRead;
    POBJECT_HEADER Child;
    PFILE_OBJECT ChildFileObject;
    PATH_POINT ChildPathPoint;
    PLIST_ENTRY CurrentEntry;
    DIRECTORY_ENTRY DirectoryEntry;
    ULONG EntrySize;
    FILE_ID FileId;
    PFILE_OBJECT FileObject;
    LONGLONG Index;
    PSTR Name;
    PSTR NameBuffer;
    ULONG NameBufferSize;
    ULONG NameSize;
    ULONG NeededSize;
    POBJECT_HEADER Object;
    RUNLEVEL OldRunLevel;
    PFILE_OBJECT ParentFileObject;
    PATH_POINT ParentPathPoint;
    PVOID PreviousChild;
    PKPROCESS Process;
    PPATH_POINT Root;
    KSTATUS Status;

    ASSERT(IoContext->IoBuffer != NULL);

    if (IoContext->Write != FALSE) {
        return STATUS_NOT_SUPPORTED;
    }

    if (IoContext->Offset != IO_OFFSET_NONE) {
        Index = IoContext->Offset;

    } else {
        Index = RtlAtomicOr64((PULONGLONG)&(IoHandle->CurrentOffset), 0);
    }

    BytesRead = 0;
    Child = NULL;
    ChildFileObject = NULL;
    FileObject = IoHandle->FileObject;
    NameBuffer = NULL;
    Object = (POBJECT_HEADER)(UINTN)(FileObject->Properties.FileId);

    ASSERT(FileObject->Properties.Type == IoObjectObjectDirectory);
    ASSERT(FileObject->Properties.DeviceId == OBJECT_MANAGER_DEVICE_ID);

    if ((IoHandle->OpenFlags & OPEN_FLAG_DIRECTORY) == 0) {
        Status = STATUS_FILE_IS_DIRECTORY;
        goto PerformObjectIoOperationEnd;
    }

    //
    // The . and .. entries always come first.
    //

    while (Index < 2) {
        if (Index == 0) {
            Name = ".";
            NameSize = sizeof(".");
            FileId = FileObject->Properties.FileId;

        } else {
            Name = "..";
            NameSize = sizeof("..");

            //
            // Carefully get the ID of ".." - don't just get the parent object
            // as this object directory might be mounted in the middle of some
            // file system that is not owned by the object manager.
            //

            Root = NULL;
            Process = PsGetCurrentProcess();
            if (Process->Paths.Root.PathEntry != NULL) {
                Root = (PPATH_POINT)&(Process->Paths.Root);
            }

            IopGetParentPathPoint(Root,
                                  &(IoHandle->PathPoint),
                                  &ParentPathPoint);

            ParentFileObject = ParentPathPoint.PathEntry->FileObject;
            FileId = ParentFileObject->Properties.FileId;
            IO_PATH_POINT_RELEASE_REFERENCE(&ParentPathPoint);
        }

        EntrySize = ALIGN_RANGE_UP(sizeof(DIRECTORY_ENTRY) + NameSize, 8);
        if (BytesRead + EntrySize > IoContext->SizeInBytes) {
            Status = STATUS_MORE_PROCESSING_REQUIRED;
            goto PerformObjectIoOperationEnd;
        }

        DirectoryEntry.Type = IoObjectRegularDirectory;
        DirectoryEntry.Size = EntrySize;
        DirectoryEntry.FileId = FileId;
        Status = MmCopyIoBufferData(IoContext->IoBuffer,
                                    &DirectoryEntry,
                                    BytesRead,
                                    sizeof(DIRECTORY_ENTRY),
                                    TRUE);

        if (!KSUCCESS(Status)) {
            goto PerformObjectIoOperationEnd;
        }

        Status = MmCopyIoBufferData(IoContext->IoBuffer,
                                    Name,
                                    BytesRead + sizeof(DIRECTORY_ENTRY),
                                    NameSize,
                                    TRUE);

        if (!KSUCCESS(Status)) {
            goto PerformObjectIoOperationEnd;
        }

        Index += 1;
        BytesRead += EntrySize;
    }

    //
    // Iterate through the object's children. This requires some song and
    // dance because the destination buffer is paged, but the object name is
    // non-paged and requires holding a dispatch level lock.
    //

    NameBufferSize = INITIAL_OBJECT_NAME_BUFFER_SIZE;
    NameBuffer = MmAllocateNonPagedPool(NameBufferSize, IO_ALLOCATION_TAG);
    if (NameBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PerformObjectIoOperationEnd;
    }

    CurrentEntry = &(Object->ChildListHead);
    PreviousChild = NULL;

    //
    // Really it should be advancing by the next index plus one from the head,
    // but the first two were . and .., so it's minus one.
    //

    AdvanceCount = Index - 1;
    while (TRUE) {
        NeededSize = 0;

        //
        // Lock the parent and advance through the list, skipping any nameless
        // ones.
        //

        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&(Object->WaitQueue.Lock));
        while (AdvanceCount > 0) {
            CurrentEntry = CurrentEntry->Next;
            while (CurrentEntry != &(Object->ChildListHead)) {
                Child = LIST_VALUE(CurrentEntry, OBJECT_HEADER, SiblingEntry);
                if (Child->Name != NULL) {
                    break;
                }

                CurrentEntry = CurrentEntry->Next;
            }

            if (CurrentEntry == &(Object->ChildListHead)) {
                break;
            }

            AdvanceCount -= 1;
        }

        //
        // Copy the file into the non-paged buffer if it's big enough. Add a
        // reference to the child so the child doesn't disappear while the
        // lock is dropped.
        //

        if (CurrentEntry != &(Object->ChildListHead)) {
            NeededSize = Child->NameLength;
            ObAddReference(Child);
            if (NeededSize <= NameBufferSize) {
                RtlStringCopy(NameBuffer, Child->Name, NameBufferSize);
            }

        } else {
            Child = NULL;
        }

        KeReleaseSpinLock(&(Object->WaitQueue.Lock));
        KeLowerRunLevel(OldRunLevel);

        //
        // Release the reference on the previous child now that the parent
        // lock isn't held
        //

        if (PreviousChild != NULL) {
            ObReleaseReference(PreviousChild);
        }

        //
        // Stop if the child list ended.
        //

        if (Child == NULL) {
            break;
        }

        //
        // If the child is a device or a volume and it has been removed or is
        // awaiting removal, do not report it.
        //

        if ((Child->Type == ObjectDevice) || (Child->Type == ObjectVolume)) {
            if ((((PDEVICE)Child)->State == DeviceRemoved) ||
                (((PDEVICE)Child)->State == DeviceAwaitingRemoval)) {

                PreviousChild = Child;
                Index += 1;
                AdvanceCount = 1;
                continue;
            }
        }

        //
        // Look for the correct file ID for the child using its name. This will
        // either find an existing entry or actually go to the appropriate
        // device to perform a root lookup.
        //

        FileId = (FILE_ID)(UINTN)Child;
        Status = IopPathLookup(TRUE,
                               NULL,
                               &(IoHandle->PathPoint),
                               FALSE,
                               Child->Name,
                               Child->NameLength,
                               OPEN_FLAG_DIRECTORY,
                               NULL,
                               &ChildPathPoint);

        if (KSUCCESS(Status)) {
            ChildFileObject = ChildPathPoint.PathEntry->FileObject;
            FileId = ChildFileObject->Properties.FileId;
        }

        if (ChildPathPoint.PathEntry != NULL) {
            IO_PATH_POINT_RELEASE_REFERENCE(&ChildPathPoint);
        }

        //
        // If the buffer is too small, allocate a new one. Always at least
        // double the previous size to at least try and avoid doing this every
        // time. Then leave the advance count at zero to try the same object
        // again.
        //

        if (NeededSize > NameBufferSize) {
            MmFreeNonPagedPool(NameBuffer);
            while (NameBufferSize < NeededSize) {
                NameBufferSize *= 2;
            }

            NameBuffer = MmAllocateNonPagedPool(NameBufferSize,
                                                IO_ALLOCATION_TAG);

            if (NameBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto PerformObjectIoOperationEnd;
            }

        //
        // Otherwise, copy the entry in and advance.
        //

        } else {
            NameSize = NeededSize;
            EntrySize = ALIGN_RANGE_UP(sizeof(DIRECTORY_ENTRY) + NameSize, 8);
            if (BytesRead + EntrySize > IoContext->SizeInBytes) {
                Status = STATUS_MORE_PROCESSING_REQUIRED;
                goto PerformObjectIoOperationEnd;
            }

            DirectoryEntry.Size = EntrySize;
            DirectoryEntry.FileId = FileId;
            DirectoryEntry.Type = IoObjectRegularDirectory;
            Status = MmCopyIoBufferData(IoContext->IoBuffer,
                                        &DirectoryEntry,
                                        BytesRead,
                                        sizeof(DIRECTORY_ENTRY),
                                        TRUE);

            if (!KSUCCESS(Status)) {
                goto PerformObjectIoOperationEnd;
            }

            Status = MmCopyIoBufferData(IoContext->IoBuffer,
                                        NameBuffer,
                                        BytesRead + sizeof(DIRECTORY_ENTRY),
                                        NameSize,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                goto PerformObjectIoOperationEnd;
            }

            BytesRead += EntrySize;
            Index += 1;
            AdvanceCount = 1;
        }

        PreviousChild = Child;
    }

    Status = STATUS_SUCCESS;

PerformObjectIoOperationEnd:
    if (Child != NULL) {
        ObReleaseReference(Child);
    }

    if (NameBuffer != NULL) {
        MmFreeNonPagedPool(NameBuffer);
    }

    if (IoContext->Offset == IO_OFFSET_NONE) {
        RtlAtomicExchange64((PULONGLONG)&(IoHandle->CurrentOffset), Index);
    }

    IoContext->BytesCompleted = BytesRead;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

