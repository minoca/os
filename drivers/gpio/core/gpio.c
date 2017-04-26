/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gpio.c

Abstract:

    This module implements support for the core GPIO library driver.

Author:

    Evan Green 4-Aug-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "gpiop.h"

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
GpioDriverUnload (
    PVOID Driver
    );

KSTATUS
GpioOpenPin (
    PGPIO_ACCESS_INTERFACE Interface,
    ULONG Pin,
    PGPIO_PIN_HANDLE PinHandle
    );

VOID
GpioClosePin (
    PGPIO_ACCESS_INTERFACE Interface,
    GPIO_PIN_HANDLE PinHandle
    );

KSTATUS
GpioPinSetConfiguration (
    GPIO_PIN_HANDLE PinHandle,
    PGPIO_PIN_CONFIGURATION Configuration
    );

KSTATUS
GpioPinSetDirection (
    GPIO_PIN_HANDLE PinHandle,
    ULONG Flags
    );

VOID
GpioPinSetValue (
    GPIO_PIN_HANDLE PinHandle,
    ULONG Value
    );

ULONG
GpioPinGetValue (
    GPIO_PIN_HANDLE PinHandle
    );

KSTATUS
GpioPrepareForInterrupts (
    PVOID Context
    );

KSTATUS
GpioSetInterruptLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    );

VOID
GpioInterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

INTERRUPT_CAUSE
GpioInterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

VOID
GpioEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    );

KSTATUS
GpioRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    );

//
// -------------------------------------------------------------------- Globals
//

UUID GpioInterfaceUuid = UUID_GPIO_ACCESS;

GPIO_ACCESS_INTERFACE GpioInterfaceTemplate = {
    NULL,
    GpioOpenPin,
    GpioClosePin,
    GpioPinSetConfiguration,
    GpioPinSetDirection,
    GpioPinSetValue,
    GpioPinGetValue
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

    This routine implements the initial entry point of the GPIO core
    library, called when the library is first loaded.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    Status code.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.Unload = GpioDriverUnload;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

GPIO_API
KSTATUS
GpioCreateController (
    PGPIO_CONTROLLER_INFORMATION Registration,
    PGPIO_CONTROLLER *Controller
    )

/*++

Routine Description:

    This routine creates a new GPIO controller.

Arguments:

    Registration - Supplies a pointer to the host registration information.

    Controller - Supplies a pointer where a pointer to the new controller will
        be returned on success.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PGPIO_CONTROLLER NewController;
    KSTATUS Status;

    if ((Registration->Version < GPIO_CONTROLLER_INFORMATION_VERSION) ||
        (Registration->Version > GPIO_CONTROLLER_INFORMATION_MAX_VERSION) ||
        (Registration->LineCount == 0) ||
        (Registration->LineCount > GPIO_MAX_LINES) ||
        (Registration->Device == NULL)) {

        return STATUS_INVALID_PARAMETER;
    }

    if ((Registration->Features & GPIO_FEATURE_LOW_RUN_LEVEL) != 0) {
        if ((Registration->FunctionTable.PrepareForInterrupts == NULL) ||
            (Registration->FunctionTable.MaskInterruptLine == NULL) ||
            (Registration->FunctionTable.BeginInterrupt == NULL) ||
            (Registration->FunctionTable.EndOfInterrupt == NULL)) {

            return STATUS_INVALID_PARAMETER;
        }
    }

    AllocationSize = sizeof(GPIO_CONTROLLER) +
                     (Registration->LineCount * sizeof(GPIO_PIN_CONFIGURATION));

    NewController = MmAllocateNonPagedPool(AllocationSize, GPIO_ALLOCATION_TAG);
    if (NewController == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateControllerEnd;
    }

    RtlZeroMemory(NewController, AllocationSize);
    NewController->Magic = GPIO_CONTROLLER_MAGIC;
    NewController->InterruptLine = -1ULL;

    //
    // It's not yet known what runlevel the device will consume, so set it to
    // the highest possible value to synchronize with an interrupt that
    // comes in before the start controller routine has been fully processed.
    //

    NewController->RunLevel = RunLevelMaxDevice;
    KeInitializeSpinLock(&(NewController->SpinLock));

    //
    // If the controller does not have interrupts or can only be accessed at
    // low runlevel, then used a queued lock for synchronization.
    //

    if (((Registration->Features & GPIO_FEATURE_INTERRUPTS) == 0) ||
        ((Registration->Features & GPIO_FEATURE_LOW_RUN_LEVEL) != 0)) {

        NewController->QueuedLock = KeCreateQueuedLock();
        if (NewController->QueuedLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateControllerEnd;
        }

        NewController->RunLevel = RunLevelLow;
    }

    NewController->Pins = (PGPIO_PIN_CONFIGURATION)(NewController + 1);
    RtlCopyMemory(&(NewController->Host),
                  Registration,
                  sizeof(GPIO_CONTROLLER_INFORMATION));

    RtlCopyMemory(&(NewController->Interface),
                  &GpioInterfaceTemplate,
                  sizeof(GPIO_ACCESS_INTERFACE));

    INITIALIZE_LIST_HEAD(&(NewController->Interface.Handles));
    Status = STATUS_SUCCESS;

CreateControllerEnd:
    if (!KSUCCESS(Status)) {
        if (NewController != NULL) {
            if (NewController->QueuedLock != NULL) {
                KeDestroyQueuedLock(NewController->QueuedLock);
            }

            MmFreeNonPagedPool(NewController);
            NewController = NULL;
        }
    }

    *Controller = NewController;
    return Status;
}

GPIO_API
VOID
GpioDestroyController (
    PGPIO_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys a GPIO controller.

Arguments:

    Controller - Supplies a pointer to the controller to tear down.

Return Value:

    None.

--*/

{

    if (Controller->InterruptController != NULL) {
        HlDestroyInterruptController(Controller->InterruptController);
        Controller->InterruptController = NULL;
    }

    if (Controller->QueuedLock != NULL) {
        KeDestroyQueuedLock(Controller->QueuedLock);
        Controller->QueuedLock = NULL;
    }

    //
    // Ruin the magic (but in a way that's still identifiable to a human).
    //

    Controller->Magic += 1;
    Controller->Pins = NULL;
    MmFreeNonPagedPool(Controller);
    return;
}

GPIO_API
KSTATUS
GpioStartController (
    PGPIO_CONTROLLER Controller,
    ULONGLONG InterruptLine,
    ULONGLONG InterruptVector
    )

/*++

Routine Description:

    This routine starts a GPIO controller. This routine should be serialized
    externally, as it does not acquire the internal controller lock. Calling
    it from the start IRP is sufficient.

Arguments:

    Controller - Supplies a pointer to the controller.

    InterruptLine - Supplies the Global System Interrupt number of the
        interrupt line that this controller is wired to. That is, the GSI of
        the line this controller tickles when it wants to generate an interrupt.
        Set to -1ULL if the controller has no interrupt resources.

    InterruptVector - Supplies the interrupt vector number of the interrupt
        line that this controller is wired to. Set to RunLevelLow if this GPIO
        controller does not support interrupts.

Return Value:

    Status code.

--*/

{

    PGPIO_CONTROLLER_INFORMATION Host;
    INTERRUPT_CONTROLLER_INFORMATION Information;
    INTERRUPT_CONTROLLER_DESCRIPTION Registration;
    KSTATUS Status;

    ASSERT(Controller->Interface.Public.Context == NULL);

    Host = &(Controller->Host);
    Controller->Interface.Public.Context = &(Controller->Interface);
    Status = IoCreateInterface(&GpioInterfaceUuid,
                               Host->Device,
                               &(Controller->Interface),
                               sizeof(GPIO_ACCESS_INTERFACE));

    if (!KSUCCESS(Status)) {
        Controller->Interface.Public.Context = NULL;
        goto StartControllerEnd;
    }

    //
    // Create a resource arbiter for these pins so that other devices can
    // allocate them as part of their official resource requirements.
    //

    if (Controller->ArbiterCreated == FALSE) {
        Status = IoCreateResourceArbiter(Host->Device, ResourceTypeGpio);
        if ((!KSUCCESS(Status)) && (Status != STATUS_ALREADY_INITIALIZED)) {
            goto StartControllerEnd;
        }

        Status = IoAddFreeSpaceToArbiter(Host->Device,
                                         ResourceTypeGpio,
                                         0,
                                         Host->LineCount,
                                         0,
                                         NULL,
                                         0);

        if (!KSUCCESS(Status)) {
            goto StartControllerEnd;
        }

        Controller->ArbiterCreated = TRUE;
    }

    //
    // Create the interrupt controller. Wire the interrupt controller functions
    // directly to the host and avoid interfering.
    //

    Controller->InterruptLine = InterruptLine;
    Controller->InterruptVector = InterruptVector;
    if (((Host->Features & GPIO_FEATURE_INTERRUPTS) != 0) &&
        (Controller->InterruptController == NULL) &&
        (Controller->InterruptLine != -1ULL)) {

        RtlZeroMemory(&Registration, sizeof(INTERRUPT_CONTROLLER_DESCRIPTION));
        Registration.TableVersion = INTERRUPT_CONTROLLER_DESCRIPTION_VERSION;
        Registration.FunctionTable.InitializeIoUnit = GpioPrepareForInterrupts;
        Registration.FunctionTable.SetLineState = GpioSetInterruptLineState;
        Registration.FunctionTable.MaskLine = GpioInterruptMaskLine;
        Registration.FunctionTable.BeginInterrupt = GpioInterruptBegin;
        Registration.FunctionTable.EndOfInterrupt = GpioEndOfInterrupt;
        if (Host->FunctionTable.RequestInterrupt != NULL) {
            Registration.FunctionTable.RequestInterrupt = GpioRequestInterrupt;
        }

        if ((Host->Features & GPIO_FEATURE_LOW_RUN_LEVEL) != 0) {
            Registration.Flags |= INTERRUPT_FEATURE_LOW_RUN_LEVEL;

        } else {

            //
            // Reset the controller's runlevel to the maximum it could be.
            //

            Controller->RunLevel = RunLevelMaxDevice;
        }

        Registration.Context = Controller;

        //
        // Set the identifier to the device pointer by convention so ACPI can
        // find this interrupt controller to get the resulting GSI base. It
        // needs that information to convert a GPIO interrupt resource
        // descriptor into a generic interrupt resource requirement.
        //

        Registration.Identifier = (UINTN)(Host->Device);
        Status = HlCreateInterruptController(Controller->InterruptLine,
                                             Controller->InterruptVector,
                                             Host->LineCount,
                                             &Registration,
                                             &Information);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("GPIO: Failed to create interrupt controller: %d\n",
                          Status);

            goto StartControllerEnd;
        }

        Controller->InterruptController = Information.Controller;
        Controller->GsiBase = Information.StartingGsi;
    }

StartControllerEnd:
    return Status;
}

GPIO_API
VOID
GpioStopController (
    PGPIO_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine stops a GPIO controller. This routine should be serialized
    externally, as it does not acquire the internal GPIO lock. Calling this
    routine from state change IRPs should be sufficient.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    ASSERT(Controller->Interface.Public.Context == &(Controller->Interface));

    Status = IoDestroyInterface(&GpioInterfaceUuid,
                                Controller->Host.Device,
                                &(Controller->Interface));

    ASSERT(KSUCCESS(Status));

    Controller->Interface.Public.Context = NULL;

    ASSERT(LIST_EMPTY(&(Controller->Interface.Handles)));

    return;
}

GPIO_API
VOID
GpioSetInterruptRunLevel (
    PGPIO_CONTROLLER Controller,
    RUNLEVEL RunLevel
    )

/*++

Routine Description:

    This routine sets the internal runlevel of the GPIO lock.

Arguments:

    Controller - Supplies a pointer to the controller.

    RunLevel - Supplies the runlevel that interrupts come in on for this
        controller.

Return Value:

    None.

--*/

{

    ASSERT(Controller->RunLevel >= RunLevel);

    Controller->RunLevel = RunLevel;
    return;
}

GPIO_API
INTERRUPT_STATUS
GpioInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents the GPIO controller interrupt service routine.
    It should be connected by GPIO controllers that can generate interrupts.

Arguments:

    Context - Supplies the context given to the connect interrupt routine,
        which must in this case be the GPIO controller pointer.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    PGPIO_CONTROLLER Controller;
    INTERRUPT_STATUS Result;

    Controller = Context;
    Result = HlSecondaryInterruptControllerService(
                                              Controller->InterruptController);

    return Result;
}

GPIO_API
RUNLEVEL
GpioLockController (
    PGPIO_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine acquires the GPIO controller lock. This routine is called
    automatically by most interface routines.

Arguments:

    Controller - Supplies a pointer to the GPIO controller to acquire the lock
        for.

Return Value:

    Returns the original runlevel, as this routine may have raised the runlevel.

--*/

{

    RUNLEVEL OldRunLevel;

    if (Controller->QueuedLock != NULL) {

        ASSERT((Controller->RunLevel == RunLevelLow) &&
               (KeGetRunLevel() == RunLevelLow));

        OldRunLevel = RunLevelLow;
        KeAcquireQueuedLock(Controller->QueuedLock);

    } else {

        ASSERT(Controller->RunLevel >= RunLevelDispatch);

        OldRunLevel = KeRaiseRunLevel(Controller->RunLevel);
        KeAcquireSpinLock(&(Controller->SpinLock));
    }

    return OldRunLevel;
}

GPIO_API
VOID
GpioUnlockController (
    PGPIO_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    )

/*++

Routine Description:

    This routine releases the GPIO controller lock. This routine is called
    automatically by most interface routines.

Arguments:

    Controller - Supplies a pointer to the GPIO controller to release the lock
        for.

    OldRunLevel - Supplies the original runlevel to return to, as returned by
        the acquire lock function.

Return Value:

    None.

--*/

{

    if (Controller->QueuedLock != NULL) {

        ASSERT((Controller->RunLevel == RunLevelLow) &&
               (KeGetRunLevel() == RunLevelLow));

        KeReleaseQueuedLock(Controller->QueuedLock);

    } else {
        KeReleaseSpinLock(&(Controller->SpinLock));
        KeLowerRunLevel(OldRunLevel);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
GpioDriverUnload (
    PVOID Driver
    )

/*++

Routine Description:

    This routine is called before a driver is about to be unloaded from memory.
    The driver should take this opportunity to free any resources it may have
    set up in the driver entry routine.

Arguments:

    Driver - Supplies a pointer to the driver being torn down.

Return Value:

    None.

--*/

{

    return;
}

KSTATUS
GpioOpenPin (
    PGPIO_ACCESS_INTERFACE Interface,
    ULONG Pin,
    PGPIO_PIN_HANDLE PinHandle
    )

/*++

Routine Description:

    This routine opens a new connection to a GPIO pin.

Arguments:

    Interface - Supplies a pointer to the interface handle.

    Pin - Supplies the zero-based pin number to open.

    PinHandle - Supplies a pointer where a GPIO pin handle will be returned on
        success.

Return Value:

    Status code.

--*/

{

    PGPIO_CONTROLLER Controller;
    PGPIO_PIN_HANDLE_DATA Handle;
    RUNLEVEL OldRunLevel;
    PGPIO_INTERFACE PrivateInterface;
    KSTATUS Status;

    PrivateInterface = Interface->Context;
    Handle = MmAllocateNonPagedPool(sizeof(GPIO_PIN_HANDLE_DATA),
                                    GPIO_ALLOCATION_TAG);

    if (Handle == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Controller = PARENT_STRUCTURE(PrivateInterface, GPIO_CONTROLLER, Interface);
    OldRunLevel = GpioLockController(Controller);

    ASSERT(Controller->Magic == GPIO_CONTROLLER_MAGIC);

    if (Pin >= Controller->Host.LineCount) {
        Status = STATUS_INVALID_PARAMETER;
        goto OpenPinEnd;
    }

    if ((Controller->Pins[Pin].Flags & GPIO_PIN_ACQUIRED) != 0) {
        Status = STATUS_RESOURCE_IN_USE;
        goto OpenPinEnd;
    }

    RtlZeroMemory(Handle, sizeof(GPIO_PIN_HANDLE_DATA));
    Handle->Magic = GPIO_PIN_HANDLE_MAGIC;
    Handle->Interface = PrivateInterface;
    Handle->Controller = Controller;
    Handle->Pin = Pin;
    INSERT_BEFORE(&(Handle->ListEntry), &(PrivateInterface->Handles));
    Controller->Pins[Pin].Flags |= GPIO_PIN_ACQUIRED;
    Status = STATUS_SUCCESS;

OpenPinEnd:
    GpioUnlockController(Controller, OldRunLevel);
    if (!KSUCCESS(Status)) {
        if (Handle != NULL) {
            MmFreeNonPagedPool(Handle);
            Handle = NULL;
        }
    }

    *PinHandle = (GPIO_PIN_HANDLE)Handle;
    return Status;
}

VOID
GpioClosePin (
    PGPIO_ACCESS_INTERFACE Interface,
    GPIO_PIN_HANDLE PinHandle
    )

/*++

Routine Description:

    This routine closes a previously opened GPIO pin handle.

Arguments:

    Interface - Supplies a pointer to the interface handle.

    PinHandle - Supplies the GPIO pin handle to close.

Return Value:

    None.

--*/

{

    PGPIO_CONTROLLER Controller;
    PGPIO_PIN_HANDLE_DATA Handle;
    RUNLEVEL OldRunLevel;

    Handle = (PGPIO_PIN_HANDLE_DATA)PinHandle;

    ASSERT(Handle->Magic == GPIO_PIN_HANDLE_MAGIC);
    ASSERT(Handle->ListEntry.Next != NULL);

    Controller = Handle->Controller;
    OldRunLevel = GpioLockController(Controller);
    LIST_REMOVE(&(Handle->ListEntry));
    Handle->ListEntry.Next = NULL;
    Controller->Pins[Handle->Pin].Flags &= ~GPIO_PIN_ACQUIRED;
    Handle->Magic += 1;
    GpioUnlockController(Controller, OldRunLevel);
    MmFreeNonPagedPool(Handle);
    return;
}

KSTATUS
GpioPinSetConfiguration (
    GPIO_PIN_HANDLE PinHandle,
    PGPIO_PIN_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine sets the complete configuration for a GPIO pin.

Arguments:

    PinHandle - Supplies the pin handle previously opened with the open pin
        interface function.

    Configuration - Supplies a pointer to the new configuration to set.

Return Value:

    Status code.

--*/

{

    PGPIO_CONTROLLER Controller;
    PGPIO_PIN_HANDLE_DATA Handle;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    Handle = (PGPIO_PIN_HANDLE_DATA)PinHandle;

    ASSERT(Handle->Magic == GPIO_PIN_HANDLE_MAGIC);

    Controller = Handle->Controller;
    OldRunLevel = GpioLockController(Controller);
    Host = &(Controller->Host);
    Status = Host->FunctionTable.SetConfiguration(Host->Context,
                                                  Handle->Pin,
                                                  Configuration);

    if (KSUCCESS(Status)) {
        RtlCopyMemory(&(Controller->Pins[Handle->Pin]),
                      Configuration,
                      sizeof(GPIO_PIN_CONFIGURATION));

        Controller->Pins[Handle->Pin].Flags |=
                                       GPIO_PIN_ACQUIRED | GPIO_PIN_CONFIGURED;
    }

    GpioUnlockController(Controller, OldRunLevel);
    return Status;
}

KSTATUS
GpioPinSetDirection (
    GPIO_PIN_HANDLE PinHandle,
    ULONG Flags
    )

/*++

Routine Description:

    This routine sets the complete configuration for an open GPIO pin.

Arguments:

    PinHandle - Supplies the pin handle previously opened with the open pin
        interface function.

    Flags - Supplies the GPIO_* configuration flags to set. Only GPIO_OUTPUT
        and GPIO_OUTPUT_HIGH are observed, all other flags are ignored.

Return Value:

    Status code.

--*/

{

    PGPIO_CONTROLLER Controller;
    PGPIO_PIN_HANDLE_DATA Handle;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    Handle = (PGPIO_PIN_HANDLE_DATA)PinHandle;

    ASSERT(Handle->Magic == GPIO_PIN_HANDLE_MAGIC);

    Controller = Handle->Controller;
    OldRunLevel = GpioLockController(Controller);
    Host = &(Controller->Host);
    Status = Host->FunctionTable.SetDirection(Host->Context,
                                              Handle->Pin,
                                              Flags);

    GpioUnlockController(Controller, OldRunLevel);
    return Status;
}

VOID
GpioPinSetValue (
    GPIO_PIN_HANDLE PinHandle,
    ULONG Value
    )

/*++

Routine Description:

    This routine sets the output value on a GPIO pin.

Arguments:

    PinHandle - Supplies the pin handle previously opened with the open pin
        interface function.

    Value - Supplies the value to set on the pin: zero for low, non-zero for
        high.

Return Value:

    None.

--*/

{

    PGPIO_CONTROLLER Controller;
    PGPIO_PIN_HANDLE_DATA Handle;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;

    Handle = (PGPIO_PIN_HANDLE_DATA)PinHandle;

    ASSERT(Handle->Magic == GPIO_PIN_HANDLE_MAGIC);

    Controller = Handle->Controller;
    OldRunLevel = GpioLockController(Controller);
    Host = &(Controller->Host);
    Host->FunctionTable.SetValue(Host->Context, Handle->Pin, Value);
    GpioUnlockController(Controller, OldRunLevel);
    return;
}

ULONG
GpioPinGetValue (
    GPIO_PIN_HANDLE PinHandle
    )

/*++

Routine Description:

    This routine gets the input value on a GPIO pin.

Arguments:

    PinHandle - Supplies the pin handle previously opened with the open pin
        interface function.

Return Value:

    0 if the value was low.

    1 if the value was high.

    -1 on error.

--*/

{

    PGPIO_CONTROLLER Controller;
    PGPIO_PIN_HANDLE_DATA Handle;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;
    ULONG Result;

    Handle = (PGPIO_PIN_HANDLE_DATA)PinHandle;

    ASSERT(Handle->Magic == GPIO_PIN_HANDLE_MAGIC);

    Controller = Handle->Controller;
    OldRunLevel = GpioLockController(Controller);
    Host = &(Controller->Host);
    Result = Host->FunctionTable.GetValue(Host->Context, Handle->Pin);
    GpioUnlockController(Controller, OldRunLevel);
    return Result;
}

KSTATUS
GpioPrepareForInterrupts (
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

    PGPIO_CONTROLLER Controller;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    Controller = Context;
    Host = &(Controller->Host);
    OldRunLevel = GpioLockController(Controller);
    Status = Host->FunctionTable.PrepareForInterrupts(Host->Context);
    GpioUnlockController(Controller, OldRunLevel);
    return Status;
}

KSTATUS
GpioSetInterruptLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    )

/*++

Routine Description:

    This routine enables or disables and configures an interrupt line.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the line to set up. This will always be a
        controller specified line.

    State - Supplies a pointer to the new configuration of the line.

    ResourceData - Supplies an optional pointer to the device specific resource
        data for the interrupt line.

    ResourceDataSize - Supplies the size of the resource data, in bytes.

Return Value:

    Status code.

--*/

{

    PGPIO_CONTROLLER Controller;
    PRESOURCE_GPIO_DATA GpioData;
    RESOURCE_GPIO_DATA GpioDataBuffer;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;
    PGPIO_PIN_CONFIGURATION Pin;
    KSTATUS Status;

    //
    // Before acquiring the controller lock, touch any paged-pool objects. This
    // includes the resource data. The lock may be acquired at a non-low
    // runlevel.
    //

    GpioData = ResourceData;
    if (GpioData != NULL) {
        if ((ResourceDataSize < sizeof(RESOURCE_GPIO_DATA)) ||
            (GpioData->Version < RESOURCE_GPIO_DATA_VERSION)) {

            return STATUS_VERSION_MISMATCH;
        }

        RtlCopyMemory(&GpioDataBuffer, GpioData, sizeof(RESOURCE_GPIO_DATA));
        GpioData = &GpioDataBuffer;
    }

    Controller = Context;
    OldRunLevel = GpioLockController(Controller);
    Host = &(Controller->Host);
    Pin = &(Controller->Pins[Line->U.Local.Line]);
    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
        Pin->Flags = GPIO_PIN_CONFIGURED | GPIO_PIN_ACQUIRED;

    } else {
        Pin->Flags = GPIO_INTERRUPT | GPIO_PIN_CONFIGURED | GPIO_PIN_ACQUIRED;
        if (State->Mode == InterruptModeEdge) {
            Pin->Flags |= GPIO_INTERRUPT_EDGE_TRIGGERED;
        }

        if (State->Polarity == InterruptActiveHigh) {
            Pin->Flags |= GPIO_INTERRUPT_RISING_EDGE;

        } else if (State->Polarity == InterruptActiveLow) {
            Pin->Flags |= GPIO_INTERRUPT_FALLING_EDGE;

        } else if (State->Polarity == InterruptActiveBoth) {
            Pin->Flags |= GPIO_INTERRUPT_RISING_EDGE |
                          GPIO_INTERRUPT_FALLING_EDGE;
        }

        if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_DEBOUNCE) != 0) {
            Pin->Flags |= GPIO_ENABLE_DEBOUNCE;
        }

        if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_WAKE) != 0) {
            Pin->Flags |= GPIO_INTERRUPT_WAKE;
        }

        if (GpioData != NULL) {
            if ((GpioData->Flags & RESOURCE_GPIO_PULL_NONE) ==
                 RESOURCE_GPIO_PULL_NONE) {

                Pin->Flags |= GPIO_PULL_NONE;

            } else if ((GpioData->Flags & RESOURCE_GPIO_PULL_UP) != 0) {
                Pin->Flags |= GPIO_PULL_UP;

            } else if ((GpioData->Flags & RESOURCE_GPIO_PULL_DOWN) != 0) {
                Pin->Flags |= GPIO_PULL_DOWN;
            }

            Pin->OutputDriveStrength = GpioData->OutputDriveStrength;
            Pin->DebounceTimeout = GpioData->DebounceTimeout;
        }
    }

    Status = Host->FunctionTable.SetConfiguration(Host->Context,
                                                  Line->U.Local.Line,
                                                  Pin);

    if (!KSUCCESS(Status)) {
        Pin->Flags &= ~GPIO_PIN_CONFIGURED;
    }

    GpioUnlockController(Controller, OldRunLevel);
    return Status;
}

VOID
GpioInterruptMaskLine (
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

    PGPIO_CONTROLLER Controller;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;

    Controller = Context;
    OldRunLevel = GpioLockController(Controller);
    Host = &(Controller->Host);
    Host->FunctionTable.MaskInterruptLine(Host->Context, Line, Enable);
    GpioUnlockController(Controller, OldRunLevel);
    return;
}

INTERRUPT_CAUSE
GpioInterruptBegin (
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

    PGPIO_CONTROLLER Controller;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;
    INTERRUPT_CAUSE Result;

    Controller = Context;
    OldRunLevel = GpioLockController(Controller);
    Host = &(Controller->Host);
    Result = Host->FunctionTable.BeginInterrupt(Host->Context,
                                                FiringLine,
                                                MagicCandy);

    GpioUnlockController(Controller, OldRunLevel);
    return Result;
}

VOID
GpioEndOfInterrupt (
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

    PGPIO_CONTROLLER Controller;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;

    Controller = Context;
    Host = &(Controller->Host);
    OldRunLevel = GpioLockController(Controller);
    Host->FunctionTable.EndOfInterrupt(Host->Context, MagicCandy);
    GpioUnlockController(Controller, OldRunLevel);
    return;
}

KSTATUS
GpioRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    )

/*++

Routine Description:

    This routine requests a hardware interrupt on the given line.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the interrupt line to spark.

    Vector - Supplies the vector to generate the interrupt on (for vectored
        architectures only).

    Target - Supplies a pointer to the set of processors to target.

Return Value:

    STATUS_SUCCESS on success.

    Error code on failure.

--*/

{

    PGPIO_CONTROLLER Controller;
    PGPIO_CONTROLLER_INFORMATION Host;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    Controller = Context;
    OldRunLevel = GpioLockController(Controller);
    Host = &(Controller->Host);
    Status = Host->FunctionTable.RequestInterrupt(Host->Context,
                                                  Line,
                                                  Vector,
                                                  Target);

    GpioUnlockController(Controller, OldRunLevel);
    return Status;
}

