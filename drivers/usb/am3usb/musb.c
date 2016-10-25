/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    musb.c

Abstract:

    This module implements support for the Mentor Graphics USB 2.0 OTG
    controller.

Author:

    Evan Green 11-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usb/usbhost.h>
#include "musb.h"

//
// --------------------------------------------------------------------- Macros
//

#define MUSB_READ8(_Controller, _Register) \
    HlReadRegister8((_Controller)->ControllerBase + (_Register))

#define MUSB_WRITE8(_Controller, _Register, _Value) \
    HlWriteRegister8((_Controller)->ControllerBase + (_Register), (_Value))

#define MUSB_READ16(_Controller, _Register) \
    HlReadRegister16((_Controller)->ControllerBase + (_Register))

#define MUSB_WRITE16(_Controller, _Register, _Value) \
    HlWriteRegister16((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
MusbpCppiDmaCompletionCallback (
    PVOID Context,
    ULONG DmaEndpoint,
    BOOL Transmit
    );

KSTATUS
MusbpCreateEndpoint (
    PVOID HostControllerContext,
    PUSB_HOST_ENDPOINT_CREATION_REQUEST Endpoint,
    PVOID *EndpointContext
    );

VOID
MusbpResetEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxPacketSize
    );

KSTATUS
MusbpFlushEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PULONG TransferCount
    );

VOID
MusbpDestroyEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext
    );

KSTATUS
MusbpCreateTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG Flags,
    ULONG MaxBufferSize,
    PVOID *TransferContext
    );

VOID
MusbpDestroyTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PVOID TransferContext
    );

KSTATUS
MusbpSubmitTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
MusbpSubmitPolledTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
MusbpCancelTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
MusbpGetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    );

KSTATUS
MusbpSetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    );

VOID
MusbpProcessUsbInterrupts (
    PMUSB_CONTROLLER Controller,
    UCHAR UsbInterrupts
    );

VOID
MusbpProcessEndpointInterrupts (
    PMUSB_CONTROLLER Controller,
    ULONG EndpointInterrupts
    );

KSTATUS
MusbpInitializeTransfer (
    PMUSB_CONTROLLER Controller,
    PMUSB_SOFT_ENDPOINT SoftEndpoint,
    PUSB_TRANSFER_INTERNAL Transfer,
    PMUSB_TRANSFER_SET TransferSet
    );

VOID
MusbpExecuteNextTransfer (
    PMUSB_CONTROLLER Controller,
    PMUSB_HARD_ENDPOINT HardEndpoint
    );

PMUSB_TRANSFER_SET
MusbpProcessCompletedTransfer (
    PMUSB_CONTROLLER Controller,
    UCHAR HardwareIndex,
    PBOOL TransferCompleted
    );

VOID
MusbpFailAllTransfers (
    PMUSB_CONTROLLER Controller
    );

VOID
MusbpConfigureFifo (
    PMUSB_CONTROLLER Controller,
    PMUSB_FIFO_CONFIGURATION Configuration,
    PULONG Offset
    );

VOID
MusbpAbortTransfer (
    PMUSB_CONTROLLER Controller,
    UCHAR HardwareIndex,
    PMUSB_TRANSFER Transfer
    );

VOID
MusbpConfigureHardwareEndpoint (
    PMUSB_CONTROLLER Controller,
    PMUSB_SOFT_ENDPOINT SoftEndpoint
    );

VOID
MusbpUpdateDataToggle (
    PMUSB_CONTROLLER Controller,
    PMUSB_TRANSFER_SET TransferSet
    );

VOID
MusbpWriteFifo (
    PMUSB_CONTROLLER Controller,
    UCHAR EndpointIndex,
    PVOID Buffer,
    ULONG Size
    );

VOID
MusbpReadFifo (
    PMUSB_CONTROLLER Controller,
    UCHAR EndpointIndex,
    PVOID Buffer,
    ULONG BufferSize
    );

VOID
MusbpFlushFifo (
    PMUSB_CONTROLLER Controller,
    UCHAR HardwareIndex,
    BOOL HostOut
    );

VOID
MusbpAssignEndpoint (
    PMUSB_CONTROLLER Controller,
    PMUSB_SOFT_ENDPOINT SoftEndpoint
    );

UCHAR
MusbpReadIndexed8 (
    PMUSB_CONTROLLER Controller,
    UCHAR Index,
    MUSB_INDEXED_REGISTER Register
    );

UCHAR
MusbpReadIndexed16 (
    PMUSB_CONTROLLER Controller,
    UCHAR Index,
    MUSB_INDEXED_REGISTER Register
    );

VOID
MusbpWriteIndexed8 (
    PMUSB_CONTROLLER Controller,
    UCHAR Index,
    MUSB_INDEXED_REGISTER Register,
    UCHAR Value
    );

VOID
MusbpWriteIndexed16 (
    PMUSB_CONTROLLER Controller,
    UCHAR Index,
    MUSB_INDEXED_REGISTER Register,
    USHORT Value
    );

VOID
MusbpAcquireLock (
    PMUSB_CONTROLLER Controller
    );

VOID
MusbpReleaseLock (
    PMUSB_CONTROLLER Controller
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this boolean to disable DMA. This must be set before endpoint creation.
//

BOOL MusbDisableDma = FALSE;

USB_HOST_CONTROLLER_INTERFACE MusbUsbHostInterfaceTemplate = {
    USB_HOST_CONTROLLER_INTERFACE_VERSION,
    NULL,
    NULL,
    NULL,
    0,
    -1,
    UsbDeviceSpeedHigh,
    1,
    MusbpCreateEndpoint,
    MusbpResetEndpoint,
    MusbpFlushEndpoint,
    MusbpDestroyEndpoint,
    MusbpCreateTransfer,
    MusbpDestroyTransfer,
    MusbpSubmitTransfer,
    MusbpSubmitPolledTransfer,
    MusbpCancelTransfer,
    MusbpGetRootHubStatus,
    MusbpSetRootHubStatus
};

MUSB_FIFO_CONFIGURATION MusbFifoConfiguration[] = {
    {1, MusbEndpointTx, 512},
    {1, MusbEndpointRx, 512},
    {2, MusbEndpointTx, 512},
    {2, MusbEndpointRx, 512},
    {3, MusbEndpointTx, 512},
    {3, MusbEndpointRx, 512},
    {4, MusbEndpointTx, 512},
    {4, MusbEndpointRx, 512},
    {5, MusbEndpointTx, 512},
    {5, MusbEndpointRx, 512},
    {6, MusbEndpointTx, 512},
    {6, MusbEndpointRx, 512},
    {7, MusbEndpointTx, 512},
    {7, MusbEndpointRx, 512},
    {8, MusbEndpointTx, 512},
    {8, MusbEndpointRx, 512},
    {9, MusbEndpointTx, 512},
    {9, MusbEndpointRx, 512},
    {10, MusbEndpointTx, 256},
    {10, MusbEndpointRx, 64},
    {11, MusbEndpointTx, 256},
    {11, MusbEndpointRx, 64},
    {12, MusbEndpointTx, 256},
    {12, MusbEndpointRx, 64},
    {13, MusbEndpointTx, 4096},
    {14, MusbEndpointRx, 1024},
    {15, MusbEndpointTx, 1024},
    {0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
MusbInitializeControllerState (
    PMUSB_CONTROLLER Controller,
    PVOID RegisterBase,
    PDRIVER Driver,
    PHYSICAL_ADDRESS PhysicalBase,
    PCPPI_DMA_CONTROLLER DmaController,
    UCHAR Instance
    )

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

{

    PMUSB_HARD_ENDPOINT Endpoint;
    ULONG Index;
    KSTATUS Status;

    Controller->ControllerBase = RegisterBase;
    Controller->Driver = Driver;
    Controller->PhysicalBase = PhysicalBase;
    Controller->NextEndpointAssignment = 1;
    Controller->CppiDma = DmaController;
    Controller->Instance = Instance;
    KeInitializeSpinLock(&(Controller->Lock));
    for (Index = 0; Index < MUSB_MAX_ENDPOINTS; Index += 1) {
        Endpoint = &(Controller->Endpoints[Index]);
        INITIALIZE_LIST_HEAD(&(Endpoint->TransferList));
        Endpoint->CurrentEndpoint = NULL;
    }

    if (DmaController != NULL) {
        CppiRegisterCompletionCallback(DmaController,
                                       Instance,
                                       MusbpCppiDmaCompletionCallback,
                                       Controller);
    }

    Status = STATUS_SUCCESS;
    if (!KSUCCESS(Status)) {
        MusbDestroyControllerState(Controller);
    }

    return STATUS_SUCCESS;
}

KSTATUS
MusbDestroyControllerState (
    PMUSB_CONTROLLER Controller
    )

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

{

    Controller->ControllerBase = NULL;
    return STATUS_SUCCESS;
}

KSTATUS
MusbResetController (
    PMUSB_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine resets and reinitializes the given controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    PMUSB_FIFO_CONFIGURATION Configuration;
    UCHAR DeviceControl;
    UCHAR EndpointCount;
    ULONG Offset;
    UCHAR UsbInterrupts;

    MUSB_WRITE8(Controller, MusbSoftReset, MUSB_SOFT_RESET_SOFT_RESET);
    EndpointCount = MUSB_READ8(Controller, MusbEndpointInfo);

    //
    // Determine the number of hardware endpoints.
    //

    Controller->EndpointCount =
                              EndpointCount & MUSB_ENDPOINT_INFO_TX_COUNT_MASK;

    ASSERT(((EndpointCount &
             MUSB_ENDPOINT_INFO_RX_COUNT_MASK) >>
            MUSB_ENDPOINT_INFO_RX_COUNT_SHIFT) == Controller->EndpointCount);

    //
    // Program the FIFO configuration for the endpoints. Endpoint 0 always gets
    // the first 64 bytes.
    //

    Offset = 64;
    Configuration = MusbFifoConfiguration;
    while (Configuration->MaxPacketSize != 0) {
        if (Configuration->Endpoint < Controller->EndpointCount) {
            MusbpConfigureFifo(Controller, Configuration, &Offset);
        }

        Configuration += 1;
    }

    //
    // Enable all endpoint interrupts, and all USB interrupts except start of
    // frame.
    //

    MUSB_WRITE16(Controller, MusbInterruptEnableTx, 0xFFFF);
    MUSB_WRITE16(Controller, MusbInterruptEnableRx, 0xFFFF);
    UsbInterrupts = MUSB_USB_INTERRUPT_SUSPEND |
                    MUSB_USB_INTERRUPT_RESUME |
                    MUSB_USB_INTERRUPT_RESET_BABBLE |
                    MUSB_USB_INTERRUPT_CONNECT |
                    MUSB_USB_INTERRUPT_DISCONNECT |
                    MUSB_USB_INTERRUPT_SESSION |
                    MUSB_USB_INTERRUPT_VBUS_ERROR;

    Controller->UsbInterruptEnable = UsbInterrupts;
    MUSB_WRITE8(Controller, MusbInterruptEnableUsb, UsbInterrupts);

    //
    // Enable a session.
    //

    DeviceControl = MUSB_READ8(Controller, MusbDeviceControl);
    DeviceControl |= MUSB_DEVICE_CONTROL_SESSION;
    MUSB_WRITE8(Controller, MusbDeviceControl, DeviceControl);
    return STATUS_SUCCESS;
}

KSTATUS
MusbRegisterController (
    PMUSB_CONTROLLER Controller,
    PDEVICE Device
    )

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

{

    USB_HOST_CONTROLLER_INTERFACE Interface;
    KSTATUS Status;

    //
    // Fill out the functions that the USB core library will use to control
    // the host controller.
    //

    RtlCopyMemory(&Interface,
                  &MusbUsbHostInterfaceTemplate,
                  sizeof(USB_HOST_CONTROLLER_INTERFACE));

    Interface.DriverObject = Controller->Driver;
    Interface.DeviceObject = Device;
    Interface.HostControllerContext = Controller;
    Interface.Identifier = Controller->PhysicalBase;
    Status = UsbHostRegisterController(&Interface,
                                       &(Controller->UsbCoreHandle));

    if (!KSUCCESS(Status)) {
        goto RegisterControllerEnd;
    }

RegisterControllerEnd:
    return Status;
}

INTERRUPT_STATUS
MusbInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the MUSB interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the controller.

Return Value:

    Interrupt status.

--*/

{

    PMUSB_CONTROLLER Controller;
    ULONG EndpointStatus;
    INTERRUPT_STATUS InterruptStatus;
    USHORT RxStatus;
    USHORT TxStatus;
    USHORT UsbStatus;

    Controller = (PMUSB_CONTROLLER)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the status register. If it's non-zero, this is USB's interrupt.
    //

    UsbStatus = MUSB_READ8(Controller, MusbInterruptUsb) &
                Controller->UsbInterruptEnable;

    if (UsbStatus != 0) {
        InterruptStatus = InterruptStatusClaimed;
        RtlAtomicOr32(&(Controller->PendingUsbInterrupts), UsbStatus);

        //
        // Clear the bits in the status register to acknowledge the interrupt.
        //

        MUSB_WRITE8(Controller, MusbInterruptUsb, UsbStatus);
    }

    RxStatus = MUSB_READ16(Controller, MusbInterruptRx);
    TxStatus = MUSB_READ16(Controller, MusbInterruptTx);
    EndpointStatus = (RxStatus << 16) | TxStatus;
    if (EndpointStatus != 0) {
        InterruptStatus = InterruptStatusClaimed;
        RtlAtomicOr32(&(Controller->PendingEndpointInterrupts), EndpointStatus);
        if (RxStatus != 0) {
            MUSB_WRITE16(Controller, MusbInterruptRx, RxStatus);
        }

        if (TxStatus != 0) {
            MUSB_WRITE16(Controller, MusbInterruptTx, TxStatus);
        }
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
MusbInterruptServiceDpc (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the MUSB dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the controller structure.

Return Value:

    None.

--*/

{

    PMUSB_CONTROLLER Controller;
    ULONG EndpointInterrupts;
    ULONG UsbInterrupts;

    Controller = Parameter;
    UsbInterrupts = RtlAtomicExchange32(&(Controller->PendingUsbInterrupts), 0);
    EndpointInterrupts =
              RtlAtomicExchange32(&(Controller->PendingEndpointInterrupts), 0);

    if ((UsbInterrupts == 0) && (EndpointInterrupts == 0)) {
        return InterruptStatusNotClaimed;
    }

    if (UsbInterrupts != 0) {
        MusbpProcessUsbInterrupts(Controller, UsbInterrupts);
    }

    if (EndpointInterrupts != 0) {
        MusbpProcessEndpointInterrupts(Controller, EndpointInterrupts);
    }

    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
MusbpCppiDmaCompletionCallback (
    PVOID Context,
    ULONG DmaEndpoint,
    BOOL Transmit
    )

/*++

Routine Description:

    This routine is called when CPPI receives an interrupt telling it that a
    queue completion occurred.

Arguments:

    Context - Supplies an opaque pointer's worth of context for the callback
        routine.

    DmaEndpoint - Supplies the zero-based DMA endpoint number. Add 1 to get
        to a USB endpoint number.

    Transmit - Supplies a boolean indicating if this is a transmit completion
        (TRUE) or a receive completion (FALSE).

Return Value:

    None.

--*/

{

    PMUSB_CONTROLLER Controller;
    ULONG Endpoint;
    ULONG Mask;

    Controller = Context;
    Endpoint = CPPI_DMA_ENDPOINT_TO_USB(DmaEndpoint);
    Mask = 1 << Endpoint;
    if (Transmit == FALSE) {
        Mask <<= 16;
    }

    MusbpProcessEndpointInterrupts(Controller, Mask);
    return;
}

KSTATUS
MusbpCreateEndpoint (
    PVOID HostControllerContext,
    PUSB_HOST_ENDPOINT_CREATION_REQUEST Endpoint,
    PVOID *EndpointContext
    )

/*++

Routine Description:

    This routine is called by the USB core when a new endpoint is being opened.
    It allows the host controller to create and store any context needed to
    support a new endpoint (such as a queue head).

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    Endpoint - Supplies a pointer containing information about the endpoint
        being created. The host controller cannot count on this buffer sticking
        around after the function returns. If it needs this information it
        should make a copy of it.

    EndpointContext - Supplies a pointer where the host controller can store a
        context pointer identifying the endpoint created.

Return Value:

    STATUS_SUCCESS if the endpoint can be successfully accommodated.

    Failing status code if the endpoint cannot be opened.

--*/

{

    USHORT Control;
    PMUSB_CONTROLLER Controller;
    USHORT PollRate;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    KSTATUS Status;
    UCHAR Type;

    Controller = HostControllerContext;
    PollRate = Endpoint->PollRate;

    //
    // For high speed endpoints, the interval is 2^(Interval - 1). This is
    // also true for full speed isochronous and full speed bulk (NAK count).
    // For other full/low speed endpoints, it's just a frame count.
    //

    if ((Endpoint->Speed == UsbDeviceSpeedHigh) ||
        ((Endpoint->Speed == UsbDeviceSpeedFull) &&
         ((Endpoint->Type == UsbTransferTypeIsochronous) ||
          (Endpoint->Type == UsbTransferTypeBulk)))) {

        if (PollRate != 0) {
            PollRate = RtlCountTrailingZeros32(PollRate) + 1;
            if (PollRate > 16) {
                PollRate = 16;
            }
        }
    }

    SoftEndpoint = MmAllocateNonPagedPool(sizeof(MUSB_SOFT_ENDPOINT),
                                          MUSB_ALLOCATION_TAG);

    if (SoftEndpoint == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEndpointEnd;
    }

    RtlZeroMemory(SoftEndpoint, sizeof(MUSB_SOFT_ENDPOINT));
    SoftEndpoint->MaxPayload = Endpoint->MaxPacketSize;
    SoftEndpoint->HubAddress = Endpoint->HubAddress;
    SoftEndpoint->HubPort = Endpoint->HubPortNumber;
    SoftEndpoint->EndpointNumber = Endpoint->EndpointNumber;
    Type = Endpoint->EndpointNumber & MUSB_TXTYPE_TARGET_ENDPOINT_MASK;
    switch (Endpoint->Speed) {
    case UsbDeviceSpeedLow:
        Type |= MUSB_TXTYPE_SPEED_LOW;
        break;

    case UsbDeviceSpeedFull:
        Type |= MUSB_TXTYPE_SPEED_FULL;
        break;

    case UsbDeviceSpeedHigh:
        Type |= MUSB_TXTYPE_SPEED_HIGH;
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto CreateEndpointEnd;
    }

    SoftEndpoint->Interval = PollRate;
    switch (Endpoint->Type) {
    case UsbTransferTypeControl:
        Type |= MUSB_TXTYPE_PROTOCOL_CONTROL;
        SoftEndpoint->HardwareIndex = 0;
        SoftEndpoint->Interval = 0;
        break;

    case UsbTransferTypeInterrupt:
        Type |= MUSB_TXTYPE_PROTOCOL_INTERRUPT;
        break;

    case UsbTransferTypeBulk:
        Type |= MUSB_TXTYPE_PROTOCOL_BULK;
        break;

    case UsbTransferTypeIsochronous:
        Type |= MUSB_TXTYPE_PROTOCOL_ISOCHRONOUS;
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto CreateEndpointEnd;
    }

    SoftEndpoint->Type = Type;
    SoftEndpoint->Direction = Endpoint->Direction;

    //
    // All control endpoints use hardware endpoint 0, and cannot use DMA. For
    // any other type, assign it a hard endpoint/channel.
    //

    if (Endpoint->Type == UsbTransferTypeControl) {

        //
        // Set the control endpoint direction to "out" so that the TX
        // control/status register is always used, which is required for
        // hardware endpoint 0.
        //

        SoftEndpoint->Direction = UsbTransferDirectionOut;

    } else {
        if ((MusbDisableDma == FALSE) && (Controller->CppiDma != NULL)) {
            Control = 0;
            if (Endpoint->Direction == UsbTransferDirectionOut) {
                Control |= MUSB_TX_CONTROL_DMA_ENABLE |
                           MUSB_TX_CONTROL_DMA_MODE;

            } else {

                ASSERT(Endpoint->Direction == UsbTransferDirectionIn);

                Control |= MUSB_RX_CONTROL_DMA_ENABLE;
            }

            SoftEndpoint->Control = Control;
        }

        //
        // Find an initial hardware endpoint for this software endpoint.
        //

        MusbpAcquireLock(Controller);
        MusbpAssignEndpoint(Controller, SoftEndpoint);
        MusbpReleaseLock(Controller);
    }

    *EndpointContext = SoftEndpoint;
    Status = STATUS_SUCCESS;

CreateEndpointEnd:
    if (!KSUCCESS(Status)) {
        if (SoftEndpoint != NULL) {
            MmFreePagedPool(SoftEndpoint);
        }
    }

    return Status;
}

VOID
MusbpResetEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxPacketSize
    )

/*++

Routine Description:

    This routine is called by the USB core when an endpoint needs to be reset.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the context returned by the host
        controller when the endpoint was created.

    MaxPacketSize - Supplies the maximum transfer size of the endpoint.

Return Value:

    None.

--*/

{

    PMUSB_CONTROLLER Controller;
    PMUSB_HARD_ENDPOINT HardEndpoint;
    ULONG Register;
    RUNLEVEL RunLevel;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    USHORT Value;

    Controller = HostControllerContext;
    SoftEndpoint = EndpointContext;

    //
    // Only control endpoints are expected to change max packet sizes.
    //

    ASSERT((SoftEndpoint->HardwareIndex == 0) ||
           (MaxPacketSize == SoftEndpoint->MaxPayload));

    SoftEndpoint->MaxPayload = MaxPacketSize;

    //
    // This needs to acquire the lock in normal mode. In crash dump mode, skip
    // that.
    //

    RunLevel = KeGetRunLevel();
    if (RunLevel != RunLevelHigh) {
        MusbpAcquireLock(Controller);
    }

    //
    // Clear the data toggle bit.
    //

    if (SoftEndpoint->HardwareIndex == 0) {
        SoftEndpoint->Control &= ~MUSB_EP0_CONTROL_DATA_TOGGLE;

    } else if (SoftEndpoint->Direction == UsbTransferDirectionIn) {
        SoftEndpoint->Control &= ~MUSB_RX_CONTROL_DATA_TOGGLE;

    } else {

        ASSERT(SoftEndpoint->Direction == UsbTransferDirectionOut);

        SoftEndpoint->Control &= ~MUSB_TX_CONTROL_DATA_TOGGLE;
    }

    //
    // If this software endpoint is currently programmed in the hardware
    // channel, clear the data toggle in the hardware too.
    //

    HardEndpoint = &(Controller->Endpoints[SoftEndpoint->HardwareIndex]);
    if (HardEndpoint->CurrentEndpoint == SoftEndpoint) {
        if (SoftEndpoint->HardwareIndex == 0) {

            ASSERT(MaxPacketSize <= 64);

            Register = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus, 0);
            Value = SoftEndpoint->Control | MUSB_EP0_CONTROL_DATA_TOGGLE_WRITE;
            MUSB_WRITE16(Controller, Register, Value);
            Value = SoftEndpoint->MaxPayload;
            Register = MUSB_ENDPOINT_CONTROL(MusbTxMaxPacketSize, 0);
            MUSB_WRITE16(Controller, Register, Value);

        } else if (SoftEndpoint->Direction == UsbTransferDirectionIn) {
            Register = MUSB_ENDPOINT_CONTROL(MusbRxControlStatus,
                                             SoftEndpoint->HardwareIndex);

            Value = SoftEndpoint->Control | MUSB_RX_CONTROL_CLEAR_TOGGLE;
            MUSB_WRITE16(Controller, Register, Value);

        } else {
            Register = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus,
                                             SoftEndpoint->HardwareIndex);

            Value = SoftEndpoint->Control | MUSB_TX_CONTROL_CLEAR_TOGGLE;
            MUSB_WRITE16(Controller, Register, Value);
        }
    }

    if (RunLevel != RunLevelHigh) {
        MusbpReleaseLock(Controller);
    }

    return;
}

KSTATUS
MusbpFlushEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PULONG TransferCount
    )

/*++

Routine Description:

    This routine flushes all the active transfers from an endpoint. It does so
    by polling for completion status and does not return until all transfers
    are completed. This must be called at high run level.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the context returned by the host
        controller when the endpoint was created.

    TransferCount - Supplies a pointer to a boolean that receives the number
        of transfers that were flushed.

Return Value:

    Status code.

--*/

{

    PMUSB_CONTROLLER Controller;
    ULONG Count;
    USHORT EndpointInterrupts;
    PMUSB_HARD_ENDPOINT HardEndpoint;
    UCHAR HardwareIndex;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    BOOL StartNextTransfer;
    KSTATUS Status;
    ULONGLONG Timeout;
    PMUSB_TRANSFER_SET TransferSet;

    Controller = HostControllerContext;
    SoftEndpoint = EndpointContext;
    Count = 0;
    HardwareIndex = SoftEndpoint->HardwareIndex;
    HardEndpoint = &(Controller->Endpoints[HardwareIndex]);

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Timeout = HlQueryTimeCounter() + (10 * HlQueryTimeCounterFrequency());
    while (!LIST_EMPTY(&(HardEndpoint->TransferList))) {

        //
        // Read the endpoint interrupt status, and wait for this endpoint to
        // arrive.
        //

        EndpointInterrupts = MUSB_READ16(Controller, MusbInterruptRx) |
                             MUSB_READ16(Controller, MusbInterruptTx);

        if ((EndpointInterrupts & (1 << HardwareIndex)) == 0) {
            if (HlQueryTimeCounter() >= Timeout) {
                Status = STATUS_TIMEOUT;
                goto FlushEndpointEnd;
            }
        }

        //
        // Clear the endpoint interrupt.
        //

        MUSB_WRITE8(Controller, MusbInterruptRx, 1 << HardwareIndex);
        MUSB_WRITE8(Controller, MusbInterruptTx, 1 << HardwareIndex);

        //
        // Process a completed transfer.
        //

        TransferSet = MusbpProcessCompletedTransfer(Controller,
                                                    HardwareIndex,
                                                    &StartNextTransfer);

        if (TransferSet != NULL) {
            Count += 1;
        }

        //
        // Pump the next transfer through.
        //

        if (StartNextTransfer != FALSE) {
            MusbpExecuteNextTransfer(Controller, HardEndpoint);
        }
    }

    Status = STATUS_SUCCESS;

FlushEndpointEnd:
    *TransferCount = Count;
    return Status;
}

VOID
MusbpDestroyEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext
    )

/*++

Routine Description:

    This routine tears down and destroys an endpoint created with the endpoint
    creation routine.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the context returned by the host
        controller when the endpoint was created.

Return Value:

    None.

--*/

{

    PMUSB_CONTROLLER Controller;
    PMUSB_HARD_ENDPOINT HardEndpoint;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;

    SoftEndpoint = EndpointContext;
    Controller = HostControllerContext;
    HardEndpoint = &(Controller->Endpoints[SoftEndpoint->HardwareIndex]);
    if (HardEndpoint->CurrentEndpoint == SoftEndpoint) {
        MusbpAcquireLock(Controller);
        if (HardEndpoint->CurrentEndpoint == SoftEndpoint) {
            HardEndpoint->CurrentEndpoint = NULL;
        }

        MusbpReleaseLock(Controller);
    }

    MmFreeNonPagedPool(SoftEndpoint);
    return;
}

KSTATUS
MusbpCreateTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxBufferSize,
    ULONG Flags,
    PVOID *TransferContext
    )

/*++

Routine Description:

    This routine allocates structures needed for the USB host controller to
    support a transfer.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the host controller's context of
        the endpoint that this transfer will eventually be submitted to.

    MaxBufferSize - Supplies the maximum buffer length, in bytes, of the
        transfer when it is submitted. It is assumed that the host controller
        will set up as many transfer descriptors as are needed to support a
        transfer of this size.

    Flags - Supplies a bitfield of flags regarding the transaction. See
        USB_TRANSFER_FLAG_* definitions.

    TransferContext - Supplies a pointer where the host controller can store a
        context pointer containing any needed structures for the transfer.

Return Value:

    None.

--*/

{

    UINTN AllocationSize;
    PMUSB_CONTROLLER Controller;
    BOOL ForceShortTransfer;
    ULONG Index;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    KSTATUS Status;
    PMUSB_TRANSFER Transfer;
    ULONG TransferCount;
    PMUSB_TRANSFER_SET TransferSet;

    Controller = HostControllerContext;
    SoftEndpoint = EndpointContext;
    TransferCount = 0;
    ForceShortTransfer = FALSE;
    if ((Flags & USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER) != 0) {
        ForceShortTransfer = TRUE;
    }

    //
    // Control transfers need at least 2 transfers: the setup packet (which
    // burns the first 8 bytes), zero or more data transfers, and a status
    // transfer.
    //

    if (SoftEndpoint->HardwareIndex == 0) {

        ASSERT(MaxBufferSize >= sizeof(USB_SETUP_PACKET));

        MaxBufferSize -= sizeof(USB_SETUP_PACKET);
        TransferCount += 2;
    }

    if (MaxBufferSize != 0) {
        TransferCount += (MaxBufferSize + (SoftEndpoint->MaxPayload - 1)) /
                         SoftEndpoint->MaxPayload;

        //
        // If it's possible for the transfer to send a multiple of the max
        // payload size and a short transfer needs to be forced, add an another
        // transfer.
        //

        if ((ForceShortTransfer != FALSE) &&
            (MaxBufferSize >= SoftEndpoint->MaxPayload)) {

            TransferCount += 1;
        }

    //
    // Account for a USB transfer that will only send zero length packets and
    // for control transfers that need to force a zero length packet in the
    // data phase.
    //

    } else if ((ForceShortTransfer != FALSE) ||
               (SoftEndpoint->HardwareIndex != 0)) {

        TransferCount += 1;
    }

    AllocationSize = sizeof(MUSB_TRANSFER_SET) +
                     (TransferCount * sizeof(MUSB_TRANSFER));

    TransferSet = MmAllocateNonPagedPool(AllocationSize, MUSB_ALLOCATION_TAG);
    if (TransferSet == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTransferEnd;
    }

    RtlZeroMemory(TransferSet, AllocationSize);
    TransferSet->MaxCount = TransferCount;
    TransferSet->Transfers = (PMUSB_TRANSFER)(TransferSet + 1);
    if ((SoftEndpoint->HardwareIndex != 0) &&
        (MusbDisableDma == FALSE) &&
        (Controller->CppiDma != NULL)) {

        Transfer = TransferSet->Transfers;
        for (Index = 0; Index < TransferCount; Index += 1) {
            Status = CppiCreateDescriptor(Controller->CppiDma,
                                          Controller->Instance,
                                          &(Transfer->DmaData));

            if (!KSUCCESS(Status)) {
                goto CreateTransferEnd;
            }

            Transfer += 1;
        }
    }

    Status = STATUS_SUCCESS;

CreateTransferEnd:
    if (!KSUCCESS(Status)) {
        if (TransferSet != NULL) {
            Transfer = TransferSet->Transfers;
            for (Index = 0; Index < TransferCount; Index += 1) {
                if (Transfer->DmaData.Descriptor != NULL) {
                    CppiDestroyDescriptor(Controller->CppiDma,
                                          &(Transfer->DmaData));
                }

                Transfer += 1;
            }

            MmFreeNonPagedPool(TransferSet);
            TransferSet = NULL;
        }
    }

    *TransferContext = TransferSet;
    return Status;
}

VOID
MusbpDestroyTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PVOID TransferContext
    )

/*++

Routine Description:

    This routine destroys host controller structures associated with a USB
    transfer.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the host controller context for the
        endpoint this transfer belonged to.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    None.

--*/

{

    PMUSB_CONTROLLER Controller;
    ULONG Index;
    PMUSB_TRANSFER Transfer;
    ULONG TransferCount;
    PMUSB_TRANSFER_SET TransferSet;

    Controller = HostControllerContext;
    TransferSet = TransferContext;
    TransferCount = TransferSet->MaxCount;
    Transfer = TransferSet->Transfers;
    for (Index = 0; Index < TransferCount; Index += 1) {
        if (Transfer->DmaData.Descriptor != NULL) {
            CppiDestroyDescriptor(Controller->CppiDma, &(Transfer->DmaData));
        }

        Transfer += 1;
    }

    MmFreeNonPagedPool(TransferSet);
    return;
}

KSTATUS
MusbpSubmitTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    )

/*++

Routine Description:

    This routine submits a transfer to the USB host controller for execution.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies the context pointer provided to the USB core by
        the host controller when the endpoint was created.

    Transfer - Supplies a pointer to the USB transfer to execute.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    STATUS_SUCCESS if the transfer was successfully added to the hardware queue.

    Failure codes if the transfer could not be added.

--*/

{

    PMUSB_CONTROLLER Controller;
    PMUSB_HARD_ENDPOINT HardEndpoint;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    KSTATUS Status;
    PMUSB_TRANSFER_SET TransferSet;

    Controller = HostControllerContext;
    SoftEndpoint = EndpointContext;
    TransferSet = TransferContext;
    MusbpAcquireLock(Controller);
    if (Controller->Connected == FALSE) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto SubmitTransferEnd;
    }

    //
    // Assign a hardware endpoint and fill out all the descriptors.
    //

    MusbpAssignEndpoint(Controller, SoftEndpoint);
    Status = MusbpInitializeTransfer(Controller,
                                     SoftEndpoint,
                                     Transfer,
                                     TransferSet);

    if (!KSUCCESS(Status)) {
        goto SubmitTransferEnd;
    }

    HardEndpoint = &(Controller->Endpoints[SoftEndpoint->HardwareIndex]);
    if (Transfer->DeviceAddress != SoftEndpoint->Device) {

        ASSERT((SoftEndpoint->Device == 0) && (Transfer->DeviceAddress != 0));

        SoftEndpoint->Device = Transfer->DeviceAddress;

        //
        // The device ID changed so the endpoint will require reconfiguration.
        //

        HardEndpoint->CurrentEndpoint = NULL;
    }

    //
    // If there are no transfers pending, kick this one off.
    //

    INSERT_BEFORE(&(TransferSet->ListEntry), &(HardEndpoint->TransferList));
    SoftEndpoint->InFlight += 1;
    if (HardEndpoint->TransferList.Next == &(TransferSet->ListEntry)) {

        ASSERT(SoftEndpoint->InFlight == 1);

        MusbpExecuteNextTransfer(Controller, HardEndpoint);
    }

    Status = STATUS_SUCCESS;

SubmitTransferEnd:
    MusbpReleaseLock(Controller);
    return Status;
}

KSTATUS
MusbpSubmitPolledTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    )

/*++

Routine Description:

    This routine submits a transfer to the USB host controller for execution
    and busy waits for it to complete. This routine is meant for crash dump
    support to allow USB transfers when the system is fragile. As a result, it
    forgoes acquiring the normal sequence of locks.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies the context pointer provided to the USB core by
        the host controller when the endpoint was created.

    Transfer - Supplies a pointer to the USB transfer to execute.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    STATUS_SUCCESS if the transfer was successfully added to the hardware queue.

    Failure codes if the transfer could not be added.

--*/

{

    PMUSB_CONTROLLER Controller;
    PMUSB_HARD_ENDPOINT HardEndpoint;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    KSTATUS Status;
    ULONG TransferCount;
    PMUSB_TRANSFER_SET TransferSet;

    Controller = HostControllerContext;
    SoftEndpoint = EndpointContext;
    HardEndpoint = &(Controller->Endpoints[SoftEndpoint->HardwareIndex]);
    TransferSet = TransferContext;

    //
    // Clear the DMA flag on the endpoint.
    //

    if (SoftEndpoint->HardwareIndex != 0) {
        if (SoftEndpoint->Direction == UsbTransferDirectionOut) {
            SoftEndpoint->Control &= ~MUSB_TX_CONTROL_DMA_ENABLE;

        } else {
            SoftEndpoint->Control &= ~MUSB_RX_CONTROL_DMA_ENABLE;
        }
    }

    MusbpAssignEndpoint(Controller, SoftEndpoint);
    Status = MusbpInitializeTransfer(Controller,
                                     SoftEndpoint,
                                     Transfer,
                                     TransferSet);

    if (!KSUCCESS(Status)) {
        goto SubmitPolledTransferEnd;
    }

    //
    // Stick this transfer on the head of the list, and then work through it.
    //

    INSERT_AFTER(&(TransferSet->ListEntry), &(HardEndpoint->TransferList));
    SoftEndpoint->InFlight += 1;
    HardEndpoint->CurrentEndpoint = NULL;
    MusbpExecuteNextTransfer(Controller, HardEndpoint);
    Status = MusbpFlushEndpoint(Controller, SoftEndpoint, &TransferCount);
    if (!KSUCCESS(Status)) {
        goto SubmitPolledTransferEnd;
    }

    if (TransferCount != 1) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto SubmitPolledTransferEnd;
    }

SubmitPolledTransferEnd:
    return Status;
}

KSTATUS
MusbpCancelTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    )

/*++

Routine Description:

    This routine submits attempts to cancel a transfer that was previously
    submitted for execution.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies the context pointer provided to the USB core by
        the host controller when the endpoint was created.

    Transfer - Supplies a pointer to the USB transfer to execute.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    STATUS_SUCCESS if the transfer was successfully removed from the hardware
    queue.

    STATUS_TOO_LATE if the transfer had already completed.

    Other failure codes if the transfer could not be cancelled but has not yet
    completed.

--*/

{

    PMUSB_CONTROLLER Controller;
    PMUSB_HARD_ENDPOINT HardEndpoint;
    UCHAR HardwareIndex;
    PMUSB_TRANSFER MusbTransfer;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    KSTATUS Status;
    PMUSB_TRANSFER_SET TransferSet;

    Controller = HostControllerContext;
    SoftEndpoint = EndpointContext;
    TransferSet = TransferContext;
    MusbpAcquireLock(Controller);
    HardwareIndex = SoftEndpoint->HardwareIndex;
    HardEndpoint = &(Controller->Endpoints[HardwareIndex]);
    if (TransferSet->ListEntry.Next == NULL) {
        Status = STATUS_TOO_LATE;
        goto CancelTransferEnd;
    }

    //
    // If the transfer hasn't even started yet, then this is super easy.
    //

    ASSERT(TransferSet->ListEntry.Next != NULL);

    if (HardEndpoint->TransferList.Next != &(TransferSet->ListEntry)) {

        ASSERT(TransferSet->CurrentIndex == 0);

    } else {

        ASSERT(TransferSet->CurrentIndex < TransferSet->Count);

        MusbTransfer = &(TransferSet->Transfers[TransferSet->CurrentIndex]);
        MusbpAbortTransfer(Controller, HardwareIndex, MusbTransfer);
        MusbpUpdateDataToggle(Controller, TransferSet);
    }

    LIST_REMOVE(&(TransferSet->ListEntry));
    TransferSet->ListEntry.Next = NULL;

    ASSERT(SoftEndpoint->InFlight != 0);

    SoftEndpoint->InFlight -= 1;

    //
    // If the hardware endpoint has another transfer to do, kick that off now.
    //

    if (!LIST_EMPTY(&(HardEndpoint->TransferList))) {
        MusbpExecuteNextTransfer(Controller, HardEndpoint);
    }

    Status = STATUS_SUCCESS;

CancelTransferEnd:
    if (KSUCCESS(Status)) {
        Transfer->Public.Status = STATUS_OPERATION_CANCELLED;
        Transfer->Public.Error = UsbErrorTransferCancelled;
        UsbHostProcessCompletedTransfer(TransferSet->UsbTransfer);
    }

    MusbpReleaseLock(Controller);
    return Status;
}

KSTATUS
MusbpGetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    )

/*++

Routine Description:

    This routine queries the host controller for the status of the root hub.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    HubStatus - Supplies a pointer where the host controller should fill out
        the root hub status.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    Failure codes if the status could not be queried.

--*/

{

    PMUSB_CONTROLLER Controller;
    PUSB_PORT_STATUS PortStatus;
    USHORT SoftwareStatus;
    USHORT Value;

    Controller = HostControllerContext;
    PortStatus = &(HubStatus->PortStatus[0]);
    SoftwareStatus = 0;
    Value = MUSB_READ8(Controller, MusbPower);
    if ((Value & MUSB_POWER_HIGH_SPEED) != 0) {
        SoftwareStatus = USB_PORT_STATUS_ENABLED | USB_PORT_STATUS_CONNECTED;
        HubStatus->PortDeviceSpeed[0] = UsbDeviceSpeedHigh;

    } else {
        Value = MUSB_READ8(Controller, MusbDeviceControl);
        if ((Value & MUSB_DEVICE_CONTROL_FULL_SPEED) != 0) {
            SoftwareStatus = USB_PORT_STATUS_ENABLED |
                             USB_PORT_STATUS_CONNECTED;

            HubStatus->PortDeviceSpeed[0] = UsbDeviceSpeedFull;

        } else if ((Value & MUSB_DEVICE_CONTROL_LOW_SPEED) != 0) {
            SoftwareStatus = USB_PORT_STATUS_ENABLED |
                             USB_PORT_STATUS_CONNECTED;

            HubStatus->PortDeviceSpeed[0] = UsbDeviceSpeedLow;
        }
    }

    PortStatus->Change |= SoftwareStatus ^ PortStatus->Status;
    PortStatus->Status = SoftwareStatus;
    return STATUS_SUCCESS;
}

KSTATUS
MusbpSetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    )

/*++

Routine Description:

    This routine sets the state of the root hub in the USB host controller. It
    looks at the status change bits for each port in order to determine what
    needs to be set.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    HubStatus - Supplies a pointer to the status that should be set in the root
        hub.

Return Value:

    STATUS_SUCCESS if the hub state was successfully programmed into the device.

    Failure codes if the status could not be set.

--*/

{

    PMUSB_CONTROLLER Controller;
    PUSB_PORT_STATUS PortStatus;
    UCHAR Power;

    Controller = HostControllerContext;
    PortStatus = &(HubStatus->PortStatus[0]);
    if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_ENABLED) != 0) {
        PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_ENABLED;
    }

    if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_RESET) != 0) {
        if ((PortStatus->Status & USB_HUB_PORT_STATUS_RESET) != 0) {
            Power = MUSB_READ8(Controller, MusbPower);
            Power |= MUSB_POWER_RESET;
            MUSB_WRITE8(Controller, MusbPower, Power);
            HlBusySpin(20 * 1000);
            Power &= ~MUSB_POWER_RESET;
            MUSB_WRITE8(Controller, MusbPower, Power);
        }

        PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_RESET;
    }

    return STATUS_SUCCESS;
}

VOID
MusbpProcessUsbInterrupts (
    PMUSB_CONTROLLER Controller,
    UCHAR UsbInterrupts
    )

/*++

Routine Description:

    This routine handles incoming general USB interrupts.

Arguments:

    Controller - Supplies a pointer to the controller.

    UsbInterrupts - Supplies the USB interrupts that occurred.

Return Value:

    None.

--*/

{

    UCHAR DeviceControl;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);

    MusbpAcquireLock(Controller);
    if ((UsbInterrupts & MUSB_USB_INTERRUPT_DISCONNECT) != 0) {
        Controller->Connected = FALSE;
        MusbpFailAllTransfers(Controller);
        UsbHostNotifyPortChange(Controller->UsbCoreHandle);
    }

    if ((UsbInterrupts & MUSB_USB_INTERRUPT_CONNECT) != 0) {
        Controller->Connected = TRUE;
        UsbHostNotifyPortChange(Controller->UsbCoreHandle);
    }

    //
    // If there was a VBUS error, just try to power the session back up.
    //

    if ((UsbInterrupts & MUSB_USB_INTERRUPT_VBUS_ERROR) != 0) {
        DeviceControl = MUSB_READ8(Controller, MusbDeviceControl);
        DeviceControl |= MUSB_DEVICE_CONTROL_SESSION;
        MUSB_WRITE8(Controller, MusbDeviceControl, DeviceControl);
    }

    MusbpReleaseLock(Controller);
    return;
}

VOID
MusbpProcessEndpointInterrupts (
    PMUSB_CONTROLLER Controller,
    ULONG EndpointInterrupts
    )

/*++

Routine Description:

    This routine handles incoming USB endpoint interrupts.

Arguments:

    Controller - Supplies a pointer to the controller.

    EndpointInterrupts - Supplies the endpoint interrupts that occurred.

Return Value:

    None.

--*/

{

    PMUSB_HARD_ENDPOINT HardEndpoint;
    UCHAR HardwareIndex;
    USHORT Interrupts;
    BOOL StartNextTransfer;
    PMUSB_TRANSFER_SET TransferSet;

    //
    // Combine the TX and RX interrupts into one.
    //

    Interrupts = (EndpointInterrupts >> 16) | EndpointInterrupts;
    MusbpAcquireLock(Controller);
    while (Interrupts != 0) {
        HardwareIndex = RtlCountTrailingZeros32(Interrupts);
        Interrupts &= ~(1 << HardwareIndex);
        HardEndpoint = &(Controller->Endpoints[HardwareIndex]);

        //
        // Process a completed transfer. If this caused the entire set to
        // complete, then send the transfer back to USB core.
        //

        if (!LIST_EMPTY(&(HardEndpoint->TransferList))) {
            TransferSet = MusbpProcessCompletedTransfer(Controller,
                                                        HardwareIndex,
                                                        &StartNextTransfer);

            if (TransferSet != NULL) {
                UsbHostProcessCompletedTransfer(TransferSet->UsbTransfer);
            }

            //
            // Kick off the next thing to do on this endpoint.
            //

            if (StartNextTransfer != FALSE) {
                MusbpExecuteNextTransfer(Controller, HardEndpoint);
            }
        }
    }

    MusbpReleaseLock(Controller);
    return;
}

KSTATUS
MusbpInitializeTransfer (
    PMUSB_CONTROLLER Controller,
    PMUSB_SOFT_ENDPOINT SoftEndpoint,
    PUSB_TRANSFER_INTERNAL Transfer,
    PMUSB_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine initializes the necessary transfer structures in preparation
    for executing a new USB transfer. The hardware endpoint must be assigned
    prior to this routine.

Arguments:

    Controller - Supplies a pointer to the controller.

    SoftEndpoint - Supplies a pointer to the endpoint.

    Transfer - Supplies a pointer to the USB transfer to execute.

    TransferSet - Supplies a pointer to the MUSB transfer set.

Return Value:

    Status code.

--*/

{

    ULONG BufferOffset;
    ULONG DmaEndpoint;
    BOOL ForceShortTransfer;
    PMUSB_TRANSFER MusbTransfer;
    BOOL ShortTransfer;
    ULONG TransferIndex;
    ULONG TransferSize;
    BOOL Transmit;

    ASSERT(((Transfer->Type == UsbTransferTypeControl) &&
            (SoftEndpoint->HardwareIndex == 0)) ||
           ((Transfer->Type != UsbTransferTypeControl) &&
            (SoftEndpoint->HardwareIndex != 0)));

    ASSERT(Transfer->EndpointNumber == SoftEndpoint->EndpointNumber);

    Transmit = TRUE;
    Transfer->Public.Status = STATUS_SUCCESS;
    Transfer->Public.Error = UsbErrorNone;
    TransferSet->SoftEndpoint = SoftEndpoint;
    TransferSet->CurrentIndex = 0;
    DmaEndpoint = CPPI_USB_ENDPOINT_TO_DMA(SoftEndpoint->HardwareIndex);
    ForceShortTransfer = FALSE;
    if ((Transfer->Public.Flags &
         USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER) != 0) {

        ForceShortTransfer = TRUE;
    }

    //
    // Go around and fill out the transfers. Make sure the data transfer end
    // with a short transfer if required and that zero-length transfers are
    // allowed.
    //

    ShortTransfer = FALSE;
    TransferIndex = 0;
    BufferOffset = 0;
    MusbTransfer = &(TransferSet->Transfers[0]);
    while ((BufferOffset < Transfer->Public.Length) ||
           ((ShortTransfer == FALSE) &&
            ((Transfer->Public.Length == 0) ||
             (ForceShortTransfer != FALSE)))) {

        //
        // If this is a control transfer on the first packet, it's a setup
        // packet.
        //

        if ((BufferOffset == 0) && (SoftEndpoint->HardwareIndex == 0)) {

            ASSERT(Transfer->Public.Length >= sizeof(USB_SETUP_PACKET));

            MusbTransfer->Flags = MUSB_TRANSFER_OUT | MUSB_TRANSFER_SETUP;
            MusbTransfer->Size = sizeof(USB_SETUP_PACKET);

        } else {
            TransferSize = Transfer->Public.Length - BufferOffset;
            if (TransferSize < SoftEndpoint->MaxPayload) {
                ShortTransfer = TRUE;

            } else {
                TransferSize = SoftEndpoint->MaxPayload;
            }

            MusbTransfer->Size = TransferSize;
            MusbTransfer->Flags = 0;
            if (Transfer->Public.Direction == UsbTransferDirectionOut) {
                MusbTransfer->Flags |= MUSB_TRANSFER_OUT;
                Transmit = TRUE;
                if ((SoftEndpoint->Control & MUSB_TX_CONTROL_DMA_ENABLE) != 0) {
                    MusbTransfer->Flags |= MUSB_TRANSFER_DMA;
                }

            } else {
                Transmit = FALSE;
                if ((SoftEndpoint->Control & MUSB_RX_CONTROL_DMA_ENABLE) != 0) {
                    MusbTransfer->Flags |= MUSB_TRANSFER_DMA;
                }
            }
        }

        if (MusbTransfer->Size != 0) {
            MusbTransfer->BufferVirtual = Transfer->Public.Buffer +
                                          BufferOffset;

            MusbTransfer->BufferPhysical =
                                       Transfer->Public.BufferPhysicalAddress +
                                       BufferOffset;

        } else {
            MusbTransfer->BufferVirtual = NULL;
            MusbTransfer->BufferPhysical = 0;
        }

        //
        // Initialize the DMA descriptor if there is one.
        //

        if (MusbTransfer->DmaData.Descriptor != NULL) {

            ASSERT(SoftEndpoint->HardwareIndex != 0);

            CppiInitializeDescriptor(Controller->CppiDma,
                                     &(MusbTransfer->DmaData),
                                     DmaEndpoint,
                                     Transmit,
                                     MusbTransfer->BufferPhysical,
                                     MusbTransfer->Size);
        }

        BufferOffset += MusbTransfer->Size;
        MusbTransfer += 1;
        TransferIndex += 1;
    }

    //
    // Add the status phase if needed. The status phase always has the opposite
    // direction of the data phase.
    //

    if (SoftEndpoint->HardwareIndex == 0) {
        MusbTransfer->Flags = MUSB_TRANSFER_STATUS;
        if (Transfer->Public.Direction == UsbTransferDirectionIn) {
            MusbTransfer->Flags |= MUSB_TRANSFER_OUT;
        }

        MusbTransfer->Size = 0;
        MusbTransfer->BufferVirtual = NULL;
        MusbTransfer->BufferPhysical = 0;
        TransferIndex += 1;
    }

    ASSERT(TransferIndex <= TransferSet->MaxCount);

    TransferSet->Count = TransferIndex;
    TransferSet->UsbTransfer = Transfer;
    return STATUS_SUCCESS;
}

VOID
MusbpExecuteNextTransfer (
    PMUSB_CONTROLLER Controller,
    PMUSB_HARD_ENDPOINT HardEndpoint
    )

/*++

Routine Description:

    This routine begins the next transfer on the given endpoint. This routine
    assumes the controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    HardEndpoint - Supplies a pointer to the endpoint to execute a transfer on.

Return Value:

    None.

--*/

{

    USHORT Control;
    UCHAR HardwareIndex;
    ULONG Register;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    PMUSB_TRANSFER Transfer;
    PMUSB_TRANSFER_SET TransferSet;

    if (LIST_EMPTY(&(HardEndpoint->TransferList))) {
        return;
    }

    TransferSet = LIST_VALUE(HardEndpoint->TransferList.Next,
                             MUSB_TRANSFER_SET,
                             ListEntry);

    SoftEndpoint = TransferSet->SoftEndpoint;
    HardwareIndex = SoftEndpoint->HardwareIndex;
    MusbpConfigureHardwareEndpoint(Controller, SoftEndpoint);
    Transfer = &(TransferSet->Transfers[TransferSet->CurrentIndex]);

    //
    // In DMA mode, enqueue the packet into the DMA controller. This actually
    // kicks off the DMA.
    //

    if ((Transfer->Flags & MUSB_TRANSFER_DMA) != 0) {

        ASSERT(Transfer->DmaData.Descriptor != NULL);

        CppiSubmitDescriptor(Controller->CppiDma, &(Transfer->DmaData));

    } else {

        //
        // If this is an out transfer, fill the FIFO with the data.
        //

        if ((Transfer->Flags & MUSB_TRANSFER_OUT) != 0) {
            MusbpWriteFifo(Controller,
                           HardwareIndex,
                           Transfer->BufferVirtual,
                           Transfer->Size);
        }
    }

    //
    // Enable interrupts.
    //

    if ((Transfer->Flags & MUSB_TRANSFER_OUT) != 0) {
        Controller->TxInterruptEnable |= 1 << HardwareIndex;
        MUSB_WRITE16(Controller,
                     MusbInterruptEnableTx,
                     Controller->TxInterruptEnable);

    } else {
        Controller->RxInterruptEnable |= 1 << HardwareIndex;
        MUSB_WRITE16(Controller,
                     MusbInterruptEnableRx,
                     Controller->RxInterruptEnable);
    }

    //
    // For outbound DMA transfers, there's no need to write the TX ready bit,
    // so just return.
    //

    if ((Transfer->Flags & (MUSB_TRANSFER_OUT | MUSB_TRANSFER_DMA)) ==
        (MUSB_TRANSFER_OUT | MUSB_TRANSFER_DMA)) {

        return;
    }

    //
    // Kick off the transfer by writing to the control register.
    //

    Control = SoftEndpoint->Control;
    if (HardwareIndex == 0) {
        if ((Transfer->Flags & MUSB_TRANSFER_OUT) != 0) {
            Control |= MUSB_EP0_CONTROL_TX_PACKET_READY;

        } else {
            Control |= MUSB_EP0_CONTROL_REQUEST_PACKET;
        }

        if ((Transfer->Flags & MUSB_TRANSFER_SETUP) != 0) {
            Control |= MUSB_EP0_CONTROL_SETUP_PACKET;

        } else if ((Transfer->Flags & MUSB_TRANSFER_STATUS) != 0) {
            Control |= MUSB_EP0_CONTROL_STATUS_PACKET;
        }

        Register = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus, 0);

    } else if ((Transfer->Flags & MUSB_TRANSFER_OUT) != 0) {
        Control |= MUSB_TX_CONTROL_PACKET_READY;
        Register = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus, HardwareIndex);

    } else {
        Control |= MUSB_RX_CONTROL_REQUEST_PACKET;
        Register = MUSB_ENDPOINT_CONTROL(MusbRxControlStatus, HardwareIndex);
    }

    //
    // Only write the low byte of control.
    //

    MUSB_WRITE8(Controller, Register, Control);
    return;
}

PMUSB_TRANSFER_SET
MusbpProcessCompletedTransfer (
    PMUSB_CONTROLLER Controller,
    UCHAR HardwareIndex,
    PBOOL TransferCompleted
    )

/*++

Routine Description:

    This routine processes a completed USB transfer on a hardware endpoint.
    This routine assumes the controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    HardwareIndex - Supplies the hardware endpoint index of the transfer that
        completed.

    TransferCompleted - Supplies a pointer where a boolean will be returned
        indicating if this transfer actually completed (TRUE) and the next
        transfer should be executed, or if this transfer is still going (FALSE).

Return Value:

    Returns a pointer to the transfer set that just completed and was removed.

    NULL if the current transfer is still in progress.

--*/

{

    BOOL CompleteSet;
    USHORT Control;
    ULONG ControlRegister;
    PMUSB_HARD_ENDPOINT HardEndpoint;
    ULONG Register;
    ULONG RxCount;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    PMUSB_TRANSFER Transfer;
    PMUSB_TRANSFER_SET TransferSet;
    PUSB_TRANSFER UsbTransfer;

    *TransferCompleted = FALSE;
    CompleteSet = FALSE;
    HardEndpoint = &(Controller->Endpoints[HardwareIndex]);

    ASSERT(!LIST_EMPTY(&(HardEndpoint->TransferList)));

    TransferSet = LIST_VALUE(HardEndpoint->TransferList.Next,
                             MUSB_TRANSFER_SET,
                             ListEntry);

    ASSERT(TransferSet->CurrentIndex < TransferSet->Count);

    SoftEndpoint = TransferSet->SoftEndpoint;
    Transfer = &(TransferSet->Transfers[TransferSet->CurrentIndex]);
    UsbTransfer = &(TransferSet->UsbTransfer->Public);

    //
    // Handle a completed control transfer if this is endpoint zero.
    //

    if (HardwareIndex == 0) {
        ControlRegister = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus,
                                                HardwareIndex);

        Control = MUSB_READ16(Controller, ControlRegister);

        //
        // If the transfer's not actually finished, the interrupt was spurious
        // or stale.
        //

        if ((Transfer->Flags & MUSB_TRANSFER_OUT) != 0) {
            if ((Control & MUSB_EP0_CONTROL_TX_PACKET_READY) != 0) {
                return NULL;
            }

        } else {
            if ((Control &
                 (MUSB_EP0_CONTROL_RX_PACKET_READY |
                  MUSB_EP0_CONTROL_ERROR_MASK)) == 0) {

                return NULL;
            }
        }

        //
        // For IN transfers, read the data from the FIFO.
        //

        if (((Transfer->Flags & MUSB_TRANSFER_OUT) == 0) &&
            ((Control & MUSB_EP0_CONTROL_RX_PACKET_READY) != 0)) {

            Register = MUSB_ENDPOINT_CONTROL(MusbCount, HardwareIndex);
            RxCount = MUSB_READ16(Controller, Register);

            ASSERT(RxCount <= Transfer->Size);

            if (RxCount >= Transfer->Size) {
                RxCount = Transfer->Size;
            }

            UsbTransfer->LengthTransferred += RxCount;
            MusbpReadFifo(Controller,
                          HardwareIndex,
                          Transfer->BufferVirtual,
                          RxCount);

            //
            // Handle a shorted transfer.
            //

            if (RxCount < Transfer->Size) {
                if ((UsbTransfer->Flags &
                     USB_TRANSFER_FLAG_NO_SHORT_TRANSFERS) != 0) {

                    if (KSUCCESS(UsbTransfer->Status)) {
                        UsbTransfer->Status = STATUS_DATA_LENGTH_MISMATCH;
                        UsbTransfer->Error = UsbErrorShortPacket;
                    }
                }

                //
                // Move to the status phase (or one before to account for the
                // increment at the end of the function).
                //

                ASSERT(TransferSet->CurrentIndex < TransferSet->Count - 1);

                TransferSet->CurrentIndex = TransferSet->Count - 2;
            }
        }

        //
        // Fail the transfer if there was an error.
        //

        if ((Control & MUSB_EP0_CONTROL_ERROR_MASK) != 0) {
            CompleteSet = TRUE;
            UsbTransfer->Status = STATUS_DEVICE_IO_ERROR;

            //
            // Write those error bits to clear them, and perform any FIFO
            // cleanup needed.
            //

            MUSB_WRITE16(Controller, ControlRegister, Control);
            MusbpAbortTransfer(Controller, HardwareIndex, Transfer);
            if ((Control & MUSB_EP0_CONTROL_ERROR) != 0) {
                UsbTransfer->Error = UsbErrorTransferCrcOrTimeoutError;

            } else if ((Control & MUSB_EP0_CONTROL_RX_STALL) != 0) {
                UsbTransfer->Error = UsbErrorTransferStalled;

            } else if ((Control & MUSB_EP0_CONTROL_NAK_TIMEOUT) != 0) {
                UsbTransfer->Error = UsbErrorTransferNakReceived;

            } else {

                ASSERT(FALSE);

            }

        } else {
            if ((Transfer->Flags & MUSB_TRANSFER_OUT) != 0) {
                UsbTransfer->LengthTransferred += Transfer->Size;
            }
        }

        //
        // The data toggle bit in the soft endpoint does not need updating
        // because it will never migrate to another hardware endpoint, and
        // a control transfer is never broken up by other requests.
        //

    //
    // Handle a completed OUT transfer.
    //

    } else if ((Transfer->Flags & MUSB_TRANSFER_OUT) != 0) {
        ControlRegister = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus,
                                                HardwareIndex);

        Control = MUSB_READ16(Controller, ControlRegister);

        //
        // In DMA mode, the packet ready and FIFO full bits might still be set
        // (even though the DMA transfer supposedly completed). The original
        // code spun here waiting for those bits to clear, but that turned out
        // to be a very significant portion of time. Instead it seems to be
        // okay to clear the control register, and then spin on seeing the
        // descriptor show up in the CPPI completion queue.
        //

        //
        // In non-DMA mode, there's a FIFO empty interrupt, so if the FIFO is
        // not currently empty just wait for that.
        //

        if ((Transfer->Flags & MUSB_TRANSFER_DMA) == 0) {
            if ((Control & MUSB_TX_CONTROL_PACKET_READY) != 0) {
                return NULL;
            }
        }

        if ((Control & MUSB_TX_CONTROL_ERROR_MASK) != 0) {
            CompleteSet = TRUE;
            UsbTransfer->Status = STATUS_DEVICE_IO_ERROR;

            //
            // Write those error bits to clear them, and perform any FIFO
            // cleanup needed.
            //

            MUSB_WRITE16(Controller, ControlRegister, Control);
            MusbpAbortTransfer(Controller, HardwareIndex, Transfer);
            if ((Control & MUSB_TX_CONTROL_ERROR) != 0) {
                UsbTransfer->Error = UsbErrorTransferCrcOrTimeoutError;

            } else if ((Control & MUSB_TX_CONTROL_RX_STALL) != 0) {
                UsbTransfer->Error = UsbErrorTransferStalled;

            } else if ((Control & MUSB_TX_CONTROL_NAK_TIMEOUT) != 0) {
                UsbTransfer->Error = UsbErrorTransferNakReceived;

            } else {

                ASSERT(FALSE);

            }

        } else {
            UsbTransfer->LengthTransferred += Transfer->Size;
            if ((Transfer->Flags & MUSB_TRANSFER_DMA) != 0) {
                CppiReapCompletedDescriptor(Controller->CppiDma,
                                            &(Transfer->DmaData),
                                            NULL);
            }
        }

        //
        // Update the data toggle bit.
        //

        SoftEndpoint->Control = (SoftEndpoint->Control &
                                 ~MUSB_TX_CONTROL_DATA_TOGGLE) |
                                (Control & MUSB_TX_CONTROL_DATA_TOGGLE);

    //
    // Handle a completed IN transfer.
    //

    } else {
        ControlRegister = MUSB_ENDPOINT_CONTROL(MusbRxControlStatus,
                                                HardwareIndex);

        Control = MUSB_READ16(Controller, ControlRegister);

        //
        // Incoming NAK timeouts aren't actually errors (except on isochronous
        // channels.
        //

        if ((Control & MUSB_RX_CONTROL_DATA_ERROR_NAK_TIMEOUT) != 0) {
            if ((TransferSet->UsbTransfer->Type !=
                 UsbTransferTypeIsochronous) &&
                (SoftEndpoint->Interval == 0)) {

                Control &= ~MUSB_RX_CONTROL_DATA_ERROR_NAK_TIMEOUT;
                MUSB_WRITE16(Controller, ControlRegister, Control);
                return NULL;
            }
        }

        //
        // Handle errors first.
        //

        if ((Control & MUSB_RX_CONTROL_ERROR_MASK) != 0) {
            CompleteSet = TRUE;
            UsbTransfer->Status = STATUS_DEVICE_IO_ERROR;

            //
            // Write those error bits to clear them, and perform any FIFO
            // cleanup needed.
            //

            MUSB_WRITE16(Controller, ControlRegister, Control);
            MusbpAbortTransfer(Controller, HardwareIndex, Transfer);
            if ((Control & MUSB_RX_CONTROL_ERROR) != 0) {
                UsbTransfer->Error = UsbErrorTransferCrcOrTimeoutError;

            } else if ((Control & MUSB_RX_CONTROL_RX_STALL) != 0) {
                UsbTransfer->Error = UsbErrorTransferStalled;

            } else if ((Control &
                        MUSB_RX_CONTROL_DATA_ERROR_NAK_TIMEOUT) != 0) {

                if (TransferSet->UsbTransfer->Type ==
                    UsbTransferTypeIsochronous) {

                    UsbTransfer->Error = UsbErrorTransferCrcOrTimeoutError;

                } else {
                    UsbTransfer->Error = UsbErrorTransferNakReceived;
                }

            } else {

                ASSERT(FALSE);

            }

        //
        // There are no errors. If the request packet flag is clear and either
        // this is DMA or the packet ready flag is set, go get the data.
        //

        } else if (((Control & MUSB_RX_CONTROL_REQUEST_PACKET) == 0) &&
                   (((Transfer->Flags & MUSB_TRANSFER_DMA) != 0) ||
                    ((Control & MUSB_RX_CONTROL_PACKET_READY) != 0))) {

            Register = MUSB_ENDPOINT_CONTROL(MusbCount, HardwareIndex);
            if ((Transfer->Flags & MUSB_TRANSFER_DMA) != 0) {
                CppiReapCompletedDescriptor(Controller->CppiDma,
                                            &(Transfer->DmaData),
                                            &RxCount);

                ASSERT(RxCount <= Transfer->Size);

            } else {
                RxCount = MUSB_READ16(Controller, Register);

                //
                // If the RX count is more than the transfer size, then it means
                // the RX max packet size was programmed incorrectly.
                //

                ASSERT(RxCount <= Transfer->Size);

                if (RxCount >= Transfer->Size) {
                    RxCount = Transfer->Size;
                }

                MusbpReadFifo(Controller,
                              HardwareIndex,
                              Transfer->BufferVirtual,
                              RxCount);
            }

            UsbTransfer->LengthTransferred += RxCount;

            //
            // Account for a shorted transfer.
            //

            if (RxCount < Transfer->Size) {
                CompleteSet = TRUE;
                if ((UsbTransfer->Flags &
                     USB_TRANSFER_FLAG_NO_SHORT_TRANSFERS) != 0) {

                    UsbTransfer->Status = STATUS_DATA_LENGTH_MISMATCH;
                    UsbTransfer->Error = UsbErrorShortPacket;
                }
            }
        }

        //
        // Update the data toggle bit.
        //

        SoftEndpoint->Control = (SoftEndpoint->Control &
                                 ~MUSB_RX_CONTROL_DATA_TOGGLE) |
                                (Control & MUSB_RX_CONTROL_DATA_TOGGLE);
    }

    MUSB_WRITE16(Controller, ControlRegister, 0);
    *TransferCompleted = TRUE;
    TransferSet->CurrentIndex += 1;
    if (TransferSet->CurrentIndex == TransferSet->Count) {
        CompleteSet = TRUE;
    }

    if (CompleteSet != FALSE) {
        LIST_REMOVE(&(TransferSet->ListEntry));
        TransferSet->ListEntry.Next = NULL;

        ASSERT(SoftEndpoint->InFlight != 0);

        SoftEndpoint->InFlight -= 1;

    } else {
        TransferSet = NULL;
    }

    return TransferSet;
}

VOID
MusbpFailAllTransfers (
    PMUSB_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine completes all pending USB transfers, failing everything with
    a device not connected error. This routine assumes the controller lock is
    already held.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

{

    PMUSB_HARD_ENDPOINT HardEndpoint;
    ULONG HardwareIndex;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    PMUSB_TRANSFER Transfer;
    PMUSB_TRANSFER_SET TransferSet;
    PUSB_TRANSFER_INTERNAL UsbTransfer;

    for (HardwareIndex = 0;
         HardwareIndex < Controller->EndpointCount;
         HardwareIndex += 1) {

        HardEndpoint = &(Controller->Endpoints[HardwareIndex]);
        if (LIST_EMPTY(&(HardEndpoint->TransferList))) {
            continue;
        }

        //
        // Kill the first transfer, which is the tricky one since it's in the
        // hardware.
        //

        TransferSet = LIST_VALUE(HardEndpoint->TransferList.Next,
                                 MUSB_TRANSFER_SET,
                                 ListEntry);

        SoftEndpoint = TransferSet->SoftEndpoint;
        Transfer = &(TransferSet->Transfers[TransferSet->CurrentIndex]);
        MusbpAbortTransfer(Controller, HardwareIndex, Transfer);
        MusbpUpdateDataToggle(Controller, TransferSet);
        UsbTransfer = TransferSet->UsbTransfer;
        UsbTransfer->Public.Status = STATUS_DEVICE_IO_ERROR;
        UsbTransfer->Public.Error = UsbErrorTransferDeviceNotConnected;
        LIST_REMOVE(&(TransferSet->ListEntry));
        TransferSet->ListEntry.Next = NULL;

        ASSERT(SoftEndpoint->InFlight != 0);

        SoftEndpoint->InFlight -= 1;
        UsbHostProcessCompletedTransfer(UsbTransfer);

        //
        // Now process all the other transfers, which were never even started.
        //

        while (!LIST_EMPTY(&(HardEndpoint->TransferList))) {
            TransferSet = LIST_VALUE(HardEndpoint->TransferList.Next,
                                     MUSB_TRANSFER_SET,
                                     ListEntry);

            UsbTransfer = TransferSet->UsbTransfer;
            UsbTransfer->Public.Status = STATUS_DEVICE_IO_ERROR;
            UsbTransfer->Public.Error = UsbErrorTransferDeviceNotConnected;
            LIST_REMOVE(&(TransferSet->ListEntry));
            TransferSet->ListEntry.Next = NULL;

            ASSERT(SoftEndpoint->InFlight != 0);

            SoftEndpoint->InFlight -= 1;
            UsbHostProcessCompletedTransfer(UsbTransfer);
        }
    }

    return;
}

VOID
MusbpConfigureFifo (
    PMUSB_CONTROLLER Controller,
    PMUSB_FIFO_CONFIGURATION Configuration,
    PULONG Offset
    )

/*++

Routine Description:

    This routine configures a hardware endpoint FIFO.

Arguments:

    Controller - Supplies a pointer to the controller.

    Configuration - Supplies the configuration to set.

    Offset - Supplies the current FIFO region offset. On output, this will be
        advanced.

Return Value:

    None.

--*/

{

    ULONG CurrentOffset;
    UCHAR Endpoint;
    UCHAR SizeValue;

    ASSERT(Configuration->Endpoint != 0);

    CurrentOffset = *Offset;
    Endpoint = Configuration->Endpoint;
    *Offset += Configuration->MaxPacketSize;

    //
    // The size register is logarithmic, with the max packet size being
    // 2^(sz+3) for single buffer mode, and 2^(sz+4) for double buffer
    // mode.
    //

    SizeValue = RtlCountTrailingZeros32(Configuration->MaxPacketSize) - 3;
    if ((Configuration->Direction == MusbEndpointTx) ||
        (Configuration->Direction == MusbEndpointTxRx)) {

        ASSERT(Controller->Endpoints[Endpoint].TxFifoSize == 0);

        Controller->Endpoints[Endpoint].TxFifoSize =
                                                  Configuration->MaxPacketSize;

        MusbpWriteIndexed8(Controller, Endpoint, MusbTxFifoSize, SizeValue);

        //
        // The FIFO address register is in units of 8 bytes.
        //

        MusbpWriteIndexed16(Controller,
                            Endpoint,
                            MusbTxFifoAddress,
                            CurrentOffset >> 3);
    }

    if ((Configuration->Direction == MusbEndpointRx) ||
        (Configuration->Direction == MusbEndpointTxRx)) {

        ASSERT(Controller->Endpoints[Endpoint].RxFifoSize == 0);

        Controller->Endpoints[Endpoint].RxFifoSize =
                                                  Configuration->MaxPacketSize;

        MusbpWriteIndexed8(Controller, Endpoint, MusbRxFifoSize, SizeValue);
        MusbpWriteIndexed16(Controller,
                            Endpoint,
                            MusbRxFifoAddress,
                            CurrentOffset >> 3);
    }

    return;
}

VOID
MusbpAbortTransfer (
    PMUSB_CONTROLLER Controller,
    UCHAR HardwareIndex,
    PMUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine aborts a transmit operation by flushing FIFOs and DMA.

Arguments:

    Controller - Supplies a pointer to the controller.

    HardwareIndex - Supplies the hardware endpoint index to abort.

    Transfer - Supplies the transfer that's stuck in there.

Return Value:

    None.

--*/

{

    USHORT Control;
    ULONG ControlRegister;
    KSTATUS Status;

    if ((Transfer->Flags & MUSB_TRANSFER_OUT) != 0) {

        //
        // Flush twice as required for double buffering.
        //

        MusbpFlushFifo(Controller, HardwareIndex, TRUE);
        MusbpFlushFifo(Controller, HardwareIndex, TRUE);
        if ((Transfer->Flags & MUSB_TRANSFER_DMA) != 0) {
            ControlRegister = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus,
                                                    HardwareIndex);

            Control = MUSB_READ16(Controller, ControlRegister);
            Control &= ~MUSB_TX_CONTROL_DMA_ENABLE;
            MUSB_WRITE16(Controller, ControlRegister, Control);

            ASSERT(Transfer->DmaData.Descriptor != NULL);

            Status = CppiTearDownDescriptor(Controller->CppiDma,
                                            &(Transfer->DmaData));

            ASSERT(KSUCCESS(Status));
        }

    } else {
        if (HardwareIndex == 0) {
            ControlRegister = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus, 0);

        } else {
            ControlRegister = MUSB_ENDPOINT_CONTROL(MusbRxControlStatus,
                                                    HardwareIndex);
        }

        //
        // Clear the auto request flag from the high byte of the control
        // register.
        //

        Control = MUSB_READ8(Controller, ControlRegister + 1);
        Control &= ~(MUSB_RX_CONTROL_AUTO_REQUEST >> BITS_PER_BYTE);
        MUSB_WRITE8(Controller, ControlRegister + 1, Control);
        if ((Transfer->Flags & MUSB_TRANSFER_DMA) != 0) {

            //
            // Clear the request packet and DMA enable flags. If a packet
            // squeaked in, flush the FIFO. Then tear down the DMA descriptor.
            //

            Control = MUSB_READ16(Controller, ControlRegister);
            Control &= ~(MUSB_RX_CONTROL_REQUEST_PACKET |
                         MUSB_RX_CONTROL_DMA_ENABLE);

            MUSB_WRITE16(Controller, ControlRegister, Control);
            HlBusySpin(250);
            Control = MUSB_READ16(Controller, ControlRegister);
            if ((Control & MUSB_RX_CONTROL_PACKET_READY) != 0) {
                Control |= MUSB_RX_CONTROL_FLUSH_FIFO;
            }

            Control |= MUSB_RX_CONTROL_ERROR_MASK;
            MUSB_WRITE16(Controller, ControlRegister, Control);
            Status = CppiTearDownDescriptor(Controller->CppiDma,
                                            &(Transfer->DmaData));

            ASSERT(KSUCCESS(Status));

        //
        // Abort a non-DMA transfer.
        //

        } else {
            Control = MUSB_READ8(Controller, ControlRegister);
            Control &= ~MUSB_RX_CONTROL_REQUEST_PACKET;
            MUSB_WRITE8(Controller, ControlRegister, Control);
            HlBusySpin(250);
            MusbpFlushFifo(Controller, HardwareIndex, FALSE);
            MusbpFlushFifo(Controller, HardwareIndex, FALSE);
            MUSB_WRITE16(Controller, ControlRegister, 0);
        }
    }

    return;
}

VOID
MusbpConfigureHardwareEndpoint (
    PMUSB_CONTROLLER Controller,
    PMUSB_SOFT_ENDPOINT SoftEndpoint
    )

/*++

Routine Description:

    This routine sets up the hardware endpoint associated with the given
    software endpoint in preparation for transfer submission. This routine
    assumes the controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    SoftEndpoint - Supplies a pointer to the software endpoint structure.

Return Value:

    None.

--*/

{

    USHORT Control;
    PMUSB_HARD_ENDPOINT HardEndpoint;
    UCHAR HardIndex;
    ULONG Register;

    HardIndex = SoftEndpoint->HardwareIndex;
    HardEndpoint = &(Controller->Endpoints[HardIndex]);

    //
    // If the hardware endpoint is already set up from last time, then there's
    // no need to reprogram it.
    //

    if (HardEndpoint->CurrentEndpoint == SoftEndpoint) {

        //
        // Just write the control register.
        //

        if (SoftEndpoint->Direction == UsbTransferDirectionOut) {
            Register = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus, HardIndex);

        } else {
            Register = MUSB_ENDPOINT_CONTROL(MusbRxControlStatus, HardIndex);
        }

        MUSB_WRITE16(Controller, Register, SoftEndpoint->Control);
        return;
    }

    if (SoftEndpoint->Direction == UsbTransferDirectionOut) {
        Controller->TxInterruptEnable &= ~(1 << HardIndex);
        MUSB_WRITE16(Controller,
                     MusbInterruptEnableTx,
                     Controller->TxInterruptEnable);

        //
        // Write the control registers.
        //

        Register = MUSB_ENDPOINT_CONTROL(MusbTxMaxPacketSize, HardIndex);
        MUSB_WRITE16(Controller, Register, SoftEndpoint->MaxPayload);
        Register = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus, HardIndex);
        if (SoftEndpoint->HardwareIndex == 0) {
            Control = SoftEndpoint->Control |
                      MUSB_EP0_CONTROL_DATA_TOGGLE_WRITE;

        } else {
            Control = SoftEndpoint->Control | MUSB_TX_CONTROL_DATA_TOGGLE_WRITE;
        }

        MUSB_WRITE16(Controller, Register, Control);
        Register = MUSB_ENDPOINT_CONTROL(MusbTxType, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->Type);
        Register = MUSB_ENDPOINT_CONTROL(MusbTxInterval, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->Interval);

        //
        // Write the setup registers.
        //

        Register = MUSB_ENDPOINT_SETUP(MusbTxFunctionAddress, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->Device);
        Register = MUSB_ENDPOINT_SETUP(MusbTxHubAddress, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->HubAddress);
        Register = MUSB_ENDPOINT_SETUP(MusbTxHubPort, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->HubPort);

        //
        // For the control endpoint, initialize both TX and RX setup registers.
        //

        if (SoftEndpoint->HardwareIndex == 0) {
            Register = MUSB_ENDPOINT_SETUP(MusbRxFunctionAddress, HardIndex);
            MUSB_WRITE8(Controller, Register, SoftEndpoint->Device);
            Register = MUSB_ENDPOINT_SETUP(MusbRxHubAddress, HardIndex);
            MUSB_WRITE8(Controller, Register, SoftEndpoint->HubAddress);
            Register = MUSB_ENDPOINT_SETUP(MusbRxHubPort, HardIndex);
        }

    } else {

        ASSERT(SoftEndpoint->Direction == UsbTransferDirectionIn);

        Controller->RxInterruptEnable &= ~(1 << HardIndex);
        MUSB_WRITE16(Controller,
                     MusbInterruptEnableRx,
                     Controller->RxInterruptEnable);

        //
        // Write the control registers.
        //

        Register = MUSB_ENDPOINT_CONTROL(MusbRxMaxPacketSize, HardIndex);
        MUSB_WRITE16(Controller, Register, SoftEndpoint->MaxPayload);
        Register = MUSB_ENDPOINT_CONTROL(MusbRxControlStatus, HardIndex);
        Control = SoftEndpoint->Control | MUSB_RX_CONTROL_DATA_TOGGLE_WRITE;
        MUSB_WRITE16(Controller, Register, Control);
        Register = MUSB_ENDPOINT_CONTROL(MusbRxType, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->Type);
        Register = MUSB_ENDPOINT_CONTROL(MusbRxInterval, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->Interval);

        //
        // Write the setup registers.
        //

        Register = MUSB_ENDPOINT_SETUP(MusbRxFunctionAddress, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->Device);
        Register = MUSB_ENDPOINT_SETUP(MusbRxHubAddress, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->HubAddress);
        Register = MUSB_ENDPOINT_SETUP(MusbRxHubPort, HardIndex);
        MUSB_WRITE8(Controller, Register, SoftEndpoint->HubPort);
    }

    HardEndpoint->CurrentEndpoint = SoftEndpoint;
    return;
}

VOID
MusbpUpdateDataToggle (
    PMUSB_CONTROLLER Controller,
    PMUSB_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine updates the data toggle bit in the control member of the soft
    endpoint corresponding to the given transfer set.

Arguments:

    Controller - Supplies a pointer to the controller.

    TransferSet - Supplies a pointer to the transfer set. The current index
        transfer will be used to determine whether the transfer is IN or OUT.

Return Value:

    None.

--*/

{

    USHORT Control;
    UCHAR HardwareIndex;
    ULONG Register;
    PMUSB_SOFT_ENDPOINT SoftEndpoint;
    PMUSB_TRANSFER Transfer;

    SoftEndpoint = TransferSet->SoftEndpoint;
    HardwareIndex = SoftEndpoint->HardwareIndex;
    Transfer = &(TransferSet->Transfers[TransferSet->CurrentIndex]);

    //
    // Update the data toggle.
    //

    if ((Transfer->Flags & MUSB_TRANSFER_OUT) != 0) {
        if (HardwareIndex != 0) {
            Register = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus,
                                             HardwareIndex);

            Control = MUSB_READ16(Controller, Register);
            SoftEndpoint->Control = (SoftEndpoint->Control &
                                     ~MUSB_TX_CONTROL_DATA_TOGGLE) |
                                    (Control & MUSB_TX_CONTROL_DATA_TOGGLE);

        }

    } else {
        Register = MUSB_ENDPOINT_CONTROL(MusbRxControlStatus,
                                         HardwareIndex);

        Control = MUSB_READ16(Controller, Register);
        SoftEndpoint->Control = (SoftEndpoint->Control &
                                 ~MUSB_RX_CONTROL_DATA_TOGGLE) |
                                (Control & MUSB_RX_CONTROL_DATA_TOGGLE);
    }

    return;
}

VOID
MusbpWriteFifo (
    PMUSB_CONTROLLER Controller,
    UCHAR EndpointIndex,
    PVOID Buffer,
    ULONG Size
    )

/*++

Routine Description:

    This routine writes the given buffer contents to the FIFO.

Arguments:

    Controller - Supplies a pointer to the controller.

    EndpointIndex - Supplies the hardware endpoint number to write to.

    Buffer - Supplies a pointer to the data to write.

    Size - Supplies the number of bytes to write.

Return Value:

    None.

--*/

{

    PUCHAR Bytes;
    ULONG Index;
    ULONG Register;

    Bytes = Buffer;
    Register = MUSB_FIFO_REGISTER(EndpointIndex);
    for (Index = 0; Index < Size; Index += 1) {
        MUSB_WRITE8(Controller, Register, Bytes[Index]);
    }

    return;
}

VOID
MusbpReadFifo (
    PMUSB_CONTROLLER Controller,
    UCHAR EndpointIndex,
    PVOID Buffer,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine reads from the FIFO into the given buffer.

Arguments:

    Controller - Supplies a pointer to the controller.

    EndpointIndex - Supplies the hardware endpoint number to read from.

    Buffer - Supplies a pointer where the read data will be returned..

    BufferSize - Supplies the number of bytes to read into the buffer.

Return Value:

    None.

--*/

{

    PUCHAR Bytes;
    ULONG Index;
    ULONG Register;

    Bytes = Buffer;
    Register = MUSB_FIFO_REGISTER(EndpointIndex);
    for (Index = 0; Index < BufferSize; Index += 1) {
        Bytes[Index] = MUSB_READ8(Controller, Register);
    }

    MmSyncCacheRegion(Buffer, BufferSize);
    return;
}

VOID
MusbpFlushFifo (
    PMUSB_CONTROLLER Controller,
    UCHAR HardwareIndex,
    BOOL HostOut
    )

/*++

Routine Description:

    This routine forcefully flushes the FIFO.

Arguments:

    Controller - Supplies a pointer to the controller.

    HardwareIndex - Supplies the hardware endpoint number to write to.

    HostOut - Supplies a boolean indicating whether to flush the transmit FIFO
        (TRUE) or the receive FIFO (FALSE).

Return Value:

    None.

--*/

{

    UCHAR Control;
    ULONG Register;

    if (HardwareIndex == 0) {

        //
        // If there's something in the FIFO, hit the red button.
        //

        Register = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus, 0);
        Control = MUSB_READ8(Controller, Register);
        if ((Control &
             (MUSB_EP0_CONTROL_TX_PACKET_READY |
              MUSB_EP0_CONTROL_RX_PACKET_READY)) != 0) {

            //
            // Just write the high byte of the control word.
            //

            Control = MUSB_EP0_CONTROL_FLUSH_FIFO >> 8;
            MUSB_WRITE8(Controller, Register + 1, Control);
        }

    } else if (HostOut != FALSE) {

        //
        // If the FIFO is not empty, flush it.
        //

        Register = MUSB_ENDPOINT_CONTROL(MusbTxControlStatus, HardwareIndex);
        Control = MUSB_READ8(Controller, Register);
        if ((Control & MUSB_TX_CONTROL_PACKET_READY) != 0) {
            Control |= MUSB_TX_CONTROL_FLUSH_FIFO | MUSB_TX_CONTROL_ERROR_MASK;
            MUSB_WRITE8(Controller, Register, Control);
        }

    } else {
        Register = MUSB_ENDPOINT_CONTROL(MusbRxControlStatus, HardwareIndex);
        Control = MUSB_READ8(Controller, Register);
        if ((Control & MUSB_RX_CONTROL_PACKET_READY) != 0) {
            Control |= MUSB_RX_CONTROL_FLUSH_FIFO | MUSB_RX_CONTROL_ERROR_MASK;
            MUSB_WRITE8(Controller, Register, Control);
        }
    }

    Controller->Endpoints[HardwareIndex].CurrentEndpoint = NULL;
    return;
}

VOID
MusbpAssignEndpoint (
    PMUSB_CONTROLLER Controller,
    PMUSB_SOFT_ENDPOINT SoftEndpoint
    )

/*++

Routine Description:

    This routine assigns a software endpoint to a hardware endpoint. Ideally it
    tries to find one with no transfers on it already, and will try not to
    move endpoints if possible. This routine assumes the controller lock is
    already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    SoftEndpoint - Supplies a pointer to the endpoint to assign. On output, the
        hardware index of this endpoint will be set.

Return Value:

    None.

--*/

{

    UCHAR Alternate;
    USHORT FifoSize;
    PMUSB_HARD_ENDPOINT HardEndpoint;
    ULONG Index;
    UCHAR SearchIndex;

    //
    // Control endpoints always go to hardware endpoint 0, by hardware mandate.
    //

    if ((SoftEndpoint->Type & MUSB_TXTYPE_PROTOCOL_MASK) ==
        MUSB_TXTYPE_PROTOCOL_CONTROL) {

        SoftEndpoint->HardwareIndex = 0;
        return;
    }

    //
    // If there are already transfers in flight on this endpoint, then it
    // cannot move as that would mess up the ordering of transfers on the bus.
    //

    if (SoftEndpoint->InFlight != 0) {

        ASSERT(SoftEndpoint->HardwareIndex != 0);

        return;
    }

    //
    // This endpoint is not a control endpoint. If its hardware index is not
    // yet assigned, pick a round-robin new one. Otherwise, start from the
    // previous one.
    //

    Alternate = 0;
    SearchIndex = SoftEndpoint->HardwareIndex;
    if (SearchIndex == 0) {
        SearchIndex = Controller->NextEndpointAssignment;
        Controller->NextEndpointAssignment += 1;
        if (Controller->NextEndpointAssignment == Controller->EndpointCount) {
            Controller->NextEndpointAssignment = 1;
        }
    }

    for (Index = 1; Index < Controller->EndpointCount; Index += 1) {

        ASSERT(SearchIndex != 0);

        HardEndpoint = &(Controller->Endpoints[SearchIndex]);
        if (SoftEndpoint->Direction == UsbTransferDirectionOut) {
            FifoSize = HardEndpoint->TxFifoSize;

        } else {
            FifoSize = HardEndpoint->RxFifoSize;
        }

        //
        // If the endpoint has the FIFO space, then this endpoint may work.
        //

        if (SoftEndpoint->MaxPayload <= FifoSize) {

            //
            // If this endpoint has no transfers on it, then definitely use it.
            //

            if (LIST_EMPTY(&(HardEndpoint->TransferList))) {
                SoftEndpoint->HardwareIndex = SearchIndex;
                return;
            }

            //
            // Otherwise, save the endpoint as a backup.
            //

            if (Alternate == 0) {
                Alternate = SearchIndex;

                //
                // This endpoint is moving off what it was before, so clear out
                // the saved configuration, since when the endpoint is
                // destroyed it may never know to clear this old pointer.
                //

                if (HardEndpoint->CurrentEndpoint == SoftEndpoint) {
                    HardEndpoint->CurrentEndpoint = NULL;
                }
            }
        }

        SearchIndex += 1;
        if (SearchIndex == Controller->EndpointCount) {
            SearchIndex = 1;
        }
    }

    //
    // Use the alternate, even though there are transfers queued on it.
    //

    ASSERT(Alternate != 0);

    SoftEndpoint->HardwareIndex = Alternate;
    return;
}

UCHAR
MusbpReadIndexed8 (
    PMUSB_CONTROLLER Controller,
    UCHAR Index,
    MUSB_INDEXED_REGISTER Register
    )

/*++

Routine Description:

    This routine performs an indexed register read. It is assumed the
    controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    Index - Supplies the hardware endpoint index to read from.

    Register - Supplies the register to read.

Return Value:

    Returns the register value.

--*/

{

    if (Controller->CurrentIndex != Index) {
        MUSB_WRITE8(Controller, MusbIndex, Index);
        Controller->CurrentIndex = Index;
    }

    return MUSB_READ8(Controller, Register);
}

UCHAR
MusbpReadIndexed16 (
    PMUSB_CONTROLLER Controller,
    UCHAR Index,
    MUSB_INDEXED_REGISTER Register
    )

/*++

Routine Description:

    This routine performs an indexed register read. It is assumed the
    controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    Index - Supplies the hardware endpoint index to read from.

    Register - Supplies the register to read.

Return Value:

    Returns the register value.

--*/

{

    if (Controller->CurrentIndex != Index) {
        MUSB_WRITE8(Controller, MusbIndex, Index);
        Controller->CurrentIndex = Index;
    }

    return MUSB_READ16(Controller, Register);
}

VOID
MusbpWriteIndexed8 (
    PMUSB_CONTROLLER Controller,
    UCHAR Index,
    MUSB_INDEXED_REGISTER Register,
    UCHAR Value
    )

/*++

Routine Description:

    This routine performs an indexed register write. It is assumed the
    controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    Index - Supplies the hardware endpoint index to write to.

    Register - Supplies the register to write.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    if (Controller->CurrentIndex != Index) {
        MUSB_WRITE8(Controller, MusbIndex, Index);
        Controller->CurrentIndex = Index;
    }

    MUSB_WRITE8(Controller, Register, Value);
    return;
}

VOID
MusbpWriteIndexed16 (
    PMUSB_CONTROLLER Controller,
    UCHAR Index,
    MUSB_INDEXED_REGISTER Register,
    USHORT Value
    )

/*++

Routine Description:

    This routine performs an indexed register write. It is assumed the
    controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    Index - Supplies the hardware endpoint index to write to.

    Register - Supplies the register to write.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    if (Controller->CurrentIndex != Index) {
        MUSB_WRITE8(Controller, MusbIndex, Index);
        Controller->CurrentIndex = Index;
    }

    MUSB_WRITE16(Controller, Register, Value);
    return;
}

VOID
MusbpAcquireLock (
    PMUSB_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine acquires the controller lock.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Controller->Lock));
    Controller->OldRunLevel = OldRunLevel;
    return;
}

VOID
MusbpReleaseLock (
    PMUSB_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine releases the controller lock.

Arguments:

    Controller - Supplies a pointer to the controller.

    OldRunLevel - Stores the original runlevel to return to.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = Controller->OldRunLevel;
    KeReleaseSpinLock(&(Controller->Lock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

