/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intr.c

Abstract:

    This module implements platform interrupt support for the Integrator/CP.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "integfw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an Integrator/CP interrupt register.
//

#define READ_INTERRUPT_REGISTER(_Register) \
    EfiReadRegister32(                     \
            (VOID *)(EFI_INTEGRATOR_INTERRUPT_CONTROLLER_BASE + (_Register)))

//
// This macro writes to an Integrator/CP interrupt register.
//

#define WRITE_INTERRUPT_REGISTER(_Register, _Value)                           \
    EfiWriteRegister32(                                                       \
            (VOID *)(EFI_INTEGRATOR_INTERRUPT_CONTROLLER_BASE + (_Register)), \
            (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_INTEGRATOR_INTERRUPT_CONTROLLER_BASE 0x14000000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the offsets to interrupt controller registers, in bytes.
//

typedef enum _INTEGRATOR_INTERRUPT_REGISTER {
    IntegratorInterruptIrqStatus              = 0x00,
    IntegratorInterruptIrqRawStatus           = 0x04,
    IntegratorInterruptIrqEnable              = 0x08,
    IntegratorInterruptIrqDisable             = 0x0C,
    IntegratorInterruptSoftwareInterruptSet   = 0x10,
    IntegratorInterruptSoftwareInterruptClear = 0x14,
    IntegratorInterruptFiqStatus              = 0x18,
    IntegratorInterruptFiqRawStatus           = 0x1C,
    IntegratorInterruptFiqEnable              = 0x20,
    IntegratorInterruptFiqDisable             = 0x24,
} INTEGRATOR_INTERRUPT_REGISTER, *PINTEGRATOR_INTERRUPT_REGISTER;

//
// ----------------------------------------------- Internal Function Prototypes
//

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

    //
    // Disable all FIQ and IRQ lines.
    //

    WRITE_INTERRUPT_REGISTER(IntegratorInterruptIrqDisable, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(IntegratorInterruptFiqDisable, 0xFFFFFFFF);
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

    UINT32 BitMask;

    //
    // Calculate the bit to flip.
    //

    BitMask = 1 << LineNumber;
    if (Enabled != FALSE) {
        WRITE_INTERRUPT_REGISTER(IntegratorInterruptIrqEnable, BitMask);

    } else {
        WRITE_INTERRUPT_REGISTER(IntegratorInterruptIrqDisable, BitMask);
    }

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

    UINT32 Index;
    UINT32 Status;

    *InterruptNumber = -1;
    Status = READ_INTERRUPT_REGISTER(IntegratorInterruptIrqStatus);
    if (Status == 0) {
        return;
    }

    //
    // Find the first firing index.
    //

    Index = 0;
    while ((Status & 0x1) == 0) {
        Status = Status >> 1;
        Index += 1;
    }

    *InterruptNumber = Index;
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

    return;
}

