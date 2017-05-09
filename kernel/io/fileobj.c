/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>
#include "iop.h"
#include "pagecach.h"

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

KSTATUS
IopFlushFileObjectProperties (
    PFILE_OBJECT FileObject,
    ULONG Flags
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

VOID
IopDestroyAsyncState (
    PIO_ASYNC_STATE Async
    );

VOID
IopSendIoSignal (
    PIO_ASYNC_STATE Async,
    USHORT SignalCode,
    ULONG BandEvent
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
// Store the lock synchronizing access to the dirty file objects list.
//

PQUEUED_LOCK IoFileObjectsDirtyListLock;

//
// Store the global list of orphaned file objects.
//

LIST_ENTRY IoFileObjectsOrphanedList;

//
// Store a queued lock that protects both the tree and the list.
//

PQUEUED_LOCK IoFileObjectsLock;

//
// Store a lock that can serialize flush operations.
//

PSHARED_EXCLUSIVE_LOCK IoFlushLock;

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

    ULONG PreviousEvents;
    ULONG RisingEdge;
    SIGNAL_OPTION SignalOption;

    //
    // Prepare to signal the events. The events mask must be updated before an
    // event is signaled as it may immediately be read by a waiter.
    //

    if (Set != FALSE) {
        SignalOption = SignalOptionSignalAll;
        PreviousEvents = RtlAtomicOr32(&(IoState->Events), Events);

    } else {
        SignalOption = SignalOptionUnsignal;
        PreviousEvents = RtlAtomicAnd32(&(IoState->Events), ~Events);
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

    //
    // If read or write just went high, potentially signal the owner.
    //

    if ((Set != FALSE) &&
        (IoState->Async != NULL) &&
        (IoState->Async->Owner != 0)) {

        RisingEdge = (PreviousEvents ^ Events) & Events;
        if ((RisingEdge & POLL_EVENT_IN) != 0) {
            IopSendIoSignal(IoState->Async, POLL_CODE_IN, POLL_EVENT_IN);
        }

        if ((RisingEdge & POLL_EVENT_OUT) != 0) {
            IopSendIoSignal(IoState->Async, POLL_CODE_OUT, POLL_EVENT_OUT);
        }
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

    ULONG ReturnEvents;
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

    WaitObjectArray[0] = IoState->ErrorEvent;
    WaitObjectCount = 1;

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

    //
    // Loop until the event flags agree with the wait.
    //

    while (TRUE) {
        Status = ObWaitOnObjects(WaitObjectArray,
                                 WaitObjectCount,
                                 WaitFlags,
                                 TimeoutInMilliseconds,
                                 NULL,
                                 NULL);

        if (!KSUCCESS(Status)) {
            goto WaitForIoObjectStateEnd;
        }

        ReturnEvents = IoState->Events & (Events | POLL_NONMASKABLE_EVENTS);

        //
        // The I/O object state maintains a bitmask of all the currently
        // signaled poll events. AND this with the requested events to get the
        // returned events for this descriptor.
        //

        if (ReturnedEvents != NULL) {
            *ReturnedEvents = ReturnEvents;
        }

        //
        // If there were no returned events, then the event fired but the flags
        // seem to be out of date. Go back and try again.
        //

        if (ReturnEvents != 0) {
            break;
        }
    }

WaitForIoObjectStateEnd:
    return Status;
}

KERNEL_API
PIO_OBJECT_STATE
IoCreateIoObjectState (
    BOOL HighPriority,
    BOOL NonPaged
    )

/*++

Routine Description:

    This routine creates a new I/O object state structure with a reference
    count of one.

Arguments:

    HighPriority - Supplies a boolean indicating whether or not the I/O object
        state should be prepared for high priority events.

    NonPaged - Supplies a boolean indicating whether or not the I/O object
        state should be allocated from non-paged pool. Default is paged pool
        (FALSE).

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
    if (NonPaged != FALSE) {
        NewState = MmAllocateNonPagedPool(sizeof(IO_OBJECT_STATE),
                                          FILE_OBJECT_ALLOCATION_TAG);

    } else {
        NewState = MmAllocatePagedPool(sizeof(IO_OBJECT_STATE),
                                       FILE_OBJECT_ALLOCATION_TAG);
    }

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
            IoDestroyIoObjectState(NewState, NonPaged);
            NewState = NULL;
        }
    }

    return NewState;
}

KERNEL_API
VOID
IoDestroyIoObjectState (
    PIO_OBJECT_STATE State,
    BOOL NonPaged
    )

/*++

Routine Description:

    This routine destroys the given I/O object state.

Arguments:

    State - Supplies a pointer to the I/O object state to destroy.

    NonPaged - Supplies a boolean indicating whether or not the I/O object
        was allocated from non-paged pool. Default is paged pool (FALSE).

Return Value:

    None.

--*/

{

    if (State->Async != NULL) {
        IopDestroyAsyncState(State->Async);
    }

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

    if (NonPaged != FALSE) {
        MmFreeNonPagedPool(State);

    } else {
        MmFreePagedPool(State);
    }

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

    FileObject = IoHandle->FileObject;
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

    Status = IopFileObjectReleaseReference(FileObject);

    ASSERT(KSUCCESS(Status));

    return;
}

KSTATUS
IoSetHandleAsynchronous (
    PIO_HANDLE IoHandle,
    HANDLE Descriptor,
    BOOL Asynchronous
    )

/*++

Routine Description:

    This routine enables or disables asynchronous mode for the given I/O
    handle.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

    Descriptor - Supplies the descriptor to associate with the asynchronous
        receiver state. This descriptor is passed to the signal information
        when an I/O signal occurs. Note that this descriptor may become stale
        if the handle is duped and the original closed, so the kernel should
        never access it.

    Asynchronous - Supplies a boolean indicating whether to set asynchronous
        mode (TRUE) or clear it (FALSE).

Return Value:

    Status code.

--*/

{

    PIO_ASYNC_STATE AsyncState;
    PIO_OBJECT_STATE IoState;
    PKPROCESS Process;
    KSTATUS Status;

    IoState = IoHandle->FileObject->IoState;
    AsyncState = IopGetAsyncState(IoState);
    if (AsyncState == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KeAcquireQueuedLock(AsyncState->Lock);
    if (Asynchronous == FALSE) {
        if (IoHandle->Async != NULL) {
            if (IoHandle->Async->ListEntry.Next != NULL) {
                LIST_REMOVE(&(IoHandle->Async->ListEntry));
                IoHandle->Async->ListEntry.Next = NULL;
            }
        }

        IoHandle->OpenFlags &= ~OPEN_FLAG_ASYNCHRONOUS;

    //
    // Enable asynchronous mode.
    //

    } else {
        if (IoHandle->Async == NULL) {
            IoHandle->Async = MmAllocatePagedPool(sizeof(ASYNC_IO_RECEIVER),
                                                  FILE_OBJECT_ALLOCATION_TAG);

            if (IoHandle->Async == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto SetHandleAsynchronousEnd;
            }

            RtlZeroMemory(IoHandle->Async, sizeof(ASYNC_IO_RECEIVER));
        }

        IoHandle->Async->Descriptor = Descriptor;
        if (IoHandle->Async->ListEntry.Next == NULL) {
            INSERT_BEFORE(&(IoHandle->Async->ListEntry),
                          &(AsyncState->ReceiverList));
        }

        Process = PsGetCurrentProcess();
        IoHandle->Async->ProcessId = Process->Identifiers.ProcessId;
        IoHandle->OpenFlags |= OPEN_FLAG_ASYNCHRONOUS;
    }

    Status = STATUS_SUCCESS;

SetHandleAsynchronousEnd:
    KeReleaseQueuedLock(AsyncState->Lock);
    return Status;;
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

    IoFileObjectsDirtyListLock = KeCreateQueuedLock();
    if (IoFileObjectsDirtyListLock == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    IoFlushLock = KeCreateSharedExclusiveLock();
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
    ULONG MapFlags,
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

    MapFlags - Supplies the additional map flags associated with this file
        object. See MAP_FLAG_* definitions.

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
    BOOL LockHeld;
    PFILE_OBJECT NewObject;
    BOOL NonPagedIoState;
    PFILE_OBJECT Object;
    KSTATUS Status;

    ASSERT(Properties->DeviceId != 0);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    Created = FALSE;
    LockHeld = FALSE;
    NewObject = NULL;
    NonPagedIoState = FALSE;
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
                INITIALIZE_LIST_HEAD(&(NewObject->FileLockList));
                INITIALIZE_LIST_HEAD(&(NewObject->DirtyPageList));
                RtlRedBlackTreeInitialize(&(NewObject->PageCacheTree),
                                          0,
                                          IopComparePageCacheEntries);

                NewObject->Lock = KeCreateSharedExclusiveLock();
                if (NewObject->Lock == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto CreateOrLookupFileObjectEnd;
                }

                if ((Flags & FILE_OBJECT_FLAG_EXTERNAL_IO_STATE) == 0) {
                    if ((Flags & FILE_OBJECT_FLAG_NON_PAGED_IO_STATE) != 0) {
                        NonPagedIoState = TRUE;
                    }

                    NewObject->IoState = IoCreateIoObjectState(FALSE,
                                                               NonPagedIoState);

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

                //
                // Currently only character devices that want to map their
                // hardware assets directly are known to need map flags. This
                // assert catches accidental uninitialized map flags. Remove
                // this assert if there's a need for other object types to
                // specify map flags.
                //

                ASSERT((MapFlags == 0) ||
                       (Properties->Type == IoObjectCharacterDevice));

                NewObject->Flags = Flags;
                NewObject->MapFlags = MapFlags;
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
            }

            //
            // It's time to insert it into the tree. Someone may have already
            // added this entry since the lock was dropped, so check once more.
            //

            KeAcquireQueuedLock(IoFileObjectsLock);
            LockHeld = TRUE;
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
            IopFileObjectReleaseReference(Object);
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
            IopFileObjectReleaseReference(Object);
            Object = NULL;

            ASSERT(Object != NewObject);
        }
    }

    if (NewObject != NULL) {

        ASSERT(NewObject->ListEntry.Next == NULL);

        if (NewObject->Lock != NULL) {
            KeDestroySharedExclusiveLock(NewObject->Lock);
        }

        if (NewObject->IoState != NULL) {
            IoDestroyIoObjectState(NewObject->IoState, NonPagedIoState);
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
    PFILE_OBJECT Object
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

    Other error codes on failure to write out the file properties to the file
    system or device.

--*/

{

    IRP_CLOSE CloseIrp;
    PDEVICE Device;
    IRP_MINOR_CODE MinorCode;
    BOOL NonPagedIoState;
    ULONG OldCount;
    KSTATUS Status;

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
        IopFileObjectReleaseReference(Object);

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

        ASSERT(RED_BLACK_TREE_EMPTY(&(Object->PageCacheTree)));
        ASSERT(LIST_EMPTY(&(Object->DirtyPageList)));

        if (Object->Lock != NULL) {
            KeDestroySharedExclusiveLock(Object->Lock);
        }

        if (((Object->Flags & FILE_OBJECT_FLAG_EXTERNAL_IO_STATE) == 0) &&
            (Object->IoState != NULL)) {

            NonPagedIoState = FALSE;
            if ((Object->Flags & FILE_OBJECT_FLAG_NON_PAGED_IO_STATE) != 0) {
                NonPagedIoState = TRUE;
            }

            IoDestroyIoObjectState(Object->IoState, NonPagedIoState);
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

    if (!KSUCCESS(Status)) {

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
    }

    return;
}

KSTATUS
IopFlushFileObject (
    PFILE_OBJECT FileObject,
    IO_OFFSET Offset,
    ULONGLONG Size,
    ULONG Flags,
    BOOL FlushExclusive,
    PUINTN PageCount
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

    FlushExclusive - Supplies a boolean indicating if this was an explicit
        flush. If so, then the flush lock is acquired exclusively to prevent
        partial flushes due to dirty page cache entries being on a local list.

    PageCount - Supplies an optional pointer describing how many pages to flush.
        On output this value will be decreased by the number of pages actually
        flushed. Supply NULL to flush all pages in the size range.

Return Value:

    Status code.

--*/

{

    ULONG ClearFlags;
    BOOL Exclusive;
    KSTATUS Status;

    if (FlushExclusive != FALSE) {
        KeAcquireSharedExclusiveLockExclusive(IoFlushLock);

    } else {
        KeAcquireSharedExclusiveLockShared(IoFlushLock);
    }

    Exclusive = FALSE;
    KeAcquireSharedExclusiveLockShared(FileObject->Lock);
    if ((FileObject->Properties.HardLinkCount == 0) &&
        (FileObject->PathEntryCount == 0)) {

        KeSharedExclusiveLockConvertToExclusive(FileObject->Lock);
        Exclusive = TRUE;
        IopEvictFileObject(FileObject, 0, EVICTION_FLAG_REMOVE);
        ClearFlags = FILE_OBJECT_FLAG_DIRTY_PROPERTIES |
                     FILE_OBJECT_FLAG_DIRTY_DATA;

        RtlAtomicAnd32(&(FileObject->Flags), ~ClearFlags);

    } else {
        Status = IopFlushPageCacheEntries(FileObject,
                                          Offset,
                                          Size,
                                          Flags,
                                          PageCount);

        if (!KSUCCESS(Status)) {
            goto FlushFileObjectEnd;
        }

        Status = IopFlushFileObjectProperties(FileObject, Flags);
        if (!KSUCCESS(Status)) {
            goto FlushFileObjectEnd;
        }
    }

    Status = STATUS_SUCCESS;

FlushFileObjectEnd:
    if (Exclusive != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);

    } else {
        KeReleaseSharedExclusiveLockShared(FileObject->Lock);
    }

    if (FlushExclusive != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(IoFlushLock);

    } else {
        KeReleaseSharedExclusiveLockShared(IoFlushLock);
    }

    return Status;
}

KSTATUS
IopFlushFileObjects (
    DEVICE_ID DeviceId,
    ULONG Flags,
    PUINTN PageCount
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

    PageCount - Supplies an optional pointer describing how many pages to flush.
        On output this value will be decreased by the number of pages actually
        flushed. Supply NULL to flush all pages.

Return Value:

    STATUS_SUCCESS if all file object were successfully iterated.

    STATUS_TRY_AGAIN if the iteration quit early for some reason (i.e. the page
    cache was found to be too dirty when flushing file objects).

    Other status codes for other errors.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PFILE_OBJECT CurrentObject;
    ULONG FlushCount;
    BOOL FlushExclusive;
    ULONG FlushIndex;
    PFILE_OBJECT NextObject;
    KSTATUS Status;
    KSTATUS TotalStatus;

    CurrentObject = NULL;
    TotalStatus = STATUS_SUCCESS;

    //
    // Synchronized flushes need to guarantee that all the data is out to disk
    // before returning.
    //

    FlushCount = 1;
    FlushExclusive = FALSE;
    if ((Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) {
        FlushExclusive = TRUE;

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
    // supplied acquire the lock to make sure any other thread has finished
    // flushing the device's data.
    //

    } else if ((DeviceId == 0) &&
               (LIST_EMPTY(&IoFileObjectsDirtyList) != FALSE)) {

        return STATUS_SUCCESS;
    }

    //
    // Now make several attempts at performing the requested clean operation.
    //

    Status = STATUS_SUCCESS;
    for (FlushIndex = 0; FlushIndex < FlushCount; FlushIndex += 1) {

        //
        // Get the first entry on the list, or the first file object for the
        // specific device in question.
        //

        KeAcquireQueuedLock(IoFileObjectsDirtyListLock);
        CurrentEntry = IoFileObjectsDirtyList.Next;
        if (DeviceId == 0) {
            CurrentObject = LIST_VALUE(CurrentEntry, FILE_OBJECT, ListEntry);

        } else {
            while (CurrentEntry != &IoFileObjectsDirtyList) {
                CurrentObject = LIST_VALUE(CurrentEntry,
                                           FILE_OBJECT,
                                           ListEntry);

                if (CurrentObject->Properties.DeviceId == DeviceId) {
                    break;
                }

                CurrentEntry = CurrentEntry->Next;
            }
        }

        if (CurrentEntry == &IoFileObjectsDirtyList) {
            CurrentObject = NULL;

        } else {
            IopFileObjectAddReference(CurrentObject);
        }

        KeReleaseQueuedLock(IoFileObjectsDirtyListLock);

        //
        // If a device ID was supplied, but no file objects were found to
        // belong to that device, then the flush was successful!
        //

        if ((CurrentObject == NULL) && (DeviceId != 0)) {
            TotalStatus = STATUS_SUCCESS;
            break;
        }

        //
        // Loop cleaning file objects.
        //

        while (CurrentObject != NULL) {
            Status = IopFlushFileObject(CurrentObject,
                                        0,
                                        -1,
                                        Flags,
                                        FlushExclusive,
                                        PageCount);

            if (!KSUCCESS(Status)) {
                if (KSUCCESS(TotalStatus)) {
                    TotalStatus = Status;
                }
            }

            if ((PageCount != NULL) && (*PageCount == 0)) {
                break;
            }

            //
            // Re-lock the list, and get the next object.
            //

            NextObject = NULL;
            KeAcquireQueuedLock(IoFileObjectsDirtyListLock);
            if (CurrentObject->ListEntry.Next != NULL) {
                CurrentEntry = CurrentObject->ListEntry.Next;

            } else {
                CurrentEntry = IoFileObjectsDirtyList.Next;
            }

            if (DeviceId == 0) {
                if (CurrentEntry != &IoFileObjectsDirtyList) {
                    NextObject = LIST_VALUE(CurrentEntry,
                                            FILE_OBJECT,
                                            ListEntry);
                }

            } else {
                while (CurrentEntry != &IoFileObjectsDirtyList) {
                    NextObject = LIST_VALUE(CurrentEntry,
                                            FILE_OBJECT,
                                            ListEntry);

                    if (NextObject->Properties.DeviceId == DeviceId) {
                        break;
                    }

                    CurrentEntry = CurrentEntry->Next;
                }

                if (CurrentEntry == &IoFileObjectsDirtyList) {
                    NextObject = NULL;
                }
            }

            //
            // Remove the file object from the list if it is clean now.
            //

            if (IS_FILE_OBJECT_CLEAN(CurrentObject)) {
                if (CurrentObject->ListEntry.Next != NULL) {
                    LIST_REMOVE(&(CurrentObject->ListEntry));
                    CurrentObject->ListEntry.Next = NULL;
                    IopFileObjectReleaseReference(CurrentObject);
                }
            }

            if (NextObject != NULL) {
                IopFileObjectAddReference(NextObject);
            }

            KeReleaseQueuedLock(IoFileObjectsDirtyListLock);
            IopFileObjectReleaseReference(CurrentObject);
            CurrentObject = NextObject;
        }

        if (CurrentObject != NULL) {
            IopFileObjectReleaseReference(CurrentObject);
            CurrentObject = NULL;
        }
    }

    ASSERT(CurrentObject == NULL);

    return TotalStatus;
}

VOID
IopEvictFileObject (
    PFILE_OBJECT FileObject,
    IO_OFFSET Offset,
    ULONG Flags
    )

/*++

Routine Description:

    This routine evicts the tail of a file object from the system. It unmaps
    all page cache entries used by image sections after the given offset and
    evicts all page cache entries after the given offset. If the remove or
    truncate flags are specified, this routine actually unmaps all mappings for
    the image sections after the given offset, not just the mapped page cache
    entries. This routine assumes the file object's lock is held exclusively.

Arguments:

    FileObject - Supplies a pointer to the file object to evict.

    Offset - Supplies the starting offset into the file or device after which
        all page cache entries should be evicted and all image sections should
        be unmapped.

    Flags - Supplies a bitmask of eviction flags. See EVICTION_FLAG_* for
        definitions.

Return Value:

    None.

--*/

{

    ULONG UnmapFlags;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock) != FALSE);

    if (FileObject->ImageSectionList != NULL) {

        //
        // If the file object is being truncated or removed, unmap all
        // overlapping portions of the image sections.
        //

        if (((Flags & EVICTION_FLAG_REMOVE) != 0) ||
            ((Flags & EVICTION_FLAG_TRUNCATE) != 0)) {

            UnmapFlags = IMAGE_SECTION_UNMAP_FLAG_TRUNCATE;

        //
        // Otherwise just unmap the page cache entries.
        //

        } else {
            UnmapFlags = IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY;
        }

        MmUnmapImageSectionList(FileObject->ImageSectionList,
                                Offset,
                                -1,
                                UnmapFlags);
    }

    //
    // Evict the page cache entries for the file object.
    //

    IopEvictPageCacheEntries(FileObject, Offset, Flags);
    return;
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

    Flags - Supplies a bitmask of eviction flags. See EVICTION_FLAG_* for
        definitions.

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
        KeAcquireSharedExclusiveLockExclusive(CurrentObject->Lock);

        //
        // Call the eviction routine for the current file object.
        //

        IopEvictFileObject(CurrentObject, 0, Flags);

        //
        // Release the reference taken on the release object.
        //

        if (ReleaseObject != NULL) {

            ASSERT(ReleaseObject->ReferenceCount >= 2);

            IopFileObjectReleaseReference(ReleaseObject);
            ReleaseObject = NULL;
        }

        KeReleaseSharedExclusiveLockExclusive(CurrentObject->Lock);
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

        IopFileObjectReleaseReference(ReleaseObject);
    }

    if (CurrentObject != NULL) {

        ASSERT(ReleaseObject->ReferenceCount >= 2);

        IopFileObjectReleaseReference(CurrentObject);
    }

    return;
}

VOID
IopUpdateFileObjectTime (
    PFILE_OBJECT FileObject,
    FILE_OBJECT_TIME_TYPE TimeType
    )

/*++

Routine Description:

    This routine updates the given file object's access and modified times. The
    latter is only updated upon request.

Arguments:

    FileObject - Supplies a pointer to a file object.

    TimeType - Supplies the type of time to update. Updating modified time also
        updates status change time.

Return Value:

    None.

--*/

{

    SYSTEM_TIME CurrentTime;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock));

    KeGetSystemTime(&CurrentTime);
    if (TimeType == FileObjectAccessTime) {
        FileObject->Properties.AccessTime = CurrentTime;

    } else if (TimeType == FileObjectModifiedTime) {
        FileObject->Properties.ModifiedTime = CurrentTime;
        FileObject->Properties.StatusChangeTime = CurrentTime;

    } else if (TimeType == FileObjectStatusTime) {
        FileObject->Properties.StatusChangeTime = CurrentTime;

    } else {

        ASSERT(FALSE);

    }

    IopMarkFileObjectPropertiesDirty(FileObject);
    return;
}

VOID
IopUpdateFileObjectFileSize (
    PFILE_OBJECT FileObject,
    ULONGLONG NewSize
    )

/*++

Routine Description:

    This routine will make sure the file object file size is at least the
    given size. If it is not, it will be set to the given size. If it is, no
    change will be performed. Use the modify file object size function to
    forcibly set a new size (eg for truncate).

Arguments:

    FileObject - Supplies a pointer to a file object.

    NewSize - Supplies the new file size.

Return Value:

    None.

--*/

{

    ULONGLONG BlockCount;
    ULONG BlockSize;
    ULONGLONG FileSize;

    FileSize = FileObject->Properties.Size;
    if (FileSize < NewSize) {

        ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock));

        FileObject->Properties.Size = NewSize;

        //
        // TODO: Block count should be managed by the file system.
        //

        BlockSize = FileObject->Properties.BlockSize;
        BlockCount = ALIGN_RANGE_UP(NewSize, BlockSize) / BlockSize;
        FileObject->Properties.BlockCount = BlockCount;
        IopMarkFileObjectPropertiesDirty(FileObject);
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
    ULONGLONG FileSize;
    IO_OFFSET Offset;
    SYSTEM_CONTROL_TRUNCATE Request;
    KSTATUS Status;

    KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);

    //
    // If the new size is the same as the old file size then just exit.
    //

    FileSize = FileObject->Properties.Size;
    if (FileSize == NewFileSize) {
        Status = STATUS_SUCCESS;
        goto ModifyFileObjectSizeEnd;
    }

    BlockSize = FileObject->Properties.BlockSize;

    //
    // TODO: Block size should be managed by the file system.
    //

    FileObject->Properties.BlockCount =
                       ALIGN_RANGE_UP(NewFileSize, BlockSize) / BlockSize;

    //
    // If this is a shared memory object, then handle that separately.
    //

    if (FileObject->Properties.Type == IoObjectSharedMemoryObject) {
        Status = IopTruncateSharedMemoryObject(FileObject, NewFileSize);

    //
    // Otherwise call the driver to truncate the file or device. The
    // driver will check the file size and truncate the file down to
    // the new size.
    //

    } else {
        if (DeviceContext == NULL) {
            DeviceContext = FileObject->DeviceContext;
        }

        Request.FileProperties = &(FileObject->Properties);
        Request.DeviceContext = DeviceContext;
        Request.NewSize = NewFileSize;
        Status = IopSendSystemControlIrp(FileObject->Device,
                                         IrpMinorSystemControlTruncate,
                                         &Request);
    }

    IopMarkFileObjectPropertiesDirty(FileObject);
    if (!KSUCCESS(Status)) {
        goto ModifyFileObjectSizeEnd;
    }

    //
    // If the new size is less than the current size, then work needs to be
    // done to make sure the system isn't using any of the truncated data.
    //

    if (NewFileSize < FileSize) {
        Offset = ALIGN_RANGE_UP(NewFileSize, IoGetCacheEntryDataSize());
        IopEvictFileObject(FileObject, Offset, EVICTION_FLAG_TRUNCATE);
    }

ModifyFileObjectSizeEnd:

    //
    // Release the lock if it exists.
    //

    KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
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

    FileObject->Properties.HardLinkCount += 1;
    IopUpdateFileObjectTime(FileObject, FileObjectStatusTime);
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

    FileObject->Properties.HardLinkCount -= 1;
    IopUpdateFileObjectTime(FileObject, FileObjectStatusTime);
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
        IopFileObjectReleaseReference(CurrentObject);
        KeAcquireQueuedLock(IoFileObjectsLock);
    }

    KeReleaseQueuedLock(IoFileObjectsLock);
    return;
}

VOID
IopAcquireFileObjectLocksExclusive (
    PFILE_OBJECT Object1,
    PFILE_OBJECT Object2
    )

/*++

Routine Description:

    This routine acquires two file object locks exclusive in the right order.
    The order is to sort first by file object type, then by file object pointer.

Arguments:

    Object1 - Supplies a pointer to the first file object.

    Object2 - Supplies a pointer to the second file object.

Return Value:

    None.

--*/

{

    PFILE_OBJECT Swap;

    if (Object1 == Object2) {
        KeAcquireSharedExclusiveLockExclusive(Object1->Lock);
        return;
    }

    //
    // If the types are in the wrong order, swap them.
    //

    if (Object1->Properties.Type > Object2->Properties.Type) {
        Swap = Object1;
        Object1 = Object2;
        Object2 = Swap;

    //
    // Otherwise, if they're equal, compare pointers.
    //

    } else if (Object1->Properties.Type == Object2->Properties.Type) {
        if (Object1 > Object2) {
            Swap = Object1;
            Object1 = Object2;
            Object2 = Swap;
        }
    }

    KeAcquireSharedExclusiveLockExclusive(Object1->Lock);
    KeAcquireSharedExclusiveLockExclusive(Object2->Lock);
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

    if ((FileObject->Flags & FILE_OBJECT_FLAG_DIRTY_DATA) == 0) {
        KeAcquireQueuedLock(IoFileObjectsDirtyListLock);
        RtlAtomicOr32(&(FileObject->Flags), FILE_OBJECT_FLAG_DIRTY_DATA);
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

        KeReleaseQueuedLock(IoFileObjectsDirtyListLock);
        IopSchedulePageCacheThread();
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

    if ((FileObject->Flags & FILE_OBJECT_FLAG_DIRTY_PROPERTIES) == 0) {
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
        }
    }

    return;
}

VOID
IopCheckDirtyFileObjectsList (
    VOID
    )

/*++

Routine Description:

    This routine iterates over all file objects, checking to make sure they're
    properly marked dirty and in the dirty list if they have dirty entries.
    This routine is slow and should only be used while actively debugging
    dirty data that won't flush.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PFILE_OBJECT FileObject;
    PRED_BLACK_TREE_NODE Node;

    KeAcquireQueuedLock(IoFileObjectsLock);
    KeAcquireQueuedLock(IoFileObjectsDirtyListLock);
    Node = RtlRedBlackTreeGetLowestNode(&IoFileObjectsTree);
    while (Node != NULL) {
        FileObject = RED_BLACK_TREE_VALUE(Node, FILE_OBJECT, TreeEntry);
        if (!LIST_EMPTY(&(FileObject->DirtyPageList))) {
            if (IS_FILE_OBJECT_CLEAN(FileObject)) {
                RtlDebugPrint("FILE_OBJECT 0x%x marked as clean with "
                              "non-empty dirty list.\n",
                              FileObject);
            }

            if (FileObject->ListEntry.Next == NULL) {
                RtlDebugPrint("FILE_OBJECT 0x%x dirty but not in dirty list.\n",
                              FileObject);
            }
        }

        Node = RtlRedBlackTreeGetNextNode(&IoFileObjectsTree, FALSE, Node);
    }

    KeReleaseQueuedLock(IoFileObjectsDirtyListLock);
    KeReleaseQueuedLock(IoFileObjectsLock);
    return;
}

PIO_ASYNC_STATE
IopGetAsyncState (
    PIO_OBJECT_STATE State
    )

/*++

Routine Description:

    This routine returns or attempts to create the asynchronous state for an
    I/O object state.

Arguments:

    State - Supplies a pointer to the I/O object state.

Return Value:

    Returns a pointer to the async state on success. This may have just been
    created.

    NULL if no async state exists and none could be created.

--*/

{

    PIO_ASYNC_STATE Async;
    PIO_ASYNC_STATE OldValue;

    if (State->Async != NULL) {
        return State->Async;
    }

    Async = MmAllocatePagedPool(sizeof(IO_ASYNC_STATE),
                                FILE_OBJECT_ALLOCATION_TAG);

    if (Async == NULL) {
        return NULL;
    }

    RtlZeroMemory(Async, sizeof(IO_ASYNC_STATE));
    INITIALIZE_LIST_HEAD(&(Async->ReceiverList));
    Async->Lock = KeCreateQueuedLock();
    if (Async->Lock == NULL) {
        goto GetAsyncStateEnd;
    }

    //
    // Try to atomically set the async state. Someone else may race and win.
    //

    OldValue = (PIO_ASYNC_STATE)RtlAtomicCompareExchange(
                                                       (PUINTN)&(State->Async),
                                                       (UINTN)Async,
                                                       (UINTN)NULL);

    if (OldValue == NULL) {
        Async = NULL;
    }

GetAsyncStateEnd:
    if (Async != NULL) {
        IopDestroyAsyncState(Async);
    }

    return State->Async;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopFlushFileObjectProperties (
    PFILE_OBJECT FileObject,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes the file properties for the given file object. The
    file object lock must already be held at least shared.

Arguments:

    FileObject - Supplies a pointer to a file object.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    IRP_MINOR_CODE MinorCode;
    ULONG OldFlags;
    KSTATUS Status;

    ASSERT(KeIsSharedExclusiveLockHeld(FileObject->Lock));

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
    }

    Status = STATUS_SUCCESS;

FlushFilePropertiesEnd:
    return Status;
}

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

VOID
IopDestroyAsyncState (
    PIO_ASYNC_STATE Async
    )

/*++

Routine Description:

    This routine destroys the given asynchronous state.

Arguments:

    Async - Supplies a pointer to the state to destroy.

Return Value:

    None.

--*/

{

    ASSERT(LIST_EMPTY(&(Async->ReceiverList)));

    if (Async->Lock != NULL) {
        KeDestroyQueuedLock(Async->Lock);
    }

    MmFreePagedPool(Async);
    return;
}

VOID
IopSendIoSignal (
    PIO_ASYNC_STATE Async,
    USHORT SignalCode,
    ULONG BandEvent
    )

/*++

Routine Description:

    This routine sends an IO signal to the given process or process group.

Arguments:

    Async - Supplies a pointer to the async state.

    SignalCode - Supplies a signal code to include.

    BandEvent - Supplies the band event to include.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    THREAD_IDENTITY Destination;
    PROCESS_ID ProcessId;
    PSIGNAL_QUEUE_ENTRY QueueEntry;
    PASYNC_IO_RECEIVER Receiver;
    ULONG Signal;
    KSTATUS Status;

    //
    // Currently, the signal can only be sent to a single process. To support
    // process groups, the appropriate permission checking would need to be
    // done for each process in the group.
    //

    ProcessId = Async->Owner;
    if (ProcessId <= 0) {
        return;
    }

    KeAcquireQueuedLock(Async->Lock);

    //
    // Ensure that whoever set the owner has permission to send a signal to
    // the owner.
    //

    Status = PsGetProcessIdentity(ProcessId, &Destination);
    if (!KSUCCESS(Status)) {
        goto SendIoSignalEnd;
    }

    if ((!PERMISSION_CHECK(Async->SetterPermissions, PERMISSION_KILL)) &&
        (Async->SetterUserId != Destination.RealUserId) &&
        (Async->SetterUserId != Destination.SavedUserId) &&
        (Async->SetterEffectiveUserId != Destination.RealUserId) &&
        (Async->SetterEffectiveUserId != Destination.SavedUserId)) {

        goto SendIoSignalEnd;
    }

    //
    // Find the receiver to ensure the caller has in fact signed up for
    // asynchronous I/O signals.
    //

    CurrentEntry = Async->ReceiverList.Next;
    while (CurrentEntry != &(Async->ReceiverList)) {
        Receiver = LIST_VALUE(CurrentEntry, ASYNC_IO_RECEIVER, ListEntry);
        if (Receiver->ProcessId == ProcessId) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry == &(Async->ReceiverList)) {
        goto SendIoSignalEnd;
    }

    if (Async->Signal == 0) {
        Signal = SIGNAL_ASYNCHRONOUS_IO_COMPLETE;

    } else {
        Signal = Async->Signal;
    }

    QueueEntry = MmAllocatePagedPool(sizeof(SIGNAL_QUEUE_ENTRY),
                                     FILE_OBJECT_ALLOCATION_TAG);

    if (QueueEntry != NULL) {
        RtlZeroMemory(QueueEntry, sizeof(SIGNAL_QUEUE_ENTRY));
        QueueEntry->Parameters.SignalNumber = Signal;

        ASSERT(SignalCode > SIGNAL_CODE_USER);

        QueueEntry->Parameters.SignalCode = SignalCode;
        QueueEntry->Parameters.FromU.Poll.BandEvent = BandEvent;
        QueueEntry->Parameters.FromU.Poll.Descriptor = Receiver->Descriptor;
        QueueEntry->CompletionRoutine = PsDefaultSignalCompletionRoutine;
    }

    Status = PsSignalProcessId(ProcessId, Signal, QueueEntry);
    if (!KSUCCESS(Status)) {
        MmFreePagedPool(QueueEntry);
        goto SendIoSignalEnd;
    }

    QueueEntry = NULL;

SendIoSignalEnd:
    KeReleaseQueuedLock(Async->Lock);
    return;
}

