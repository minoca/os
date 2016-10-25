/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gpiohost.h

Abstract:

    This header contains definitions for creating and managing new GPIO
    controllers via the GPIO library.

Author:

    Evan Green 4-Aug-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/gpio/gpio.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifndef GPIO_API

#define GPIO_API __DLLIMPORT

#endif

#define GPIO_CONTROLLER_INFORMATION_VERSION 1

//
// This feature is set if the GPIO controller has interrupts.
//

#define GPIO_FEATURE_INTERRUPTS 0x00000001

//
// This feature is set if access to the GPIO controller can only be done at
// low run level. This is the case for GPIO controllers behind busses like
// I2C and SPI, as that bus I/O cannot be done at interrupt level.
//

#define GPIO_FEATURE_LOW_RUN_LEVEL 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _GPIO_CONTROLLER GPIO_CONTROLLER, *PGPIO_CONTROLLER;

typedef
KSTATUS
(*PGPIO_SET_CONFIGURATION) (
    PVOID Context,
    ULONG Pin,
    PGPIO_PIN_CONFIGURATION Configuration
    );

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

typedef
KSTATUS
(*PGPIO_SET_DIRECTION) (
    PVOID Context,
    ULONG Pin,
    ULONG Flags
    );

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

typedef
VOID
(*PGPIO_SET_VALUE) (
    PVOID Context,
    ULONG Pin,
    ULONG Value
    );

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

typedef
ULONG
(*PGPIO_GET_VALUE) (
    PVOID Context,
    ULONG Pin
    );

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

/*++

Structure Description:

    This structure stores the set of GPIO controller functions called by the
    GPIO library.

Members:

    SetConfiguration - Stores a pointer to a function used to set the
        complete configuration for a GPIO pin.

    SetDirection - Stores a pointer to a function used to configure only the
        input/output direction of a pin.

    SetValue - Stores a pointer to a function used to set the current output
        value of a pin.

    GetValue - Stores a pointer to a function used to get the current input
        value of a pin.

    PrepareForInterrupts - Stores a pointer to a function used to prepare the
        controller to enable and receive interrupts.

    MaskInterruptLine - Stores a pointer to a function used to mask or unmask
        interrupts on the controller without altering other configuration
        properties.

    BeginInterrupt - Stores a pointer to a function used to determine the
        source of an interrupt on the controller.

    EndOfInterrupt - Stores a pointer to a function used to acknowledge a
        completed interrupt. This is optional.

    RequestInterrupt - Stores a pointer to a function used to request an
        interrupt on the line in software. This is optional.

--*/

typedef struct _GPIO_FUNCTION_TABLE {
    PGPIO_SET_CONFIGURATION SetConfiguration;
    PGPIO_SET_DIRECTION SetDirection;
    PGPIO_SET_VALUE SetValue;
    PGPIO_GET_VALUE GetValue;
    PINTERRUPT_INITIALIZE_IO_UNIT PrepareForInterrupts;
    PINTERRUPT_MASK_LINE MaskInterruptLine;
    PINTERRUPT_BEGIN BeginInterrupt;
    PINTERRUPT_END_OF_INTERRUPT EndOfInterrupt;
    PINTERRUPT_REQUEST_INTERRUPT RequestInterrupt;
} GPIO_FUNCTION_TABLE, *PGPIO_FUNCTION_TABLE;

/*++

Structure Description:

    This structure defines the information provided to the GPIO library by a
    GPIO controller.

Members:

    Version - Stores the value GPIO_CONTROLLER_INFORMATION_VERSION, used to
        enable future expansion of this structure.

    Context - Stores an opaque context pointer that is passed to the GPIO
        controller functions.

    Device - Stores a pointer to the OS device associated with this controller.

    LineCount - Stores the number of lines in the interrupt controller.

    Features - Stores a bitfield of features about this GPIO controller. See
        GPIO_FEATURE_* definitions.

    Function - Stores the function table of functions the library uses to call
        back into the controller.

--*/

typedef struct _GPIO_CONTROLLER_INFORMATION {
    ULONG Version;
    PVOID Context;
    PDEVICE Device;
    ULONG LineCount;
    ULONG Features;
    GPIO_FUNCTION_TABLE FunctionTable;
} GPIO_CONTROLLER_INFORMATION, *PGPIO_CONTROLLER_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

GPIO_API
KSTATUS
GpioCreateController (
    PGPIO_CONTROLLER_INFORMATION Registration,
    PGPIO_CONTROLLER *Controller
    );

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

GPIO_API
VOID
GpioDestroyController (
    PGPIO_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys a GPIO controller.

Arguments:

    Controller - Supplies a pointer to the controller to tear down.

Return Value:

    None.

--*/

GPIO_API
KSTATUS
GpioStartController (
    PGPIO_CONTROLLER Controller,
    ULONGLONG InterruptLine,
    ULONGLONG InterruptVector
    );

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

GPIO_API
VOID
GpioStopController (
    PGPIO_CONTROLLER Controller
    );

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

GPIO_API
VOID
GpioSetInterruptRunLevel (
    PGPIO_CONTROLLER Controller,
    RUNLEVEL RunLevel
    );

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

GPIO_API
INTERRUPT_STATUS
GpioInterruptService (
    PVOID Context
    );

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

GPIO_API
RUNLEVEL
GpioLockController (
    PGPIO_CONTROLLER Controller
    );

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

GPIO_API
VOID
GpioUnlockController (
    PGPIO_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    );

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

