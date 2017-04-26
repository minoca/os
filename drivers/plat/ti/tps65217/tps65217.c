/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tps65217.c

Abstract:

    This module implements support for the TPS65217 Power Management IC.

Author:

    Evan Green 8-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/spb/spb.h>
#include <minoca/intrface/tps65217.h>
#include "tps65217.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TPS65217_ALLOCATION_TAG 0x35367054

#define TPS65217_MAX_PACKET_SIZE 2

#define TPS65217_DCDC_SETTINGS 64

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TPS65217_PASSWORD_LEVEL {
    Tps65217PasswordNone,
    Tps65217PasswordLevel1,
    Tps65217PasswordLevel2
} TPS65217_PASSWORD_LEVEL, *PTPS65217_PASSWORD_LEVEL;

/*++

Structure Description:

    This structure defines the context for a TPS65217 PMIC.

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
        TPS65217_MAX_PACKET_SIZE used for request data.

    RequestIoBuffer - Stores a pointer to the I/O buffer around the request
        buffer.

    Interface - Stores the interface definition.

--*/

typedef struct _TPS65217_CONTROLLER {
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
    INTERFACE_TPS65217 Interface;
} TPS65217_CONTROLLER, *PTPS65217_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Tps65217AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Tps65217DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Tps65217DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Tps65217DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Tps65217DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Tps65217DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
Tps65217InterruptServiceWorker (
    PVOID Context
    );

KSTATUS
Tps65217ProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
Tps65217StartDevice (
    PIRP Irp,
    PTPS65217_CONTROLLER Device
    );

VOID
Tps65217SpbInterfaceNotificationCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

KSTATUS
Tps65217Initialize (
    PTPS65217_CONTROLLER Controller
    );

VOID
Tps65217InterruptThread (
    PVOID Parameter
    );

KSTATUS
Tps65217InterfaceSetDcDcRegulator (
    PINTERFACE_TPS65217 Interface,
    TPS65217_DCDC_REGULATOR Regulator,
    ULONG Millivolts
    );

KSTATUS
Tps65217SetDcDcRegulator (
    PTPS65217_CONTROLLER Controller,
    TPS65217_REGISTER Register,
    ULONG Millivolts
    );

KSTATUS
Tps65217Write (
    PTPS65217_CONTROLLER Controller,
    TPS65217_REGISTER Register,
    UCHAR Data
    );

KSTATUS
Tps65217Read (
    PTPS65217_CONTROLLER Controller,
    TPS65217_REGISTER Register,
    PUCHAR Data
    );

KSTATUS
Tps65217AccessRegister (
    PTPS65217_CONTROLLER Controller,
    TPS65217_REGISTER Register,
    PUCHAR Data,
    BOOL Write
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the password levels for each register.
//

const TPS65217_PASSWORD_LEVEL Tps65217PasswordLevel[Tps65217RegisterCount] = {
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordNone,
    Tps65217PasswordLevel1,
    Tps65217PasswordLevel2,
    Tps65217PasswordLevel2,
    Tps65217PasswordLevel2,
    Tps65217PasswordLevel2,
    Tps65217PasswordLevel2,
    Tps65217PasswordLevel2,
    Tps65217PasswordLevel2,
    Tps65217PasswordLevel2,
    Tps65217PasswordLevel1,
    Tps65217PasswordNone,
    Tps65217PasswordLevel1,
    Tps65217PasswordLevel1,
    Tps65217PasswordLevel1,
    Tps65217PasswordLevel1,
    Tps65217PasswordLevel1,
    Tps65217PasswordLevel1,
    Tps65217PasswordLevel1,
};

//
// Store the conversion from DCDC regulator values to millivolts.
//

const USHORT Tps65217DcDcMillivolts[TPS65217_DCDC_SETTINGS] = {
    900,
    925,
    950,
    975,
    1000,
    1025,
    1050,
    1075,
    1100,
    1125,
    1150,
    1175,
    1200,
    1225,
    1250,
    1275,
    1300,
    1325,
    1350,
    1375,
    1400,
    1425,
    1450,
    1475,
    1500,
    1550,
    1600,
    1650,
    1700,
    1750,
    1800,
    1850,
    1900,
    1950,
    2000,
    2050,
    2100,
    2150,
    2200,
    2250,
    2300,
    2350,
    2400,
    2450,
    2500,
    2550,
    2600,
    2650,
    2700,
    2750,
    2800,
    2850,
    2900,
    3000,
    3100,
    3200,
    3300,
    3300,
    3300,
    3300,
    3300,
    3300,
    3300,
    3300
};

PDRIVER Tps65217Driver;
UUID Tps65217SpbInterfaceUuid = UUID_SPB_INTERFACE;
UUID Tps65217InterfaceUuid = UUID_TPS65217_INTERFACE;

INTERFACE_TPS65217 Tps65217InterfaceTemplate = {
    NULL,
    Tps65217InterfaceSetDcDcRegulator
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

    This routine is the entry point for the TPS65217. It registers its other
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

    Tps65217Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Tps65217AddDevice;
    FunctionTable.DispatchStateChange = Tps65217DispatchStateChange;
    FunctionTable.DispatchOpen = Tps65217DispatchOpen;
    FunctionTable.DispatchClose = Tps65217DispatchClose;
    FunctionTable.DispatchIo = Tps65217DispatchIo;
    FunctionTable.DispatchSystemControl = Tps65217DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Tps65217AddDevice (
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
    PTPS65217_CONTROLLER Controller;
    KSTATUS Status;

    AllocationSize = sizeof(TPS65217_CONTROLLER) + TPS65217_MAX_PACKET_SIZE;
    Controller = MmAllocatePagedPool(AllocationSize, TPS65217_ALLOCATION_TAG);
    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, AllocationSize);
    Controller->OsDevice = DeviceToken;
    Controller->InterruptHandle = INVALID_HANDLE;
    Controller->RequestBuffer = (PVOID)(Controller + 1);
    RtlCopyMemory(&(Controller->Interface),
                  &Tps65217InterfaceTemplate,
                  sizeof(INTERFACE_TPS65217));

    Status = MmCreateIoBuffer(Controller->RequestBuffer,
                              TPS65217_MAX_PACKET_SIZE,
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
Tps65217DispatchStateChange (
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
            Status = Tps65217ProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Tps65217Driver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Tps65217StartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Tps65217Driver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Tps65217DispatchOpen (
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
Tps65217DispatchClose (
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
Tps65217DispatchIo (
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
Tps65217DispatchSystemControl (
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
Tps65217InterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents the low level interrupt service routine for the
    TPS65217.

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
    ThreadParameters.ThreadRoutine = Tps65217InterruptThread;
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
Tps65217ProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for a TPS65217. It adds an interrupt vector requirement for any
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
Tps65217StartDevice (
    PIRP Irp,
    PTPS65217_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the TPS65217 PMIC device.

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
                                      &Tps65217SpbInterfaceUuid,
                                      Tps65217SpbInterfaceNotificationCallback,
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
            RtlDebugPrint("TPS65217: Open SPB Failed: %d\n", Status);
            goto StartDeviceEnd;
        }
    }

    Status = Tps65217Initialize(Device);
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
        Connect.LowLevelServiceRoutine = Tps65217InterruptServiceWorker;
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
        Status = IoCreateInterface(&Tps65217InterfaceUuid,
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
Tps65217SpbInterfaceNotificationCallback (
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

    PTPS65217_CONTROLLER Controller;
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

            Status = IoDestroyInterface(&Tps65217InterfaceUuid,
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
Tps65217Initialize (
    PTPS65217_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes the TPS65217 PMIC. It reads the chip revision.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    UCHAR ChipId;
    UCHAR ChipId2;
    ULONG LoopIndex;
    KSTATUS Status;

    KeAcquireQueuedLock(Controller->Lock);
    Status = Tps65217Read(Controller, Tps65217ChipId, &ChipId);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    if ((ChipId == 0) || (ChipId == 0xFF)) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto InitializeEnd;
    }

    for (LoopIndex = 0; LoopIndex < 20; LoopIndex += 1) {
        Status = Tps65217Read(Controller, Tps65217ChipId, &ChipId2);
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed on loop %d\n", LoopIndex);

            ASSERT(FALSE);

            goto InitializeEnd;
        }

        if (ChipId2 != ChipId) {
            RtlDebugPrint("Mismatch (%d) %x %x\n", LoopIndex, ChipId, ChipId2);
        }
    }

InitializeEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return Status;
}

VOID
Tps65217InterruptThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the entry point for a new thread that is spawned each time
    the TPS65217 interrupt fires.

Arguments:

    Parameter - Supplies a context pointer, which in this case points to the
        controller.

Return Value:

    None.

--*/

{

    PTPS65217_CONTROLLER Controller;
    UCHAR InterruptRegister;
    INTERRUPT_STATUS InterruptStatus;
    KSTATUS Status;

    InterruptStatus = InterruptStatusNotClaimed;
    Controller = Parameter;
    Status = Tps65217Read(Controller, Tps65217Interrupt, &InterruptRegister);
    if (!KSUCCESS(Status)) {
        goto InterruptThreadEnd;
    }

    RtlDebugPrint("TPS65217 Interrupt 0x%x\n", InterruptRegister);
    if ((InterruptRegister & TPS65217_INTERRUPT_STATUS_MASK) != 0) {
        InterruptStatus = InterruptStatusClaimed;
    }

InterruptThreadEnd:
    HlContinueInterrupt(Controller->InterruptHandle, InterruptStatus);
    return;
}

KSTATUS
Tps65217InterfaceSetDcDcRegulator (
    PINTERFACE_TPS65217 Interface,
    TPS65217_DCDC_REGULATOR Regulator,
    ULONG Millivolts
    )

/*++

Routine Description:

    This routine sets a TPS65217 DC-DC regulator voltage to the given value.

Arguments:

    Interface - Supplies a pointer to the interface instance.

    Regulator - Supplies the regulator number to change.

    Millivolts - Supplies the millivolt value to change to.

Return Value:

    Status code.

--*/

{

    PTPS65217_CONTROLLER Controller;
    TPS65217_REGISTER Register;
    KSTATUS Status;

    Controller = Interface->Context;
    switch (Regulator) {
    case Tps65217DcDc1:
        Register = Tps65217DcDc1Voltage;
        break;

    case Tps65217DcDc2:
        Register = Tps65217DcDc2Voltage;
        break;

    case Tps65217DcDc3:
        Register = Tps65217DcDc3Voltage;
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireQueuedLock(Controller->Lock);
    Status = Tps65217SetDcDcRegulator(Controller, Register, Millivolts);
    KeReleaseQueuedLock(Controller->Lock);
    return Status;
}

KSTATUS
Tps65217SetDcDcRegulator (
    PTPS65217_CONTROLLER Controller,
    TPS65217_REGISTER Register,
    ULONG Millivolts
    )

/*++

Routine Description:

    This routine sets a TPS65217 DC-DC regulator voltage to the given value.
    This routine assumes the controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller.

    Register - Supplies the DC-DC regulator register to set.

    Millivolts - Supplies the millivolt value to change to.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the given voltage value cannot be achieved.

    Other errors on I/O failure.

--*/

{

    UCHAR Control;
    ULONG Index;
    KSTATUS Status;
    UCHAR Value;

    //
    // Convert from millivolts to a register value.
    //

    Value = 0;
    for (Index = 0; Index < TPS65217_DCDC_SETTINGS; Index += 1) {
        if (Tps65217DcDcMillivolts[Index] == Millivolts) {
            Value = Index;
            break;
        }
    }

    if (Index == TPS65217_DCDC_SETTINGS) {
        return STATUS_NOT_SUPPORTED;
    }

    Status = Tps65217Write(Controller, Register, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Set the GO bit to enact the change.
    //

    Status = Tps65217Read(Controller, Tps65217SlewControl, &Control);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Control |= TPS65217_SLEW_CONTROL_DCDC_GO;
    Status = Tps65217Write(Controller, Tps65217SlewControl, Control);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
Tps65217Write (
    PTPS65217_CONTROLLER Controller,
    TPS65217_REGISTER Register,
    UCHAR Data
    )

/*++

Routine Description:

    This routine reads a TPS65217 register over I2C. This routine takes care of
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

    ULONG LoopIndex;
    ULONG Loops;
    UCHAR Password;
    KSTATUS Status;

    Loops = 0;
    if (Register > Tps65217RegisterCount) {
        return STATUS_INVALID_PARAMETER;
    }

    Status = STATUS_SUCCESS;
    switch (Tps65217PasswordLevel[Register]) {
    case Tps65217PasswordNone:
        Status = Tps65217AccessRegister(Controller, Register, &Data, TRUE);
        break;

    case Tps65217PasswordLevel1:
        Loops = 1;
        break;

    case Tps65217PasswordLevel2:
        Loops = 2;
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    //
    // Write the password, then the data. For level 1 registers this only needs
    // to be done once, but for level 2 registers this needs to be done twice.
    //

    for (LoopIndex = 0; LoopIndex < Loops; LoopIndex += 1) {
        Password = TPS65217_PASSWORD_UNLOCK ^ Register;
        Status = Tps65217AccessRegister(Controller,
                                        Tps65217Password,
                                        &Password,
                                        TRUE);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Status = Tps65217AccessRegister(Controller, Register, &Data, TRUE);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    return Status;
}

KSTATUS
Tps65217Read (
    PTPS65217_CONTROLLER Controller,
    TPS65217_REGISTER Register,
    PUCHAR Data
    )

/*++

Routine Description:

    This routine reads a TPS65217 register over I2C.

Arguments:

    Controller - Supplies a pointer to the controller.

    Register - Supplies the register to read.

    Data - Supplies a pointer where the register contents will be returned on
        success.

Return Value:

    Status code.

--*/

{

    return Tps65217AccessRegister(Controller, Register, Data, FALSE);
}

KSTATUS
Tps65217AccessRegister (
    PTPS65217_CONTROLLER Controller,
    TPS65217_REGISTER Register,
    PUCHAR Data,
    BOOL Write
    )

/*++

Routine Description:

    This routine performs an I2C bus access to get or set a single register.
    Note that this routine alone is not sufficient to write to many TPS
    registers, due to the password mechanism.

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
    Transfer[0].Direction = SpbTransferDirectionOut;
    Transfer[0].IoBuffer = Controller->RequestIoBuffer;
    Transfer[0].Size = 1;
    Transfer[1].Direction = SpbTransferDirectionIn;
    if (Write != FALSE) {
        Transfer[1].Direction = SpbTransferDirectionOut;
        Buffer[1] = *Data;
    }

    Transfer[1].IoBuffer = Controller->RequestIoBuffer;
    Transfer[1].Offset = 1;
    Transfer[1].Size = 1;
    RtlZeroMemory(&TransferSet, sizeof(TransferSet));
    INITIALIZE_LIST_HEAD(&(TransferSet.TransferList));
    INSERT_BEFORE(&(Transfer[0].ListEntry), &(TransferSet.TransferList));
    INSERT_BEFORE(&(Transfer[1].ListEntry), &(TransferSet.TransferList));
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

