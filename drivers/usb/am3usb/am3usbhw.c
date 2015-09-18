/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    am3usbhw.c

Abstract:

    This module implements hardware support for portions of the USB subsystem
    on the AM335x SoC.

Author:

    Evan Green 11-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/driver.h>
#include <usbhost.h>
#include "am3usb.h"

//
// --------------------------------------------------------------------- Macros
//

#define AM3_READ_USBSS(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define AM3_WRITE_USBSS(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

#define AM3_READ_USBCTRL(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define AM3_WRITE_USBCTRL(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// The legacy interrupt flag hands interrupts into the Mentor Controller.
//

#define AM3_USB_CONTROL_LEGACY_INTERRUPTS 0x00000008

//
// Define interrupt status bits.
//

#define AM3_USB_INTERRUPT1_MENTOR 0x00000200

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _AM335_USBSS_REGISTER {
    Am3UsbssRevision = 0x000,
    Am3UsbssSysConfig = 0x010,
    Am3UsbssInterruptStatusRaw = 0x024,
    Am3UsbssInterruptStatus = 0x028,
    Am3UsbssInterruptEnableSet = 0x02C,
    Am3UsbssInterruptEnableClear = 0x30,
    Am3UsbssInterruptDmaThresholdTx0 = 0x100,
    Am3UsbssInterruptDmaThresholdRx0 = 0x110,
    Am3UsbssInterruptDmaThresholdTx1 = 0x120,
    Am3UsbssInterruptDmaThresholdRx1 = 0x130,
    Am3UsbssInterruptDmaEnable0 = 0x140,
    Am3UsbssInterruptDmaEnable1 = 0x144,
    Am3UsbssInterruptFrameThresholdTx0 = 0x200,
    Am3UsbssInterruptFrameThresholdRx0 = 0x210,
    Am3UsbssInterruptFrameThresholdTx1 = 0x220,
    Am3UsbssInterruptFrameThresholdRx1 = 0x230,
    Am3UsbssInterruptFrameEnable0 = 0x240,
    Am3UsbssInterruptFrameEnable1 = 0x244,
} AM335_USBSS_REGISTER, *PAM335_USBSS_REGISTER;

typedef enum _AM3_USB_CONTROL_REGISTER {
    Am3UsbRevision = 0x00,
    Am3UsbControl = 0x14,
    Am3UsbStatus = 0x18,
    Am3UsbInterruptMStatus = 0x20,
    Am3UsbInterruptStatusRaw0 = 0x28,
    Am3UsbInterruptStatusRaw1 = 0x2C,
    Am3UsbInterruptStatus0 = 0x30,
    Am3UsbInterruptStatus1 = 0x34,
    Am3UsbInterruptEnableSet0 = 0x38,
    Am3UsbInterruptEnableSet1 = 0x3C,
    Am3UsbInterruptEnableClear0 = 0x40,
    Am3UsbInterruptEnableClear1 = 0x44,
    Am3UsbTxMode = 0x70,
    Am3UsbRxMode = 0x74,
    Am3UsbGenericRndisSize1 = 0x80,
    Am3UsbGenericRndisSize2 = 0x84,
    Am3UsbGenericRndisSize3 = 0x88,
    Am3UsbGenericRndisSize4 = 0x8C,
    Am3UsbGenericRndisSize5 = 0x90,
    Am3UsbGenericRndisSize6 = 0x94,
    Am3UsbGenericRndisSize7 = 0x98,
    Am3UsbGenericRndisSize8 = 0x9C,
    Am3UsbGenericRndisSize9 = 0xA0,
    Am3UsbGenericRndisSize10 = 0xA4,
    Am3UsbGenericRndisSize11 = 0xA8,
    Am3UsbGenericRndisSize12 = 0xAC,
    Am3UsbGenericRndisSize13 = 0xB0,
    Am3UsbGenericRndisSize14 = 0xB4,
    Am3UsbGenericRndisSize15 = 0xB8,
    Am3UsbAutoRequest = 0xD0,
    Am3UsbSrpFixTime = 0xD4,
    Am3UsbTearDown = 0xD8,
    Am3UsbUtmi = 0xE0,
    Am3UsbUtmiLoopback = 0xE4,
    Am3UsbMode = 0xE8
} AM3_USB_CONTROL_REGISTER, *PAM3_USB_CONTROL_REGISTER;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Am3UsbssInitializeControllerState (
    PAM3_USBSS_CONTROLLER Controller,
    PVOID RegisterBase
    )

/*++

Routine Description:

    This routine initializes data structures for the AM335 USBSS controllers.

Arguments:

    Controller - Supplies a pointer to the controller structure, which has
        already been zeroed.

    RegisterBase - Supplies the virtual address of the registers for the
        device.

Return Value:

    Status code.

--*/

{

    Controller->ControllerBase = RegisterBase;
    return STATUS_SUCCESS;
}

KSTATUS
Am3UsbssDestroyControllerState (
    PAM3_USBSS_CONTROLLER Controller
    )

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

{

    Controller->ControllerBase = NULL;
    return STATUS_SUCCESS;
}

KSTATUS
Am3UsbssResetController (
    PAM3_USBSS_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine performs a hardware reset and initialization on USBSS.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    ULONG Value;

    //
    // Initiate soft reset of USBSS, USB0, and USB1.
    //

    Value = AM335_USBSS_SYSCONFIG_SOFT_RESET;
    AM3_WRITE_USBSS(Controller, Am3UsbssSysConfig, Value);
    do {
        Value = AM3_READ_USBSS(Controller, Am3UsbssSysConfig);

    } while ((Value & AM335_USBSS_SYSCONFIG_SOFT_RESET) != 0);

    //
    // Disable all interrupts.
    //

    AM3_WRITE_USBSS(Controller, Am3UsbssInterruptEnableClear, 0xFFFFFFFF);
    AM3_WRITE_USBSS(Controller, Am3UsbssInterruptDmaEnable0, 0);
    AM3_WRITE_USBSS(Controller, Am3UsbssInterruptDmaEnable1, 0);
    AM3_WRITE_USBSS(Controller, Am3UsbssInterruptFrameEnable0, 0);
    AM3_WRITE_USBSS(Controller, Am3UsbssInterruptFrameEnable1, 0);
    return STATUS_SUCCESS;
}

INTERRUPT_STATUS
Am3UsbssInterruptService (
    PVOID Context
    )

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

{

    RtlDebugPrint("UsbSS interrupt!\n");
    return InterruptStatusNotClaimed;
}

INTERRUPT_STATUS
Am3UsbInterruptService (
    PVOID Context
    )

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

{

    PAM3_USB_CONTROL Controller;
    INTERRUPT_STATUS InterruptStatus;
    ULONG Status0;
    ULONG Status1;

    Controller = Context;
    InterruptStatus = InterruptStatusNotClaimed;
    Status0 = AM3_READ_USBCTRL(Controller, Am3UsbInterruptStatus0);
    Status1 = AM3_READ_USBCTRL(Controller, Am3UsbInterruptStatus1);
    if ((Status0 | Status1) != 0) {
        if (Status0 != 0) {
            AM3_WRITE_USBCTRL(Controller, Am3UsbInterruptStatus0, Status0);
        }

        if (Status1 != 0) {
            AM3_WRITE_USBCTRL(Controller, Am3UsbInterruptStatus1, Status1);
        }

        InterruptStatus = MusbInterruptService(&(Controller->MentorUsb));
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
Am3UsbInterruptServiceDpc (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the AM335 USB dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the controller structure.

Return Value:

    None.

--*/

{

    PAM3_USB_CONTROL Controller;

    Controller = Parameter;
    return MusbInterruptServiceDpc(&(Controller->MentorUsb));
}

KSTATUS
Am3UsbControlReset (
    PAM3_USB_CONTROL Controller
    )

/*++

Routine Description:

    This routine performs a hardware reset and initialization of the given
    controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Set legacy mode so that the mentor registers get the interrupt bits.
    //

    AM3_WRITE_USBCTRL(Controller,
                      Am3UsbControl,
                      AM3_USB_CONTROL_LEGACY_INTERRUPTS);

    //
    // Enable all interrupts.
    //

    AM3_WRITE_USBCTRL(Controller, Am3UsbInterruptEnableSet0, 0xFFFFFFFF);
    AM3_WRITE_USBCTRL(Controller, Am3UsbInterruptEnableSet1, 0xFFFFFFFF);
    Status = MusbResetController(&(Controller->MentorUsb));
    if (!KSUCCESS(Status)) {
        goto ControlResetEnd;
    }

ControlResetEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

