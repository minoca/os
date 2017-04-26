/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rk808.c

Abstract:

    This module implements support for the RK808 Power Management IC.

Author:

    Evan Green 4-Apr-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/spb/spb.h>
#include <minoca/intrface/rk808.h>
#include "rk808.h"

//
// ---------------------------------------------------------------- Definitions
//

#define RK808_ALLOCATION_TAG 0x35367054

#define RK808_MAX_PACKET_SIZE 2

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for an RK808 PMIC.

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
        RK808_MAX_PACKET_SIZE used for request data.

    RequestIoBuffer - Stores a pointer to the I/O buffer around the request
        buffer.

    Interface - Stores the interface definition.

--*/

typedef struct _RK808_CONTROLLER {
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
    INTERFACE_RK808 Interface;
} RK808_CONTROLLER, *PRK808_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Rk808AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Rk808DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk808DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk808DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk808DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk808DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
Rk808InterruptServiceWorker (
    PVOID Context
    );

KSTATUS
Rk808ProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
Rk808StartDevice (
    PIRP Irp,
    PRK808_CONTROLLER Device
    );

VOID
Rk808SpbInterfaceNotificationCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

KSTATUS
Rk808Initialize (
    PRK808_CONTROLLER Controller
    );

VOID
Rk808InterruptThread (
    PVOID Parameter
    );

KSTATUS
Rk808InterfaceSetLdo (
    PINTERFACE_RK808 Interface,
    UCHAR Ldo,
    PRK808_LDO_CONFIGURATION Configuration
    );

KSTATUS
Rk808SetLdo (
    PRK808_CONTROLLER Controller,
    UCHAR Ldo,
    PRK808_LDO_CONFIGURATION Configuration
    );

UCHAR
Rk808GetLdoSetting (
    UCHAR Ldo,
    USHORT Voltage
    );

KSTATUS
Rk808RtcRead (
    PVOID Context,
    PHARDWARE_MODULE_TIME CurrentTime
    );

KSTATUS
Rk808RtcWrite (
    PVOID Context,
    PHARDWARE_MODULE_TIME NewTime
    );

KSTATUS
Rk808RtcStart (
    PRK808_CONTROLLER Controller
    );

KSTATUS
Rk808RtcStop (
    PRK808_CONTROLLER Controller
    );

KSTATUS
Rk808Write (
    PRK808_CONTROLLER Controller,
    RK808_REGISTER Register,
    UCHAR Data
    );

KSTATUS
Rk808Read (
    PRK808_CONTROLLER Controller,
    RK808_REGISTER Register,
    PUCHAR Data
    );

KSTATUS
Rk808AccessRegister (
    PRK808_CONTROLLER Controller,
    RK808_REGISTER Register,
    PUCHAR Data,
    BOOL Write
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Rk808Driver;
UUID Rk808SpbInterfaceUuid = UUID_SPB_INTERFACE;
UUID Rk808InterfaceUuid = UUID_RK808_INTERFACE;

INTERFACE_RK808 Rk808InterfaceTemplate = {
    NULL,
    Rk808InterfaceSetLdo
};

PSTR Rk808Interrupt1Names[8] = {
    "Low Vout",
    "Low Battery",
    "Power Button Pressed",
    "Power Off",
    "Hot Die",
    "RTC Alarm",
    "RTC Interrupt"
};

PSTR Rk808Interrupt2Names[8] = {
    "AC Plugged in",
    "AC Unplugged",
    "Unknown Event",
    "Unknown Event",
    "Unknown Event",
    "Unknown Event",
    "Unknown Event",
    "Unknown Event",
};

//
// Define the output ranges for the LDOs. The first index here is LDO1.
//

RK808_LDO_RANGE Rk808LdoRanges[8] = {
    {1800, 3400, 100},
    {1800, 3400, 100},
    {800, 2500, 100},
    {1800, 3400, 100},
    {1800, 3400, 100},
    {800, 2500, 100},
    {800, 2500, 100},
    {1800, 3400, 100}
};

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

    This routine is the entry point for the RK808. It registers its other
    dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    Rk808Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Rk808AddDevice;
    FunctionTable.DispatchStateChange = Rk808DispatchStateChange;
    FunctionTable.DispatchOpen = Rk808DispatchOpen;
    FunctionTable.DispatchClose = Rk808DispatchClose;
    FunctionTable.DispatchIo = Rk808DispatchIo;
    FunctionTable.DispatchSystemControl = Rk808DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Rk808AddDevice (
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
    PRK808_CONTROLLER Controller;
    KSTATUS Status;

    AllocationSize = sizeof(RK808_CONTROLLER) + RK808_MAX_PACKET_SIZE;
    Controller = MmAllocatePagedPool(AllocationSize, RK808_ALLOCATION_TAG);
    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, AllocationSize);
    Controller->OsDevice = DeviceToken;
    Controller->InterruptHandle = INVALID_HANDLE;
    Controller->RequestBuffer = (PVOID)(Controller + 1);
    RtlCopyMemory(&(Controller->Interface),
                  &Rk808InterfaceTemplate,
                  sizeof(INTERFACE_RK808));

    Status = MmCreateIoBuffer(Controller->RequestBuffer,
                              RK808_MAX_PACKET_SIZE,
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

            MmFreePagedPool(Controller);
        }
    }

    return Status;
}

VOID
Rk808DispatchStateChange (
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
            Status = Rk808ProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Rk808Driver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Rk808StartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Rk808Driver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Rk808DispatchOpen (
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
Rk808DispatchClose (
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
Rk808DispatchIo (
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
Rk808DispatchSystemControl (
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
Rk808InterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents the low level interrupt service routine for the
    RK808.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    KSTATUS Status;
    THREAD_CREATION_PARAMETERS ThreadParameters;

    RtlZeroMemory(&ThreadParameters, sizeof(THREAD_CREATION_PARAMETERS));
    ThreadParameters.ThreadRoutine = Rk808InterruptThread;
    ThreadParameters.Parameter = Context;
    Status = PsCreateThread(&ThreadParameters);
    if (!KSUCCESS(Status)) {
        return InterruptStatusNotClaimed;
    }

    return InterruptStatusDefer;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Rk808ProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an RK808. It adds an interrupt vector requirement for any
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
Rk808StartDevice (
    PIRP Irp,
    PRK808_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the RK808 PMIC device.

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
    PRESOURCE_ALLOCATION LineAllocation;
    KSTATUS Status;

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
                                      &Rk808SpbInterfaceUuid,
                                      Rk808SpbInterfaceNotificationCallback,
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
            RtlDebugPrint("RK808: Open SPB Failed: %d\n", Status);
            goto StartDeviceEnd;
        }
    }

    Status = Rk808Initialize(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
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
        Connect.LowLevelServiceRoutine = Rk808InterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    //
    // Publish the interface.
    //

    if (Device->Interface.Context == NULL) {
        Device->Interface.Context = Device;
        Status = IoCreateInterface(&Rk808InterfaceUuid,
                                   Device->OsDevice,
                                   &(Device->Interface),
                                   sizeof(Device->Interface));

        if (!KSUCCESS(Status)) {
            Device->Interface.Context = NULL;
            goto StartDeviceEnd;
        }
    }

StartDeviceEnd:
    return Status;
}

VOID
Rk808SpbInterfaceNotificationCallback (
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

    PRK808_CONTROLLER Controller;
    PSPB_INTERFACE Interface;
    KSTATUS Status;

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

        //
        // First close the published interface.
        //

        if (Controller->Interface.Context != NULL) {

            ASSERT(Controller->Interface.Context == Controller);

            Status = IoDestroyInterface(&Rk808InterfaceUuid,
                                        Controller->OsDevice,
                                        &(Controller->Interface));

            ASSERT(KSUCCESS(Status));

            Controller->Interface.Context = NULL;
        }

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

KSTATUS
Rk808Initialize (
    PRK808_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes the RK808 PMIC. It reads the chip revision.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    CALENDAR_TIMER_DESCRIPTION CalendarTimer;
    UCHAR Mask;
    KSTATUS Status;

    Status = Rk808Write(Controller,
                        Rk808RtcControl,
                        RK808_RTC_CONTROL_READ_SHADOWED);

    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = Rk808Write(Controller, Rk808InterruptStatus1, 0xFF);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = Rk808Write(Controller, Rk808InterruptStatus2, 0xFF);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Enable interrupts.
    //

    Status = Rk808Read(Controller, Rk808InterruptMask1, &Mask);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Mask &= ~(RK808_INTERRUPT1_VOUT_LOW | RK808_INTERRUPT1_BATTERY_LOW |
              RK808_INTERRUPT1_POWER_ON | RK808_INTERRUPT1_POWER_ON_LONG_PRESS |
              RK808_INTERRUPT1_HOT_DIE);

    Status = Rk808Write(Controller, Rk808InterruptMask1, Mask);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = Rk808Write(Controller, Rk808InterruptMask2, 0);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Register the calendar timer portion as well.
    //

    RtlZeroMemory(&CalendarTimer, sizeof(CALENDAR_TIMER_DESCRIPTION));
    CalendarTimer.TableVersion = CALENDAR_TIMER_DESCRIPTION_VERSION;
    CalendarTimer.Context = Controller;
    CalendarTimer.Features = CALENDAR_TIMER_FEATURE_WANT_CALENDAR_FORMAT |
                             CALENDAR_TIMER_FEATURE_LOW_RUNLEVEL;

    CalendarTimer.FunctionTable.Read = Rk808RtcRead;
    CalendarTimer.FunctionTable.Write = Rk808RtcWrite;
    Status = HlRegisterHardware(HardwareModuleCalendarTimer, &CalendarTimer);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

InitializeEnd:
    return Status;
}

VOID
Rk808InterruptThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the entry point for a new thread that is spawned each time
    the RK808 interrupt fires.

Arguments:

    Parameter - Supplies a context pointer, which in this case points to the
        controller.

Return Value:

    None.

--*/

{

    PRK808_CONTROLLER Controller;
    ULONG Index;
    UCHAR InterruptRegister;
    INTERRUPT_STATUS InterruptStatus;
    KSTATUS Status;

    InterruptStatus = InterruptStatusNotClaimed;
    Controller = Parameter;
    Status = Rk808Read(Controller, Rk808InterruptStatus1, &InterruptRegister);
    if (!KSUCCESS(Status)) {
        goto InterruptThreadEnd;
    }

    if (InterruptRegister != 0) {
        InterruptStatus = InterruptStatusClaimed;
        Rk808Write(Controller, Rk808InterruptStatus1, InterruptRegister);
        for (Index = 0; Index < BITS_PER_BYTE; Index += 1) {
            if ((InterruptRegister & (1 << Index)) != 0) {
                RtlDebugPrint("RK808: %s\n", Rk808Interrupt1Names[Index]);
            }
        }

    } else {
        Status = Rk808Read(Controller,
                           Rk808InterruptStatus2,
                           &InterruptRegister);

        if (!KSUCCESS(Status)) {
            goto InterruptThreadEnd;
        }

        if (InterruptRegister != 0) {
            InterruptStatus = InterruptStatusClaimed;
            Rk808Write(Controller, Rk808InterruptStatus2, InterruptRegister);
            for (Index = 0; Index < BITS_PER_BYTE; Index += 1) {
                if ((InterruptRegister & (1 << Index)) != 0) {
                    RtlDebugPrint("RK808: %s\n", Rk808Interrupt2Names[Index]);
                }
            }

        }
    }

InterruptThreadEnd:
    HlContinueInterrupt(Controller->InterruptHandle, InterruptStatus);
    return;
}

KSTATUS
Rk808InterfaceSetLdo (
    PINTERFACE_RK808 Interface,
    UCHAR Ldo,
    PRK808_LDO_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine configures an RK808 LDO.

Arguments:

    Interface - Supplies a pointer to the interface instance.

    Ldo - Supplies the LDO number to change. Valid values are between 1 and 8.

    Configuration - Supplies a pointer to the new configuration to set.

Return Value:

    Status code.

--*/

{

    PRK808_CONTROLLER Controller;
    KSTATUS Status;

    Controller = Interface->Context;
    KeAcquireQueuedLock(Controller->Lock);
    Status = Rk808SetLdo(Controller, Ldo, Configuration);
    KeReleaseQueuedLock(Controller->Lock);
    return Status;
}

KSTATUS
Rk808SetLdo (
    PRK808_CONTROLLER Controller,
    UCHAR Ldo,
    PRK808_LDO_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine configures an RK808 LDO. It is assumed that the controller
    lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    Ldo - Supplies the LDO number to change. Valid values are between 1 and 8.

    Configuration - Supplies a pointer to the new configuration to set.

Return Value:

    Status code.

--*/

{

    UCHAR Enable;
    UCHAR LdoMask;
    UCHAR SleepOff;
    KSTATUS Status;
    UCHAR Value;

    if ((Ldo < 1) || (Ldo > RK808_LDO_COUNT)) {
        return STATUS_INVALID_ADDRESS;
    }

    LdoMask = 1 << (Ldo - 1);
    Status = Rk808Read(Controller, Rk808LdoEnable, &Enable);
    if (!KSUCCESS(Status)) {
        goto SetLdoEnd;
    }

    //
    // Configure the off in sleep mode setting.
    //

    Status = Rk808Read(Controller, Rk808SleepSetOff2, &SleepOff);
    if (!KSUCCESS(Status)) {
        goto SetLdoEnd;
    }

    Value = SleepOff;
    if ((Configuration->Flags & RK808_LDO_OFF_IN_SLEEP) != 0) {
        Value |= LdoMask;

    } else {
        Value &= ~LdoMask;
    }

    if (Value != SleepOff) {
        Status = Rk808Write(Controller, Rk808SleepSetOff2, Value);
        if (!KSUCCESS(Status)) {
            goto SetLdoEnd;
        }
    }

    //
    // Simply disable it if requested.
    //

    if ((Configuration->Flags & RK808_LDO_ENABLED) == 0) {
        Enable &= ~LdoMask;
        Status = Rk808Write(Controller, Rk808LdoEnable, Enable);
        goto SetLdoEnd;
    }

    //
    // Configure the voltages.
    //

    if (Configuration->ActiveVoltage != 0) {
        Value = Rk808GetLdoSetting(Ldo, Configuration->ActiveVoltage);
        Status = Rk808Write(Controller, RK808_LDO_ON_VSEL(Ldo), Value);
        if (!KSUCCESS(Status)) {
            goto SetLdoEnd;
        }
    }

    if (Configuration->SleepVoltage != 0) {
        Value = Rk808GetLdoSetting(Ldo, Configuration->SleepVoltage);
        Status = Rk808Write(Controller, RK808_LDO_SLP_VSEL(Ldo), Value);
        if (!KSUCCESS(Status)) {
            goto SetLdoEnd;
        }
    }

    //
    // Enable it if needed.
    //

    ASSERT((Configuration->Flags & RK808_LDO_ENABLED) != 0);

    if ((Enable & LdoMask) == 0) {
        Enable |= LdoMask;
        Status = Rk808Write(Controller, Rk808LdoEnable, Enable);
        if (!KSUCCESS(Status)) {
            goto SetLdoEnd;
        }
    }

    Status = STATUS_SUCCESS;

SetLdoEnd:
    return Status;
}

UCHAR
Rk808GetLdoSetting (
    UCHAR Ldo,
    USHORT Voltage
    )

/*++

Routine Description:

    This routine returns the LDO voltage register setting for a given LDO and
    voltage. If the voltage is out of the range, then the closest voltage
    setting not over the specified value will be returned.

Arguments:

    Ldo - Supplies the LDO number. Valid values are between 1 and 8.

    Voltage - Supplies the desired voltage setting.

Return Value:

    Status code.

--*/

{

    PRK808_LDO_RANGE Range;
    UCHAR Value;

    ASSERT((Ldo >= 1) && (Ldo <= RK808_LDO_COUNT));

    Range = &(Rk808LdoRanges[Ldo - 1]);
    if (Voltage < Range->Min) {
        Voltage = Range->Min;

    } else if (Voltage > Range->Max) {
        Voltage = Range->Max;
    }

    Value = (Voltage - Range->Min) / Range->Step;
    return Value;
}

KSTATUS
Rk808RtcRead (
    PVOID Context,
    PHARDWARE_MODULE_TIME CurrentTime
    )

/*++

Routine Description:

    This routine returns the calendar timer's current value.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    CurrentTime - Supplies a pointer where the read current time will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PRK808_CONTROLLER Controller;
    KSTATUS Status;
    PCALENDAR_TIME Time;
    UCHAR Value;

    Controller = Context;
    Time = &(CurrentTime->U.CalendarTime);
    CurrentTime->IsCalendarTime = TRUE;

    //
    // Read and clear the power up status and alarm bits.
    //

    Status = Rk808Read(Controller, Rk808RtcStatus, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = Rk808Write(Controller, Rk808RtcStatus, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Write a zero and then a one to snap the current time into the shadow
    // registers.
    //

    Status = Rk808Read(Controller, Rk808RtcControl, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value &= ~RK808_RTC_CONTROL_GET_TIME;
    Value |= RK808_RTC_CONTROL_READ_SHADOWED;
    Status = Rk808Write(Controller, Rk808RtcControl, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value |= RK808_RTC_CONTROL_GET_TIME;
    Status = Rk808Write(Controller, Rk808RtcControl, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value &= ~RK808_RTC_CONTROL_GET_TIME;
    Status = Rk808Write(Controller, Rk808RtcControl, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = Rk808Read(Controller, Rk808Seconds, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Time->Second = BCD_TO_BINARY(Value);
    Status = Rk808Read(Controller, Rk808Minutes, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Time->Minute = BCD_TO_BINARY(Value);
    Status = Rk808Read(Controller, Rk808Hours, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Time->Hour = BCD_TO_BINARY(Value);
    Status = Rk808Read(Controller, Rk808Days, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Time->Day = BCD_TO_BINARY(Value);
    Status = Rk808Read(Controller, Rk808Months, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Time->Month = BCD_TO_BINARY(Value);

    ASSERT(Time->Month != 0);

    Time->Month -= 1;
    Status = Rk808Read(Controller, Rk808Years, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Time->Year = BCD_TO_BINARY(Value) + 2000;
    Time->IsDaylightSaving = FALSE;
    Time->Nanosecond = 0;
    return STATUS_SUCCESS;
}

KSTATUS
Rk808RtcWrite (
    PVOID Context,
    PHARDWARE_MODULE_TIME NewTime
    )

/*++

Routine Description:

    This routine writes to the calendar timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    NewTime - Supplies a pointer to the new time to set. The hardware module
        should set this as quickly as possible. The system will supply either
        a calendar time or a system time in here based on which type the timer
        requested at registration.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PRK808_CONTROLLER Controller;
    KSTATUS Status;
    PCALENDAR_TIME Time;
    UCHAR Value;

    Controller = Context;
    Time = &(NewTime->U.CalendarTime);

    ASSERT(NewTime->IsCalendarTime != FALSE);

    //
    // Stop the clock while programming.
    //

    Status = Rk808RtcStop(Controller);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value = BINARY_TO_BCD(Time->Second);
    Status = Rk808Write(Controller, Rk808Seconds, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value = BINARY_TO_BCD(Time->Minute);
    Status = Rk808Write(Controller, Rk808Minutes, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value = BINARY_TO_BCD(Time->Hour);
    Status = Rk808Write(Controller, Rk808Hours, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value = BINARY_TO_BCD(Time->Day);
    Status = Rk808Write(Controller, Rk808Days, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    ASSERT(Time->Month != MONTHS_PER_YEAR);

    Value = BINARY_TO_BCD(Time->Month + 1);
    Status = Rk808Write(Controller, Rk808Months, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (Time->Year < 2000) {
        Value = BINARY_TO_BCD(Time->Year - 1900);

    } else {
        Value = BINARY_TO_BCD(Time->Year - 2000);
    }

    Status = Rk808Write(Controller, Rk808Years, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Fire the clock back up.
    //

    Status = Rk808RtcStart(Controller);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
Rk808RtcStart (
    PRK808_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine starts the RK808 RTC.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    UCHAR Value;

    Status = Rk808Read(Controller, Rk808RtcControl, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value &= ~RK808_RTC_CONTROL_STOP;
    Status = Rk808Write(Controller, Rk808RtcControl, Value);
    return Status;
}

KSTATUS
Rk808RtcStop (
    PRK808_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine stops the RK808 RTC.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    UCHAR Value;

    Status = Rk808Read(Controller, Rk808RtcControl, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value |= RK808_RTC_CONTROL_STOP;
    Status = Rk808Write(Controller, Rk808RtcControl, Value);
    return Status;
}

KSTATUS
Rk808Write (
    PRK808_CONTROLLER Controller,
    RK808_REGISTER Register,
    UCHAR Data
    )

/*++

Routine Description:

    This routine reads a RK808 register over I2C. This routine takes care of
    the password protocol.

Arguments:

    Controller - Supplies a pointer to the controller.

    Register - Supplies the register to write.

    Data - Supplies a pointer where the register contents will be returned on
        success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = Rk808AccessRegister(Controller, Register, &Data, TRUE);
    return Status;
}

KSTATUS
Rk808Read (
    PRK808_CONTROLLER Controller,
    RK808_REGISTER Register,
    PUCHAR Data
    )

/*++

Routine Description:

    This routine reads a RK808 register over I2C.

Arguments:

    Controller - Supplies a pointer to the controller.

    Register - Supplies the register to read.

    Data - Supplies a pointer where the register contents will be returned on
        success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = Rk808AccessRegister(Controller, Register, Data, FALSE);
    return Status;
}

KSTATUS
Rk808AccessRegister (
    PRK808_CONTROLLER Controller,
    RK808_REGISTER Register,
    PUCHAR Data,
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
        Transfer[0].Size = 2;
        Buffer[1] = *Data;

    //
    // For reads, a second transfer is needed that takes in the data.
    //

    } else {
        Transfer[0].Size = 1;
        Transfer[1].Direction = SpbTransferDirectionIn;
        Transfer[1].IoBuffer = Controller->RequestIoBuffer;
        Transfer[1].Size = 1;
        Transfer[1].Offset = 1;
        INSERT_BEFORE(&(Transfer[1].ListEntry), &(TransferSet.TransferList));
    }

    Interface = Controller->SpbInterface;
    Status = Interface->ExecuteTransferSet(Controller->SpbHandle, &TransferSet);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (Write == FALSE) {
        *Data = Buffer[1];
    }

    return Status;
}

