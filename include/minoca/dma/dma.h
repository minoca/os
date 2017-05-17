/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dma.h

Abstract:

    This header contains definitions for interacting with generic Direct Memory
    Access controllers.

Author:

    Evan Green 1-Feb-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// UUID for interfacing with Direct Memory Access controllers.
//

#define UUID_DMA_INTERFACE \
    {{0x33D10646, 0x595A4840, 0x9D42E2EA, 0x5C13FBA8}}

//
// Define DMA transfer flags.
//

//
// Set this flag to advance the device address. If this flag is clear, the
// device address will not change throughout the course of the transfer
// (appropriate for writing to a register).
//

#define DMA_TRANSFER_ADVANCE_DEVICE 0x00000001

//
// Set this flag to initiate a continuous DMA transfer that will run until it
// is canceled, looping back to the beginning of the provided memory regions.
// The interrupt rate, if required, can be specified with a non-zero interrupt
// period in the DMA transfer.
//

#define DMA_TRANSFER_CONTINUOUS 0x00000002

//
// Define the current version of the DMA information table.
//

#define DMA_INFORMATION_VERSION 1
#define DMA_INFORMATION_MAX_VERSION 0x00001000

//
// Define the capabilities that can be advertised by a DMA controller.
//

#define DMA_CAPABILITY_CONTINUOUS_MODE 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DMA_TRANSFER_DIRECTION {
    DmaTransferDirectionInvalid,
    DmaTransferToDevice,
    DmaTransferFromDevice,
    DmaTransferMemoryToMemory
} DMA_TRANSFER_DIRECTION, *PDMA_TRANSFER_DIRECTION;

//
// Define the type of an open DMA connection.
//

typedef struct _DMA_INTERFACE DMA_INTERFACE, *PDMA_INTERFACE;
typedef struct _DMA_TRANSFER DMA_TRANSFER, *PDMA_TRANSFER;

typedef
VOID
(*PDMA_TRANSFER_COMPLETION_CALLBACK) (
    PDMA_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine is called when a transfer set has completed or errored out.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores information about a DMA controller.

Members:

    Version - Stores the version number of this table. This is set to
        DMA_INFORMATION_VERSION.

    ControllerUuid - Stores a universally unique identifier that identifies the
        manufacturer and model of the DMA controller. This specifies the format
        of the controller-specific configuration information.

    ControllerRevision - Stores the minor revision information for the DMA
        controller. Changes in these revisions are not significant enough to
        change the configuration and information structures.

    Capabilities - Stores a bitmask of DMA controller capabilities. See
        DMA_CAPABILITY_* for definnitions.

    ExtendedInfo - Stores a pointer to controller-specific extended information,
        the format of which depends on the UUID.

    ExtendedInfoSize - Stores the size of the extended information in bytes.

    ChannelCount - Stores the number of channels in the controller.

    MinAddress - Stores the lowest physical address (inclusive) that the DMA
        controller can access.

    MaxAddress - Stores the highest physical address (inclusive) that the DMA
        controller can access.

--*/

typedef struct _DMA_INFORMATION {
    ULONG Version;
    UUID ControllerUuid;
    ULONG ControllerRevision;
    ULONG Capabilities;
    PVOID ExtendedInfo;
    UINTN ExtendedInfoSize;
    ULONG ChannelCount;
    PHYSICAL_ADDRESS MinAddress;
    PHYSICAL_ADDRESS MaxAddress;
} DMA_INFORMATION, *PDMA_INFORMATION;

/*++

Structure Description:

    This structure stores information about a single DMA transfer request.

Members:

    ListEntry - Stores a list entry used internally by the DMA library. Users
        should ignore this member.

    Allocation - Stores a pointer to the resource allocation describing the
        channel, request line, and a few other standardized DMA configuration
        details.

    Configuration - Stores a pointer to the controller-specific DMA channel
        configuration for this transfer. This memory should remain valid for
        the duration of the transfer.

    ConfigurationSize - Stores the size of configuration data in bytes.

    Direction - Stores the transfer direction. For memory to memory transfers,
        the transfer always goes from the "memory" member to the
        "device memory" member.

    Memory - Stores a pointer to the memory side of the transfer. This is the
        non-device side. This must be a non-paged I/O buffer.

    Device - Stores a pointer to the device side of the transfer, or the
        destination for memory to memory transfers.

    CompletionCallback - Stores a pointer to the routine to call when the
        transfer is complete. This callback will occur at dispatch level.

    UserContext - Stores a pointer's worth of context information that is
        unused by the DMA library or host controller. The user can store
        context here.

    Size - Stores the size of the transfer in bytes. It is an error if this
        size does not translate evenly to bus sized transactions. This size may
        be truncated after submission if there weren't enough internal DMA
        descriptors to accommodate the full size.

    Width - Stores the width of the transfer, in bytes. Supply 0 to use the
        width from the resource allocation.

    Flags - Stores a bitfield of flags governing the transfer. See
        DMA_TRANSFER_* definitions.

    Completed - Stores the number of bytes successfully transferred.

    Status - Stores the final status code of the transfer, as returned by the
        DMA controller.

    InterruptPeriod - Stores the number of bytes after which a continuous DMA
        transfer will interrupt. If this is zero, the continuous transfer will
        interrupt after 'Size' bytes have been transferred.

--*/

struct _DMA_TRANSFER {
    LIST_ENTRY ListEntry;
    PRESOURCE_ALLOCATION Allocation;
    PVOID Configuration;
    UINTN ConfigurationSize;
    DMA_TRANSFER_DIRECTION Direction;
    PIO_BUFFER Memory;
    union {
        PHYSICAL_ADDRESS Address;
        PIO_BUFFER Memory;
    } Device;

    PDMA_TRANSFER_COMPLETION_CALLBACK CompletionCallback;
    PVOID UserContext;
    UINTN Size;
    ULONG Width;
    ULONG Flags;
    UINTN Completed;
    KSTATUS Status;
    ULONG InterruptPeriod;
};

typedef
KSTATUS
(*PDMA_GET_INFORMATION) (
    PDMA_INTERFACE Interface,
    PDMA_INFORMATION Information
    );

/*++

Routine Description:

    This routine returns information about a given DMA controller.

Arguments:

    Interface - Supplies a pointer to the interface instance, used to identify
        which specific controller is being queried.

    Information - Supplies a pointer where the DMA controller information is
        returned on success. The caller should initialize the version number of
        this structure.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PDMA_SUBMIT_TRANSFER) (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine submits a transfer to the DMA controller for execution. This
    routine will ensure that other devices do not perform transfers on the
    given channel while this transfer is in progress. The submission is
    asynchronous, this routine will return immediately, and the callback
    function will be called when the transfer is complete.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies a pointer to the transfer to execute.

Return Value:

    Status code. This routine will return immediately, the transfer will not
    have been complete. The caller should utilize the callback function to get
    notified when a transfer has completed.

--*/

typedef
KSTATUS
(*PDMA_CANCEL_TRANSFER) (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine attempts to cancel a transfer that is currently in flight.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies a pointer to the transfer to cancel.

Return Value:

    STATUS_SUCCESS if the transfer was successfully canceled.

    STATUS_TOO_LATE if the transfer is already complete.

    Other status codes on other failures.

--*/

typedef
KSTATUS
(*PDMA_CONTROL_REQUEST) (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer,
    PVOID Request,
    UINTN RequestSize
    );

/*++

Routine Description:

    This routine is called to perform a DMA controller-specific operation. It
    provides a direct link between DMA controllers and users, for controller-
    specific functionality.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies an optional pointer to the transfer involved.

    Request - Supplies a pointer to the request/response data.

    RequestSize - Supplies the size of the request in bytes.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PDMA_ALLOCATE_TRANSFER) (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER *Transfer
    );

/*++

Routine Description:

    This routine creates a new DMA transfer structure.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies a pointer where a pointer to the newly allocated
        transfer is returned on success.

Return Value:

    Status code.

--*/

typedef
VOID
(*PDMA_FREE_TRANSFER) (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine destroys a previously created DMA transfer. This transfer
    must not be actively submitted to any controller.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies a pointer to the transfer to destroy.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the interface to a Simple Peripheral Bus device.
    Each handle given out by the open function of this interface is not thread
    safe.

Members:

    Context - Stores an opaque pointer to additinal data that the interface
        producer uses to identify this interface instance.

    GetInformation - Stores a pointer to a function used to get information
        about the DMA controller.

    Submit - Stores a pointer to a function used to submit a new DMA transfer.

    Cancel - Stores a pointer to a function used to cancel a submitted but not
        yet complete DMA transfer.

    ControlRequest - Stores a pointer to a function used to implement
        controller-specific features.

    AllocateTransfer - Stores a pointer to a function used to allocate a DMA
        transfer.

    FreeTransfer - Stores a pointer to a function used to free a DMA transfer.

--*/

struct _DMA_INTERFACE {
    PVOID Context;
    PDMA_GET_INFORMATION GetInformation;
    PDMA_SUBMIT_TRANSFER Submit;
    PDMA_CANCEL_TRANSFER Cancel;
    PDMA_CONTROL_REQUEST ControlRequest;
    PDMA_ALLOCATE_TRANSFER AllocateTransfer;
    PDMA_FREE_TRANSFER FreeTransfer;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
