/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    convert.c

Abstract:

    This module implements the type conversion interface subsystem. The
    interfaces are used to translate between C library types and native system
    types.

Author:

    Chris Stevens 24-Mar-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
// Store the global list of type conversion interfaces, protected by a global
// lock.
//

LIST_ENTRY ClTypeConversionInterfaceList;
pthread_mutex_t ClTypeConversionInterfaceLock;

//
// ------------------------------------------------------------------ Functions
//

BOOL
ClpInitializeTypeConversions (
    VOID
    )

/*++

Routine Description:

    This routine initializes the type conversion subsystem of the C library.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    INITIALIZE_LIST_HEAD(&ClTypeConversionInterfaceList);
    pthread_mutex_init(&ClTypeConversionInterfaceLock, NULL);
    return TRUE;
}

LIBC_API
KSTATUS
ClRegisterTypeConversionInterface (
    CL_CONVERSION_TYPE Type,
    PVOID Interface,
    BOOL Register
    )

/*++

Routine Description:

    This routine registers or deregisters a C library type conversion interface.

Arguments:

    Type - Supplies the type of the conversion interface being registered.

    Interface - Supplies a pointer to the type conversion interface. This
        memory will be used by the library; the caller cannot release it or
        stack allocate it.

    Register - Supplies a boolean indicating whether or not the given interface
        should be registered or de-registered.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PCL_TYPE_CONVERSION_INTERFACE Entry;
    PCL_TYPE_CONVERSION_INTERFACE ExistingEntry;
    PCL_TYPE_CONVERSION_INTERFACE NewEntry;
    KSTATUS Status;

    ExistingEntry = NULL;
    NewEntry = NULL;
    Status = STATUS_SUCCESS;

    //
    // Allocate and initialize the entry before acquiring the lock.
    //

    if (Register != FALSE) {
        NewEntry = malloc(sizeof(CL_TYPE_CONVERSION_INTERFACE));
        if (NewEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RegisterTypeConversionInterfaceEnd;
        }

        NewEntry->ListEntry.Next = NULL;
        NewEntry->Type = Type;
        NewEntry->Interface.Buffer = Interface;
    }

    //
    // Look for an existing entry matching the type and interface buffer.
    //

    pthread_mutex_lock(&ClTypeConversionInterfaceLock);
    CurrentEntry = ClTypeConversionInterfaceList.Next;
    while (CurrentEntry != &ClTypeConversionInterfaceList) {
        Entry = LIST_VALUE(CurrentEntry,
                           CL_TYPE_CONVERSION_INTERFACE,
                           ListEntry);

        CurrentEntry = CurrentEntry->Next;
        if ((Entry->Type == Type) && (Entry->Interface.Buffer == Interface)) {
            ExistingEntry = Entry;
            break;
        }
    }

    //
    // If there was an existing entry, remove and free it if necessary.
    //

    if (ExistingEntry != NULL) {
        if (Register == FALSE) {
            LIST_REMOVE(&(ExistingEntry->ListEntry));
            ExistingEntry->ListEntry.Next = NULL;

        //
        // The entry exists and the caller is trying to register it again.
        // Clear out the pointer so it's not freed at the end of this routine.
        //

        } else {
            ExistingEntry = NULL;
            Status = STATUS_DUPLICATE_ENTRY;
        }

    //
    // There was no previous entry, add the new one if needed.
    //

    } else {
        if (Register != FALSE) {
            INSERT_AFTER(&(NewEntry->ListEntry),
                         &ClTypeConversionInterfaceList);

            NewEntry = NULL;
        }
    }

RegisterTypeConversionInterfaceEnd:
    pthread_mutex_unlock(&ClTypeConversionInterfaceLock);
    if (ExistingEntry != NULL) {

        ASSERT(ExistingEntry->ListEntry.Next == NULL);

        free(ExistingEntry);
    }

    if (NewEntry != NULL) {

        ASSERT(NewEntry->ListEntry.Next == NULL);

        free(NewEntry);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

