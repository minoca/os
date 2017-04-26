/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bc27gpio.c

Abstract:

    This module implements the Broadcom BC27xx GPIO driver.

Author:

    Chris Stevens 10-May-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/gpio/gpiohost.h>
#include <minoca/soc/bcm2709.h>

//
// --------------------------------------------------------------------- Macros
//

#define BCM27_READ_GPIO(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define BCM27_WRITE_GPIO(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define BCM27_GPIO_ALLOCATION_TAG 0x47326342 // 'G2cB'

#define BCM27_GPIO_LINE_COUNT 54

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for a BC27xx GPIO controller.

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

    ControllerBase - Stores the virtual address of the memory mapping to the
        GPIO registers.

    GpioController - Stores a pointer to the library GPIO controller.

    PinInterruptState - Stores an array of bitmasks that describe each pin's
        interrupt state. See GPIO_FLAG_* for definitions.

--*/

typedef struct _BCM27_GPIO_CONTROLLER {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PGPIO_CONTROLLER GpioController;
    ULONG PinInterruptState[BCM27_GPIO_LINE_COUNT];
} BCM27_GPIO_CONTROLLER, *PBCM27_GPIO_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Bcm27GpioAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Bcm27GpioDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27GpioDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27GpioDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27GpioDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27GpioDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
Bcm27GpioProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
Bcm27GpioStartDevice (
    PIRP Irp,
    PBCM27_GPIO_CONTROLLER Device
    );

KSTATUS
Bcm27GpioSetConfiguration (
    PVOID Context,
    ULONG Pin,
    PGPIO_PIN_CONFIGURATION Configuration
    );

KSTATUS
Bcm27GpioSetDirection (
    PVOID Context,
    ULONG Pin,
    ULONG Flags
    );

VOID
Bcm27GpioSetValue (
    PVOID Context,
    ULONG Pin,
    ULONG Value
    );

ULONG
Bcm27GpioGetValue (
    PVOID Context,
    ULONG Pin
    );

KSTATUS
Bcm27GpioPrepareForInterrupts (
    PVOID Context
    );

VOID
Bcm27GpioInterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

INTERRUPT_CAUSE
Bcm27GpioInterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

VOID
Bcm27GpioEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    );

VOID
Bcm27GpioInterruptMaskPin (
    PBCM27_GPIO_CONTROLLER Controller,
    ULONG Pin,
    BOOL Enable
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Bcm27GpioDriver = NULL;

GPIO_FUNCTION_TABLE Bcm27GpioFunctionTableTemplate = {
    Bcm27GpioSetConfiguration,
    Bcm27GpioSetDirection,
    Bcm27GpioSetValue,
    Bcm27GpioGetValue,
    Bcm27GpioPrepareForInterrupts,
    Bcm27GpioInterruptMaskLine,
    Bcm27GpioInterruptBegin,
    Bcm27GpioEndOfInterrupt,
    NULL
};

//
// Store the default pull up/down settings for each GPIO pin. If the pin's bit
// is not set in either array, then the pull up/down is disabled by default.
//

ULONG Bcm27GpioPullUpDefaults[2] = {0x000001FF, 0x003FC01C};
ULONG Bcm27GpioPullDownDefaults[2] = {0xCFFFFE00, 0x00000FE3};

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

    This routine is the entry point for the BC27xx GPIO driver. It registers
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

    Bcm27GpioDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Bcm27GpioAddDevice;
    FunctionTable.DispatchStateChange = Bcm27GpioDispatchStateChange;
    FunctionTable.DispatchOpen = Bcm27GpioDispatchOpen;
    FunctionTable.DispatchClose = Bcm27GpioDispatchClose;
    FunctionTable.DispatchIo = Bcm27GpioDispatchIo;
    FunctionTable.DispatchSystemControl = Bcm27GpioDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Bcm27GpioAddDevice (
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

    PBCM27_GPIO_CONTROLLER Controller;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(BCM27_GPIO_CONTROLLER),
                                        BCM27_GPIO_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(BCM27_GPIO_CONTROLLER));
    Controller->OsDevice = DeviceToken;
    Controller->InterruptHandle = INVALID_HANDLE;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, Controller);
    return Status;
}

VOID
Bcm27GpioDispatchStateChange (
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
            Status = Bcm27GpioProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Bcm27GpioDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Bcm27GpioStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Bcm27GpioDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Bcm27GpioDispatchOpen (
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
Bcm27GpioDispatchClose (
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
Bcm27GpioDispatchIo (
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
Bcm27GpioDispatchSystemControl (
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

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Bcm27GpioProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an BC27xx GPIO controller. It adds an interrupt vector requirement
    for any interrupt line requested.

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
Bcm27GpioStartDevice (
    PIRP Irp,
    PBCM27_GPIO_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the BC27xx GPIO device.

Arguments:

    Irp - Supplies a pointer to the start IRP.

    Device - Supplies a pointer to the device information.

Return Value:

    Status code.

--*/

{

    ULONG AlignmentOffset;
    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PRESOURCE_ALLOCATION ControllerBase;
    PHYSICAL_ADDRESS EndAddress;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    PRESOURCE_ALLOCATION LineAllocation;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    GPIO_CONTROLLER_INFORMATION Registration;
    RUNLEVEL RunLevel;
    ULONG Size;
    KSTATUS Status;

    ControllerBase = NULL;
    InterruptLine = -1ULL;
    InterruptVector = -1ULL;

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
                InterruptLine = Device->InterruptLine;
                InterruptVector = Device->InterruptVector;

            } else {

                ASSERT((Device->InterruptLine == LineAllocation->Allocation) &&
                       (Device->InterruptVector == Allocation->Allocation));

                InterruptLine = Device->InterruptLine;
            }

        //
        // Look for the first physical address reservation, the registers.
        //

        } else if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            if (ControllerBase == NULL) {
                ControllerBase = Allocation;
            }
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Fail to start if the controller base was not found.
    //

    if (ControllerBase == NULL) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    //
    // Map the controller.
    //

    if (Device->ControllerBase == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = ControllerBase->Allocation;
        EndAddress = PhysicalAddress + ControllerBase->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = ControllerBase->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);

        //
        // If the size is not a page, then the failure code at the bottom needs
        // to be fancier.
        //

        ASSERT(Size == PageSize);

        Device->ControllerBase = MmMapPhysicalAddress(PhysicalAddress,
                                                      Size,
                                                      TRUE,
                                                      FALSE,
                                                      TRUE);

        if (Device->ControllerBase == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartDeviceEnd;
        }

        Device->ControllerBase += AlignmentOffset;
    }

    ASSERT(Device->ControllerBase != NULL);

    //
    // Allocate the controller structures.
    //

    if (Device->GpioController == NULL) {
        RtlZeroMemory(&Registration, sizeof(GPIO_CONTROLLER_INFORMATION));
        Registration.Version = GPIO_CONTROLLER_INFORMATION_VERSION;
        Registration.Context = Device;
        Registration.Device = Device->OsDevice;
        Registration.LineCount = BCM27_GPIO_LINE_COUNT;
        Registration.Features = GPIO_FEATURE_INTERRUPTS;
        RtlCopyMemory(&(Registration.FunctionTable),
                      &Bcm27GpioFunctionTableTemplate,
                      sizeof(GPIO_FUNCTION_TABLE));

        Status = GpioCreateController(&Registration, &(Device->GpioController));
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    //
    // Start up the controller.
    //

    Status = GpioStartController(Device->GpioController,
                                 InterruptLine,
                                 InterruptVector);

    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Connect the interrupt, handing it to the GPIO library, which will
    // eventually call back into the Begin Interrupt and End Interrupt routines
    // here.
    //

    if (Device->InterruptHandle == INVALID_HANDLE) {
        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Device->InterruptLine;
        Connect.Vector = Device->InterruptVector;
        Connect.InterruptServiceRoutine = GpioInterruptService;
        Connect.Context = Device->GpioController;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    RunLevel = IoGetInterruptRunLevel(&(Device->InterruptHandle), 1);
    GpioSetInterruptRunLevel(Device->GpioController, RunLevel);

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->ControllerBase != NULL) {
            MmUnmapAddress(Device->ControllerBase, MmPageSize());
            Device->ControllerBase = NULL;
        }

        if (Device->GpioController != NULL) {
            GpioDestroyController(Device->GpioController);
            Device->GpioController = NULL;
        }
    }

    return Status;
}

KSTATUS
Bcm27GpioSetConfiguration (
    PVOID Context,
    ULONG Pin,
    PGPIO_PIN_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine sets the complete configuration for one GPIO pin.

Arguments:

    Context - Supplies the controller context pointer.

    Pin - Supplies the zero-based pin number on the controller to configure.

    Configuration - Supplies a pointer to the new configuration to set.

Return Value:

    Status code.

--*/

{

    BCM2709_GPIO_REGISTER ClockRegister;
    PBCM27_GPIO_CONTROLLER Controller;
    ULONG DefaultIndex;
    ULONG Delay;
    ULONG Flags;
    ULONG PinMask;
    ULONG Pull;
    KSTATUS Status;

    Controller = Context;
    Flags = Configuration->Flags;

    //
    // Disable this pin as an interrupt source while it's being configured.
    //

    Bcm27GpioInterruptMaskPin(Controller, Pin, FALSE);

    //
    // Set the direction.
    //

    Status = Bcm27GpioSetDirection(Controller, Pin, Flags);
    if (!KSUCCESS(Status)) {
        goto SetConfigurationEnd;
    }

    //
    // Configure the interrupt configuration if the line is configured as
    // an interrupt. This only applies to the input direction.
    //

    if (((Flags & GPIO_OUTPUT) == 0) &&
        ((Flags & GPIO_INTERRUPT) != 0)) {

        Controller->PinInterruptState[Pin] = Flags;
        Bcm27GpioInterruptMaskPin(Controller, Pin, TRUE);
    }

    //
    // Set the pull up and pull down state. It is either requested to be in a
    // certain state or the default is set.
    //

    if (Pin < 32) {
        PinMask = 1 << Pin;
        ClockRegister = Bcm2709GpioPinPullUpDownClock0;
        DefaultIndex = 0;

    } else {
        PinMask = 1 << (Pin - 32);
        ClockRegister = Bcm2709GpioPinPullUpDownClock1;
        DefaultIndex = 1;
    }

    if ((Flags & (GPIO_PULL_UP | GPIO_PULL_DOWN | GPIO_PULL_NONE)) != 0) {
        if ((Flags & GPIO_PULL_NONE) == GPIO_PULL_NONE) {
            Pull = BCM2709_GPIO_PULL_NONE;

        } else if ((Flags & GPIO_PULL_UP) != 0) {
            Pull = BCM2709_GPIO_PULL_UP;

        } else {

            ASSERT((Flags & GPIO_PULL_DOWN) != 0);

            Pull = BCM2709_GPIO_PULL_DOWN;
        }

    } else {

        ASSERT((Bcm27GpioPullDownDefaults[DefaultIndex] &
                Bcm27GpioPullUpDefaults[DefaultIndex]) == 0);

        if ((Bcm27GpioPullDownDefaults[DefaultIndex] & PinMask) != 0) {
            Pull = BCM2709_GPIO_PULL_DOWN;

        } else if ((Bcm27GpioPullUpDefaults[DefaultIndex] & PinMask) != 0) {
            Pull = BCM2709_GPIO_PULL_UP;

        } else {
            Pull = BCM2709_GPIO_PULL_NONE;
        }
    }

    //
    // After setting the pull up/down control, the system must wait 150
    // cycles before programming the clock.
    //

    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinPullUpDownEnable, Pull);
    for (Delay = 0; Delay < 150; Delay += 1) {
        NOTHING;
    }

    //
    // The hold time for the control signal is 150 cycles. Wait after the
    // clock is set.
    //

    BCM27_WRITE_GPIO(Controller, ClockRegister, PinMask);
    for (Delay = 0; Delay < 150; Delay += 1) {
        NOTHING;
    }

    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinPullUpDownEnable, 0);
    BCM27_WRITE_GPIO(Controller, ClockRegister, 0);

SetConfigurationEnd:
    return Status;
}

KSTATUS
Bcm27GpioSetDirection (
    PVOID Context,
    ULONG Pin,
    ULONG Flags
    )

/*++

Routine Description:

    This routine sets the complete configuration for one GPIO pin.

Arguments:

    Context - Supplies the controller context pointer.

    Pin - Supplies the zero-based pin number on the controller to configure.

    Flags - Supplies the GPIO_* configuration flags to set. Only GPIO_OUTPUT
        and GPIO_OUTPUT_HIGH are observed, all other flags are ignored.

Return Value:

    Status code.

--*/

{

    PBCM27_GPIO_CONTROLLER Controller;
    ULONG Mode;
    BCM2709_GPIO_REGISTER Register;
    ULONG Shift;
    ULONG Value;

    Controller = Context;
    if (Pin > BCM2709_GPIO_PIN_MAX) {
        return STATUS_INVALID_PARAMETER;
    }

    Register = Bcm2709GpioSelect0 +
               (Pin / BCM2709_GPIO_FUNCTION_SELECT_PIN_COUNT) *
                BCM2709_GPIO_FUNCTION_SELECT_REGISTER_BYTE_WIDTH;

    Shift = (Pin % BCM2709_GPIO_FUNCTION_SELECT_PIN_COUNT) *
            BCM2709_GPIO_FUNCTION_SELECT_PIN_BIT_WIDTH;

    //
    // Determine the desired mode. Only input and output are supported here.
    // The alternate functions are not exposed.
    //

    if ((Flags & GPIO_OUTPUT) != 0) {
        Mode = BCM2709_GPIO_FUNCTION_SELECT_OUTPUT;

        //
        // Set the initial output value.
        //

        Value = 0;
        if ((Flags & GPIO_OUTPUT_HIGH) != 0) {
            Value = 1;
        }

        Bcm27GpioSetValue(Context, Pin, Value);

    } else {
        Mode = BCM2709_GPIO_FUNCTION_SELECT_INPUT;
    }

    //
    // Clear the function select value first and then set it.
    //

    Value = BCM27_READ_GPIO(Controller, Register);
    Value &= ~(BCM2709_GPIO_FUNCTION_SELECT_MASK << Shift);
    BCM27_WRITE_GPIO(Controller, Register, Value);
    Value |= (Mode << Shift);
    BCM27_WRITE_GPIO(Controller, Register, Value);
    return STATUS_SUCCESS;
}

VOID
Bcm27GpioSetValue (
    PVOID Context,
    ULONG Pin,
    ULONG Value
    )

/*++

Routine Description:

    This routine sets the output value on a GPIO pin.

Arguments:

    Context - Supplies the controller context pointer.

    Pin - Supplies the zero-based pin number on the controller to set.

    Value - Supplies the value to set on the pin: zero for low, non-zero for
        high.

Return Value:

    None.

--*/

{

    PBCM27_GPIO_CONTROLLER Controller;
    ULONG Offset;
    ULONG PinMask;
    BCM2709_GPIO_REGISTER Register;

    Controller = Context;
    if (Pin > BCM2709_GPIO_PIN_MAX) {
        return;
    }

    Register = Bcm2709GpioPinOutputClear0;
    if (Value != 0) {
        Register = Bcm2709GpioPinOutputSet0;
    }

    Offset = 0;
    if (Pin >= 32) {
        Pin -= 32;
        Offset = 4;
    }

    //
    // Writing 0 to a bit has no effect; there is no need to read-modify-write.
    //

    PinMask = 1 << Pin;
    BCM27_WRITE_GPIO(Controller, Register + Offset, PinMask);
    return;
}

ULONG
Bcm27GpioGetValue (
    PVOID Context,
    ULONG Pin
    )

/*++

Routine Description:

    This routine gets the input value on a GPIO pin.

Arguments:

    Context - Supplies the controller context pointer.

    Pin - Supplies the zero-based pin number on the controller to get.

Return Value:

    0 if the value was low.

    1 if the value was high.

    -1 on error.

--*/

{

    PBCM27_GPIO_CONTROLLER Controller;
    ULONG PinMask;
    BCM2709_GPIO_REGISTER Register;
    ULONG Value;

    Controller = Context;
    if (Pin > BCM2709_GPIO_PIN_MAX) {
        return -1;
    }

    Register = Bcm2709GpioPinLevel0;
    if (Pin >= 32) {
        Register = Bcm2709GpioPinLevel1;
        Pin -= 32;
    }

    PinMask = 1 << Pin;
    Value = BCM27_READ_GPIO(Controller, Register);
    return ((Value & PinMask) != 0);
}

KSTATUS
Bcm27GpioPrepareForInterrupts (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an interrupt controller. It's responsible for
    masking all interrupt lines on the controller and setting the current
    priority to the lowest (allow all interrupts). Once completed successfully,
    it is expected that interrupts can be enabled at the processor core with
    no interrupts occurring.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PBCM27_GPIO_CONTROLLER Controller;

    //
    // Mask all interrupts.
    //

    Controller = Context;
    RtlZeroMemory(Controller->PinInterruptState,
                  sizeof(Controller->PinInterruptState));

    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinRisingEdgeDetect0, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinRisingEdgeDetect1, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinFallingEdgeDetect0, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinFallingEdgeDetect1, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinHighDetect0, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinHighDetect1, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinLowDetect0, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinLowDetect1, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinAsyncRisingEdgeDetect0, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinAsyncRisingEdgeDetect1, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinAsyncFallingEdgeDetect0, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinAsyncFallingEdgeDetect1, 0);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinEventDetectStatus0, 0xFFFFFFFF);
    BCM27_WRITE_GPIO(Controller, Bcm2709GpioPinEventDetectStatus1, 0x003FFFFF);
    return STATUS_SUCCESS;
}

VOID
Bcm27GpioInterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    )

/*++

Routine Description:

    This routine masks or unmasks an interrupt line, leaving the rest of the
    line state intact.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the line to maek or unmask. This will always
        be a controller specified line.

    Enable - Supplies a boolean indicating whether to mask the interrupt,
        preventing interrupts from coming through (FALSE), or enable the line
        and allow interrupts to come through (TRUE).

Return Value:

    None.

--*/

{

    Bcm27GpioInterruptMaskPin(Context, Line->U.Local.Line, Enable);
    return;
}

INTERRUPT_CAUSE
Bcm27GpioInterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    )

/*++

Routine Description:

    This routine is called when an interrupt fires. Its role is to determine
    if an interrupt has fired on the given controller, accept it, and determine
    which line fired if any. This routine will always be called with interrupts
    disabled at the processor core.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    FiringLine - Supplies a pointer where the interrupt hardware module will
        fill in which line fired, if applicable.

    MagicCandy - Supplies a pointer where the interrupt hardware module can
        store 32 bits of private information regarding this interrupt. This
        information will be returned to it when the End Of Interrupt routine
        is called.

Return Value:

    Returns an interrupt cause indicating whether or not an interrupt line,
    spurious interrupt, or no interrupt fired on this controller.

--*/

{

    PBCM27_GPIO_CONTROLLER Controller;
    ULONG Line;
    ULONG LineOffset;
    ULONG Value;

    Controller = Context;
    LineOffset = 0;
    Value = BCM27_READ_GPIO(Controller, Bcm2709GpioPinEventDetectStatus0);
    if (Value == 0) {
        Value = BCM27_READ_GPIO(Controller, Bcm2709GpioPinEventDetectStatus1);
        if (Value == 0) {
            return InterruptCauseNoInterruptHere;
        }

        LineOffset = 32;
    }

    Line = RtlCountTrailingZeros32(Value) + LineOffset;
    FiringLine->Type = InterruptLineControllerSpecified;
    FiringLine->U.Local.Controller = (UINTN)(Controller->OsDevice);
    FiringLine->U.Local.Line = Line;
    *MagicCandy = Line;
    return InterruptCauseLineFired;
}

VOID
Bcm27GpioEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    )

/*++

Routine Description:

    This routine is called after an interrupt has fired and been serviced. Its
    role is to tell the interrupt controller that processing has completed.
    This routine will always be called with interrupts disabled at the
    processor core.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    MagicCandy - Supplies the magic candy that that the interrupt hardware
        module stored when the interrupt began.

Return Value:

    None.

--*/

{

    PBCM27_GPIO_CONTROLLER Controller;
    ULONG PinMask;
    BCM2709_GPIO_REGISTER Register;

    Controller = Context;
    Register = Bcm2709GpioPinEventDetectStatus0;
    if (MagicCandy >= 32) {
        MagicCandy -= 32;
        Register = Bcm2709GpioPinEventDetectStatus1;
    }

    PinMask = 1 << MagicCandy;
    BCM27_WRITE_GPIO(Controller, Register, PinMask);
    return;
}

VOID
Bcm27GpioInterruptMaskPin (
    PBCM27_GPIO_CONTROLLER Controller,
    ULONG Pin,
    BOOL Enable
    )

/*++

Routine Description:

    This routine enables or disables GPIO interrupts for the given pin. This
    routine is protected by the GPIO core's lock, making the read-modify-write
    behavior safe.

Arguments:

    Controller - Supplies a pointer to the controller on which to disable the
        interrupts.

    Pin - Supplies the GPIO pin for which interrupts are to be enabled or
        disabled.

    Enable - Supplies a boolean indicating whether to mask the interrupt,
        preventing interrupts from coming through (FALSE), or enable the pin's
        line and allow interrupts to come through (TRUE).

Return Value:

    None.

--*/

{

    ULONG Flags;
    ULONG Offset;
    ULONG PinMask;
    BCM2709_GPIO_REGISTER Register;
    ULONG Value;

    ASSERT(Pin <= BCM2709_GPIO_PIN_MAX);

    Offset = 0;
    Flags = Controller->PinInterruptState[Pin];
    if (Pin >= 32) {
        Pin -= 32;
        Offset = 4;
    }

    PinMask = 1 << Pin;
    if ((Flags & GPIO_INTERRUPT_EDGE_TRIGGERED) != 0) {
        if ((Flags & GPIO_INTERRUPT_RISING_EDGE) != 0) {
            Register = Bcm2709GpioPinRisingEdgeDetect0;
            Value = BCM27_READ_GPIO(Controller, Register + Offset);
            if (Enable != FALSE) {
                Value |= PinMask;

            } else {
                Value &= ~PinMask;
            }

            BCM27_WRITE_GPIO(Controller, Register, Value);
        }

        if ((Flags & GPIO_INTERRUPT_FALLING_EDGE) != 0) {
            Register = Bcm2709GpioPinFallingEdgeDetect0;
            Value = BCM27_READ_GPIO(Controller, Register + Offset);
            if (Enable != FALSE) {
                Value |= PinMask;

            } else {
                Value &= ~PinMask;
            }

            BCM27_WRITE_GPIO(Controller, Register, Value);
        }

    //
    // This is a level-triggered interrupt.
    //

    } else {
        if ((Flags & GPIO_INTERRUPT_ACTIVE_LOW) != 0) {
            Register = Bcm2709GpioPinLowDetect0;

        } else {
            Register = Bcm2709GpioPinHighDetect0;
        }

        Value = BCM27_READ_GPIO(Controller, Register + Offset);
        if (Enable != FALSE) {
            Value |= PinMask;

        } else {
            Value &= ~PinMask;
        }

        BCM27_WRITE_GPIO(Controller, Register + Offset, Value);
    }

    return;
}

