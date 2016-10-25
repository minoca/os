/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    event.c

Abstract:

    This module implements UEFI core event services.

Author:

    Evan Green 4-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/uefi/guid/eventgrp.h>

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_EVENT_MAGIC 0x746F7645 // 'tnvE'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores timing information about a timer event.

Members:

    ListEntry - Stores pointers to the next and previous timer event structures.

    DueTime - Stores the time when the timer expires.

    Period - Stores the period of the timer, if periodic.

--*/

typedef struct _EFI_TIMER_EVENT {
    LIST_ENTRY ListEntry;
    UINT64 DueTime;
    UINT64 Period;
} EFI_TIMER_EVENT, *PEFI_TIMER_EVENT;

/*++

Structure Description:

    This structure stores the internal structure of an EFI event.

Members:

    Magic - Stores the magic constant EFI_EVENT_MAGIC.

    Type - Stores the type of event.

    SignalCount - Stores the number of times this event has been signaled.

    SignalListEntry - Stores pointers to the next and previous events in the
        signal queue.

    NotifyTpl - Stores the task priority level of the event.

    NotifyFunction - Stores a pointer to a function called when the event fires.

    NotifyContext - Stores a pointer's worth of data passed to the notify
        function.

    EventGroup - Stores the GUID of the event group this timer is in.

    NotifyListEntry - Stores pointers to the next and previous entries in the
        notify list.

    EventEx - Stores a boolean indicating if this event was created with the
        Ex function or the regular one.

    RuntimeData - Stores runtime data about the event.

    TimerData - Stores timer event data.

--*/

typedef struct _EFI_EVENT_DATA {
    UINTN Magic;
    UINT32 Type;
    UINT32 SignalCount;
    LIST_ENTRY SignalListEntry;
    EFI_TPL NotifyTpl;
    EFI_EVENT_NOTIFY NotifyFunction;
    VOID *NotifyContext;
    EFI_GUID EventGroup;
    LIST_ENTRY NotifyListEntry;
    BOOLEAN EventEx;
    EFI_RUNTIME_EVENT_ENTRY RuntimeData;
    EFI_TIMER_EVENT TimerData;
} EFI_EVENT_DATA, *PEFI_EVENT_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipCoreCreateEvent (
    UINT32 Type,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    EFI_GUID *EventGroup,
    EFI_EVENT *Event
    );

VOID
EfipCoreNotifyEvent (
    PEFI_EVENT_DATA Event
    );

VOID
EfipCoreInsertEventTimer (
    PEFI_EVENT_DATA Event
    );

EFIAPI
VOID
EfipCoreCheckTimers (
    EFI_EVENT CheckEvent,
    VOID *Context
    );

EFIAPI
VOID
EfipCoreEmptyCallbackFunction (
    EFI_EVENT Event,
    VOID *Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store some well known event GUIDs.
//

EFI_GUID EfiEventExitBootServicesGuid = EFI_EVENT_GROUP_EXIT_BOOT_SERVICES;
EFI_GUID EfiEventVirtualAddressChangeGuid =
                                        EFI_EVENT_GROUP_VIRTUAL_ADDRESS_CHANGE;

EFI_GUID EfiEventMemoryMapChangeGuid = EFI_EVENT_GROUP_MEMORY_MAP_CHANGE;
EFI_GUID EfiEventReadyToBootGuid = EFI_EVENT_GROUP_READY_TO_BOOT;

//
// Store the idle loop event, which is signaled when there's nothing to do.
//

EFI_GUID EfiIdleLoopEventGuid = EFI_IDLE_LOOP_EVENT_GUID;
EFI_EVENT EfiIdleLoopEvent;

//
// Store the event queue.
//

EFI_LOCK EfiEventQueueLock;
LIST_ENTRY EfiEventQueue[TPL_HIGH_LEVEL + 1];
UINTN EfiEventsPending;

LIST_ENTRY EfiEventSignalQueue;

//
// Store the timer list.
//

EFI_LOCK EfiTimerLock;
LIST_ENTRY EfiTimerList;
EFI_EVENT EfiCheckTimerEvent;

//
// Store a table of valid event creation flags.
//

UINT32 EfiValidEventFlags[] = {
    EVT_TIMER | EVT_NOTIFY_SIGNAL,
    EVT_TIMER,
    EVT_NOTIFY_WAIT,
    EVT_NOTIFY_SIGNAL,
    EVT_SIGNAL_EXIT_BOOT_SERVICES,
    EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
    0,
    EVT_TIMER | EVT_NOTIFY_WAIT,
};

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiCoreCreateEvent (
    UINT32 Type,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    EFI_EVENT *Event
    )

/*++

Routine Description:

    This routine creates an event.

Arguments:

    Type - Supplies the type of event to create, as well as its mode and
        attributes.

    NotifyTpl - Supplies an optional task priority level of event notifications.

    NotifyFunction - Supplies an optional pointer to the event's notification
        function.

    NotifyContext - Supplies an optional context pointer that will be passed
        to the notify function when the event is signaled.

    Event - Supplies a pointer where the new event will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if one or more parameters are not valid.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

--*/

{

    EFI_STATUS Status;

    Status = EfiCoreCreateEventEx(Type,
                                  NotifyTpl,
                                  NotifyFunction,
                                  NotifyContext,
                                  NULL,
                                  Event);

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreCreateEventEx (
    UINT32 Type,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    EFI_GUID *EventGroup,
    EFI_EVENT *Event
    )

/*++

Routine Description:

    This routine creates an event.

Arguments:

    Type - Supplies the type of event to create, as well as its mode and
        attributes.

    NotifyTpl - Supplies an optional task priority level of event notifications.

    NotifyFunction - Supplies an optional pointer to the event's notification
        function.

    NotifyContext - Supplies an optional context pointer that will be passed
        to the notify function when the event is signaled.

    EventGroup - Supplies an optional pointer to the unique identifier of the
        group to which this event belongs. If this is NULL, the function
        behaves as if the parameters were passed to the original create event
        function.

    Event - Supplies a pointer where the new event will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if one or more parameters are not valid.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

--*/

{

    EFI_STATUS Status;

    if ((Type & (EVT_NOTIFY_WAIT | EVT_NOTIFY_SIGNAL)) != 0) {
        if ((NotifyTpl != TPL_APPLICATION) &&
            (NotifyTpl != TPL_CALLBACK) &&
            (NotifyTpl != TPL_NOTIFY)) {

            return EFI_INVALID_PARAMETER;
        }
    }

    Status = EfipCoreCreateEvent(Type,
                                 NotifyTpl,
                                 NotifyFunction,
                                 NotifyContext,
                                 EventGroup,
                                 Event);

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreCloseEvent (
    EFI_EVENT Event
    )

/*++

Routine Description:

    This routine closes an event.

Arguments:

    Event - Supplies the event to close.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the given event is invalid.

--*/

{

    PEFI_EVENT_DATA EventData;
    EFI_STATUS Status;

    EventData = Event;
    if (EventData == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (EventData->Magic != EFI_EVENT_MAGIC) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // If it's a timer event, cancel it.
    //

    if ((EventData->Type & EVT_TIMER) != 0) {
        EfiCoreSetTimer(EventData, TimerCancel, 0);
    }

    EfiCoreAcquireLock(&EfiEventQueueLock);
    if (EventData->RuntimeData.ListEntry.Next != NULL) {
        LIST_REMOVE(&(EventData->RuntimeData.ListEntry));
    }

    if (EventData->NotifyListEntry.Next != NULL) {
        LIST_REMOVE(&(EventData->NotifyListEntry));
    }

    if (EventData->SignalListEntry.Next != NULL) {
        LIST_REMOVE(&(EventData->SignalListEntry));
    }

    EfiCoreReleaseLock(&EfiEventQueueLock);

    //
    // If the event is registered on a protocol notify, remove it from the
    // protocol database.
    //

    EfipCoreUnregisterProtocolNotify(Event);
    Status = EfiCoreFreePool(EventData);

    ASSERT(!EFI_ERROR(Status));

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreSignalEvent (
    EFI_EVENT Event
    )

/*++

Routine Description:

    This routine signals an event.

Arguments:

    Event - Supplies the event to signal.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the given event is not valid.

--*/

{

    PEFI_EVENT_DATA EventData;

    EventData = Event;
    if ((EventData == NULL) || (EventData->Magic != EFI_EVENT_MAGIC)) {
        return EFI_INVALID_PARAMETER;
    }

    EfiCoreAcquireLock(&EfiEventQueueLock);
    if (EventData->SignalCount == 0) {
        EventData->SignalCount += 1;

        //
        // If the signal type is a notify function, queue it.
        //

        if ((EventData->Type & EVT_NOTIFY_SIGNAL) != 0) {

            //
            // If it's an event "Ex", then signal all members of the event
            // group.
            //

            if (EventData->EventEx != FALSE) {
                EfiCoreReleaseLock(&EfiEventQueueLock);
                EfipCoreNotifySignalList(&(EventData->EventGroup));
                EfiCoreAcquireLock(&EfiEventQueueLock);

            } else {
                EfipCoreNotifyEvent(EventData);
            }
        }
    }

    EfiCoreReleaseLock(&EfiEventQueueLock);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreCheckEvent (
    EFI_EVENT Event
    )

/*++

Routine Description:

    This routine checks whether or not an event is in the signaled state.

Arguments:

    Event - Supplies the event to check.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_READY if the event is not signaled.

    EFI_INVALID_PARAMETER if the event is of type EVT_NOTIFY_SIGNAL.

--*/

{

    PEFI_EVENT_DATA EventData;
    EFI_STATUS Status;

    EventData = Event;
    if ((EventData == NULL) || (EventData->Magic != EFI_EVENT_MAGIC) ||
        ((EventData->Type & EVT_NOTIFY_SIGNAL) != 0)) {

        return EFI_INVALID_PARAMETER;
    }

    Status = EFI_NOT_READY;
    if ((EventData->SignalCount == 0) &&
        ((EventData->Type & EVT_NOTIFY_WAIT) != 0)) {

        //
        // Queue the wait notify function.
        //

        EfiCoreAcquireLock(&EfiEventQueueLock);
        if (EventData->SignalCount == 0) {
            EfipCoreNotifyEvent(EventData);
        }

        EfiCoreReleaseLock(&EfiEventQueueLock);
    }

    if (EventData->SignalCount != 0) {
        EfiCoreAcquireLock(&EfiEventQueueLock);
        if (EventData->SignalCount != 0) {
            EventData->SignalCount = 0;
            Status = EFI_SUCCESS;
        }

        EfiCoreReleaseLock(&EfiEventQueueLock);
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreWaitForEvent (
    UINTN NumberOfEvents,
    EFI_EVENT *Event,
    UINTN *Index
    )

/*++

Routine Description:

    This routine stops execution until an event is signaled.

Arguments:

    NumberOfEvents - Supplies the number of events in the event array.

    Event - Supplies the array of EFI_EVENTs.

    Index - Supplies a pointer where the index of the event which satisfied the
        wait will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the number of events is zero, or the event
    indicated by the index return parameter is of type EVT_NOTIFY_SIGNAL.

    EFI_UNSUPPORTED if the current TPL is not TPL_APPLICATION.

--*/

{

    UINTN EventIndex;
    EFI_STATUS Status;

    if (NumberOfEvents == 0) {
        return EFI_INVALID_PARAMETER;
    }

    if (EfiCurrentTpl != TPL_APPLICATION) {
        return EFI_UNSUPPORTED;
    }

    while (TRUE) {
        for (EventIndex = 0; EventIndex < NumberOfEvents; EventIndex += 1) {
            Status = EfiCoreCheckEvent(Event[EventIndex]);
            if (Status != EFI_NOT_READY) {
                *Index = EventIndex;
                return Status;
            }
        }

        EfiCoreSignalEvent(&EfiIdleLoopEvent);
    }

    //
    // Execution never gets here.
    //

    ASSERT(FALSE);

    return EFI_NOT_READY;
}

EFIAPI
EFI_STATUS
EfiCoreSetTimer (
    EFI_EVENT Event,
    EFI_TIMER_DELAY Type,
    UINT64 TriggerTime
    )

/*++

Routine Description:

    This routine sets the type of timer and trigger time for a timer event.

Arguments:

    Event - Supplies the timer to set.

    Type - Supplies the type of trigger to set.

    TriggerTime - Supplies the number of 100ns units until the timer expires.
        Zero is legal, and means the timer will be signaled on the next timer
        tick.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the event or type is not valid.

--*/

{

    PEFI_EVENT_DATA EventData;
    UINT64 Frequency;

    EventData = Event;
    if ((EventData == NULL) || (EventData->Magic != EFI_EVENT_MAGIC)) {
        return EFI_INVALID_PARAMETER;
    }

    if (((UINT32)Type > TimerRelative) ||
        ((EventData->Type & EVT_TIMER) == 0)) {

        return EFI_INVALID_PARAMETER;
    }

    EfiCoreAcquireLock(&EfiTimerLock);

    //
    // If the timer is queued to a database, remove it.
    //

    if (EventData->TimerData.ListEntry.Next != NULL) {
        LIST_REMOVE(&(EventData->TimerData.ListEntry));
        EventData->TimerData.ListEntry.Next = NULL;
    }

    EventData->TimerData.DueTime = 0;
    EventData->TimerData.Period = 0;
    if (Type != TimerCancel) {
        Frequency = EfiCoreGetTimeCounterFrequency();
        TriggerTime = (TriggerTime * Frequency) / 10000000ULL;
        if (Type == TimerPeriodic) {
            if (TriggerTime == 0) {
                EventData->TimerData.Period = 1;

            } else {
                EventData->TimerData.Period = TriggerTime;
            }
        }

        EventData->TimerData.DueTime = EfiCoreReadTimeCounter() + TriggerTime;
        EfipCoreInsertEventTimer(EventData);
        if (TriggerTime == 0) {
            EfiCoreSignalEvent(&EfiCheckTimerEvent);
        }
    }

    EfiCoreReleaseLock(&EfiTimerLock);
    return EFI_SUCCESS;
}

EFI_STATUS
EfiCoreInitializeEventServices (
    UINTN Phase
    )

/*++

Routine Description:

    This routine initializes event support.

Arguments:

    Phase - Supplies the initialization phase. Valid values are 0 and 1.

Return Value:

    EFI Status code.

--*/

{

    UINTN Index;

    if (Phase == 0) {
        EfiCoreInitializeLock(&EfiEventQueueLock, TPL_HIGH_LEVEL);
        EfiCoreInitializeLock(&EfiTimerLock, TPL_HIGH_LEVEL - 1);
        for (Index = 0; Index <= TPL_HIGH_LEVEL; Index += 1) {
            INITIALIZE_LIST_HEAD(&(EfiEventQueue[Index]));
        }

        INITIALIZE_LIST_HEAD(&EfiEventSignalQueue);
        INITIALIZE_LIST_HEAD(&EfiTimerList);

    } else {

        ASSERT(Phase == 1);

        EfiCoreCreateEventEx(EVT_NOTIFY_SIGNAL,
                             TPL_NOTIFY,
                             EfipCoreEmptyCallbackFunction,
                             NULL,
                             &EfiIdleLoopEventGuid,
                             &EfiIdleLoopEvent);

        EfipCoreCreateEvent(EVT_NOTIFY_SIGNAL,
                            TPL_HIGH_LEVEL - 1,
                            EfipCoreCheckTimers,
                            NULL,
                            NULL,
                            &EfiCheckTimerEvent);
    }

    return EFI_SUCCESS;
}

VOID
EfiCoreDispatchEventNotifies (
    EFI_TPL Priority
    )

/*++

Routine Description:

    This routine dispatches all pending events.

Arguments:

    Priority - Supplies the task priority level of the event notifications to
        dispatch.

Return Value:

    None.

--*/

{

    PEFI_EVENT_DATA Event;
    PLIST_ENTRY ListHead;

    EfiCoreAcquireLock(&EfiEventQueueLock);

    ASSERT(EfiEventQueueLock.OwnerTpl == Priority);

    ListHead = &(EfiEventQueue[Priority]);
    while (LIST_EMPTY(ListHead) == FALSE) {
        Event = LIST_VALUE(ListHead->Next, EFI_EVENT_DATA, NotifyListEntry);

        ASSERT(Event->Magic == EFI_EVENT_MAGIC);

        LIST_REMOVE(&(Event->NotifyListEntry));
        Event->NotifyListEntry.Next = NULL;

        //
        // Only clear the signal status if it is a signal type event. Wait type
        // events are cleared in the check event function.
        //

        if ((Event->Type & EVT_NOTIFY_SIGNAL) != 0) {
            Event->SignalCount = 0;
        }

        //
        // Call the notification function without the lock held.
        //

        EfiCoreReleaseLock(&EfiEventQueueLock);
        Event->NotifyFunction(Event, Event->NotifyContext);
        EfiCoreAcquireLock(&EfiEventQueueLock);
    }

    EfiEventsPending &= ~(1 << Priority);
    EfiCoreReleaseLock(&EfiEventQueueLock);
    return;
}

VOID
EfipCoreTimerTick (
    UINT64 CurrentTime
    )

/*++

Routine Description:

    This routine is called when a clock interrupt comes in.

Arguments:

    CurrentTime - Supplies the new current time.

Return Value:

    None.

--*/

{

    PEFI_EVENT_DATA Event;

    if (LIST_EMPTY(&EfiTimerList) == FALSE) {
        Event = LIST_VALUE(EfiTimerList.Next,
                           EFI_EVENT_DATA,
                           TimerData.ListEntry);

        if (Event->TimerData.DueTime <= CurrentTime) {
            EfiCoreSignalEvent(EfiCheckTimerEvent);
        }
    }

    return;
}

VOID
EfipCoreNotifySignalList (
    EFI_GUID *EventGroup
    )

/*++

Routine Description:

    This routine signals all events in the given event group.

Arguments:

    EventGroup - Supplies a pointer to the GUID identifying the event group
        to signal.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_EVENT_DATA Event;

    EfiCoreAcquireLock(&EfiEventQueueLock);
    CurrentEntry = EfiEventSignalQueue.Next;
    while (CurrentEntry != &EfiEventSignalQueue) {
        Event = LIST_VALUE(CurrentEntry, EFI_EVENT_DATA, SignalListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (EfiCoreCompareGuids(&(Event->EventGroup), EventGroup) != FALSE) {
            EfipCoreNotifyEvent(Event);
        }
    }

    EfiCoreReleaseLock(&EfiEventQueueLock);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipCoreCreateEvent (
    UINT32 Type,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    EFI_GUID *EventGroup,
    EFI_EVENT *Event
    )

/*++

Routine Description:

    This routine creates an event.

Arguments:

    Type - Supplies the type of event to create, as well as its mode and
        attributes.

    NotifyTpl - Supplies an optional task priority level of event notifications.

    NotifyFunction - Supplies an optional pointer to the event's notification
        function.

    NotifyContext - Supplies an optional context pointer to pass when the event
        is signaled.

    EventGroup - Supplies an optional pointer to the unique identifier of the
        group to which this event belongs. If this is NULL, the function
        behaves as if the parameters were passed to the original create event
        function.

    Event - Supplies a pointer where the new event will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if one or more parameters are not valid.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

--*/

{

    UINTN EntryCount;
    UINTN Index;
    BOOLEAN Match;
    EFI_EVENT_DATA *NewEvent;
    EFI_STATUS Status;

    if (Event == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Check to make sure a valid combination of flags is set.
    //

    EntryCount = sizeof(EfiValidEventFlags) / sizeof(EfiValidEventFlags[0]);
    Status = EFI_INVALID_PARAMETER;
    for (Index = 0; Index < EntryCount; Index += 1) {
        if (Type == EfiValidEventFlags[Index]) {
            Status = EFI_SUCCESS;
            break;
        }
    }

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Convert the event type for pre-existing event groups.
    //

    if (EventGroup != NULL) {
        if ((Type == EVT_SIGNAL_EXIT_BOOT_SERVICES) ||
            (Type == EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE)) {

            return EFI_INVALID_PARAMETER;
        }

        if (EfiCoreCompareGuids(EventGroup, &EfiEventExitBootServicesGuid) !=
            FALSE) {

            Type = EVT_SIGNAL_EXIT_BOOT_SERVICES;

        } else {
            Match = EfiCoreCompareGuids(EventGroup,
                                        &EfiEventVirtualAddressChangeGuid);

            if (Match != FALSE) {
                Type = EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE;
            }
        }

    } else {
        if (Type == EVT_SIGNAL_EXIT_BOOT_SERVICES) {
            EventGroup = &EfiEventExitBootServicesGuid;

        } else if (Type == EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE) {
            EventGroup = &EfiEventVirtualAddressChangeGuid;
        }
    }

    //
    // If it's a notify type event, check parameters.
    //

    if ((Type & (EVT_NOTIFY_WAIT | EVT_NOTIFY_SIGNAL)) != 0) {
        if ((NotifyFunction == NULL) ||
            (NotifyTpl <= TPL_APPLICATION) ||
            (NotifyTpl >= TPL_HIGH_LEVEL)) {

            return EFI_INVALID_PARAMETER;
        }

    //
    // No notifications are needed.
    //

    } else {
        NotifyTpl = 0;
        NotifyFunction = NULL;
        NotifyContext = NULL;
    }

    //
    // Allocate and initialize the new event.
    //

    if ((Type & EVT_RUNTIME) != 0) {
        NewEvent = EfiCoreAllocateRuntimePool(sizeof(EFI_EVENT_DATA));

    } else {
        NewEvent = EfiCoreAllocateBootPool(sizeof(EFI_EVENT_DATA));
    }

    if (NewEvent == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    EfiCoreSetMemory(NewEvent, sizeof(EFI_EVENT_DATA), 0);
    NewEvent->Magic = EFI_EVENT_MAGIC;
    NewEvent->Type = Type;
    NewEvent->NotifyTpl = NotifyTpl;
    NewEvent->NotifyFunction = NotifyFunction;
    NewEvent->NotifyContext = NotifyContext;
    if (EventGroup != NULL) {
        EfiCoreCopyMemory(&(NewEvent->EventGroup),
                          EventGroup,
                          sizeof(EFI_GUID));

        NewEvent->EventEx = TRUE;
    }

    *Event = NewEvent;

    //
    // Keep a list of all the runtime events specifically.
    //

    if ((Type & EVT_RUNTIME) != 0) {
        NewEvent->RuntimeData.Type = Type;
        NewEvent->RuntimeData.NotifyTpl = NotifyTpl;
        NewEvent->RuntimeData.NotifyFunction = NotifyFunction;
        NewEvent->RuntimeData.NotifyContext = NotifyContext;
        NewEvent->RuntimeData.Event = (EFI_EVENT *)NewEvent;
        INSERT_BEFORE(&(NewEvent->RuntimeData.ListEntry),
                      &(EfiRuntimeProtocol->EventListHead));
    }

    EfiCoreAcquireLock(&EfiEventQueueLock);
    if ((Type & EVT_NOTIFY_SIGNAL) != 0) {
        INSERT_AFTER(&(NewEvent->SignalListEntry), &EfiEventSignalQueue);
    }

    EfiCoreReleaseLock(&EfiEventQueueLock);
    return EFI_SUCCESS;
}

VOID
EfipCoreNotifyEvent (
    PEFI_EVENT_DATA Event
    )

/*++

Routine Description:

    This routine notifies the given event.

Arguments:

    Event - Supplies the event to notify.

Return Value:

    None.

--*/

{

    ASSERT(EfiCoreIsLockHeld(&EfiEventQueueLock) != FALSE);

    //
    // If the event is queued somewhere, remove it.
    //

    if (Event->NotifyListEntry.Next != NULL) {
        LIST_REMOVE(&(Event->NotifyListEntry));
        Event->NotifyListEntry.Next = NULL;
    }

    INSERT_BEFORE(&(Event->NotifyListEntry),
                  &(EfiEventQueue[Event->NotifyTpl]));

    EfiEventsPending = 1 << Event->NotifyTpl;
    return;
}

VOID
EfipCoreInsertEventTimer (
    PEFI_EVENT_DATA Event
    )

/*++

Routine Description:

    This routine inserts the given timer event into the global list.

Arguments:

    Event - Supplies the timer event to insert.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_EVENT_DATA SearchEvent;

    ASSERT(EfiCoreIsLockHeld(&EfiTimerLock) != FALSE);

    CurrentEntry = EfiTimerList.Next;
    while (CurrentEntry != &EfiTimerList) {
        SearchEvent = LIST_VALUE(CurrentEntry,
                                 EFI_EVENT_DATA,
                                 TimerData.ListEntry);

        if (SearchEvent->TimerData.DueTime > Event->TimerData.DueTime) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    INSERT_BEFORE(&(Event->TimerData.ListEntry), CurrentEntry);
    return;
}

EFIAPI
VOID
EfipCoreCheckTimers (
    EFI_EVENT CheckEvent,
    VOID *Context
    )

/*++

Routine Description:

    This routine checks the sorted timer list against the current system time,
    and signals any expired timers.

Arguments:

    CheckEvent - Supplies the event that fired, the EFI check timer event.

    Context - Supplies a context pointer associated with the event, which is
        not used.

Return Value:

    None.

--*/

{

    PEFI_EVENT_DATA Event;
    UINT64 TimeCounter;

    TimeCounter = EfiCoreReadTimeCounter();
    EfiCoreAcquireLock(&EfiTimerLock);
    while (LIST_EMPTY(&EfiTimerList) == FALSE) {
        Event = LIST_VALUE(EfiTimerList.Next,
                           EFI_EVENT_DATA,
                           TimerData.ListEntry);

        //
        // If this timer is not expired, then neither is anything after it,
        // so break.
        //

        if (Event->TimerData.DueTime > TimeCounter) {
            break;
        }

        LIST_REMOVE(&(Event->TimerData.ListEntry));
        Event->TimerData.ListEntry.Next = NULL;
        EfiCoreSignalEvent(Event);

        //
        // If this is a periodic timer, compute the next due time and set it
        // again.
        //

        if (Event->TimerData.Period != 0) {
            Event->TimerData.DueTime += Event->TimerData.Period;

            //
            // If the new due time is still in the past, reset the timer to
            // start from now.
            //

            if (Event->TimerData.DueTime < TimeCounter) {
                Event->TimerData.DueTime = TimeCounter;
                EfiCoreSignalEvent(EfiCheckTimerEvent);
            }

            EfipCoreInsertEventTimer(Event);
        }
    }

    EfiCoreReleaseLock(&EfiTimerLock);
    return;
}

EFIAPI
VOID
EfipCoreEmptyCallbackFunction (
    EFI_EVENT Event,
    VOID *Context
    )

/*++

Routine Description:

    This routine implements a null callback that does nothing but returns.

Arguments:

    Event - Supplies the event that fired.

    Context - Supplies a context pointer associated with the event.

Return Value:

    None.

--*/

{

    return;
}

