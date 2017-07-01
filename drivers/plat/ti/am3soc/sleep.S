/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sleep.S

Abstract:

    This module implements low level sleep code for TI AM33xx SoCs.

Author:

    Evan Green 1-Oct-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/arm.inc>

//
// ---------------------------------------------------------------- Definitions
//

#define AM335_GPIO1_OUTPUT_SET 0x4804C194
#define AM335_GPIO1_OUTPUT_CLEAR 0x4804C190

#define AM335_EMIF_0_REGISTERS 0x4C000000
#define AM335_PRCM_REGISTERS 0x44E00000
#define AM335_SOC_CONTROL_REGISTERS 0x44E10000

#define AM335_EMIF_SDRAM_CONFIG 0x08
#define AM335_EMIF_SDRAM_REF_CONTROL 0x10
#define AM335_EMIF_SDRAM_REF_CONTROL_SHADOW 0x14
#define AM335_EMIF_SDRAM_TIM_1 0x18
#define AM335_EMIF_SDRAM_TIM_1_SHADOW 0x1C
#define AM335_EMIF_SDRAM_TIM_2 0x20
#define AM335_EMIF_SDRAM_TIM_2_SHADOW 0x24
#define AM335_EMIF_SDRAM_TIM_3 0x28
#define AM335_EMIF_SDRAM_TIM_3_SHADOW 0x2C
#define AM335_EMIF_POWER_MANAGEMENT_CONTROL 0x38
#define AM335_EMIF_POWER_MANAGEMENT_SHADOW 0x3C
#define AM335_EMIF_ZQ_CONFIG 0xC8
#define AM335_EMIF_DDR_PHY_CONTROL_1 0xE4

#define AM335_CM_PER_EMIF_CLOCK_CONTROL_MODE_DISABLED 0x00000003
#define AM335_CM_PER_EMIF_CLOCK_CONTROL_MODE_ENABLED 0x00000002
#define AM335_CM_PER_EMIF_CLOCK_CONTROL_STATUS_DISABLED 0x00030000

#define AM335_CM_MPU_MPU_CLOCK_CONTROL_MODE_MASK 0x00000003
#define AM335_CM_MPU_MPU_CLOCK_CONTROL_MODE_ENABLED 0x00000002

#define AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU 0x088
#define AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER 0x08C
#define AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE 0x090
#define AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR 0x094
#define AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP 0x098

#define AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU 0x020
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU 0x02C
#define AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DDR 0x034
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DDR 0x040
#define AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP 0x048
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP 0x054
#define AM335_CM_WAKEUP_IDLE_STATUS_DPLL_CORE 0x05C
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_CORE 0x068
#define AM335_CM_WAKEUP_IDLE_STATUS_DPLL_PER 0x070
#define AM335_CM_WAKEUP_CLOCK_DCO_LDO_DPLL_PER 0x7C

#define AM335_EMIF_POWER_CONTROL_SELF_REFRESH_64 (0x3 << 4)
#define AM335_EMIF_POWER_CONTROL_SELF_REFRESH_8192 (0xA << 4)
#define AM335_EMIF_POWER_CONTROL_CLOCK_STOP (0x1 << 8)
#define AM335_EMIF_POWER_CONTROL_SELF_REFRESH (0x2 << 8)
#define AM335_EMIF_POWER_CONTROL_POWER_DOWN (0x4 << 8)

#define AM335_DDR_START 0x80000000

#define AM335_CM_PER_OFFSET 0x0000
#define AM335_CM_WAKEUP_OFFSET 0x0400
#define AM335_CM_MPU_OFFSET 0x0600
#define AM335_PRM_DEVICE_OFFSET 0x0F00

#define AM335_CM_WAKEUP_REGISTERS \
    (AM335_PRCM_REGISTERS + AM335_CM_WAKEUP_OFFSET)

#define AM335_CM_PER_EMIF_CLOCK_CONTROL \
    (AM335_PRCM_REGISTERS + AM335_CM_PER_OFFSET + 0x0028)

#define AM335_CM_MPU_MPU_CLOCK_CONTROL \
    (AM335_PRCM_REGISTERS + AM335_CM_MPU_OFFSET + 0x4)

#define AM335_PRM_LDO_SRAM_MPU_CONTROL \
    (AM335_PRCM_REGISTERS + AM335_PRM_DEVICE_OFFSET + 0x1C)

#define AM335_DDR_IO_CONTROL (AM335_SOC_CONTROL_REGISTERS + 0xE04)
#define AM335_DDR_VTP_CONTROL (AM335_SOC_CONTROL_REGISTERS + 0xE0C)
#define AM335_DDR_DATA0_IO_CONTROL (AM335_SOC_CONTROL_REGISTERS + 0x1440)
#define AM335_DDR_DATA1_IO_CONTROL (AM335_SOC_CONTROL_REGISTERS + 0x1444)

#define AM335_DDR_IO_CONTROL_MDDR (1 << 28)
#define AM335_DDR_IO_CONTROL_RESET_DEFAULT (1 << 31)
#define AM335_DDR_IO_CONTROL_SLEEP_VALUE \
    (AM335_DDR_IO_CONTROL_MDDR | AM335_DDR_IO_CONTROL_RESET_DEFAULT)

#define AM335_DDR_IO_CONTROL_WAKE_VALUE 0

#define AM335_DDR_DATA_IO_CONTROL_SLEEP_VALUE 0x3FF00003
#define AM335_DDR_DATA_IO_CONTROL_RESUME_VALUE 0x18B

#define AM335_DDR_VTP_CONTROL_SLEEP_VALUE 0x00010117
#define AM335_DDR_VTP_CONTROL_FILTER_VALUE 0x00000006
#define AM335_DDR_VTP_CONTROL_ENABLE 0x00000040
#define AM335_DDR_VTP_CONTROL_CLEARZ 0x00000001
#define AM335_DDR_VTP_CONTROL_READY 0x00000020

#define AM335_PRM_LDO_SRAM_MPU_CONTROL_RETENTION_ENABLE 0x00000001

#define AM335_PLL_MODE_MASK 0x00000007
#define AM335_PLL_MODE_BYPASS 0x00000005
#define AM335_PLL_MODE_IDLE_BYPASS_FAST_RELOCK 0x00000006
#define AM335_PLL_MODE_LOCK 0x00000007

#define AM335_PLL_STATUS_BYPASS 0x00000100
#define AM335_PLL_STATUS_LOCKED 0x00000001

#define BEAGLEBONE_BLACK_LED1 (1 << 21)

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// These functions are defined in ARM mode, which is okay since the MMU-disable
// transition code uses blx jumps to possibly transition between modes.
//

.arm

.globl Am3SocOcmcCode
.globl Am3SocRefreshWfi
.globl Am3SocStandby
.globl Am3SocResumeStandby
.globl Am3SocSleep
.globl Am3SocResume
.globl Am3SocResumeAddress
.globl Am3SocOcmcCodeEnd

Am3SocOcmcCode:

//
// This macro is useful for debugging on the BeagleBone Black.
//

.macro SetLeds, Value
    ldr     %r3, =AM335_GPIO1_OUTPUT_CLEAR
    ldr     %r12, =(0xF << 21)
    str     %r12, [%r3]
    ldr     %r3, =AM335_GPIO1_OUTPUT_SET
    ldr     %r12, =(\Value << 21)
    str     %r12, [%r3]
.endm

.macro Am3SocEmifSelfRefresh
    ldr     %r0, =AM335_EMIF_0_REGISTERS    @ Get EMIF base.
    ldr     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_CONTROL]
    orr     %r1, %r1, #AM335_EMIF_POWER_CONTROL_SELF_REFRESH_64
    str     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_CONTROL]
    str     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_SHADOW]
    ldr     %r2, =AM335_DDR_START       @ Perform a DDR access to cause
    ldr     %r2, [%r2]                  @ The changes to take effect.
    ldr     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_CONTROL]
    orr     %r1, %r1, #AM335_EMIF_POWER_CONTROL_SELF_REFRESH
    str     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_CONTROL]
    DSB
.endm

.macro Am3SocEmifDisableSelfRefresh
    ldr     %r0, =AM335_EMIF_0_REGISTERS    @ Get EMIF base.
    mov     %r1, #0
    str     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_CONTROL]
    ldr     %r2, =AM335_DDR_START           @ Perform a DDR access...
    ldr     %r2, [%r2]                      @ ... for fun.
.endm

.macro Am3SocWfi
    DSB
    wfi
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
.endm

//
// VOID
// Am3SocRefreshWfi (
//     VOID
//     )
//

/*++

Routine Description:

    This routine puts the RAM in self-refresh and executes a WFI. This routine
    is called with the MMU and caches disabled.

Arguments:

    None.

Return Value:

    None.

--*/

Am3SocRefreshWfi:

    //
    // Put the DDR into self refresh.
    //

    Am3SocEmifSelfRefresh

    //
    // Turn off LED1.
    // TODO: Remove this as it's BeagleBone Black specific (but super cool).
    //

    ldr     %r3, =AM335_GPIO1_OUTPUT_CLEAR      @ Load GPIO clear address.
    ldr     %r2, =BEAGLEBONE_BLACK_LED1         @ Load LED pin.
    str     %r2, [%r3]                          @ Turn off LED.

    //
    // Go down. Add enough nops to ensure the A8 pipeline is clean.
    //

    Am3SocWfi

    //
    // Turn on the LED to indicate the core is running.
    // TODO: Remove this as it's BeagleBone Black specific.
    //

    ldr     %r3, =AM335_GPIO1_OUTPUT_SET        @ Load GPIO set address.
    ldr     %r2, =BEAGLEBONE_BLACK_LED1         @ Load LED pin.
    str     %r2, [%r3]                          @ Turn on LED.

    //
    // Disable self-refresh.
    //

    Am3SocEmifDisableSelfRefresh
    bx      %lr             @ Return.

//
// VOID
// Am3SocStandby (
//     ULONG ResumeAddress
//     )
//

/*++

Routine Description:

    This routine contains the low level sleep code needed to go down for
    standby, where all the peripherals are still up, and the memory is in
    self-refresh. This routine is called with the MMU and caches disabled.

Arguments:

    ResumeAddress - Supplies the physical address to resume to.

Return Value:

    None.

--*/

Am3SocStandby:
    str     %lr, Am3SocAbortAddress     @ Save return address for failure.

    //
    // Save the OS resume physical address into the global in OCMC RAM.
    //

    str     %r0, Am3SocResumeAddress    @ Save OS resume address.

    //
    // Put the DDR into self refresh.
    //

    Am3SocEmifSelfRefresh

    //
    // Set the MPU clock control register to disable.
    //

    ldr     %r0, =AM335_CM_MPU_MPU_CLOCK_CONTROL
    ldr     %r1, [%r0]
    bic     %r1, %r1, #AM335_CM_MPU_MPU_CLOCK_CONTROL_MODE_MASK
    str     %r1, [%r0]

    //
    // Put the PLLs into bypass mode. The display is still active in standby,
    // so CORE, DDR, and PER need to stay locked.
    //

    ldr     %r12, =AM335_CM_WAKEUP_REGISTERS    @ Get CM wakeup base.
    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP
    adrl    %r1, Am3SocDpllDispControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP
    bl      Am3SocPllBypass

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU
    adrl    %r1, Am3SocDpllMpuControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU
    bl      Am3SocPllBypass

    //
    // Turn off LED1.
    // TODO: Remove this as it's BeagleBone Black specific (but super cool).
    //

    ldr     %r3, =AM335_GPIO1_OUTPUT_CLEAR      @ Load GPIO clear address.
    ldr     %r2, =BEAGLEBONE_BLACK_LED1         @ Load LED pin.
    str     %r2, [%r3]                          @ Turn off LED.

    //
    // Go down. Add enough nops to ensure the A8 pipeline is clean.
    //

    Am3SocWfi

    //
    // Execution might resume here if the A8 came out of WFI before the Cortex
    // M3 could take it down. Make the resume address the abort address and
    // resume normally.
    //

    ldr     %r0, Am3SocAbortAddress
    str     %r0, Am3SocResumeAddress

    //
    // Real resume starts here.
    //

Am3SocResumeStandby:

    //
    // Turn on the LED to indicate the core is running.
    // TODO: Remove this as it's BeagleBone Black specific.
    //

    ldr     %r3, =AM335_GPIO1_OUTPUT_SET        @ Load GPIO set address.
    ldr     %r2, =BEAGLEBONE_BLACK_LED1         @ Load LED pin.
    str     %r2, [%r3]                          @ Turn on LED.

    //
    // Set the MPU module mode to enabled.
    //

    ldr     %r0, =AM335_CM_MPU_MPU_CLOCK_CONTROL
    ldr     %r1, [%r0]
    bic     %r1, %r1, #AM335_CM_MPU_MPU_CLOCK_CONTROL_MODE_MASK
    orr     %r1, %r1, #AM335_CM_MPU_MPU_CLOCK_CONTROL_MODE_ENABLED
    str     %r1, [%r0]

    //
    // Re-lock the PLLs.
    //

    ldr     %r12, =AM335_CM_WAKEUP_REGISTERS    @ Get CM wakeup base.
    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU
    ldr     %r1, Am3SocDpllMpuControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU
    bl      Am3SocPllRestore

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP
    ldr     %r1, Am3SocDpllDispControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP
    bl      Am3SocPllRestore

    //
    // Disable self-refresh.
    //

    Am3SocEmifDisableSelfRefresh

    //
    // Jump to the restore entry point, which is either the resume address or
    // the original return address of this function, depending on whether or
    // not the context was actually lost.
    //

    ldr     %lr, Am3SocResumeAddress     @ Get the resume location.
    bx      %lr             @ Return.

//
// VOID
// Am3SocSleep (
//     ULONG ResumeAddress
//     )
//

/*++

Routine Description:

    This routine contains the low level sleep code needed to go down to a
    suspend or deep sleep state. This routine is called with the MMU and caches
    disabled.

Arguments:

    ResumeAddress - Supplies the physical address to resume to.

Return Value:

    None.

--*/

Am3SocSleep:
    str     %lr, Am3SocAbortAddress     @ Save return address for failure.

    //
    // Save the OS resume physical address into the global in OCMC RAM.
    //

    str     %r0, Am3SocResumeAddress    @ Save OS resume address.

    //
    // Save EMIF configuration.
    //

    ldr     %r0, =AM335_EMIF_0_REGISTERS    @ Get EMIF base.
    ldr     %r1, [%r0, #AM335_EMIF_SDRAM_CONFIG]
    str     %r1, Am3SocSdramConfig
    ldr     %r1, [%r0, #AM335_EMIF_SDRAM_REF_CONTROL]
    str     %r1, Am3SocRefreshControl
    ldr     %r1, [%r0, #AM335_EMIF_SDRAM_TIM_1]
    str     %r1, Am3SocTiming1
    ldr     %r1, [%r0, #AM335_EMIF_SDRAM_TIM_2]
    str     %r1, Am3SocTiming2
    ldr     %r1, [%r0, #AM335_EMIF_SDRAM_TIM_3]
    str     %r1, Am3SocTiming3
    ldr     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_CONTROL]
    str     %r1, Am3SocPowerControl
    ldr     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_SHADOW]
    str     %r1, Am3SocPowerControlShadow
    ldr     %r1, [%r0, #AM335_EMIF_ZQ_CONFIG]
    str     %r1, Am3SocZqConfig
    ldr     %r1, [%r0, #AM335_EMIF_DDR_PHY_CONTROL_1]
    str     %r1, Am3SocPhyControl1
    DSB

    //
    // Put the DDR into self refresh.
    //

    Am3SocEmifSelfRefresh

    //
    // Disable EMIF.
    //

    ldr     %r0, =AM335_CM_PER_EMIF_CLOCK_CONTROL
    ldr     %r1, [%r0]
    bic     %r1, %r1, #AM335_CM_PER_EMIF_CLOCK_CONTROL_MODE_DISABLED
    str     %r1, [%r0]

Am3SocEmifDisableLoop:
    ldr     %r1, [%r0]
    ldr     %r2, =AM335_CM_PER_EMIF_CLOCK_CONTROL_STATUS_DISABLED
    cmp     %r1, %r2
    bne     Am3SocEmifDisableLoop

    //
    // Set the MPU clock control register to disable.
    //

    ldr     %r0, =AM335_CM_MPU_MPU_CLOCK_CONTROL
    ldr     %r1, [%r0]
    bic     %r1, %r1, #AM335_CM_MPU_MPU_CLOCK_CONTROL_MODE_MASK
    str     %r1, [%r0]

    //
    // Set the DDR3 default reset value and mDDR (CMOS) mode.
    //

    ldr     %r0, =AM335_DDR_IO_CONTROL
    ldr     %r1, =AM335_DDR_IO_CONTROL_SLEEP_VALUE
    str     %r1, [%r0]

    //
    // Set a weak pull-down for DQ and DM.
    //

    ldr     %r0, =AM335_DDR_DATA0_IO_CONTROL
    ldr     %r2, =AM335_DDR_DATA1_IO_CONTROL
    ldr     %r1, =AM335_DDR_DATA_IO_CONTROL_SLEEP_VALUE
    str     %r1, [%r0]
    str     %r1, [%r2]

    //
    // Disable VTP.
    //

    ldr     %r0, =AM335_DDR_VTP_CONTROL
    ldr     %r1, =AM335_DDR_VTP_CONTROL_SLEEP_VALUE
    str     %r1, [%r0]

    //
    // Enable SRAM retention mode.
    //

    ldr     %r0, =AM335_PRM_LDO_SRAM_MPU_CONTROL
    ldr     %r1, [%r0]
    orr     %r1, %r1, #AM335_PRM_LDO_SRAM_MPU_CONTROL_RETENTION_ENABLE
    str     %r1, [%r0]

    //
    // Put the PLLs into bypass mode.
    //

    ldr     %r12, =AM335_CM_WAKEUP_REGISTERS    @ Get CM wakeup base.
    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE
    adrl    %r1, Am3SocDpllCoreControl          @ Get relative address for save.
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_CORE
    bl      Am3SocPllBypass

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR
    adrl    %r1, Am3SocDpllDdrControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DDR
    bl      Am3SocPllBypass

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP
    adrl    %r1, Am3SocDpllDispControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP
    bl      Am3SocPllBypass

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER
    adrl    %r1, Am3SocDpllPerControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_PER
    bl      Am3SocPllBypass

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU
    adrl    %r1, Am3SocDpllMpuControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU
    bl      Am3SocPllBypass

    //
    // Turn off LED1.
    // TODO: Remove this as it's BeagleBone Black specific (but super cool).
    //

    ldr     %r3, =AM335_GPIO1_OUTPUT_CLEAR      @ Load GPIO clear address.
    ldr     %r2, =BEAGLEBONE_BLACK_LED1         @ Load LED pin.
    str     %r2, [%r3]                          @ Turn off LED.

    //
    // Go down. Add enough nops to ensure the A8 pipeline is clean.
    //

    Am3SocWfi

    //
    // Execution might resume here if the A8 came out of WFI before the Cortex
    // M3 could take it down. Make the resume address the abort address, then
    // resume normally.
    //

    ldr     %r0, Am3SocAbortAddress
    str     %r0, Am3SocResumeAddress

Am3SocResume:

    //
    // Turn on the LED to indicate the core is running.
    // TODO: Remove this as it's BeagleBone Black specific.
    //

    ldr     %r3, =AM335_GPIO1_OUTPUT_SET        @ Load GPIO set address.
    ldr     %r2, =BEAGLEBONE_BLACK_LED1         @ Load LED pin.
    str     %r2, [%r3]                          @ Turn on LED.

    //
    // Set the MPU module mode to enabled.
    //

    ldr     %r0, =AM335_CM_MPU_MPU_CLOCK_CONTROL
    ldr     %r1, [%r0]
    bic     %r1, %r1, #AM335_CM_MPU_MPU_CLOCK_CONTROL_MODE_MASK
    orr     %r1, %r1, #AM335_CM_MPU_MPU_CLOCK_CONTROL_MODE_ENABLED
    str     %r1, [%r0]

    //
    // Re-lock the PLLs.
    //

    ldr     %r12, =AM335_CM_WAKEUP_REGISTERS    @ Get CM wakeup base.
    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU
    ldr     %r1, Am3SocDpllMpuControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU
    bl      Am3SocPllRestore

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER
    ldr     %r1, Am3SocDpllPerControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_PER
    bl      Am3SocPllRestore

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP
    ldr     %r1, Am3SocDpllDispControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP
    bl      Am3SocPllRestore

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR
    ldr     %r1, Am3SocDpllDdrControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DDR
    bl      Am3SocPllRestore

    add     %r0, %r12, #AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE
    ldr     %r1, Am3SocDpllCoreControl
    add     %r2, %r12, #AM335_CM_WAKEUP_IDLE_STATUS_DPLL_CORE
    bl      Am3SocPllRestore

    //
    // Disable SRAM retention mode.
    //

    ldr     %r0, =AM335_PRM_LDO_SRAM_MPU_CONTROL
    ldr     %r1, [%r0]
    bic     %r1, %r1, #AM335_PRM_LDO_SRAM_MPU_CONTROL_RETENTION_ENABLE
    str     %r1, [%r0]

    //
    // Restore the pull resistor settings for DQ and DM.
    //

    ldr     %r0, =AM335_DDR_DATA0_IO_CONTROL
    ldr     %r2, =AM335_DDR_DATA1_IO_CONTROL
    ldr     %r1, =AM335_DDR_DATA_IO_CONTROL_RESUME_VALUE
    str     %r1, [%r0]
    str     %r1, [%r2]

    //
    // Enable VTP (changes refresh rate dynamically based on temperature, etc).
    //

    ldr     %r0, =AM335_DDR_VTP_CONTROL
    ldr     %r1, [%r0]
    mov     %r2, #0
    str     %r2, [%r0]      @ Write zero to VTP control.
    ldr     %r1, =AM335_DDR_VTP_CONTROL_FILTER_VALUE
    str     %r1, [%r0]      @ Write filter value to VTP control.
    ldr     %r1, [%r0]      @ Reload VTP control.
    orr     %r1, %r1, #AM335_DDR_VTP_CONTROL_ENABLE
    str     %r1, [%r0]      @ Write to enable VTP.
    ldr     %r1, [%r0]      @ Reload VTP control.
    bic     %r1, %r1, #AM335_DDR_VTP_CONTROL_CLEARZ
    str     %r1, [%r0]      @ Clear CLRZ bit.
    ldr     %r1, [%r0]      @ Reload VTP control.
    orr     %r1, %r1, #AM335_DDR_VTP_CONTROL_CLEARZ
    str     %r1, [%r0]      @ Set CLRZ bit.

Am3SocVtpReadyLoop:
    ldr     %r1, [%r0]      @ Get VTP control.
    tst     %r1, #AM335_DDR_VTP_CONTROL_READY
    beq     Am3SocVtpReadyLoop  @ Loop if the bit is set.

    //
    // Clear the DDR3 default reset value and mDDR (CMOS) mode.
    //

    ldr     %r0, =AM335_DDR_IO_CONTROL
    ldr     %r1, =AM335_DDR_IO_CONTROL_WAKE_VALUE
    str     %r1, [%r0]

    //
    // Enable EMIF.
    //

    ldr     %r0, =AM335_CM_PER_EMIF_CLOCK_CONTROL
    ldr     %r1, =AM335_CM_PER_EMIF_CLOCK_CONTROL_MODE_ENABLED
    str     %r1, [%r0]

Am3SocEmifEnableLoop:
    ldr     %r2, [%r0]
    cmp     %r1, %r2
    bne     Am3SocEmifEnableLoop

    //
    // Restore EMIF configuration.
    //

    ldr     %r0, =AM335_EMIF_0_REGISTERS    @ Get EMIF base.
    ldr     %r1, Am3SocPhyControl1
    str     %r1, [%r0, #AM335_EMIF_DDR_PHY_CONTROL_1]
    ldr     %r1, Am3SocTiming1
    str     %r1, [%r0, #AM335_EMIF_SDRAM_TIM_1]
    str     %r1, [%r0, #AM335_EMIF_SDRAM_TIM_1_SHADOW]
    ldr     %r1, Am3SocTiming2
    str     %r1, [%r0, #AM335_EMIF_SDRAM_TIM_2]
    str     %r1, [%r0, #AM335_EMIF_SDRAM_TIM_2_SHADOW]
    ldr     %r1, Am3SocTiming3
    str     %r1, [%r0, #AM335_EMIF_SDRAM_TIM_3]
    str     %r1, [%r0, #AM335_EMIF_SDRAM_TIM_3_SHADOW]
    ldr     %r1, Am3SocRefreshControl
    str     %r1, [%r0, #AM335_EMIF_SDRAM_REF_CONTROL]
    str     %r1, [%r0, #AM335_EMIF_SDRAM_REF_CONTROL_SHADOW]
    ldr     %r1, Am3SocPowerControl
    str     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_CONTROL]
    ldr     %r1, Am3SocPowerControlShadow
    str     %r1, [%r0, #AM335_EMIF_POWER_MANAGEMENT_SHADOW]
    ldr     %r1, Am3SocZqConfig
    str     %r1, [%r0, #AM335_EMIF_ZQ_CONFIG]
    ldr     %r1, Am3SocSdramConfig      @ Trigger the new config to activate.
    str     %r1, [%r0, #AM335_EMIF_SDRAM_CONFIG]
    DSB
    ldr     %r2, =AM335_DDR_START       @ Perform a DDR access...
    ldr     %r2, [%r2]                  @ ... for fun.

    //
    // Return to the resume (or abort) address.
    //

    ldr     %lr, Am3SocResumeAddress    @ Get the resume physical address.
    bx      %lr                         @ Branch to the OS resume address.

//
// VOID
// Am3SocPllBypass (
//     PVOID ClockModeRegister,
//     PULONG SavedMode,
//     PVOID IdleStateRegister
//     )
//

/*++

Routine Description:

    This routine puts the given PLL into bypass mode.

Arguments:

    ClockModeRegister - Supplies the address of the clock mode register.

    SavedMode - Supplies a pointer where the original mode of the PLL will be
        saved.

    IdleStateRegister - Supplies a pointer to the idle state register.

Return Value:

    None.

--*/

Am3SocPllBypass:
    ldr     %r3, [%r0]              @ Get clock mode register value.
    str     %r3, [%r1]              @ Save to storage location.
    bic     %r3, %r3, #AM335_PLL_MODE_MASK      @ Clear the mode mask.
    orr     %r3, %r3, #AM335_PLL_MODE_BYPASS    @ Set bypass mode.
    str     %r3, [%r0]              @ Write new clock mode.
    DSB                             @ Make sure the write gets out.

Am3SocPllBypassLoop:
    ldr     %r3, [%r2]              @ Get the idle status.
    tst     %r3, #AM335_PLL_STATUS_BYPASS   @ Check the bypass bit.
    bne     Am3SocPllBypassLoop
    bx      %lr

//
// VOID
// Am3SocPllRestore (
//     PVOID ClockModeRegister,
//     ULONG SavedMode,
//     PVOID IdleStateRegister
//     )
//

/*++

Routine Description:

    This routine restores the PLL clock register value saved previously, and
    if the previous mode was locked then it waits for it to lock.

Arguments:

    ClockModeRegister - Supplies the address of the clock mode register.

    SavedMode - Supplies the mode to restore.

    IdleStateRegister - Supplies a pointer to the idle state register.

Return Value:

    None.

--*/

Am3SocPllRestore:
    str     %r1, [%r0]              @ Set the new value.
    DSB                             @ Make sure the write gets out.
    and     %r1, %r1, #AM335_PLL_MODE_MASK      @ Isolate the mode portion.
    cmp     %r1, #AM335_PLL_MODE_LOCK           @ Compare against locked.
    bne     Am3SocPllRestoreReturn  @ Return if not locked before.

Am3SocPllRestoreLoop:
    ldr     %r3, [%r2]              @ Get the status register.
    tst     %r3, #AM335_PLL_STATUS_LOCKED   @ Test the locked bit.
    bne     Am3SocPllRestoreLoop    @ Jump back if not yet locked.

Am3SocPllRestoreReturn:
    bx      %lr

//
// -------------------------------------------------------------------- Globals
//

Am3SocResumeAddress:
    .long 0

Am3SocAbortAddress:
    .long 0

Am3SocSdramConfig:
    .long 0

Am3SocRefreshControl:
    .long 0

Am3SocTiming1:
    .long 0

Am3SocTiming2:
    .long 0

Am3SocTiming3:
    .long 0

Am3SocPowerControl:
    .long 0

Am3SocPowerControlShadow:
    .long 0

Am3SocZqConfig:
    .long 0

Am3SocPhyControl1:
    .long 0

Am3SocDpllCoreControl:
    .long 0

Am3SocDpllDdrControl:
    .long 0

Am3SocDpllDispControl:
    .long 0

Am3SocDpllPerControl:
    .long 0

Am3SocDpllMpuControl:
    .long 0

.ltorg

Am3SocOcmcCodeEnd:

