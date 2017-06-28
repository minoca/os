/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smbios.h

Abstract:

    This header contains definitions for the SMBIOS tables.

Author:

    Evan Green 6-May-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the UEFI SMBIOS table GUID.
//

#define EFI_SMBIOS_TABLE_GUID                               \
    {                                                       \
        0xEB9D2D31, 0x2D88, 0x11D3,                         \
        {0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}    \
    }

#define SMBIOS_ANCHOR_STRING_VALUE 0x5F4D535F
#define SMBIOS_INTERMEDIATE_ANCHOR "_DMI_"
#define SMBIOS_INTERMEDIATE_ANCHOR_SIZE (sizeof(SMBIOS_INTERMEDIATE_ANCHOR) - 1)

//
// Define SMBIOS BIOS Characteristics flags.
//

#define SMBIOS_BIOS_CHARACTERISTIC_UNKNOWN              (1 << 2)
#define SMBIOS_BIOS_CHARACTERISTIC_UNSUPPORTED          (1 << 3)
#define SMBIOS_BIOS_CHARACTERISTIC_ISA                  (1 << 4)
#define SMBIOS_BIOS_CHARACTERISTIC_MCA                  (1 << 5)
#define SMBIOS_BIOS_CHARACTERISTIC_EISA                 (1 << 6)
#define SMBIOS_BIOS_CHARACTERISTIC_PCI                  (1 << 7)
#define SMBIOS_BIOS_CHARACTERISTIC_PCMCIA               (1 << 8)
#define SMBIOS_BIOS_CHARACTERISTIC_PNP                  (1 << 9)
#define SMBIOS_BIOS_CHARACTERISTIC_APM                  (1 << 10)
#define SMBIOS_BIOS_CHARACTERISTIC_UPGRADEABLE          (1 << 11)
#define SMBIOS_BIOS_CHARACTERISTIC_SHADOWING            (1 << 12)
#define SMBIOS_BIOS_CHARACTERISTIC_VESA                 (1 << 13)
#define SMBIOS_BIOS_CHARACTERISTIC_ESCD                 (1 << 14)
#define SMBIOS_BIOS_CHARACTERISTIC_CD_BOOT              (1 << 15)
#define SMBIOS_BIOS_CHARACTERISTIC_SELECTABLE_BOOT      (1 << 16)
#define SMBIOS_BIOS_CHARACTERISTIC_ROM_SOCKETED         (1 << 17)
#define SMBIOS_BIOS_CHARACTERISTIC_PCMCIA_BOOT          (1 << 18)
#define SMBIOS_BIOS_CHARACTERISTIC_EDD                  (1 << 19)
#define SMBIOS_BIOS_CHARACTERISTIC_JAPAN_NEC_FLOPPY     (1 << 20)
#define SMBIOS_BIOS_CHARACTERISTIC_JAPAN_TOSHIBA_FLOPPY (1 << 21)
#define SMBIOS_BIOS_CHARACTERISTIC_525_360KB_FLOPPY     (1 << 22)
#define SMBIOS_BIOS_CHARACTERISTIC_525_1MB_FLOPPY       (1 << 23)
#define SMBIOS_BIOS_CHARACTERISTIC_35_720KB_FLOPPY      (1 << 24)
#define SMBIOS_BIOS_CHARACTERISTIC_35_2M_FLOPPY         (1 << 25)
#define SMBIOS_BIOS_CHARACTERISTIC_INT5_PRINT_SCREEN    (1 << 26)
#define SMBIOS_BIOS_CHARACTERISTIC_INT9_8042            (1 << 27)
#define SMBIOS_BIOS_CHARACTERISTIC_INT14_SERIAL         (1 << 28)
#define SMBIOS_BIOS_CHARACTERISTIC_INT17_PRINTER        (1 << 29)
#define SMBIOS_BIOS_CHARACTERISTIC_INT10_CGA            (1 << 30)
#define SMBIOS_BIOS_CHARACTERISTIC_NEC_PC98             (1 << 31)

#define SMBIOS_BIOS_CHARACTERISTIC_EX_ACPI              (1 << 0)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_USB_LEGACY        (1 << 1)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_AGP               (1 << 2)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_I2O_BOOT          (1 << 3)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_LS120_SUPERDISK   (1 << 4)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_ZIP               (1 << 5)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_1394              (1 << 6)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_SMART_BATTERY     (1 << 7)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_BIOS_BOOT         (1 << 8)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_NETWORK_BOOT      (1 << 9)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_TARGETED          (1 << 10)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_UEFI              (1 << 11)
#define SMBIOS_BIOS_CHARACTERISTIC_EX_VIRTUAL_MACHINE   (1 << 12)

//
// Define SMBIOS System information wakeup types.
//

#define SMBIOS_SYSTEM_WAKEUP_RESERVED               0x0
#define SMBIOS_SYSTEM_WAKEUP_OTHER                  0x1
#define SMBIOS_SYSTEM_WAKEUP_UNKNOWN                0x2
#define SMBIOS_SYSTEM_WAKEUP_APM_TIMER              0x3
#define SMBIOS_SYSTEM_WAKEUP_MODEM_RING             0x4
#define SMBIOS_SYSTEM_WAKEUP_LAN_REMOTE             0x5
#define SMBIOS_SYSTEM_WAKEUP_POWER_SWITCH           0x6
#define SMBIOS_SYSTEM_WAKEUP_PCI_PME                0x7
#define SMBIOS_SYSTEM_WAKEUP_AC_POWER_RESTORED      0x8

//
// Define SMBIOS module/baseboard feature flags.
//

#define SMBIOS_MODULE_MOTHERBOARD                   (1 << 0)
#define SMBIOS_MODULE_DAUGHTERBOARD_REQUIRED        (1 << 1)
#define SMBIOS_MODULE_REMOVABLE                     (1 << 2)
#define SMBIOS_MODULE_REPLACEABLE                   (1 << 3)
#define SMBIOS_MODULE_HOT_SWAPPABLE                 (1 << 4)

//
// Define SMBIOS baseboard types.
//

#define SMBIOS_MODULE_TYPE_UNKNOWN                  0x01
#define SMBIOS_MODULE_TYPE_OTHER                    0x02
#define SMBIOS_MODULE_TYPE_BLADE                    0x03
#define SMBIOS_MODULE_TYPE_SWITCH                   0x04
#define SMBIOS_MODULE_TYPE_SYSTEM_MANAGEMENT_MODULE 0x05
#define SMBIOS_MODULE_TYPE_PROCESSOR                0x06
#define SMBIOS_MODULE_TYPE_IO                       0x07
#define SMBIOS_MODULE_TYPE_MEMORY                   0x08
#define SMBIOS_MODULE_TYPE_DAUGHTERBOARD            0x09
#define SMBIOS_MODULE_TYPE_MOTHERBOARD              0x0A
#define SMBIOS_MODULE_TYPE_PROCESSOR_MEMORY         0x0B
#define SMBIOS_MODULE_TYPE_PROCESSOR_IO             0x0C
#define SMBIOS_MODULE_TYPE_INTERCONNECT             0x0D

//
// Define SMBIOS enclosure types.
//

#define SMBIOS_ENCLOSURE_TYPE_OTHER                 0x01
#define SMBIOS_ENCLOSURE_TYPE_UNKNOWN               0x02
#define SMBIOS_ENCLOSURE_TYPE_DESKTOP               0x03
#define SMBIOS_ENCLOSURE_TYPE_LOW_PROFILE_DESKTOP   0x04
#define SMBIOS_ENCLOSURE_TYPE_PIZZA_BOX             0x05
#define SMBIOS_ENCLOSURE_TYPE_MINI_TOWER            0x06
#define SMBIOS_ENCLOSURE_TYPE_TOWER                 0x07
#define SMBIOS_ENCLOSURE_TYPE_PORTABLE              0x08
#define SMBIOS_ENCLOSURE_TYPE_LAPTOP                0x09
#define SMBIOS_ENCLOSURE_TYPE_NOTEBOOK              0x0A
#define SMBIOS_ENCLOSURE_TYPE_HANDHELD              0x0B
#define SMBIOS_ENCLOSURE_TYPE_DOCKING_STATION       0x0C
#define SMBIOS_ENCLOSURE_TYPE_ALL_IN_ONE            0x0D
#define SMBIOS_ENCLOSURE_TYPE_SUB_NOTEBOOK          0x0E
#define SMBIOS_ENCLOSURE_TYPE_SPACE_SAVING          0x0F
#define SMBIOS_ENCLOSURE_TYPE_LUNCH_BOX             0x10
#define SMBIOS_ENCLOSURE_TYPE_MAIN_SERVER_CHASSIS   0x11
#define SMBIOS_ENCLOSURE_TYPE_EXPANSION_CHASSIS     0x12
#define SMBIOS_ENCLOSURE_TYPE_SUB_CHASSIS           0x13
#define SMBIOS_ENCLOSURE_TYPE_BUS_EXPANSION_CHASSIS 0x14
#define SMBIOS_ENCLOSURE_TYPE_PERIPHERAL_CHASSIS    0x15
#define SMBIOS_ENCLOSURE_TYPE_RAID_CHASSIS          0x16
#define SMBIOS_ENCLOSURE_TYPE_RACK_MOUNT_CHASSIS    0x17
#define SMBIOS_ENCLOSURE_TYPE_SEALED_CASE_PC        0x18
#define SMBIOS_ENCLOSURE_TYPE_MULTI_SYSTEM_CHASSIS  0x19
#define SMBIOS_ENCLOSURE_TYPE_COMPACT_PCI           0x1A
#define SMBIOS_ENCLOSURE_TYPE_ADVANCED_TCA          0x1B
#define SMBIOS_ENCLOSURE_TYPE_BLADE                 0x1C
#define SMBIOS_ENCLOSURE_TYPE_BLADE_ENCLOSURE       0x1D

//
// Define SMBIOS enclosure states.
//

#define SMBIOS_ENCLOSURE_STATE_OTHER                0x01
#define SMBIOS_ENCLOSURE_STATE_UNKNOWN              0x02
#define SMBIOS_ENCLOSURE_STATE_SAFE                 0x03
#define SMBIOS_ENCLOSURE_STATE_WARNING              0x04
#define SMBIOS_ENCLOSURE_STATE_CRITICAL             0x05
#define SMBIOS_ENCLOSURE_STATE_NON_RECOVERABLE      0x06

//
// Define SMBIOS enclosure security states.
//

#define SMBIOS_ENCLOSURE_SECURITY_STATE_OTHER       0x01
#define SMBIOS_ENCLOSURE_SECURITY_STATE_UNKNOWN     0x02
#define SMBIOS_ENCLOSURE_SECURITY_STATE_NONE        0x03
#define SMBIOS_ENCLOSURE_SECURITY_STATE_LOCKED      0x04
#define SMBIOS_ENCLOSURE_SECURITY_STATE_UNLOCKED    0x05

//
// Define SMBIOS processor status flags.
//

#define SMBIOS_PROCESSOR_STATUS_POPULATED           (1 << 6)
#define SMBIOS_PROCESSOR_STATUS_MASK                0x7
#define SMBIOS_PROCESSOR_STATUS_UNKNOWN             0x0
#define SMBIOS_PROCESSOR_STATUS_ENABLED             0x1
#define SMBIOS_PROCESSOR_STATUS_DISABLED_BY_USER    0x2
#define SMBIOS_PROCESSOR_STATUS_DISABLED_BY_BIOS    0x3
#define SMBIOS_PROCESSOR_STATUS_IDLE                0x4

//
// Define SMBIOS processor types.
//

#define SMBIOS_PROCESSOR_TYPE_OTHER                 0x01
#define SMBIOS_PROCESSOR_TYPE_UNKNOWN               0x02
#define SMBIOS_PROCESSOR_TYPE_CENTRAL_PROCESSOR     0x03
#define SMBIOS_PROCESSOR_TYPE_MATH_PROCESSOR        0x04
#define SMBIOS_PROCESSOR_TYPE_DSP_PROCESSOR         0x05
#define SMBIOS_PROCESSOR_TYPE_VIDEO_PROCESSOR       0x06

//
// Define SMBIOS processor characteristics.
//

#define SMBIOS_PROCESSOR_CHARACTERISTIC_RESERVED            (1 << 0)
#define SMBIOS_PROCESSOR_CHARACTERISTIC_UNKNOWN             (1 << 1)
#define SMBIOS_PROCESSOR_CHARACTERISTIC_64_BIT              (1 << 2)
#define SMBIOS_PROCESSOR_CHARACTERISTIC_MULTI_CORE          (1 << 3)
#define SMBIOS_PROCESSOR_CHARACTERISTIC_HARDWARE_THREAD     (1 << 4)
#define SMBIOS_PROCESSOR_CHARACTERISTIC_EXECUTE_PROTECTION  (1 << 5)
#define SMBIOS_PROCESSOR_CHARACTERISTIC_VIRTUALIZATION      (1 << 6)
#define SMBIOS_PROCESSOR_CHARACTERISTIC_POWER_CONTROL       (1 << 7)

//
// Define cache information bits.
//

#define SMBIOS_CACHE_OPERATIONAL_MODE_MASK          (0x3 << 8)
#define SMBIOS_CACHE_WRITE_THROUGH                  (0x0 << 8)
#define SMBIOS_CACHE_WRITE_BACK                     (0x1 << 8)
#define SMBIOS_CACHE_VARIES                         (0x2 << 8)
#define SMBIOS_CACHE_UNKNOWN                        (0x3 << 8)
#define SMBIOS_CACHE_ENABLED                        (1 << 7)
#define SMBIOS_CACHE_LOCATION_MASK                  (0x3 << 5)
#define SMBIOS_CACHE_LOCATION_INTERNAL              (0x0 << 5)
#define SMBIOS_CACHE_LOCATION_EXTERNAL              (0x1 << 5)
#define SMBIOS_CACHE_LOCATION_UNKNOWN               (0x3 << 5)
#define SMBIOS_CACHE_SOCKETED                       (1 << 3)
#define SMBIOS_CACHE_LEVEL_MASK                     0x7

#define SMBIOS_CACHE_GRANULARITY_64K                (1 << 15)

#define SMBIOS_CACHE_SRAM_OTHER                     (1 << 0)
#define SMBIOS_CACHE_SRAM_UNKNOWN                   (1 << 1)
#define SMBIOS_CACHE_SRAM_NON_BURST                 (1 << 2)
#define SMBIOS_CACHE_SRAM_BURST                     (1 << 3)
#define SMBIOS_CACHE_SRAM_PIPELINE_BURST            (1 << 4)
#define SMBIOS_CACHE_SRAM_SYNCHRONOUS               (1 << 5)
#define SMBIOS_CACHE_SRAM_ASYNCHRONOUS              (1 << 6)

#define SMBIOS_CACHE_ERROR_CORRECTION_OTHER             0x01
#define SMBIOS_CACHE_ERROR_CORRECTION_UNKNOWN           0x02
#define SMBIOS_CACHE_ERROR_CORRECTION_NONE              0x03
#define SMBIOS_CACHE_ERROR_CORRECTION_PARITY            0x04
#define SMBIOS_CACHE_ERROR_CORRECTION_SINGLE_BIT_ECC    0x05
#define SMBIOS_CACHE_ERROR_CORRECTION_MULTI_BIT_ECC     0x06

#define SMBIOS_CACHE_TYPE_OTHER                     0x01
#define SMBIOS_CACHE_TYPE_UNKNOWN                   0x02
#define SMBIOS_CACHE_TYPE_INSTRUCTION               0x03
#define SMBIOS_CACHE_TYPE_DATA                      0x04
#define SMBIOS_CACHE_TYPE_UNIFIED                   0x05

#define SMBIOS_CACHE_ASSOCIATIVITY_OTHER            0x01
#define SMBIOS_CACHE_ASSOCIATIVITY_UNKNOWN          0x02
#define SMBIOS_CACHE_ASSOCIATIVITY_DIRECT_MAPPED    0x03
#define SMBIOS_CACHE_ASSOCIATIVITY_2_WAY_SET        0x04
#define SMBIOS_CACHE_ASSOCIATIVITY_4_WAY_SET        0x05
#define SMBIOS_CACHE_ASSOCIATIVITY_FULL             0x06
#define SMBIOS_CACHE_ASSOCIATIVITY_8_WAY_SET        0x07
#define SMBIOS_CACHE_ASSOCIATIVITY_16_WAY_SET       0x08
#define SMBIOS_CACHE_ASSOCIATIVITY_12_WAY_SET       0x09
#define SMBIOS_CACHE_ASSOCIATIVITY_24_WAY_SET       0x0A
#define SMBIOS_CACHE_ASSOCIATIVITY_32_WAY_SET       0x0B
#define SMBIOS_CACHE_ASSOCIATIVITY_48_WAY_SET       0x0C
#define SMBIOS_CACHE_ASSOCIATIVITY_64_WAY_SET       0x0D
#define SMBIOS_CACHE_ASSOCIATIVITY_20_WAY_SET       0x0E

//
// Define port and connector types.
//

#define SMBIOS_CONNECTOR_NONE                       0x00
#define SMBIOS_CONNECTOR_CENTRONICS                 0x01
#define SMBIOS_CONNECTOR_MINI_CENTRONICS            0x02
#define SMBIOS_CONNECTOR_PROPRIETARY                0x03
#define SMBIOS_CONNECTOR_DB25_MALE                  0x04
#define SMBIOS_CONNECTOR_DB25_FEMALE                0x05
#define SMBIOS_CONNECTOR_DB15_MALE                  0x06
#define SMBIOS_CONNECTOR_DB15_FEMALE                0x07
#define SMBIOS_CONNECTOR_DB9_MALE                   0x08
#define SMBIOS_CONNECTOR_DB9_FEMALE                 0x09
#define SMBIOS_CONNECTOR_RJ11                       0x0A
#define SMBIOS_CONNECTOR_RJ45                       0x0B
#define SMBIOS_CONNECTOR_50_PIN_MINISCSI            0x0C
#define SMBIOS_CONNECTOR_MINI_DIN                   0x0D
#define SMBIOS_CONNECTOR_MICRO_DIN                  0x0E
#define SMBIOS_CONNECTOR_PS2                        0x0F
#define SMBIOS_CONNECTOR_INFRARED                   0x10
#define SMBIOS_CONNECTOR_HP_HIL                     0x11
#define SMBIOS_CONNECTOR_ACCESS_BUS_USB             0x12
#define SMBIOS_CONNECTOR_SSA_SCSI                   0x13
#define SMBIOS_CONNECTOR_CIRCULAR_DIN8_MALE         0x14
#define SMBIOS_CONNECTOR_CIRCULAR_DIN8_FEMALE       0x15
#define SMBIOS_CONNECTOR_ON_BOARD_IDE               0x16
#define SMBIOS_CONNECTOR_ON_BOARD_FLOPPY            0x17
#define SMBIOS_CONNECTOR_9_PIN_DUAL_INLINE          0x18
#define SMBIOS_CONNECTOR_25_PIN_DUAL_INLINE         0x19
#define SMBIOS_CONNECTOR_50_PIN_DUAL_INLINE         0x1A
#define SMBIOS_CONNECTOR_68_PIN_DUAL_INLINE         0x1B
#define SMBIOS_CONNECTOR_CDROM_SOUND_INPUT          0x1C
#define SMBIOS_CONNECTOR_MINI_CENTRONICS_TYPE_14    0x1D
#define SMBIOS_CONNECTOR_MINI_CENTRONICS_TYPE_26    0x1E
#define SMBIOS_CONNECTOR_HEADPHONES                 0x1F
#define SMBIOS_CONNECTOR_BNC                        0x20
#define SMBIOS_CONNECTOR_1394                       0x21
#define SMBIOS_CONNECTOR_SAS_SATA                   0x22
#define SMBIOS_CONNECTOR_PC98                       0xA0
#define SMBIOS_CONNECTOR_PC98_HIRESO                0xA1
#define SMBIOS_CONNECTOR_PCH98                      0xA2
#define SMBIOS_CONNECTOR_PC98_NOTE                  0xA3
#define SMBIOS_CONNECTOR_PC98_FULL                  0xA4
#define SMBIOS_CONNECTOR_OTHER                      0xFF

#define SMBIOS_PORT_NONE                            0x00
#define SMBIOS_PORT_PARALLEL_XT_AT                  0x01
#define SMBIOS_PORT_PARALLEL_PS2                    0x02
#define SMBIOS_PORT_PARALLEL_ECP                    0x03
#define SMBIOS_PORT_PARALLEL_EPP                    0x04
#define SMBIOS_PORT_PARALLEL_ECP_EPP                0x05
#define SMBIOS_PORT_SERIAL_XT_AT                    0x06
#define SMBIOS_PORT_SERIAL_16450                    0x07
#define SMBIOS_PORT_SERIAL_16550                    0x08
#define SMBIOS_PORT_SERIAL_16550A                   0x09
#define SMBIOS_PORT_SCSI                            0x0A
#define SMBIOS_PORT_MIDI                            0x0B
#define SMBIOS_PORT_JOYSTICK                        0x0C
#define SMBIOS_PORT_KEYBOARD                        0x0D
#define SMBIOS_PORT_MOUSE                           0x0E
#define SMBIOS_PORT_SSA_SCSI                        0x0F
#define SMBIOS_PORT_USB                             0x10
#define SMBIOS_PORT_FIREWIRE                        0x11
#define SMBIOS_PORT_PCMCIA_TYPE_I                   0x12
#define SMBIOS_PORT_PCMCIA_TYPE_II                  0x13
#define SMBIOS_PORT_PCMCIA_TYPE_III                 0x14
#define SMBIOS_PORT_CARDBUS                         0x15
#define SMBIOS_PORT_ACCESS_BUS_PORT                 0x16
#define SMBIOS_PORT_SCSI_II                         0x17
#define SMBIOS_PORT_SCSI_WIDE                       0x18
#define SMBIOS_PORT_PC98                            0x19
#define SMBIOS_PORT_PC98_HIRESO                     0x1A
#define SMBIOS_PORT_PCH98                           0x1B
#define SMBIOS_PORT_VIDEO                           0x1C
#define SMBIOS_PORT_AUDIO                           0x1D
#define SMBIOS_PORT_MODEM                           0x1E
#define SMBIOS_PORT_NETWORK                         0x1F
#define SMBIOS_PORT_SATA                            0x20
#define SMBIOS_PORT_SAS                             0x21
#define SMBIOS_PORT_8251                            0xA0
#define SMBIOS_PORT_8251_FIFO                       0xA1
#define SMBIOS_PORT_OTHER                           0xFF

//
// Define SMBIOS slot types.
//

#define SMBIOS_SLOT_TYPE_OTHER                      0x01
#define SMBIOS_SLOT_TYPE_UNKNOWN                    0x02
#define SMBIOS_SLOT_TYPE_ISA                        0x03
#define SMBIOS_SLOT_TYPE_MCA                        0x04
#define SMBIOS_SLOT_TYPE_EISA                       0x05
#define SMBIOS_SLOT_TYPE_PCI                        0x06
#define SMBIOS_SLOT_TYPE_PCMCIA                     0x07
#define SMBIOS_SLOT_TYPE_VESA                       0x08
#define SMBIOS_SLOT_TYPE_PROPRIETARY                0x09
#define SMBIOS_SLOT_TYPE_PROCESSOR                  0x0A
#define SMBIOS_SLOT_TYPE_PROPRIETARY_MEMORY         0x0B
#define SMBIOS_SLOT_TYPE_IO_RISER                   0x0C
#define SMBIOS_SLOT_TYPE_NUBUS                      0x0D
#define SMBIOS_SLOT_TYPE_PCI_66MHZ                  0x0E
#define SMBIOS_SLOT_TYPE_AGP                        0x0F
#define SMBIOS_SLOT_TYPE_AGP_2X                     0x10
#define SMBIOS_SLOT_TYPE_AGP_4X                     0x11
#define SMBIOS_SLOT_TYPE_PCI_X                      0x12
#define SMBIOS_SLOT_TYPE_AGP_8X                     0x13
#define SMBIOS_SLOT_TYPE_PC98_C20                   0xA0
#define SMBIOS_SLOT_TYPE_PC98_C24                   0xA1
#define SMBIOS_SLOT_TYPE_PC98_E                     0xA2
#define SMBIOS_SLOT_TYPE_PC98_LOCAL_BUS             0xA3
#define SMBIOS_SLOT_TYPE_PC98_CARD                  0xA4
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS                0xA5
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_X1             0xA6
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_X2             0xA7
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_X4             0xA8
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_X8             0xA9
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_X16            0xAA
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G2             0xAB
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G2_X1          0xAC
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G2_X2          0xAD
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G2_X4          0xAE
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G2_X8          0xAF
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G2_X16         0xB0
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G3             0xB1
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G3_X1          0xB2
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G3_X2          0xB3
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G3_X4          0xB4
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G3_X8          0xB5
#define SMBIOS_SLOT_TYPE_PCI_EXPRESS_G3_X16         0xB6

#define SMBIOS_SLOT_WIDTH_OTHER                     0x01
#define SMBIOS_SLOT_WIDTH_UNKNOWN                   0x02
#define SMBIOS_SLOT_WIDTH_8_BIT                     0x03
#define SMBIOS_SLOT_WIDTH_16_BIT                    0x04
#define SMBIOS_SLOT_WIDTH_32_BIT                    0x05
#define SMBIOS_SLOT_WIDTH_64_BIT                    0x06
#define SMBIOS_SLOT_WIDTH_128_BIT                   0x07
#define SMBIOS_SLOT_WIDTH_1X                        0x08
#define SMBIOS_SLOT_WIDTH_2X                        0x09
#define SMBIOS_SLOT_WIDTH_4X                        0x0A
#define SMBIOS_SLOT_WIDTH_8X                        0x0B
#define SMBIOS_SLOT_WIDTH_12X                       0x0C
#define SMBIOS_SLOT_WIDTH_16X                       0x0D
#define SMBIOS_SLOT_WIDTH_32X                       0x0E

#define SMBIOS_SLOT_USAGE_OTHER                     0x01
#define SMBIOS_SLOT_USAGE_UNKNOWN                   0x02
#define SMBIOS_SLOT_USAGE_AVAILABLE                 0x03
#define SMBIOS_SLOT_USAGE_IN_USE                    0x04

#define SMBIOS_SLOT_LENGTH_OTHER                    0x01
#define SMBIOS_SLOT_LENGTH_UNKNOWN                  0x02
#define SMBIOS_SLOT_LENGTH_SHORT                    0x03
#define SMBIOS_SLOT_LENGTH_LONG                     0x04

#define SMBIOS_SLOT_CHARACTERISTIC1_UNKNOWN         (1 << 0)
#define SMBIOS_SLOT_CHARACTERISTIC1_5_VOLTS         (1 << 1)
#define SMBIOS_SLOT_CHARACTERISTIC1_3V3_VOLTS       (1 << 2)
#define SMBIOS_SLOT_CHARACTERISTIC1_OPENING_SHARED  (1 << 3)
#define SMBIOS_SLOT_CHARACTERISTIC1_PC_CARD_16      (1 << 4)
#define SMBIOS_SLOT_CHARACTERISTIC1_CARDBUS         (1 << 5)
#define SMBIOS_SLOT_CHARACTERISTIC1_ZOOM_VIDEO      (1 << 6)
#define SMBIOS_SLOT_CHARACTERISTIC1_MODEM_RING_RESUME (1 << 7)

#define SMBIOS_SLOT_CHARACTERISTIC2_PCI_PME         (1 << 0)
#define SMBIOS_SLOT_CHARACTERISTIC2_HOT_PLUG        (1 << 1)
#define SMBIOS_SLOT_CHARACTERISTIC2_SMBUS           (1 << 2)

//
// Define SMBIOS memory array type bits.
//

#define SMBIOS_MEMORY_LOCATION_OTHER                0x01
#define SMBIOS_MEMORY_LOCATION_UNKNOWN              0x02
#define SMBIOS_MEMORY_LOCATION_MOTHERBOARD          0x03
#define SMBIOS_MEMORY_LOCATION_ISA_CARD             0x04
#define SMBIOS_MEMORY_LOCATION_EISA_CARD            0x05
#define SMBIOS_MEMORY_LOCATION_PCI_CARD             0x06
#define SMBIOS_MEMORY_LOCATION_MCA_CARD             0x07
#define SMBIOS_MEMORY_LOCATION_PCMCIA_CARD          0x08
#define SMBIOS_MEMORY_LOCATION_PROPRIETARY_CARD     0x09
#define SMBIOS_MEMORY_LOCATION_NUBUS                0x0A
#define SMBIOS_MEMORY_LOCATION_PC98_C20_CARD        0xA0
#define SMBIOS_MEMORY_LOCATION_PC98_C24_CARD        0xA1
#define SMBIOS_MEMORY_LOCATION_PC98_E_CARD          0xA2
#define SMBIOS_MEMORY_LOCATION_PC98_LOCAL_BUS_CARD  0xA3

#define SMBIOS_MEMORY_USE_OTHER                     0x01
#define SMBIOS_MEMORY_USE_UNKNOWN                   0x02
#define SMBIOS_MEMORY_USE_SYSTEM_MEMORY             0x03
#define SMBIOS_MEMORY_USE_VIDEO_MEMORY              0x04
#define SMBIOS_MEMORY_USE_FLASH_MEMORY              0x05
#define SMBIOS_MEMORY_USE_NON_VOLATILE_RAM          0x06
#define SMBIOS_MEMORY_USE_CACHE_MEMORY              0x07

#define SMBIOS_MEMORY_ERROR_CORRECTION_OTHER            0x01
#define SMBIOS_MEMORY_ERROR_CORRECTION_UNKNOWN          0x02
#define SMBIOS_MEMORY_ERROR_CORRECTION_NONE             0x03
#define SMBIOS_MEMORY_ERROR_CORRECTION_PARITY           0x04
#define SMBIOS_MEMORY_ERROR_CORRECTION_SINGLE_BIT_ECC   0x05
#define SMBIOS_MEMORY_ERROR_CORRECTION_MULTI_BIT_ECC    0x06
#define SMBIOS_MEMORY_ERROR_CORRECTION_CRC 0x07

//
// Define SMBIOS memory device flags.
//

#define SMBIOS_MEMORY_FORM_OTHER                    0x01
#define SMBIOS_MEMORY_FORM_UNKNOWN                  0x02
#define SMBIOS_MEMORY_FORM_SIMM                     0x03
#define SMBIOS_MEMORY_FORM_SIP                      0x04
#define SMBIOS_MEMORY_FORM_CHIP                     0x05
#define SMBIOS_MEMORY_FORM_DIP                      0x06
#define SMBIOS_MEMORY_FORM_ZIP                      0x07
#define SMBIOS_MEMORY_FORM_PROPRIETARY_CARD         0x08
#define SMBIOS_MEMORY_FORM_DIMM                     0x09
#define SMBIOS_MEMORY_FORM_TSOP                     0x0A
#define SMBIOS_MEMORY_FORM_ROW_OF_CHIPS             0x0B
#define SMBIOS_MEMORY_FORM_RIMM                     0x0C
#define SMBIOS_MEMORY_FORM_SODIMM                   0x0D
#define SMBIOS_MEMORY_FORM_SRIMM                    0x0E
#define SMBIOS_MEMORY_FORM_FB_DIMM                  0x0F

#define SMBIOS_MEMORY_DEVICE_OTHER                  0x01
#define SMBIOS_MEMORY_DEVICE_UNKNOWN                0x02
#define SMBIOS_MEMORY_DEVICE_DRAM                   0x03
#define SMBIOS_MEMORY_DEVICE_EDRAM                  0x04
#define SMBIOS_MEMORY_DEVICE_VRAM                   0x05
#define SMBIOS_MEMORY_DEVICE_SRAM                   0x06
#define SMBIOS_MEMORY_DEVICE_RAM                    0x07
#define SMBIOS_MEMORY_DEVICE_ROM                    0x08
#define SMBIOS_MEMORY_DEVICE_FLASH                  0x09
#define SMBIOS_MEMORY_DEVICE_EEPROM                 0x0A
#define SMBIOS_MEMORY_DEVICE_FEPROM                 0x0B
#define SMBIOS_MEMORY_DEVICE_EPROM                  0x0C
#define SMBIOS_MEMORY_DEVICE_CDRAM                  0x0D
#define SMBIOS_MEMORY_DEVICE_3DRAM                  0x0E
#define SMBIOS_MEMORY_DEVICE_SDRAM                  0x0F
#define SMBIOS_MEMORY_DEVICE_SGRAM                  0x10
#define SMBIOS_MEMORY_DEVICE_RDRAM                  0x11
#define SMBIOS_MEMORY_DEVICE_DDR                    0x12
#define SMBIOS_MEMORY_DEVICE_DDR2                   0x13
#define SMBIOS_MEMORY_DEVICE_DDR2_FB_DIMM           0x14
#define SMBIOS_MEMORY_DEVICE_DDR3                   0x18
#define SMBIOS_MEMORY_DEVICE_FBD2                   0x19

#define SMBIOS_MEMORY_DETAIL_OTHER                  (1 << 1)
#define SMBIOS_MEMORY_DETAIL_UNKNOWN                (1 << 2)
#define SMBIOS_MEMORY_DETAIL_FAST_PAGED             (1 << 3)
#define SMBIOS_MEMORY_DETAIL_STATIC_COLUMN          (1 << 4)
#define SMBIOS_MEMORY_DETAIL_PSEUDO_STATIC          (1 << 5)
#define SMBIOS_MEMORY_DETAIL_RAMBUS                 (1 << 6)
#define SMBIOS_MEMORY_DETAIL_SYNCHRONOUS            (1 << 7)
#define SMBIOS_MEMORY_DETAIL_CMOS                   (1 << 8)
#define SMBIOS_MEMORY_DETAIL_EDO                    (1 << 9)
#define SMBIOS_MEMORY_DETAIL_WINDOW_DRAM            (1 << 10)
#define SMBIOS_MEMORY_DETAIL_CACHE_DRAM             (1 << 11)
#define SMBIOS_MEMORY_DETAIL_NON_VOLATILE           (1 << 12)
#define SMBIOS_MEMORY_DETAIL_BUFFERED               (1 << 13)
#define SMBIOS_MEMORY_DETAIL_UNBUFFERED             (1 << 14)
#define SMBIOS_MEMORY_DETAIL_LRDIMM                 (1 << 15)

//
// Define boot status data.
//

#define SMBIOS_BOOT_NO_ERRORS                           0
#define SMBIOS_BOOT_NO_BOOTABLE_MEDIA                   1
#define SMBIOS_BOOT_NORMAL_OS_FAILED_LOAD               2
#define SMBIOS_BOOT_FIRMWARE_DETECTED_HARDWARE_FAILURE  3
#define SMBIOS_BOOT_OS_DETECTED_HARDWARE_FAILURE        4
#define SMBIOS_BOOT_USER_REQUESTED_BOOT                 5
#define SMBIOS_BOOT_SECURITY_VIOLATION                  6
#define SMBIOS_BOOT_PREVIOUSLY_REQUESTED_IMAGE          7
#define SMBIOS_BOOT_WATCHDOG_RESET                      8

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SMBIOS_TABLE_TYPE {
    SmbiosBiosInformation           = 0,
    SmbiosSystemInformation         = 1,
    SmbiosModuleInformation         = 2,
    SmbiosSystemEnclosure           = 3,
    SmbiosProcessorInformation      = 4,
    SmbiosCacheInformation          = 7,
    SmbiosPortConnector             = 8,
    SmbiosSystemSlots               = 9,
    SmbiosOemStrings                = 11,
    SmbiosPhysicalMemoryArray       = 16,
    SmbiosMemoryDevice              = 17,
    SmbiosMemoryArrayMappedAddress  = 19,
    SmbiosMemoryDeviceMappedAddress = 20,
    SmbiosSystemBootInformation     = 32,
    SmbiosInactive                  = 126,
    SmbiosEndOfTable                = 127
} SMBIOS_TABLE_TYPE, *PSMBIOS_TABLE_TYPE;

/*++

Structure Description:

    This structure defines the entry point structure for the SMBIOS tables.
    On legacy PC/AT systems, this structure is located somewhere between
    address 0xF0000 and 0xFFFFF aligned to a 16-byte boundary.

Members:

    AnchorString - Stores the string constant '_SM_'.

    Checksum - Stores the checksum of the table bytes. When all bytes including
        this checksum are summed, the result should be zero. The length to
        checksum is the entry point length field.

    EntryPointLength - Stores the length of the entry point structure, and the
        number of bytes to checksum. This should be 0x1F.

    MajorVersion - Stores the major version of the specification implemented in
        this table structure.

    MinorVersion - Stores the minor version of the specification implemtned in
        this table structure.

    MaxStructureSize - Stores the size of the largest structure, in bytes, and
        encompasses the structure's formatted area and text strings.

    EntryPointRevision - Stores the format of the formatted area. This is
        currently set to zero, and the formatted area is reserved.

    FormattedArea - Stores reserved bytes that may be used in the future.

    IntermediateAnchor - Stores the intermediate anchor string "_DMI_" (not
        null terminated).

    IntermediateChecksum - Stores the checksum such that all bytes starting at
        the intermediate anchor and going for length 0xF sum to zero.

    StructureTableLength - Stores the length of the SMBIOS structure table,
        in bytes.

    StructureTableAddress - Stores the 32-bit physical address of the read-only
        SMBIOS structure table. These structures are fully-packed.

    NumberOfStructures - Stores the count of structures in the array.

    BcdRevision - Stores the major and minor SMBIOS specification version
        numbers in binary coded decimal form. Revision 2.1 for instance would
        have a value 0x21.

--*/

typedef struct _SMBIOS_ENTRY_POINT {
    ULONG AnchorString;
    UCHAR Checksum;
    UCHAR EntryPointLength;
    UCHAR MajorVersion;
    UCHAR MinorVersion;
    USHORT MaxStructureSize;
    UCHAR EntryPointRevision;
    UCHAR FormattedArea[5];
    UCHAR IntermediateAnchor[5];
    UCHAR IntermediateChecksum;
    USHORT StructureTableLength;
    ULONG StructureTableAddress;
    USHORT NumberOfStructures;
    UCHAR BcdRevision;
} PACKED SMBIOS_ENTRY_POINT, *PSMBIOS_ENTRY_POINT;

/*++

Structure Description:

    This structure defines the common header that all SMBIOS structures start
    with.

Members:

    Type - Stores the structure type, which defines the remaining content of
        the structure after this header.

    Length - Stores the length of the formatted area of the structure. The
        unformatted area always ends with a double null character (two zero
        bytes).

    Handle - Stores the structure's handle, a unique 16-bit number in the range
        of 0x0000 to 0xFFFE (or 0xFEFF for version 2.1 and later).

--*/

typedef struct _SMBIOS_HEADER {
    UCHAR Type;
    UCHAR Length;
    USHORT Handle;
} PACKED SMBIOS_HEADER, *PSMBIOS_HEADER;

/*++

Structure Description:

    This structure defines the SMBIOS BIOS information structure.

Members:

    Header - Stores the common header. The structure type is 0, BIOS
        information.

    Vendor - Stores the string index of the BIOS Vendor name string.

    BiosVersion - Stores the string index of the BIOS version string.

    BiosStartingAddressSegment - Stores the segment location of the BIOS
        starting address. The size of the runtime BIOS can be computed by
        subtracting the starting address segment from 0x10000 and multiplying
        the result by 16.

    BiosReleaseDate - Stores the string index of the BIOS release date. This
        should be in the format mm/dd/yyyy. The year could be a two digit year,
        in which case 19xx is assumed.

    BiosRomSize - Stores the size, where 64K * (size + 1) is the size of the
        physical device containing the BIOS, in bytes.

    BiosCharacteristics - Stores a bitfield of BIOS characteristics.

    BiosCharacteristicsExtensions - Stores additional BIOS characteristics.

    BiosMajorRelease - Stores the major release number of the BIOS.

    BiosMinorRelease - Stores the minor release number of the BIOS.

    EmbeddedControllerFirmwareMajorRelease - Stores the major release number of
        the firmware in the embedded controller.

    EmbeddedControllerFirmwareMinorRelease - Stores the minor release number of
        the firmware in the embedded controller.

--*/

typedef struct _SMBIOS_BIOS_INFORMATION {
    SMBIOS_HEADER Header;
    UCHAR Vendor;
    UCHAR BiosVersion;
    USHORT BiosStartingAddressSegment;
    UCHAR BiosReleaseDate;
    UCHAR BiosRomSize;
    ULONGLONG BiosCharacteristics;
    ULONG BiosCharacteristicsExtensions;
    UCHAR BiosMajorRelease;
    UCHAR BiosMinorRelease;
    UCHAR EmbeddedControllerFirmwareMajorRelease;
    UCHAR EmbeddedControllerFirmwareMinorRelease;
} PACKED SMBIOS_BIOS_INFORMATION, *PSMBIOS_BIOS_INFORMATION;

/*++

Structure Description:

    This structure defines the SMBIOS system information structure.

Members:

    Header - Stores the common header. The structure type is 1, system
        information.

    Manufacturer - Stores the string index of the system manufacturer.

    ProductName - Stores the string index of the product name.

    Version - Stores the string index of the product version string.

    SerialNumber - Stores the string index of the serial number string.

    Uuid - Stores the universally unique identifier of the system. If the value
        is all ones, the ID is not currently set, but it can be. If the value
        is all zeros, then the ID is not set.

    WakeupType - Stores the event that caused the system to power up. See the
        SMBIOS_SYSTEM_WAKEUP_* definitions.

    SkuNumber - Stores the index of the SKU number string. This text identifies
        a particular computer configuration for sale.

    Family - Stores the index of the family string. A family refers to a set of
        computers that are similar but not identical from a hardware or
        software point of view.

--*/

typedef struct _SMBIOS_SYSTEM_INFORMATION {
    SMBIOS_HEADER Header;
    UCHAR Manufacturer;
    UCHAR ProductName;
    UCHAR Version;
    UCHAR SerialNumber;
    UCHAR Uuid[16];
    UCHAR WakeupType;
    UCHAR SkuNumber;
    UCHAR Family;
} PACKED SMBIOS_SYSTEM_INFORMATION, *PSMBIOS_SYSTEM_INFORMATION;

/*++

Structure Description:

    This structure defines the SMBIOS module/baseboard information structure.

Members:

    Header - Stores the common header. The structure type is 1, system
        information.

    Manufacturer - Stores the string index of the manufacturer string.

    Product - Stores the string index of the product string.

    Version - Stores the string index of the product version.

    SerialNumber - Stores the string index of the serial number.

    AssetTag - Stores the string index of the asset tag.

    FeatureFlags - Stores the bitfield of feature flags for the baseboard.
        See SMBIOS_MODULE_* definitions.

    ChassisLocation - Stores the string index of the string that describes the
        board's location within the chassis referenced by the chassis handle.

    ChassisHandle - Stores the table handle of the chassis this board resides
        in.

    BoardType - Stores the board type. See SMBIOS_MODULE_TYPE_* definitions.

    NumberOfContainedObjectHandles - Stores the count of handles to objects
        within this board.

    ContainedObjectHandles - Stores a variable sized array of handles to
        other objects that are contained within this module or baseboard.

--*/

typedef struct _SMBIOS_MODULE_INFORMATION {
    SMBIOS_HEADER Header;
    UCHAR Manufacturer;
    UCHAR Product;
    UCHAR Version;
    UCHAR SerialNumber;
    UCHAR AssetTag;
    UCHAR FeatureFlags;
    UCHAR ChassisLocation;
    USHORT ChassisHandle;
    UCHAR BoardType;
    UCHAR NumberOfContainedObjectHandles;
    USHORT ContainedObjectHandles[ANYSIZE_ARRAY];
} PACKED SMBIOS_MODULE_INFORMATION, *PSMBIOS_MODULE_INFORMATION;

/*++

Structure Description:

    This structure defines an SMBIOS enclosure or chassis.

Members:

    Header - Stores the common header. The structure type is 3, enclosure or
        chassis.

    Manufacturer - Stores the string index of the chassis manufacturer.

    Type - Stores the chassis type. Bit 7 is set if a chassis lock is present.
        Bits 6:0 correspond to SMBIOS_ENCLOSURE_TYPE_* definitions.

    Version - Stores the index of the version string.

    SerialNumber - Stores the string index of the serial number.

    AssetTag - Stores the string index of the asset tag.

    BootState - Stores the state of the enclosure when it was last booted.

    PowerSupplyState - Stores the state of the enclosure's power supply when it
        was last booted.

    ThermalState - Stores the thermal state of the enclosure when it was last
        booted.

    SecurityStatus - Stores the security state of the enclosure when it was
        last booted.

    OemDefined - Stores OEM defined data.

    Height - Stores the enclosure height, in U units. A U is a standard unit of
        server height equal to 1.75 inches or 4.445 cm. A value of 0 indicates
        the height in unspecified.

    NumberOfPowerCords - Stores the number of power cords associated with the
        enclosure. Zero means the value is unspecified.

    ElementCount - Stores the number of contained elements following this
        structure.

    ElementRecordLength - Stores the number of each contained element record
        in the elements following this structure.

    SkuNumber - Stores the string index of the chassis SKU number.

--*/

typedef struct _SMBIOS_ENCLOSURE {
    SMBIOS_HEADER Header;
    UCHAR Manufacturer;
    UCHAR Type;
    UCHAR Version;
    UCHAR SerialNumber;
    UCHAR AssetTag;
    UCHAR BootState;
    UCHAR PowerSupplyState;
    UCHAR ThermalState;
    UCHAR SecurityStatus;
    ULONG OemDefined;
    UCHAR Height;
    UCHAR NumberOfPowerCords;
    UCHAR ElementCount;
    UCHAR ElementLength;
    UCHAR SkuNumber;
} PACKED SMBIOS_ENCLOSURE, *PSMBIOS_ENCLOSURE;

/*++

Structure Description:

    This structure defines an SMBIOS contained enclosure. These elements are
    found inside an enclosure structure.

Members:

    ElementType - Stores the type of element associated with this record. Bit 7
        indicates whether this is an SMBIOS structure type enumeration (1) or
        an SMBIOS baseboard type enumeration (0). Bits 6:0 specify either the
        board type enumeration or the structure type.

    ElementMinimum - Stores the minimum number of the element type that can be
        installed in the chassis for the chassis to properly operate.

    ElementMaximum - Stores the maximum number of the element type that can be
        installed in the chassis.

--*/

typedef struct _SMBIOS_CONTAINED_ENCLOSURE {
    UCHAR ElementType;
    UCHAR ElementMinimum;
    UCHAR ElementMaximum;
} PACKED SMBIOS_CONTAINED_ENCLOSURE, *PSMBIOS_CONTAINED_ENCLOSURE;

/*++

Structure Description:

    This structure defines SMBIOS processor information.

Members:

    Header - Stores the common header. The structure type is 4, processor
        information

    SocketDesignation - Stores the string index of the processor socket type.

    ProcessorType - Stores the processor type.

    ProcessorFamily - Stores the processor family.

    ProcessorManufacturer - Stores the string index of the processor
        manufacturer.

    ProcessorId - Stores the identifier of the processor.

    ProcessorVersion - Stores the string index of the processor version.

    Voltage - Stores the processor voltage.

    ExternalClock - Stores the external clock frequency, in megahertz. This is
        0 if the speed is unknown.

    MaxSpeed - Stores the maximum processor speed, in megahertz. This is 0 if
        the speed is unknown.

    CurrentSpeed - Stores the current speed at system boot, in megahertz. This
        is 0 if unknown.

    Status - Stores the processor status. See SMBIOS_PROCESSOR_STATUS_*
        definitions.

    ProcessorUpgrade - Stores the upgrad information.

    L1CacheHandle - Stores the handle of the L1 cache information. Set to
        0xFFFF if no cache information is supplied.

    L2CacheHandle - Stores the handle of the L2 cache information. Set to
        0xFFFF if no cache information is supplied.

    L3CacheHandle - Stores the handle of the L3 cache information. Set to
        0xFFFF if no cache information is supplied.

    SerialNumber - Stores the string index of the serial number.

    AssetTag - Stores the string index of the asset tag.

    PartNumber - Stores the string index for the part number.

    CoreCount - Stores the number of core per processor socket. If unknown,
        this value is zero.

    ThreadCount - Stores the number of threads per processor socket. If
        unknown, this value is zero.

    ProcessorCharacteristics - Stores processor characteristics flags.

    ProcessorFamily2 - Stores additional family information.

--*/

typedef struct _SMBIOS_PROCESSOR_INFORMATION {
    SMBIOS_HEADER Header;
    UCHAR SocketDesignation;
    UCHAR ProcessorType;
    UCHAR ProcessorFamily;
    UCHAR ProcessorManufacturer;
    ULONGLONG ProcessorId;
    UCHAR ProcessorVersion;
    UCHAR Voltage;
    USHORT ExternalClock;
    USHORT MaxSpeed;
    USHORT CurrentSpeed;
    UCHAR Status;
    UCHAR ProcessorUpgrade;
    USHORT L1CacheHandle;
    USHORT L2CacheHandle;
    USHORT L3CacheHandle;
    UCHAR SerialNumber;
    UCHAR AssetTag;
    UCHAR PartNumber;
    UCHAR CoreCount;
    UCHAR ThreadCount;
    USHORT ProcessorCharacteristics;
    USHORT ProcessorFamily2;
} PACKED SMBIOS_PROCESSOR_INFORMATION, *PSMBIOS_PROCESSOR_INFORMATION;

/*++

Structure Description:

    This structure defines SMBIOS cache information.

Members:

    Header - Stores the common header. The structure type is 4, processor
        information

    SocketDesignation - Stores the string index of the cache description
        string.

    CacheConfiguration - Stores the cache configuration bits. See
        SMBIOS_CACHE_* definitions.

    MaxCacheSize - Stores the maximum cache size that can be installed. This is
        either in units of kilobytes or 64 kilobytes, depending on whether bit
        15 is set.

    InstalledSize - Stores the currently installed cache size.

    SupportedSramType - Stores the supported SRAM types. See
        SMBIOS_CACHE_SRAM_* definitions.

    CurrentSramType - Stores the current SRAM type.

    CacheSpeed - Stores the cache module speed, in nanoseconds. The value 0
        indicates the speed is unknown.

    ErrorCorrectionType - Stores the error correction type. See
        SMBIOS_CACHE_ERROR_CORRECTION_* definitions.

    SystemCacheType - Stores the system cache type. See SMBIOS_CACHE_TYPE_*
        definitions.

    Associativity - Stores the associativity of the cache. See
        SMBIOS_CACHE_ASSOCIATIVITY_* definitions.

--*/

typedef struct _SMBIOS_CACHE_INFORMATION {
    SMBIOS_HEADER Header;
    UCHAR SocketDesignation;
    USHORT CacheConfiguration;
    USHORT MaxCacheSize;
    USHORT InstalledSize;
    USHORT SupportedSramType;
    USHORT CurrentSramType;
    UCHAR CacheSpeed;
    UCHAR ErrorCorrectionType;
    UCHAR SystemCacheType;
    UCHAR Associativity;
} PACKED SMBIOS_CACHE_INFORMATION, *PSMBIOS_CACHE_INFORMATION;

/*++

Structure Description:

    This structure defines the SMBIOS port connector structure, which describes
    each port on the system.

Members:

    Header - Stores the common header. The structure type is 8, port connector.

    InternalReferenceDesignator - Stores the string index of the designator
        for the connector inside the enclosure.

    InternalConnectorType - Stores the internal connector type.

    ExternalReferenceDesignator - Stores the string index of the designator
        for the connector outside the enclosure.

    ExternalConnectorType - Stores the external connector type.

    PortType - Stores the function of the port.

--*/

typedef struct _SMBIOS_PORT_CONNECTOR {
    SMBIOS_HEADER Header;
    UCHAR InternalReferenceDesignator;
    UCHAR InternalConnectorType;
    UCHAR ExternalReferenceDesignator;
    UCHAR ExternalConnectorType;
    UCHAR PortType;
} PACKED SMBIOS_PORT_CONNECTOR, *PSMBIOS_PORT_CONNECTOR;

/*++

Structure Description:

    This structure defines the SMBIOS slot structure, which describes a
    slot in the system.

Members:

    Header - Stores the common header. The structure type is 8, port connector.

    SlotDesignation - Stores the string index for the slot designation string.

    SlotType - Stores the slot type. See SMBIOS_SLOT_TYPE_* definitions.

    SlotDataBusWidth - Stores the slot data bus width.

    CurrentUsage - Stores the slot's current usage state.

    SlotLength - Stores the length of the slot.

    SlotId - Stores the slot identifier.

    SlotCharacteristics1 - Stores the first byte of characteristics. See
        SMBIOS_SLOT_CHARACTERISTIC1_* definitions.

    SlotCharacteristics2 - Stores the first byte of characteristics. See
        SMBIOS_SLOT_CHARACTERISTIC2_* definitions.

    SegmentGroupNumber - Stores the bus segment number this slot resides on.

    BusNumber - Stores the bus number this slot resides on.

    DeviceAndFunctionNumber - Stores the slot device number in bits 7:3 and
        the function number in bits 2:0.

--*/

typedef struct _SMBIOS_SLOT {
    SMBIOS_HEADER Header;
    UCHAR SlotDesignation;
    UCHAR SlotType;
    UCHAR SlotDataBusWidth;
    UCHAR CurrentUsage;
    UCHAR SlotLength;
    USHORT SlotId;
    UCHAR SlotCharacteristics1;
    UCHAR SlotCharacteristics2;
    USHORT SegmentGroupNumber;
    UCHAR BusNumber;
    UCHAR DeviceAndFunctionNumber;
} PACKED SMBIOS_SLOT, *PSMBIOS_SLOT;

/*++

Structure Description:

    This structure defines the SMBIOS OEM strings structure, which contains
    custom OEM strings.

Members:

    Header - Stores the common header. The structure type is 11, OEM strings.

    Count - Stores the number of OEM strings.

--*/

typedef struct _SMBIOS_OEM_STRINGS {
    SMBIOS_HEADER Header;
    UCHAR Count;
} PACKED SMBIOS_OEM_STRINGS, *PSMBIOS_OEM_STRINGS;

/*++

Structure Description:

    This structure defines the SMBIOS physical memory array, which describes
    a collection of memory devices that operate together to form a memory
    address space.

Members:

    Header - Stores the common header. The structure type is 16, physical
        memory array.

    Location - Stores the location of the memory array, whether on the system
        board on an add-in board.

    Use - Stores the function for which the array is used.

    MemoryErrorCorrection - Stores the primary hardware error correction or
        detection mechanism supported by this memory array.

    MaximumCapacity - Stores the maximum memory capacity, in kilobytes, for
        this array. If this value is 0x80000000 (2 TB), then the extended
        memory capacity is used instead.

    MemoryErrorInformationHandle - Stores the handle number associated with any
        error that was previously detected for the array. Set to 0xFFFE if the
        system does not provide error detection, or set to 0xFFFF if no error
        was detected.

    NumberOfMemoryDevices - Stores the count of memory devices in the array.

    ExtendedMemoryCapacity - Stores the maximum capacity in bytes for this
        array. This field is only valid when the memory capacity field is
        0x80000000.

--*/

typedef struct _SMBIOS_PHYSICAL_MEMORY_ARRAY {
    SMBIOS_HEADER Header;
    UCHAR Location;
    UCHAR Use;
    UCHAR MemoryErrorCorrection;
    ULONG MaximumCapacity;
    USHORT MemoryErrorInformationHandle;
    USHORT NumberOfMemoryDevices;
    ULONGLONG ExtendedMemoryCapacity;
} PACKED SMBIOS_PHYSICAL_MEMORY_ARRAY, *PSMBIOS_PHYSICAL_MEMORY_ARRAY;

/*++

Structure Description:

    This structure defines an SMBIOS memory device.

Members:

    Header - Stores the common header. The structure type is 17, physical
        memory device.

    PhysicalMemoryArrayHandle - Stores the handle associated with the array
        to which this device belongs.

    MemoryErrorInformationHandle - Stores the handle of the error event that
        occurred. Set to 0xFFFE if error reporting is not supported, or 0xFFFF
        if there is no error.

    TotalWidth - Stores the total width in bits of this memory device,
        including any check or error correction bits. Set to 0xFFFF if unknown.

    DataWidth - Stores the width of data in bits. Set to 0xFFFF if unknown.
        This may be zero if the device is used only for error correction.

    Size - Stores the size of the memory device. If the value is 0, no device
        is installed. Set to 0xFFFF if the size is unknown. Set to 0x7FFF if
        the size is actually stored in the extended size field.

    FormFactor - Stores the form factor of this memory device.

    DeviceSet - Stores the set number if the device belongs to a set of memory
        devices that must all be populated with the same type of memory. Set to
        0 if the device is not part of a set, or 0xFF if the attribute is
        unknown.

    DeviceLocator - Stores the string index of the string that identifies the
        physically labeled socket or board position where the device is.

    BankLocator - Stores the string index of the string that identifies the
        physically labeled bank where the memory is located.

    MemoryType - Stores the memory type.

    TypeDetail - Stores additional details about the memory type.

    Speed - Stores the maximum capable speed of the device, in MHz. Set to
        0 if unknown. Note that in this case MHz = (1000 / n) nanoseconds.

    Manufacturer - Stores the string index of the memory manufacturer.

    SerialNumber - Stores the string index of the serial number.

    AssetTag - Stores the string index of the asset tag.

    PartNumber - Stores the string index of the part number.

    Attributes - Stores memory attributes. Bits 3:0 store the rank number, or
        0 for unknown rank.

    ExtendedSize - Stores the extended size of the memory device, in megabytes.
        This is only valid if it is too big to fit in the size field.

    ConfiguredMemoryClockSpeed - Stores the configured clock speed to the
        memory device, in MHz. Set to 0 if unknown.

    MinimumVoltage - Stores the minimum operating voltage in millivolts.

    MaximumVoltage - Stores the maximum operating voltage in millivolts.

    ConfiguredVoltage - Stores the configured operating voltage in millivolts.

--*/

typedef struct _SMBIOS_MEMORY_DEVICE {
    SMBIOS_HEADER Header;
    USHORT PhysicalMemoryArrayHandle;
    USHORT MemoryErrorInformationHandle;
    USHORT TotalWidth;
    USHORT DataWidth;
    USHORT Size;
    UCHAR FormFactor;
    UCHAR DeviceSet;
    UCHAR DeviceLocator;
    UCHAR BankLocator;
    UCHAR MemoryType;
    USHORT TypeDetail;
    USHORT Speed;
    UCHAR Manufacturer;
    UCHAR SerialNumber;
    UCHAR AssetTag;
    UCHAR PartNumber;
    UCHAR Attributes;
    ULONG ExtendedSize;
    USHORT ConfiguredMemoryClockSpeed;
    USHORT MinimumVoltage;
    USHORT MaximumVoltage;
    USHORT ConfiguredVoltage;
} PACKED SMBIOS_MEMORY_DEVICE, *PSMBIOS_MEMORY_DEVICE;

/*++

Structure Description:

    This structure defines an SMBIOS memory array mapped address.

Members:

    Header - Stores the common header. The structure type is 19, memory array
        mapped address.

    StartingAddress - Stores the physical address in kilobytes of a range of
        area mapped to the specified memory array. Use the extended starting
        address if this field is 0xFFFFFFFF.

    EndingAddress - Stores the physical ending address of the last kilobyte
        of the range mapped to the specified memory array. Use the extended
        end address is this field is 0xFFFFFFFF.

    MemoryArrayHandle - Stores the handle of the memory array associated with
        this mapping.

    PartitionWidth - Stores the number of memory devices that form a single
        row of memory for the address partition defined by this structure.

    ExtendedStartingAddress - Stores the physical address in bytes of the
        start of the range. This is only valid if the starting address is
        0xFFFFFFFF.

    ExtendedEndingAddress - Stores the physical ending addres in bytes of the
        last byte of the range. This is only valid if the starting address is
        0xFFFFFFFF.

--*/

typedef struct _SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS {
    SMBIOS_HEADER Header;
    ULONG StartingAddress;
    ULONG EndingAddress;
    USHORT MemoryArrayHandle;
    UCHAR PartitionWidth;
    ULONGLONG ExtendedStartingAddress;
    ULONGLONG ExtendedEndingAddress;
} PACKED SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS,
    *PSMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS;

/*++

Structure Description:

    This structure defines an SMBIOS memory array mapped address.

Members:

    Header - Stores the common header. The structure type is 20, memory device
        mapped address.

    StartingAddress - Stores the physical address in kilobytes of a range of
        area mapped to the specified memory array. Use the extended starting
        address if this field is 0xFFFFFFFF.

    EndingAddress - Stores the physical ending address of the last kilobyte
        of the range mapped to the specified memory array. Use the extended
        end address is this field is 0xFFFFFFFF.

    MemoryDeviceHandle - Stores the handle of the memory device this mapping
        describes.

    MemoryArrayMappedAddressHandle - Stores the handle of the memory array
        mapped address associated with this mapping.

    PartitionRowPosition - Stores the position of the referenced memory device
        in a row of the address partition. For example, if two 8-bit devices
        form a 16-bit row, the field's value is either 1 or 2. If unknown, set
        to 0xFF.

    InterleavePosition - Stores the position of the referenced memory device
        in an interleave. 0 indicates non-interleaved, 1 indicates first
        interleave position, 2 indicates second interleave position, and so on.
        Set to 0xFF if unknown.

    InterleavedDataDepth - Stores the maximum number of consecutive rows from
        the referenced memory device that are accessed in a single interleaved
        transfer. Set to 0 if the device is not part of an interleave, or 0xFF
        if the value is unknown.

    ExtendedStartingAddress - Stores the physical address in bytes of the
        start of the range. This is only valid if the starting address is
        0xFFFFFFFF.

    ExtendedEndingAddress - Stores the physical ending addres in bytes of the
        last byte of the range. This is only valid if the starting address is
        0xFFFFFFFF.

--*/

typedef struct _SMBIOS_MEMORY_DEVICE_MAPPED_ADDRESS {
    SMBIOS_HEADER Header;
    ULONG StartingAddress;
    ULONG EndingAddress;
    USHORT MemoryDeviceHandle;
    USHORT MemoryArrayMappedAddressHandle;
    UCHAR PartitionRowPosition;
    UCHAR InterleavePosition;
    UCHAR InterleavedDataDepth;
    ULONGLONG ExtendedStartingAddress;
    ULONGLONG ExtendedEndingAddress;
} PACKED SMBIOS_MEMORY_DEVICE_MAPPED_ADDRESS,
    *PSMBIOS_MEMORY_DEVICE_MAPPED_ADDRESS;

/*++

Structure Description:

    This structure defines an SMBIOS boot information.

Members:

    Header - Stores the common header. The structure type is 32, boot
        information.

    Reserved - Stores six reserved bytes that are currently all set to zero.

    BootStatus - Stores the boot status data.

--*/

typedef struct _SMBIOS_BOOT_INFORMATION {
    SMBIOS_HEADER Header;
    UCHAR Reserved[6];
    UCHAR BootStatus[ANYSIZE_ARRAY];
} PACKED SMBIOS_BOOT_INFORMATION, *PSMBIOS_BOOT_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
