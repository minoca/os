/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    flock.c

Abstract:

    This module implements support for user mode file locking in the kernel.

Author:

    Evan Green 29-Apr-2014

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

/*++

Structure Description:

    This structure defines an active file lock.

Members:

    ListEntry - Stores pointers to the next and previous lock entries in the
        file object.

    Type - Stores the lock type.

    Process - Stores a pointer to the process that owns the file lock.

    Offset - Stores the offset into the file where the lock begins.

    Size - Stores the size of the lock. If zero, then the lock extends to the
        end of the file.

--*/

typedef struct _FILE_LOCK_ENTRY {
    LIST_ENTRY ListEntry;
    FILE_LOCK_TYPE Type;
    PKPROCESS Process;
    ULONGLONG Offset;
    ULONGLONG Size;
} FILE_LOCK_ENTRY, *PFILE_LOCK_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopTryToSetFileLock (
    PFILE_OBJECT FileObject,
    PFILE_LOCK_ENTRY NewEntry,
    PLIST_ENTRY FreeList,
    BOOL DryRun
    );

BOOL
IopDoFileLocksOverlap (
    ULONGLONG IncomingOffset,
    ULONGLONG IncomingSize,
    PFILE_LOCK_ENTRY LockEntry
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IopGetFileLock (
    PIO_HANDLE IoHandle,
    PFILE_LOCK Lock
    )

/*++

Routine Description:

    This routine gets information about a file lock. Existing locks are not
    reported if they are compatible with making a new lock in the given region.
    So set the lock type to write if both read and write locks should be
    reported.

Arguments:

    IoHandle - Supplies a pointer to the open I/O handle.

    Lock - Supplies a pointer to the lock information.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PFILE_OBJECT FileObject;
    PFILE_LOCK_ENTRY FoundEntry;
    PFILE_LOCK_ENTRY LockEntry;
    BOOL Overlap;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if ((Lock->Type != FileLockRead) && (Lock->Type != FileLockReadWrite)) {
        return STATUS_INVALID_PARAMETER;
    }

    FileObject = IoHandle->FileObject;
    FoundEntry = NULL;
    KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
    CurrentEntry = FileObject->FileLockList.Next;
    while (CurrentEntry != &(FileObject->FileLockList)) {
        LockEntry = LIST_VALUE(CurrentEntry, FILE_LOCK_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // If the caller only wants write locks, then skip read locks.
        //

        if ((Lock->Type == FileLockRead) && (LockEntry->Type == FileLockRead)) {
            continue;
        }

        Overlap = IopDoFileLocksOverlap(Lock->Offset, Lock->Size, LockEntry);
        if (Overlap != FALSE) {
            FoundEntry = LockEntry;
            break;
        }
    }

    if (FoundEntry != NULL) {
        Lock->Type = FoundEntry->Type;
        Lock->Offset = FoundEntry->Offset;
        Lock->Size = FoundEntry->Size;
        Lock->ProcessId = FoundEntry->Process->Identifiers.ProcessId;

    } else {
        Lock->Type = FileLockUnlock;
    }

    KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
    return STATUS_SUCCESS;
}

KSTATUS
IopSetFileLock (
    PIO_HANDLE IoHandle,
    PFILE_LOCK Lock,
    BOOL Blocking
    )

/*++

Routine Description:

    This routine locks or unlocks a portion of a file. If the process already
    has a lock on any part of the region, the old lock is replaced with this
    new region. Remove a lock by specifying a lock type of unlock.

Arguments:

    IoHandle - Supplies a pointer to the open I/O handle.

    Lock - Supplies a pointer to the lock information.

    Blocking - Supplies a boolean indicating if this should block until a
        determination is made.

Return Value:

    Status code.

--*/

{

    PFILE_LOCK_ENTRY Entry;
    PFILE_OBJECT FileObject;
    LIST_ENTRY FreeList;
    BOOL LockHeld;
    PFILE_LOCK_ENTRY NewEntry;
    PKEVENT NewEvent;
    PKEVENT PreviousEvent;
    FILE_LOCK_ENTRY RemoveEntry;
    PFILE_LOCK_ENTRY SplitEntry;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    FileObject = IoHandle->FileObject;
    INITIALIZE_LIST_HEAD(&FreeList);
    NewEntry = NULL;
    SplitEntry = NULL;
    LockHeld = FALSE;
    if ((Lock->Type == FileLockInvalid) || (Lock->Type >= FileLockTypeCount)) {
        Status = STATUS_INVALID_PARAMETER;
        goto SetFileLockEnd;
    }

    //
    // Use a stack allocated entry if things are being unlocked.
    //

    if (Lock->Type == FileLockUnlock) {
        NewEntry = &RemoveEntry;

    } else {

        //
        // Check to make sure the handle has the appropriate permissions.
        //

        if (Lock->Type == FileLockRead) {
            if ((IoHandle->Access & IO_ACCESS_READ) == 0) {
                Status = STATUS_ACCESS_DENIED;
                goto SetFileLockEnd;
            }

        } else {

            ASSERT(Lock->Type == FileLockReadWrite);

            if ((IoHandle->Access & IO_ACCESS_WRITE) == 0) {
                Status = STATUS_ACCESS_DENIED;
                goto SetFileLockEnd;
            }
        }

        NewEntry = MmAllocateNonPagedPool(sizeof(FILE_LOCK_ENTRY),
                                          FILE_LOCK_ALLOCATION_TAG);

        if (NewEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetFileLockEnd;
        }
    }

    NewEntry->Type = Lock->Type;
    NewEntry->Offset = Lock->Offset;
    NewEntry->Size = Lock->Size;
    NewEntry->Process = PsGetCurrentProcess();
    SplitEntry = MmAllocateNonPagedPool(sizeof(FILE_LOCK_ENTRY),
                                        FILE_LOCK_ALLOCATION_TAG);

    if (SplitEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SetFileLockEnd;
    }

    INSERT_BEFORE(&(SplitEntry->ListEntry), &FreeList);

    //
    // Race to create the event if not created.
    //

    if (FileObject->FileLockEvent == NULL) {
        NewEvent = KeCreateEvent(NULL);
        if (NewEvent == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetFileLockEnd;
        }

        PreviousEvent = (PKEVENT)RtlAtomicCompareExchange(
                                          (PUINTN)&(FileObject->FileLockEvent),
                                          (UINTN)NewEvent,
                                          (UINTN)NULL);

        if (PreviousEvent != NULL) {
            KeDestroyEvent(NewEvent);
        }
    }

    while (TRUE) {
        if (LockHeld == FALSE) {
            KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
            LockHeld = TRUE;
        }

        //
        // If this really is setting a lock, do a dry run to see if this would
        // work.
        //

        if (Lock->Type != FileLockUnlock) {
            Status = IopTryToSetFileLock(FileObject,
                                         NewEntry,
                                         NULL,
                                         TRUE);

            if (!KSUCCESS(Status)) {

                //
                // Wait for something to unlock.
                //

                if (Blocking != FALSE) {
                    KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
                    LockHeld = FALSE;
                    Status = KeWaitForEvent(FileObject->FileLockEvent,
                                            TRUE,
                                            WAIT_TIME_INDEFINITE);

                    //
                    // The thread was interrupted.
                    //

                    if (!KSUCCESS(Status)) {
                        if (Status == STATUS_INTERRUPTED) {
                            Status = STATUS_RESTART_AFTER_SIGNAL;
                        }

                        goto SetFileLockEnd;
                    }

                    continue;

                //
                // Not blocking, the dry run was the only attempt.
                //

                } else {
                    break;
                }
            }
        }

        //
        // Do this for real. This should not fail, as any failures should
        // have happened during the dry run.
        //

        Status = IopTryToSetFileLock(FileObject, NewEntry, &FreeList, FALSE);

        ASSERT(KSUCCESS(Status));

        NewEntry = NULL;
        break;
    }

SetFileLockEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
    }

    if ((NewEntry != NULL) && (NewEntry != &RemoveEntry)) {
        MmFreeNonPagedPool(NewEntry);
    }

    while (LIST_EMPTY(&FreeList) == FALSE) {
        Entry = LIST_VALUE(FreeList.Next, FILE_LOCK_ENTRY, ListEntry);
        LIST_REMOVE(&(Entry->ListEntry));
        MmFreeNonPagedPool(Entry);
    }

    return Status;
}

VOID
IopRemoveFileLocks (
    PIO_HANDLE IoHandle,
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine destroys any locks the given process has on the file object
    pointed to by the given I/O handle. This routine is called anytime any
    file descriptor is closed by a process, even if other file descriptors
    to the same file in the process remain open.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle being closed.

    Process - Supplies the process closing the handle.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PFILE_OBJECT FileObject;
    LIST_ENTRY FreeList;
    PFILE_LOCK_ENTRY LockEntry;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Exit quickly if there are no file locks.
    //

    FileObject = IoHandle->FileObject;
    if (LIST_EMPTY(&(FileObject->FileLockList)) != FALSE) {
        return;
    }

    INITIALIZE_LIST_HEAD(&FreeList);
    KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);

    //
    // Loop through and remove any active locks belonging to this process.
    //

    CurrentEntry = FileObject->FileLockList.Next;
    while (CurrentEntry != &(FileObject->FileLockList)) {
        LockEntry = LIST_VALUE(CurrentEntry, FILE_LOCK_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (LockEntry->Process == Process) {
            LIST_REMOVE(&(LockEntry->ListEntry));
            INSERT_BEFORE(&(LockEntry->ListEntry), &FreeList);
        }
    }

    //
    // If locks were removed, signal anyone blocked on this file.
    //

    if (LIST_EMPTY(&FreeList) == FALSE) {
        KeSignalEvent(FileObject->FileLockEvent, SignalOptionSignalAll);
    }

    KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);

    //
    // Free any removed entries now that the lock is released.
    //

    while (LIST_EMPTY(&FreeList) == FALSE) {
        LockEntry = LIST_VALUE(FreeList.Next, FILE_LOCK_ENTRY, ListEntry);
        LIST_REMOVE(&(LockEntry->ListEntry));
        MmFreeNonPagedPool(LockEntry);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopTryToSetFileLock (
    PFILE_OBJECT FileObject,
    PFILE_LOCK_ENTRY NewEntry,
    PLIST_ENTRY FreeList,
    BOOL DryRun
    )

/*++

Routine Description:

    This routine attempts to lock or unlocks a portion of a file. If the
    process already has a lock on any part of the region, the old lock is
    replaced with this new region. Remove a lock by specifying a lock type of
    unlock. This routine assumes the file properties lock is already held.

Arguments:

    FileObject - Supplies a pointer to the file object.

    NewEntry - Supplies a pointer to the new lock to add.

    FreeList - Supplies a pointer to a list head that on input contains one
        free entry, needed to potentially split an entry. On output, entries
        that need to be freed will be put on this list.

    DryRun - Supplies a pointer indicating if the lock should actually be
        created/destroyed or just checked.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PFILE_LOCK_ENTRY LockEntry;
    BOOL LocksRemoved;
    ULONGLONG NewEnd;
    BOOL Overlap;
    PKPROCESS Process;
    PFILE_LOCK_ENTRY SplitEntry;
    KSTATUS Status;

    LocksRemoved = FALSE;
    NewEnd = NewEntry->Offset + NewEntry->Size;
    Process = PsGetCurrentProcess();
    Status = STATUS_SUCCESS;
    CurrentEntry = FileObject->FileLockList.Next;
    while (CurrentEntry != &(FileObject->FileLockList)) {
        LockEntry = LIST_VALUE(CurrentEntry, FILE_LOCK_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // If the lock belongs to the current process and overlaps the given
        // region, it is to be removed.
        //

        if (LockEntry->Process == Process) {
            Overlap = IopDoFileLocksOverlap(NewEntry->Offset,
                                            NewEntry->Size,
                                            LockEntry);

            if ((Overlap != FALSE) && (DryRun == FALSE)) {
                LocksRemoved = TRUE;

                //
                // If the existing entry starts before the new one, it needs to
                // be shrunk or split.
                //

                if (LockEntry->Offset < NewEntry->Offset) {

                    //
                    // If it ends after the new one, split it.
                    //

                    if ((NewEntry->Size != 0) &&
                        ((LockEntry->Offset + LockEntry->Size > NewEnd) ||
                         (LockEntry->Size == 0))) {

                        ASSERT(LIST_EMPTY(FreeList) == FALSE);

                        SplitEntry = LIST_VALUE(FreeList->Next,
                                                FILE_LOCK_ENTRY,
                                                ListEntry);

                        LIST_REMOVE(&(SplitEntry->ListEntry));
                        SplitEntry->Type = LockEntry->Type;
                        SplitEntry->Offset = NewEnd;
                        if (LockEntry->Size == 0) {
                            SplitEntry->Size = 0;

                        } else {
                            SplitEntry->Size = LockEntry->Offset +
                                               LockEntry->Size - NewEnd;
                        }

                        INSERT_AFTER(&(SplitEntry->ListEntry),
                                     &(LockEntry->ListEntry));
                    }

                    //
                    // Shrink its length.
                    //

                    LockEntry->Size = NewEntry->Offset - LockEntry->Offset;

                //
                // The current entry starts within the new entry. If it
                // ends after the new entry, shrink it.
                //

                } else if ((NewEntry->Size != 0) &&
                           ((LockEntry->Offset + LockEntry->Size > NewEnd) ||
                            (LockEntry->Size == 0))) {

                    if (LockEntry->Size != 0) {
                        LockEntry->Size = LockEntry->Offset + LockEntry->Size -
                                          NewEnd;
                    }

                    LockEntry->Offset = NewEnd;

                //
                // The new entry completely swallows the existing one.
                //

                } else {
                    LIST_REMOVE(&(LockEntry->ListEntry));
                    INSERT_BEFORE(&(LockEntry->ListEntry), FreeList);
                }
            }

        //
        // Another process owns this lock.
        //

        } else if (NewEntry->Type != FileLockUnlock) {

            //
            // Read locks can coexist.
            //

            if ((NewEntry->Type == FileLockRead) &&
                (LockEntry->Type == FileLockRead)) {

                continue;
            }

            //
            // If the file lock overlaps with the incoming one, then fail.
            //

            Overlap = IopDoFileLocksOverlap(NewEntry->Offset,
                                            NewEntry->Size,
                                            LockEntry);

            //
            // This routine should not be discovering overlaps on the real
            // deal.
            //

            ASSERT((Overlap == FALSE) || (DryRun != FALSE));

            if (Overlap != FALSE) {
                Status = STATUS_RESOURCE_IN_USE;
                KeSignalEvent(FileObject->FileLockEvent, SignalOptionUnsignal);
                break;
            }
        }
    }

    //
    // Add the new entry if conditions are right.
    //

    if ((KSUCCESS(Status)) && (DryRun == FALSE) &&
        (NewEntry->Type != FileLockUnlock)) {

        INSERT_AFTER(&(NewEntry->ListEntry), &(FileObject->FileLockList));
    }

    if (LocksRemoved != FALSE) {
        KeSignalEvent(FileObject->FileLockEvent, SignalOptionSignalAll);
    }

    return Status;
}

BOOL
IopDoFileLocksOverlap (
    ULONGLONG IncomingOffset,
    ULONGLONG IncomingSize,
    PFILE_LOCK_ENTRY LockEntry
    )

/*++

Routine Description:

    This routine checks to see if the given lock entry overlaps with the
    incoming lock. The lock types are not checked by this routine, only the
    regions.

Arguments:

    IncomingOffset - Supplies the file offset of the incoming lock.

    IncomingSize - Supplies the file size of the incoming lock.

    LockEntry - Supplies a pointer to the existing lock entry.

Return Value:

    TRUE if the lock entry overlaps the incoming lock.

    FALSE if the lock entry does not overlap.

--*/

{

    //
    // If the incoming lock goes to the end of the file, then this lock
    // overlaps if it has an offset greater than the incoming one or its
    // size is also zero.
    //

    if (IncomingSize == 0) {
        if ((LockEntry->Size == 0) || (LockEntry->Offset >= IncomingOffset)) {
            return TRUE;
        }

    //
    // The incoming lock spans a specific region. This lock overlaps if
    // one of these conditions is met:
    // 1. Size == 0 and the offset starts before the incoming lock's end.
    // 2. Offset <= IncomingOffset and Offset + Size > IncomingOffset.
    // 3. Offset > IncomingOffset and Offset < IncomingOffset + IncomingSize.
    //

    } else {
        if (LockEntry->Size == 0) {
            if (LockEntry->Offset < IncomingOffset + IncomingSize) {
                return TRUE;
            }

        } else if (LockEntry->Offset <= IncomingOffset) {
            if (LockEntry->Offset + LockEntry->Size > IncomingOffset) {
                return TRUE;
            }

        } else {
            if (LockEntry->Offset < IncomingOffset + IncomingSize) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

