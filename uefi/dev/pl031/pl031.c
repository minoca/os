/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pl031.c

Abstract:

    This module implements support for the ARM PrimeCell PL-031 Real Time Clock.

Author:

    Evan Green 7-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "dev/pl031.h"

//
// --------------------------------------------------------------------- Macros
//

#define PL031_READ(_Context, _Register) \
    EfiReadRegister32((_Context)->Base + (_Register))

#define PL031_WRITE(_Context, _Register, _Value) \
    EfiWriteRegister32((_Context)->Base + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define PL031_CONTROL_START 0x00000001

#define PL031_INTERRUPT 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PL031_REGISTER {
    Pl031RegisterCount                  = 0x00,
    Pl031RegisterMatch                  = 0x04,
    Pl031RegisterLoad                   = 0x08,
    Pl031RegisterControl                = 0x0C,
    Pl031RegisterInterruptMask          = 0x10,
    Pl031RegisterRawInterruptStatus     = 0x14,
    Pl031RegisterMaskedInterruptStatus  = 0x18,
    Pl031RegisterInterruptClear         = 0x1C,
    Pl031RegisterPeripheralId           = 0xFE0,
    Pl031RegisterPrimeCellId            = 0xFF0
} PL031_REGISTER, *PPL031_REGISTER;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipPl031Initialize (
    PPL031_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a PL-031 device.

Arguments:

    Context - Supplies a pointer to the device context. The caller must have
        filled out the base register.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device could not be initialized.

--*/

{

    UINT32 Control;

    Control = PL031_READ(Context, Pl031RegisterControl);
    if ((Control & PL031_CONTROL_START) == 0) {
        Control |= PL031_CONTROL_START;
        PL031_WRITE(Context, Pl031RegisterControl, Control);
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipPl031GetTime (
    PPL031_CONTEXT Context,
    UINT32 *CurrentTime
    )

/*++

Routine Description:

    This routine reads the current value from the RTC device.

Arguments:

    Context - Supplies a pointer to the device context. The caller must have
        filled out the base register.

    CurrentTime - Supplies a pointer where the current time will be returned
        on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device could not be initialized.

--*/

{

    *CurrentTime = PL031_READ(Context, Pl031RegisterCount);
    return EFI_SUCCESS;
}

EFI_STATUS
EfipPl031GetWakeupTime (
    PPL031_CONTEXT Context,
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    UINT32 *WakeupTime
    )

/*++

Routine Description:

    This routine reads the current wakeup time from the RTC device.

Arguments:

    Context - Supplies a pointer to the device context. The caller must have
        filled out the base register.

    Enabled - Supplies a pointer where a boolean will be returned indicating
        whether or not the wake alarm is enabled.

    Pending - Supplies a pointer where a boolean will be retunred indicating
        whether or not the wake alarm is pending.

    WakeupTime - Supplies a pointer where the current wakeup time will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device could not be initialized.

--*/

{

    *Enabled = FALSE;
    *Pending = FALSE;
    *WakeupTime = PL031_READ(Context, Pl031RegisterMatch);
    if ((PL031_READ(Context, Pl031RegisterInterruptMask) & PL031_INTERRUPT) !=
        0) {

        *Enabled = TRUE;
    }

    if ((PL031_READ(Context, Pl031RegisterMaskedInterruptStatus) &
         PL031_INTERRUPT) != 0) {

        *Pending = TRUE;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipPl031SetTime (
    PPL031_CONTEXT Context,
    UINT32 NewTime
    )

/*++

Routine Description:

    This routine reads the current value from the RTC device.

Arguments:

    Context - Supplies a pointer to the device context. The caller must have
        filled out the base register.

    NewTime - Supplies the new time to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device could not be initialized.

--*/

{

    PL031_WRITE(Context, Pl031RegisterLoad, NewTime);
    return EFI_SUCCESS;
}

EFI_STATUS
EfipPl031SetWakeupTime (
    PPL031_CONTEXT Context,
    BOOLEAN Enable,
    UINT32 NewWakeTime
    )

/*++

Routine Description:

    This routine reads the current value from the RTC device.

Arguments:

    Context - Supplies a pointer to the device context. The caller must have
        filled out the base register.

    Enable - Supplies a boolean indicating if the wake alarm should be enabled
        (TRUE) or disabled (FALSE).

    NewWakeTime - Supplies the new time to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device could not be initialized.

--*/

{

    UINT32 Value;

    //
    // Clear the status either way.
    //

    Value = PL031_READ(Context, Pl031RegisterInterruptClear);
    Value |= PL031_INTERRUPT;
    PL031_WRITE(Context, Pl031RegisterInterruptClear, PL031_INTERRUPT);
    Value = PL031_READ(Context, Pl031RegisterInterruptMask);
    if (Enable != FALSE) {
        PL031_WRITE(Context, Pl031RegisterMatch, NewWakeTime);
        Value |= PL031_INTERRUPT;

    } else {
        Value &= ~PL031_INTERRUPT;
    }

    PL031_WRITE(Context, Pl031RegisterInterruptMask, Value);
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

