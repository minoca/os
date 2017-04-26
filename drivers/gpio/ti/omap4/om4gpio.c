/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    om4gpio.c

Abstract:

    This module implements the TI OMAP4 GPIO driver.

Author:

    Evan Green 4-Aug-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/gpio/gpiohost.h>

//
// --------------------------------------------------------------------- Macros
//

#define OMAP4_READ_GPIO(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define OMAP4_WRITE_GPIO(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4_GPIO_ALLOCATION_TAG 0x47346D4F

#define OMAP4_GPIO_LINE_COUNT 32

#define OMAP4_GPIO_SYS_CONFIG_NO_IDLE 0x00000001
#define OMAP4_GPIO_SYS_CONFIG_SMART_IDLE 0x00000002

#define OMAP4_GPIO_CONTROL_DISABLE_MODULE 0x00000001

#define OMAP4_GPIO_DEBOUNCE_GRANULARITY 31
#define OMAP4_GPIO_MAX_DEBOUNCE 0xFF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _OMAP4_GPIO_REGISTER {
    Omap4GpioRevision = 0x000,
    Omap4GpioSysConfig = 0x010,
    Omap4GpioIrqStatusRaw0 = 0x024,
    Omap4GpioIrqStatusRaw1 = 0x028,
    Omap4GpioIrqStatus0 = 0x02C,
    Omap4GpioIrqStatus1 = 0x030,
    Omap4GpioIrqStatusSet0 = 0x034,
    Omap4GpioIrqStatusSet1 = 0x038,
    Omap4GpioIrqStatusClear0 = 0x03C,
    Omap4GpioIrqStatusClear1 = 0x040,
    Omap4GpioIrqWakeEnable0 = 0x044,
    Omap4GpioIrqWakeEnable1 = 0x048,
    Omap4GpioSysStatus = 0x114,
    Omap4GpioWakeUpEnable = 0x120,
    Omap4GpioControl = 0x130,
    Omap4GpioOutputEnable = 0x134,
    Omap4GpioDataIn = 0x138,
    Omap4GpioDataOut = 0x13C,
    Omap4GpioLevelDetect0 = 0x140,
    Omap4GpioLevelDetect1 = 0x144,
    Omap4GpioRisingDetect = 0x148,
    Omap4GpioFallingDetect = 0x14C,
    Omap4GpioDebounceEnable = 0x150,
    Omap4GpioDebouncingTime = 0x154,
    Omap4GpioClearWakeUpEnable = 0x180,
    Omap4GpioSetWakeUpEnable = 0x184,
    Omap4GpioClearDataOut = 0x190,
    Omap4GpioSetDataOut = 0x194
} OMAP4_GPIO_REGISTER, *POMAP4_GPIO_REGISTER;

/*++

Structure Description:

    This structure defines the context for an OMAP4 GPIO controller.

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

--*/

typedef struct _OMAP4_GPIO_CONTROLLER {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PGPIO_CONTROLLER GpioController;
} OMAP4_GPIO_CONTROLLER, *POMAP4_GPIO_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Omap4GpioAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Omap4GpioDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Omap4GpioDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Omap4GpioDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Omap4GpioDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Omap4GpioDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
Omap4GpioProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
Omap4GpioStartDevice (
    PIRP Irp,
    POMAP4_GPIO_CONTROLLER Device
    );

VOID
Omap4GpioEnableController (
    POMAP4_GPIO_CONTROLLER Controller
    );

KSTATUS
Omap4GpioSetConfiguration (
    PVOID Context,
    ULONG Pin,
    PGPIO_PIN_CONFIGURATION Configuration
    );

KSTATUS
Omap4GpioSetDirection (
    PVOID Context,
    ULONG Pin,
    ULONG Flags
    );

VOID
Omap4GpioSetValue (
    PVOID Context,
    ULONG Pin,
    ULONG Value
    );

ULONG
Omap4GpioGetValue (
    PVOID Context,
    ULONG Pin
    );

KSTATUS
Omap4GpioPrepareForInterrupts (
    PVOID Context
    );

VOID
Omap4GpioInterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

INTERRUPT_CAUSE
Omap4GpioInterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

VOID
Omap4GpioEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Omap4GpioDriver = NULL;

GPIO_FUNCTION_TABLE Omap4GpioFunctionTableTemplate = {
    Omap4GpioSetConfiguration,
    Omap4GpioSetDirection,
    Omap4GpioSetValue,
    Omap4GpioGetValue,
    Omap4GpioPrepareForInterrupts,
    Omap4GpioInterruptMaskLine,
    Omap4GpioInterruptBegin,
    Omap4GpioEndOfInterrupt,
    NULL
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

    This routine is the entry point for the OMAP4 GPIO driver. It registers
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

    Omap4GpioDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Omap4GpioAddDevice;
    FunctionTable.DispatchStateChange = Omap4GpioDispatchStateChange;
    FunctionTable.DispatchOpen = Omap4GpioDispatchOpen;
    FunctionTable.DispatchClose = Omap4GpioDispatchClose;
    FunctionTable.DispatchIo = Omap4GpioDispatchIo;
    FunctionTable.DispatchSystemControl = Omap4GpioDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Omap4GpioAddDevice (
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

    POMAP4_GPIO_CONTROLLER Controller;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(OMAP4_GPIO_CONTROLLER),
                                        OMAP4_GPIO_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(OMAP4_GPIO_CONTROLLER));
    Controller->OsDevice = DeviceToken;
    Controller->InterruptHandle = INVALID_HANDLE;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, Controller);
    return Status;
}

VOID
Omap4GpioDispatchStateChange (
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
            Status = Omap4GpioProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Omap4GpioDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Omap4GpioStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Omap4GpioDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Omap4GpioDispatchOpen (
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
Omap4GpioDispatchClose (
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
Omap4GpioDispatchIo (
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
Omap4GpioDispatchSystemControl (
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
Omap4GpioProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an OMAP4 GPIO controller. It adds an interrupt vector requirement
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
Omap4GpioStartDevice (
    PIRP Irp,
    POMAP4_GPIO_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the OMAP4 GPIO device.

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
        Registration.LineCount = OMAP4_GPIO_LINE_COUNT;
        Registration.Features = GPIO_FEATURE_INTERRUPTS;
        RtlCopyMemory(&(Registration.FunctionTable),
                      &Omap4GpioFunctionTableTemplate,
                      sizeof(GPIO_FUNCTION_TABLE));

        Status = GpioCreateController(&Registration, &(Device->GpioController));
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    //
    // Start up the controller.
    //

    Omap4GpioEnableController(Device);
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

VOID
Omap4GpioEnableController (
    POMAP4_GPIO_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine makes sure that a GPIO controller is enabled and active.

Arguments:

    Controller - Supplies a pointer to the controller to initialize.

Return Value:

    None.

--*/

{

    ULONG Value;

    Value = OMAP4_GPIO_SYS_CONFIG_NO_IDLE;
    OMAP4_WRITE_GPIO(Controller, Omap4GpioSysConfig, Value);
    Value = OMAP4_READ_GPIO(Controller, Omap4GpioControl);
    Value &= ~OMAP4_GPIO_CONTROL_DISABLE_MODULE;
    OMAP4_WRITE_GPIO(Controller, Omap4GpioControl, Value);
    return;
}

KSTATUS
Omap4GpioSetConfiguration (
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

    POMAP4_GPIO_CONTROLLER Controller;
    ULONG Debounce;
    ULONG Flags;
    ULONG PinMask;
    OMAP4_GPIO_REGISTER Register;
    ULONG Value;

    Controller = Context;
    PinMask = 1 << Pin;
    Flags = Configuration->Flags;

    //
    // Disable this pin as an interrupt source while it's being configured.
    //

    OMAP4_WRITE_GPIO(Controller, Omap4GpioIrqStatusClear0, PinMask);

    //
    // Set up debouncing if requested.
    //

    Value = OMAP4_READ_GPIO(Controller, Omap4GpioDebounceEnable);
    Value &= ~PinMask;
    if ((Flags & GPIO_ENABLE_DEBOUNCE) != 0) {
        Value |= PinMask;
        if (Configuration->DebounceTimeout != GPIO_DEBOUNCE_TIMEOUT_DEFAULT) {
            Debounce = Configuration->DebounceTimeout /
                       OMAP4_GPIO_DEBOUNCE_GRANULARITY;

            if (Debounce > OMAP4_GPIO_MAX_DEBOUNCE) {
                Debounce = OMAP4_GPIO_MAX_DEBOUNCE;
            }

            OMAP4_WRITE_GPIO(Controller, Omap4GpioDebouncingTime, Debounce);
        }
    }

    OMAP4_WRITE_GPIO(Controller, Omap4GpioDebounceEnable, Value);

    //
    // Potentially configure the pin as an output.
    //

    if ((Flags & GPIO_OUTPUT) != 0) {
        Register = Omap4GpioClearDataOut;
        if ((Flags & GPIO_OUTPUT_HIGH) != 0) {
            Register = Omap4GpioSetDataOut;
        }

        OMAP4_WRITE_GPIO(Controller, Register, PinMask);
        Value = OMAP4_READ_GPIO(Controller, Omap4GpioOutputEnable);
        Value &= ~PinMask;
        OMAP4_WRITE_GPIO(Controller, Omap4GpioOutputEnable, Value);

    //
    // This pin is configured for input.
    //

    } else {
        Value = OMAP4_READ_GPIO(Controller, Omap4GpioOutputEnable);
        Value |= PinMask;
        OMAP4_WRITE_GPIO(Controller, Omap4GpioOutputEnable, Value);

        //
        // Configure the interrupt configuration if the line is configured as
        // an interrupt.
        //

        if ((Flags & GPIO_INTERRUPT) != 0) {
            if ((Flags & GPIO_INTERRUPT_EDGE_TRIGGERED) != 0) {
                Value = OMAP4_READ_GPIO(Controller, Omap4GpioRisingDetect);
                Value &= ~PinMask;
                if ((Flags & GPIO_INTERRUPT_RISING_EDGE) != 0) {
                    Value |= PinMask;
                }

                OMAP4_WRITE_GPIO(Controller, Omap4GpioRisingDetect, Value);
                Value = OMAP4_READ_GPIO(Controller, Omap4GpioFallingDetect);
                Value &= ~PinMask;
                if ((Flags & GPIO_INTERRUPT_FALLING_EDGE) != 0) {
                    Value |= PinMask;
                }

                OMAP4_WRITE_GPIO(Controller, Omap4GpioFallingDetect, Value);

            //
            // This is a level-triggered interrupt.
            //

            } else {
                Value = OMAP4_READ_GPIO(Controller, Omap4GpioLevelDetect0);
                Value &= ~PinMask;
                if ((Flags & GPIO_INTERRUPT_ACTIVE_LOW) != 0) {
                    Value |= PinMask;
                }

                OMAP4_WRITE_GPIO(Controller, Omap4GpioLevelDetect0, Value);
            }

            //
            // Enable the interrupt source.
            //

            OMAP4_WRITE_GPIO(Controller, Omap4GpioIrqStatusSet0, PinMask);
        }
    }

    //
    // Set the pin as a wake source if requested.
    //

    Value = OMAP4_READ_GPIO(Controller, Omap4GpioIrqWakeEnable0);
    Value &= ~PinMask;
    if ((Flags & GPIO_INTERRUPT_WAKE) != 0) {
        Value |= PinMask;
    }

    OMAP4_WRITE_GPIO(Controller, Omap4GpioIrqWakeEnable0, Value);

    //
    // Pull up and pull down configuration is not handled by this module.
    //

    return STATUS_SUCCESS;
}

KSTATUS
Omap4GpioSetDirection (
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

    POMAP4_GPIO_CONTROLLER Controller;
    ULONG OutputEnable;
    ULONG PinMask;
    OMAP4_GPIO_REGISTER Register;

    Controller = Context;
    PinMask = 1 << Pin;
    OutputEnable = OMAP4_READ_GPIO(Controller, Omap4GpioOutputEnable);

    //
    // Potentially configure the pin as an output.
    //

    if ((Flags & GPIO_OUTPUT) != 0) {
        Register = Omap4GpioClearDataOut;
        if ((Flags & GPIO_OUTPUT_HIGH) != 0) {
            Register = Omap4GpioSetDataOut;
        }

        OMAP4_WRITE_GPIO(Controller, Register, PinMask);
        OutputEnable &= ~PinMask;

    //
    // This pin is configured for input.
    //

    } else {
        OutputEnable |= PinMask;
    }

    OMAP4_WRITE_GPIO(Controller, Omap4GpioOutputEnable, OutputEnable);
    return STATUS_SUCCESS;
}

VOID
Omap4GpioSetValue (
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

    POMAP4_GPIO_CONTROLLER Controller;
    ULONG PinMask;
    OMAP4_GPIO_REGISTER Register;

    Controller = Context;
    PinMask = 1 << Pin;
    Register = Omap4GpioClearDataOut;
    if (Value != 0) {
        Register = Omap4GpioSetDataOut;
    }

    OMAP4_WRITE_GPIO(Controller, Register, PinMask);
    return;
}

ULONG
Omap4GpioGetValue (
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

    POMAP4_GPIO_CONTROLLER Controller;
    ULONG PinMask;
    ULONG Value;

    Controller = Context;
    PinMask = 1 << Pin;
    Value = OMAP4_READ_GPIO(Controller, Omap4GpioDataIn);
    return ((Value & PinMask) != 0);
}

KSTATUS
Omap4GpioPrepareForInterrupts (
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

    POMAP4_GPIO_CONTROLLER Controller;

    //
    // Mask all interrupts.
    //

    Controller = Context;
    OMAP4_WRITE_GPIO(Controller, Omap4GpioIrqStatusClear0, 0xFFFFFFFF);
    return STATUS_SUCCESS;
}

VOID
Omap4GpioInterruptMaskLine (
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

    POMAP4_GPIO_CONTROLLER Controller;
    ULONG PinMask;
    OMAP4_GPIO_REGISTER Register;

    Controller = Context;
    PinMask = 1 << Line->U.Local.Line;
    Register = Omap4GpioIrqStatusClear0;
    if (Enable != FALSE) {
        Register = Omap4GpioIrqStatusSet0;
    }

    OMAP4_WRITE_GPIO(Controller, Register, PinMask);
    return;
}

INTERRUPT_CAUSE
Omap4GpioInterruptBegin (
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

    POMAP4_GPIO_CONTROLLER Controller;
    ULONG Line;
    ULONG Value;

    Controller = Context;
    Value = OMAP4_READ_GPIO(Controller, Omap4GpioIrqStatus0);
    if (Value == 0) {
        return InterruptCauseNoInterruptHere;
    }

    Line = RtlCountTrailingZeros32(Value);
    FiringLine->Type = InterruptLineControllerSpecified;
    FiringLine->U.Local.Controller = (UINTN)(Controller->OsDevice);
    FiringLine->U.Local.Line = Line;
    *MagicCandy = 1 << Line;
    return InterruptCauseLineFired;
}

VOID
Omap4GpioEndOfInterrupt (
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

    POMAP4_GPIO_CONTROLLER Controller;

    Controller = Context;
    OMAP4_WRITE_GPIO(Controller, Omap4GpioIrqStatus0, MagicCandy);
    return;
}

