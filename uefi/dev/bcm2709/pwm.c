/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pwm.c

Abstract:

    This module implements platform PWM support for the BCM2709 SoC family.

Author:

    Chris Stevens 10-May-2017

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/soc/b2709os.h>
#include <uefifw.h>
#include <dev/bcm2709.h>

//
// --------------------------------------------------------------------- Macros
//

#define BCM2709_CLOCK_READ(_Register) \
    EfiReadRegister32(BCM2709_CLOCK_BASE + (_Register))

#define BCM2709_CLOCK_WRITE(_Register, _Value) \
    EfiWriteRegister32(BCM2709_CLOCK_BASE + (_Register), _Value)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the PWM clock's integer and fractional divisors. The frequency can be
// calculated as:
//
//                    (source_frequency)
//     -----------------------------------------------
//     (integer_divisor + (fractional_divisor / 1024))
//

#define EFI_BCM2709_PWM_CLOCK_INTEGER_DIVISOR 5
#define EFI_BCM2709_PWM_CLOCK_FRACTION_DIVISOR 0

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
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709PwmInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the PWM controller making sure that it is exposed
    on GPIO pins 40 and 45. This allows audio to be generated using PWM and it
    will go out the headphone jack. This also initializes the PWM clock to
    run at a reasonable rate.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    UINT32 Control;
    UINT32 Divisor;
    UINT64 Frequency;
    EFI_STATUS Status;
    PBCM2709_TABLE Table;

    Status = EfipBcm2709GpioFunctionSelect(BCM2709_GPIO_HEADPHONE_JACK_LEFT,
                                           BCM2709_GPIO_FUNCTION_SELECT_ALT_0);

    if (EFI_ERROR(Status)) {
        goto PwmInitializeEnd;
    }

    Status = EfipBcm2709GpioFunctionSelect(BCM2709_GPIO_HEADPHONE_JACK_RIGHT,
                                           BCM2709_GPIO_FUNCTION_SELECT_ALT_0);

    if (EFI_ERROR(Status)) {
        goto PwmInitializeEnd;
    }

    //
    // Disable the clock in order to changes its source and divisor.
    //

    Control = BCM2709_CLOCK_READ(Bcm2709ClockPwmControl);
    Control &= ~BCM2709_CLOCK_CONTROL_ENABLE;
    Control |= BCM2709_CLOCK_PASSWORD;
    BCM2709_CLOCK_WRITE(Bcm2709ClockPwmControl, Control);
    do {
        Control = BCM2709_CLOCK_READ(Bcm2709ClockPwmControl);

    } while ((Control & BCM2709_CLOCK_CONTROL_BUSY) != 0);

    //
    // Set the divisors.
    //

    Divisor = (EFI_BCM2709_PWM_CLOCK_INTEGER_DIVISOR <<
               BCM2709_CLOCK_DIVISOR_INTEGER_SHIFT) &
              BCM2709_CLOCK_DIVISOR_INTEGER_MASK;

    Divisor |= (EFI_BCM2709_PWM_CLOCK_FRACTION_DIVISOR <<
                BCM2709_CLOCK_DIVISOR_FRACTION_SHIFT) &
               BCM2709_CLOCK_DIVISOR_FRACTION_MASK;

    Divisor |= BCM2709_CLOCK_PASSWORD;
    BCM2709_CLOCK_WRITE(Bcm2709ClockPwmDivisor, Divisor);

    //
    // Change the clock source to PLLD. This runs at a base rate of 500MHz. The
    // spec recommends against changing this at the same time as enabling the
    // clock.
    //

    Control &= ~BCM2709_CLOCK_CONTROL_SOURCE_MASK;
    Control |= (BCM2709_CLOCK_CONTROL_SOURCE_PLLD <<
                BCM2709_CLOCK_CONTROL_SOURCE_SHIFT) &
               BCM2709_CLOCK_CONTROL_SOURCE_MASK;

    Control |= BCM2709_CLOCK_PASSWORD;
    BCM2709_CLOCK_WRITE(Bcm2709ClockPwmControl, Control);
    Control |= BCM2709_CLOCK_PASSWORD | BCM2709_CLOCK_CONTROL_ENABLE;
    BCM2709_CLOCK_WRITE(Bcm2709ClockPwmControl, Control);

    //
    // The PLLD source's base rate of 500MHz is the same as the min/max rate
    // advertised by the PWM clock via the video core mailbox. That said,
    // enabling the PWM clock via the mailbox seemingly breaks PWM audio, even
    // when trying clock sources other than PLLD. As a result, rather than
    // dynamically getting the base clock rate, grab it from the ACPI table and
    // then modify it by the given divisor.
    //

    Table = EfiGetAcpiTable(BCM2709_SIGNATURE, NULL);
    if (Table == NULL) {
        Status = EFI_NOT_FOUND;
        goto PwmInitializeEnd;
    }

    Divisor = (EFI_BCM2709_PWM_CLOCK_INTEGER_DIVISOR *
               BCM2709_CLOCK_DIVISOR_FRACTION_DENOMINATOR) +
              EFI_BCM2709_PWM_CLOCK_FRACTION_DIVISOR;

    Frequency = (ULONGLONG)Table->PwmClockFrequency *
                BCM2709_CLOCK_DIVISOR_FRACTION_DENOMINATOR;

    Frequency /= Divisor;
    if (Frequency > MAX_ULONG) {
        Status = EFI_UNSUPPORTED;
        goto PwmInitializeEnd;
    }

    Table->PwmClockFrequency = (UINT32)Frequency;
    EfiAcpiChecksumTable(Table,
                         Table->Header.Length,
                         OFFSET_OF(DESCRIPTION_HEADER, Checksum));

PwmInitializeEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

