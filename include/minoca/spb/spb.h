/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spb.h

Abstract:

    This header contains definitions for the Simple Peripheral Bus
    interface.

Author:

    Evan Green 14-Aug-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Interface UUID for the Simple Peripheral Bus.
//

#define UUID_SPB_INTERFACE \
    {{0xC56A4C6F, 0xA81547D7, 0xA8DE4E74, 0x0853B3D5}}

//
// Define flags that go on an individual SPB transfer.
//

//
// This flag is set automatically by the SPB library on the first transfer of
// a transfer set.
//

#define SPB_TRANSFER_FLAG_FIRST 0x00000001

//
// This flag is set automatically by the SPB library on the last transfer of
// a transfer set.
//

#define SPB_TRANSFER_FLAG_LAST 0x00000002

//
// Define the set of SPB transfer flags that are set automatically by the SPB
// library.
//

#define SPB_TRANSFER_FLAG_AUTO_MASK \
    (SPB_TRANSFER_FLAG_FIRST | SPB_TRANSFER_FLAG_LAST)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the transfer directions for bus tranfers. Both is for busses like SPI
// that can simultaneously read and write in full duplex mode.
//

typedef enum _SPB_TRANSFER_DIRECTION {
    SpbTransferDirectionInvalid,
    SpbTransferDirectionIn,
    SpbTransferDirectionOut,
    SpbTransferDirectionBoth
} SPB_TRANSFER_DIRECTION, *PSPB_TRANSFER_DIRECTION;

//
// Define the type of an open SPB connection.
//

typedef PVOID SPB_HANDLE, *PSPB_HANDLE;
typedef struct _SPB_INTERFACE SPB_INTERFACE, *PSPB_INTERFACE;
typedef struct _SPB_TRANSFER_SET SPB_TRANSFER_SET, *PSPB_TRANSFER_SET;

typedef
VOID
(*PSPB_TRANSFER_COMPLETION_CALLBACK) (
    PSPB_TRANSFER_SET TransferSet
    );

/*++

Routine Description:

    This routine is called when a transfer set has completed or errored out.

Arguments:

    TransferSet - Supplies a pointer to the transfer set that completed.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores information about a grouped set of transfers on a
    Simple Peripheral Bus.

Members:

    ListEntry - Stores a list entry used internally to keep transfer sets on a
        queue.

    Handle - Stores the handle that has queued this transfer set. This will be
        set by the SPB library upon submitting the transfer.

    TransferList - Stores the head of the list of transfers to execute in this
        set.

    Flags - Stores a set of flags governing the behavior of this transfer
        sequence.

    EntriesProcessed - Stores the number of entries that were completely or
        partially processed.

    Status - Stores the resulting status code of the transfer attempt.

    CompletionRoutine - Stores a pointer to the routine to call when the
        transfer has completed.

    CompletionContext - Stores a pointer's worth of context that the completion
        routine can use. The SPB bus functions do not touch this value.

--*/

struct _SPB_TRANSFER_SET {
    LIST_ENTRY ListEntry;
    SPB_HANDLE Handle;
    LIST_ENTRY TransferList;
    ULONG Flags;
    UINTN EntriesProcessed;
    KSTATUS Status;
    PSPB_TRANSFER_COMPLETION_CALLBACK CompletionRoutine;
    PVOID Context;
};

/*++

Structure Description:

    This structure stores information about a single Simple Peripheral Bus
    transfer.

Members:

    ListEntry - Stores pointers to the next and previous transfers in the
        transfer set.

    Direction - Stores the transfer direction. For "both" direction transfers,
        the same buffer will be used for both input and output.

    IoBuffer - Stores a pointer to the I/O buffer.

    Offset - Stores the offset within the I/O buffer for this data transfer
        portion.

    Size - Stores the size of the transfer in bytes. It is an error if this
        size does not translate evenly to bus sized words (where each word is
        rounded up to its next power of 2).

    ReceiveSizeCompleted - Stores the number of bytes that have been
        successfully received.

    TransmitSizeCompleted - Stores the number of bytes that have been
        successfully transmitted.

    MicrosecondDelay - Stores the minimum number of microseconds to delay
        before executing this transfer. If this is the first transfer, the
        device will be activated first (chip select, etc) before the delay
        begins. The delay may end up being larger than this value.

    Flags - Stores a bitfield of bus-specific flags regarding this transfer.

--*/

typedef struct _SPB_TRANSFER {
    LIST_ENTRY ListEntry;
    SPB_TRANSFER_DIRECTION Direction;
    PIO_BUFFER IoBuffer;
    UINTN Offset;
    UINTN Size;
    UINTN ReceiveSizeCompleted;
    UINTN TransmitSizeCompleted;
    ULONG MicrosecondDelay;
    ULONG Flags;
} SPB_TRANSFER, *PSPB_TRANSFER;

typedef
KSTATUS
(*PSPB_OPEN) (
    PSPB_INTERFACE Interface,
    PRESOURCE_SPB_DATA Configuration,
    PSPB_HANDLE Handle
    );

/*++

Routine Description:

    This routine opens a new connection to a Simple Peripheral Bus.

Arguments:

    Interface - Supplies a pointer to the interface instance, used to identify
        which specific bus is being opened.

    Configuration - Supplies a pointer to the configuration data that specifies
        bus specific configuration parameters.

    Handle - Supplies a pointer where a handle will be returned on success
        representing the connection to the device.

Return Value:

    Status code.

--*/

typedef
VOID
(*PSPB_CLOSE) (
    PSPB_INTERFACE Interface,
    SPB_HANDLE Handle
    );

/*++

Routine Description:

    This routine closes a previously opened to a Simple Peripheral Bus.

Arguments:

    Interface - Supplies a pointer to the interface instance, used to identify
        which specific bus is being operated on.

    Handle - Supplies the open handle to close.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PSPB_SET_CONFIGURATION) (
    SPB_HANDLE Handle,
    PRESOURCE_SPB_DATA Configuration
    );

/*++

Routine Description:

    This routine writes a new set of bus parameters to the bus.

Arguments:

    Handle - Supplies the open handle to change configuration of.

    Configuration - Supplies the new configuration to set.

Return Value:

    Status code.

--*/

typedef
VOID
(*PSPB_LOCK_BUS) (
    SPB_HANDLE Handle
    );

/*++

Routine Description:

    This routine locks the bus so that this handle may perform a sequence of
    accesses without being interrupted. The perform transfer function
    automatically locks the bus throughout the sequence of transfers, so this
    routine should not be necessary for normal operations.

Arguments:

    Handle - Supplies the open handle to the bus to lock.

Return Value:

    None.

--*/

typedef
VOID
(*PSPB_UNLOCK_BUS) (
    SPB_HANDLE Handle
    );

/*++

Routine Description:

    This routine unlocks a bus that was previously locked with the lock
    function. See the lock function description for why this routine should not
    be needed for normal operation.

Arguments:

    Handle - Supplies the open handle to the bus to unlock. The caller must
        have previously locked the bus.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PSPB_SUBMIT_TRANSFER_SET) (
    SPB_HANDLE Handle,
    PSPB_TRANSFER_SET TransferSet
    );

/*++

Routine Description:

    This routine submits a set of transfers to the bus for execution. This
    routine will ensure that other devices do not perform transfers while any
    transfer in this set is in progress. The submission is asynchronous, this
    routine will return immediately, and the callback function will be called
    when the transfer is complete.

Arguments:

    Handle - Supplies the open handle to the bus to unlock.

    TransferSet - Supplies a pointer to the transfer set to execute.

Return Value:

    Status code. This routine will return immediately, the transfer will not
    have been complete. The caller should utilize the callback function to get
    notified when a transfer has completed.

--*/

typedef
KSTATUS
(*PSPB_EXECUTE_TRANSFER_SET) (
    SPB_HANDLE Handle,
    PSPB_TRANSFER_SET TransferSet
    );

/*++

Routine Description:

    This routine submits a set of transfers to the bus for execution. This
    routine will ensure that other devices do not perform transfers while any
    transfer in this set is in progress. This routine is synchronous, it will
    not return until the transfer is complete.

Arguments:

    Handle - Supplies the open handle to the bus to unlock.

    TransferSet - Supplies a pointer to the transfer set to execute.

Return Value:

    Status code indicating completion status of the transfer. This routine will
    not return until the transfer is complete (or failed).

--*/

/*++

Structure Description:

    This structure defines the interface to a Simple Peripheral Bus device.
    Each handle given out by the open function of this interface is not thread
    safe.

Members:

    Context - Stores an opaque pointer to additinal data that the interface
        producer uses to identify this interface instance.

    Open - Stores a pointer to a function that opens a connection to a bus
        device.

    Close - Stores a pointer to a function that closes a previously opened
        connection to a bus device.

    SetConfiguration - Stores a pointer to a function used to set new bus
        parameters for the connection.

    LockBus - Stores a pointer to a function that locks the bus from all other
        users.

    UnlockBus - Stores a pointer to a function that unlocks the bus.

    SubmitTransferSet - Stores a pointer to a function used to submit a
        transfer on the bus asynchronously

    ExecuteTransferSet - Stores a pointer to a function used to submit a
        transfer on the bus synchronously.

--*/

struct _SPB_INTERFACE {
    PVOID Context;
    PSPB_OPEN Open;
    PSPB_CLOSE Close;
    PSPB_SET_CONFIGURATION SetConfiguration;
    PSPB_LOCK_BUS LockBus;
    PSPB_UNLOCK_BUS UnlockBus;
    PSPB_SUBMIT_TRANSFER_SET SubmitTransferSet;
    PSPB_EXECUTE_TRANSFER_SET ExecuteTransferSet;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
