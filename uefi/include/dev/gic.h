/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gic.h

Abstract:

    This header contains definitions for the ARM Generic Interrupt Controller.

Author:

    Evan Green 3-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the Generic Interrupt Controller context.

Members:

    DistributorBase - Stores the base address of the distributor registers.

    CpuInterfaceBase - Stores the base address of the CPU interface.

    MaxLines - Stores the maximum line count in this controller.

--*/

typedef struct _GIC_CONTEXT {
    VOID *DistributorBase;
    VOID *CpuInterfaceBase;
    UINT32 MaxLines;
} GIC_CONTEXT, *PGIC_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipGicInitialize (
    PGIC_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes a Generic Interrupt Controller. It enables the
    controller and masks all interrupt lines.

Arguments:

    Context - Supplies the pointer to the controller's context. The base must
        be filled in by the caller, and the rest must be zeroed out by the
        caller.

Return Value:

    EFI Status code.

--*/

VOID
EfipGicBeginInterrupt (
    PGIC_CONTEXT Context,
    UINT32 *InterruptNumber,
    VOID **InterruptContext
    );

/*++

Routine Description:

    This routine is called when an interrupts comes in. This routine determines
    the interrupt source.

Arguments:

    Context - Supplies a pointer to the interrupt controller context.

    InterruptNumber - Supplies a pointer where interrupt line number will be
        returned.

    InterruptContext - Supplies a pointer where the platform can store a
        pointer's worth of context that will be passed back when ending the
        interrupt.

Return Value:

    None.

--*/

VOID
EfipGicEndInterrupt (
    PGIC_CONTEXT Context,
    UINT32 InterruptNumber,
    VOID *InterruptContext
    );

/*++

Routine Description:

    This routine is called to finish handling of a platform interrupt. This is
    where the End-Of-Interrupt would get sent to the interrupt controller.

Arguments:

    Context - Supplies a pointer to the interrupt controller context.

    InterruptNumber - Supplies the interrupt number that occurred.

    InterruptContext - Supplies the context returned by the interrupt
        controller when the interrupt began.

Return Value:

    None.

--*/

EFI_STATUS
EfipGicSetLineState (
    PGIC_CONTEXT Context,
    UINT32 LineNumber,
    BOOLEAN Enabled,
    BOOLEAN EdgeTriggered
    );

/*++

Routine Description:

    This routine enables or disables an interrupt line.

Arguments:

    Context - Supplies the pointer to the controller's context.

    LineNumber - Supplies the line number to enable or disable.

    Enabled - Supplies a boolean indicating if the line should be enabled or
        disabled.

    EdgeTriggered - Supplies a boolean indicating if the interrupt is edge
        triggered (TRUE) or level triggered (FALSE).

Return Value:

    EFI Status code.

--*/

