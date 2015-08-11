/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    veyronfw.h

Abstract:

    This header contains internal definitions for the Veyron firmware, which
    supports the Asus C201 Chromebook.

Author:

    Evan Green 9-Jul-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "cpu/rk32xx.h"

//
// --------------------------------------------------------------------- Macros
//

#define RK32_READ_CRU(_Register) \
    EfiReadRegister32((VOID *)RK32_CRU_BASE + (_Register))

#define RK32_WRITE_CRU(_Register, _Value) \
    EfiWriteRegister32((VOID *)RK32_CRU_BASE + (_Register), (_Value))

#define RK32_READ_GRF(_Register) \
    EfiReadRegister32((VOID *)RK32_GRF_BASE + (_Register))

#define RK32_WRITE_GRF(_Register, _Value) \
    EfiWriteRegister32((VOID *)RK32_GRF_BASE + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define VEYRON_RAM_START 0x00000000
#define VEYRON_RAM_SIZE  0xFF000000
#define VEYRON_OSC_HERTZ 24000000
#define VEYRON_ARM_CPU_HERTZ 1704000000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the APB bus clock frequency, which is a function of the PLLs and the
// divisors.
//

extern UINT32 EfiRk32ApbFrequency;

//
// Store a boolean used for debugging that disables the watchdog timer.
//

extern BOOLEAN EfiDisableWatchdog;

//
// The runtime stores a pointer to the CRU for system reset purposes.
//

extern VOID *EfiRk32CruBase;

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipPlatformSetInterruptLineState (
    UINT32 LineNumber,
    BOOLEAN Enabled,
    BOOLEAN EdgeTriggered
    );

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

EFI_STATUS
EfipVeyronCreateSmbiosTables (
    VOID
    );

/*++

Routine Description:

    This routine creates the SMBIOS tables.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipEnumerateRamDisks (
    VOID
    );

/*++

Routine Description:

    This routine enumerates any RAM disks embedded in the firmware.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipVeyronEnumerateVideo (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the display on the Veyron.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipVeyronEnumerateSerial (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the serial port on the Veyron RK3288 SoC.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

VOID
EfipVeyronInitializeUart (
    VOID
    );

/*++

Routine Description:

    This routine completes any platform specific UART initialization steps.

Arguments:

    None.

Return Value:

    None.

--*/

EFI_STATUS
EfipSmpInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes and parks the second core on the RK32xx.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipVeyronEnumerateSd (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the SD card on the Veyron SoC.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

VOID
EfipVeyronUsbInitialize (
    VOID
    );

/*++

Routine Description:

    This routine performs any board-specific high speed USB initialization.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Runtime service functions
//

EFIAPI
VOID
EfipRk32ResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    );

/*++

Routine Description:

    This routine resets the entire platform.

Arguments:

    ResetType - Supplies the type of reset to perform.

    ResetStatus - Supplies the status code for this reset.

    DataSize - Supplies the size of the reset data.

    ResetData - Supplies an optional pointer for reset types of cold, warm, or
        shutdown to a null-terminated string, optionally followed by additional
        binary data.

Return Value:

    None. This routine does not return.

--*/

EFIAPI
EFI_STATUS
EfipRk32GetTime (
    EFI_TIME *Time,
    EFI_TIME_CAPABILITIES *Capabilities
    );

/*++

Routine Description:

    This routine returns the current time and dat information, and
    timekeeping capabilities of the hardware platform.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

    Capabilities - Supplies an optional pointer where the capabilities will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the time parameter was NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

--*/

EFIAPI
EFI_STATUS
EfipRk32SetTime (
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine sets the current local time and date information.

Arguments:

    Time - Supplies a pointer to the time to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

--*/

EFIAPI
EFI_STATUS
EfipRk32GetWakeupTime (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine gets the current wake alarm setting.

Arguments:

    Enabled - Supplies a pointer that receives a boolean indicating if the
        alarm is currently enabled or disabled.

    Pending - Supplies a pointer that receives a boolean indicating if the
        alarm signal is pending and requires acknowledgement.

    Time - Supplies a pointer that receives the current wake time.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if any parameter is NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

EFIAPI
EFI_STATUS
EfipRk32SetWakeupTime (
    BOOLEAN Enable,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine sets the current wake alarm setting.

Arguments:

    Enable - Supplies a boolean enabling or disabling the wakeup timer.

    Time - Supplies an optional pointer to the time to set. This parameter is
        only optional if the enable parameter is FALSE.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

