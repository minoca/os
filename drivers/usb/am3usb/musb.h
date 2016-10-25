/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    musb.h

Abstract:

    This header contains definitions for the Mentor Graphics USB controller
    driver support.

Author:

    Evan Green 11-Sep-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "musbhw.h"
#include "cppi.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define MUSB_ALLOCATION_TAG 0x6273554D

//
// Define (software) USB transfer flags.
//

#define MUSB_TRANSFER_OUT 0x0001
#define MUSB_TRANSFER_SETUP 0x0002
#define MUSB_TRANSFER_STATUS 0x0004
#define MUSB_TRANSFER_DMA 0x0008

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MUSB_ENDPOINT_DIRECTION {
    MusbEndpointInvalid,
    MusbEndpointTx,
    MusbEndpointRx,
    MusbEndpointTxRx
} MUSB_ENDPOINT_DIRECTION, *PMUSB_ENDPOINT_DIRECTION;

/*++

Structure Description:

    This structure stores context for a MUSB software endpoint (not to be
    confused with the hardware endpoints which are more like channels in host
    mode).

Members:

    HardwareIndex - Stores the index of the allocated hardware endpoint, or
        -1 if the endpoint is currently not assigned.

    Device - Stores the device ID, which always starts out as zero.

    EndpointNumber - Stores the USB endpoint number for this endpoint.

    Type - Stores the value to plunk in the type register.

    Interval - Stores the value to put in the TX/RX interval register.

    HubAddress - Stores the hub address this device is connected to if it is
        a full or low-speed device.

    HubPort - Stores the port on the hub this device is connected to if it is
        a full or low-speed device.

    Control - Stores the value to put in the control register.

    MaxPayload - Stores the value to put in the max payload register.

    Direction - Stores the endpoint direction.

    InFlight - Stores the count of transfer sets currently in flight.

--*/

typedef struct _MUSB_SOFT_ENDPOINT {
    UCHAR HardwareIndex;
    UCHAR Device;
    UCHAR EndpointNumber;
    UCHAR Type;
    UCHAR Interval;
    UCHAR HubAddress;
    UCHAR HubPort;
    USHORT Control;
    USHORT MaxPayload;
    USB_TRANSFER_DIRECTION Direction;
    ULONG InFlight;
} MUSB_SOFT_ENDPOINT, *PMUSB_SOFT_ENDPOINT;

/*++

Structure Description:

    This structure stores context for a MUSB hardware endpoint.

Members:

    CurrentEndpoint - Stores a pointer to the soft endpoint this channel is
        currently configured to.

    TransferList - Stores the head of the list of transfers to execute on this
        endpoint.

    TxFifoSize - Stores the transmit FIFO size for this endpoint.

    RxFifoSize - Stores the receive FIFO size for this endpoint.

--*/

typedef struct _MUSB_HARD_ENDPOINT {
    PMUSB_SOFT_ENDPOINT CurrentEndpoint;
    LIST_ENTRY TransferList;
    USHORT TxFifoSize;
    USHORT RxFifoSize;
} MUSB_HARD_ENDPOINT, *PMUSB_HARD_ENDPOINT;

/*++

Structure Description:

    This structure stores controller information for a Mentor Graphics USB OTG
    host/device controller.

Members:

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    ControllerBase - Stores the virtual address of the hardware registers.

    Driver - Stores a pointer to the controller driver.

    PhysicalBase - Stores the physical address of the controller.

    Endpoints - Stores the array of hardware endpoint state.

    EndpointCount - Stores the number of endpoints present in this controller
        instance.

    CurrentIndex - Stores the current value programmed into the index register.

    NextEndpointAssignment - Stores the index to start the next search for an
        appropriate hardware endpoint for a software endpoint.

    UsbInterruptEnable - Stores the mask of enabled USB interrupts.

    Instance - Stores the instance number of this controller, which is passed
        in to the potentially common DMA controller.

    Lock - Stores the spin lock used to serialize access to the device.

    OldRunLevel - Stores the runlevel to return to when the lock is released.
        This value must be read before the lock is actually released, and set
        only after the lock is acquired.

    TxInterruptEnable - Stores the TX interrupt enable register value.

    RxInterruptEnable - Stores the RX interrupt enable register value.

    PendingUsbInterrupts - Stores the mask of USB interrupts pending.

    PendingEndpointInterrupts - Stores the mask of endpoint interrupts pending,
        with the RX interrupts in the upper 16 bits and the TX interrupts in
        the lower 16 bits.

    Connect - Stores a boolean indicating whether a device is currently
        connected.

    UsbCoreHandle - Stores the handle to the USB core representing this
        controller.

    CppiDma - Stores an optional pointer to the CPPI DMA controller to use for
        DMA.

--*/

typedef struct _MUSB_CONTROLLER {
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PDRIVER Driver;
    PHYSICAL_ADDRESS PhysicalBase;
    MUSB_HARD_ENDPOINT Endpoints[MUSB_MAX_ENDPOINTS];
    UCHAR EndpointCount;
    UCHAR CurrentIndex;
    UCHAR NextEndpointAssignment;
    UCHAR UsbInterruptEnable;
    UCHAR Instance;
    KSPIN_LOCK Lock;
    RUNLEVEL OldRunLevel;
    USHORT TxInterruptEnable;
    USHORT RxInterruptEnable;
    ULONG PendingUsbInterrupts;
    ULONG PendingEndpointInterrupts;
    BOOL Connected;
    HANDLE UsbCoreHandle;
    PCPPI_DMA_CONTROLLER CppiDma;
} MUSB_CONTROLLER, *PMUSB_CONTROLLER;

/*++

Structure Description:

    This structure defines the FIFO configuration for a hardware endpoint.

Members:

    Endpoint - Stores the hardware endpoint index.

    Direction - Stores the endpoint direction, of type MUSB_ENDPOINT_DIRECTION.

    MaxPacketSize - Stores the max packet size for the endpoint.

--*/

typedef struct _MUSB_FIFO_CONFIGURATION {
    UCHAR Endpoint;
    UCHAR Direction;
    USHORT MaxPacketSize;
} MUSB_FIFO_CONFIGURATION, *PMUSB_FIFO_CONFIGURATION;

/*++

Structure Description:

    This structure stores the context for an individual packet going out on the
    USB bus via MUSB.

Members:

    Size - Stores the size of the packet in bytes.

    Flags - Stores a bitfield of flags. See MUSB_TRANSFER_* definitions.

    BufferVirtual - Stores the virtual address of the buffer.

    BufferPhysical - Stores the physical address of the buffer.

    DmaData - Stores the DMA information for this transfer.

--*/

typedef struct _MUSB_TRANSFER {
    USHORT Size;
    USHORT Flags;
    PUCHAR BufferVirtual;
    ULONG BufferPhysical;
    CPPI_DESCRIPTOR_DATA DmaData;
} MUSB_TRANSFER, *PMUSB_TRANSFER;

/*++

Structure Description:

    This structure stores the context for a complete USB transfer in the MUSB
    controller.

Members:

    ListEntry - Stores pointers to the next and previous transfer sets in the
        queue for the hardware endpoint.

    Count - Stores the number of transfers configured in the set currently.

    MaxCount - Stores the maximum number of transfers that can be configured
        for this set.

    CurrentIndex - Stores the index of the transfer that is currently executing
        or should be executed next.

    SoftEndpoint - Stores a pointer to the endpoint this transfer is queued on.

    UsbTransfer - Stores a pointer to the USB transfer associated with this
        transfer set.

    Transfers - Stores a pointer to the array of transfers.

--*/

typedef struct _MUSB_TRANSFER_SET {
    LIST_ENTRY ListEntry;
    USHORT Count;
    USHORT MaxCount;
    USHORT CurrentIndex;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    PUSB_TRANSFER_INTERNAL UsbTransfer;
    PMUSB_TRANSFER Transfers;
} MUSB_TRANSFER_SET, *PMUSB_TRANSFER_SET;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
MusbInitializeControllerState (
    PMUSB_CONTROLLER Controller,
    PVOID RegisterBase,
    PDRIVER Driver,
    PHYSICAL_ADDRESS PhysicalBase,
    PCPPI_DMA_CONTROLLER DmaController,
    UCHAR Instance
    );

/*++

Routine Description:

    This routine initializes data structures for the Mentor USB controller.
    It's assumed the controller structure has already been properly zeroed.

Arguments:

    Controller - Supplies a pointer to the controller structure, which has
        already been zeroed.

    RegisterBase - Supplies the virtual address of the registers for the
        device.

    Driver - Supplies a pointer to the driver that owns this device.

    PhysicalBase - Supplies the physical address of the controller.

    DmaController - Supplies an optional pointer to the DMA controller to use.

    Instance - Supplies the instance ID to pass into the potentially shared
        DMA controller.

Return Value:

    Status code.

--*/

KSTATUS
MusbDestroyControllerState (
    PMUSB_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys the given Mentor USB controller structure, freeing
    all resources associated with the controller except the controller
    structure itself and the register base, which were passed in on initialize.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    Status code.

--*/

KSTATUS
MusbResetController (
    PMUSB_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine resets and reinitializes the given controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

KSTATUS
MusbRegisterController (
    PMUSB_CONTROLLER Controller,
    PDEVICE Device
    );

/*++

Routine Description:

    This routine registers the started Mentor USB controller with the core USB
    library.

Arguments:

    Controller - Supplies a pointer to the state of the controller to register.

    Device - Supplies a pointer to the device object.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
MusbInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the MUSB interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the controller.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
MusbInterruptServiceDpc (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine implements the MUSB dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the controller structure.

Return Value:

    None.

--*/

