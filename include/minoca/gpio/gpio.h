/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gpio.h

Abstract:

    This header contains definitions for the General Purpose Input/Output
    library and those wanting to consume (use) GPIO resources.

Author:

    Evan Green 4-Aug-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Interface UUID for GPIO access.
//

#define UUID_GPIO_ACCESS \
    {{0x7495B584, 0xC84D4BDB, 0xBCD458A1, 0x5B290E85}}

//
// Define GPIO settings flags.
//

//
// This flag is set to indicate the GPIO should be an output. If this flag is
// not set, then the pin should be set as an input.
//

#define GPIO_OUTPUT 0x00000001

//
// This flag indicates that the initial output state for the GPIO pin should be
// set to high. If this is not set and the GPIO is configured to be an output,
// then the initial state is assumed to be low. This flag is used to prevent
// momentary glitches during configuration.
//

#define GPIO_OUTPUT_HIGH 0x00000002

//
// This flag is set to indicate the GPIO should be enabled as an interrupt
// source. In this case the GPIO output flag will not be set.
//

#define GPIO_INTERRUPT 0x00000004

//
// This flag is set to indicate that the GPIO interrupt is edge triggered. If
// this is clear, then the interrupt is level triggered.
//

#define GPIO_INTERRUPT_EDGE_TRIGGERED 0x00000008

//
// This flag is set to indicate the GPIO interrupt configuration should be set
// for edge triggered mode, and interrupt on the rising edge. If this flag and
// the falling edge flag are not set, then the interrupt should be level
// triggered.
//

#define GPIO_INTERRUPT_RISING_EDGE 0x00000010

//
// This flag is set to indicate the GPIO interrupt should trigger on the
// falling edge. This may be set alone or with the rising edge flag to
// indicate the interrupt should trigger on both rising and falling edges.
//

#define GPIO_INTERRUPT_FALLING_EDGE 0x00000020

//
// This flag is set to indicate interrupts should occur when the GPIO level is
// low. If not set, then the interrupt is active high.
//

#define GPIO_INTERRUPT_ACTIVE_LOW GPIO_INTERRUPT_FALLING_EDGE

//
// This flag is set to enable the internal pull-up resistor in the GPIO pin.
//

#define GPIO_PULL_UP 0x00000040

//
// This flag is set to enable the internal pull-down resistor in the GPIO pin.
//

#define GPIO_PULL_DOWN 0x00000080

//
// This flag is set to disable the interall pull-up and pull-down resistors in
// the GPIO pin.
//

#define GPIO_PULL_NONE 0x000000C0

//
// This flag is set if the GPIO pin should be enabled as a wake source.
//

#define GPIO_INTERRUPT_WAKE 0x00000100

//
// This flag is set to enable debouncing.
//

#define GPIO_ENABLE_DEBOUNCE 0x00000200

//
// Define default values for the output drive strength and debounce timeout.
//

#define GPIO_OUTPUT_DRIVE_DEFAULT (-1)
#define GPIO_DEBOUNCE_TIMEOUT_DEFAULT (-1)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _GPIO_ACCESS_INTERFACE
    GPIO_ACCESS_INTERFACE, *PGPIO_ACCESS_INTERFACE;

typedef PVOID GPIO_PIN_HANDLE, *PGPIO_PIN_HANDLE;

/*++

Structure Description:

    This structure defines the pin configuration for a GPIO pin.

Members:

    Flags - Stores the GPIO pin configuration flags. See GPIO_* definitions.

    OutputDriveStrength - Stores the output drive strength in microamps.

    DebounceTimeout - Stores the interrupt debounce timeout in microseconds.

--*/

typedef struct _GPIO_PIN_CONFIGURATION {
    ULONG Flags;
    ULONG OutputDriveStrength;
    ULONG DebounceTimeout;
} GPIO_PIN_CONFIGURATION, *PGPIO_PIN_CONFIGURATION;

typedef
KSTATUS
(*PGPIO_OPEN_PIN) (
    PGPIO_ACCESS_INTERFACE Interface,
    ULONG Pin,
    PGPIO_PIN_HANDLE PinHandle
    );

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

typedef
VOID
(*PGPIO_CLOSE_PIN) (
    PGPIO_ACCESS_INTERFACE Interface,
    GPIO_PIN_HANDLE PinHandle
    );

/*++

Routine Description:

    This routine closes a previously opened GPIO pin handle.

Arguments:

    Interface - Supplies a pointer to the interface handle.

    PinHandle - Supplies the GPIO pin handle to close.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PGPIO_PIN_SET_CONFIGURATION) (
    GPIO_PIN_HANDLE PinHandle,
    PGPIO_PIN_CONFIGURATION Configuration
    );

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

typedef
KSTATUS
(*PGPIO_PIN_SET_DIRECTION) (
    GPIO_PIN_HANDLE PinHandle,
    ULONG Flags
    );

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

typedef
VOID
(*PGPIO_PIN_SET_VALUE) (
    GPIO_PIN_HANDLE PinHandle,
    ULONG Value
    );

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

typedef
ULONG
(*PGPIO_PIN_GET_VALUE) (
    GPIO_PIN_HANDLE PinHandle
    );

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

/*++

Structure Description:

    This structure defines the interface to a GPIO pin.

Members:

    Context - Stores an opaque pointer to additinal data that the interface
        producer uses to identify this interface instance.

    OpenPin - Stores a pointer to a function used to open a particular pin on
        a GPIO controller.

    ClosePin - Stores a pointer to a function used to close a previously
        opened GPIO pin.

    SetConfiguration - Stores a pointer to a function used to set the pin
        configuration for a GPIO pin.

    SetDirection - Stores a pointer to a function used to set the direction
        of an open GPIO pin.

    SetValue - Stores a pointer to a function used to set the output value of
        a GPIO pin.

    GetValue - Stores a pointer to a function used to get the input value of an
        open GPIO pin.

--*/

struct _GPIO_ACCESS_INTERFACE {
    PVOID Context;
    PGPIO_OPEN_PIN OpenPin;
    PGPIO_CLOSE_PIN ClosePin;
    PGPIO_PIN_SET_CONFIGURATION SetConfiguration;
    PGPIO_PIN_SET_DIRECTION SetDirection;
    PGPIO_PIN_SET_VALUE SetValue;
    PGPIO_PIN_GET_VALUE GetValue;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

