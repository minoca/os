/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intr.c

Abstract:

    This module implements support for the BCM2709 Interrupt Controller.

Author:

    Chris Stevens 18-Mar-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/bcm2709.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from the BCM2709 interrupt controller. The parameter should
// be a BCM2709_INTERRUPT_REGISTER value.
//

#define READ_INTERRUPT_REGISTER(_Register) \
    EfiReadRegister32(BCM2709_INTERRUPT_BASE + (_Register))

//
// This macro writes to the BCM2709 interrupt controller. _Register should be a
// BCM2709_INTERRUPT_REGISTER value and _Value should be a ULONG.
//

#define WRITE_INTERRUPT_REGISTER(_Register, _Value) \
    EfiWriteRegister32(BCM2709_INTERRUPT_BASE + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store a table that tracks which GPU IRQs are in the basic pending status
// register.
//

UINT32
EfiBcm2709InterruptIrqBasicGpuTable[BCM2709_INTERRUPT_IRQ_BASIC_GPU_COUNT] = {
    7,
    9,
    10,
    18,
    19,
    53,
    54,
    55,
    56,
    57,
    62
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709InterruptInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes a BCM2709 Interrupt Controller.

Arguments:

    PlatformBase - Supplies the platform's BCM2709 register base address.

Return Value:

    EFI Status code.

--*/

{

    //
    // Fail if the BCM2709 device library is not initialized.
    //

    if (EfiBcm2709Initialized == FALSE) {
        return EFI_NOT_READY;
    }

    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable1, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable2, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisableBasic, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptFiqControl, 0);
    return EFI_SUCCESS;
}

VOID
EfipBcm2709InterruptBeginInterrupt (
    UINT32 *InterruptNumber,
    VOID **InterruptContext
    )

/*++

Routine Description:

    This routine is called when an interrupts comes in. This routine is
    responsible for reporting the interrupt number.

Arguments:

    InterruptNumber - Supplies a pointer where interrupt line number will be
        returned.

    InterruptContext - Supplies a pointer where the platform can store a
        pointer's worth of context that will be passed back when ending the
        interrupt.

Return Value:

    None.

--*/

{

    UINT32 Base;
    UINT32 Index;
    UINT32 Status;

    //
    // Determine which interrupt fired based on the pending status.
    //

    Status = READ_INTERRUPT_REGISTER(Bcm2709InterruptIrqPendingBasic);
    if (Status == 0) {
        return;
    }

    //
    // If this is a basic interrupt, then determine which line fired based on
    // the bit set.
    //

    if ((Status & BCM2709_INTERRUPT_IRQ_BASIC_MASK) != 0) {
        Index = 0;
        while ((Status & 0x1) == 0) {
            Status = Status >> 1;
            Index += 1;
        }

        Index += Bcm2709InterruptArmTimer;

    //
    // If this is a GPU interrupt that gets set in the basic pending status
    // register, then check which bit is set. The pending 1 and 2 bits do not
    // get set for these interrupts.
    //

    } else if ((Status & BCM2709_INTERRUPT_IRQ_BASIC_GPU_MASK) != 0) {
        Status = Status >> BCM2709_INTERRUPT_IRQ_BASIC_GPU_SHIFT;
        Index = 0;
        while ((Status & 0x1) == 0) {
            Status = Status >> 1;
            Index += 1;
        }

        Index = EfiBcm2709InterruptIrqBasicGpuTable[Index];

    } else {
        if ((Status & BCM2709_INTERRUPT_IRQ_BASIC_PENDING_1) != 0) {
            Status = READ_INTERRUPT_REGISTER(Bcm2709InterruptIrqPending1);
            Base = 0;

        } else {
            Status = READ_INTERRUPT_REGISTER(Bcm2709InterruptIrqPending2);
            Base = 32;
        }

        Index = 0;
        while ((Status & 0x1) == 0) {
            Status = Status >> 1;
            Index += 1;
        }

        Index += Base;
    }

    *InterruptNumber = Index;
    return;
}

VOID
EfipBcm2709InterruptEndInterrupt (
    UINT32 InterruptNumber,
    VOID *InterruptContext
    )

/*++

Routine Description:

    This routine is called to finish handling of a platform interrupt. This is
    where the End-Of-Interrupt would get sent to the interrupt controller.

Arguments:

    InterruptNumber - Supplies the interrupt number that occurred.

    InterruptContext - Supplies the context returned by the interrupt
        controller when the interrupt began.

Return Value:

    None.

--*/

{

    return;
}

EFI_STATUS
EfipBcm2709InterruptSetInterruptLineState (
    UINT32 LineNumber,
    BOOLEAN Enabled,
    BOOLEAN EdgeTriggered
    )

/*++

Routine Description:

    This routine enables or disables an interrupt line.

Arguments:

    LineNumber - Supplies the line number to enable or disable.

    Enabled - Supplies a boolean indicating if the line should be enabled or
        disabled.

    EdgeTriggered - Supplies a boolean indicating if the interrupt is edge
        triggered (TRUE) or level triggered (FALSE).

Return Value:

    EFI Status code.

--*/

{

    BCM2709_INTERRUPT_REGISTER Register;
    UINT32 RegisterValue;
    UINT32 Shift;

    //
    // If the line is a GPU line, then determine which of the two
    // disable/enable registers it belongs to.
    //

    if (LineNumber < BCM2709_INTERRUPT_GPU_LINE_COUNT) {
        Shift = LineNumber;
        if (Shift >= 32) {
            Shift -= 32;
        }

        RegisterValue = 1 << Shift;
        if (Enabled == FALSE) {
            if (LineNumber < 32) {
                Register = Bcm2709InterruptIrqDisable1;

            } else {
                Register = Bcm2709InterruptIrqDisable2;
            }

        } else {
            if (LineNumber < 32) {
                Register = Bcm2709InterruptIrqEnable1;

            } else {
                Register = Bcm2709InterruptIrqEnable2;
            }
        }

    //
    // Otherwise the interrupt belongs to the basic enable and disable
    // registers.
    //

    } else {
        Shift = LineNumber - BCM2709_INTERRUPT_GPU_LINE_COUNT;
        RegisterValue = 1 << Shift;
        if (Enabled == FALSE) {
            Register = Bcm2709InterruptIrqDisableBasic;

        } else {
            Register = Bcm2709InterruptIrqEnableBasic;
        }
    }

    //
    // Change the state of the interrupt based on the register and the value
    // determined above.
    //

    WRITE_INTERRUPT_REGISTER(Register, RegisterValue);
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

