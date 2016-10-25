/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am3usb.h

Abstract:

    This header contains internal definitions for the TI AM33xx USB subsystem
    driver.

Author:

    Evan Green 11-Sep-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/fw/acpitabs.h>
#include <minoca/soc/am335x.h>
#include "musb.h"

//
// ---------------------------------------------------------------- Definitions
//

#define AM3_USB_ALLOCATION_TAG 0x55336D41

//
// Define the number of USB controllers exposed here.
//

#define AM3_USB_COUNT 2

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores controller information for the USBSS region of the
    USB subsystem on the AM33xx.

Members:

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    ControllerBase - Stores the virtual address of the hardware registers.

    CppiDma - Stores a pointer to the CPPI DMA controller.

--*/

typedef struct _AM3_USBSS_CONTROLLER {
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PCPPI_DMA_CONTROLLER CppiDma;
} AM3_USBSS_CONTROLLER, *PAM3_USBSS_CONTROLLER;

/*++

Structure Description:

    This structure stores controller information for the USB control regions.

Members:

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    ControllerBase - Stores the virtual address of the hardware registers.

    MentorUsb - Stores the Mentor Graphics USB controller state.

--*/

typedef struct _AM3_USB_CONTROL {
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    MUSB_CONTROLLER MentorUsb;
} AM3_USB_CONTROL, *PAM3_USB_CONTROL;

/*++

Structure Description:

    This structure stores the information for the USB subsystem on the TI
    AM33xx SoCs.

Members:

    UsbSs - Stores the USBSS controller.

    CppiDma - Stores the CPPI DMA controller.

    Usb - Stores the USB control definitions.

    UsbCore - Stores the two Mentor USB controller contexts.

--*/

typedef struct _AM3_USB_CONTROLLER {
    AM3_USBSS_CONTROLLER UsbSs;
    CPPI_DMA_CONTROLLER CppiDma;
    AM3_USB_CONTROL Usb[AM3_USB_COUNT];
    MUSB_CONTROLLER UsbCore[AM3_USB_COUNT];
} AM3_USB_CONTROLLER, *PAM3_USB_CONTROLLER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
Am3UsbssInitializeControllerState (
    PAM3_USBSS_CONTROLLER Controller,
    PVOID RegisterBase,
    PCPPI_DMA_CONTROLLER CppiDma
    );

/*++

Routine Description:

    This routine initializes data structures for the AM335 USBSS controllers.

Arguments:

    Controller - Supplies a pointer to the controller structure, which has
        already been zeroed.

    RegisterBase - Supplies the virtual address of the registers for the
        device.

    CppiDma - Supplies a pointer to the CPPI DMA controller.

Return Value:

    Status code.

--*/

KSTATUS
Am3UsbssDestroyControllerState (
    PAM3_USBSS_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys the given USBSS controller structure, freeing all
    resources associated with the controller except the controller structure
    itself and the register base, which were passed in on initialize.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    Status code.

--*/

KSTATUS
Am3UsbssResetController (
    PAM3_USBSS_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine performs a hardware reset and initialization on USBSS.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
Am3UsbssInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the USBSS interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the EHCI
        controller.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
Am3UsbssInterruptServiceDpc (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the USBSS dispatch level interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the EHCI
        controller.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
Am3UsbInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the USB Control interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the EHCI
        controller.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
Am3UsbInterruptServiceDpc (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine implements the AM335 USB dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the controller structure.

Return Value:

    None.

--*/

KSTATUS
Am3UsbControlReset (
    PAM3_USB_CONTROL Controller
    );

/*++

Routine Description:

    This routine performs a hardware reset and initialization of the given
    controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

