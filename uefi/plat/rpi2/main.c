/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    main.c

Abstract:

    This module implements the entry point for the UEFI firmware running on top
    of the Raspberry Pi 2.

Author:

    Chris Stevens 19-Mar-2015

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
#include "rpi2fw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define FIRMWARE_IMAGE_NAME "rpi2fw.elf"

//
// Define the core timer's crystal clock frequency: 19.2 MHz.
//

#define BCM2836_CORE_TIMER_CRYSTAL_CLOCK_FREQUENCY 19200000

//
// Define the maximum pre-scaler value.
//

#define BCM2836_CORE_TIMER_MAX_PRE_SCALER 0x80000000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the data necessary to get a BCM2709's clock rate.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    ClockRate - Stores a message getting the clock rate.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _EFI_BCM2709_GET_CLOCK_RATE {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_GET_CLOCK_RATE ClockRate;
    UINT32 EndTag;
} EFI_BCM2709_GET_CLOCK_RATE, *PEFI_BCM2709_GET_CLOCK_RATE;

/*++

Structure Description:

    This structure defines the data necessary to set the BCM2709's ARM clock
    rate.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    ArmClockRate - Stores a message setting the ARM core's clock rate.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _EFI_BCM2709_SET_CLOCK_RATE {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_SET_CLOCK_RATE ArmClockRate;
    UINT32 EndTag;
} EFI_BCM2709_SET_CLOCK_RATE, *PEFI_BCM2709_SET_CLOCK_RATE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipBcm2836InitializeUart (
    VOID
    );

EFI_STATUS
EfipBcm2836InitializePwm (
    VOID
    );

EFI_STATUS
EfipBcm2836InitializeArmClock (
    VOID
    );

EFI_STATUS
EfipBcm2836InitializeApbClock (
    VOID
    );

EFI_STATUS
EfipBcm2836InitializeCoreTimerClock (
    VOID
    );

UINT32
EfipBcm2836GetGtFrequency (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Variables defined in the linker script that mark the start and end of the
// image.
//

extern INT8 _end;
extern INT8 __executable_start;

//
// Store a template to set the Raspberry Pi 2's ARM clock frequency.
//

EFI_BCM2709_SET_CLOCK_RATE EfiRpi2SetClockTemplate = {
    {
        sizeof(EFI_BCM2709_SET_CLOCK_RATE),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_CLOCK_RATE,
            sizeof(UINT32) * 3,
            sizeof(UINT32) * 3
        },

        BCM2709_MAILBOX_CLOCK_ID_ARM,
        0,
        0
    },

    0
};

EFI_BCM2709_GET_CLOCK_RATE EfiRpi2GetClockTemplate = {
    {
        sizeof(EFI_BCM2709_GET_CLOCK_RATE),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_CLOCK_MAX_RATE,
            sizeof(UINT32) * 2,
            sizeof(UINT32)
        },

        BCM2709_MAILBOX_CLOCK_ID_ARM,
        0
    },

    0
};

//
// ------------------------------------------------------------------ Functions
//

__USED
VOID
EfiRpi2Main (
    VOID *TopOfStack,
    UINTN StackSize
    )

/*++

Routine Description:

    This routine is the C entry point for the firmware.

Arguments:

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

Return Value:

    This routine does not return.

--*/

{

    UINTN FirmwareSize;
    EFI_STATUS Status;

    //
    // Force GPIO pins 14 and 15 to the UART (rather than the mini-UART) before
    // debugging comes online.
    //

    Status = EfipBcm2836InitializeUart();
    if (EFI_ERROR(Status)) {
        return;
    }

    //
    // Initialize UEFI enough to get into the debugger.
    //

    FirmwareSize = (UINTN)&_end - (UINTN)&__executable_start;
    EfiCoreMain((VOID *)-1,
                &__executable_start,
                FirmwareSize,
                FIRMWARE_IMAGE_NAME,
                TopOfStack - StackSize,
                StackSize);

    return;
}

EFI_STATUS
EfiPlatformInitialize (
    UINT32 Phase
    )

/*++

Routine Description:

    This routine performs platform-specific firmware initialization.

Arguments:

    Phase - Supplies the iteration number this routine is being called on.
        Phase zero occurs very early, just after the debugger comes up.
        Phase one occurs a bit later, after timer and interrupt services are
        initialized. Phase two happens right before boot, after all platform
        devices have been enumerated.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    Status = EFI_SUCCESS;
    if (Phase == 0) {
        Status = EfipBcm2709Initialize((VOID *)BCM2836_BASE);
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }

        Status = EfipBcm2836SmpInitialize(0);
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }

    } else if (Phase == 1) {
        Status = EfipBcm2836InitializeArmClock();
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }

        Status = EfipBcm2709UsbInitialize();
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }

        Status = EfipBcm2836SmpInitialize(1);
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }

        Status = EfipRpi2CreateSmbiosTables();
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }

    } else if (Phase == 2) {
        Status = EfipBcm2836SmpInitialize(2);
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }

        Status = EfipBcm2836InitializeApbClock();
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }

        Status = EfipBcm2836InitializeCoreTimerClock();
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }

        Status = EfipBcm2709PwmInitialize();
        if (EFI_ERROR(Status)) {
            goto PlatformInitializeEnd;
        }
    }

PlatformInitializeEnd:
    return Status;
}

EFI_STATUS
EfiPlatformEnumerateDevices (
    VOID
    )

/*++

Routine Description:

    This routine enumerates and connects any builtin devices the platform
    contains.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    Status = EfipBcm2709EnumerateSd();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfipBcm2709EnumerateVideo();
    EfipBcm2709EnumerateSerial();
    Status = EfipEnumerateRamDisks();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipBcm2836InitializeUart (
    VOID
    )

/*++

Routine Description:

    This routine initializes the PL011 UART making sure that it is exposed on
    GPIO pins 14 and 15. On some versions of the Raspberry Pi, the mini-UART is
    exposed on said pins.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    //
    // The BCM2709 device must be initialized before using GPIO.
    //

    Status = EfipBcm2709Initialize((VOID *)BCM2836_BASE);
    if (EFI_ERROR(Status)) {
        goto InitializeUartEnd;
    }

    Status = EfipBcm2709GpioFunctionSelect(BCM2709_GPIO_RECEIVE_PIN,
                                           BCM2709_GPIO_FUNCTION_SELECT_ALT_0);

    if (EFI_ERROR(Status)) {
        goto InitializeUartEnd;
    }

    Status = EfipBcm2709GpioFunctionSelect(BCM2709_GPIO_TRANSMIT_PIN,
                                           BCM2709_GPIO_FUNCTION_SELECT_ALT_0);

    if (EFI_ERROR(Status)) {
        goto InitializeUartEnd;
    }

InitializeUartEnd:
    return Status;
}

EFI_STATUS
EfipBcm2836InitializeArmClock (
    VOID
    )

/*++

Routine Description:

    This routine initializes the ARM clock to its maximu supported frequency.
    The firmware initializes it to less than the maximum (i.e. 600Mhz, rather
    than the 900Mhz max on the RPI 2).

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_BCM2709_GET_CLOCK_RATE GetClockRate;
    EFI_BCM2709_SET_CLOCK_RATE SetClockRate;
    EFI_STATUS Status;

    //
    // Get the maximum supported ARM core clock rate from the mailbox.
    //

    EfiCopyMem(&GetClockRate,
               &EfiRpi2GetClockTemplate,
               sizeof(EFI_BCM2709_GET_CLOCK_RATE));

    Status = EfipBcm2709MailboxSendCommand(BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                           &GetClockRate,
                                           sizeof(EFI_BCM2709_GET_CLOCK_RATE),
                                           FALSE);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Set the ARM core clock rate to the maximum.
    //

    EfiCopyMem(&SetClockRate,
               &EfiRpi2SetClockTemplate,
               sizeof(EFI_BCM2709_SET_CLOCK_RATE));

    SetClockRate.ArmClockRate.Rate = GetClockRate.ClockRate.Rate;
    Status = EfipBcm2709MailboxSendCommand(BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                           &SetClockRate,
                                           sizeof(EFI_BCM2709_SET_CLOCK_RATE),
                                           TRUE);

    return Status;
}

EFI_STATUS
EfipBcm2836InitializeApbClock (
    VOID
    )

/*++

Routine Description:

    This routine initializes the APB clock. This is the video core's clock and
    can be configured via config.txt. Initialization simply consists of reading
    the clock and updating the BCM2 ACPI table if necessary.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_BCM2709_GET_CLOCK_RATE GetClockRate;
    EFI_STATUS Status;
    PBCM2709_TABLE Table;

    //
    // Get the current video core clock rate from the mailbox.
    //

    EfiCopyMem(&GetClockRate,
               &EfiRpi2GetClockTemplate,
               sizeof(EFI_BCM2709_GET_CLOCK_RATE));

    GetClockRate.ClockRate.TagHeader.Tag = BCM2709_MAILBOX_TAG_GET_CLOCK_RATE;
    GetClockRate.ClockRate.ClockId = BCM2709_MAILBOX_CLOCK_ID_VIDEO;
    Status = EfipBcm2709MailboxSendCommand(BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                           &GetClockRate,
                                           sizeof(EFI_BCM2709_GET_CLOCK_RATE),
                                           FALSE);

    if (EFI_ERROR(Status)) {
        goto InitializeApbClockEnd;
    }

    //
    // Get the Broadcom ACPI table.
    //

    Table = EfiGetAcpiTable(BCM2709_SIGNATURE, NULL);
    if (Table == NULL) {
        Status = EFI_NOT_FOUND;
        goto InitializeApbClockEnd;
    }

    if (Table->ApbClockFrequency != GetClockRate.ClockRate.Rate) {
        Table->ApbClockFrequency = GetClockRate.ClockRate.Rate;
        EfiAcpiChecksumTable(Table,
                             Table->Header.Length,
                             OFFSET_OF(DESCRIPTION_HEADER, Checksum));
    }

InitializeApbClockEnd:
    return Status;
}

EFI_STATUS
EfipBcm2836InitializeCoreTimerClock (
    VOID
    )

/*++

Routine Description:

    This routine initializes the ARM core's timer clock. This backs the ARM
    Generic Timer.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    UINT32 Divider;
    UINT32 Frequency;
    UINT32 PreScaler;
    UINT32 TimerControl;

    //
    // Use the 19.2 MHz crystal clock to back the ARM Generic Timer.
    //

    TimerControl = BCM2836_CORE_TIMER_CONTROL_INCREMENT_BY_1 |
                   BCM2836_CORE_TIMER_CONTROL_CRYSTAL_CLOCK;

    EfiWriteRegister32((VOID*)BCM2836_CORE_TIMER_CONTROL, TimerControl);

    //
    // Get the programmed frequency and try to match the pre-scaler so the
    // clock runs at the targeted frequency.
    //

    Frequency = EfipBcm2836GetGtFrequency();
    if (Frequency > BCM2836_CORE_TIMER_CRYSTAL_CLOCK_FREQUENCY) {
        return EFI_UNSUPPORTED;
    }

    //
    // The frequency is obtained by dividing 19.2 MHz by the divider. The
    // divider is obtained by dividing 2^31 by the pre-scaler. Determine the
    // appropriate pre-scaler for the frequency.
    //

    Divider = BCM2836_CORE_TIMER_CRYSTAL_CLOCK_FREQUENCY / Frequency;
    PreScaler = BCM2836_CORE_TIMER_MAX_PRE_SCALER / Divider;
    EfiWriteRegister32((VOID*)BCM2836_CORE_TIMER_PRE_SCALER, PreScaler);
    return EFI_SUCCESS;
}

