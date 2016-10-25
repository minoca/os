/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pl031.h

Abstract:

    This header contains definitions for the ARM PrimeCell PL-031 Real Time
    Clock library.

Author:

    Evan Green 7-Apr-2014

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

    This structure defines the context for a PL031 Real Time Clock. Upon
    initialization, the consumer of the library is responsible for initializing
    some of these values.

Members:

    Base - Stores the base address of the controller. The consumer of the
        library is responsible for initializing this.

--*/

typedef struct _PL031_CONTEXT {
    VOID *Base;
} PL031_CONTEXT, *PPL031_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipPl031Initialize (
    PPL031_CONTEXT Context
    );

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

EFI_STATUS
EfipPl031GetTime (
    PPL031_CONTEXT Context,
    UINT32 *CurrentTime
    );

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

EFI_STATUS
EfipPl031GetWakeupTime (
    PPL031_CONTEXT Context,
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    UINT32 *WakeupTime
    );

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

EFI_STATUS
EfipPl031SetTime (
    PPL031_CONTEXT Context,
    UINT32 NewTime
    );

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

EFI_STATUS
EfipPl031SetWakeupTime (
    PPL031_CONTEXT Context,
    BOOLEAN Enable,
    UINT32 NewWakeTime
    );

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

