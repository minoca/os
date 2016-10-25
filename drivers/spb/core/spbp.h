/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spbp.h

Abstract:

    This header contains internal definitions for the Simple Peripheral Bus
    core library driver.

Author:

    Evan Green 14-Aug-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#define SPB_API __DLLEXPORT

#include <minoca/spb/spbhost.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the Simple Peripheral Bus allocation Tag: SbpA.
//

#define SPB_ALLOCATION_TAG 0x41627053
#define SPB_CONTROLLER_MAGIC SPB_ALLOCATION_TAG
#define SPB_HANDLE_MAGIC 0x42627053

#define SBP_CONTROLLER_INFORMATION_MAX_VERSION 0x0001000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the internal data of a Simple Peripheral Bus
    device handle.

Members:

    ListEntry - Stores pointers to the next and previous open handles in the
        controller.

    Magic - Stores the constant SPB_HANDLE_MAGIC.

    BusReferenceCount - Stores the number of references on the bus itself. If
        this changes to non-zero, then the bus lock is acquired. If this
        changes to zero, the bus lock is released.

    Controller - Stores a pointer back to the controller that owns this handle.

    Configuration - Stores a pointer to the configuration required by the
        device.

    Event - Stores a pointer to the event used for synchronous execution.

--*/

typedef struct _SPB_HANDLE_DATA {
    LIST_ENTRY ListEntry;
    ULONG Magic;
    volatile ULONG BusReferenceCount;
    PSPB_CONTROLLER Controller;
    PRESOURCE_SPB_DATA Configuration;
    PKEVENT Event;
} SPB_HANDLE_DATA, *PSPB_HANDLE_DATA;

/*++

Structure Description:

    This structure stores the internal data of a Simple Peripheral Bus library
    controller.

Members:

    Magic - Stores the constant SPB_CONTROLLER_MAGIC.

    Host - Stores the host controller information.

    Interface - Stores the public published interface.

    HandleList - Stores the head of the list of open bus handles.

    ArbiterCreated - Stores a boolean indicating whether or not the SPB
        arbiter has been created yet.

    Lock - Stores a pointer to the lock serializing access to internal data
        structures.

    BusLock - Stores a pointer to the lock representing whether or not the
        bus is claimed.

    CurrentConfiguration - Stores a pointer to the current configuration of
        the bus.

    TransferQueue - Stores the head of the list of transfer sets queued on the
        controller.

    CurrentSet - Stores a pointer to the current transfer set in progress. This
        is cleared when the transfer is finished. Setting this requires holding
        the controller lock.

--*/

struct _SPB_CONTROLLER {
    ULONG Magic;
    SPB_CONTROLLER_INFORMATION Host;
    SPB_INTERFACE Interface;
    LIST_ENTRY HandleList;
    BOOL ArbiterCreated;
    PQUEUED_LOCK Lock;
    PQUEUED_LOCK BusLock;
    PRESOURCE_SPB_DATA CurrentConfiguration;
    LIST_ENTRY TransferQueue;
    PSPB_TRANSFER_SET CurrentSet;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
