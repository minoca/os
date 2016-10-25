/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intr.c

Abstract:

    This module implements interrupt controller support for UEFI on the TI
    BeagleBone Black.

Author:

    Evan Green 22-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/soc/am335x.h>
#include "bbonefw.h"

//
// --------------------------------------------------------------------- Macros
//

#define AM335_INTC_READ(_Register) \
    EfiReadRegister32((VOID *)(AM335_INTC_BASE + (_Register)))

#define AM335_INTC_WRITE(_Register, _Value) \
    EfiWriteRegister32((VOID *)(AM335_INTC_BASE + (_Register)), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the hardcoded priority currently assigned to all interrupts.
//

#define EFI_AM335_INTERRUPT_PRIORITY 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfiAm335ResetInterruptController (
    VOID
    );

VOID
EfipPlatformBeginInterrupt (
    UINT32 *InterruptNumber,
    VOID **InterruptContext
    );

VOID
EfipPlatformEndInterrupt (
    UINT32 InterruptNumber,
    VOID *InterruptContext
    );

VOID
EfiAm335ResetInterruptController (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiPlatformInitializeInterrupts (
    EFI_PLATFORM_BEGIN_INTERRUPT *BeginInterruptFunction,
    EFI_PLATFORM_HANDLE_INTERRUPT *HandleInterruptFunction,
    EFI_PLATFORM_END_INTERRUPT *EndInterruptFunction
    )

/*++

Routine Description:

    This routine initializes support for platform interrupts. Interrupts are
    assumed to be disabled at the processor now. This routine should enable
    interrupts at the procesor core.

Arguments:

    BeginInterruptFunction - Supplies a pointer where a pointer to a function
        will be returned that is called when an interrupt occurs.

    HandleInterruptFunction - Supplies a pointer where a pointer to a function
        will be returned that is called to handle a platform-specific interurpt.
        NULL may be returned here.

    EndInterruptFunction - Supplies a pointer where a pointer to a function
        will be returned that is called to complete an interrupt.

Return Value:

    EFI Status code.

--*/

{

    EfiAm335ResetInterruptController();
    *BeginInterruptFunction = EfipPlatformBeginInterrupt;
    *HandleInterruptFunction = NULL;
    *EndInterruptFunction = EfipPlatformEndInterrupt;
    EfiEnableInterrupts();
    return EFI_SUCCESS;
}

VOID
EfiPlatformTerminateInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine terminates interrupt services in preparation for transitioning
    out of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Just reset the controller again to mask everything.
    //

    EfiAm335ResetInterruptController();
    return;
}

EFI_STATUS
EfipPlatformSetInterruptLineState (
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

    UINT32 Index;
    UINTN Register;
    UINT32 Value;

    //
    // Configure the priority of the line.
    //

    Value = (EFI_AM335_INTERRUPT_PRIORITY << AM335_INTC_LINE_PRIORITY_SHIFT) |
            AM335_INTC_LINE_IRQ;

    AM335_INTC_WRITE(AM335_INTC_LINE(LineNumber), Value);

    //
    // Enable or disable the line.
    //

    Index = AM335_INTC_LINE_TO_INDEX(LineNumber);
    Value = AM335_INTC_LINE_TO_MASK(LineNumber);
    if (Enabled != FALSE) {
        Register = AM335_INTC_MASK_CLEAR(Index);

    } else {
        Register = AM335_INTC_MASK_SET(Index);
    }

    AM335_INTC_WRITE(Register, Value);
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipPlatformBeginInterrupt (
    UINT32 *InterruptNumber,
    VOID **InterruptContext
    )

/*++

Routine Description:

    This routine is called when an interrupts comes in. The platform code is
    responsible for reporting the interrupt number. Interrupts are disabled at
    the processor core at this point.

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

    UINT32 Value;

    Value = AM335_INTC_READ(Am335IntcSortedIrq);
    *InterruptContext = (VOID *)(Value);
    if ((Value & AM335_INTC_SORTED_SPURIOUS) != 0) {
        *InterruptNumber = -1;

    } else {
        *InterruptNumber = Value & AM335_INTC_SORTED_ACTIVE_MASK;
    }

    return;
}

VOID
EfipPlatformEndInterrupt (
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

    UINTN Value;

    Value = (UINTN)InterruptContext;
    if ((Value & AM335_INTC_SORTED_SPURIOUS) == 0) {
        AM335_INTC_WRITE(Am335IntcControl,
                         AM335_INTC_CONTROL_NEW_IRQ_AGREEMENT);
    }

    return;
}

VOID
EfiAm335ResetInterruptController (
    VOID
    )

/*++

Routine Description:

    This routine resets the interrupt controller, masking all of its lines.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Reset the interrupt controller. This masks all lines.
    //

    Value = AM335_INTC_SYSTEM_CONFIG_SOFT_RESET;
    AM335_INTC_WRITE(Am335IntcSystemConfig, Value);
    do {
        Value = AM335_INTC_READ(Am335IntcSystemStatus);

    } while ((Value & AM335_INTC_SYSTEM_STATUS_RESET_DONE) == 0);

    return;
}

