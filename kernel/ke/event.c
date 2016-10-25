/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    event.c

Abstract:

    This module impelements kernel events.

Author:

    Evan Green 12-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

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
// Directory for events with no parent, used primarily to keep the root
// directory uncluttered.
//

POBJECT_HEADER KeEventDirectory = NULL;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PKEVENT
KeCreateEvent (
    PVOID ParentObject
    )

/*++

Routine Description:

    This routine creates a kernel event. It comes initialized to Not Signaled.

Arguments:

    ParentObject - Supplies an optional parent object to create the event
        under.

Return Value:

    Returns a pointer to the event, or NULL if the event could not be created.

--*/

{

    PKEVENT Event;

    if (ParentObject == NULL) {
        ParentObject = KeEventDirectory;
    }

    Event = ObCreateObject(ObjectEvent,
                           ParentObject,
                           NULL,
                           0,
                           sizeof(KEVENT),
                           NULL,
                           0,
                           KE_EVENT_ALLOCATION_TAG);

    return Event;
}

KERNEL_API
VOID
KeDestroyEvent (
    PKEVENT Event
    )

/*++

Routine Description:

    This routine destroys an event created with KeCreateEvent. The event is no
    longer valid after this call.

Arguments:

    Event - Supplies a pointer to the event to free.

Return Value:

    None.

--*/

{

    ObReleaseReference(Event);
    return;
}

KERNEL_API
KSTATUS
KeWaitForEvent (
    PKEVENT Event,
    BOOL Interruptible,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine waits until an event enters a signaled state.

Arguments:

    Event - Supplies a pointer to the event to wait for.

    Interruptible - Supplies a boolean indicating whether or not the wait can
        be interrupted if a signal is sent to the process on which this thread
        runs. If TRUE is supplied, the caller must check the return status
        code to find out if the wait was really satisfied or just interrupted.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        objects should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on these objects.

Return Value:

    Status code.

--*/

{

    ULONG Flags;

    Flags = 0;
    if (Interruptible != FALSE) {
        Flags = WAIT_FLAG_INTERRUPTIBLE;
    }

    return ObWaitOnObject(Event, Flags, TimeoutInMilliseconds);
}

KERNEL_API
VOID
KeSignalEvent (
    PKEVENT Event,
    SIGNAL_OPTION Option
    )

/*++

Routine Description:

    This routine sets an event to the given signal state.

Arguments:

    Event - Supplies a pointer to the event to signal or unsignal.

    Option - Supplies the signaling behavior to apply.

Return Value:

    None.

--*/

{

    ObSignalObject(Event, Option);
    return;
}

KERNEL_API
SIGNAL_STATE
KeGetEventState (
    PKEVENT Event
    )

/*++

Routine Description:

    This routine returns the signal state of an event.

Arguments:

    Event - Supplies a pointer to the event to get the state of.

Return Value:

    Returns the signal state of the event.

--*/

{

    return Event->Header.WaitQueue.State;
}

//
// --------------------------------------------------------- Internal Functions
//

