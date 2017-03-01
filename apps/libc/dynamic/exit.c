/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    exit.c

Abstract:

    This module implements functionality associated with the ending of a
    program.

Author:

    Evan Green 11-Mar-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define ABORT_RAISE_FAILURE_STATUS 127

//
// Define the number of atexit handlers per block.
//

#define AT_EXIT_BLOCK_SIZE 32

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
void
(*PCXA_AT_EXIT_ROUTINE) (
    void *Parameter
    );

/*++

Routine Description:

    This routine is the prototype for a routine registered to be called via
    the __cxa_atexit routine.

Arguments:

    Parameter - Supplies the parameter supplied on registration.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a registered exit handler.

Members:

    AtExitRoutine - Stores the pointer to the exit routine handler. This is
        NULL if the slot is free.

    Argument - Stores the argument to pass to the cxa_atexit handler.

    SharedObject - Stores a pointer to the shared object the cxa_atexit handler
        is for.

--*/

typedef struct _AT_EXIT_HANDLER {
    PCXA_AT_EXIT_ROUTINE AtExitRoutine;
    PVOID Argument;
    PVOID SharedObject;
} AT_EXIT_HANDLER, *PAT_EXIT_HANDLER;

/*++

Structure Description:

    This structure defines a block of functions to call when the process
    exits normally.

Members:

    ListEntry - Stores the list entry for the block. For the global at exit
        block, this is the head of the list of child blocks. For the child
        blocks, this is the list entry storing sibling blocks.

    Handlers - Stores the array of atexit handlers.

--*/

typedef struct _AT_EXIT_BLOCK {
    LIST_ENTRY ListEntry;
    AT_EXIT_HANDLER Handlers[AT_EXIT_BLOCK_SIZE];
} AT_EXIT_BLOCK, *PAT_EXIT_BLOCK;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ClpRegisterExitHandler (
    PVOID Routine,
    PVOID Argument,
    PVOID SharedObject
    );

VOID
ClpCallExitHandlers (
    PVOID SharedObject
    );

VOID
ClpAcquireAtExitLock (
    VOID
    );

VOID
ClpReleaseAtExitLock (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

ULONG ClAtExitLock;

//
// Store the first block of atexit registration routines, which don't require
// dynamic allocation. In this block, the list entry is the head of the list.
//

AT_EXIT_BLOCK ClAtExitBlock;

//
// Store a boolean indicating if atexit has been called to know whether to
// restart calling the exit handlers.
//

BOOL ClAtExitCalled = FALSE;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
__NO_RETURN
void
abort (
    void
    )

/*++

Routine Description:

    This routine causes abnormal process termination to occur, unless the
    signal SIGABRT is being caught and the signal handler does not return. The
    abort function shall override ignoring or blocking of the SIGABRT signal.

Arguments:

    None.

Return Value:

    This routine does not return.

--*/

{

    sigset_t Mask;

    ClpFlushAllStreams(TRUE, NULL);
    sigemptyset(&Mask);
    sigaddset(&Mask, SIGABRT);
    sigprocmask(SIG_UNBLOCK, &Mask, NULL);
    while (TRUE) {
        if (raise(SIGABRT) != 0) {
            _exit(ABORT_RAISE_FAILURE_STATUS);
        }
    }
}

LIBC_API
__NO_RETURN
void
exit (
    int Status
    )

/*++

Routine Description:

    This routine terminates the current process, calling any routines registered
    to run upon exiting.

Arguments:

    Status - Supplies a status code to return to the parent program.

Return Value:

    None. This routine does not return.

--*/

{

    ClpCallExitHandlers(NULL);
    ClpFlushAllStreams(FALSE, NULL);
    _Exit(Status);
}

LIBC_API
__NO_RETURN
void
_exit (
    int Status
    )

/*++

Routine Description:

    This routine terminates the current process. It does not call any routines
    registered to run upon exit.

Arguments:

    Status - Supplies a status code to return to the parent program.

Return Value:

    None. This routine does not return.

--*/

{

    _Exit(Status);
}

LIBC_API
__NO_RETURN
void
_Exit (
    int Status
    )

/*++

Routine Description:

    This routine terminates the current process. It does not call any routines
    registered to run upon exit.

Arguments:

    Status - Supplies a status code to return to the parent program.

Return Value:

    None. This routine does not return.

--*/

{

    OsExitProcess(Status);

    //
    // Execution should never get here.
    //

    while (TRUE) {
        raise(SIGABRT);
    }
}

LIBC_API
int
__cxa_atexit (
    void (*DestructorFunction)(void *),
    void *Argument,
    void *SharedObject
    )

/*++

Routine Description:

    This routine is called to register a global static destructor function.

Arguments:

    DestructorFunction - Supplies a pointer to the function to call.

    Argument - Supplies an argument to pass the function when it is called.

    SharedObject - Supplies a pointer to the shared object this destructor is
        associated with.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;

    Result = ClpRegisterExitHandler(DestructorFunction,
                                    Argument,
                                    SharedObject);

    return Result;
}

LIBC_API
void
__cxa_finalize (
    void *SharedObject
    )

/*++

Routine Description:

    This routine is called when a shared object unloads. It calls the static
    destructors.

Arguments:

    SharedObject - Supplies a pointer to the shared object being destroyed, or
        0 if all destructors should be called.

Return Value:

    None.

--*/

{

    ClpCallExitHandlers(SharedObject);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ClpRegisterExitHandler (
    PVOID Routine,
    PVOID Argument,
    PVOID SharedObject
    )

/*++

Routine Description:

    This routine registers a function to be called when the process exits
    normally via a call to exit or a return from main. Calls to exec clear
    the list of registered exit functions. This routine may allocate memory.
    Functions are called in the reverse order in which they were registered.

Arguments:

    Routine - Supplies a pointer to the routine to call, which is either of
        type PAT_EXIT_ROUTINE or PCXA_AT_EXIT_ROUTINE.

    Argument - Supplies the argument to pass to cxa_atexit routines.

    SharedObject - Supplies the shared object this routine is attached to for
        cxa_atexit handlers.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PAT_EXIT_BLOCK Block;
    PAT_EXIT_HANDLER Handler;
    ULONG Index;
    INT Status;

    if (Routine == NULL) {
        return EINVAL;
    }

    ClpAcquireAtExitLock();
    ClAtExitCalled = TRUE;

    //
    // Initialize the list head if not yet done.
    //

    if (ClAtExitBlock.ListEntry.Next == NULL) {
        INITIALIZE_LIST_HEAD(&(ClAtExitBlock.ListEntry));
    }

    //
    // Look in the last block for a free slot.
    //

    Status = ENOMEM;
    Block = LIST_VALUE(ClAtExitBlock.ListEntry.Previous,
                       AT_EXIT_BLOCK,
                       ListEntry);

    for (Index = 0; Index < AT_EXIT_BLOCK_SIZE; Index += 1) {
        Handler = &(Block->Handlers[Index]);
        if (Handler->AtExitRoutine == NULL) {
            Handler->AtExitRoutine = Routine;
            Handler->Argument = Argument;
            Handler->SharedObject = SharedObject;
            Status = 0;
            goto RegisterExitHandlerEnd;
        }
    }

    //
    // Create a new block.
    //

    Block = malloc(sizeof(AT_EXIT_BLOCK));
    if (Block == NULL) {
        Status = ENOMEM;
        goto RegisterExitHandlerEnd;
    }

    memset(Block, 0, sizeof(AT_EXIT_BLOCK));
    INSERT_BEFORE(&(Block->ListEntry), &(ClAtExitBlock.ListEntry));
    Handler->AtExitRoutine = Routine;
    Handler->Argument = Argument;
    Handler->SharedObject = SharedObject;
    Status = 0;

RegisterExitHandlerEnd:
    ClpReleaseAtExitLock();
    return Status;
}

VOID
ClpCallExitHandlers (
    PVOID SharedObject
    )

/*++

Routine Description:

    This routine calls the exit handlers, either all of them or only for a
    particular shared object.

Arguments:

    SharedObject - Supplies an optional pointer to the shared object being
        unloaded. If NULL, then all at exit handlers are called.

Return Value:

    None. This routine does not return.

--*/

{

    PCXA_AT_EXIT_ROUTINE AtExitRoutine;
    PAT_EXIT_BLOCK Block;
    PLIST_ENTRY CurrentEntry;
    PAT_EXIT_HANDLER Handler;
    LONG Index;

    //
    // If the list entry is not initialized, no one has ever called into the
    // exit handler registration routine.
    //

    if (ClAtExitBlock.ListEntry.Next == NULL) {
        return;
    }

    do {
        ClAtExitCalled = FALSE;

        //
        // Handle the blocks in the reverse order.
        //

        CurrentEntry = ClAtExitBlock.ListEntry.Previous;
        while (TRUE) {
            Block = LIST_VALUE(CurrentEntry, AT_EXIT_BLOCK, ListEntry);
            for (Index = AT_EXIT_BLOCK_SIZE - 1; Index >= 0; Index -= 1) {
                Handler = &(Block->Handlers[Index]);
                if (Handler->AtExitRoutine == NULL) {
                    continue;
                }

                //
                // If only calling handlers for a specific shared object being
                // unloaded, skip all handlers that aren't registers in the
                // shared object.
                //

                if (SharedObject != NULL) {
                    if (Handler->SharedObject != SharedObject) {
                        continue;
                    }
                }

                //
                // Atomic exchange in the free value so that only one fella
                // gets to call the exit handler. Handler slots don't get
                // get reused so don't worry about that.
                //

                AtExitRoutine = (PVOID)RtlAtomicExchange(
                                             (PUINTN)&(Handler->AtExitRoutine),
                                             (UINTN)NULL);

                if (AtExitRoutine == NULL) {
                    continue;
                }

                AtExitRoutine(Handler->Argument);

                //
                // If the handler called atexit, start over.
                //

                if (ClAtExitCalled != FALSE) {
                    break;
                }
            }

            if (ClAtExitCalled != FALSE) {
                break;
            }

            if (Block == &ClAtExitBlock) {
                break;
            }

            CurrentEntry = CurrentEntry->Previous;
        }

    } while (ClAtExitCalled != FALSE);

    if (SharedObject != NULL) {
        ClpUnregisterAtfork(SharedObject);
    }

    return;
}

VOID
ClpAcquireAtExitLock (
    VOID
    )

/*++

Routine Description:

    This routine acquires the global lock protecting the structures storing the
    functions to be called upon exit.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG OriginalValue;

    //
    // Loop trying to acquire the lock and also checking to see if the process
    // has already started exiting.
    //

    while (TRUE) {
        OriginalValue = RtlAtomicCompareExchange32(&ClAtExitLock, 1, 0);
        if (OriginalValue == 0) {
            break;
        }
    }

    return;
}

VOID
ClpReleaseAtExitLock (
    VOID
    )

/*++

Routine Description:

    This routine releases the global lock protecting the structures storing the
    functions to be called upon exit.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG PreviousValue;

    PreviousValue = RtlAtomicExchange32(&ClAtExitLock, 0);

    assert(PreviousValue == 1);

    return;
}

