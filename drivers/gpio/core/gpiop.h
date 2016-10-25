/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gpiop.h

Abstract:

    This header contains internal definitions for the GPIO library.

Author:

    Evan Green 4-Aug-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#define GPIO_API __DLLEXPORT

#include <minoca/gpio/gpiohost.h>

//
// ---------------------------------------------------------------- Definitions
//

#define GPIO_ALLOCATION_TAG 0x6F697047

#define GPIO_CONTROLLER_MAGIC GPIO_ALLOCATION_TAG
#define GPIO_PIN_HANDLE_MAGIC 0x48697047

#define GPIO_MAX_LINES 1024
#define GPIO_CONTROLLER_INFORMATION_MAX_VERSION 0x0001000

//
// This bit is set in the configuration flags if the pin has been configured
// before.
//

#define GPIO_PIN_CONFIGURED 0x80000000

//
// This bit is set if the GPIO pin is currently open.
//

#define GPIO_PIN_ACQUIRED 0x40000000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the internal data of GPIO interface.

Members:

    Public - Stores the public interface instance.

    Handles - Stores the head of the list of open handles.

--*/

typedef struct _GPIO_INTERFACE {
    GPIO_ACCESS_INTERFACE Public;
    LIST_ENTRY Handles;
} GPIO_INTERFACE, *PGPIO_INTERFACE;

/*++

Structure Description:

    This structure stores the internal data for an open GPIO pin.

Members:

    ListEntry - Stores pointers to the next and previous open handles in the
        interface.

    Magic - Stores the constant value GPIO_PIN_HANDLE_MAGIC.

    Interface - Stores a pointer back to the interface that created this handle.

    Controller - Stores a pointer to the GPIO controller.

    Pin - Stores the pin number that's open.

--*/

typedef struct _GPIO_PIN_HANDLE_DATA {
    LIST_ENTRY ListEntry;
    ULONG Magic;
    PGPIO_INTERFACE Interface;
    PGPIO_CONTROLLER Controller;
    ULONG Pin;
} GPIO_PIN_HANDLE_DATA, *PGPIO_PIN_HANDLE_DATA;

/*++

Structure Description:

    This structure stores the internal data of a GPIO library controller.

Members:

    Magic - Stores the constant GPIO_CONTROLLER_MAGIC.

    Host - Stores the host controller information.

    Pins - Stores a pointer to an array of pin configuration data, one for each
        pin.

    Interface - Stores the GPIO interface presented to the world for use.

    ArbiterCreated - Stores a boolean indicating whether or not the GPIO
        arbiter has been created yet.

    InterruptController - Stores a pointer to the interrupt controller created
        for the GPIO device.

    InterruptLine - Stores the interrupt line that this GPIO controller
        connects to.

    InterruptVector - Stores the interrupt vector that this GPIO controller
        connects to.

    GsiBase - Stores the global system interrupt base of this controller.

    RunLevel - Stores the runlevel for this controller, if using the spin lock.

    SpinLock - Stores the spin lock if this controller has interrupts and is
        can access its registers at interrupt runlevel.

    QueuedLock - Stores a pointer to the queued lock if this controller can
        only be accessed at low runlevel.

--*/

struct _GPIO_CONTROLLER {
    ULONG Magic;
    GPIO_CONTROLLER_INFORMATION Host;
    PGPIO_PIN_CONFIGURATION Pins;
    GPIO_INTERFACE Interface;
    BOOL ArbiterCreated;
    PINTERRUPT_CONTROLLER InterruptController;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    ULONG GsiBase;
    RUNLEVEL RunLevel;
    KSPIN_LOCK SpinLock;
    PQUEUED_LOCK QueuedLock;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

