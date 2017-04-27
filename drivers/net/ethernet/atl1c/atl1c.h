/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    atl1c.h

Abstract:

    This header contains internal definitions for the Atheros L1C and L2C
    ethernet controller families.

Author:

    Evan Green 18-Apr-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros for accessing a generic register in the controller.
//

#define ATL_READ_REGISTER32(_Controller, _Register) \
    HlReadRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define ATL_READ_REGISTER16(_Controller, _Register) \
    HlReadRegister16((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define ATL_WRITE_REGISTER32(_Controller, _Register, _Value)               \
    HlWriteRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

#define ATL_WRITE_REGISTER16(_Controller, _Register, _Value)               \
    HlWriteRegister16((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

#define ATL_MICROSECONDS(_Microseconds) \
    ((_Microseconds) / ATL_TICK_MICROSECONDS)

//
// ---------------------------------------------------------------- Definitions
//

#define ATL1C_ALLOCATION_TAG 0x436C7441 // 'CltA'

//
// Define the size of receive frame data.
//

#define ATL1C_RECEIVE_FRAME_DATA_SIZE 1536

//
// Define the number of transmit descriptors in the ring.
//

#define ATL1C_TRANSMIT_DESCRIPTOR_COUNT 128

//
// Define the number of receive buffers that will be allocated for the
// controller.
//

#define ATL1C_RECEIVE_FRAME_COUNT 32

//
// Define how long to wait for a free transmit descriptor before just giving
// up and trying anyway.
//

#define ATL1C_TRANSMIT_DESCRIPTOR_WAIT_INTERVAL WAIT_TIME_INDEFINITE

//
// Define the interrupt moderator timer values in units of 2us.
//

#define ATL_TRANSMIT_INTERRUPT_TIMER_VALUE 1000
#define ATL_RECEIVE_INTERRUPT_TIMER_VALUE 100

//
// Define how many microseconds to wait for the PHY MDIO module to complete a
// command.
//

#define ATL_MDIO_WAIT_LOOP_COUNT 120
#define ATL_MDIO_WAIT_LOOP_DELAY 10

//
// Define how many microseconds to wait for the unit to idle out of an
// operation.
//

#define ATL_IDLE_WAIT_LOOP_COUNT 10
#define ATL_IDLE_WAIT_LOOP_DELAY 1000

//
// Define how many microseconds to wait for a TWSI EEPROM operation.
//

#define ATL_TWSI_EEPROM_LOOP_COUNT 10
#define ATL_TWSI_EEPROM_LOOP_DELAY 10000

//
// Define transmit descriptor flags.
//

#define ATL_TRANSMIT_DESCRIPTOR_CHECKSUM_OFFSET_SHIFT 18
#define ATL_TRANSMIT_DESCRIPTOR_CHECKSUM_OFFSET_MASK 0x00FF
#define ATL_TRANSMIT_DESCRIPTOR_CHECKSUM_ENABLE 0x00000100
#define ATL_TRANSMIT_DESCRIPTOR_ETHERNET_FRAME 0x00020000
#define ATL_TRANSMIT_DESCRIPTOR_END_OF_PACKET 0x80000000

//
// Define ATL PCI command register bits.
//

#define ATL_PCI_COMMAND_ENABLE_IO         0x0001
#define ATL_PCI_COMMAND_ENABLE_MEMORY     0x0002
#define ATL_PCI_COMMAND_ENABLE_BUS_MASTER 0x0004
#define ATL_PCI_COMMAND_INTX_DISABLE      0x0400

//
// Define "Unc err sev" register bits.
//

#define ATL_PEX_UNC_ERR_SEV_DLP     0x00000010
#define ATL_PEX_UNC_ERR_SEV_FCP     0x00002000

//
// Define the Link Training and Status State Machine register value.
//

#define ATL_LTSSM_ID_ENABLE_WRO 0x1000

#define ATL_L2CB_MAX_TRANSMIT_LENGTH (6 * _1KB)
#define ATL_TRANSMIT_DESCRIPTOR_BURST_COUNT 5
#define ATL_L2CB_TRANSMIT_TXF_BURST_PREF 0x40
#define ATL_TRANSMIT_TCP_SEGMENTATION_OFFSET_FRAME_SIZE (6 * _1KB)
#define ATL_RECEIVE_DESCRIPTOR_BURST_COUNT 8

#define ATL_DMA_REQUEST_1024 3
#define ATL_PREAMBLE_LENGTH 7
#define ATL_TICK_MICROSECONDS 2

//
// Define master control bits.
//

#define ATL_MASTER_CONTROL_SOFT_RESET (1 << 0)
#define ATL_MASTER_CONTROL_OOB_DISABLE (1 << 6)
#define ATL_MASTER_CONTROL_SYSTEM_ALIVE_TIMER (1 << 7)
#define ATL_MASTER_CONTROL_TRANSMIT_ITIMER_ENABLE (1 << 10)
#define ATL_MASTER_CONTROL_RECEIVE_ITIMER_ENABLE (1 << 11)
#define ATL_MASTER_CONTROL_DISABLE_CLOCK_SWITCH (1 << 12)
#define ATL_MASTER_CONTROL_CLEAR_INTERRUPT_ON_READ (1 << 14)
#define ATL_MASTER_CONTROL_OTP_SEL (1 << 31)

//
// Define interrupt timer shifts.
//

#define ATL_INTERRUPT_TIMER_TRANSMIT_MASK 0xFFFF
#define ATL_INTERRUPT_TIMER_TRANSMIT_SHIFT 0
#define ATL_INTERRUPT_TIMER_RECEIVE_MASK 0xFFFF
#define ATL_INTERRUPT_TIMER_RECEIVE_SHIFT 16

//
// Define the "PHY Miscellaneous" register bits.
//

#define ATL_PHY_MISCELLANEOUS_FORCE_RECEIVE_DETECT (1 << 2)

//
// Define the "TWSI Control" register bits.
//

#define ATL_TWSI_CONTROL_SOFTWARE_LOAD_START (1 << 11)

//
// Define "TWSI Debug" register bits.
//

#define ATL_TWSI_DEBUG_DEVICE_EXISTS (1 << 29)

//
// Define the "OTP Control" register bits.
//

#define ATL_OTP_CONTROL_CLOCK_ENABLE (1 << 1)

//
// Define the power management register bits.
//

#define ATL_POWER_MANAGEMENT_CONTROL_L1_ENABLE (1 << 3)
#define ATL_POWER_MANAGEMENT_CONTROL_SERDES_L1_ENABLE (1 << 4)
#define ATL_POWER_MANAGEMENT_CONTROL_SERDES_PLL_L1_ENABLE (1 << 5)
#define ATL_POWER_MANAGEMENT_CONTROL_SERDES_PD_EX_L1 (1 << 6)
#define ATL_POWER_MANAGEMENT_CONTROL_SERDES_BUFS_RECEIVE_L1_ENABLE (1 << 7)
#define ATL_POWER_MANAGEMENT_CONTROL_L0S_ENABLE (1 << 12)
#define ATL_POWER_MANAGEMENT_CONTROL_CLK_SWH_L1 (1 << 13)
#define ATL_POWER_MANAGEMENT_CONTROL_L1_ENTRY_TIME 0x0F
#define ATL_POWER_MANAGEMENT_CONTROL_L1_ENTRY_TIME_MASK 0x0F
#define ATL_POWER_MANAGEMENT_CONTROL_L1_ENTRY_TIME_SHIFT 16
#define ATL_POWER_MANAGEMENT_CONTROL_ASPM_MAC_CHECK (1 << 30)

//
// Define PHY Control register bits.
//

#define ATL_PHY_CONTROL_EXT_RESET (1 << 0)
#define ATL_PHY_CONTROL_LED_MODE (1 << 2)
#define ATL_PHY_CONTROL_25MHZ_GATE_ENABLED (1 << 5)
#define ATL_PHY_CONTROL_IDDQ (1 << 7)
#define ATL_PHY_CONTROL_HIBERNATE_ENABLE (1 << 10)
#define ATL_PHY_CONTROL_HIBERNATE_PULSE (1 << 11)
#define ATL_PHY_CONTROL_SEL_ANA_RESET (1 << 12)
#define ATL_PHY_CONTROL_PLL_ENABLED (1 << 13)
#define ATL_PHY_CONTROL_POWER_DOWN (1 << 14)
#define ATL_PHY_CONTROL_100AB_ENABLE (1 << 17)

//
// Define the idle status register bits.
//

#define ATL_IDLE_STATUS_RECEIVE_MAC_BUSY (1 << 0)
#define ATL_IDLE_STATUS_TRANSMIT_MAC_BUSY (1 << 1)
#define ATL_IDLE_STATUS_RECEIVE_QUEUE_BUSY (1 << 2)
#define ATL_IDLE_STATUS_TRANSMIT_QUEUE_BUSY (1 << 3)
#define ATL_IDLE_IO_MASK                  \
    (ATL_IDLE_STATUS_RECEIVE_MAC_BUSY |   \
     ATL_IDLE_STATUS_TRANSMIT_MAC_BUSY |  \
     ATL_IDLE_STATUS_RECEIVE_QUEUE_BUSY | \
     ATL_IDLE_STATUS_TRANSMIT_QUEUE_BUSY)

//
// Define MDIO control register bits.
//

#define ATL_MDIO_CONTROL_CLOCK_25MHZ_DIVIDE_4 0
#define ATL_MDIO_CONTROL_DATA_MASK 0xFFFF
#define ATL_MDIO_CONTROL_DATA_SHIFT 0
#define ATL_MDIO_CONTROL_REGISTER_MASK 0x1F
#define ATL_MDIO_CONTROL_REGISTER_SHIFT 16
#define ATL_MDIO_CONTROL_READ_OPERATION (1 << 21)
#define ATL_MDIO_CONTROL_SPRES_PRMBL (1 << 22)
#define ATL_MDIO_CONTROL_START (1 << 23)
#define ATL_MDIO_CONTROL_CLOCK_SELECT_MASK 0x07
#define ATL_MDIO_CONTROL_CLOCK_SELECT_SHIFT 24
#define ATL_MDIO_CONTROL_BUSY (1 << 27)
#define ATL_MDIO_CONTROL_EXTENSION_MODE (1 << 30)

//
// Define MDIO extension register bits.
//

#define ATL_MDIO_EXTENSION_DEVICE_ADDRESS_MASK 0x1F
#define ATL_MDIO_EXTENSION_DEVICE_ADDRESS_SHIFT 16
#define ATL_MDIO_EXTENSION_REGISTER_MASK 0xFFFF
#define ATL_MDIO_EXTENSION_REGISTER_SHIFT 0

//
// Define MAC control register bits.
//

#define ATL_MAC_CONTROL_TRANSMIT_ENABLED (1 << 0)
#define ATL_MAC_CONTROL_RECEIVE_ENABLED (1 << 1)
#define ATL_MAC_CONTROL_TRANSMIT_FLOW_ENABLED (1 << 2)
#define ATL_MAC_CONTROL_RECEIVE_FLOW_ENABLED (1 << 3)
#define ATL_MAC_CONTROL_DUPLEX (1 << 5)
#define ATL_MAC_CONTROL_ADD_CRC (1 << 6)
#define ATL_MAC_CONTROL_PAD (1 << 7)
#define ATL_MAC_CONTROL_PREAMBLE_LENGTH_MASK 0xF
#define ATL_MAC_CONTROL_PREAMBLE_LENGTH_SHIFT 10
#define ATL_MAC_CONTROL_STRIP_VLAN (1 << 14)
#define ATL_MAC_CONTROL_PROMISCUOUS_MODE_ENABLE (1 << 15)
#define ATL_MAC_CONTROL_SPEED_10_100 1
#define ATL_MAC_CONTROL_SPEED_1000 2
#define ATL_MAC_CONTROL_SPEED_MASK 0x3
#define ATL_MAC_CONTROL_SPEED_SHIFT 20
#define ATL_MAC_CONTROL_ALL_MULTICAST_ENABLE (1 << 25)
#define ATL_MAC_CONTROL_BROADCAST_ENABLED (1 << 26)
#define ATL_MAC_CONTROL_SINGLE_PAUSE_ENABLED (1 << 28)
#define ATL_MAC_CONTROL_CRC32_HASH_ALGORITHM (1 << 29)
#define ATL_MAC_CONTROL_SOFTWARE_CONTROLLED_SPEED (1 << 30)

//
// Define the IPG/IFG control register bits.
//

#define ATL_IPG_IFG_IPGT_MASK 0x0000007F
#define ATL_IPG_IFG_MIFG_MASK 0x0000FF00
#define ATL_IPG_IFG_IPG1_MASK 0x007F0000
#define ATL_IPG_IFG_IPG2_MASK 0x7F000000
#define ATL_IPG_IFG_IPGT_SHIFT 0
#define ATL_IPG_IFG_IPGT_DEFAULT 0x60
#define ATL_IPG_IFG_MIFG_SHIFT 8
#define ATL_IPG_IFG_MIFG_DEFAULT 0x50
#define ATL_IPG_IFG_IPG1_SHIFT 16
#define ATL_IPG_IFG_IPG1_DEFAULT 0x40
#define ATL_IPG_IFG_IPG2_SHIFT 24
#define ATL_IPG_IFG_IPG2_DEFAULT 0x60

#define ATL_IPG_IFG_VALUE                                    \
    (((ATL_IPG_IFG_IPGT_DEFAULT << ATL_IPG_IFG_IPGT_SHIFT) & \
      ATL_IPG_IFG_IPGT_MASK) |                               \
     ((ATL_IPG_IFG_MIFG_DEFAULT << ATL_IPG_IFG_MIFG_SHIFT) & \
      ATL_IPG_IFG_MIFG_MASK) |                               \
     ((ATL_IPG_IFG_IPG1_DEFAULT << ATL_IPG_IFG_IPG1_SHIFT) & \
      ATL_IPG_IFG_IPG1_MASK) |                               \
     ((ATL_IPG_IFG_IPG2_DEFAULT << ATL_IPG_IFG_IPG2_SHIFT) & \
      ATL_IPG_IFG_IPG2_MASK))

//
// Define the Half Duplex control register bits.
//

#define ATL_HALF_DUPLEX_CONTROL_LCOL_MASK      0x000003FF
#define ATL_HALF_DUPLEX_CONTROL_RETRY_MASK     0x0000F000
#define ATL_HALF_DUPLEX_CONTROL_EXC_DEF_EN     0x00010000
#define ATL_HALF_DUPLEX_CONTROL_NO_BACK_C      0x00020000
#define ATL_HALF_DUPLEX_CONTROL_NO_BACK_P      0x00040000
#define ATL_HALF_DUPLEX_CONTROL_ABEBE          0x00080000
#define ATL_HALF_DUPLEX_CONTROL_ABEBT_MASK     0x00F00000
#define ATL_HALF_DUPLEX_CONTROL_JAMIPG_MASK    0x0F000000
#define ATL_HALF_DUPLEX_CONTROL_LCOL_SHIFT     0
#define ATL_HALF_DUPLEX_CONTROL_LCOL_DEFAULT   0x37
#define ATL_HALF_DUPLEX_CONTROL_RETRY_SHIFT    12
#define ATL_HALF_DUPLEX_CONTROL_RETRY_DEFAULT  0x0F
#define ATL_HALF_DUPLEX_CONTROL_ABEBT_SHIFT    20
#define ATL_HALF_DUPLEX_CONTROL_ABEBT_DEFAULT  0x0A
#define ATL_HALF_DUPLEX_CONTROL_JAMIPG_SHIFT   24
#define ATL_HALF_DUPLEX_CONTROL_JAMIPG_DEFAULT 0x07

#define ATL_HALF_DUPLEX_CONTROL_VALUE            \
    (((ATL_HALF_DUPLEX_CONTROL_LCOL_DEFAULT <<   \
       ATL_HALF_DUPLEX_CONTROL_LCOL_SHIFT) &     \
      ATL_HALF_DUPLEX_CONTROL_LCOL_MASK) |       \
     ((ATL_HALF_DUPLEX_CONTROL_RETRY_DEFAULT <<  \
       ATL_HALF_DUPLEX_CONTROL_RETRY_SHIFT) &    \
      ATL_HALF_DUPLEX_CONTROL_RETRY_MASK) |      \
     ATL_HALF_DUPLEX_CONTROL_EXC_DEF_EN |        \
     ((ATL_HALF_DUPLEX_CONTROL_ABEBT_DEFAULT <<  \
       ATL_HALF_DUPLEX_CONTROL_ABEBT_SHIFT) &    \
      ATL_HALF_DUPLEX_CONTROL_ABEBT_MASK) |      \
     ((ATL_HALF_DUPLEX_CONTROL_JAMIPG_DEFAULT << \
       ATL_HALF_DUPLEX_CONTROL_JAMIPG_SHIFT) &   \
      ATL_HALF_DUPLEX_CONTROL_JAMIPG_MASK))

//
// Define the Load Registers command bits, which cause the addresses and indices
// written for the transmit and receive rings to get loaded into the device.
//

#define ATL_LOAD_POINTERS_COMMAND_GO 0x00000001

//
// Define the transmit/receive ring address masks.
//

#define ATL_RING_HIGH_ADDRESS_MASK 0xFFFFFFFF00000000ULL
#define ATL_RING_HIGH_ADDRESS_SHIFT 32
#define ATL_RING_LOW_ADDRESS_MASK 0x00000000FFFFFFFFULL

//
// Define "SMB Stat Timer" register bits.
//

#define ATL_SMB_STAT_TIMER_400MS 200000

//
// Define the Basic Mode Control Register (in the PHY) bits.
//

#define ATL_PHY_BASIC_MODE_CONTROL_REGISTER 0x00
#define ATL_PHY_AUTONEGOTIATE_RESTART 0x0200
#define ATL_PHY_AUTONEGOTIATE_ENABLE 0x1000

//
// Define the Basic Mode Status register (in the PHY) bits.
//

#define ATL_PHY_BASIC_MODE_STATUS_REGISTER 0x01
#define ATL_PHY_BASIC_MODE_STATUS_LINK_UP 0x0004

//
// Define the Physical ID registers (in the PHY).
//

#define ATL_PHY_PHYSICAL_ID1_REGISTER 0x02
#define ATL_PHY_PHYSICAL_ID2_REGISTER 0x03

//
// Define the Advertise register (in the PHY) bits.
//

#define ATL_PHY_ADVERTISE_REGISTER 0x04
#define ATL_PHY_ADVERTISE_10_HALF 0x0020
#define ATL_PHY_ADVERTISE_10_FULL 0x0040
#define ATL_PHY_ADVERTISE_100_HALF 0x0080
#define ATL_PHY_ADVERTISE_100_FULL 0x0100
#define ATL_PHY_ADVERTISE_PAUSE 0x0400
#define ATL_PHY_ADVERTISE_ASYMMETRIC_PAUSE 0x0800

//
// Define the Gigabit Control register (in the PHY).
//

#define ATL_PHY_GIGABIT_CONTROL_REGISTER 0x09
#define ATL_PHY_GIGABIT_CONTROL_DEFAULT_CAPABILITIES 0x0300

//
// Define the Giga Status register (in the PHY) bits.
//

#define ATL_PHY_GIGA_PSSR_REGISTER 0x11
#define ATL_PHY_GIGA_PSSR_SPEED_AND_DUPLEX_RESOLVED 0x0800
#define ATL_PHY_GIGA_PSSR_DUPLEX 0x2000
#define ATL_PHY_GIGA_PSSR_SPEED_MASK 0xC000
#define ATL_PHY_GIGA_PSSR_SPEED_1000 0x8000
#define ATL_PHY_GIGA_PSSR_SPEED_100 0x4000
#define ATL_PHY_GIGA_PSSR_SPEED_10 0x0000

//
// Define the MII (Media Independent Interface) Interrupt Status Register (in
// the PHY).
//

#define ATL_PHY_MII_INTERRUPT_STATUS 0x13

//
// Define PHY debug address and data register locations (inside MDIO).
//

#define ATL_PHY_DEBUG_ADDRESS 0x1D
#define ATL_PHY_DEBUG_DATA 0x1E

//
// Define PHY interrupt enable register bits (inside MDIO).
//

#define ATL_PHY_INTERRUPT_ENABLE_REGISTER 0x12
#define ATL_PHY_INTERRUPT_ENABLE_LINK_UP 0x0400
#define ATL_PHY_INTERRUPT_ENABLE_LINK_DOWN 0x0800

//
// Define other PHY debug port registers and values.
//

#define ATL_PHY_DEBUG_ANA_CONTROL_REGISTER 0x00
#define ATL_PHY_ANA_CONTROL_RESTART_CAL 0x0001
#define ATL_PHY_ANA_CONTROL_MANUAL_SWITCH_ON_MASK 0x001E
#define ATL_PHY_ANA_CONTROL_MAN_ENABLE 0x0020
#define ATL_PHY_ANA_CONTROL_SEL_HSP 0x0040
#define ATL_PHY_ANA_CONTROL_EN_HB 0x0080
#define ATL_PHY_ANA_CONTROL_EN_HBIAS 0x0100
#define ATL_PHY_ANA_CONTROL_OEN_125M 0x0200
#define ATL_PHY_ANA_CONTROL_ENABLE_LCKDT 0x0400
#define ATL_PHY_ANA_CONTROL_LCKDT_PHY 0x0800
#define ATL_PHY_ANA_CONTROL_AFE_MODE 0x1000
#define ATL_PHY_ANA_CONTROL_VCO_SLOW 0x2000
#define ATL_PHY_ANA_CONTROL_VCO_FAST 0x4000
#define ATL_PHY_ANA_CONTROL_SEL_CLK125M_DSP 0x8000
#define ATL_PHY_ANA_CONTROL_MANUAL_SWITCH_ON_SHIFT    1

#define ATL_PHY_ANA_CONTROL_MANUAL_SWITCH_ON_VALUE 1

#define ATL_PHY_DEBUG_SYSMODCTRL_REGISTER 0x04
#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_MASK 0x0F
#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_3_MASK 0x000F
#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_2_MASK 0x00F0
#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_1_MASK 0x0F00
#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_0_MASK 0xF000
#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_3_SHIFT 0
#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_2_SHIFT 4
#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_1_SHIFT 8
#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_0_SHIFT 12

#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_3_VALUE        \
    ((11 << ATL_PHY_SYSMODCTRL_IECHO_ADJ_3_SHIFT) & \
     ATL_PHY_SYSMODCTRL_IECHO_ADJ_3_MASK)

#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_2_VALUE        \
    ((11 << ATL_PHY_SYSMODCTRL_IECHO_ADJ_2_SHIFT) & \
     ATL_PHY_SYSMODCTRL_IECHO_ADJ_2_MASK)

#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_1_VALUE        \
    ((8 << ATL_PHY_SYSMODCTRL_IECHO_ADJ_1_SHIFT) &  \
     ATL_PHY_SYSMODCTRL_IECHO_ADJ_1_MASK)

#define ATL_PHY_SYSMODCTRL_IECHO_ADJ_0_VALUE        \
    ((8 << ATL_PHY_SYSMODCTRL_IECHO_ADJ_0_SHIFT) &  \
     ATL_PHY_SYSMODCTRL_IECHO_ADJ_0_MASK)

#define ATL_PHY_DEBUG_SRDSYSMOD_REGISTER 0x05
#define ATL_PHY_SRDSYSMOD_SERDES_CDR_BW_SHIFT 0
#define ATL_PHY_SRDSYSMOD_SERDES_CDR_BW_MASK 0x0003
#define ATL_PHY_SRDSYSMOD_SERDES_EN_DEEM 0x0040
#define ATL_PHY_SRDSYSMOD_SERDES_SEL_HSP 0x0400
#define ATL_PHY_SRDSYSMOD_SERDES_ENABLE_PLL 0x0800
#define ATL_PHY_SRDSYSMOD_SERDES_EN_LCKDT 0x2000

#define ATL_PHY_SRDSYSMOD_SERDES_CDR_BW_VALUE       \
    ((2 << ATL_PHY_SRDSYSMOD_SERDES_CDR_BW_SHIFT) & \
     ATL_PHY_SRDSYSMOD_SERDES_CDR_BW_MASK)

#define ATL_PHY_DEBUG_HIBNEG_REGISTER 0x0B
#define ATL_PHY_HIBNEG_PSHIB_ENABLE 0x8000

#define ATL_PHY_DEBUG_TST10BTCFG_REGISTER 0x12
#define ATL_PHY_TST10BTCFG_LOOP_SEL_10BT 0x0004
#define ATL_PHY_TST10BTCFG_EN_MASK_TB 0x0800
#define ATL_PHY_TST10BTCFG_EN_10BT_IDLE 0x0400
#define ATL_PHY_TST10BTCFG_INTERVAL_SEL_TIMER_SHIFT 14
#define ATL_PHY_TST10BTCFG_INTERVAL_SEL_TIMER_MASK 0xC000

#define ATL_PHY_TST10BTCFG_INTERVAL_SEL_TIMER_VALUE       \
    ((1 << ATL_PHY_TST10BTCFG_INTERVAL_SEL_TIMER_SHIFT) & \
     ATL_PHY_TST10BTCFG_INTERVAL_SEL_TIMER_MASK)

#define ATL_PHY_DEBUG_LEGCYPS_REGISTER 0x29
#define ATL_PHY_DEBUG_LEGCYPS_ENABLED 0x8000
#define ATL_PHY_DEBUG_LEGCYPS_VALUE 0xB6DD

#define ATL_PHY_DEBUG_TST100BTCFG_REGISTER 0x36
#define ATL_PHY_TST100BTCFG_LONG_CABLE_TH_100_MASK  0x003F
#define ATL_PHY_TST100BTCFG_DESERVED            0x0040
#define ATL_PHY_TST100BTCFG_EN_LIT_CH           0x0080
#define ATL_PHY_TST100BTCFG_SHORT_CABLE_TH_100_MASK 0x3F00
#define ATL_PHY_TST100BTCFG_BP_BAD_LINK_ACCUM       0x4000
#define ATL_PHY_TST100BTCFG_BP_SMALL_BW         0x8000
#define ATL_PHY_TST100BTCFG_LONG_CABLE_TH_100_SHIFT 0
#define ATL_PHY_TST100BTCFG_SHORT_CABLE_TH_100_SHIFT    8

#define ATL_PHY_TST100BTCFG_LONG_CABLE_TH_100_VALUE        \
    ((44 << ATL_PHY_TST100BTCFG_LONG_CABLE_TH_100_SHIFT) & \
    ATL_PHY_TST100BTCFG_LONG_CABLE_TH_100_MASK)

#define ATL_PHY_TST100BTCFG_SHORT_CABLE_TH_100_VALUE \
    ((33 << ATL_PHY_TST100BTCFG_SHORT_CABLE_TH_100_SHIFT) & \
    ATL_PHY_TST100BTCFG_SHORT_CABLE_TH_100_SHIFT)

//
// Define receive queue control register bits.
//

#define ATL_RECEIVE_QUEUE_CONTROL_THROUGHPUT_LIMIT_MASK 0x03
#define ATL_RECEIVE_QUEUE_CONTROL_THROUGHPUT_LIMIT_SHIFT 0
#define ATL_RECEIVE_QUEUE_CONTROL_THROUGHPUT_LIMIT_1M 0x01
#define ATL_RECEIVE_QUEUE_CONTROL_THROUGHPUT_LIMIT_100M 0x03
#define ATL_RECEIVE_QUEUE_CONTROL_ENABLED1 0x00000010
#define ATL_RECEIVE_QUEUE_CONTROL_ENABLED2 0x00000020
#define ATL_RECEIVE_QUEUE_CONTROL_ENABLED3 0x00000040
#define ATL_RECEIVE_QUEUE_CONTROL_BURST_MASK 0x0000003F
#define ATL_RECEIVE_QUEUE_CONTROL_BURST_SHIFT 20
#define ATL_RECEIVE_QUEUE_CONTROL_ENABLED0 0x80000000

#define ATL_RECEIVE_QUEUE_CONTROL_ENABLED                                      \
    (ATL_RECEIVE_QUEUE_CONTROL_ENABLED0 | ATL_RECEIVE_QUEUE_CONTROL_ENABLED1 | \
     ATL_RECEIVE_QUEUE_CONTROL_ENABLED2 | ATL_RECEIVE_QUEUE_CONTROL_ENABLED3)

//
// Define the Receive Free Descriptor prefetching threshold register bits.
//

#define ATL_RECEIVE_FREE_THRESHOLD_HIGH_MASK 0x0000003F
#define ATL_RECEIVE_FREE_THRESHOLD_LOW_MASK 0x00000FC0
#define ATL_RECEIVE_FREE_THRESHOLD_HIGH_SHIFT 0
#define ATL_RECEIVE_FREE_THRESHOLD_LOW_SHIFT 6
#define ATL_RECEIVE_FREE_THRESHOLD_HIGH_DEFAULT 16
#define ATL_RECEIVE_FREE_THRESHOLD_LOW_DEFAULT 8

#define ATL_RECEIVE_FREE_THRESHOLD_VALUE          \
    (((ATL_RECEIVE_FREE_THRESHOLD_HIGH_DEFAULT << \
       ATL_RECEIVE_FREE_THRESHOLD_HIGH_SHIFT) &   \
      ATL_RECEIVE_FREE_THRESHOLD_HIGH_MASK) |     \
     ((ATL_RECEIVE_FREE_THRESHOLD_LOW_DEFAULT <<  \
       ATL_RECEIVE_FREE_THRESHOLD_LOW_SHIFT) &    \
      ATL_RECEIVE_FREE_THRESHOLD_LOW_MASK))

//
// Define the receive FIFO pause threshold register bits.
//

#define ATL_RECEIVE_FIFO_PAUSE_THRESHOLD_LOW_MASK 0x00000FFF
#define ATL_RECEIVE_FIFO_PAUSE_THRESHOLD_HIGH_MASK 0x0FFF0000
#define ATL_RECEIVE_FIFO_PAUSE_THRESHOLD_LOW_SHIFT 0
#define ATL_RECEIVE_FIFO_PAUSE_THRESHOLD_HIGH_SHIFT 16

//
// This macro sets the Receive FIFO pause threshold based on the FIFO size.
// It is set to send an XON at 80% of the FIFO size and XOFF at 30% of the FIFO
// size.

#define ATL_RECEIVE_FIFO_PAUSE_VALUE(_FifoSize)       \
    ((((((_FifoSize) * 8) / 10) <<                    \
       ATL_RECEIVE_FIFO_PAUSE_THRESHOLD_HIGH_SHIFT) & \
      ATL_RECEIVE_FIFO_PAUSE_THRESHOLD_HIGH_MASK) |   \
     (((((_FifoSize) * 3) / 10) <<                    \
       ATL_RECEIVE_FIFO_PAUSE_THRESHOLD_LOW_SHIFT) &  \
      ATL_RECEIVE_FIFO_PAUSE_THRESHOLD_LOW_MASK))     \

//
// Define DMA control register bits.
//

#define ATL_DMA_CONTROL_RORDER_MODE_OUT 4
#define ATL_DMA_CONTROL_RORDER_MODE_MASK 0x00000007
#define ATL_DMA_CONTROL_RORDER_MODE_SHIFT 0
#define ATL_DMA_CONTROL_OUT_OF_ORDER 0x00000004
#define ATL_DMA_CONTROL_RCB_128 0x00000008
#define ATL_DMA_CONTROL_RREQ_BLEN_MASK 0x00000007
#define ATL_DMA_CONTROL_RREQ_BLEN_SHIFT 4
#define ATL_DMA_CONTROL_WREQ_BLEN_MASK 0x00000007
#define ATL_DMA_CONTROL_WREQ_BLEN_SHIFT 7
#define ATL_DMA_CONTROL_RREQ_PRI_DATA (1 << 10)
#define ATL_DMA_CONTROL_WDELAY_CNT_DEF 4
#define ATL_DMA_CONTROL_WDELAY_CNT_MASK 0x0000000F
#define ATL_DMA_CONTROL_WDELAY_CNT_SHIFT 16
#define ATL_DMA_CONTROL_RDELAY_CNT_DEF 15
#define ATL_DMA_CONTROL_RDELAY_CNT_MASK 0x0000001F
#define ATL_DMA_CONTROL_RDELAY_CNT_SHIFT 11
#define ATL_DMA_CONTROL_CMB_ENABLE 0x00100000
#define ATL_DMA_CONTROL_SMB_ENABLE 0x00200000
#define ATL_DMA_CONTROL_SMB_DISABLE 0x01000000

#define ATL_DMA_CONTROL_VALUE                                               \
    (ATL_DMA_CONTROL_OUT_OF_ORDER | ATL_DMA_CONTROL_RREQ_PRI_DATA |         \
     ATL_DMA_CONTROL_SMB_DISABLE | ATL_DMA_CONTROL_RCB_128 |                \
     ((ATL_DMA_REQUEST_1024 & ATL_DMA_CONTROL_RREQ_BLEN_MASK) <<            \
      ATL_DMA_CONTROL_RREQ_BLEN_SHIFT) |                                    \
     ((ATL_DMA_REQUEST_1024 & ATL_DMA_CONTROL_WREQ_BLEN_MASK) <<            \
      ATL_DMA_CONTROL_WREQ_BLEN_SHIFT) |                                    \
     ((ATL_DMA_CONTROL_WDELAY_CNT_DEF & ATL_DMA_CONTROL_WDELAY_CNT_MASK) << \
      ATL_DMA_CONTROL_WDELAY_CNT_SHIFT) |                                   \
     ((ATL_DMA_CONTROL_RDELAY_CNT_DEF & ATL_DMA_CONTROL_RDELAY_CNT_MASK) << \
      ATL_DMA_CONTROL_RDELAY_CNT_SHIFT))

//
// Define the Receive Frame Index (producer) register bits.
//

#define ATL_RECEIVE_FRAME_INDEX_MASK 0xFFFF

//
// Define transmit queue control register bits.
//

#define ATL_TRANSMIT_QUEUE_CONTROL_BURST_MASK 0x0000000F
#define ATL_TRANSMIT_QUEUE_CONTROL_BURST_SHIFT 0
#define ATL_TRANSMIT_QUEUE_CONTROL_IP_OPTION_ENABLE (1 << 4)
#define ATL_TRANSMIT_QUEUE_CONTROL_ENABLED (1 << 5)
#define ATL_TRANSMIT_QUEUE_CONTROL_ENHANCED_MODE (1 << 6)
#define ATL_TRANSMIT_QUEUE_CONTROL_LS_802_3_ENABLE (1 << 7)
#define ATL_TRANSMIT_QUEUE_CONTROL_BURST_NUMBER_MASK 0x0000FFFF
#define ATL_TRANSMIT_QUEUE_CONTROL_BURST_NUMBER_SHIFT 16

//
// Define the TCP Segmentation Threshold register bits.
//

#define ATL_TCP_SEGMENTATION_OFFLOAD_THRESHOLD_DOWNSHIFT 3
#define ATL_TCP_SEGMENTATION_OFFLOAD_THRESHOLD_MASK 0x07FF

//
// Define interrupt status bits.
//

#define ATL_INTERRUPT_MANUAL                0x00000004
#define ATL_INTERRUPT_RECEIVE_OVERFLOW      0x00000008
#define ATL_INTERRUPT_RECEIVE_UNDERRUN_MASK 0x000000F0
#define ATL_INTERRUPT_TRANSMIT_UNDERRUN     0x00000100
#define ATL_INTERRUPT_DMAR_TO_RST           0x00000200
#define ATL_INTERRUPT_DMAW_TO_RST           0x00000400
#define ATL_INTERRUPT_GPHY                  0x00001000
#define ATL_INTERRUPT_GPHY_LOW_POWER        0x00002000
#define ATL_INTERRUPT_TRANSMIT_QUEUE_TO_RST 0x00004000
#define ATL_INTERRUPT_TRANSMIT_PACKET       0x00008000
#define ATL_INTERRUPT_RECEIVE_PACKET        0x00010000
#define ATL_INTERRUPT_RECEIVE_PACKET_MASK   0x000F0000
#define ATL_INTERRUPT_PHY_LINK_DOWN         0x04000000

#define ATL_INTERRUPT_BUFFER_ERROR_MASK                                     \
    (ATL_INTERRUPT_RECEIVE_OVERFLOW | ATL_INTERRUPT_RECEIVE_UNDERRUN_MASK | \
     ATL_INTERRUPT_TRANSMIT_UNDERRUN)

#define ATL_INTERRUPT_ERROR_MASK                                         \
    (ATL_INTERRUPT_DMAR_TO_RST | ATL_INTERRUPT_DMAW_TO_RST |             \
     ATL_INTERRUPT_TRANSMIT_QUEUE_TO_RST | ATL_INTERRUPT_PHY_LINK_DOWN)

#define ATL_INTERRUPT_DEFAULT_MASK         \
    (ATL_INTERRUPT_MANUAL |                \
     ATL_INTERRUPT_RECEIVE_OVERFLOW |      \
     ATL_INTERRUPT_RECEIVE_UNDERRUN_MASK | \
     ATL_INTERRUPT_TRANSMIT_UNDERRUN |     \
     ATL_INTERRUPT_DMAR_TO_RST |           \
     ATL_INTERRUPT_DMAW_TO_RST |           \
     ATL_INTERRUPT_TRANSMIT_QUEUE_TO_RST | \
     ATL_INTERRUPT_GPHY |                  \
     ATL_INTERRUPT_TRANSMIT_PACKET |       \
     ATL_INTERRUPT_RECEIVE_PACKET_MASK |   \
     ATL_INTERRUPT_GPHY_LOW_POWER |        \
     ATL_INTERRUPT_PHY_LINK_DOWN)

#define ATL_INTERRUPT_MASK 0x7FFFFFFF
#define ATL_INTERRUPT_DISABLE 0x80000000

//
// Define interrupt retrigger timer bits.
//

#define ATL_INTERRUPT_RETRIGGER_100MS 50000

//
// Define clock gating control bits.
//

#define ATL_CLOCK_GATING_DMA_WRITE_ENABLE 0x0001
#define ATL_CLOCK_GATING_DMA_READ_ENABLE 0x0002
#define ATL_CLOCK_GATING_TRANSMIT_QUEUE_ENABLE 0x0004
#define ATL_CLOCK_GATING_RECEIVE_QUEUE_ENABLE 0x0008
#define ATL_CLOCK_GATING_TRANSMIT_MAC_ENABLE 0x0010
#define ATL_CLOCK_GATING_RECEIVE_MAC_ENABLE 0x0020

#define ATL_CLOCK_GATING_ALL_MASK             \
    (ATL_CLOCK_GATING_DMA_WRITE_ENABLE |      \
     ATL_CLOCK_GATING_DMA_READ_ENABLE |       \
     ATL_CLOCK_GATING_TRANSMIT_QUEUE_ENABLE | \
     ATL_CLOCK_GATING_RECEIVE_QUEUE_ENABLE |  \
     ATL_CLOCK_GATING_TRANSMIT_MAC_ENABLE |   \
     ATL_CLOCK_GATING_RECEIVE_MAC_ENABLE)

//
// Define received packet flags.
//

#define ATL_RECEIVED_PACKET_COUNT_MASK 0x000F
#define ATL_RECEIVED_PACKET_COUNT_SHIFT 16
#define ATL_RECEIVED_PACKET_FREE_INDEX_MASK 0x0FFF
#define ATL_RECEIVED_PACKET_FREE_INDEX_SHIFT 20
#define ATL_RECEIVED_PACKET_SIZE_MASK 0x3FFF

#define ATL_RECEIVED_PACKET_FLAG_CHECKSUM_ERROR 0x00100000
#define ATL_RECEIVED_PACKET_FLAG_802_3_LENGTH_ERROR 0x40000000
#define ATL_RECEIVED_PACKET_FLAG_VALID 0x80000000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ATL_SPEED {
    AtlSpeedOff,
    AtlSpeed10,
    AtlSpeed100,
    AtlSpeed1000
} ATL_SPEED, *PATL_SPEED;

typedef enum _ATL_DUPLEX_MODE {
    AtlDuplexInvalid,
    AtlDuplexHalf,
    AtlDuplexFull
} ATL_DUPLEX_MODE, *PATL_DUPLEX_MODE;

typedef enum _ATL1C_REGISTER {
    AtlRegisterPciCommand = 0x0004,
    AtlRegisterPexUncErrSev = 0x010C,
    AtlRegisterTwsiControl = 0x0218,
    AtlRegisterPhyMiscellaneous = 0x1000,
    AtlRegisterTwsiDebug = 0x1108,
    AtlRegisterOtpControl = 0x12F0,
    AtlRegisterPowerManagementControl = 0x12F8,
    AtlRegisterLtssmIdControl = 0x12FC,
    AtlRegisterMasterControl = 0x1400,
    AtlRegisterInterruptTimers = 0x1408,
    AtlRegisterPhyControl = 0x140C,
    AtlRegisterIdleStatus = 0x1410,
    AtlRegisterMdioControl = 0x1414,
    AtlRegisterSerdes = 0x1424,
    AtlRegisterMdioExtension = 0x1448,
    AtlRegisterMacControl = 0x1480,
    AtlRegisterIpgIfgControl = 0x1484,
    AtlRegisterMacAddress1 = 0x1488,
    AtlRegisterMacAddress2 = 0x148C,
    AtlRegisterReceiveHashTable = 0x1490,
    AtlRegisterHalfDuplexControl = 0x1498,
    AtlRegisterMaximumTransmissionUnit = 0x149C,
    AtlRegisterWakeOnLanControl = 0x14A0,
    AtlRegisterRssIdtTable0 = 0x14E0,
    AtlRegisterSramReceiveFifoLength = 0x1524,
    AtlRegisterLoadRingPointers = 0x1534,
    AtlRegisterReceiveBaseAddressHigh = 0x1540,
    AtlRegisterTransmitBaseAddressHigh = 0x1544,
    AtlRegisterSmbBaseAddressHigh = 0x1548,
    AtlRegisterSmbBaseAddressLow = 0x154C,
    AtlRegisterReceiveBaseAddressLow = 0x1550,
    AtlRegisterReceive1BaseAddressLow = 0x1554,
    AtlRegisterReceive2BaseAddressLow = 0x1558,
    AtlRegisterReceive3BaseAddressLow = 0x155C,
    AtlRegisterReceiveSlotRingSize = 0x1560,
    AtlRegisterReceiveBufferSize = 0x1564,
    AtlRegisterReceiveRingBaseAddressLow = 0x1568,
    AtlRegisterReceiveRing1BaseAddressLow = 0x156C,
    AtlRegisterReceiveRing2BaseAddressLow = 0x1570,
    AtlRegisterReceiveRing3BaseAddressLow = 0x1574,
    AtlRegisterReceiveStatusRingSize = 0x1578,
    AtlRegisterTransmitBaseAddressLowHighPriority = 0x157C,
    AtlRegisterTransmitBaseAddressLow = 0x1580,
    AtlRegisterTransmitRingSize = 0x1584,
    AtlRegisterCmbBaseAddressLow = 0x1588,
    AtlRegisterTransmitQueueControl = 0x1590,
    AtlRegisterTcpSegmentationOffloadThreshold = 0x1594,
    AtlRegisterReceiveQueueControl = 0x15A0,
    AtlRegisterReceiveFreeThreshold = 0x15A4,
    AtlRegisterReceiveFifoPauseThreshold = 0x15A8,
    AtlRegisterRssCpu = 0x15B8,
    AtlRegisterDmaControl = 0x15C0,
    AtlRegisterSmbStatTimer = 0x15C4,
    AtlRegisterCmbTransmitTimer = 0x15CC,
    AtlRegisterReceiveFrameIndex = 0x15E0,
    AtlRegisterTransmitHighPriorityNextIndex = 0x15F0, // producer
    AtlRegisterTransmitNextIndex = 0x15F2, // producer
    AtlRegisterTransmitHighPriorityCurrentIndex = 0x15F4,
    AtlRegisterTransmitCurrentIndex = 0x15F6,
    AtlRegisterInterruptStatus = 0x1600,
    AtlRegisterInterruptMask = 0x1604,
    AtlRegisterInterruptRetriggerTimer = 0x1608,
    AtlRegisterHdsControl = 0x160C,
    AtlRegisterClockGatingControl = 0x1814,
} ATL1C_REGISTER, *PATL1C_REGISTER;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for a transmit
    packet descriptor.

Members:

    BufferLength - Stores the length of the packet to send, including the
        4-byte CRC.

    VlanTag - Stores the VLAN tag to send with this packet.

    Flags - Stores various control flags for the descriptor.

    PhysicalAddress - Stores the physical address of the buffer to send out the
        wire.

--*/

typedef struct _ATL1C_TRANSMIT_DESCRIPTOR {
    USHORT BufferLength;
    USHORT VlanTag;
    ULONG Flags;
    ULONGLONG PhysicalAddress;
} PACKED ATL1C_TRANSMIT_DESCRIPTOR, *PATL1C_TRANSMIT_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for a received
    packet.

Members:

    FreeIndex - Stores the index of the free descriptor that was used to store
        this packet.

    RssHash - Stores the RSS hash of the received packet.

    VlanTag - Stores the VLAN tag of the packet if VLAN stripping is enabled on
        the receive side.

    Reserved - Stores a reserved word.

    FlagsAndLength - Stores control flags regarding the receive, and the length
        of the received packet.

--*/

typedef struct _ATL1C_RECEIVED_PACKET {
    ULONG FreeIndex;
    ULONG RssHash;
    USHORT VlanTag;
    USHORT Reserved;
    ULONG FlagsAndLength;
} PACKED ATL1C_RECEIVED_PACKET, *PATL1C_RECEIVED_PACKET;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for a free
    receive descriptor.

Members:

    PhysicalAddress - Stores the physical address of the location where the
        device should DMA a received packet to.

--*/

typedef struct _ATL1C_RECEIVE_SLOT {
    ULONGLONG PhysicalAddress;
} PACKED ATL1C_RECEIVE_SLOT, *PATL1C_RECEIVE_SLOT;

/*++

Structure Description:

    This structure defines an Attansic L1C LAN device.

Members:

    OsDevice - Stores a pointer to the OS device.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    ControllerBase - Stores the virtual address of the memory mapping to the
        ATL1c's registers.

    NetworkLink - Stores a pointer to the core networking link.

    ReceiveLock - Stores the queued lock protecting access to the receive
        packets and receive slots.

    ReceiveNextToClean - Stores the index of the next received packet descriptor
        to check for new data.

    DescriptorIoBuffer - Stores the I/O buffer that holds the transmit
        descriptor array, received packet descriptor array, receive slot array,
        array of transmit buffer virtual addresses, and the array of received
        packet memory.

    TransmitDescriptor - Stores the array of transmit descriptor headers.

    TransmitBuffer - Stores an array of pointers to the virtual addresses of the
        transmitted packets. This is used when freeing packets that have
        successfully been sent.

    ReceiveSlot - Stores the array of receive slots, also known as receive
        free descriptors.

    ReceivedPacket - Stores the array of received packet descriptors, also
        known as Receive Ring Descriptors.

    ReceivedPacketData - Stores the virtual address of the first descriptor's
        packet data.

    TransmitPacketList - Stores a list of network packets waiting to be sent.

    TransmitNextToClean - Stores the index of the oldest in-flight packet, the
        first one to check to see if transmission is done.

    TransmitNextToUse - Stores the index of the next transmit descriptor to use
        when sending a new packet.

    TransmitLock - Stores the queued lock that protects access to the transmit
        ring.

    LinkActive - Stores a boolean indicating if there is an active network link.

    InterruptLock - Stores the spin lock, synchronized at the interrupt
        run level, that synchronizes access to the pending status bits, DPC,
        and work item.

    PendingInterrupts - Stores the bitfield of status bits that have yet to be
        dealt with by software.

    EnabledInterrupts - Stores the bitfield of enabled interrupts.

    Speed - Stores the current speed of the link.

    Duplex - Stores the current duplex mode of the link.

    EepromMacAddress - Stores the default MAC address of the device.

    SupportedCapabilities - Stores the set of capabilities that this device
        supports. See NET_LINK_CAPABILITY_* for definitions.

    EnabledCapabilities - Stores the currently enabled capabilities on the
        devices. See NET_LINK_CAPABILITY_* for definitions.

    ConfigurationLock - Stores a queued lock that synchronizes changes to the
        enabled capabilities field and their supporting hardware registers.

--*/

typedef struct _ATL1C_DEVICE {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PNET_LINK NetworkLink;
    PQUEUED_LOCK ReceiveLock;
    USHORT ReceiveNextToClean;
    PIO_BUFFER DescriptorIoBuffer;
    PATL1C_TRANSMIT_DESCRIPTOR TransmitDescriptor;
    PNET_PACKET_BUFFER *TransmitBuffer;
    PATL1C_RECEIVE_SLOT ReceiveSlot;
    PATL1C_RECEIVED_PACKET ReceivedPacket;
    PVOID ReceivedPacketData;
    NET_PACKET_LIST TransmitPacketList;
    USHORT TransmitNextToClean;
    USHORT TransmitNextToUse;
    PQUEUED_LOCK TransmitLock;
    BOOL LinkActive;
    KSPIN_LOCK InterruptLock;
    volatile ULONG PendingInterrupts;
    ULONG EnabledInterrupts;
    ATL_SPEED Speed;
    ATL_DUPLEX_MODE Duplex;
    BYTE EepromMacAddress[ETHERNET_ADDRESS_SIZE];
    ULONG SupportedCapabilities;
    ULONG EnabledCapabilities;
    PQUEUED_LOCK ConfigurationLock;
} ATL1C_DEVICE, *PATL1C_DEVICE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Hardware functions called by the administrative side.
//

KSTATUS
AtlSend (
    PVOID DeviceContext,
    PNET_PACKET_LIST PacketList
    );

/*++

Routine Description:

    This routine sends data through the network.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link down which this data is to be sent.

    PacketList - Supplies a pointer to a list of network packets to send. Data
        in these packets may be modified by this routine, but must not be used
        once this routine returns.

Return Value:

    STATUS_SUCCESS if all packets were sent.

    STATUS_RESOURCE_IN_USE if some or all of the packets were dropped due to
    the hardware being backed up with too many packets to send.

    Other failure codes indicate that none of the packets were sent.

--*/

KSTATUS
AtlGetSetInformation (
    PVOID DeviceContext,
    NET_LINK_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the network device layer's link information.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link for which information is being set or queried.

    InformationType - Supplies the type of information being queried or set.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or a
        set operation (TRUE).

Return Value:

    Status code.

--*/

KSTATUS
AtlpInitializeDeviceStructures (
    PATL1C_DEVICE Device
    );

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an ATL1c device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

KSTATUS
AtlpResetDevice (
    PATL1C_DEVICE Device
    );

/*++

Routine Description:

    This routine resets the ATL1c device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
AtlpInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the ATL1c interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the ATL1c device
        structure.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
AtlpInterruptServiceWorker (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine processes interrupts for the ATL1c controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt Status.

--*/

//
// Administrative functions called by the hardware side.
//

KSTATUS
AtlpAddNetworkDevice (
    PATL1C_DEVICE Device
    );

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

