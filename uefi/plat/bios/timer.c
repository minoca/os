/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements platform timer services for BIOS machines.

Author:

    Evan Green 3-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/fw/acpitabs.h>
#include <uefifw.h>
#include "biosfw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

UINT64
EfipPlatformReadTimer (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Hold on to the location of the PM timer.
//

UINT16 EfiPmTimerPort;
UINT32 EfiPmTimerBitWidth;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiPlatformSetWatchdogTimer (
    UINTN Timeout,
    UINT64 WatchdogCode,
    UINTN DataSize,
    CHAR16 *WatchdogData
    )

/*++

Routine Description:

    This routine sets the system's watchdog timer.

Arguments:

    Timeout - Supplies the number of seconds to set the timer for.

    WatchdogCode - Supplies a numeric code to log on a watchdog timeout event.

    DataSize - Supplies the size of the watchdog data.

    WatchdogData - Supplies an optional buffer that includes a null-terminated
        string, optionally followed by additional binary data.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the supplied watchdog code is invalid.

    EFI_UNSUPPORTED if there is no watchdog timer.

    EFI_DEVICE_ERROR if an error occurred accessing the device hardware.

--*/

{

    return EFI_UNSUPPORTED;
}

EFI_STATUS
EfiPlatformInitializeTimers (
    UINT32 *ClockTimerInterruptNumber,
    EFI_PLATFORM_SERVICE_TIMER_INTERRUPT *ClockTimerServiceRoutine,
    EFI_PLATFORM_READ_TIMER *ReadTimerRoutine,
    UINT64 *ReadTimerFrequency,
    UINT32 *ReadTimerWidth
    )

/*++

Routine Description:

    This routine initializes platform timer services. There are actually two
    different timer services returned in this routine. The periodic timer tick
    provides a periodic interrupt. The read timer provides a free running
    counter value. These are likely serviced by different timers. For the
    periodic timer tick, this routine should start the periodic interrupts
    coming in. The periodic rate of the timer can be anything reasonable, as
    the time counter will be used to count actual duration. The rate should be
    greater than twice the rollover rate of the time counter to ensure proper
    time accounting. Interrupts are disabled at the processor core for the
    duration of this routine.

Arguments:

    ClockTimerInterruptNumber - Supplies a pointer where the interrupt line
        number of the periodic timer tick will be returned.

    ClockTimerServiceRoutine - Supplies a pointer where a pointer to a routine
        called when the periodic timer tick interrupt occurs will be returned.

    ReadTimerRoutine - Supplies a pointer where a pointer to a routine
        called to read the current timer value will be returned.

    ReadTimerFrequency - Supplies the frequency of the counter.

    ReadTimerWidth - Supplies a pointer where the read timer bit width will be
        returned.

Return Value:

    EFI Status code.

--*/

{

    PFADT Fadt;
    PRSDP Rsdp;
    PRSDT Rsdt;
    ULONG RsdtTableCount;
    PULONG RsdtTableEntry;
    UINTN TableIndex;

    //
    // Find the FADT to figure out where the PM timer is.
    //

    Rsdp = EfiRsdpPointer;
    if (Rsdp == NULL) {
        return EFI_UNSUPPORTED;
    }

    Rsdt = (PRSDT)(UINTN)(Rsdp->RsdtAddress);
    RsdtTableCount = (Rsdt->Header.Length - sizeof(DESCRIPTION_HEADER)) /
                     sizeof(ULONG);

    if (RsdtTableCount == 0) {
        return EFI_NOT_FOUND;
    }

    RsdtTableEntry = (PULONG)&(Rsdt->Entries);
    for (TableIndex = 0; TableIndex < RsdtTableCount; TableIndex += 1) {
        Fadt = (PFADT)RsdtTableEntry[TableIndex];
        if (Fadt->Header.Signature == FADT_SIGNATURE) {
            break;
        }
    }

    if (TableIndex == RsdtTableCount) {
        return EFI_NOT_FOUND;
    }

    //
    // Grab the PM timer port and width out of the FADT table.
    //

    EfiPmTimerPort = Fadt->PmTimerBlock;
    if (EfiPmTimerPort == 0) {
        return EFI_UNSUPPORTED;
    }

    EfiPmTimerBitWidth = 24;
    if ((Fadt->Flags & FADT_FLAG_PM_TIMER_32_BITS) != 0) {
        EfiPmTimerBitWidth = 32;
    }

    //
    // Clock interrupts are not supported as the BIOS may have 16-bit real mode
    // interrupts coming in.
    //

    *ClockTimerInterruptNumber = 0;
    *ClockTimerServiceRoutine = NULL;
    *ReadTimerRoutine = EfipPlatformReadTimer;
    *ReadTimerFrequency = PM_TIMER_FREQUENCY;
    *ReadTimerWidth = EfiPmTimerBitWidth;
    return EFI_SUCCESS;
}

VOID
EfiPlatformTerminateTimers (
    VOID
    )

/*++

Routine Description:

    This routine terminates timer services in preparation for the termination
    of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

UINT64
EfipPlatformReadTimer (
    VOID
    )

/*++

Routine Description:

    This routine is called to read the current platform time value. The timer
    is assumed to be free running at a constant frequency, and should have a
    bit width as reported in the initialize function. The UEFI core will
    manage software bit extension out to 64 bits, this routine should just
    reporte the hardware timer value.

Arguments:

    None.

Return Value:

    Returns the hardware timer value.

--*/

{

    return EfiIoPortIn32(EfiPmTimerPort);
}

