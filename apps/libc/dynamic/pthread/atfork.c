/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    atfork.c

Abstract:

    This module implements support for the fork callback routines.

Author:

    Evan Green 4-May-2015

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
// ------------------------------------------------------ Data Type Definitions
//

typedef
void
(*PPTHREAD_ATFORK_ROUTINE) (
    void
    );

/*++

Routine Description:

    This routine represents the function prototype for a fork callback.

Arguments:

    None.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure describes a registered fork callback.

Members:

    ListEntry - Stores pointers to the next and previous entries.

    PrepareRoutine - Stores an optional pointer to a routine to call before
        forking.

    ChildRoutine - Stores an optional pointer to a routine to call in the
        child after forking.

    ParentRoutine - Stores an optional pointer to a routine to call in the
        parent after forking.

    DynamicObjectHandle - Stores an optional pointer identifying which dynamic
        object these functions belong to.

--*/

typedef struct _PTHREAD_ATFORK_ENTRY {
    LIST_ENTRY ListEntry;
    PPTHREAD_ATFORK_ROUTINE PrepareRoutine;
    PPTHREAD_ATFORK_ROUTINE ChildRoutine;
    PPTHREAD_ATFORK_ROUTINE ParentRoutine;
    PVOID DynamicObjectHandle;
} PTHREAD_ATFORK_ENTRY, *PPTHREAD_ATFORK_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

pthread_mutex_t ClAtforkMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
LIST_ENTRY ClAtforkList;

//
// ------------------------------------------------------------------ Functions
//

PTHREAD_API
int
__register_atfork (
    void (*PrepareRoutine)(void),
    void (*ParentRoutine)(void),
    void (*ChildRoutine)(void),
    void *DynamicObjectHandle
    )

/*++

Routine Description:

    This routine is called to register an at-fork handler, remembering the
    dynamic object it was registered from.

Arguments:

    PrepareRoutine - Supplies an optional pointer to a routine to be called
        immediately before a fork operation.

    ParentRoutine - Supplies an optional pointer to a routine to be called
        after a fork in the parent process.

    ChildRoutine - Supplies an optional pointer to a routine to be called
        after a fork in the child process.

    DynamicObjectHandle - Supplies an identifier unique to the dynamic object
        registering the handlers. This can be used to unregister the handles if
        the dynamic object is unloaded.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATFORK_ENTRY NewEntry;

    NewEntry = malloc(sizeof(PTHREAD_ATFORK_ENTRY));
    if (NewEntry == NULL) {
        return ENOMEM;
    }

    NewEntry->PrepareRoutine = PrepareRoutine;
    NewEntry->ParentRoutine = ParentRoutine;
    NewEntry->ChildRoutine = ChildRoutine;
    NewEntry->DynamicObjectHandle = DynamicObjectHandle;
    pthread_mutex_lock(&ClAtforkMutex);
    if (ClAtforkList.Next == NULL) {
        INITIALIZE_LIST_HEAD(&ClAtforkList);
    }

    INSERT_BEFORE(&(NewEntry->ListEntry), &ClAtforkList);
    pthread_mutex_unlock(&ClAtforkMutex);
    return 0;
}

void
ClpUnregisterAtfork (
    void *DynamicObjectHandle
    )

/*++

Routine Description:

    This routine unregisters any at-fork handlers registered with the given
    dynamic object handle.

Arguments:

    DynamicObjectHandle - Supplies the value unique to the dynamic object. All
        handlers registered with this same value will be removed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PPTHREAD_ATFORK_ENTRY Entry;

    pthread_mutex_lock(&ClAtforkMutex);
    if (ClAtforkList.Next == NULL) {
        goto UnregisterAtforkEnd;
    }

    CurrentEntry = ClAtforkList.Next;
    while (CurrentEntry != &ClAtforkList) {
        Entry = LIST_VALUE(CurrentEntry, PTHREAD_ATFORK_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Entry->DynamicObjectHandle == DynamicObjectHandle) {
            LIST_REMOVE(&(Entry->ListEntry));
            free(Entry);
        }
    }

UnregisterAtforkEnd:
    pthread_mutex_unlock(&ClAtforkMutex);
    return;
}

VOID
ClpRunAtforkPrepareRoutines (
    VOID
    )

/*++

Routine Description:

    This routine calls the prepare routine for any fork handlers.

Arguments:

    None.

Return Value:

    None. This function returns with the at-fork mutex held.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PPTHREAD_ATFORK_ENTRY Entry;

    pthread_mutex_lock(&ClAtforkMutex);
    if (ClAtforkList.Next == NULL) {
        return;
    }

    //
    // Walk backwards through the registration list.
    //

    CurrentEntry = ClAtforkList.Previous;
    while (CurrentEntry != &ClAtforkList) {
        Entry = LIST_VALUE(CurrentEntry, PTHREAD_ATFORK_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Previous;
        if (Entry->PrepareRoutine != NULL) {
            Entry->PrepareRoutine();
        }
    }

    return;
}

VOID
ClpRunAtforkChildRoutines (
    VOID
    )

/*++

Routine Description:

    This routine calls the child routine for any fork handlers. This routine
    must obviously be called from a newly forked child. This function assumes
    that the at-fork mutex is held, and re-initializes it.

Arguments:

    None.

Return Value:

    None.

--*/

{

    pthread_mutexattr_t Attribute;
    PLIST_ENTRY CurrentEntry;
    PPTHREAD_ATFORK_ENTRY Entry;

    pthread_mutexattr_init(&Attribute);
    pthread_mutexattr_settype(&Attribute, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&ClAtforkMutex, &Attribute);
    pthread_mutexattr_destroy(&Attribute);
    if (ClAtforkList.Next == NULL) {
        return;
    }

    CurrentEntry = ClAtforkList.Next;
    while (CurrentEntry != &ClAtforkList) {
        Entry = LIST_VALUE(CurrentEntry, PTHREAD_ATFORK_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Entry->ChildRoutine != NULL) {
            Entry->ChildRoutine();
        }
    }

    return;
}

VOID
ClpRunAtforkParentRoutines (
    VOID
    )

/*++

Routine Description:

    This routine calls the child routine for any fork handlers. This routine
    must obviously be called from a newly forked child. This function assumes
    that the at-fork mutex is held, and releases it.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PPTHREAD_ATFORK_ENTRY Entry;

    if (ClAtforkList.Next != NULL) {
        CurrentEntry = ClAtforkList.Next;
        while (CurrentEntry != &ClAtforkList) {
            Entry = LIST_VALUE(CurrentEntry, PTHREAD_ATFORK_ENTRY, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if (Entry->ParentRoutine != NULL) {
                Entry->ParentRoutine();
            }
        }
    }

    pthread_mutex_unlock(&ClAtforkMutex);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

