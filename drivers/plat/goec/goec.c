/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    goec.c

Abstract:

    This module implements support for the Google Embedded Controller.

Author:

    Evan Green 25-Aug-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/spb/spb.h>
#include "goec.h"
#include "goecprot.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define GOEC_ALLOCATION_TAG 0x63656F47

//
// Define the amount of time to stall between selecting the device and
// beginning a transmission.
//

#define GOEC_COMMAND_MICROSECOND_DELAY 100

//
// Define the amount of time in seconds to wait for the response to come back.
//

#define GOEC_RESPONSE_TIMEOUT 1

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for a Google Embedded Controller.

Members:

    OsDevice - Stores a pointer to the OS device object.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    Lock - Stores a pointer to a lock serializing access to the controller.

    SpbResource - Stores a pointer to the Simple Peripheral Bus resource
        allocation used to connect to the controller.

    SpbSignedUp - Stores a boolean indicating whether or not interface
        notifications have been signed up for yet or not.

    SpbInterface - Stores a pointer to the Simple Peripheral Bus interface used
        to communicate with the device.

    SpbHandle - Stores the open handle to the Simple Peripheral Bus for this
        device.

    RequestBuffer - Stores a pointer to a buffer of size
        GOEC_PROTO3_MAX_PACKET_SIZE used for request data.

    ResponseBuffer - Stores a pointer to a buffer of size
        GOEC_PROTO3_MAX_PACKET_SIZE used for response data.

    RequestIoBuffer - Stores a pointer to the I/O buffer around the request
        buffer.

    ResponseIoBuffer - Stores a pointer to the I/O buffer around the response
        buffer.

    InterruptEvent - Stores a pointer to the event used to indicate to the
        EC worker thread that an interrupt has fired.

    Irp - Stores a pointer to the state change IRP to work on.

    KeyColumns - Stores the number of keyboard columns in the keyboard.

    KeyRows - Stores the number of keyboard rows in the keyboard.

    KeyState - Stores the previous key state.

    InputHandle - Stores the user input device handle, used to report keyboard
        events to the system.

--*/

typedef struct _GOEC_CONTROLLER {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PQUEUED_LOCK Lock;
    PRESOURCE_ALLOCATION SpbResource;
    BOOL SpbSignedUp;
    PSPB_INTERFACE SpbInterface;
    SPB_HANDLE SpbHandle;
    PVOID RequestBuffer;
    PVOID ResponseBuffer;
    PIO_BUFFER RequestIoBuffer;
    PIO_BUFFER ResponseIoBuffer;
    PKEVENT InterruptEvent;
    ULONG KeyColumns;
    ULONG KeyRows;
    UCHAR KeyState[GOEC_MAX_COLUMNS];
    HANDLE InputHandle;
} GOEC_CONTROLLER, *PGOEC_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
GoecAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
GoecDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
GoecDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
GoecDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
GoecDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
GoecDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
GoecInterruptServiceWorker (
    PVOID Context
    );

KSTATUS
GoecProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
GoecStartDevice (
    PIRP Irp,
    PGOEC_CONTROLLER Device
    );

VOID
GoecSpbInterfaceNotificationCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

VOID
GoecWorkerThread (
    PVOID Parameter
    );

KSTATUS
GoecInitialize (
    PGOEC_CONTROLLER Controller
    );

KSTATUS
GoecSayHello (
    PGOEC_CONTROLLER Controller
    );

KSTATUS
GoecGetVersion (
    PGOEC_CONTROLLER Controller
    );

KSTATUS
GoecEnablePeripheralBoot (
    PGOEC_CONTROLLER Controller
    );

KSTATUS
GoecGetKeyboardInformation (
    PGOEC_CONTROLLER Controller
    );

KSTATUS
GoecGetKeyboardState (
    PGOEC_CONTROLLER Controller,
    UCHAR State[GOEC_MAX_COLUMNS]
    );

VOID
GoecUpdateKeyboardState (
    PGOEC_CONTROLLER Controller
    );

KSTATUS
GoecExecuteCommand (
    PGOEC_CONTROLLER Controller,
    PGOEC_COMMAND Command
    );

KSTATUS
GoecExecuteCommandV3 (
    PGOEC_CONTROLLER Controller,
    PGOEC_COMMAND Command
    );

KSTATUS
GoecCreateCommandV3 (
    PGOEC_CONTROLLER Controller,
    PGOEC_COMMAND Command,
    PGOEC_COMMAND_V3 HardwareCommand,
    PUINTN Size
    );

KSTATUS
GoecPrepareResponseBufferV3 (
    PGOEC_CONTROLLER Controller,
    PGOEC_COMMAND Command,
    PGOEC_RESPONSE_V3 Response,
    PUINTN Size
    );

KSTATUS
GoecHandleResponseV3 (
    PGOEC_RESPONSE_V3 Response,
    PGOEC_COMMAND Command
    );

INT
GoecComputeChecksum (
    PUCHAR Data,
    UINTN Size
    );

KSTATUS
GoecPerformSpiIo (
    PGOEC_CONTROLLER Controller,
    UINTN OutBytes,
    UINTN InBytes
    );

UCHAR
GoecCrc8 (
    PVOID Data,
    ULONG Size
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER GoecDriver;
UUID GoecSpbInterfaceUuid = UUID_SPB_INTERFACE;

//
// Set this debug boolean to TRUE to print the Google EC keyboard state when
// it's transferred.
//

BOOL GoecPrintKeyState = FALSE;

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the Google EC driver. It registers
    its other dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    GoecDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = GoecAddDevice;
    FunctionTable.DispatchStateChange = GoecDispatchStateChange;
    FunctionTable.DispatchOpen = GoecDispatchOpen;
    FunctionTable.DispatchClose = GoecDispatchClose;
    FunctionTable.DispatchIo = GoecDispatchIo;
    FunctionTable.DispatchSystemControl = GoecDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
GoecAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which this driver
    acts as the function driver. The driver will attach itself to the stack.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    UINTN AllocationSize;
    PGOEC_CONTROLLER Controller;
    KSTATUS Status;

    AllocationSize = sizeof(GOEC_CONTROLLER) +
                     (2 * GOEC_PROTO3_MAX_PACKET_SIZE);

    Controller = MmAllocatePagedPool(AllocationSize, GOEC_ALLOCATION_TAG);
    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, AllocationSize);
    Controller->OsDevice = DeviceToken;
    Controller->InterruptHandle = INVALID_HANDLE;
    Controller->InputHandle = INVALID_HANDLE;
    Controller->RequestBuffer = (PVOID)(Controller + 1);
    Controller->ResponseBuffer = (PUCHAR)Controller->RequestBuffer +
                                 GOEC_PROTO3_MAX_PACKET_SIZE;

    Status = MmCreateIoBuffer(Controller->RequestBuffer,
                              GOEC_PROTO3_MAX_PACKET_SIZE,
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &(Controller->RequestIoBuffer));

    if (!KSUCCESS(Status)) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    Status = MmCreateIoBuffer(Controller->ResponseBuffer,
                              GOEC_PROTO3_MAX_PACKET_SIZE,
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &(Controller->ResponseIoBuffer));

    if (!KSUCCESS(Status)) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    Controller->Lock = KeCreateQueuedLock();
    if (Controller->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    Controller->InterruptEvent = KeCreateEvent(NULL);
    if (Controller->InterruptEvent == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Controller);

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            if (Controller->RequestIoBuffer != NULL) {
                MmFreeIoBuffer(Controller->RequestIoBuffer);
            }

            if (Controller->ResponseIoBuffer != NULL) {
                MmFreeIoBuffer(Controller->ResponseIoBuffer);
            }

            if (Controller->Lock != NULL) {
                KeDestroyQueuedLock(Controller->Lock);
            }

            if (Controller->InterruptEvent != NULL) {
                KeDestroyEvent(Controller->InterruptEvent);
            }

            MmFreePagedPool(Controller);
        }
    }

    return Status;
}

VOID
GoecDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    if (Irp->Direction == IrpUp) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            Status = GoecProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(GoecDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = GoecStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(GoecDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
GoecDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Open IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    return;
}

VOID
GoecDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Close IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    return;
}

VOID
GoecDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles I/O IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    return;
}

VOID
GoecDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles System Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    //
    // Do no processing on any IRPs. Let them flow.
    //

    return;
}

INTERRUPT_STATUS
GoecInterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents the low level interrupt service routine for the
    Google Embedded Controller.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    PGOEC_CONTROLLER Controller;

    Controller = Context;
    KeSignalEvent(Controller->InterruptEvent, SignalOptionSignalAll);
    return InterruptStatusDefer;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
GoecProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for a Google EC. It adds an interrupt vector requirement for any
    interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

Return Value:

    Status code.

--*/

{

    PRESOURCE_CONFIGURATION_LIST Requirements;
    KSTATUS Status;
    RESOURCE_REQUIREMENT VectorRequirement;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    //
    // Initialize a nice interrupt vector requirement in preparation.
    //

    RtlZeroMemory(&VectorRequirement, sizeof(RESOURCE_REQUIREMENT));
    VectorRequirement.Type = ResourceTypeInterruptVector;
    VectorRequirement.Minimum = 0;
    VectorRequirement.Maximum = -1;
    VectorRequirement.Length = 1;

    //
    // Loop through all configuration lists, creating a vector for each line.
    //

    Requirements = Irp->U.QueryResources.ResourceRequirements;
    Status = IoCreateAndAddInterruptVectorsForLines(Requirements,
                                                    &VectorRequirement);

    if (!KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

ProcessResourceRequirementsEnd:
    return Status;
}

KSTATUS
GoecStartDevice (
    PIRP Irp,
    PGOEC_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the Google EC device.

Arguments:

    Irp - Supplies a pointer to the start IRP.

    Device - Supplies a pointer to the device information.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    USER_INPUT_DEVICE_DESCRIPTION InputDevice;
    PRESOURCE_ALLOCATION LineAllocation;
    KSTATUS Status;
    THREAD_CREATION_PARAMETERS Thread;

    //
    // Loop through the allocated resources to get the controller base and the
    // interrupt.
    //

    ASSERT(Device->InterruptHandle == INVALID_HANDLE);

    Device->InterruptResourcesFound = FALSE;
    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // If the resource is an interrupt vector, then it should have an
        // owning interrupt line allocation.
        //

        if (Allocation->Type == ResourceTypeInterruptVector) {
            LineAllocation = Allocation->OwningAllocation;
            if (Device->InterruptResourcesFound == FALSE) {

                ASSERT(Allocation->OwningAllocation != NULL);

                //
                // Save the line and vector number.
                //

                Device->InterruptLine = LineAllocation->Allocation;
                Device->InterruptVector = Allocation->Allocation;
                Device->InterruptResourcesFound = TRUE;

            } else {

                ASSERT((Device->InterruptLine == LineAllocation->Allocation) &&
                       (Device->InterruptVector == Allocation->Allocation));
            }

        } else if (Allocation->Type == ResourceTypeSimpleBus) {
            if (Device->SpbResource == NULL) {
                Device->SpbResource = Allocation;
            }
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    if (Device->SpbResource == NULL) {
        Status = STATUS_NOT_READY;
        goto StartDeviceEnd;
    }

    //
    // Sign up for interface notifications on the Simple Bus device to get
    // access to the simple bus interface. This should call back immediately.
    //

    if (Device->SpbSignedUp == FALSE) {
        Status = IoRegisterForInterfaceNotifications(
                                          &GoecSpbInterfaceUuid,
                                          GoecSpbInterfaceNotificationCallback,
                                          Device->SpbResource->Provider,
                                          Device,
                                          TRUE);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Device->SpbSignedUp = TRUE;
    }

    //
    // The device cannot start up if there is no bus interface to talk over.
    //

    if (Device->SpbInterface == NULL) {
        Status = STATUS_NO_INTERFACE;
        goto StartDeviceEnd;
    }

    //
    // Try to open up communications over the simple bus.
    //

    if (Device->SpbHandle == NULL) {

        ASSERT(Device->SpbResource->DataSize >= sizeof(RESOURCE_SPB_DATA));

        KeAcquireQueuedLock(Device->Lock);
        Status = Device->SpbInterface->Open(Device->SpbInterface,
                                            Device->SpbResource->Data,
                                            &(Device->SpbHandle));

        KeReleaseQueuedLock(Device->Lock);
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("GOEC: Open SPB Failed: %d\n", Status);
            goto StartDeviceEnd;
        }
    }

    //
    // Create a keyboard device.
    //

    if (Device->InputHandle == INVALID_HANDLE) {
        RtlZeroMemory(&InputDevice, sizeof(USER_INPUT_DEVICE_DESCRIPTION));
        InputDevice.Device = Irp->Device;
        InputDevice.DeviceContext = Device;
        InputDevice.Type = UserInputDeviceKeyboard;
        InputDevice.InterfaceVersion =
                                  USER_INPUT_KEYBOARD_DEVICE_INTERFACE_VERSION;

        InputDevice.U.KeyboardInterface.SetLedState = NULL;
        Device->InputHandle = InRegisterInputDevice(&InputDevice);
        if (Device->InputHandle == INVALID_HANDLE) {
            Status = STATUS_NOT_INITIALIZED;
            goto StartDeviceEnd;
        }
    }

    //
    // Connect the interrupt.
    //

    if ((Device->InterruptHandle == INVALID_HANDLE) &&
        (Device->InterruptResourcesFound != FALSE)) {

        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Device->InterruptLine;
        Connect.Vector = Device->InterruptVector;
        Connect.LowLevelServiceRoutine = GoecInterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    RtlZeroMemory(&Thread, sizeof(THREAD_CREATION_PARAMETERS));
    Thread.Name = "GoecWorker";
    Thread.NameSize = sizeof("GoecWorker");
    Thread.ThreadRoutine = GoecWorkerThread;
    Thread.Parameter = Device;
    Status = PsCreateThread(&Thread);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

StartDeviceEnd:
    return Status;
}

VOID
GoecSpbInterfaceNotificationCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    )

/*++

Routine Description:

    This routine is called to notify listeners that an interface has arrived
    or departed.

Arguments:

    Context - Supplies the caller's context pointer, supplied when the caller
        requested interface notifications.

    Device - Supplies a pointer to the device exposing or deleting the
        interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer of the
        interface.

    InterfaceBufferSize - Supplies the buffer size.

    Arrival - Supplies TRUE if a new interface is arriving, or FALSE if an
        interface is departing.

Return Value:

    None.

--*/

{

    PGOEC_CONTROLLER Controller;
    PSPB_INTERFACE Interface;

    Controller = Context;
    KeAcquireQueuedLock(Controller->Lock);

    //
    // If the interface is arriving, store a pointer to it.
    //

    if (Arrival != FALSE) {
        Interface = InterfaceBuffer;
        if (InterfaceBufferSize < sizeof(SPB_INTERFACE)) {

            ASSERT(FALSE);

            return;
        }

        ASSERT(Controller->SpbInterface == NULL);

        Controller->SpbInterface = Interface;

        ASSERT(Controller->SpbHandle == NULL);

    //
    // If the interface is disappearing, close the handle.
    //

    } else {
        Interface = Controller->SpbInterface;
        if (Controller->SpbHandle != NULL) {
            Interface->Close(Interface, Controller->SpbHandle);
            Controller->SpbHandle = NULL;
        }

        Controller->SpbInterface = NULL;
    }

    KeReleaseQueuedLock(Controller->Lock);
    return;
}

VOID
GoecWorkerThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the Google EC worker thread. This thread is needed
    because synchronous requests across busses like SPI cannot be made on the
    system work queue (because the ISRs require work items to run).

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread.

Return Value:

    None.

--*/

{

    PGOEC_CONTROLLER Controller;
    KSTATUS Status;

    Controller = Parameter;

    //
    // This should eventually happen inside the loop, with a pended IRP to
    // complete depending on the outcome.
    //

    Status = GoecInitialize(Controller);
    if (!KSUCCESS(Status)) {
        return;
    }

    while (TRUE) {
        KeWaitForEvent(Controller->InterruptEvent, FALSE, WAIT_TIME_INDEFINITE);
        KeSignalEvent(Controller->InterruptEvent, SignalOptionUnsignal);
        GoecUpdateKeyboardState(Controller);
        HlContinueInterrupt(Controller->InterruptHandle,
                            InterruptStatusClaimed);
    }

    return;
}

KSTATUS
GoecInitialize (
    PGOEC_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes communications with the Google EC device.

Arguments:

    Controller - Supplies a pointer to the connected controller.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = GoecSayHello(Controller);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("GOEC: Hello returned %d\n", Status);
        goto InitializeEnd;
    }

    Status = GoecGetVersion(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = GoecEnablePeripheralBoot(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    GoecGetKeyboardInformation(Controller);

InitializeEnd:
    return Status;
}

KSTATUS
GoecSayHello (
    PGOEC_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes communications with the Google EC device.

Arguments:

    Controller - Supplies a pointer to the connected controller.

Return Value:

    Status code.

--*/

{

    GOEC_COMMAND Command;
    GOEC_PARAMS_HELLO CommandHello;
    GOEC_RESPONSE_HELLO Response;
    KSTATUS Status;

    CommandHello.InData = 0x10203040;
    Command.Code = GoecCommandHello;
    Command.Version = 0;
    Command.DataIn = &CommandHello;
    Command.DataOut = &Response;
    Command.SizeIn = sizeof(GOEC_PARAMS_HELLO);
    Command.SizeOut = sizeof(GOEC_RESPONSE_HELLO);
    Command.DeviceIndex = 0;
    Status = GoecExecuteCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("GOEC: Failed to execute hello command: %d\n", Status);
        return Status;
    }

    if (Response.OutData != CommandHello.InData + 0x01020304) {
        RtlDebugPrint("GOEC: Embedded controller responsed to hello with "
                      "0x%x!\n",
                      Response.OutData);

        Status = STATUS_NOT_READY;
    }

    KeDelayExecution(FALSE, FALSE, 10000);
    return Status;
}

KSTATUS
GoecGetVersion (
    PGOEC_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine gets the version strings out of the EC.

Arguments:

    Controller - Supplies a pointer to the connected controller.

Return Value:

    Status code.

--*/

{

    GOEC_COMMAND Command;
    KSTATUS Status;
    GOEC_RESPONSE_GET_VERSION Version;

    Command.Code = GoecCommandGetVersion;
    Command.Version = 0;
    Command.DataIn = NULL;
    Command.DataOut = &Version;
    Command.SizeIn = 0;
    Command.SizeOut = sizeof(GOEC_RESPONSE_GET_VERSION);
    Command.DeviceIndex = 0;
    Status = GoecExecuteCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("GOEC: Failed to get version: %d\n", Status);
        return Status;
    }

    KeDelayExecution(FALSE, FALSE, 10000);
    Version.VersionStringRo[sizeof(Version.VersionStringRo) - 1] = '\0';
    Version.VersionStringRw[sizeof(Version.VersionStringRw) - 1] = '\0';
    RtlDebugPrint("Google Chrome EC version:\n"
                  "    RO: %s\n    RW: %s\n    Current: %d\n",
                  Version.VersionStringRo,
                  Version.VersionStringRw,
                  Version.CurrentImage);

    return Status;
}

KSTATUS
GoecEnablePeripheralBoot (
    PGOEC_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine ensures that booting from USB and SD is enabled.

Arguments:

    Controller - Supplies a pointer to the connected controller.

Return Value:

    Status code.

--*/

{

    GOEC_COMMAND Command;
    GOEC_PARAMS_VBNV_CONTEXT CommandData;
    GOEC_RESPONSE_VBNV_CONTEXT Response;
    KSTATUS Status;

    RtlZeroMemory(&CommandData, sizeof(CommandData));
    CommandData.Operation = GOEC_VBNV_CONTEXT_OP_READ;
    Command.Code = GoecCommandVbNvContext;
    Command.Version = GOEC_VBNV_CONTEXT_VERSION;
    Command.DataIn = &CommandData;
    Command.DataOut = &Response;
    Command.SizeIn = sizeof(GOEC_PARAMS_VBNV_CONTEXT);
    Command.SizeOut = sizeof(GOEC_RESPONSE_VBNV_CONTEXT);
    Command.DeviceIndex = 0;
    Status = GoecExecuteCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("GOEC: Failed to read NVRAM: %d\n", Status);
        return Status;
    }

    KeDelayExecution(FALSE, FALSE, 10000);
    if (((Response.NvRam.Header & GOEC_NVRAM_HEADER_SIGNATURE_MASK) !=
         GOEC_NVRAM_HEADER_SIGNATURE_VALUE) ||
        (GoecCrc8(&(Response.NvRam), sizeof(GOEC_NVRAM) - 1) !=
         Response.NvRam.Crc8)) {

        RtlDebugPrint("GOEC: Invalid NVRAM!\n");
        Status = STATUS_CHECKSUM_MISMATCH;
        return Status;
    }

    //
    // Make sure the dev boot USB bit is set.
    //

    if ((Response.NvRam.DevFlags & GOEC_NVRAM_DEV_BOOT_USB) != 0) {
        return STATUS_SUCCESS;
    }

    RtlDebugPrint("GOEC: Enabling USB/SD boot.\n");
    RtlCopyMemory(&(CommandData.NvRam), &(Response.NvRam), sizeof(GOEC_NVRAM));
    CommandData.NvRam.DevFlags |= GOEC_NVRAM_DEV_BOOT_USB;
    CommandData.NvRam.Crc8 = GoecCrc8(&(CommandData.NvRam),
                                      sizeof(GOEC_NVRAM) - 1);

    CommandData.Operation = GOEC_VBNV_CONTEXT_OP_WRITE;
    Command.Code = GoecCommandVbNvContext;
    Command.Version = GOEC_VBNV_CONTEXT_VERSION;
    Command.DataIn = &CommandData;
    Command.DataOut = &Response;
    Command.SizeIn = sizeof(GOEC_PARAMS_VBNV_CONTEXT);
    Command.SizeOut = 0;
    Command.DeviceIndex = 0;
    Status = GoecExecuteCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("GOEC: Failed to set NVRAM.\n", Status);
    }

    KeDelayExecution(FALSE, FALSE, 10000);
    return Status;
}

KSTATUS
GoecGetKeyboardInformation (
    PGOEC_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine gets the number of rows and columns in the keyboard.

Arguments:

    Controller - Supplies a pointer to the connected controller.

Return Value:

    Status code.

--*/

{

    GOEC_COMMAND Command;
    GOEC_RESPONSE_KEYBOARD_INFO KeyboardInfo;
    KSTATUS Status;

    Command.Code = GoecCommandKeyboardInfo;
    Command.Version = 0;
    Command.DataIn = NULL;
    Command.DataOut = &KeyboardInfo;
    Command.SizeIn = 0;
    Command.SizeOut = sizeof(GOEC_RESPONSE_KEYBOARD_INFO);
    Command.DeviceIndex = 0;
    Status = GoecExecuteCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    KeDelayExecution(FALSE, FALSE, 10000);
    Controller->KeyColumns = KeyboardInfo.Columns;
    if (Controller->KeyColumns > GOEC_MAX_COLUMNS) {
        Controller->KeyColumns = GOEC_MAX_COLUMNS;
    }

    Controller->KeyRows = KeyboardInfo.Rows;
    if (Controller->KeyRows > BITS_PER_BYTE) {
        Controller->KeyRows = BITS_PER_BYTE;
    }

    return Status;
}

KSTATUS
GoecGetKeyboardState (
    PGOEC_CONTROLLER Controller,
    UCHAR State[GOEC_MAX_COLUMNS]
    )

/*++

Routine Description:

    This routine gets the current state of the matrix keyboard.

Arguments:

    Controller - Supplies a pointer to the connected controller.

    State - Supplies a pointer where the keyboard state will be returned on
        success.

Return Value:

    Status code.

--*/

{

    GOEC_COMMAND Command;
    UINTN Index;
    KSTATUS Status;

    Command.Code = GoecCommandKeyboardState;
    Command.Version = 0;
    Command.DataIn = NULL;
    Command.DataOut = State;
    Command.SizeIn = 0;
    Command.SizeOut = Controller->KeyColumns;

    ASSERT(Controller->KeyColumns <= GOEC_MAX_COLUMNS);

    Command.DeviceIndex = 0;
    Status = GoecExecuteCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (GoecPrintKeyState != FALSE) {
        RtlDebugPrint("KeyState: ");
        for (Index = 0; Index < Controller->KeyColumns; Index += 1) {
            RtlDebugPrint("%02x ", State[Index]);
        }

        RtlDebugPrint("\n");
    }

    return Status;
}

VOID
GoecUpdateKeyboardState (
    PGOEC_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine updates the current keyboard state coming from the Google
    Embedded Controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

{

    ULONG Column;
    UCHAR Current;
    UCHAR Delta;
    USER_INPUT_EVENT Event;
    UCHAR NewKeys[GOEC_MAX_COLUMNS];
    UCHAR Previous;
    ULONG Row;
    KSTATUS Status;

    Status = GoecGetKeyboardState(Controller, NewKeys);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("GOEC: Failed to get keyboard state: %d\n", Status);
        return;
    }

    ASSERT(Controller->KeyRows <= BITS_PER_BYTE);

    if (Controller->InputHandle == INVALID_HANDLE) {
        return;
    }

    RtlZeroMemory(&Event, sizeof(USER_INPUT_EVENT));
    Event.DeviceType = UserInputDeviceKeyboard;
    for (Column = 0; Column < Controller->KeyColumns; Column += 1) {
        Current = NewKeys[Column];
        Previous = Controller->KeyState[Column];
        Controller->KeyState[Column] = Current;
        Delta = Previous ^ Current;
        if (Delta == 0) {
            continue;
        }

        for (Row = 0; Row < Controller->KeyRows; Row += 1) {
            if ((Delta & (1 << Row)) != 0) {
                if ((Current & (1 << Row)) != 0) {
                    Event.EventType = UserInputEventKeyDown;

                } else {
                    Event.EventType = UserInputEventKeyUp;
                }

                Event.U.Key = GoecKeyMap[Column][Row];
                if (Event.U.Key == KeyboardKeyInvalid) {
                    RtlDebugPrint("GOEC: Invalid key at col/row %d, %d\n",
                                  Column,
                                  Row);

                } else {
                    InReportInputEvent(Controller->InputHandle, &Event);
                }
            }
        }
    }

    return;
}

KSTATUS
GoecExecuteCommand (
    PGOEC_CONTROLLER Controller,
    PGOEC_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a Google EC command.

Arguments:

    Controller - Supplies a pointer to the controller.

    Command - Supplies a pointer to the command.

Return Value:

    Status code.

--*/

{

    PSPB_INTERFACE Interface;
    KSTATUS Status;

    KeAcquireQueuedLock(Controller->Lock);
    Interface = Controller->SpbInterface;
    if ((Interface == NULL) || (Controller->SpbHandle == NULL)) {
        Status = STATUS_NO_INTERFACE;
        goto ExecuteCommandEnd;
    }

    Status = GoecExecuteCommandV3(Controller, Command);

ExecuteCommandEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return Status;
}

KSTATUS
GoecExecuteCommandV3 (
    PGOEC_CONTROLLER Controller,
    PGOEC_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a Google EC v3 command. This routine assumes the
    controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    Command - Supplies a pointer to the command.

Return Value:

    Status code.

--*/

{

    UINTN InBytes;
    UINTN OutBytes;
    KSTATUS Status;

    Status = GoecCreateCommandV3(Controller,
                                 Command,
                                 Controller->RequestBuffer,
                                 &OutBytes);

    if (!KSUCCESS(Status)) {
        goto ExecuteCommandV3End;
    }

    Status = GoecPrepareResponseBufferV3(Controller,
                                         Command,
                                         Controller->ResponseBuffer,
                                         &InBytes);

    if (!KSUCCESS(Status)) {
        goto ExecuteCommandV3End;
    }

    Status = GoecPerformSpiIo(Controller, OutBytes, InBytes);
    if (!KSUCCESS(Status)) {
        goto ExecuteCommandV3End;
    }

    Status = GoecHandleResponseV3(Controller->ResponseBuffer, Command);

ExecuteCommandV3End:
    return Status;
}

KSTATUS
GoecCreateCommandV3 (
    PGOEC_CONTROLLER Controller,
    PGOEC_COMMAND Command,
    PGOEC_COMMAND_V3 HardwareCommand,
    PUINTN Size
    )

/*++

Routine Description:

    This routine creates a Google EC v3 command.

Arguments:

    Controller - Supplies a pointer to the controller.

    Command - Supplies a pointer to the command.

    HardwareCommand - Supplies a pointer where the hardware command will be
        returned on success.

    Size - Supplies a pointer where the size of the hardware command in bytes
        will be returned.

Return Value:

    Status code.

--*/

{

    PGOEC_COMMAND_HEADER Header;
    UINTN OutBytes;

    Header = &(HardwareCommand->Header);
    OutBytes = Command->SizeIn + sizeof(GOEC_COMMAND_HEADER);
    if (OutBytes >= sizeof(GOEC_COMMAND_V3)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    Header->Version = GOEC_COMMAND_HEADER_VERSION;
    Header->Checksum = 0;
    Header->Command = Command->Code;
    Header->CommandVersion = Command->Version;
    Header->Reserved = 0;
    Header->DataLength = Command->SizeIn;
    RtlCopyMemory(HardwareCommand->Data, Command->DataIn, Command->SizeIn);
    Header->Checksum = -GoecComputeChecksum((PUCHAR)HardwareCommand, OutBytes);
    *Size = OutBytes;
    return STATUS_SUCCESS;
}

KSTATUS
GoecPrepareResponseBufferV3 (
    PGOEC_CONTROLLER Controller,
    PGOEC_COMMAND Command,
    PGOEC_RESPONSE_V3 Response,
    PUINTN Size
    )

/*++

Routine Description:

    This routine prepares a Google EC v3 response buffer for reception.

Arguments:

    Controller - Supplies a pointer to the controller.

    Command - Supplies a pointer to the command, in software structure form.

    Response - Supplies a pointer to the hardware response buffer to prepare.

    Size - Supplies a pointer where the size of the hardware response in bytes
        will be returned.

Return Value:

    Status code.

--*/

{

    UINTN InBytes;

    InBytes = Command->SizeOut + sizeof(GOEC_RESPONSE_HEADER);
    if (InBytes > sizeof(GOEC_RESPONSE_V3)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    *Size = InBytes;
    return STATUS_SUCCESS;
}

KSTATUS
GoecHandleResponseV3 (
    PGOEC_RESPONSE_V3 Response,
    PGOEC_COMMAND Command
    )

/*++

Routine Description:

    This routine performs generic validation on an EC response.

Arguments:

    Response - Supplies a pointer to the response from the hardware.

    Command - Supplies a pointer where the response information will be
        returned on success.

Return Value:

    Status code.

--*/

{

    INT Checksum;
    PGOEC_RESPONSE_HEADER Header;
    UINTN InBytes;

    Header = &(Response->Header);
    if (Header->Version != GOEC_RESPONSE_HEADER_VERSION) {
        RtlDebugPrint("GOEC: Version mismatch! Got %x, wanted %x.\n",
                      Header->Version,
                      GOEC_COMMAND_HEADER_VERSION);

        return STATUS_DEVICE_IO_ERROR;
    }

    if (Header->Reserved != 0) {
        return STATUS_DEVICE_IO_ERROR;
    }

    if ((Header->DataLength > GOEC_MAX_DATA) ||
        (Header->DataLength > Command->SizeOut)) {

        return STATUS_BUFFER_TOO_SMALL;
    }

    InBytes = sizeof(GOEC_RESPONSE_HEADER) + Header->DataLength;
    Checksum = GoecComputeChecksum((PUCHAR)Response, InBytes);
    if (Checksum != 0) {
        RtlDebugPrint("GOEC: Bad Checksum 0x%x\n", Checksum);
        return STATUS_CHECKSUM_MISMATCH;
    }

    Command->Code = Header->Result;
    Command->SizeOut = Header->DataLength;
    RtlCopyMemory(Command->DataOut, Response->Data, Header->DataLength);
    if (Header->Result != 0) {
        RtlDebugPrint("GOEC: Error response 0x%x\n", Header->Result);
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

INT
GoecComputeChecksum (
    PUCHAR Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine computes the sum of all the bytes in the given buffer.

Arguments:

    Data - Supplies a pointer to the data to sum.

    Size - Supplies the number of bytes to sum.

Return Value:

    Returns the lower 8 bits of the sum of all the bytes in the buffer.

--*/

{

    UINTN Index;
    INT Sum;

    Sum = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Sum += Data[Index];
    }

    return Sum & 0xFF;
}

KSTATUS
GoecPerformSpiIo (
    PGOEC_CONTROLLER Controller,
    UINTN OutBytes,
    UINTN InBytes
    )

/*++

Routine Description:

    This routine performs SPI I/O for the given controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    OutBytes - Supplies the number of bytes to transmit from the command buffer.

    InBytes - Supplies the number of bytes to receive from the response buffer.

Return Value:

    Status code.

--*/

{

    PUCHAR Byte;
    SPB_HANDLE Handle;
    PSPB_INTERFACE Interface;
    KSTATUS Status;
    ULONGLONG Timeout;
    SPB_TRANSFER Transfer;
    SPB_TRANSFER_SET TransferSet;

    Handle = Controller->SpbHandle;
    Interface = Controller->SpbInterface;
    if ((Handle == NULL) || (Interface == NULL)) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Lock the bus so that other transfers don't come in.
    //

    RtlZeroMemory(&Transfer, sizeof(SPB_TRANSFER));
    RtlZeroMemory(&TransferSet, sizeof(SPB_TRANSFER_SET));
    Interface->LockBus(Handle);
    HlBusySpin(GOEC_COMMAND_MICROSECOND_DELAY);
    Transfer.Direction = SpbTransferDirectionOut;
    Transfer.IoBuffer = Controller->RequestIoBuffer;
    Transfer.Size = OutBytes;
    Transfer.MicrosecondDelay = 0;
    TransferSet.Handle = Handle;
    INITIALIZE_LIST_HEAD(&(TransferSet.TransferList));
    INSERT_BEFORE(&(Transfer.ListEntry), &(TransferSet.TransferList));
    Status = Interface->ExecuteTransferSet(Handle, &TransferSet);
    if (!KSUCCESS(Status)) {
        goto PerformSpiIoEnd;
    }

    ASSERT((TransferSet.EntriesProcessed == 1) &&
           (Transfer.TransmitSizeCompleted == OutBytes));

    //
    // Now read a single byte at a time until it's the start of frame byte.
    // Other bytes are the EC stalling for time.
    //

    Transfer.Direction = SpbTransferDirectionIn;
    Transfer.IoBuffer = Controller->ResponseIoBuffer;
    Transfer.Size = 1;
    Transfer.MicrosecondDelay = 0;
    Timeout = HlQueryTimeCounter() +
              (GOEC_RESPONSE_TIMEOUT * HlQueryTimeCounterFrequency());

    Byte = Controller->ResponseBuffer;
    *Byte = 0;
    while (TRUE) {
        Status = Interface->ExecuteTransferSet(Handle, &TransferSet);
        if (!KSUCCESS(Status)) {
            goto PerformSpiIoEnd;
        }

        if (*Byte == GoecSpiFrameStart) {
            break;

        } else if ((*Byte != GoecSpiProcessing) &&
                   (*Byte != GoecSpiReceiving)) {

            RtlDebugPrint("GOEC: Got bad status 0x%x\n", *Byte);
            Status = STATUS_DEVICE_IO_ERROR;
            goto PerformSpiIoEnd;
        }

        if (KeGetRecentTimeCounter() > Timeout) {
            Status = STATUS_TIMEOUT;
            goto PerformSpiIoEnd;
        }
    }

    Transfer.Size = InBytes;
    Status = Interface->ExecuteTransferSet(Handle, &TransferSet);
    if (!KSUCCESS(Status)) {
        goto PerformSpiIoEnd;
    }

    ASSERT((TransferSet.EntriesProcessed == 1) &&
           (Transfer.ReceiveSizeCompleted == InBytes));

PerformSpiIoEnd:
    Interface->UnlockBus(Handle);
    return Status;
}

UCHAR
GoecCrc8 (
    PVOID Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine computes the CRC-8 of the given data using the polynomial
    x^8 + x^2 + x + 1.

Arguments:

    Data - Supplies a pointer to the data to compute the CRC for.

    Size - Supplies the number of bytes.

Return Value:

    Returns the CRC-8 of the data.

--*/

{

    PUCHAR Bytes;
    ULONG Crc;
    ULONG Index;
    ULONG InnerIndex;

    Bytes = Data;
    Crc = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Crc ^= (*Bytes << 8);
        for (InnerIndex = 0; InnerIndex < 8; InnerIndex += 1) {
            if ((Crc & 0x8000) != 0) {
                Crc ^= 0x1070 << 3;
            }

            Crc <<= 1;
        }

        Bytes += 1;
    }

    return (UCHAR)(Crc >> 8);
}

