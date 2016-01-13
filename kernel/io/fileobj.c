/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    fileobj.c

Abstract:

    This module implements support routines for working with file objects.

Author:

    Evan Green 25-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

#define FILE_OBJECT_ALLOCATION_TAG 0x624F6946 // 'bOiF'
#define FILE_OBJECT_MAX_REFERENCE_COUNT 0x10000000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
IopRemoveFailedFileObjects (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    ULONG Level,
    PVOID Context
    );

COMPARISON_RESULT
IopCompareFileObjectNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

PFILE_OBJECT
IopLookupFileObjectByProperties (
    PFILE_PROPERTIES Properties
    );

KSTATUS
IopFlushFileObjectPropertiesIterator (
    PFILE_OBJECT FileObject,
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global tree of file objects.
//

RED_BLACK_TREE IoFileObjectsTree;

//
// Store the global list of dirty file objects.
//

LIST_ENTRY IoFileObjectsDirtyList;

//
// Store the global list of orphaned file objects.
//

LIST_ENTRY IoFileObjectsOrphanedList;

//
// Store a queued lock that protects both the tree and the list.
//

PQUEUED_LOCK IoFileObjectsLock;

//
// Store a queued lock to synchronized flushing file objects.
//

PQUEUED_LOCK IoFlushLock;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
VOID
IoSetIoObjectState (
    PIO_OBJECT_STATE IoState,
    ULONG Events,
    BOOL Set
    )

/*++

Routine Description:

    This routine sets or clears one or more events in the I/O object state.

Arguments:

    IoState - Supplies a pointer to the I/O object state to change.

    Events - Supplies a mask of poll events to change. See POLL_EVENT_*
        definitions.

    Set - Supplies a boolean indicating if the event(s) should be set (TRUE) or
        cleared (FALSE).

Return Value:

    None.

--*/

{

    SIGNAL_OPTION SignalOption;

    //
    // Prepare to signal the events. The events mask must be updated before an
    // event is signaled as it may immediately be read by a waiter.
    //

    if (Set != FALSE) {
        SignalOption = SignalOptionSignalAll;
        RtlAtomicOr32(&(IoState->Events), Events);

    } else {
        SignalOption = SignalOptionUnsignal;
        RtlAtomicAnd32(&(IoState->Events), ~Events);
    }

    if ((Events & POLL_EVENT_IN) != 0) {
        KeSignalEvent(IoState->ReadEvent, SignalOption);
    }

    if (((Events & POLL_EVENT_IN_HIGH_PRIORITY) != 0) &&
        (IoState->ReadHighPriorityEvent != NULL)) {

        KeSignalEvent(IoState->ReadHighPriorityEvent, SignalOption);
    }

    if ((Events & POLL_EVENT_OUT) != 0) {
        KeSignalEvent(IoState->WriteEvent, SignalOption);
    }

    if (((Events & POLL_EVENT_OUT_HIGH_PRIORITY) != 0) &&
        (IoState->WriteHighPriorityEvent != NULL)) {

        KeSignalEvent(IoState->WriteHighPriorityEvent, SignalOption);
    }

    if ((Events & POLL_ERROR_EVENTS) != 0) {
        KeSignalEvent(IoState->ErrorEvent, SignalOption);
    }

    return;
}

KERNEL_API
KSTATUS
IoWaitForIoObjectState (
    PIO_OBJECT_STATE IoState,
    ULONG Events,
    BOOL Interruptible,
    ULONG TimeoutInMilliseconds,
    PULONG ReturnedEvents
    )

/*++

Routine Description:

    This routine waits for the given events to trigger on the I/O object state.

Arguments:

    IoState - Supplies a pointer to the I/O object state to wait on.

    Events - Supplies a mask of poll events to wait for. See POLL_EVENT_*
        definitions. Errors are non-maskable and will always be returned.

    Interruptible - Supplies a boolean indicating whether or not the wait can
        be interrupted if a signal is sent to the process on which this thread
        runs. If TRUE is supplied, the caller must check the return status
        code to find out if the wait was really satisfied or just interrupted.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        objects should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on these objects.

    ReturnedEvents - Supplies an optional pointer where the poll events that
        satisfied the wait will be returned on success. If the wait was
        interrupted this will return 0.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONG WaitFlags;
    PVOID WaitObjectArray[5];
    ULONG WaitObjectCount;

    if (ReturnedEvents != NULL) {
        *ReturnedEvents = 0;
    }

    WaitFlags = 0;
    if (Interruptible != FALSE) {
        WaitFlags |= WAIT_FLAG_INTERRUPTIBLE;
    }

    //
    // Always wait on the error state.
    //

    WaitObjectCount = 0;
    WaitObjectArray[WaitObjectCount] = IoState->ErrorEvent;
    WaitObjectCount += 1;

    //
    // Determine which I/O state events to wait on.
    //

    if ((Events & POLL_EVENT_IN) != 0) {
        WaitObjectArray[WaitObjectCount] = IoState->ReadEvent;
        WaitObjectCount += 1;
    }

    if ((Events & POLL_EVENT_IN_HIGH_PRIORITY) != 0) {
        if (IoState->ReadHighPriorityEvent == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto WaitForIoObjectStateEnd;
        }

        WaitObjectArray[WaitObjectCount] = IoState->ReadHighPriorityEvent;
        WaitObjectCount += 1;
    }

    if ((Events & POLL_EVENT_OUT) != 0) {
        WaitObjectArray[WaitObjectCount] = IoState->WriteEvent;
        WaitObjectCount += 1;
    }

    if ((Events & POLL_EVENT_OUT_HIGH_PRIORITY) != 0) {
        if (IoState->WriteHighPriorityEvent == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto WaitForIoObjectStateEnd;
        }

        WaitObjectArray[WaitObjectCount] = IoState->WriteHighPriorityEvent;
        WaitObjectCount += 1;
    }

    Status = ObWaitOnObjects(WaitObjectArray,
                             WaitObjectCount,
                             WaitFlags,
                             TimeoutInMilliseconds,
                             NULL,
                             NULL);

    if (!KSUCCESS(Status)) {
        goto WaitForIoObjectStateEnd;
    }

    //
    // The I/O object state maintains a bitmask of all the currently signaled
    // poll events. AND this with the requested events to get the returned
    // events for this descriptor.
    //

    if (ReturnedEvents != NULL) {
        *ReturnedEvents = IoState->Events & (Events | POLL_NONMASKABLE_EVENTS);
    }

WaitForIoObjectStateEnd:
    return Status;
}

KERNEL_API
PIO_OBJECT_STATE
IoCreateIoObjectState (
    BOOL HighPriority
    )

/*++

Routine Description:

    This routine creates a new I/O object state structure with a reference
    count of one.

Arguments:

    HighPriority - Supplies a boolean indicating whether or not the I/O object
        state should be prepared for high priority events.

Return Value:

    Returns a pointer to the new state structure on success.

    NULL on allocation failure.

--*/

{

    PIO_OBJECT_STATE NewState;
    KSTATUS Status;

    //
    // Create the I/O state structure.
    //

    Status = STATUS_INSUFFICIENT_RESOURCES;
    NewState = MmAllocatePagedPool(sizeof(IO_OBJECT_STATE),
                                   FILE_OBJECT_ALLOCATION_TAG);

    if (NewState == NULL) {
        goto CreateIoObjectStateEnd;
    }

    RtlZeroMemory(NewState, sizeof(IO_OBJECT_STATE));

    //
    // Create the events and lock.
    //

    NewState->ReadEvent = KeCreateEvent(NULL);
    if (NewState->ReadEvent == NULL) {
        goto CreateIoObjectStateEnd;
    }

    NewState->WriteEvent = KeCreateEvent(NULL);
    if (NewState->WriteEvent == NULL) {
        goto CreateIoObjectStateEnd;
    }

    NewState->ErrorEvent = KeCreateEvent(NULL);
    if (NewState->ErrorEvent == NULL) {
        goto CreateIoObjectStateEnd;
    }

    if (HighPriority != FALSE) {
        NewState->ReadHighPriorityEvent = KeCreateEvent(NULL);
        if (NewState->ReadHighPriorityEvent == NULL) {
            goto CreateIoObjectStateEnd;
        }

        NewState->WriteHighPriorityEvent = KeCreateEvent(NULL);
        if (NewState->WriteHighPriorityEvent == NULL) {
            goto CreateIoObjectStateEnd;
        }
    }

    Status = STATUS_SUCCESS;

CreateIoObjectStateEnd:
    if (!KSUCCESS(Status)) {
        if (NewState != NULL) {
            IoDestroyIoObjectState(NewState);
            NewState = NULL;
        }
    }

    return NewState;
}

KERNEL_API
VOID
IoDestroyIoObjectState (
    PIO_OBJECT_STATE State
    )

/*++

Routine Description:

    This routine destroys the given I/O object state.

Arguments:

    State - Supplies a pointer to the I/O object state to destroy.

Return Value:

    None.

--*/

{

    if (State->ReadEvent != NULL) {
        KeDestroyEvent(State->ReadEvent);
    }

    if (State->ReadHighPriorityEvent != NULL) {
        KeDestroyEvent(State->ReadHighPriorityEvent);
    }

    if (State->WriteEvent != NULL) {
        KeDestroyEvent(State->WriteEvent);
    }

    if (State->WriteHighPriorityEvent != NULL) {
        KeDestroyEvent(State->WriteHighPriorityEvent);
    }

    if (State->ErrorEvent != NULL) {
        KeDestroyEvent(State->ErrorEvent);
    }

    MmFreePagedPool(State);
    return;
}

PVOID
IoReferenceFileObjectForHandle (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine returns an opaque pointer to the file object opened by the
    given handle. It also adds a reference to the file object, which the caller
    is responsible for freeing.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle whose underlying file
        object should be referenced.

Return Value:

    Returns an opaque pointer to the file object, with an incremented reference
    count. The caller is responsible for releasing this reference.

--*/

{

    PFILE_OBJECT FileObject;

    FileObject = IoHandle->PathPoint.PathEntry->FileObject;
    IopFileObjectAddReference(FileObject);
    return FileObject;
}

VOID
IoFileObjectReleaseReference (
    PVOID FileObject
    )

/*++

Routine Description:

    This routine releases an external reference on a file object taken by
    referencing the file object for a handle.

Arguments:

    FileObject - Supplies the opaque pointer to the file object.

Return Value:

    None. The caller should not count on this pointer remaining unique after
    this call returns.

--*/

{

    KSTATUS Status;

    Status = IopFileObjectReleaseReference(FileObject, FALSE);

    ASSERT(KSUCCESS(Status));

    return;
}

KSTATUS
IopInitializeFileObjectSupport (
    VOID
    )

/*++

Routine Description:

    This routine performs global initialization for file object support.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    RtlRedBlackTreeInitialize(&IoFileObjectsTree, 0, IopCompareFileObjectNodes);
    INITIALIZE_LIST_HEAD(&IoFileObjectsDirtyList);
    INITIALIZE_LIST_HEAD(&IoFileObjectsOrphanedList);
    IoFileObjectsLock = KeCreateQueuedLock();
    if (IoFileObjectsLock == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    IoFlushLock = KeCreateQueuedLock();
    if (IoFlushLock == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
}

KSTATUS
IopCreateOrLookupFileObject (
    PFILE_PROPERTIES Properties,
    PDEVICE Device,
    ULONG Flags,
    PFILE_OBJECT *FileObject,
    PBOOL ObjectCreated
    )

/*++

Routine Description:

    This routine attempts to look up a file object with the given properties
    (specifically the I-Node number and volume). If one does not exist, it
    is created and inserted in the global list. If a special file object is
    created, the ready event is left unsignaled so the remainder of the state
    can be created.

Arguments:

    Properties - Supplies a pointer to the file object properties.

    Device - Supplies a pointer to the device that owns the file serial number.
        This may also be a volume or an object directory.

    Flags - Supplies a bitmask of file object flags. See FILE_OBJECT_FLAG_* for
        definitions.

    FileObject - Supplies a pointer where the file object will be returned on
        success.

    ObjectCreated - Supplies a pointer where a boolean will be returned
        indicating whether or not the object was just created. If it was just
        created, the caller is responsible for signaling the ready event when
        the object is fully set up.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    ULONGLONG FileSize;
    BOOL LockHeld;
    PFILE_OBJECT NewObject;
    PFILE_OBJECT Object;
    KSTATUS Status;

    ASSERT(Properties->DeviceId != 0);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    Created = FALSE;
    LockHeld = FALSE;
    NewObject = NULL;
    Object = NULL;
    while (TRUE) {

        //
        // See if the file object already exists.
        //

        KeAcquireQueuedLock(IoFileObjectsLock);
        LockHeld = TRUE;
        Object = IopLookupFileObjectByProperties(Properties);
        if (Object == NULL) {

            //
            // There's no object, so drop the lock and go allocate one.
            //

            KeReleaseQueuedLock(IoFileObjectsLock);
            LockHeld = FALSE;
            if (NewObject == NULL) {
                NewObject = MmAllocatePagedPool(sizeof(FILE_OBJECT),
                                                FILE_OBJECT_ALLOCATION_TAG);

                if (NewObject == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto CreateOrLookupFileObjectEnd;
                }

                RtlZeroMemory(NewObject, sizeof(FILE_OBJECT));
                KeInitializeSpinLock(&(NewObject->PropertiesLock));
                INITIALIZE_LIST_HEAD(&(NewObject->FileLockList));
                RtlRedBlackTreeInitialize(&(NewObject->PageCacheEntryTree),
                                          0,
                                          IopComparePageCacheEntries);

                if ((Properties->Type != IoObjectBlockDevice) &&
                    (Properties->Type != IoObjectCharacterDevice)) {

                    NewObject->Lock = KeCreateSharedExclusiveLock();
                    if (NewObject->Lock == NULL) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto CreateOrLookupFileObjectEnd;
                    }
                }

                if ((Flags & FILE_OBJECT_FLAG_EXTERNAL_IO_STATE) == 0) {
                    NewObject->IoState = IoCreateIoObjectState(FALSE);
                    if (NewObject->IoState == NULL) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto CreateOrLookupFileObjectEnd;
                    }
                }

                NewObject->ReadyEvent = KeCreateEvent(NULL);
                if (NewObject->ReadyEvent == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto CreateOrLookupFileObjectEnd;
                }

                NewObject->Flags = Flags;
                NewObject->Device = Device;
                ObAddReference(Device);

                //
                // If the device is a special device, then more state needs to
                // be set up. Don't let additional lookups come in and use the
                // object before it's completely set up.
                //

                switch (Properties->Type) {
                case IoObjectPipe:
                case IoObjectSocket:
                case IoObjectTerminalMaster:
                case IoObjectTerminalSlave:
                case IoObjectSharedMemoryObject:
                    break;

                default:
                    KeSignalEvent(NewObject->ReadyEvent, SignalOptionSignalAll);
                    break;
                }

                ASSERT(NewObject->ImageSectionList == NULL);

                //
                // Each file object starts with two references: one for the
                // caller, and one for being in the tree. When the reference
                // count reaches one, begin the process of flushing the file
                // object to disk. When that's done, it's removed from the
                // tree, and the second reference is released.
                //

                NewObject->ReferenceCount = 2;
                RtlCopyMemory(&(NewObject->Properties),
                              Properties,
                              sizeof(FILE_PROPERTIES));

                //
                // Copy the file size out of the properties and into the file
                // object proper. The system will update the version in the
                // file object as it modifies the file size and then will
                // reflect any changes to the file properties version when the
                // bytes actually reach the device.
                //

                READ_INT64_SYNC(&(Properties->FileSize), &FileSize);
                WRITE_INT64_SYNC(&(NewObject->FileSize), FileSize);
            }

            //
            // It's time to insert it into the tree. Someone may have already
            // added this entry since the lock was dropped, so check once more.
            //

            LockHeld = TRUE;
            KeAcquireQueuedLock(IoFileObjectsLock);
            Object = IopLookupFileObjectByProperties(Properties);
            if (Object == NULL) {
                RtlRedBlackTreeInsert(&IoFileObjectsTree,
                                      &(NewObject->TreeEntry));

                ASSERT(NewObject->ListEntry.Next == NULL);

                Object = NewObject;
                NewObject = NULL;
                Created = TRUE;
            }
        }

        KeReleaseQueuedLock(IoFileObjectsLock);
        LockHeld = FALSE;

        //
        // If the object was created, it's the caller's responsibility to get it
        // ready, so don't wait on the event.
        //

        if (Created != FALSE) {
            break;
        }

        //
        // Wait on the file object to become ready.
        //

        KeWaitForEvent(Object->ReadyEvent, FALSE, WAIT_TIME_INDEFINITE);

        //
        // If the file object is closing, then it's too late. Release this
        // reference and try again.
        //

        if ((Object->Flags & FILE_OBJECT_FLAG_CLOSING) != 0) {
            IopFileObjectReleaseReference(Object, FALSE);
            Object = NULL;
            continue;
        }

        break;
    }

    Status = STATUS_SUCCESS;

    ASSERT(Object->Device == Device);

CreateOrLookupFileObjectEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(IoFileObjectsLock);
    }

    if (!KSUCCESS(Status)) {
        if (Object != NULL) {
            IopFileObjectReleaseReference(Object, FALSE);
            Object = NULL;
        }
    }

    if (NewObject != NULL) {

        ASSERT(NewObject->ListEntry.Next == NULL);

        if (NewObject->Lock != NULL) {
            KeDestroySharedExclusiveLock(NewObject->Lock);
        }

        if (NewObject->IoState != NULL) {
            IoDestroyIoObjectState(NewObject->IoState);
        }

        if (NewObject->ReadyEvent != NULL) {
            KeDestroyEvent(NewObject->ReadyEvent);
        }

        ObReleaseReference(NewObject->Device);
        MmFreePagedPool(NewObject);
    }

    *FileObject = Object;
    if (ObjectCreated != NULL) {
        *ObjectCreated = Created;
    }

    return Status;
}

ULONG
IopFileObjectAddReference (
    PFILE_OBJECT Object
    )

/*++

Routine Description:

    This routine increments the reference count on a file object.

Arguments:

    Object - Supplies a pointer to the object to retain.

Return Value:

    Returns the reference count before the addition.

--*/

{

    ULONG OldCount;

    OldCount = RtlAtomicAdd32(&(Object->ReferenceCount), 1);

    ASSERT((OldCount != 0) && (OldCount < FILE_OBJECT_MAX_REFERENCE_COUNT));

    return OldCount;
}

KSTATUS
IopFileObjectReleaseReference (
    PFILE_OBJECT Object,
    BOOL FailIfLastReference
    )

/*++

Routine Description:

    This routine decrements the reference count on a file object. If the
    reference count hits zero, then the file object will be destroyed.

Arguments:

    Object - Supplies a pointer to the object to release.

    FailIfLastReference - Supplies a boolean that if set causes the reference
        count not to be decremented if this would involve releasing the very
        last reference on the file object. Callers that set this flag must be
        able to take responsibility for the reference they continue to own in
        the failure case. Set this to FALSE.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OPERATION_CANCELLED if the caller passed in the fail if last
    reference flag and this is the final reference on the file object.

    Other error codes on failure to write out the file properties to the file
    system or device.

--*/

{

    BOOL Cancelled;
    IRP_CLOSE CloseIrp;
    PDEVICE Device;
    IRP_MINOR_CODE MinorCode;
    ULONG OldCount;
    KSTATUS Status;

    Cancelled = FALSE;
    Status = STATUS_SUCCESS;

    //
    // Acquire the lock before decrementing the reference count. This is needed
    // to make the "decrement reference count, signal event, set closing"
    // operation atomic. If it weren't, people could increment the reference
    // count thinking the file object was good to use, and then this function
    // would close it down on them. It's assumed that people calling add
    // reference on the file object already had some other valid reference,
    // otherwise the global lock would have to be acquired in the add reference
    // routine as well.
    //

    KeAcquireQueuedLock(IoFileObjectsLock);
    OldCount = RtlAtomicAdd32(&(Object->ReferenceCount), -1);

    ASSERT((OldCount != 0) && (OldCount < FILE_OBJECT_MAX_REFERENCE_COUNT));

    //
    // If this is the second to last reference, then the only one left is the
    // internal one. Begin the cleanup process. Because it is the last
    // reference, modification of the file object's flags does not need to use
    // the atomic AND and OR operations.
    //

    if (OldCount == 2) {

        //
        // If someone else is already in the middle of closing, just roll on
        // through, releasing this reference.
        //

        if ((Object->Flags & FILE_OBJECT_FLAG_CLOSING) != 0) {
            KeReleaseQueuedLock(IoFileObjectsLock);
            goto FileObjectReleaseReferenceEnd;
        }

        //
        // If the caller doesn't want to release the last reference, bump the
        // reference count back up and cancel the operation. This is done
        // under the lock to prevent new folks from coming in and finding it.
        //

        if (FailIfLastReference != FALSE) {
            RtlAtomicAdd32(&(Object->ReferenceCount), 1);
            KeReleaseQueuedLock(IoFileObjectsLock);
            Cancelled = TRUE;
            Status = STATUS_OPERATION_CANCELLED;
            goto FileObjectReleaseReferenceEnd;
        }

        //
        // Unsignal the ready event to pause anyone trying to open this file
        // object or delete lingering failed objects.
        //

        KeSignalEvent(Object->ReadyEvent, SignalOptionUnsignal);

        //
        // Mark the object as closing and make sure it isn't marked as failed.
        // This thread is about to take responsibility of the removal and will
        // do the right thing if removal fails.
        //

        Object->Flags |= FILE_OBJECT_FLAG_CLOSING;
        Object->Flags &= ~FILE_OBJECT_FLAG_CLOSE_FAILED;

        //
        // The file object should not be on the dirty list.
        //

        ASSERT(Object->ListEntry.Next == NULL);

        //
        // Release the file object lock.
        //
        // N.B. Do not reacquire the file object lock before signaling the
        //      parties waiting on the ready event. Otherwise this might
        //      deadlock with the failed file clean-up.
        //

        KeReleaseQueuedLock(IoFileObjectsLock);

        //
        // As dirty file objects sit on the dirty file object list with a
        // reference, only clean file object can make it this far.
        //

        ASSERT((Object->Properties.HardLinkCount == 0) ||
               ((Object->Flags & FILE_OBJECT_FLAG_DIRTY_PROPERTIES) == 0));

        //
        // The file object is being destroyed, now it is safe to notify the
        // driver that the the context is no longer needed. If the file object
        // fails to close and gets re-used, the next open path will open the
        // file object again.
        //

        if ((Object->Flags & FILE_OBJECT_FLAG_OPEN) != 0) {
            Device = Object->Device;

            ASSERT(IS_DEVICE_OR_VOLUME(Device));

            CloseIrp.DeviceContext = Object->DeviceContext;
            Status = IopSendCloseIrp(Device, &CloseIrp);
            if (!KSUCCESS(Status) &&
                (Status != STATUS_DEVICE_NOT_CONNECTED)) {

                Object->Flags |= FILE_OBJECT_FLAG_CLOSE_FAILED;
                goto FileObjectReleaseReferenceEnd;
            }

            Object->DeviceContext = NULL;
            Object->Flags &= ~FILE_OBJECT_FLAG_OPEN;
            Status = STATUS_SUCCESS;
        }

        //
        // If the hard link count went to zero then delete the file object now
        // that the system can no longer reference it.
        //

        if (Object->Properties.HardLinkCount == 0) {
            MinorCode = IrpMinorSystemControlDelete;
            Status = IopSendFileOperationIrp(MinorCode, Object, NULL, 0);
            if (!KSUCCESS(Status) &&
                (Status != STATUS_DEVICE_NOT_CONNECTED)) {

                Object->Flags |= FILE_OBJECT_FLAG_CLOSE_FAILED;
                goto FileObjectReleaseReferenceEnd;
            }

            Status = STATUS_SUCCESS;
        }

        //
        // The file system is officially disengaged from this file object,
        // remove the file object from the global tree, allowing new callers to
        // recreate the file object.
        //

        KeAcquireQueuedLock(IoFileObjectsLock);
        RtlRedBlackTreeRemove(&IoFileObjectsTree, &(Object->TreeEntry));
        KeReleaseQueuedLock(IoFileObjectsLock);

        //
        // Now release everyone who got stuck while trying to open this closing
        // file object, so they can try again for a fresh version. Drop the
        // last reference. The failed file clean-up might also be waiting on
        // this event to check status.
        //

        KeSignalEvent(Object->ReadyEvent, SignalOptionSignalAll);

        ASSERT(FailIfLastReference == FALSE);

        IopFileObjectReleaseReference(Object, FALSE);

    //
    // If this is the very last reference, then actually destroy the object.
    //

    } else if (OldCount == 1) {
        KeReleaseQueuedLock(IoFileObjectsLock);

        ASSERT(Object->ListEntry.Next == NULL);
        ASSERT((Object->Flags & FILE_OBJECT_FLAG_CLOSING) != 0);
        ASSERT(Object->PathEntryCount == 0);
        ASSERT(LIST_EMPTY(&(Object->FileLockList)) != FALSE);

        //
        // If this was an object manager object, release the reference on the
        // file. The only exception here is sockets, which are not official
        // object manager objects. They get destroyed differently.
        //

        if (Object->Properties.DeviceId == OBJECT_MANAGER_DEVICE_ID) {
            if (Object->Properties.Type != IoObjectSocket) {
                ObReleaseReference((PVOID)(UINTN)Object->Properties.FileId);
            }
        }

        if (Object->SpecialIo != NULL) {
            switch (Object->Properties.Type) {
            case IoObjectSocket:
                IoSocketReleaseReference(Object->SpecialIo);
                break;

            case IoObjectPipe:
            case IoObjectTerminalMaster:
            case IoObjectTerminalSlave:
            case IoObjectSharedMemoryObject:
                ObReleaseReference(Object->SpecialIo);
                break;

            default:

                ASSERT(FALSE);

                break;
            }
        }

        //
        // Release the reference on the device.
        //

        ObReleaseReference(Object->Device);
        if (Object->ImageSectionList != NULL) {
            MmDestroyImageSectionList(Object->ImageSectionList);
        }

        if (Object->Lock != NULL) {
            KeDestroySharedExclusiveLock(Object->Lock);
        }

        if (((Object->Flags & FILE_OBJECT_FLAG_EXTERNAL_IO_STATE) == 0) &&
            (Object->IoState != NULL)) {

            IoDestroyIoObjectState(Object->IoState);
        }

        if (Object->ReadyEvent != NULL) {
            KeDestroyEvent(Object->ReadyEvent);
        }

        if (Object->FileLockEvent != NULL) {
            KeDestroyEvent(Object->FileLockEvent);
        }

        MmFreePagedPool(Object);
        Object = NULL;

    //
    // This is not the last reference to this file in the system. Just release
    // the lock, and feel a little silly for holding it in the first place.
    //

    } else {
        KeReleaseQueuedLock(IoFileObjectsLock);
    }

FileObjectReleaseReferenceEnd:

    //
    // This routine should only fail if the device fails to write or delete the
    // file object. Let anyone waiting on this file object know that it is
    // free to use.
    //

    if ((!KSUCCESS(Status)) && (Cancelled == FALSE)) {

        ASSERT((Object->Flags & FILE_OBJECT_FLAG_CLOSE_FAILED) != 0);
        ASSERT(Object->ListEntry.Next == NULL);

        //
        // If the object's reference count is still 1, add it to the list of
        // orphaned objects.
        //

        KeAcquireQueuedLock(IoFileObjectsLock);
        if (Object->ReferenceCount == 1) {
            INSERT_BEFORE(&(Object->ListEntry), &IoFileObjectsOrphanedList);
        }

        KeReleaseQueuedLock(IoFileObjectsLock);

        //
        // The signal event acts as a memory barrier still protecting this
        // non-atomic AND.
        //

        Object->Flags &= ~FILE_OBJECT_FLAG_CLOSING;
        KeSignalEvent(Object->ReadyEvent, SignalOptionSignalAll);
    }

    return Status;
}

VOID
IopFileObjectAddPathEntryReference (
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine increments the path entry reference count on a file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    None.

--*/

{

    RtlAtomicAdd32(&(FileObject->PathEntryCount), 1);
    return;
}

VOID
IopFileObjectReleasePathEntryReference (
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine decrements the path entry reference count on a file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    None.

--*/

{

    ULONG OldCount;

    OldCount = RtlAtomicAdd32(&(FileObject->PathEntryCount), (ULONG)-1);

    //
    // If this file object was deleted and this was the last path entry
    // reference then notify the page cache. It might want to evict the
    // entries.
    //

    if ((OldCount == 1) &&
        (FileObject->Properties.HardLinkCount == 0) &&
        (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE)) {

        IopMarkFileObjectDirty(FileObject);
        IopNotifyPageCacheFileDeleted();
    }

    return;
}

KSTATUS
IopFlushFileObject (
    PFILE_OBJECT FileObject,
    ULONGLONG Offset,
    ULONGLONG Size,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes all file object data to the next lowest cache layer.
    If the flags request synchronized I/O, then all file data and meta-data
    will be flushed to the backing media.

Arguments:

    FileObject - Supplies a pointer to a file object for the device or file.

    Offset - Supplies the offset from the beginning of the file or device where
        the flush should be done.

    Size - Supplies the size, in bytes, of the region to flush. Supply a value
        of -1 to flush from the given offset to the end of the file.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = IopFlushPageCacheEntries(FileObject, Offset, Size, Flags);
    if (!KSUCCESS(Status)) {
        goto FlushFileObjectEnd;
    }

    Status = IopFlushFileObjectProperties(FileObject, Flags);
    if (!KSUCCESS(Status)) {
        goto FlushFileObjectEnd;
    }

FlushFileObjectEnd:
    return Status;
}

KSTATUS
IopFlushFileObjects (
    DEVICE_ID DeviceId,
    ULONG Flags
    )

/*++

Routine Description:

    This routine iterates over file objects in the global dirty file objects
    list, flushing each one that belongs to the given device or to all entries
    if a device ID of 0 is specified.

Arguments:

    DeviceId - Supplies an optional device ID filter. Supply 0 to iterate over
        dirty file objects for all devices.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

Return Value:

    STATUS_SUCCESS if all file object were successfully iterated.

    STATUS_TRY_AGAIN if the iteration quit early for some reason (i.e. the page
    cache was found to be too dirty when flushing file objects).

    Other status codes for other errors.

--*/

{

    ULONG Count;
    PLIST_ENTRY CurrentEntry;
    PFILE_OBJECT CurrentObject;
    LIST_ENTRY DirtyListHead;
    ULONG FlushCount;
    BOOL ReleaseReference;
    KSTATUS Status;

    //
    // Synchronized flushes need to guarantee that all the data is out to disk
    // before returning. They've got be careful about the presense of other
    // threads removing dirty file objects from the list and doing work. As
    // such, they must acquire the lock to make sure all other threads are done.
    //

    FlushCount = 1;
    if ((Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) {

        //
        // If the goal is to flush the entire cache, then don't actually
        // perform the flush synchronized. Just loop twice so that the first
        // round gets all dirty data from the upper layers to the disk layer
        // and the second loop will flush it to disk. This allows for larger,
        // faster writes to disk.
        //

        if (DeviceId == 0) {
            Flags &= ~(IO_FLAG_DATA_SYNCHRONIZED |
                       IO_FLAG_METADATA_SYNCHRONIZED);

            FlushCount = 2;
        }

    //
    // Non-synchronized flushes that encounter an empty list can just exit. Any
    // necessary work is already being done. But if a specific device is
    // supplied acquire the lock to make sure any other thread has finsihed
    // flushing the device's data.
    //

    } else if ((DeviceId == 0) &&
               (LIST_EMPTY(&IoFileObjectsDirtyList) != FALSE)) {

        return STATUS_SUCCESS;
    }

    //
    // Now work to clean dirty file objects.
    //

    Status = STATUS_SUCCESS;
    Count = 0;
    KeAcquireQueuedLock(IoFlushLock);
    while (Count < FlushCount) {
        Count += 1;

        //
        // Transfer the dirty file objects to a local list.
        //

        INITIALIZE_LIST_HEAD(&DirtyListHead);
        KeAcquireQueuedLock(IoFileObjectsLock);
        if (DeviceId == 0) {
            if (LIST_EMPTY(&IoFileObjectsDirtyList) == FALSE) {
                MOVE_LIST(&IoFileObjectsDirtyList, &DirtyListHead);
                INITIALIZE_LIST_HEAD(&IoFileObjectsDirtyList);
            }

        } else {
            CurrentEntry = IoFileObjectsDirtyList.Next;
            while (CurrentEntry != &(IoFileObjectsDirtyList)) {
                CurrentObject = LIST_VALUE(CurrentEntry,
                                           FILE_OBJECT,
                                           ListEntry);

                CurrentEntry = CurrentEntry->Next;
                if (CurrentObject->Properties.DeviceId == DeviceId) {
                    LIST_REMOVE(&(CurrentObject->ListEntry));
                    INSERT_BEFORE(&(CurrentObject->ListEntry), &DirtyListHead);
                }
            }
        }

        KeReleaseQueuedLock(IoFileObjectsLock);

        //
        // Iterate over the dirty file object list. The global dirty list held
        // a reference on the file object, so release a reference after
        // processing the file object.
        //

        while (LIST_EMPTY(&DirtyListHead) == FALSE) {
            CurrentObject = LIST_VALUE(DirtyListHead.Next,
                                       FILE_OBJECT,
                                       ListEntry);

            LIST_REMOVE(&(CurrentObject->ListEntry));
            CurrentObject->ListEntry.Next = NULL;

            //
            // As the dirty list held a reference on the file object. The
            // reference count must be greater than or equal to 2.
            //

            ASSERT(CurrentObject->ReferenceCount >= 2);

            //
            // Flush the file object. If this fails, add the file object and
            // any unprocessed file objects back to the dirty list. Be careful
            // as the current file object may have been added back to the dirty
            // list by another thread or during the flush routine.
            //

            Status = IopFlushFileObject(CurrentObject, 0, -1, Flags);
            if (!KSUCCESS(Status)) {
                ReleaseReference = TRUE;
                if ((CurrentObject->ListEntry.Next == NULL) ||
                    (LIST_EMPTY(&DirtyListHead) == FALSE)) {

                    KeAcquireQueuedLock(IoFileObjectsLock);
                    if (CurrentObject->ListEntry.Next == NULL) {
                        INSERT_AFTER(&(CurrentObject->ListEntry),
                                     &DirtyListHead);

                        ReleaseReference = FALSE;
                    }

                    if (LIST_EMPTY(&DirtyListHead) == FALSE) {
                        APPEND_LIST(&DirtyListHead, &IoFileObjectsDirtyList);
                    }

                    KeReleaseQueuedLock(IoFileObjectsLock);

                    //
                    // Notify the page cache that there is work to be done. It
                    // may have just ran and found the list empty. Now that it
                    // is repopulated, reschedule the I/O.
                    //

                    if (LIST_EMPTY(&IoFileObjectsDirtyList) == FALSE) {
                        IopNotifyPageCacheWrite();
                    }
                }

                //
                // Release the old reference for the dirty file object list
                // unless it was re-inserted.
                //

                if (ReleaseReference != FALSE) {
                    IopFileObjectReleaseReference(CurrentObject, FALSE);
                }

                break;
            }

            IopFileObjectReleaseReference(CurrentObject, FALSE);
        }
    }

    KeReleaseQueuedLock(IoFlushLock);
    return Status;
}

VOID
IopEvictFileObjects (
    DEVICE_ID DeviceId,
    ULONG Flags
    )

/*++

Routine Description:

    This routine iterates over all file objects evicting page cache entries for
    each one that belongs to the given device.

Arguments:

    DeviceId - Supplies an optional device ID filter. Supply 0 to iterate over
        file objects for all devices.

    Flags - Supplies a bitmask of eviction flags. See
        PAGE_CACHE_EVICTION_FLAG_* for definitions.

Return Value:

    None.

--*/

{

    PFILE_OBJECT CurrentObject;
    PRED_BLACK_TREE_NODE Node;
    PFILE_OBJECT ReleaseObject;

    ASSERT(DeviceId != 0);

    ReleaseObject = NULL;

    //
    // Grab the global file objects lock and iterate over the file objects that
    // belong to the given device.
    //

    KeAcquireQueuedLock(IoFileObjectsLock);
    Node = RtlRedBlackTreeGetLowestNode(&IoFileObjectsTree);
    while (Node != NULL) {
        CurrentObject = RED_BLACK_TREE_VALUE(Node, FILE_OBJECT, TreeEntry);

        //
        // Skip file objects that do not match the device ID. Also skip any
        // file objects that only have 1 reference. This means that they are
        // about to get removed from the tree if close/delete are successful.
        // As such, they don't have any page cache entries, as a page cache
        // entry takes a reference on the file object.
        //

        if ((CurrentObject->Properties.DeviceId != DeviceId) ||
            (CurrentObject->ReferenceCount == 1)) {

            Node = RtlRedBlackTreeGetNextNode(&(IoFileObjectsTree),
                                              FALSE,
                                              Node);

            CurrentObject = NULL;
            continue;
        }

        //
        // Take a reference on this object so it does not disappear when the
        // lock is released.
        //

        IopFileObjectAddReference(CurrentObject);
        KeReleaseQueuedLock(IoFileObjectsLock);

        //
        // Call the eviction routine for the current file object.
        //

        IopEvictPageCacheEntries(CurrentObject, 0, Flags);

        //
        // Release the reference taken on the release object.
        //

        if (ReleaseObject != NULL) {

            ASSERT(ReleaseObject->ReferenceCount >= 2);

            IopFileObjectReleaseReference(ReleaseObject, FALSE);
            ReleaseObject = NULL;
        }

        KeAcquireQueuedLock(IoFileObjectsLock);

        //
        // The current object and node should match.
        //

        ASSERT(&(CurrentObject->TreeEntry) == Node);

        Node = RtlRedBlackTreeGetNextNode(&(IoFileObjectsTree),
                                          FALSE,
                                          Node);

        ReleaseObject = CurrentObject;
        CurrentObject = NULL;
    }

    KeReleaseQueuedLock(IoFileObjectsLock);

    //
    // Release any lingering references.
    //

    if (ReleaseObject != NULL) {

        ASSERT(ReleaseObject->ReferenceCount >= 2);

        IopFileObjectReleaseReference(ReleaseObject, FALSE);
    }

    if (CurrentObject != NULL) {

        ASSERT(ReleaseObject->ReferenceCount >= 2);

        IopFileObjectReleaseReference(CurrentObject, FALSE);
    }

    return;
}

KSTATUS
IopFlushFileObjectProperties (
    PFILE_OBJECT FileObject,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes the file properties for the given file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    BOOL LockHeld;
    IRP_MINOR_CODE MinorCode;
    ULONG OldFlags;
    KSTATUS Status;

    LockHeld = FALSE;

    //
    // Write out the file properties if a flush is required. A flush is
    // required if the file properties are dirty and the hard link count is not
    // zero.
    //

    OldFlags = RtlAtomicAnd32(&(FileObject->Flags),
                              ~FILE_OBJECT_FLAG_DIRTY_PROPERTIES);

    if (((OldFlags & FILE_OBJECT_FLAG_DIRTY_PROPERTIES) != 0) &&
        (FileObject->Properties.HardLinkCount != 0)) {

        //
        // Acquire the file's lock shared to synchronize with exclusive changes
        // to the hard link count.
        //

        if (FileObject->Lock != NULL) {
            KeAcquireSharedExclusiveLockShared(FileObject->Lock);
            LockHeld = TRUE;
        }

        //
        // If the hard link count sneaked down to zero then do not write out
        // the file properties.
        //

        if (FileObject->Properties.HardLinkCount == 0) {
            Status = STATUS_SUCCESS;
            goto FlushFilePropertiesEnd;
        }

        //
        // Write out the file properties. Don't report a failure if the device
        // got yanked in the middle of this operation. Other failures should
        // reset the properties as dirty. Something else may have marked them
        // dirty already and they may already have been cleaned successfully.
        // But this at least guarantees it will be tried again.
        //

        MinorCode = IrpMinorSystemControlWriteFileProperties;
        Status = IopSendFileOperationIrp(MinorCode, FileObject, NULL, Flags);
        if ((!KSUCCESS(Status)) && (Status != STATUS_DEVICE_NOT_CONNECTED)) {
            IopMarkFileObjectPropertiesDirty(FileObject);
            goto FlushFilePropertiesEnd;
        }

        if (LockHeld != FALSE) {
            KeReleaseSharedExclusiveLockShared(FileObject->Lock);
            LockHeld = FALSE;
        }
    }

    Status = STATUS_SUCCESS;

FlushFilePropertiesEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockShared(FileObject->Lock);
    }

    return Status;
}

VOID
IopUpdateFileObjectTime (
    PFILE_OBJECT FileObject,
    BOOL Modified
    )

/*++

Routine Description:

    This routine updates the given file object's access and modified times. The
    latter is only updated upon request.

Arguments:

    FileObject - Supplies a pointer to a file object.

    Modified - Supplies a boolean indicating whether or not the modified time
        needs to be updated.

Return Value:

    None.

--*/

{

    SYSTEM_TIME CurrentTime;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireSpinLock(&(FileObject->PropertiesLock));
    KeGetSystemTime(&CurrentTime);
    FileObject->Properties.AccessTime = CurrentTime;
    if (Modified != FALSE) {
        FileObject->Properties.ModifiedTime = CurrentTime;
    }

    KeReleaseSpinLock(&(FileObject->PropertiesLock));
    IopMarkFileObjectPropertiesDirty(FileObject);
    return;
}

VOID
IopUpdateFileObjectFileSize (
    PFILE_OBJECT FileObject,
    ULONGLONG NewSize,
    BOOL UpdateFileObject,
    BOOL UpdateProperties
    )

/*++

Routine Description:

    This routine decides whether or not to update the file size based on the
    supplied size. This routine will never decrease the file size. It is not
    intended to change the file size, just to update the size based on changes
    that other parts of the system have already completed. Use
    IopModifyFileObjectSize to actually change the size (e.g. truncate).

Arguments:

    FileObject - Supplies a pointer to a file object.

    NewSize - Supplies the new file size.

    UpdateFileObject - Supplies a boolean indicating whether or not to update
        the file size that is in the file object.

    UpdateProperties - Supplies a boolean indicating whether or not to update
        the file size that is within the file properties.

Return Value:

    None.

--*/

{

    ULONG BlockSize;
    ULONGLONG FileSize;
    BOOL Updated;

    ASSERT((FileObject->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeld(FileObject->Lock) != FALSE));

    //
    // If specified, try to update the file size stored in the file object.
    //

    if (UpdateFileObject != FALSE) {
        READ_INT64_SYNC(&(FileObject->FileSize), &FileSize);
        if (FileSize < NewSize) {

            ASSERT(KeGetRunLevel() == RunLevelLow);

            KeAcquireSpinLock(&(FileObject->PropertiesLock));
            READ_INT64_SYNC(&(FileObject->FileSize), &FileSize);
            if (FileSize < NewSize) {
                WRITE_INT64_SYNC(&(FileObject->FileSize), NewSize);
                BlockSize = FileObject->Properties.BlockSize;
                FileObject->Properties.BlockCount =
                                ALIGN_RANGE_UP(NewSize, BlockSize) / BlockSize;
            }

            KeReleaseSpinLock(&(FileObject->PropertiesLock));
        }
    }

    //
    // If specified, try to update the file size stored in the file properties.
    //

    if (UpdateProperties != FALSE) {

        //
        // If this call did not also update the file object's file size, then
        // the new size should be truncated by the size in the file object.
        // This is necessary because there are cases where the page cache is
        // flushing a full page to disk but the file size does not extend to
        // the end of that page.
        //

        if (UpdateFileObject == FALSE) {
            READ_INT64_SYNC(&(FileObject->FileSize), &FileSize);
            if (FileSize < NewSize) {
                NewSize = FileSize;
            }
        }

        Updated = FALSE;
        READ_INT64_SYNC(&(FileObject->Properties.FileSize), &FileSize);
        if (FileSize < NewSize) {

            ASSERT(KeGetRunLevel() == RunLevelLow);

            KeAcquireSpinLock(&(FileObject->PropertiesLock));
            READ_INT64_SYNC(&(FileObject->Properties.FileSize), &FileSize);
            if (FileSize < NewSize) {
                WRITE_INT64_SYNC(&(FileObject->Properties.FileSize), NewSize);
                Updated = TRUE;
            }

            KeReleaseSpinLock(&(FileObject->PropertiesLock));
        }

        if (Updated != FALSE) {
            IopMarkFileObjectPropertiesDirty(FileObject);
        }
    }

    return;
}

KSTATUS
IopModifyFileObjectSize (
    PFILE_OBJECT FileObject,
    PVOID DeviceContext,
    ULONGLONG NewFileSize
    )

/*++

Routine Description:

    This routine modifies the given file object's size. It will either increase
    or decrease the file size. If the size is decreased then the file object's
    driver will be notified, any existing page cache entries for the file will
    be evicted and any image sections that map the file will be unmapped.

Arguments:

    FileObject - Supplies a pointer to the file object whose size will be
        modified.

    DeviceContext - Supplies an optional pointer to the device context to use
        when doing file operations. Not every file object has a built-in device
        context.

    NewFileSize - Supplies the desired new size of the file object.

Return Value:

    Status code.

--*/

{

    ULONG BlockSize;
    ULONG EvictionFlags;
    ULONGLONG EvictionOffset;
    ULONGLONG FileSize;
    ULONG PageSize;
    ULONGLONG PropertiesSize;
    KSTATUS Status;
    ULONGLONG UnmapOffset;
    ULONGLONG UnmapSize;

    //
    // If the file object synchronizes I/O, then acquire the lock exclusively.
    //

    if (FileObject->Lock != NULL) {
        KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
    }

    //
    // If the new size is the same as the old file size then just exit.
    //

    READ_INT64_SYNC(&(FileObject->FileSize), &FileSize);
    if (FileSize == NewFileSize) {
        Status = STATUS_SUCCESS;
        goto ModifyFileObjectSizeEnd;
    }

    //
    // If the new size is less than the current size, then work needs to be
    // done to make sure the system isn't using any of the truncated data.
    //

    if (NewFileSize < FileSize) {

        //
        // Set both the file object's file size and the file properties file
        // size.
        //

        KeAcquireSpinLock(&(FileObject->PropertiesLock));
        WRITE_INT64_SYNC(&(FileObject->FileSize), NewFileSize);
        READ_INT64_SYNC(&(FileObject->Properties.FileSize), &PropertiesSize);
        if (NewFileSize < PropertiesSize) {
            WRITE_INT64_SYNC(&(FileObject->Properties.FileSize), NewFileSize);
        }

        BlockSize = FileObject->Properties.BlockSize;
        FileObject->Properties.BlockCount =
                           ALIGN_RANGE_UP(NewFileSize, BlockSize) / BlockSize;

        KeReleaseSpinLock(&(FileObject->PropertiesLock));
        IopMarkFileObjectPropertiesDirty(FileObject);

        //
        // If the new file size is less than the size that was stored in the
        // file properties, then the file properties were updated above and the
        // file needs to be truncated.
        //

        if (NewFileSize < PropertiesSize) {

            //
            // If this is a shared memory object, then handle that separately.
            //

            if (FileObject->Properties.Type == IoObjectSharedMemoryObject) {
                Status = IopTruncateSharedMemoryObject(FileObject);

            //
            // Otherwise call the driver to truncate the file or device. The
            // driver will check the file size and truncate the file down to
            // the new size.
            //

            } else {
                if (DeviceContext == NULL) {
                    DeviceContext = FileObject->DeviceContext;
                }

                Status = IopSendFileOperationIrp(IrpMinorSystemControlTruncate,
                                                 FileObject,
                                                 DeviceContext,
                                                 0);
            }

            if (!KSUCCESS(Status)) {
                goto ModifyFileObjectSizeEnd;
            }
        }

        //
        // Unmap all image sections that might have mapped portions of this
        // file.
        //

        if (FileObject->ImageSectionList != NULL) {
            PageSize = MmPageSize();
            UnmapOffset = ALIGN_RANGE_UP(NewFileSize, PageSize);
            UnmapSize = ALIGN_RANGE_UP((FileSize - UnmapOffset), PageSize);
            MmUnmapImageSectionList(FileObject->ImageSectionList,
                                    UnmapOffset,
                                    UnmapSize,
                                    IMAGE_SECTION_UNMAP_FLAG_TRUNCATE,
                                    NULL);
        }

        //
        // Evict all full page cache entries beyond the new file size for this
        // file object if it is cacheable.
        //

        if (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE) {
            EvictionFlags = PAGE_CACHE_EVICTION_FLAG_TRUNCATE;
            EvictionOffset = ALIGN_RANGE_UP(NewFileSize,
                                            IoGetCacheEntryDataSize());

            IopEvictPageCacheEntries(FileObject, EvictionOffset, EvictionFlags);
        }

        Status = STATUS_SUCCESS;

    //
    // Otherwise just update the file object's file size to allow reads beyond
    // the old file size.
    //

    } else {
        IopUpdateFileObjectFileSize(FileObject, NewFileSize, TRUE, FALSE);
        Status = STATUS_SUCCESS;
    }

ModifyFileObjectSizeEnd:

    //
    // Release the lock if it exists.
    //

    if (FileObject->Lock != NULL) {
        KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
    }

    return Status;
}

VOID
IopFileObjectIncrementHardLinkCount (
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine decrements the hard link count for a file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    None.

--*/

{

    KeAcquireSpinLock(&(FileObject->PropertiesLock));
    FileObject->Properties.HardLinkCount += 1;
    KeReleaseSpinLock(&(FileObject->PropertiesLock));
    IopMarkFileObjectPropertiesDirty(FileObject);
    return;
}

VOID
IopFileObjectDecrementHardLinkCount (
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine decrements the hard link count for a file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    None.

--*/

{

    ASSERT(FileObject->Properties.HardLinkCount != 0);

    KeAcquireSpinLock(&(FileObject->PropertiesLock));
    FileObject->Properties.HardLinkCount -= 1;
    KeReleaseSpinLock(&(FileObject->PropertiesLock));
    IopMarkFileObjectPropertiesDirty(FileObject);
    return;
}

VOID
IopCleanupFileObjects (
    VOID
    )

/*++

Routine Description:

    This routine releases any lingering file objects that were left around as a
    result of I/O failures during the orignal release attempt.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PFILE_OBJECT CurrentObject;
    LIST_ENTRY LocalList;

    //
    // Exit immediately if there are no orphaned file objects.
    //

    if (LIST_EMPTY(&IoFileObjectsOrphanedList) != FALSE) {
        return;
    }

    //
    // Grab the global file objects lock, migrate the global orphaned file
    // object list to a local list head and iterate over it. All objects on the
    // list should have only 1 reference. If another thread resurrects any
    // object during iteration, it will remove it from the local list and this
    // routine will not see it. For those file objects processed, just add an
    // extra reference with the lock held and release it with the lock released.
    // This should kick off another attempt at closing out the file object.
    //

    INITIALIZE_LIST_HEAD(&LocalList);
    KeAcquireQueuedLock(IoFileObjectsLock);
    MOVE_LIST(&IoFileObjectsOrphanedList, &LocalList);
    INITIALIZE_LIST_HEAD(&IoFileObjectsOrphanedList);
    while (LIST_EMPTY(&LocalList) == FALSE) {
        CurrentObject = LIST_VALUE(LocalList.Next, FILE_OBJECT, ListEntry);
        LIST_REMOVE(&(CurrentObject->ListEntry));
        CurrentObject->ListEntry.Next = NULL;

        ASSERT(CurrentObject->ReferenceCount == 1);

        IopFileObjectAddReference(CurrentObject);
        KeReleaseQueuedLock(IoFileObjectsLock);
        IopFileObjectReleaseReference(CurrentObject, FALSE);
        KeAcquireQueuedLock(IoFileObjectsLock);
    }

    KeReleaseQueuedLock(IoFileObjectsLock);
    return;
}

VOID
IopAcquireFileObjectIoLocksExclusive (
    PFILE_OBJECT *FileArray,
    ULONG FileArraySize
    )

/*++

Routine Description:

    This routine sorts the file objects into the appropriate locking order and
    then acquires their locks exclusively. It only operates on arrays that have
    length between 1 and 4, inclusive.

Arguments:

    FileArray - Supplies an array of file objects to sort.

    FileArraySize - Supplies the size of the file array.

Return Value:

    None.

--*/

{

    PFILE_OBJECT FileObject;
    ULONG Index;
    PFILE_OBJECT PreviousFile;
    PFILE_OBJECT SortedArray[4];

    ASSERT((FileArraySize >= 1) && (FileArraySize <= 4));
    ASSERT(FileArray[0] != NULL);

    if (FileArraySize == 1) {
        SortedArray[0] = FileArray[0];
        goto AcquireFileObjectIoLocksExclusiveEnd;
    }

    //
    // Sort the first two elements.
    //

    if (FileArray[0]->Properties.FileId < FileArray[1]->Properties.FileId) {
        SortedArray[0] = FileArray[0];
        SortedArray[1] = FileArray[1];

    } else {
        SortedArray[0] = FileArray[1];
        SortedArray[1] = FileArray[0];
    }

    if (FileArraySize == 2) {
        goto AcquireFileObjectIoLocksExclusiveEnd;
    }

    //
    // Either fill in the third, or sort the third and fourth.
    //

    if (FileArraySize == 3) {
        SortedArray[2] = FileArray[2];

    } else {

        ASSERT(FileArraySize == 4);

        if (FileArray[2]->Properties.FileId < FileArray[3]->Properties.FileId) {
            SortedArray[2] = FileArray[2];
            SortedArray[3] = FileArray[3];

        } else {
            SortedArray[2] = FileArray[3];
            SortedArray[3] = FileArray[2];
        }
    }

    //
    // Now compare sorted 0 to sorted 2. Swap if they are out of order.
    //

    if (SortedArray[0]->Properties.FileId > SortedArray[2]->Properties.FileId) {
        FileObject = SortedArray[0];
        SortedArray[0] = SortedArray[2];
        SortedArray[2] = FileObject;
    }

    //
    // Sort the two high elements if the array is of size four.
    //

    if ((FileArraySize == 4) &&
        (SortedArray[1]->Properties.FileId >
         SortedArray[3]->Properties.FileId)) {

        FileObject = SortedArray[1];
        SortedArray[1] = SortedArray[3];
        SortedArray[3] = FileObject;
    }

    //
    // Finish by sorting the two middle elements.
    //

    if (SortedArray[1]->Properties.FileId > SortedArray[2]->Properties.FileId) {
        FileObject = SortedArray[1];
        SortedArray[1] = SortedArray[2];
        SortedArray[2] = FileObject;
    }

AcquireFileObjectIoLocksExclusiveEnd:

    //
    // Lock the file objects, making sure not to lock the same element twice.
    // Equal elements are neighbors in the array, so just check the current
    // file against the file that was locked last.
    //

    PreviousFile = NULL;
    for (Index = 0; Index < FileArraySize; Index += 1) {
        FileArray[Index] = SortedArray[Index];
        if (FileArray[Index] != PreviousFile) {

            ASSERT(FileArray[Index] != NULL);

            KeAcquireSharedExclusiveLockExclusive(FileArray[Index]->Lock);
            PreviousFile = FileArray[Index];
        }
    }

    return;
}

VOID
IopReleaseFileObjectIoLocksExclusive (
    PFILE_OBJECT *FileArray,
    ULONG FileArraySize
    )

/*++

Routine Description:

    This routine release the given files' locks in reverse order. The array
    should already be correctly sorted.

Arguments:

    FileArray - Supplies an array of file objects whose locks need to be
        released.

    FileArraySize - Supplies the number of files in the array.

Return Value:

    None.

--*/

{

    ULONG Index;
    PFILE_OBJECT PreviousFile;

    ASSERT((FileArraySize >= 1) && (FileArraySize <= 4));
    ASSERT(FileArray[0] != NULL);

    //
    // Release the file objects' locks in reverse order. If there are any
    // duplicates in the list, they will be appropriately skipped.
    //

    PreviousFile = NULL;
    for (Index = FileArraySize; Index > 0; Index -= 1) {
        if ((FileArray[Index - 1] != PreviousFile) &&
            (FileArray[Index - 1] != NULL)) {

            KeReleaseSharedExclusiveLockExclusive(FileArray[Index - 1]->Lock);
            PreviousFile = FileArray[Index - 1];
        }
    }

    return;
}

PIMAGE_SECTION_LIST
IopGetImageSectionListFromFileObject (
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine gets the image section for the given file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    Returns a pointer to the file object's image section list or NULL on
    failure.

--*/

{

    PIMAGE_SECTION_LIST ImageSectionList;
    PIMAGE_SECTION_LIST OldList;

    //
    // If there is no image section list, then allocate one and try to set it
    // in the file object.
    //

    if (FileObject->ImageSectionList == NULL) {
        ImageSectionList = MmCreateImageSectionList();
        if (ImageSectionList == NULL) {
            goto GetImageSectionListFromFileObjectEnd;
        }

        OldList = (PVOID)RtlAtomicCompareExchange(
                             (volatile UINTN *)&(FileObject->ImageSectionList),
                             (UINTN)ImageSectionList,
                             (UINTN)NULL);

        if (OldList != NULL) {
            MmDestroyImageSectionList(ImageSectionList);
        }
    }

    ASSERT(FileObject->ImageSectionList != NULL);

    ImageSectionList = FileObject->ImageSectionList;

GetImageSectionListFromFileObjectEnd:
    return ImageSectionList;
}

VOID
IopMarkFileObjectDirty (
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine marks the given file object as dirty, moving it to the list of
    dirty file objects if it is not already on a list.

Arguments:

    FileObject - Supplies a pointer to the dirty file object.

Return Value:

    None.

--*/

{

    if (FileObject->ListEntry.Next == NULL) {
        KeAcquireQueuedLock(IoFileObjectsLock);
        if (FileObject->ListEntry.Next == NULL) {
            IopFileObjectAddReference(FileObject);

            //
            // The lower layer file objects go at the end of the list. This
            // allows flush to only traverse the list once to get all the data
            // out to the block devices.
            //

            if (FileObject->Properties.Type == IoObjectBlockDevice) {
                INSERT_BEFORE(&(FileObject->ListEntry),
                              &IoFileObjectsDirtyList);

            } else {
                INSERT_AFTER(&(FileObject->ListEntry), &IoFileObjectsDirtyList);
            }
        }

        KeReleaseQueuedLock(IoFileObjectsLock);
    }

    return;
}

VOID
IopMarkFileObjectPropertiesDirty (
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine marks that the given file object's properties are dirty.

Arguments:

    FileObject - Supplies a pointer to the file object whose properties are
        dirty.

Return Value:

    None.

--*/

{

    ULONG OldFlags;

    OldFlags = RtlAtomicOr32(&(FileObject->Flags),
                             FILE_OBJECT_FLAG_DIRTY_PROPERTIES);

    //
    // If this operation just transitioned the file properties from clean to
    // dirty and the file object has a hard link, add the file object to the
    // dirty list and let the page cache know so it can flush out this file
    // object data.
    //

    if (((OldFlags & FILE_OBJECT_FLAG_DIRTY_PROPERTIES) == 0) &&
        (FileObject->Properties.HardLinkCount != 0)) {

        IopMarkFileObjectDirty(FileObject);
        IopNotifyPageCacheFilePropertiesUpdate();
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

COMPARISON_RESULT
IopCompareFileObjectNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes contained inside file
    objects.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PFILE_OBJECT FirstObject;
    PFILE_OBJECT SecondObject;

    FirstObject = RED_BLACK_TREE_VALUE(FirstNode, FILE_OBJECT, TreeEntry);
    SecondObject = RED_BLACK_TREE_VALUE(SecondNode, FILE_OBJECT, TreeEntry);

    //
    // First check the file IDs, which are most likely to be different.
    //

    if (FirstObject->Properties.FileId > SecondObject->Properties.FileId) {
        return ComparisonResultDescending;
    }

    if (FirstObject->Properties.FileId < SecondObject->Properties.FileId) {
        return ComparisonResultAscending;
    }

    //
    // The File IDs are equal, also compare the volumes.
    //

    if (FirstObject->Properties.DeviceId > SecondObject->Properties.DeviceId) {
        return ComparisonResultDescending;
    }

    if (FirstObject->Properties.DeviceId < SecondObject->Properties.DeviceId) {
        return ComparisonResultAscending;
    }

    //
    // Both the File ID and the volume are equal, these nodes are the
    // same.
    //

    return ComparisonResultSame;
}

PFILE_OBJECT
IopLookupFileObjectByProperties (
    PFILE_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine attempts to look up a file object with the given properties
    (specifically the device and file IDs). It assumes the global file objects
    lock is already held.

Arguments:

    Properties - Supplies a pointer to the file object properties.

Return Value:

    Returns a pointer to the found file object, with an incremented reference
    count on success. The caller is responsible for releasing this reference.

    NULL if the file object could not be found.

--*/

{

    PRED_BLACK_TREE_NODE FoundNode;
    PFILE_OBJECT Object;
    ULONG OldReferenceCount;
    FILE_OBJECT SearchObject;

    ASSERT(Properties->DeviceId != 0);

    Object = NULL;
    SearchObject.Properties.FileId = Properties->FileId;
    SearchObject.Properties.DeviceId = Properties->DeviceId;
    FoundNode = RtlRedBlackTreeSearch(&IoFileObjectsTree,
                                      &(SearchObject.TreeEntry));

    if (FoundNode != NULL) {
        Object = RED_BLACK_TREE_VALUE(FoundNode, FILE_OBJECT, TreeEntry);

        //
        // Increment the reference count. If this ends up resurrecting an
        // orphaned or about to be closed file object, then make sure it is not
        // on the orphaned list (or any list for that matter) as it could be
        // used and made dirty.
        //

        OldReferenceCount = IopFileObjectAddReference(Object);
        if (OldReferenceCount == 1) {
            if (Object->ListEntry.Next != NULL) {
                LIST_REMOVE(&(Object->ListEntry));
                Object->ListEntry.Next = NULL;
            }
        }
    }

    return Object;
}

