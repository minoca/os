/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sysres.c

Abstract:

    This module implements management of builtin system resources.

Author:

    Evan Green 17-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "keinit.h"
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SYSTEM_RESOURCE_ALLOCATION_TAG 0x52737953 // 'RsyS'

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
// Define a lock that protects access to the system resources and the system
// resource list head.
//

KSPIN_LOCK KeSystemResourceSpinLock;
LIST_ENTRY KeSystemResourceListHead;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PSYSTEM_RESOURCE_HEADER
KeAcquireSystemResource (
    SYSTEM_RESOURCE_TYPE ResourceType
    )

/*++

Routine Description:

    This routine attempts to find an unacquired system resource of the given
    type.

Arguments:

    ResourceType - Supplies the type of builtin resource to acquire.

Return Value:

    Returns a pointer to a resource of the given type on success.

    NULL on failure.

--*/

{

    return KepGetSystemResource(ResourceType, TRUE);
}

KERNEL_API
VOID
KeReleaseSystemResource (
    PSYSTEM_RESOURCE_HEADER ResourceHeader
    )

/*++

Routine Description:

    This routine releases a system resource.

Arguments:

    ResourceHeader - Supplies a pointer to the resource header to release back
        to the system.

Return Value:

    None.

--*/

{

    BOOL Enabled;

    Enabled = ArDisableInterrupts();
    KeAcquireSpinLock(&KeSystemResourceSpinLock);
    ResourceHeader->Acquired = FALSE;
    KeReleaseSpinLock(&KeSystemResourceSpinLock);
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    return;
}

KSTATUS
KepInitializeSystemResources (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Phase
    )

/*++

Routine Description:

    This routine initializes the system resource manager.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

    Phase - Supplies the phase. Valid values are 0 and 1.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSYSTEM_RESOURCE_HEADER GenericHeader;
    ULONG NewEntrySize;
    PSYSTEM_RESOURCE_HEADER NewHeader;
    KSTATUS Status;
    LIST_ENTRY TemporaryListHead;

    Status = STATUS_SUCCESS;

    //
    // In phase 0, initialize the spin lock and move all resources off of the
    // loader block. Pools are not yet available.
    //

    if (Phase == 0) {
        KeInitializeSpinLock(&KeSystemResourceSpinLock);
        INITIALIZE_LIST_HEAD(&KeSystemResourceListHead);
        if (LIST_EMPTY(&(Parameters->SystemResourceListHead)) == FALSE) {
            MOVE_LIST(&(Parameters->SystemResourceListHead),
                      &KeSystemResourceListHead);

            INITIALIZE_LIST_HEAD(&(Parameters->SystemResourceListHead));
        }

    } else {

        ASSERT(Phase == 1);

        //
        // In preparation for all boot mappings being released, reallocate
        // each entry in non-paged pool. Start by putting all current entries
        // on a temporary list.
        //

        INITIALIZE_LIST_HEAD(&TemporaryListHead);
        if (LIST_EMPTY(&KeSystemResourceListHead) == FALSE) {
            MOVE_LIST(&KeSystemResourceListHead, &TemporaryListHead);
            INITIALIZE_LIST_HEAD(&KeSystemResourceListHead);
        }

        //
        // Grab each item off the temporary list.
        //

        while (!LIST_EMPTY(&TemporaryListHead)) {
            CurrentEntry = TemporaryListHead.Next;
            LIST_REMOVE(CurrentEntry);
            GenericHeader = LIST_VALUE(CurrentEntry,
                                       SYSTEM_RESOURCE_HEADER,
                                       ListEntry);

            //
            // Determine the size of the entry.
            //

            switch (GenericHeader->Type) {
            case SystemResourceFrameBuffer:
                NewEntrySize = sizeof(SYSTEM_RESOURCE_FRAME_BUFFER);
                break;

            case SystemResourceHardwareModule:
                NewEntrySize = sizeof(SYSTEM_RESOURCE_HARDWARE_MODULE);
                break;

            case SystemResourceRamDisk:
                NewEntrySize = sizeof(SYSTEM_RESOURCE_RAM_DISK);
                break;

            case SystemResourceDebugDevice:
                NewEntrySize = sizeof(SYSTEM_RESOURCE_DEBUG_DEVICE);
                break;

            //
            // Unknown resource type. Something bad has probably happened.
            //

            default:

                ASSERT(FALSE);

                Status = STATUS_UNSUCCESSFUL;
                goto InitializeSystemResourcesEnd;
            }

            //
            // Allocate and initialize the new entry.
            //

            NewHeader = MmAllocateNonPagedPool(NewEntrySize,
                                               SYSTEM_RESOURCE_ALLOCATION_TAG);

            if (NewHeader == NULL) {
                Status = STATUS_NO_MEMORY;
                goto InitializeSystemResourcesEnd;
            }

            RtlCopyMemory(NewHeader, GenericHeader, NewEntrySize);

            //
            // Insert the new entry onto the main list. Simply drop the old
            // boot entry as it will get cleaned up later during MM
            // initialization.
            //

            INSERT_BEFORE(&(NewHeader->ListEntry), &KeSystemResourceListHead);
        }
    }

InitializeSystemResourcesEnd:
    return Status;
}

PSYSTEM_RESOURCE_HEADER
KepGetSystemResource (
    SYSTEM_RESOURCE_TYPE ResourceType,
    BOOL Acquire
    )

/*++

Routine Description:

    This routine attempts to find an unacquired system resource of the given
    type.

Arguments:

    ResourceType - Supplies the type of builtin resource to acquire.

    Acquire - Supplies a boolean indicating if the resource should be acquired
        or not.

Return Value:

    Returns a pointer to a resource of the given type on success.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    BOOL Enabled;
    PSYSTEM_RESOURCE_HEADER Header;

    //
    // Acquire the high level lock.
    //

    Enabled = ArDisableInterrupts();
    KeAcquireSpinLock(&KeSystemResourceSpinLock);
    CurrentEntry = KeSystemResourceListHead.Next;
    while (CurrentEntry != &KeSystemResourceListHead) {
        Header = LIST_VALUE(CurrentEntry, SYSTEM_RESOURCE_HEADER, ListEntry);
        if ((Header->Type == ResourceType) && (Header->Acquired == FALSE)) {
            if (Acquire != FALSE) {
                Header->Acquired = TRUE;
            }

            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry == &KeSystemResourceListHead) {
        Header = NULL;
    }

    //
    // Release the high level lock.
    //

    KeReleaseSpinLock(&KeSystemResourceSpinLock);
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    return Header;
}

//
// --------------------------------------------------------- Internal Functions
//

