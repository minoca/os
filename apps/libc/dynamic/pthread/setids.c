/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    setids.c

Abstract:

    This module implements the logic to make setuid and friends calls work
    across all threads.

Author:

    Evan Green 1-May-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "pthreadp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to wait for a set ID request to go through, in
// seconds.
//

#define PTHREAD_SETID_TIMEOUT 60

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores details for a setuid (and friends) request.

Members:

    SetGroups - Stores a boolean indicating if this is a set supplementary
        groups call (TRUE) or a set thread identity call.

    Fields - Stores the fields to set in the thread identity.

    Identity - Stores a pointer to the thread identity to set.

    Groups - Stores a pointer to an array of supplementary group IDs to set.

    GroupCount - Stores the number of elements in the supplementary group ID
        array.

    Thread - Stores the thread the request is directed to.

    Mutex - Stores the mutex guarding the condition.

    Condition - Stores the condition variable.

--*/

typedef struct _PTHREAD_SETID_REQUEST {
    BOOL SetGroups;
    ULONG Fields;
    PTHREAD_IDENTITY Identity;
    PGROUP_ID Groups;
    UINTN GroupCount;
    pthread_t Thread;
    pthread_mutex_t Mutex;
    pthread_cond_t Condition;
} PTHREAD_SETID_REQUEST, *PPTHREAD_SETID_REQUEST;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ClpExecuteSetIdRequest (
    PPTHREAD_SETID_REQUEST Request
    );

//
// -------------------------------------------------------------------- Globals
//

PPTHREAD_SETID_REQUEST ClSetIdRequest;

//
// ------------------------------------------------------------------ Functions
//

VOID
ClpSetThreadIdentityOnAllThreads (
    ULONG Fields,
    PTHREAD_IDENTITY Identity
    )

/*++

Routine Description:

    This routine uses a signal to set the thread identity on all threads
    except the current one (which is assumed to have already been set).

Arguments:

    Fields - Supplies the bitfield of identity fields to set. See
        THREAD_IDENTITY_FIELD_* definitions.

    Identity - Supplies a pointer to the thread identity information to set.

Return Value:

    None.

--*/

{

    PTHREAD_SETID_REQUEST Request;

    //
    // If threading's not been fired up, nothing needs to be done.
    //

    if (ClThreadList.Next == NULL) {
        return;
    }

    RtlZeroMemory(&Request, sizeof(PTHREAD_SETID_REQUEST));
    Request.SetGroups = FALSE;
    Request.Fields = Fields;
    Request.Identity = Identity;
    ClpExecuteSetIdRequest(&Request);
    return;
}

VOID
ClpSetSupplementaryGroupsOnAllThreads (
    PGROUP_ID GroupIds,
    UINTN GroupIdCount
    )

/*++

Routine Description:

    This routine uses a signal to set the supplementary groups on all threads
    except the current one (which is assumed to have already been set).

Arguments:

    GroupIds - Supplies a pointer to the array of group IDs to set.

    GroupIdCount - Supplies the number of elements in the group ID array.

Return Value:

    None.

--*/

{

    PTHREAD_SETID_REQUEST Request;

    //
    // If threading's not been fired up, nothing needs to be done.
    //

    if (ClThreadList.Next == NULL) {
        return;
    }

    RtlZeroMemory(&Request, sizeof(PTHREAD_SETID_REQUEST));
    Request.SetGroups = TRUE;
    Request.Groups = GroupIds;
    Request.GroupCount = GroupIdCount;
    ClpExecuteSetIdRequest(&Request);
    return;
}

void
ClpSetIdSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine is a signal handler called to fix up the user identity on a
    thread.

Arguments:

    Signal - Supplies the signal that caused this handler to be invoked.

Return Value:

    None.

--*/

{

    PPTHREAD_SETID_REQUEST Request;

    Request = ClSetIdRequest;

    //
    // Ignore spurious requests. Note that this is not foolproof, as the
    // request might be set now but be destroyed in just a moment if a request
    // is not actually going through.
    //

    if ((Request == NULL) || (Request->Thread != pthread_self())) {
        abort();
    }

    if (Request->SetGroups != FALSE) {
        OsSetSupplementaryGroups(TRUE, Request->Groups, &(Request->GroupCount));

    } else {
        OsSetThreadIdentity(Request->Fields, Request->Identity);
    }

    pthread_mutex_lock(&(Request->Mutex));
    Request->Thread = 0;
    pthread_cond_signal(&(Request->Condition));
    pthread_mutex_unlock(&(Request->Mutex));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ClpExecuteSetIdRequest (
    PPTHREAD_SETID_REQUEST Request
    )

/*++

Routine Description:

    This routine uses a signal to set the thread identity on all threads
    except the current one (which is assumed to have already been set).

Arguments:

    Request - Supplies a pointer to the request to execute.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    KSTATUS KernelStatus;
    PPTHREAD Self;
    int Status;
    PPTHREAD Thread;
    struct timespec Timeout;

    pthread_mutex_init(&(Request->Mutex), NULL);
    pthread_mutex_lock(&(Request->Mutex));
    Self = (PPTHREAD)pthread_self();
    pthread_mutex_lock(&ClThreadListMutex);
    ClSetIdRequest = Request;
    CurrentEntry = ClThreadList.Next;
    while (CurrentEntry != &ClThreadList) {
        Thread = LIST_VALUE(CurrentEntry, PTHREAD, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Thread == Self) {
            continue;
        }

        Request->Thread = (pthread_t)Thread;

        //
        // Fire off the request. Allow for the possibility that the thread has
        // died, which is okay.
        //

        KernelStatus = OsSendSignal(SignalTargetThread,
                                    Thread->ThreadId,
                                    SIGNAL_SETID,
                                    SIGNAL_CODE_USER,
                                    0);

        if (KernelStatus == STATUS_NO_SUCH_THREAD) {
            continue;
        }

        if (!KSUCCESS(KernelStatus)) {
            fprintf(stderr,
                    "Error: Failed to signal thread %x: %d\n",
                    Thread,
                    KernelStatus);

            abort();
        }

        clock_gettime(CLOCK_REALTIME_COARSE, &Timeout);
        Timeout.tv_sec += PTHREAD_SETID_TIMEOUT;
        while (Request->Thread == (pthread_t)Thread) {
            Status = pthread_cond_timedwait(&(Request->Condition),
                                            &(Request->Mutex),
                                            &Timeout);

            if (Status == ETIMEDOUT) {
                fprintf(stderr,
                        "Error: Thread %x failed to respond to set ID "
                        "request.\n",
                        Thread);

                abort();
            }
        }
    }

    ClSetIdRequest = NULL;
    pthread_mutex_unlock(&ClThreadListMutex);
    pthread_mutex_unlock(&(Request->Mutex));
    pthread_mutex_destroy(&(Request->Mutex));
    return;
}

