/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    integfw.h

Abstract:

    This header contains definitions for the Integrator/CP

Author:

    Evan Green 4-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define INTEGRATOR_RAM_START 0x80000000
#define INTEGRATOR_RAM_SIZE (1024 * 1024 * 16)

#define INTEGRATOR_CM_BASE 0x10000000
#define INTEGRATOR_CM_CONTROL 0x0C
#define INTEGRATOR_CM_CONTROL_RESET 0x08
#define INTEGRATOR_CM_SIZE 0x1000

//
// Define the location of the PL031 Real Time Clock.
//

#define INTEGRATOR_PL031_RTC_BASE 0x15000000
#define INTEGRATOR_PL031_RTC_SIZE 0x1000

//
// Define the serial port address.
//

#define INTEGRATOR_UART_BASE 0x16000000

#define INTEGRATOR_PL110_BASE 0xC0000000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipIntegratorEnumerateVideo (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the display on the Integrator/CP.

Arguments:

    None.

Return Value:

    EFI status code.

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
EfipIntegratorEnumerateSerial (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the serial port on the Integrator board.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipIntegratorCreateSmbiosTables (
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

//
// Runtime services
//

EFI_STATUS
EfipIntegratorInitializeRtc (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for the EFI time runtime services.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

VOID
EfipIntegratorRtcVirtualAddressChange (
    VOID
    );

/*++

Routine Description:

    This routine is called when the firmware is converting to virtual address
    mode. It converts any pointers it's got. This routine is called after
    ExitBootServices, so no EFI boot services are available.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfipIntegratorGetTime (
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
EfipIntegratorSetTime (
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
EfipIntegratorGetWakeupTime (
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
EfipIntegratorSetWakeupTime (
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

