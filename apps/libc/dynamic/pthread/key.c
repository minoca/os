/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    key.c

Abstract:

    This module implements support for POSIX thread keys, which are the POSIX
    notion of thread-local storage.

Author:

    Evan Green 1-May-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "pthreadp.h"
#include <assert.h>
#include <limits.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns non-zero if the given key is valid.
//

#define PTHREAD_VALID_KEY(_Key)                 \
    ((((_Key) & PTHREAD_KEY_VALID) != 0) &&     \
     (((_Key) & ~PTHREAD_KEY_VALID) < PTHREAD_KEYS_MAX))

//
// ---------------------------------------------------------------- Definitions
//

//
// This bit is set when the key is in use.
//

#define PTHREAD_KEY_IN_USE 0x00000001

//
// This is the increment for the sequence number. It both toggles the in-use
// bit and acts as part of the sequence number.
//

#define PTHREAD_KEY_SEQUENCE_INCREMENT 1

//
// This bit is part of what is returned to the user.
//

#define PTHREAD_KEY_VALID 0x80000000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
void
(*PPTHREAD_KEY_DESTRUCTOR) (
    void *Value
    );

/*++

Routine Description:

    This routine is called when a thread with thread-local storage for a
    particular key exits.

Arguments:

    Value - Supplies the thread-local key value.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores information for a thread key (thread local storage).

Members:

    Sequence - Stores the sequence number of the key. The first bit is used
        to determine if the key is in use.

    Destructor - Stores the pointer to the optional destructor function called
        when the key is deleted.

--*/

typedef struct _PTHREAD_KEY {
    UINTN Sequence;
    UINTN Destructor;
} PTHREAD_KEY, *PPTHREAD_KEY;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the dynamically allocated thread key map.
//

PPTHREAD_KEY ClThreadKeys;

//
// ------------------------------------------------------------------ Functions
//

PTHREAD_API
int
pthread_key_create (
    pthread_key_t *Key,
    void (*KeyDestructorRoutine)(void *)
    )

/*++

Routine Description:

    This routine attempts to create and reserve a new thread key.

Arguments:

    Key - Supplies a pointer where the key information will be returned.

    KeyDestructorRoutine - Supplies an optional pointer to a routine to call
        when the key is destroyed on a particular thread. This routine will
        be called with a pointer to the thread-specific value for the key.

Return Value:

    0 on success.

    EAGAIN if the system lacked the resources to create a new key slot, or
    there are too many keys.

    ENOMEM if insufficient memory exists to create the key.

--*/

{

    UINTN KeyIndex;
    PPTHREAD_KEY Keys;
    UINTN NewOldValue;
    UINTN NewValue;
    UINTN OldValue;

    //
    // Allocate the key structure if needed. This may race with other threads
    // trying to do the same, so be prepared to lose and back out.
    //

    if (ClThreadKeys == NULL) {
        Keys = malloc(sizeof(PTHREAD_KEY) * PTHREAD_KEYS_MAX);
        if (Keys == NULL) {
            return ENOMEM;
        }

        memset(Keys, 0, sizeof(PTHREAD_KEY) * PTHREAD_KEYS_MAX);

        //
        // Try to make this the official array.
        //

        OldValue = RtlAtomicCompareExchange((PUINTN)&ClThreadKeys,
                                            (UINTN)Keys,
                                            (UINTN)NULL);

        if (OldValue != (UINTN)NULL) {

            //
            // Someone else beat this thread to the punch. Use their array.
            //

            free(Keys);
        }
    }

    assert(ClThreadKeys != NULL);

    //
    // Loop trying to find a free key.
    //

    for (KeyIndex = 0; KeyIndex < PTHREAD_KEYS_MAX; KeyIndex += 1) {
        OldValue = ClThreadKeys[KeyIndex].Sequence;
        while ((OldValue & PTHREAD_KEY_IN_USE) == 0) {

            //
            // Try to jam in an incremented sequence number that will have the
            // in-use bit set.
            //

            NewValue = OldValue + PTHREAD_KEY_SEQUENCE_INCREMENT;
            NewOldValue = RtlAtomicCompareExchange(
                                            &(ClThreadKeys[KeyIndex].Sequence),
                                            NewValue,
                                            OldValue);

            if (NewOldValue == OldValue) {
                *Key = KeyIndex | PTHREAD_KEY_VALID;
                ClThreadKeys[KeyIndex].Destructor =
                                                 (UINTN)(KeyDestructorRoutine);

                return 0;
            }

            OldValue = NewOldValue;
        }
    }

    *Key = -1;

    //
    // No keys could be located.
    //

    return EAGAIN;
}

PTHREAD_API
int
pthread_key_delete (
    pthread_key_t Key
    )

/*++

Routine Description:

    This routine releases a thread key. It is the responsibility of the
    application to release any thread-specific data associated with the old key.
    No destructors are called from this function.

Arguments:

    Key - Supplies a pointer to the key to delete.

Return Value:

    0 on success.

    EINVAL if the key is invalid.

--*/

{

    UINTN Index;
    UINTN NewValue;
    UINTN OldValue;

    if (!PTHREAD_VALID_KEY(Key)) {
        return EINVAL;
    }

    Index = Key & ~PTHREAD_KEY_VALID;
    OldValue = ClThreadKeys[Index].Sequence;
    if ((OldValue & PTHREAD_KEY_IN_USE) == 0) {
        return EINVAL;
    }

    //
    // Try to increment the sequence number and clear the in-use bit.
    //

    NewValue = RtlAtomicCompareExchange(
                                     &(ClThreadKeys[Index].Sequence),
                                     OldValue + PTHREAD_KEY_SEQUENCE_INCREMENT,
                                     OldValue);

    if (NewValue == OldValue) {
        return 0;
    }

    //
    // The sequence number changed out from underneath this function. The
    // caller is double deleting somewhere.
    //

    return EINVAL;
}

PTHREAD_API
void *
pthread_getspecific (
    pthread_key_t Key
    )

/*++

Routine Description:

    This routine returns the thread-specific value for the given key.

Arguments:

    Key - Supplies a pointer to the key whose value should be returned.

Return Value:

    Returns the last value set for the current thread and key combination, or
    NULL if no value has been set or the key is not valid.

--*/

{

    UINTN Index;
    PPTHREAD Thread;

    if ((!PTHREAD_VALID_KEY(Key)) || (ClThreadKeys == NULL)) {
        return NULL;
    }

    Index = Key & ~PTHREAD_KEY_VALID;
    Thread = (PPTHREAD)pthread_self();
    if (Thread->KeyData[Index].Sequence == ClThreadKeys[Index].Sequence) {
        return Thread->KeyData[Index].Value;
    }

    //
    // The caller passed us a key that has since been deleted.
    //

    return NULL;
}

PTHREAD_API
int
pthread_setspecific (
    pthread_key_t Key,
    const void *Value
    )

/*++

Routine Description:

    This routine sets the thread-specific value for the given key and current
    thread.

Arguments:

    Key - Supplies the key whose value should be set.

    Value - Supplies the value to set.

Return Value:

    0 on success.

    EINVAL if the key passed was invalid.

--*/

{

    UINTN Index;
    PPTHREAD_KEY_DATA KeyData;
    UINTN Sequence;
    PPTHREAD Thread;

    if (!PTHREAD_VALID_KEY(Key)) {
        return EINVAL;
    }

    Index = Key & ~PTHREAD_KEY_VALID;

    assert(Index < PTHREAD_KEYS_MAX);
    assert(ClThreadKeys != NULL);

    Thread = (PPTHREAD)pthread_self();
    KeyData = &(Thread->KeyData[Index]);
    Sequence = ClThreadKeys[Index].Sequence;
    if ((Sequence & PTHREAD_KEY_IN_USE) != 0) {
        KeyData->Sequence = Sequence;
        KeyData->Value = (PVOID)Value;
        return 0;
    }

    //
    // The caller asked to set a key that is not in use.
    //

    return EINVAL;
}

VOID
ClpDestroyThreadKeyData (
    PPTHREAD Thread
    )

/*++

Routine Description:

    This routine destroys the thread key data for the given thread and calls
    all destructor routines.

Arguments:

    Thread - Supplies a pointer to the thread that is exiting.

Return Value:

    None.

--*/

{

    PPTHREAD_KEY_DESTRUCTOR Destructor;
    UINTN DestructorsCalled;
    UINTN DestructorValue;
    UINTN Index;
    UINTN Round;
    UINTN Sequence;
    void *Value;

    if (ClThreadKeys == NULL) {
        Thread->KeyData = NULL;
        return;
    }

    for (Round = 0; Round < PTHREAD_DESTRUCTOR_ITERATIONS; Round += 1) {
        DestructorsCalled = 0;
        for (Index = 0; Index < PTHREAD_KEYS_MAX; Index += 1) {
            Sequence = ClThreadKeys[Index].Sequence;
            DestructorValue = ClThreadKeys[Index].Destructor;

            //
            // If the key is in use and the thread-local value is valid, the
            // destructor needs to be called.
            //

            if (((Sequence & PTHREAD_KEY_IN_USE) != 0) &&
                (Sequence == Thread->KeyData[Index].Sequence) &&
                (Thread->KeyData[Index].Value != NULL)) {

                Destructor = (PPTHREAD_KEY_DESTRUCTOR)(DestructorValue);
                if (Destructor == NULL) {
                    continue;
                }

                //
                // Clear out the value (so this only happens once) and call
                // the destructor routine.
                //

                Value = Thread->KeyData[Index].Value;
                Thread->KeyData[Index].Value = NULL;
                Destructor(Value);
                DestructorsCalled += 1;
            }
        }

        //
        // If no destructors were called, then stop doing rounds of looping.
        //

        if (DestructorsCalled == 0) {
            break;
        }
    }

    Thread->KeyData = NULL;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

