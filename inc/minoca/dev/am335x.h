/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    am335x.h

Abstract:

    This header contains definitions for the hardware modules supporting the
    TI AM335x SoCs.

Author:

    Evan Green 6-Jan-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros access interrupt controller registers.
//

#define AM335_INTC_LINE_TO_INDEX(_Line) ((_Line) >> 5)
#define AM335_INTC_LINE_TO_MASK(_Line) (1 << ((_Line) & 0x1F))
#define AM335_INTC_MASK(_Index) (Am335IntcMask + ((_Index) * 0x20))
#define AM335_INTC_MASK_CLEAR(_Index) (Am335IntcMaskClear + ((_Index) * 0x20))
#define AM335_INTC_MASK_SET(_Index) (Am335IntcMaskSet + ((_Index) * 0x20))
#define AM335_INTC_LINE(_Line) (Am335IntcLine + ((_Line) * 0x4))

//
// ---------------------------------------------------------------- Definitions
//

#define AM335_ALLOCATION_TAG 0x33336D41

//
// Define the signature of the AM335x ACPI table: AM33
//

#define AM335X_SIGNATURE 0x33334D41

//
// Define the number of timers in the SoC.
//

#define AM335X_TIMER_COUNT 8

//
// Define attributes of the timers.
//

#define AM335_TIMER_BIT_WIDTH 32
#define AM335_TIMER_FREQUENCY_32KHZ 32768
#define AM335_TIMER_CONTROLLER_SIZE 0x1000

//
// Define the size of the interrupt controller register space.
//

#define AM335_INTC_CONTROLLER_SIZE 0x1000

//
// Define the number of unique interrupt priorities in the INTC controller.
//

#define AM335_INTC_PRIORITY_COUNT 63

//
// Define PRCM offsets.
//

#define AM335_PRCM_SIZE 0x2000
#define AM335_CM_PER_OFFSET 0x0000
#define AM335_CM_WAKEUP_OFFSET 0x0400
#define AM335_CM_DPLL_OFFSET 0x0500
#define AM335_CM_MPU_OFFSET 0x0600
#define AM335_CM_DEVICE_OFFSET 0x0700
#define AM335_CM_RTC_OFFSET 0x0800
#define AM335_CM_GFX_OFFSET 0x0900
#define AM335_CM_CEFUSE_OFFSET 0x0A00
#define AM335_PRM_IRQ_OFFSET 0x0B00
#define AM335_PRM_PER_OFFSET 0x0C00
#define AM335_PRM_WAKEUP_OFFSET 0x0D00
#define AM335_PRM_MPU_OFFSET 0x0E00
#define AM335_PRM_DEVICE_OFFSET 0x0F00
#define AM335_PRM_RTC_OFFSET 0x1000
#define AM335_PRM_GFX_OFFSET 0x1100
#define AM335_PRM_CEFUSE_OFFSET 0x1200

//
// CM wakeup registers.
//

#define AM335_CM_WAKEUP_TIMER0_CLOCK_CONTROL 0x10
#define AM335_CM_WAKEUP_TIMER1_CLOCK_CONTROL 0x0C4

//
// Define CM DPLL registers.
//

#define AM335_CM_DPLL_CLOCK_SELECT_TIMER7 0x04
#define AM335_CM_DPLL_CLOCK_SELECT_TIMER2 0x08
#define AM335_CM_DPLL_CLOCK_SELECT_TIMER3 0x0C
#define AM335_CM_DPLL_CLOCK_SELECT_TIMER4 0x10
#define AM335_CM_DPLL_CLOCK_SELECT_TIMER5 0x18
#define AM335_CM_DPLL_CLOCK_SELECT_TIMER6 0x1C
#define AM335_CM_DPLL_CLOCK_SELECT_TIMER1 0x28

//
// CM DPLL clock select timer register bits (any timer except 1).
//

#define AM335_CM_DPLL_CLOCK_SELECT_TIMER_MASK 0x00000003
#define AM335_CM_DPLL_CLOCK_SELECT_TIMER_TCLKIN 0x0
#define AM335_CM_DPLL_CLOCK_SELECT_TIMER_SYSTEM_CLOCK 0x1
#define AM335_CM_DPLL_CLOCK_SELECT_TIMER_32KHZ 0x2

#define AM335_CM_PER_TIMER2_CLOCK_ENABLE 0x00000002

#define AM335_CM_WAKEUP_TIMER0_CLOCK_ENABLE 0x00000002

//
// CM Wakeup Timer1 PLL clock select register bits.
//

#define AM335_CM_DPLL_CLOCK_SELECT_TIMER1_32KHZ 0x00000001

//
// Define CM PER registers.
//

#define AM335_CM_PER_TIMER7_CLOCK_CONTROL 0x07C
#define AM335_CM_PER_TIMER2_CLOCK_CONTROL 0x080
#define AM335_CM_PER_TIMER3_CLOCK_CONTROL 0x084
#define AM335_CM_PER_TIMER4_CLOCK_CONTROL 0x088
#define AM335_CM_PER_TIMER5_CLOCK_CONTROL 0x0EC
#define AM335_CM_PER_TIMER6_CLOCK_CONTROL 0x0F0

//
// Define AM335 timer register bits.
//

//
// Idle bits.
//

#define AM335_TIMER_IDLEMODE_NOIDLE 0x00000080

//
// Mode bits.
//

#define AM335_TIMER_STARTED 0x00000001
#define AM335_TIMER_OVERFLOW_TRIGGER 0x00000400
#define AM335_TIMER_OVERFLOW_AND_MATCH_TRIGGER 0x00000800
#define AM335_TIMER_COMPARE_ENABLED 0x00000040
#define AM335_TIMER_AUTORELOAD 0x00000002

//
// Interrupt enable bits.
//

#define AM335_TIMER_MATCH_INTERRUPT 0x00000001
#define AM335_TIMER_OVERFLOW_INTERRUPT 0x00000002

#define AM335_TIMER_INTERRUPT_MASK 0x7

//
// Define AM335 interrupt controller register bits.
//

//
// Interrupt system configuration register bits.
//

#define AM335_INTC_SYSTEM_CONFIG_SOFT_RESET 0x00000002

//
// Interrupt system status register bits.
//

#define AM335_INTC_SYSTEM_STATUS_RESET_DONE 0x00000001

//
// Interrupt sorted IRQ/FIQ register bits.
//

#define AM335_INTC_SORTED_ACTIVE_MASK 0x0000007F
#define AM335_INTC_SORTED_SPURIOUS 0x00000080

//
// Interrupt line register bits.
//

#define AM335_INTC_LINE_IRQ 0x00000000
#define AM335_INTC_LINE_FIQ 0x00000001
#define AM335_INTC_LINE_PRIORITY_SHIFT 2

//
// Interrupt control register bits.
//

#define AM335_INTC_CONTROL_NEW_IRQ_AGREEMENT 0x00000001
#define AM335_INTC_CONTROL_NEW_FIQ_AGREEMENT 0x00000002

//
// Define internal I2C parameters (recommended convention).
//

#define AM335_I2C_SYSTEM_CLOCK_SPEED 48000000
#define AM335_I2C_INTERNAL_CLOCK_SPEED 12000000

//
// SoC control definitions
//

#define AM335_SOC_CONTROL_SIZE 0x2000

//
// Define SoC control device ID register bits.
//

#define AM335_SOC_CONTROL_DEVICE_ID_REVISION_SHIFT 0x1C
#define AM335_SOC_DEVICE_VERSION_1_0 0
#define AM335_SOC_DEVICE_VERSION_2_0 1
#define AM335_SOC_DEVICE_VERSION_2_1 2

//
// EFuse bit for OPP100 275MHz, 1.1v.
//

#define AM335_EFUSE_OPP100_275_MASK 0x00000001
#define AM335_EFUSE_OPP100_275 0

//
// EFuse bit for OPP100 500MHz, 1.1v.
//

#define AM335_EFUSE_OPP100_500_MASK 0x00000002
#define AM335_EFUSE_OPP100_500 1

//
// EFuse bit for OPP100 600MHz, 1.2v.
//

#define AM335_EFUSE_OPP120_600_MASK 0x00000004
#define AM335_EFUSE_OPP120_600 2

//
// EFuse bit for OPP Turbo 720MHz, 1.26v.
//

#define AM335_EFUSE_OPPTB_720_MASK 0x00000008
#define AM335_EFUSE_OPPTB_720 3

//
// EFuse bit for OPP50 300MHz, 1.1v.
//

#define AM335_EFUSE_OPP50_300_MASK 0x00000010
#define AM335_EFUSE_OPP50_300 4

//
// EFuse bit for OPP100 300MHz, 1.1v.
//

#define AM335_EFUSE_OPP100_300_MASK 0x00000020
#define AM335_EFUSE_OPP100_300 5

//
// EFuse bit for OPP100 600MHz, 1.1v.
//

#define AM335_EFUSE_OPP100_600_MASK 0x00000040
#define AM335_EFUSE_OPP100_600 6

//
// EFuse bit for OPP120 700MHz, 1.2v.
//

#define AM335_EFUSE_OPP120_720_MASK 0x00000080
#define AM335_EFUSE_OPP120_720 7

//
// EFuse bit for OPP Turbo 800MHz, 1.26v.
//

#define AM335_EFUSE_OPPTB_800_MASK 0x00000100
#define AM335_EFUSE_OPPTB_800 8

//
// EFuse bit for OPP Turbo 1000MHz, 1.325v.
//

#define AM335_EFUSE_OPPNT_1000_MASK 0x00000200
#define AM335_EFUSE_OPPNT_1000 9
#define AM335_SOC_CONTROL_EFUSE_OPP_MASK 0x00001FFF
#define AM335_EFUSE_OPP_MAX (AM335_EFUSE_OPPNT_1000 + 1)

//
// CM Wakeup MPU PLL clock mode register bits.
//

#define AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU_ENABLE_MN_BYPASS 0x00000004
#define AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU_ENABLE 0x00000007

//
// CM Wakeup MPU PLL idle status register bits.
//

#define AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU_CLOCK 0x00000001
#define AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU_MN_BYPASS 0x00000100

//
// CM Wakeup MPU PLL clock select register bits.
//

#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_DIV_MASK 0x0000007F
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_DIV_SHIFT 0
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_MULT_MASK 0x0007FF00
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_MULT_SHIFT 8

//
// CM Wakeup MPU PLL M2 divisor register bits.
//

#define AM335_CM_WAKEUP_DIV_M2_DPLL_MPU_CLOCK_OUT_MASK 0x0000001F

//
// CM Wakeup Display PLL clock mode register bits.
//

#define AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP_ENABLE_MN_BYPASS 0x00000004
#define AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP_ENABLE 0x0000007

//
// CM Wakeup Display PLL idle status register bits.
//

#define AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP_MN_BYPASS 0x00000100
#define AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP_CLOCK 0x00000001

//
// CM Wakeup Display PLL clock select register bits.
//

#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP_DIV_MASK 0x0000007F
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP_DIV_SHIFT 0
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP_MULT_MASK 0x0007FF00
#define AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP_MULT_SHIFT 8

//
// CM Wakeup Display PLL M2 divider register bits.
//

#define AM335_CM_WAKEUP_DIV_M2_DPLL_DISP_CLOCK_OUT_MASK 0x0000001F

//
// Hardcoded PLL values.
//

#define AM335_MPU_PLL_N 23
#define AM335_MPU_PLL_M2 1

#define AM335_CORE_PLL_M 1000
#define AM335_CORE_PLL_N 23
#define AM335_CORE_PLL_HSDIVIDER_M4 10
#define AM335_CORE_PLL_HSDIVIDER_M5 8
#define AM335_CORE_PLL_HSDIVIDER_M6 4

#define AM335_PER_PLL_M 960
#define AM335_PER_PLL_N 23
#define AM335_PER_PLL_M2 5

#define AM335_DDR_PLL_M_DDR2 266
#define AM335_DDR_PLL_M_DDR3 303
#define AM335_DDR_PLL_N 23
#define AM335_DDR_PLL_M2 1

#define AM335_DISP_PLL_M 25
#define AM335_DISP_PLL_N 2
#define AM335_DISP_PLL_M2 1

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the TI AM335x ACPI table.

Members:

    Header - Stores the standard ACPI table header. The signature here is
        'AM33'.

    TimerBase - Stores the array of physical addresses of all the timers.

    TimerGsi - Stores the array of Global System Interrupt numbers for each of
        the timers.

    InterruptLineCount - Stores the number of interrupt lines in the interrupt
        controller (one beyond the highest valid line number).

    InterruptControllerBase - Stores the physical address of the INTC interrupt
        controller unit.

    PrcmBase - Stores the physical address of the PRCM registers.

--*/

typedef struct _AM335X_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG TimerBase[AM335X_TIMER_COUNT];
    ULONG TimerGsi[AM335X_TIMER_COUNT];
    ULONG InterruptLineCount;
    ULONGLONG InterruptControllerBase;
    ULONGLONG PrcmBase;
} PACKED AM335X_TABLE, *PAM335X_TABLE;

//
// Define the DM timer register offsets.
//

typedef enum _AM335_DM_TIMER_REGISTER {
    Am335TimerId                            = 0x00,
    Am335TimerOcpConfig                     = 0x10,
    Am335TimerEndOfInterrupt                = 0x14,
    Am335TimerRawInterruptStatus            = 0x24,
    Am335TimerInterruptStatus               = 0x28,
    Am335TimerInterruptEnableSet            = 0x2C,
    Am335TimerInterruptEnableClear          = 0x30,
    Am335TimerInterruptWakeEnable           = 0x34,
    Am335TimerControl                       = 0x38,
    Am335TimerCount                         = 0x3C,
    Am335TimerLoad                          = 0x40,
    Am335TimerTrigger                       = 0x44,
    Am335TimerWritePosting                  = 0x48,
    Am335TimerMatch                         = 0x4C,
    Am335TimerCapture1                      = 0x50,
    Am335TimerSynchronousInterfaceControl   = 0x54,
    Am335TimerCapture2                      = 0x58
} AM335_DM_TIMER_REGISTER, *PAM335_DM_TIMER_REGISTER;

//
// Define INTC register offsets.
//

typedef enum _AM335_INTC_REGISTER {
    Am335IntcSystemConfig                   = 0x010,
    Am335IntcSystemStatus                   = 0x014,
    Am335IntcSortedIrq                      = 0x040,
    Am335IntcSortedFiq                      = 0x044,
    Am335IntcControl                        = 0x048,
    Am335IntcIrqPriority                    = 0x060,
    Am335IntcFiqPriority                    = 0x064,
    Am335IntcThreshold                      = 0x068,
    Am335IntcMask                           = 0x084,
    Am335IntcMaskClear                      = 0x088,
    Am335IntcMaskSet                        = 0x08C,
    Am335IntcLine                           = 0x100,
} AM335_INTC_REGISTER, *PAM335_INTC_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the provided hardware layer services.
//

extern PHARDWARE_MODULE_KERNEL_SERVICES HlAm335KernelServices;

//
// Store a pointer to the AM335x ACPI table.
//

extern PAM335X_TABLE HlAm335Table;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
HlpAm335InitializePowerAndClocks (
    VOID
    );

/*++

Routine Description:

    This routine initializes the PRCM and turns on clocks and power domains
    needed by the system.

Arguments:

    None.

Return Value:

    Status code.

--*/

