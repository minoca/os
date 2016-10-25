/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap4usb.c

Abstract:

    This module fires up the OMAP4 High Speed USB controller.

Author:

    Evan Green 24-Mar-2013

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "pandafw.h"

//
// --------------------------------------------------------------------- Macros
//

#define ULPI_SET_REGISTER(_Register) (_Register + 1)
#define ULPI_CLEAR_REGISTER(_Register) (_Register + 1)

//
// These macros read from and write to the L3 INIT CM2 block.
//

#define OMAP4_READ_L3_INIT_CM2_REGISTER(_Register) \
    *(volatile UINT32 *)((UINT8 *)EfiOmap4L3InitCm2Address + (_Register))

#define OMAP4_WRITE_L3_INIT_CM2_REGISTER(_Register, _Value)                   \
    *((volatile UINT32 *)((UINT8 *)EfiOmap4L3InitCm2Address + (_Register))) = \
                                                                       (_Value)
//
// These macros read from and write to the OMAP4 SCRM.
//

#define OMAP4_READ_SCRM_REGISTER(_Register) \
    *(volatile UINT32 *)((UINT8 *)EfiOmap4ScrmAddress + (_Register))

#define OMAP4_WRITE_SCRM_REGISTER(_Register, _Value)                     \
    *((volatile UINT32 *)((UINT8 *)EfiOmap4ScrmAddress + (_Register))) = \
    (_Value)

//
// These macros read from and write to the OMAP4 High Speed USB Host block.
//

#define OMAP4_READ_HS_USB_HOST_REGISTER(_Register) \
    *(volatile UINT32 *)((UINT8 *)EfiOmap4HsUsbHostAddress + (_Register))

#define OMAP4_WRITE_HS_USB_HOST_REGISTER(_Register, _Value)                   \
    *((volatile UINT32 *)((UINT8 *)EfiOmap4HsUsbHostAddress + (_Register))) = \
                                                                       (_Value)

//
// These macros read from and write to the OMAP4 EHCI registers.
//

#define OMAP4_READ_EHCI_REGISTER(_Register) \
        *(volatile UINT32 *)((UINT8 *)EfiOmap4EhciAddress + (_Register))

#define OMAP4_WRITE_EHCI_REGISTER(_Register, _Value)                         \
        *((volatile UINT32 *)((UINT8 *)EfiOmap4EhciAddress + (_Register))) = \
                                                                       (_Value)

//
// These macros read from and write to the OMAP4 USB TLL config registers.
//

#define OMAP4_READ_USB_TLL_CONFIG_REGISTER(_Register) \
        *(volatile UINT32 *)((UINT8 *)EfiOmap4UsbTllConfigAddress + (_Register))

#define OMAP4_WRITE_USB_TLL_CONFIG_REGISTER(_Register, _Value)               \
        *((volatile UINT32 *)((UINT8 *)EfiOmap4UsbTllConfigAddress +         \
                             (_Register))) = (_Value)

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4_HS_USB_PORT_COUNT 1

#define OMAP4_L3_INIT_CM2_USB_HOST_PORT_1_UTMI_EXTERNALLY_CLOCKED (1 << 24)
#define OMAP4_L3_INIT_CM2_USB_HOST_48_MHZ_CLOCK_ENABLED (1 << 15)
#define OMAP4_L3_INIT_CM2_USB_HOST_MODULE_ENABLED (0x2 << 0)

#define OMAP4_L3_INIT_CM2_USB_TLL_ENABLED (0x1 << 0)

#define OMAP4_L3_INIT_CM2_FULL_SPEED_USB_CLOCK_ENABLED (0x2 << 0)

#define OMAP4_L3_INIT_CM2_USB_PHY_48_MHZ_CLOCK_ENABLE (1 << 8)
#define OMAP4_L3_INIT_CM2_USB_PHY_ENABLED (0x1 << 0)

#define OMAP4_USB_TLL_CLOCKS_ON_DURING_IDLE (1 << 8)
#define OMAP4_USB_TLL_CONFIG_NO_IDLE (0x1 << 3)
#define OMAP4_USB_TLL_CONFIG_WAKEUP_ENABLE (1 << 2)
#define OMAP4_USB_TLL_CONFIG_SOFT_RESET (1 << 1)

#define OMAP4_USB_TLL_STATUS_RESET_DONE (1 << 0)

#define OMAP4_AUX_CLOCK_DIVIDE_BY_2 (0x1 << 16)
#define OMAP4_AUX_CLOCK_ENABLED (1 << 8)

#define OMAP4_ALT_CLOCK_ENABLE_EXT (1 << 3)
#define OMAP4_ALT_CLOCK_ENABLE_INT (1 << 2)
#define OMAP4_ALT_CLOCK_ACTIVE (0x1 << 0)

#define OMAP4_HS_USB_SYSTEM_CONFIG_STANDBY_MASK (0x3 << 4)
#define OMAP4_HS_USB_SYSTEM_CONFIG_NO_STANDBY   (0x1 << 4)
#define OMAP4_HS_USB_SYSTEM_CONFIG_IDLE_MASK    (0x3 << 2)
#define OMAP4_HS_USB_SYSTEM_CONFIG_NO_IDLE      (0x1 << 2)

#define OMAP4_HS_USB_HOST_CONFIG_INCR4_ENABLE  (1 << 2)
#define OMAP4_HS_USB_HOST_CONFIG_INCR8_ENABLE  (1 << 3)
#define OMAP4_HS_USB_HOST_CONFIG_INCR16_ENABLE (1 << 4)
#define OMAP4_HS_USB_HOST_CONFIG_INCR_ALIGNED  (1 << 5)
#define OMAP4_HS_USB_P1_MODE_MASK              (0x3 << 16)
#define OMAP4_HS_USB_P2_MODE_MASK              (0x3 << 18)

#define OMAP4_EHCI_INSNREG4_DISABLE_UNSUSPEND  (1 << 5)

#define OMAP4_EHCI_INSNREG5_ULPI_DIRECT_REGISTER_ADDRESS_SHIFT 16
#define OMAP4_EHCI_INSNREG5_ULPI_WRITE                         (0x2 << 22)
#define OMAP4_EHCI_INSNREG5_ULPI_PORT_SHIFT                    24
#define OMAP4_EHCI_INSNREG5_ULPI_START_ACCESS                  (1 << 31)

#define ULPI_FUNCTION_CONTROL       0x04
#define ULPI_FUNCTION_CONTROL_RESET (1 << 5)

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _OMAP4_L3_INIT_CM2_REGISTER {
    Omap4L3InitCm2UsbHostClockControl = 0x58, // CM_L3INIT_HSUSBHOST_CLKCTRL
    Omap4L3InitCm2UsbTllClockControl  = 0x68, // CM_L3INIT_HSUSBTLL_CLKCTRL
    Omap4L3InitCm2FullSpeedUsbClockControl = 0xD0, // CM_L3INIT_FSUSB_CLKCTRL
    Omap4L3InitCm2UsbPhyClockControl  = 0xE0, // CM_L3INIT_USBPHY_CLKCTRL
} OMAP4_L3_INIT_CM2_REGISTER, POMAP4_L3_INIT_CM2_REGISTER;

typedef enum _OMAP4_SCRM_REGISTER {
    Omap4ScrmAltClockSource = 0x110, // ALTCLKSRC
    Omap4ScrmAuxClock3      = 0x31C, // AUXCLK3
} OMAP4_SCRM_REGISTER, *POMAP4_SCRM_REGISTER;

typedef enum _OMAP4_HS_USB_HOST_REGISTER {
    Omap4HsUsbHostSystemConfiguration = 0x10, // UHH_SYSCONFIG
    Omap4HsUsbHostConfiguration       = 0x40, // UHH_HOSTCONFIG
} OMAP4_HS_USB_HOST_REGISTER, POMAP4_HS_USB_HOST_REGISTER;

typedef enum _OMAP4_EHCI_REGISTER {
    Omap4EhciImplementationRegister4 = 0xA0, // INSNREG04
    Omap4EhciImplementationRegister5 = 0xA4, // INSNREG05
} OMAP4_EHCI_REGISTER, *POMAP4_EHCI_REGISTER;

typedef enum _OMAP4_USB_TLL_CONFIG_REGISTER {
    Omap4UsbTllSystemConfiguration = 0x10, // USBTLL_SYSCONFIG
    Omap4UsbTllSystemStatus        = 0x14, // USBTLL_SYSSTATUS
} OMAP4_USB_TLL_CONFIG_REGISTER, *POMAP4_USB_TLL_CONFIG_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the L3 INIT CM2 register block.
//

VOID *EfiOmap4L3InitCm2Address = (VOID *)OMAP4430_L3_INIT_CM2_BASE;

//
// Store a pointer to the SCRM.
//

VOID *EfiOmap4ScrmAddress = (VOID *)OMAP4430_SCRM_BASE;

//
// Store a pointer to the High Speed USB Host block.
//

VOID *EfiOmap4HsUsbHostAddress = (VOID *)OMAP4430_HS_USB_HOST_BASE;

//
// Store a pointer to the EHCI register base.
//

VOID *EfiOmap4EhciAddress = (VOID *)OMAP4430_EHCI_BASE;

//
// Store a pointer to the USB TLL configuration register base.
//

VOID *EfiOmap4UsbTllConfigAddress = (VOID *)OMAP4430_USB_TLL_CONFIG_BASE;

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipOmap4UsbInitialize (
    VOID
    )

/*++

Routine Description:

    This routine performs any board-specific high speed USB initialization.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 PortIndex;
    UINT32 Value;

    //
    // Enable the USB module's clocks.
    //

    Value = OMAP4_L3_INIT_CM2_USB_HOST_PORT_1_UTMI_EXTERNALLY_CLOCKED |
            OMAP4_L3_INIT_CM2_USB_HOST_MODULE_ENABLED;

    OMAP4_WRITE_L3_INIT_CM2_REGISTER(Omap4L3InitCm2UsbHostClockControl, Value);
    Value = OMAP4_L3_INIT_CM2_FULL_SPEED_USB_CLOCK_ENABLED;
    OMAP4_WRITE_L3_INIT_CM2_REGISTER(Omap4L3InitCm2FullSpeedUsbClockControl,
                                     Value);

    Value = OMAP4_L3_INIT_CM2_USB_TLL_ENABLED;
    OMAP4_WRITE_L3_INIT_CM2_REGISTER(Omap4L3InitCm2UsbTllClockControl, Value);
    Value = (1 << 9) |
            OMAP4_L3_INIT_CM2_USB_PHY_48_MHZ_CLOCK_ENABLE |
            OMAP4_L3_INIT_CM2_USB_PHY_ENABLED;

    OMAP4_WRITE_L3_INIT_CM2_REGISTER(Omap4L3InitCm2UsbPhyClockControl, Value);

    //
    // Reset the USB TLL module, and wait for reset to complete.
    //

    Value = OMAP4_USB_TLL_CONFIG_SOFT_RESET;
    OMAP4_WRITE_USB_TLL_CONFIG_REGISTER(Omap4UsbTllSystemConfiguration, Value);
    do {
        Value = OMAP4_READ_USB_TLL_CONFIG_REGISTER(Omap4UsbTllSystemStatus);

    } while ((Value & OMAP4_USB_TLL_STATUS_RESET_DONE) == 0);

    Value = OMAP4_USB_TLL_CLOCKS_ON_DURING_IDLE |
            OMAP4_USB_TLL_CONFIG_NO_IDLE |
            OMAP4_USB_TLL_CONFIG_WAKEUP_ENABLE;

    OMAP4_WRITE_USB_TLL_CONFIG_REGISTER(Omap4UsbTllSystemConfiguration, Value);

    //
    // The USB3320C ULPI PHY's clock is fed by fref_clk3_out, a pin sourced by
    // AUXCLK3. Enable that puppy and set it to run at the required 19.2MHz,
    // half of the system clock's 38.4MHz.
    //

    Value = OMAP4_AUX_CLOCK_DIVIDE_BY_2 | OMAP4_AUX_CLOCK_ENABLED;
    OMAP4_WRITE_SCRM_REGISTER(Omap4ScrmAuxClock3, Value);
    Value = OMAP4_ALT_CLOCK_ENABLE_EXT | OMAP4_ALT_CLOCK_ENABLE_INT |
            OMAP4_ALT_CLOCK_ACTIVE;

    OMAP4_WRITE_SCRM_REGISTER(Omap4ScrmAltClockSource, Value);

    //
    // Set up the serial configuration (ULPI bypass) and burst configuration.
    //

    Value = OMAP4_READ_HS_USB_HOST_REGISTER(Omap4HsUsbHostSystemConfiguration);
    Value &= ~(OMAP4_HS_USB_SYSTEM_CONFIG_STANDBY_MASK |
               OMAP4_HS_USB_SYSTEM_CONFIG_IDLE_MASK);

    Value |= OMAP4_HS_USB_SYSTEM_CONFIG_NO_STANDBY |
             OMAP4_HS_USB_SYSTEM_CONFIG_NO_IDLE;

    OMAP4_WRITE_HS_USB_HOST_REGISTER(Omap4HsUsbHostSystemConfiguration, Value);
    Value = OMAP4_READ_HS_USB_HOST_REGISTER(Omap4HsUsbHostConfiguration);
    Value |= OMAP4_HS_USB_HOST_CONFIG_INCR4_ENABLE |
             OMAP4_HS_USB_HOST_CONFIG_INCR8_ENABLE |
             OMAP4_HS_USB_HOST_CONFIG_INCR16_ENABLE;

    Value &= ~(OMAP4_HS_USB_HOST_CONFIG_INCR_ALIGNED |
               OMAP4_HS_USB_P1_MODE_MASK |
               OMAP4_HS_USB_P2_MODE_MASK);

    OMAP4_WRITE_HS_USB_HOST_REGISTER(Omap4HsUsbHostConfiguration, Value);

    //
    // Turn on the magic disable unsuspend bit to prevent the root hub from
    // coming out of suspend when the run bit is cleared.
    //

    OMAP4_WRITE_EHCI_REGISTER(Omap4EhciImplementationRegister4,
                              OMAP4_EHCI_INSNREG4_DISABLE_UNSUSPEND);

    //
    // Set GPIO 62 to take the USB3320C PHY out of reset.
    //

    Value = READ_GPIO2_REGISTER(OmapGpioOutputEnable);
    WRITE_GPIO2_REGISTER(OmapGpioOutputEnable, Value & ~(1 << (62 - 32)));
    WRITE_GPIO2_REGISTER(OmapGpioOutputSet, (1 << (62 - 32)));

    //
    // Reset the PHY on each port.
    //

    for (PortIndex = 0; PortIndex < OMAP4_HS_USB_PORT_COUNT; PortIndex += 1) {

        //
        // Send a RESET command, which is a write, to the function control
        // address of the given port.
        //

        Value = ULPI_FUNCTION_CONTROL_RESET |
                (ULPI_SET_REGISTER(ULPI_FUNCTION_CONTROL) <<
                 OMAP4_EHCI_INSNREG5_ULPI_DIRECT_REGISTER_ADDRESS_SHIFT) |
                OMAP4_EHCI_INSNREG5_ULPI_WRITE |
                ((PortIndex + 1) << OMAP4_EHCI_INSNREG5_ULPI_PORT_SHIFT) |
                OMAP4_EHCI_INSNREG5_ULPI_START_ACCESS;

        OMAP4_WRITE_EHCI_REGISTER(Omap4EhciImplementationRegister5, Value);
        do {
            Value = OMAP4_READ_EHCI_REGISTER(Omap4EhciImplementationRegister5);

        } while ((Value & OMAP4_EHCI_INSNREG5_ULPI_START_ACCESS) != 0);
    }

    //
    // Set GPIO 1 to enable the TPS73633 LDO which provides power to the root
    // hub slash ethernet combo. For the output enable register, when a bit is
    // 0, then the GPIO is in output mode.
    //

    Value = READ_GPIO1_REGISTER(OmapGpioOutputEnable);
    WRITE_GPIO1_REGISTER(OmapGpioOutputEnable, Value & ~(1 << 1));
    WRITE_GPIO1_REGISTER(OmapGpioOutputSet, (1 << 1));
    return;
}

