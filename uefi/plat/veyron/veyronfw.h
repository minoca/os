/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/soc/rk32xx.h>

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

//
// Do not use the last 16MB of RAM as it causes AHB errors during DMA
// transations.
//

#define VEYRON_RAM_START 0x00000000
#define VEYRON_RAM_SIZE  0xFE000000
#define VEYRON_OSC_HERTZ 24000000
#define VEYRON_ARM_CPU_HERTZ 1704000000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store a boolean used for debugging that disables the watchdog timer.
//

extern BOOLEAN EfiDisableWatchdog;

//
// The runtime stores a pointer to GPIO0 for system reset purposes.
//

extern VOID *EfiRk32Gpio0Base;

//
// The runtime stores a pointer to the I2C PMU for the RTC.
//

extern VOID *EfiRk32I2cPmuBase;

//
// Define a boolean indicating whether the firmware was loaded via SD or eMMC.
//

extern BOOLEAN EfiBootedViaSd;

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
EfipRk32GetPllClockFrequency (
    RK32_PLL_TYPE PllType,
    UINT32 *Frequency
    );

/*++

Routine Description:

    This routine returns the base PLL clock frequency of the given type.

Arguments:

    PllType - Supplies the type of the PLL clock whose frequency is being
        queried.

    Frequency - Supplies a pointer that receives the PLL clock's frequency in
        Hertz.

Return Value:

    Status code.

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

EFI_STATUS
EfipRk32I2cInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes the I2C device.

Arguments:

    None.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipRk32I2cWrite (
    UINT8 Chip,
    UINT32 Address,
    UINT32 AddressLength,
    VOID *Buffer,
    UINT32 Length
    );

/*++

Routine Description:

    This routine writes the given buffer out to the given i2c device.

Arguments:

    Chip - Supplies the device to write to.

    Address - Supplies the address.

    AddressLength - Supplies the width of the address. Valid values are zero
        through two.

    Buffer - Supplies the buffer containing the data to write.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipRk32I2cRead (
    UINT8 Chip,
    UINT32 Address,
    UINT32 AddressLength,
    VOID *Buffer,
    UINT32 Length
    );

/*++

Routine Description:

    This routine reads from the given i2c device into the given buffer.

Arguments:

    Chip - Supplies the device to read from.

    Address - Supplies the address.

    AddressLength - Supplies the width of the address. Valid values are zero
        through two.

    Buffer - Supplies a pointer to the buffer where the read data will be
        returned.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipRk808InitializeRtc (
    VOID
    );

/*++

Routine Description:

    This routine enables the MMC power rails controlled by the TWL4030.

Arguments:

    None.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipRk808ReadRtc (
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine reads the current time from the RK808.

Arguments:

    Time - Supplies a pointer where the time is returned on success.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipRk808ReadRtcWakeupTime (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine reads the wake alarm time from the RK808.

Arguments:

    Enabled - Supplies a pointer where a boolean will be returned indicating if
        the wake time interrupt is enabled.

    Pending - Supplies a pointer where a boolean will be returned indicating if
        the wake alarm interrupt is pending and requires service.

    Time - Supplies a pointer where the time is returned on success.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipRk808WriteRtc (
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine writes the current time to the RK808.

Arguments:

    Time - Supplies a pointer to the new time to set.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipRk808WriteRtcWakeupTime (
    BOOLEAN Enable,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine writes the alarm time to the RK808.

Arguments:

    Enable - Supplies a boolean enabling or disabling the wakeup timer.

    Time - Supplies an optional pointer to the time to set. This parameter is
        only optional if the enable parameter is FALSE.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipRk808Shutdown (
    VOID
    );

/*++

Routine Description:

    This routine performs a system shutdown using the RK808.

Arguments:

    None.

Return Value:

    Status code.

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

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

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

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

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

