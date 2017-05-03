/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elani2c.c

Abstract:

    This module implements support for the Elan i2C touchpad device.

Author:

    Evan Green 28-Apr-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usrinput/usrinput.h>
#include <minoca/spb/spb.h>
#include "elani2c.h"

//
// --------------------------------------------------------------------- Macros
//

#define ElanI2cSetMode(_Controller, _Mode) \
    ElanI2cWriteCommand((_Controller), ElanI2cCommandSetMode, (_Mode))

//
// ---------------------------------------------------------------- Definitions
//

#define ELAN_I2C_ALLOCATION_TAG 0x49616C45

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a finger position.

Members:

    X - Stores the X position.

    Y - Stores the Y position.

--*/

typedef struct _ELAN_I2C_POSITION {
    ULONG X;
    ULONG Y;
} ELAN_I2C_POSITION, *PELAN_I2C_POSITION;

/*++

Structure Description:

    This structure defines the context for an Elan i2C touchpad device.

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

    RequestBuffer - Stores a pointer to a buffer used for request data.

    RequestIoBuffer - Stores a pointer to the I/O buffer around the request
        buffer.

    InterruptEvent - Stores a pointer to the event used to indicate to the
        worker thread that an interrupt has fired.

    Irp - Stores a pointer to the state change IRP to work on.

    InputHandle - Stores the user input device handle, used to report keyboard
        events to the system.

    ProductId - Stores the product identifier.

    FirmwareVersion - Stores the device's firmware version.

    FirmwareChecksum - Stores the device's firmware checksum.

    SampleVersion - Stores the device's SM version.

    IapVersion - Stores the device's IAP version.

    PressureAdjustment - Stores the amount to adjust the pressure by.

    MaxX - Stores the maximum X axis value in absolute coordinates.

    MaxY - Stores the maximum Y axis value in absolute coordinates.

    TraceCountX - Stores the number of supported traces in the X direction.

    TraceCountY - Stores the number of supported traces in the Y direction.

    ResolutionX - Stores the horizontal resolution.

    ResolutionY - Stores the vertical resolution.

    LastPosition - Stores the previous position for each of the fingers, or
        zero if the finger is not down.

--*/

typedef struct _ELAN_I2C_CONTROLLER {
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
    PIO_BUFFER RequestIoBuffer;
    PKEVENT InterruptEvent;
    HANDLE InputHandle;
    USHORT ProductId;
    USHORT FirmwareVersion;
    USHORT FirmwareChecksum;
    USHORT SampleVersion;
    USHORT IapVersion;
    LONG PressureAdjustment;
    ULONG MaxX;
    ULONG MaxY;
    UCHAR TraceCountX;
    UCHAR TraceCountY;
    UCHAR ResolutionX;
    UCHAR ResolutionY;
    ELAN_I2C_POSITION LastPosition[ELAN_I2C_FINGER_COUNT];
} ELAN_I2C_CONTROLLER, *PELAN_I2C_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
ElanI2cAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
ElanI2cDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
ElanI2cDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
ElanI2cDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
ElanI2cDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
ElanI2cDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
ElanI2cInterruptServiceWorker (
    PVOID Context
    );

KSTATUS
ElanI2cProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
ElanI2cStartDevice (
    PIRP Irp,
    PELAN_I2C_CONTROLLER Device
    );

VOID
ElanI2cSpbInterfaceNotificationCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

VOID
ElanI2cWorkerThread (
    PVOID Parameter
    );

KSTATUS
ElanI2cInitialize (
    PELAN_I2C_CONTROLLER Controller
    );

KSTATUS
ElanI2cInitializeController (
    PELAN_I2C_CONTROLLER Controller
    );

KSTATUS
ElanI2cReadAndProcessReport (
    PELAN_I2C_CONTROLLER Controller
    );

KSTATUS
ElanI2cReadDeviceInformation (
    PELAN_I2C_CONTROLLER Controller
    );

KSTATUS
ElanI2cGetReport (
    PELAN_I2C_CONTROLLER Controller,
    UCHAR Report[ELAN_I2C_REPORT_SIZE]
    );

KSTATUS
ElanI2cSleepControl (
    PELAN_I2C_CONTROLLER Controller,
    BOOL Wake
    );

KSTATUS
ElanI2cReadCommand (
    PELAN_I2C_CONTROLLER Controller,
    ELAN_I2C_COMMAND Register,
    PUSHORT Value
    );

KSTATUS
ElanI2cWriteCommand (
    PELAN_I2C_CONTROLLER Controller,
    ELAN_I2C_COMMAND Register,
    USHORT Value
    );

KSTATUS
ElanI2cAccessRegister (
    PELAN_I2C_CONTROLLER Controller,
    ELAN_I2C_COMMAND Register,
    PUCHAR Data,
    ULONG Length,
    BOOL Write
    );

KSTATUS
ElanI2cRawReceive (
    PELAN_I2C_CONTROLLER Controller,
    PUCHAR Data,
    ULONG Length
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER ElanI2cDriver;
UUID ElanI2cSpbInterfaceUuid = UUID_SPB_INTERFACE;

//
// Set this debug boolean to TRUE to print the touchpad events and other
// debugging information.
//

BOOL ElanI2cPrintEvents = FALSE;

//
// Set this debug boolean to TRUE to print the raw report bytes.
//

BOOL ElanI2cPrintReports = FALSE;

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

    This routine is the entry point for the Elan I2C driver. It registers
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

    ElanI2cDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = ElanI2cAddDevice;
    FunctionTable.DispatchStateChange = ElanI2cDispatchStateChange;
    FunctionTable.DispatchOpen = ElanI2cDispatchOpen;
    FunctionTable.DispatchClose = ElanI2cDispatchClose;
    FunctionTable.DispatchIo = ElanI2cDispatchIo;
    FunctionTable.DispatchSystemControl = ElanI2cDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
ElanI2cAddDevice (
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
    PELAN_I2C_CONTROLLER Controller;
    KSTATUS Status;

    AllocationSize = sizeof(ELAN_I2C_CONTROLLER) + ELAN_I2C_MAX_PACKET_SIZE;
    Controller = MmAllocatePagedPool(AllocationSize, ELAN_I2C_ALLOCATION_TAG);
    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, AllocationSize);
    Controller->OsDevice = DeviceToken;
    Controller->InterruptHandle = INVALID_HANDLE;
    Controller->InputHandle = INVALID_HANDLE;
    Controller->RequestBuffer = (PVOID)(Controller + 1);
    Status = MmCreateIoBuffer(Controller->RequestBuffer,
                              ELAN_I2C_MAX_PACKET_SIZE,
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &(Controller->RequestIoBuffer));

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
ElanI2cDispatchStateChange (
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
            Status = ElanI2cProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(ElanI2cDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = ElanI2cStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(ElanI2cDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
ElanI2cDispatchOpen (
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
ElanI2cDispatchClose (
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
ElanI2cDispatchIo (
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
ElanI2cDispatchSystemControl (
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
ElanI2cInterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents the low level interrupt service routine for the
    Elan touchpad Controller.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    PELAN_I2C_CONTROLLER Controller;

    Controller = Context;
    KeSignalEvent(Controller->InterruptEvent, SignalOptionSignalAll);
    return InterruptStatusDefer;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
ElanI2cProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an Elan touchpad. It adds an interrupt vector requirement for any
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
ElanI2cStartDevice (
    PIRP Irp,
    PELAN_I2C_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the Elan touchpad device.

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
                                       &ElanI2cSpbInterfaceUuid,
                                       ElanI2cSpbInterfaceNotificationCallback,
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
            RtlDebugPrint("ELAN_I2C: Open SPB Failed: %d\n", Status);
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
        InputDevice.Type = UserInputDeviceMouse;
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
        Connect.LowLevelServiceRoutine = ElanI2cInterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    RtlZeroMemory(&Thread, sizeof(THREAD_CREATION_PARAMETERS));
    Thread.Name = "ElanI2cWorker";
    Thread.NameSize = sizeof("ElanI2cWorker");
    Thread.ThreadRoutine = ElanI2cWorkerThread;
    Thread.Parameter = Device;
    Status = PsCreateThread(&Thread);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

StartDeviceEnd:
    return Status;
}

VOID
ElanI2cSpbInterfaceNotificationCallback (
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

    PELAN_I2C_CONTROLLER Controller;
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
ElanI2cWorkerThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the Elan touchpad worker thread. This thread is
    needed because synchronous requests across busses like SPI cannot be made
    on the system work queue (because the ISRs require work items to run).

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread.

Return Value:

    None.

--*/

{

    PELAN_I2C_CONTROLLER Controller;
    KSTATUS Status;

    Controller = Parameter;

    //
    // This should eventually happen inside the loop, with a pended IRP to
    // complete depending on the outcome.
    //

    Status = ElanI2cInitialize(Controller);
    if (!KSUCCESS(Status)) {
        return;
    }

    //
    // Expect one spurious interrupt.
    //

    KeWaitForEvent(Controller->InterruptEvent, FALSE, WAIT_TIME_INDEFINITE);
    KeSignalEvent(Controller->InterruptEvent, SignalOptionUnsignal);
    HlContinueInterrupt(Controller->InterruptHandle, InterruptStatusClaimed);
    while (TRUE) {
        KeWaitForEvent(Controller->InterruptEvent, FALSE, WAIT_TIME_INDEFINITE);
        KeSignalEvent(Controller->InterruptEvent, SignalOptionUnsignal);
        ElanI2cReadAndProcessReport(Controller);
        HlContinueInterrupt(Controller->InterruptHandle,
                            InterruptStatusClaimed);
    }

    return;
}

KSTATUS
ElanI2cInitialize (
    PELAN_I2C_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes the Elan i2C device.

Arguments:

    Controller - Supplies a pointer to the connected controller.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = ElanI2cInitializeController(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = ElanI2cSetMode(Controller, ELAN_I2C_ENABLE_ABSOLUTE);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = ElanI2cSleepControl(Controller, TRUE);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = ElanI2cReadDeviceInformation(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    if (ElanI2cPrintEvents != FALSE) {
        RtlDebugPrint("Elan I2C Touchpad:\n"
                      "  Product ID: %04x\n"
                      "  Firmware Version: %04x\n"
                      "  Sample Version: %04x\n"
                      "  IAP Version: %04x\n"
                      "  Max X/Y: %d,%d\n"
                      "  Trace X/Y: %d,%d\n"
                      "  Resolution X/Y: %d,%d\n",
                      Controller->ProductId,
                      Controller->FirmwareVersion,
                      Controller->SampleVersion,
                      Controller->IapVersion,
                      Controller->MaxX,
                      Controller->MaxY,
                      Controller->TraceCountX,
                      Controller->TraceCountY,
                      Controller->ResolutionX,
                      Controller->ResolutionY);
    }

InitializeEnd:
    return Status;
}

KSTATUS
ElanI2cInitializeController (
    PELAN_I2C_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes communications with the Elan I2C device.

Arguments:

    Controller - Supplies a pointer to the connected controller.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    UCHAR Value[ELAN_I2C_MAX_PACKET_SIZE];

    Status = ElanI2cWriteCommand(Controller,
                                 ElanI2cCommandStandby,
                                 ElanI2cCommandReset);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ELAN_I2C: Failed to send reset: %d\n", Status);
        goto InitializeControllerEnd;
    }

    KeDelayExecution(FALSE, FALSE, 100000);

    //
    // Receive the acknowledgement bytes.
    //

    Status = ElanI2cRawReceive(Controller, Value, ELAN_I2C_INFO_LENGTH);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ELAN_I2C: Failed to get reset acknowledgment: %d\n",
                      Status);

        goto InitializeControllerEnd;
    }

    Status = ElanI2cAccessRegister(Controller,
                                   ElanI2cCommandDeviceDescriptor,
                                   Value,
                                   ELAN_I2C_DEVICE_DESCRIPTOR_LENGTH,
                                   FALSE);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ELAN_I2C: Failed to get device descriptor: %d\n",
                      Status);

        goto InitializeControllerEnd;
    }

    Status = ElanI2cAccessRegister(Controller,
                                   ElanI2cCommandReportDescriptor,
                                   Value,
                                   ELAN_I2C_REPORT_DESCRIPTOR_LENGTH,
                                   FALSE);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ELAN_I2C: Failed to get report descriptor: %d\n",
                      Status);

        goto InitializeControllerEnd;
    }

InitializeControllerEnd:
    return Status;
}

KSTATUS
ElanI2cReadAndProcessReport (
    PELAN_I2C_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine reads a report from the device and handles it.

Arguments:

    Controller - Supplies a pointer to the connected controller.

Return Value:

    Status code.

--*/

{

    USER_INPUT_EVENT Event;
    PUCHAR Finger;
    ULONG Index;
    LONG LastX;
    LONG LastY;
    LONG PositionX;
    LONG PositionY;
    UCHAR Report[ELAN_I2C_REPORT_SIZE];
    KSTATUS Status;
    UCHAR Touches;

    Status = ElanI2cGetReport(Controller, Report);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (Report[ELAN_I2C_REPORT_ID_OFFSET] != ELAN_I2C_REPORT_ID) {
        RtlDebugPrint("ELAN_I2C: Unexpected report %x\n",
                      Report[ELAN_I2C_REPORT_ID_OFFSET]);

        return STATUS_UNEXPECTED_TYPE;
    }

    if (ElanI2cPrintReports != FALSE) {
        RtlDebugPrint("ElanI2c Report: ");
        for (Index = 0; Index < ELAN_I2C_REPORT_SIZE; Index += 1) {
            RtlDebugPrint("%02x ", Report[Index]);
        }

        RtlDebugPrint("\n");
    }

    RtlZeroMemory(&Event, sizeof(USER_INPUT_EVENT));
    Event.EventType = UserInputEventMouse;
    Touches = Report[ELAN_I2C_REPORT_TOUCH_OFFSET];
    if ((Touches & ELAN_I2C_REPORT_TOUCH_LEFT_BUTTON) != 0) {

        //
        // If there are two fingers during the click, treat it as a right
        // click.
        //

        if ((Touches & (ELAN_I2C_REPORT_TOUCH_FINGER << 1)) != 0) {
            if ((Touches & (ELAN_I2C_REPORT_TOUCH_FINGER << 2)) != 0) {
                Event.U.Mouse.Buttons = MOUSE_BUTTON_MIDDLE;

            } else {
                Event.U.Mouse.Buttons = MOUSE_BUTTON_RIGHT;
            }

        } else {
            Event.U.Mouse.Buttons = MOUSE_BUTTON_LEFT;
        }
    }

    Finger = &(Report[ELAN_I2C_REPORT_FINGER_DATA_OFFSET]);
    for (Index = 0; Index < ELAN_I2C_FINGER_COUNT; Index += 1) {
        if ((Touches & (ELAN_I2C_REPORT_TOUCH_FINGER << Index)) == 0) {
            PositionX = 0;
            PositionY = 0;

        } else {
            PositionX = ((Finger[ELAN_I2C_FINGER_XY_HIGH_OFFSET] & 0xF0) << 4) |
                        Finger[ELAN_I2C_FINGER_X_OFFSET];

            PositionY = ((Finger[ELAN_I2C_FINGER_XY_HIGH_OFFSET] & 0x0F) << 8) |
                        Finger[ELAN_I2C_FINGER_Y_OFFSET];

            LastX = Controller->LastPosition[Index].X;
            LastY = Controller->LastPosition[Index].Y;
            if (((LastX | LastY) == 0) || ((PositionX | PositionY) == 0)) {
                if (ElanI2cPrintEvents != FALSE) {
                    RtlDebugPrint("Skipping finger %d (%d, %d) -> (%d, %d)\n",
                                  Index,
                                  LastX,
                                  LastY,
                                  PositionX,
                                  PositionY);
                }

            } else {
                Event.U.Mouse.MovementX += PositionX - LastX;

                //
                // Positive Y is up instead of down, so negate that here by
                // swapping the order.
                //

                Event.U.Mouse.MovementY += LastY - PositionY;
                if (ElanI2cPrintEvents != FALSE) {
                    RtlDebugPrint("Finger %d (%d, %d) -> (%d, %d)\n",
                                  Index,
                                  LastX,
                                  LastY,
                                  PositionX,
                                  PositionY);
                }

            }
        }

        Controller->LastPosition[Index].X = PositionX;
        Controller->LastPosition[Index].Y = PositionY;
        Finger += ELAN_I2C_REPORT_FINGER_DATA_LENGTH;
    }

    if (ElanI2cPrintEvents != FALSE) {
        RtlDebugPrint("Event: (%d, %d) [%x]\n",
                      Event.U.Mouse.MovementX,
                      Event.U.Mouse.MovementY,
                      Event.U.Mouse.Buttons);
    }

    Status = InReportInputEvent(Controller->InputHandle, &Event);
    return Status;
}

KSTATUS
ElanI2cReadDeviceInformation (
    PELAN_I2C_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine reads the device information registers and saves them in the
    controller.

Arguments:

    Controller - Supplies a pointer to the connected controller.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    USHORT Value;

    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandUniqueId,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    Controller->ProductId = Value;
    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandFirmwareVersion,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    Controller->FirmwareVersion = Value;
    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandFirmwareChecksum,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    Controller->FirmwareChecksum = Value;
    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandSampleVersion,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    Controller->SampleVersion = Value;
    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandIapVersion,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    Controller->IapVersion = Value;
    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandPressureFormat,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    if ((Value & ELAN_I2C_PRESSURE_ADJUSTED) != 0) {
        Controller->PressureAdjustment = 0;

    } else {
        Controller->PressureAdjustment = ELAN_I2C_PRESSURE_OFFSET;
    }

    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandMaxXAxis,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    Controller->MaxX = Value;
    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandMaxYAxis,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    Controller->MaxY = Value;
    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandTraceCount,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    Controller->TraceCountX = (UCHAR)Value;
    Controller->TraceCountY = (UCHAR)(Value >> BITS_PER_BYTE);
    Status = ElanI2cReadCommand(Controller,
                                ElanI2cCommandResolution,
                                &Value);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceInformationEnd;
    }

    Controller->ResolutionX = (UCHAR)Value;
    Controller->ResolutionY = (UCHAR)(Value >> BITS_PER_BYTE);

ReadDeviceInformationEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ELAN_I2C: Failed to read device information: %d\n",
                      Status);
    }

    return Status;
}

KSTATUS
ElanI2cGetReport (
    PELAN_I2C_CONTROLLER Controller,
    UCHAR Report[ELAN_I2C_REPORT_SIZE]
    )

/*++

Routine Description:

    This routine reads a report from the device.

Arguments:

    Controller - Supplies a pointer to the connected controller.

    Report - Supplies a pointer where the report will be returned.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = ElanI2cRawReceive(Controller, Report, ELAN_I2C_REPORT_SIZE);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ELAN_I2C: Failed to read report.\n");
    }

    return Status;
}

KSTATUS
ElanI2cSleepControl (
    PELAN_I2C_CONTROLLER Controller,
    BOOL Wake
    )

/*++

Routine Description:

    This routine sets the sleep control register.

Arguments:

    Controller - Supplies a pointer to the connected controller.

    Wake - Supplies a boolean indicating whether to put the device to sleep
        (FALSE) or wake it up (TRUE).

Return Value:

    Status code.

--*/

{

    ELAN_I2C_COMMAND Command;
    KSTATUS Status;

    Command = ElanI2cCommandSleep;
    if (Wake != FALSE) {
        Command = ElanI2cCommandWake;
    }

    Status = ElanI2cWriteCommand(Controller, ElanI2cCommandStandby, Command);
    return Status;
}

KSTATUS
ElanI2cReadCommand (
    PELAN_I2C_CONTROLLER Controller,
    ELAN_I2C_COMMAND Register,
    PUSHORT Value
    )

/*++

Routine Description:

    This routine performs a read command from the device.

Arguments:

    Controller - Supplies a pointer to the controller.

    Register - Supplies the register to read.

    Data - Supplies a pointer to the register to read.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = ElanI2cAccessRegister(Controller,
                                   Register,
                                   (PUCHAR)Value,
                                   sizeof(USHORT),
                                   FALSE);

    return Status;
}

KSTATUS
ElanI2cWriteCommand (
    PELAN_I2C_CONTROLLER Controller,
    ELAN_I2C_COMMAND Register,
    USHORT Value
    )

/*++

Routine Description:

    This routine performs a write command to the device.

Arguments:

    Controller - Supplies a pointer to the controller.

    Register - Supplies the register to write.

    Data - Supplies a pointer to the register to read.

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = ElanI2cAccessRegister(Controller,
                                   Register,
                                   (PUCHAR)&Value,
                                   sizeof(USHORT),
                                   TRUE);

    return Status;
}

KSTATUS
ElanI2cAccessRegister (
    PELAN_I2C_CONTROLLER Controller,
    ELAN_I2C_COMMAND Register,
    PUCHAR Data,
    ULONG Length,
    BOOL Write
    )

/*++

Routine Description:

    This routine performs an I2C bus access to get or set a single register.

Arguments:

    Controller - Supplies a pointer to the controller.

    Register - Supplies the register to read or write.

    Data - Supplies a pointer to the register content (either the value to
        write or where the read data will be returned).

    Length - Supplies the length of the read or write.

    Write - Supplies a boolean indicating whether to read the register (FALSE)
        or write the register.

Return Value:

    Status code.

--*/

{

    PUCHAR Buffer;
    PSPB_INTERFACE Interface;
    KSTATUS Status;
    SPB_TRANSFER Transfer[2];
    SPB_TRANSFER_SET TransferSet;

    Buffer = Controller->RequestBuffer;
    Buffer[0] = Register;
    Buffer[1] = (UCHAR)(Register >> BITS_PER_BYTE);
    RtlZeroMemory(Transfer, sizeof(Transfer));
    RtlZeroMemory(&TransferSet, sizeof(TransferSet));
    INITIALIZE_LIST_HEAD(&(TransferSet.TransferList));
    INSERT_BEFORE(&(Transfer[0].ListEntry), &(TransferSet.TransferList));
    Transfer[0].Direction = SpbTransferDirectionOut;
    Transfer[0].IoBuffer = Controller->RequestIoBuffer;

    //
    // For writes, only a single transfer is needed that contains both the
    // register and the value.
    //

    if (Write != FALSE) {
        Transfer[0].Size = Length + sizeof(USHORT);
        RtlCopyMemory(&(Buffer[2]), Data, Length);

    //
    // For reads, a second transfer is needed that takes in the data.
    //

    } else {
        Transfer[0].Size = sizeof(USHORT);
        Transfer[1].Direction = SpbTransferDirectionIn;
        Transfer[1].IoBuffer = Controller->RequestIoBuffer;
        Transfer[1].Size = Length;
        Transfer[1].Offset = sizeof(USHORT);
        INSERT_BEFORE(&(Transfer[1].ListEntry), &(TransferSet.TransferList));
    }

    Interface = Controller->SpbInterface;
    Status = Interface->ExecuteTransferSet(Controller->SpbHandle, &TransferSet);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (Write == FALSE) {
        RtlCopyMemory(Data, &(Buffer[sizeof(USHORT)]), Length);
    }

    return Status;
}

KSTATUS
ElanI2cRawReceive (
    PELAN_I2C_CONTROLLER Controller,
    PUCHAR Data,
    ULONG Length
    )

/*++

Routine Description:

    This routine performs direct I2C read.

Arguments:

    Controller - Supplies a pointer to the controller.

    Data - Supplies a pointer to the register content (either the value to
        write or where the read data will be returned).

    Length - Supplies the length of the read or write.

Return Value:

    Status code.

--*/

{

    PUCHAR Buffer;
    PSPB_INTERFACE Interface;
    KSTATUS Status;
    SPB_TRANSFER Transfer;
    SPB_TRANSFER_SET TransferSet;

    Buffer = Controller->RequestBuffer;
    RtlZeroMemory(&Transfer, sizeof(Transfer));
    RtlZeroMemory(&TransferSet, sizeof(TransferSet));
    INITIALIZE_LIST_HEAD(&(TransferSet.TransferList));
    INSERT_BEFORE(&(Transfer.ListEntry), &(TransferSet.TransferList));
    Transfer.Direction = SpbTransferDirectionIn;
    Transfer.IoBuffer = Controller->RequestIoBuffer;
    Transfer.Size = Length;
    Interface = Controller->SpbInterface;
    Status = Interface->ExecuteTransferSet(Controller->SpbHandle, &TransferSet);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    RtlCopyMemory(Data, Buffer, Length);
    return Status;
}

