/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    acpitabs.h

Abstract:

    This header contains definitions for tables defined by the Advanced
    Configuration and Power Interface specification.

Author:

    Evan Green 4-Aug-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// Well known table signatures.
//

#define RSDP_SIGNATURE 0x2052545020445352ULL // "RSD PTR "
#define RSDT_SIGNATURE 0x54445352 // 'RSDT'
#define XSDT_SIGNATURE 0x54445358 // 'XSDT'
#define FADT_SIGNATURE 0x50434146 // 'FACP'
#define FACS_SIGNATURE 0x53434146 // 'FACS'
#define MADT_SIGNATURE 0x43495041 // 'APIC'
#define DSDT_SIGNATURE 0x54445344 // 'DSDT'
#define SSDT_SIGNATURE 0x54445353 // 'SSDT'
#define DBG2_SIGNATURE 0x32474244 // 'DBG2'
#define GTDT_SIGNATURE 0x54445447 // 'GTDT'

#define ACPI_20_RSDP_REVISION 0x02
#define ACPI_30_RSDT_REVISION 0x01
#define ACPI_30_XSDT_REVISION 0x01

//
// Normally the entire contents of the table is checksummed, however in the
// case of the RSDP only the bytes defined in ACPI 1.0 are checksummed.
//

#define RSDP_CHECKSUM_SIZE 20

typedef enum _ADDRESS_SPACE_TYPE {
    AddressSpaceMemory = 0,
    AddressSpaceIo = 1,
    AddressSpacePciConfig = 2,
    AddressSpaceEmbeddedController = 3,
    AddressSpaceSmBus = 4,
    AddressSpaceFixedHardware = 0x7F
} ADDRESS_SPACE_TYPE, *PADDRESS_SPACE_TYPE;

typedef enum _MADT_ENTRY_TYPE {
    MadtEntryTypeLocalApic                = 0x0,
    MadtEntryTypeIoApic                   = 0x1,
    MadtEntryTypeInterruptOverride        = 0x2,
    MadtEntryTypeNmiSource                = 0x3,
    MadtEntryTypeLocalApicNmi             = 0x4,
    MadtEntryTypeLocalApicAddressOverride = 0x5,
    MadtEntryTypeIoSapic                  = 0x6,
    MadtEntryTypeLocalSapic               = 0x7,
    MadtEntryTypePlatformInterruptSource  = 0x8,
    MadtEntryTypeLocalX2Apic              = 0x9,
    MadtEntryTypeGic                      = 0xB,
    MadtEntryTypeGicDistributor           = 0xC,
} MADT_ENTRY_TYPE, *PMADT_ENTRY_TYPE;

//
// Define the frequency of the ACPI PM timer.
//

#define PM_TIMER_FREQUENCY 3579545

//
// Define the values for the argument to the \_PIC method.
//

#define ACPI_INTERRUPT_PIC_MODEL 0
#define ACPI_INTERRUPT_APIC_MODEL 1
#define ACPI_INTERRUPT_SAPIC_MODEL 2

//
// MADT Flags.
//

//
// This flag is set if the machine has a PC/AT compatible dual 8259 PIC
// interrupt controller.
//

#define MADT_FLAG_DUAL_8259 0x00000001

//
// Set if the processor is present.
//

#define MADT_LOCAL_APIC_FLAG_ENABLED 1

//
// MADT Interrupt Override flags. For those muddling through this, ISA
// interrupts that "conform to bus" are edge triggered, active low.
//

#define MADT_INTERRUPT_POLARITY_MASK 0x03
#define MADT_INTERRUPT_POLARITY_CONFORMS_TO_BUS 0x00
#define MADT_INTERRUPT_POLARITY_ACTIVE_HIGH 0x01
#define MADT_INTERRUPT_POLARITY_ACTIVE_LOW 0x03

#define MADT_INTERRUPT_TRIGGER_MODE_MASK 0xC0
#define MADT_INTERRUPT_TRIGGER_MODE_CONFORMS_TO_BUS 0x00
#define MADT_INTERRUPT_TRIGGER_MODE_EDGE 0x01
#define MADT_INTERRUPT_TRIGGER_MODE_LEVEL 0x11

//
// Set if the processor is present.
//

#define MADT_LOCAL_GIC_FLAG_ENABLED 0x00000001

//
// Set if the performance interrupt for the processor is edge triggered.
//

#define MADT_LOCAL_GIC_FLAG_PERFORMANCE_INTERRUPT_EDGE_TRIGGERED 0x00000002

//
// FADT Flags.
//

//
// Set if the processor correctly flushes the processor caches and maintains
// memory coherency when the WBINVD instruction is invoked.
//

#define FADT_FLAG_WRITEBACK_INVALIDATE_CORRECT 0x00000001

//
// Set if the processor properly flushes all caches and maintains memory
// coherency when the WBINVD instruction is invoked, but doesn't necessarily
// invalidate all caches.
//

#define FADT_FLAG_WRITEBACK_INVALIDATE_FLUSH 0x00000002

//
// Set if the C1 power state is supported on all processors.
//

#define FADT_FLAG_C1_SUPPORTED 0x00000004

//
// Set if the C2 power state can work with more than one processor.
//

#define FADT_FLAG_C2_MULTIPROCESSOR 0x00000008

//
// Set if the power button is implemented as a control method device. If not
// set, it is implemented as a fixed feature device.
//

#define FADT_FLAG_POWER_BUTTON_CONTROL_METHOD 0x00000010

//
// Set if the sleep button is implemented as a control method device. If not
// set, it is implemented as a fixed feature device.
//

#define FADT_FLAG_SLEEP_BUTTON_CONTROL_METHOD 0x00000020

//
// Set if RTC wake status is not supported in fixed register space.
//

#define FADT_FLAG_NO_RTC_FIXED_WAKE_STATUS 0x00000040

//
// Set if the RTC can wake the system from the S4 power state.
//

#define FADT_FLAG_RTC_WAKES_S4 0x00000080

//
// Set if the PM timer is 32 bits. If clear, the timer is 24 bits.
//

#define FADT_FLAG_PM_TIMER_32_BITS 0x00000100

//
// Set if the system can support docking.
//

#define FADT_FLAG_DOCKING_SUPPORTED 0x00000200

//
// Set if the ACPI reset register is supported.
//

#define FADT_FLAG_RESET_REGISTER_SUPPORTED 0x00000400

//
// Set if the system has no external expansion capabilities and the case is
// sealed.
//

#define FADT_FLAG_SEALED_CASE 0x00000800

//
// Set if the system cannot detect the monitor or keyboard/mouse devices.
//

#define FADT_FLAG_HEADLESS 0x00001000

//
// Set if the OSPM must execute a processor native instruction after writing
// the SLP_TYPx register.
//

#define FADT_FLAG_SOFTWARE_SLEEP 0x00002000

//
// Set if the platform supports waking from PCI express.
//

#define FADT_FLAG_PCI_EXPRESS_WAKE 0x00004000

//
// Set if the operating system should use a platform clock, and not a
// processor-based timer to measure time.
//

#define FADT_FLAG_USE_PLATFORM_CLOCK 0x00008000

//
// Set if the contents of the RTC_STS flag is valid when waking from S4.
//

#define FADT_FLAG_S4_RTC_STATUS_VALID 0x00010000

//
// Set if the platform is compatible with remote power on.
//

#define FADT_FLAG_REMOTE_POWER_ON_SUPPORTED 0x00020000

//
// Set if all local APICs must be used in clustered mode.
//

#define FADT_FLAG_USE_CLUSTERED_MODE 0x00040000

//
// Set if all local APICs must be used in physical destination mode.
//

#define FADT_FLAG_USE_PHYSICAL_MODE 0x00080000

//
// Set if ACPI hardware is not available.
//

#define FADT_FLAG_HARDWARE_REDUCED_ACPI 0x00100000

//
// Define IA boot flags in the FADT.
//

#define FADT_IA_FLAG_LEGACY_DEVICES             0x0001
#define FADT_IA_FLAG_8042_PRESENT               0x0002
#define FADT_IA_FLAG_VGA_NOT_PRESENT            0x0004
#define FADT_IA_FLAG_MSI_NOT_SUPPORTED          0x0008
#define FADT_IA_FLAG_PCIE_ASPM_NOT_SUPPORTED    0x0010

//
// Define PM1 Control register bit definitions.
//

//
// Set if the SCI interrupt is enabled, which is also used as an indication that
// ACPI mode is enabled. If this flag is cleared, SCI interrupts generate SMI
// interrupts.
//

#define FADT_PM1_CONTROL_SCI_ENABLED 0x00000001

//
// Set if the generation of a bus master request can cause any processor in the
// C3 state to transition to the C0 state. When this bit is not set, the
// generation of a bus master request does not affect any processor in the C3
// state.
//

#define FADT_PM1_CONTROL_BUS_MASTER_WAKE 0x00000002

//
// This write-only bit is used by the ACPI software to raise an event to the
// BIOS indicating that the OS has released the global lock.
//

#define FADT_PM1_CONTROL_GLOBAL_LOCK_RELEASED 0x00000004

//
// Defines the shift of the field that indicates the type of sleep state to
// enter when the sleep enable bit is set.
//

#define FADT_PM1_CONTROL_SLEEP_TYPE_SHIFT 10
#define FADT_PM1_CONTROL_SLEEP_TYPE 0x00001C00

//
// Define PM2 Control register bit definitions.
//

//
// This bit is set to disable the system bus arbiter, which disallows bus
// masters other than the CPU from using the system bus.
//

#define FADT_PM2_ARBITER_DISABLE 0x00000001

//
// Sends the system to sleep. The sleep level is determined by the sleep type
// bits.
//

#define FADT_PM1_CONTROL_SLEEP 0x00002000

//
// Define PM1 Event register bit definitions.
//

//
// This bit is set when the most significant bit of the PM timer rolls over.
//

#define FADT_PM1_EVENT_TIMER_STATUS 0x00000001

//
// This bit is set any time a system bus master requests the system bus. It can
// only be cleared by writing a 1 to this bit. This bit reflects bus master
// activity, not CPU activity.
//

#define FADT_PM1_EVENT_BUS_MASTER_STATUS 0x00000010

//
// This bit is set when the BIOS has raised the SCI interrupt and would the
// attention of the OS.
//

#define FADT_PM1_EVENT_GLOBAL_STATUS 0x00000020

//
// This bit is set when the power button was pressed. It is cleared by writing
// a one to this bit.
//

#define FADT_PM1_EVENT_POWER_BUTTON_STATUS 0x00000100

//
// This bit is set when the sleep button was pressed. It is cleared by writing
// a one to this bit.
//

#define FADT_PM1_EVENT_SLEEP_BUTTON_STATUS 0x00000200

//
// This bit is set when the RTC alarm has fired. It is cleared by writing a
// one to this bit.
//

#define FADT_PM1_EVENT_RTC_STATUS 0x00000400

//
// This bit is set when a PCI wake event is requested. It is cleared by writing
// a one to this bit.
//

#define FADT_PM1_EVENT_PCIE_WAKE_STATUS 0x00004000

//
// This bit is set when the system was sleeping and a wake event occurred.
// It is cleared by writing a one to it.
//

#define FADT_PM1_EVENT_WAKE_STATUS 0x00008000

//
// Define PM1 interrupt enable register bits. They correspond to the PM1 event
// register bits.
//

#define FADT_PM1_ENABLE_PM_TIMER 0x00000001
#define FADT_PM1_ENABLE_GLOBAL 0x00000020
#define FADT_PM1_ENABLE_POWER_BUTTON 0x00000100
#define FADT_PM1_ENABLE_SLEEP_BUTTON 0x00000200
#define FADT_PM1_ENABLE_RTC 0x00000400
#define FADT_PM1_ENABLE_PCIE_DISABLE 0x00004000

//
// Define FACS flags.
//

//
// This bit is set to indicate that the OS supports S4BIOS_REQ. If not
// supported, the OSPM must be able to save and restore memory state in order
// to use the S4 state.
//

#define FACS_FLAG_S4_BIOS_REQUEST_SUPPORTED 0x00000001

//
// This bit is set by the platform firmware to indicate that a 64-bit
// environment is available for the waking vector.
//

#define FACS_FLAG_64_BIT_WAKE_SUPPORTED 0x00000001

//
// This bit is set by the OS to indicate that it would like a 64-bit
// execution environment when coming out of sleep via the
// XFirmwareWakingVector.
//

#define FACS_OSPM_FLAG_64_BIT_WAKE_ENABLED 0x00000001

//
// This bit is set in the global lock to indicate that there is a request to
// own the lock.
//

#define FACS_GLOBAL_LOCK_PENDING 0x00000001

//
// This bit is set to indicate ownership of the global lock.
//

#define FACS_GLOBAL_LOCK_OWNED 0x00000002

//
// Resource descriptor definitions.
//

#define RESOURCE_DESCRIPTOR_LARGE 0x80
#define RESOURCE_DESCRIPTOR_LENGTH_MASK 0x7

//
// Small resource types.
//

#define SMALL_RESOURCE_TYPE_MASK 0x78
#define SMALL_RESOURCE_TYPE_IRQ (0x4 << 3)
#define SMALL_RESOURCE_TYPE_DMA (0x5 << 3)
#define SMALL_RESOURCE_TYPE_START_DEPENDENT_FUNCTIONS (0x6 << 3)
#define SMALL_RESOURCE_TYPE_END_DEPENDENT_FUNCTIONS (0x7 << 3)
#define SMALL_RESOURCE_TYPE_IO_PORT (0x8 << 3)
#define SMALL_RESOURCE_TYPE_FIXED_LOCATION_IO_PORT (0x9 << 3)
#define SMALL_RESOURCE_TYPE_FIXED_DMA (0xA << 3)
#define SMALL_RESOURCE_TYPE_VENDOR_DEFINED (0xE << 3)
#define SMALL_RESOURCE_TYPE_END_TAG (0xF << 3)

//
// I/O port resource bit definitions.
//

#define IO_PORT_RESOURCE_DECODES_16_BITS 0x01

//
// Large resource types.
//

#define LARGE_RESOURCE_TYPE_MASK 0x7F
#define LARGE_RESOURCE_TYPE_MEMORY24 0x01
#define LARGE_RESOURCE_TYPE_GENERIC_REGISTER 0x02
#define LARGE_RESOURCE_TYPE_VENDOR_DEFINED 0x04
#define LARGE_RESOURCE_TYPE_MEMORY32 0x05
#define LARGE_RESOURCE_TYPE_FIXED_MEMORY32 0x06
#define LARGE_RESOURCE_TYPE_ADDRESS_SPACE32 0x07
#define LARGE_RESOURCE_TYPE_ADDRESS_SPACE16 0x08
#define LARGE_RESOURCE_TYPE_IRQ 0x09
#define LARGE_RESOURCE_TYPE_ADDRESS_SPACE64 0x0A
#define LARGE_RESOURCE_TYPE_ADDRESS_SPACE_EXTENDED 0x0B
#define LARGE_RESOURCE_TYPE_GPIO 0x0C
#define LARGE_RESOURCE_TYPE_SPB 0x0E

//
// Memory descriptor information flags.
//

#define ACPI_MEMORY_DESCRIPTOR_WRITEABLE 0x01
#define ACPI_MEMORY_DESCRIPTOR_ATTRIBUTES_MASK 0x06
#define ACPI_MEMORY_DESCRIPTOR_ATTRIBUTE_UNCACHED (0x00 << 1)
#define ACPI_MEMORY_DESCRIPTOR_ATTRIBUTE_CACHEABLE (0x01 << 1)
#define ACPI_MEMORY_DESCRIPTOR_ATTRIBUTE_WRITE_COMBINED (0x02 << 1)
#define ACPI_MEMORY_DESCRIPTOR_ATTRIBUTE_PREFETCHABLE (0x03 << 1)
#define ACPI_MEMORY_DESCRIPTOR_TYPE_MASK 0x18
#define ACPI_MEMORY_DESCRIPTOR_TYPE_MEMORY (0x00 << 3)
#define ACPI_MEMORY_DESCRIPTOR_TYPE_RESERVED (0x01 << 3)
#define ACPI_MEMORY_DESCRIPTOR_TYPE_ACPI (0x02 << 3)
#define ACPI_MEMORY_DESCRIPTOR_TYPE_NON_VOLATILE (0x03 << 3)
#define ACPI_MEMORY_DESCRIPTOR_TRANSLATES_TO_IO (1 << 5)

//
// Generic address types.
//

#define GENERIC_ADDRESS_TYPE_MEMORY 0
#define GENERIC_ADDRESS_TYPE_IO 1
#define GENERIC_ADDRESS_TYPE_BUS_NUMBER 2
#define GENERIC_ADDRESS_TYPE_VENDOR_DEFINED 192

//
// Generic address descriptor flags.
//

#define GENERIC_ADDRESS_SUBTRACTIVE_DECODE 0x02
#define GENERIC_ADDRESS_MINIMUM_FIXED 0x04
#define GENERIC_ADDRESS_MAXIMUM_FIXED 0x08

//
// Memory attribute flags.
//

#define ACPI_MEMORY_ATTRIBUTE_UNCACHED 0x1
#define ACPI_MEMORY_ATTRIBUTE_WRITE_COMBINED 0x2
#define ACPI_MEMORY_ATTRIBUTE_WRITE_THROUGH 0x4
#define ACPI_MEMORY_ATTRIBUTE_WRITE_BACK 0x8
#define ACPI_MEMORY_ATTRIBUTE_UNCACHED_EXPORTED 0x10
#define ACPI_MEMORY_ATTRIBUTE_NON_VOLATILE 0x8000

//
// Small IRQ flags.
//

#define ACPI_SMALL_IRQ_FLAG_EDGE_TRIGGERED 0x01
#define ACPI_SMALL_IRQ_FLAG_ACTIVE_LOW 0x08
#define ACPI_SMALL_IRQ_FLAG_SHAREABLE 0x10

//
// Large IRQ flags.
//

#define ACPI_LARGE_IRQ_FLAG_CONSUMER 0x01
#define ACPI_LARGE_IRQ_FLAG_EDGE_TRIGGERED 0x02
#define ACPI_LARGE_IRQ_FLAG_ACTIVE_LOW 0x04
#define ACPI_LARGE_IRQ_FLAG_SHAREABLE 0x08

//
// Small DMA flags.
//

#define ACPI_SMALL_DMA_SPEED_MASK 0x3
#define ACPI_SMALL_DMA_SPEED_SHIFT 5
#define ACPI_SMALL_DMA_SPEED_ISA (0x0 << ACPI_SMALL_DMA_SPEED_SHIFT)
#define ACPI_SMALL_DMA_SPEED_EISA_A (0x1 << ACPI_SMALL_DMA_SPEED_SHIFT)
#define ACPI_SMALL_DMA_SPEED_EISA_B (0x2 << ACPI_SMALL_DMA_SPEED_SHIFT)
#define ACPI_SMALL_DMA_SPEED_EISA_F (0x3 << ACPI_SMALL_DMA_SPEED_SHIFT)
#define ACPI_SMALL_DMA_BUS_MASTER 0x4
#define ACPI_SMALL_DMA_SIZE_MASK 0x3
#define ACPI_SMALL_DMA_SIZE_8_BIT 0x0
#define ACPI_SMALL_DMA_SIZE_8_AND_16_BIT 0x1
#define ACPI_SMALL_DMA_SIZE_16_BIT 0x2

//
// Small Fixed DMA flags.
//

#define ACPI_SMALL_FIXED_DMA_8BIT 0x00
#define ACPI_SMALL_FIXED_DMA_16BIT 0x01
#define ACPI_SMALL_FIXED_DMA_32BIT 0x02
#define ACPI_SMALL_FIXED_DMA_64BIT 0x03
#define ACPI_SMALL_FIXED_DMA_128BIT 0x04
#define ACPI_SMALL_FIXED_DMA_256BIT 0x05

//
// GPIO descriptor flags.
//

#define ACPI_GPIO_CONNECTION_INTERRUPT 0x00
#define ACPI_GPIO_CONNECTION_IO 0x01

#define ACPI_GPIO_WAKE 0x0010
#define ACPI_GPIO_SHARED 0x0008
#define ACPI_GPIO_POLARITY_MASK (0x3 << 1)
#define ACPI_GPIO_POLARITY_ACTIVE_HIGH (0x0 << 1)
#define ACPI_GPIO_POLARITY_ACTIVE_LOW (0x1 << 1)
#define ACPI_GPIO_POLARITY_ACTIVE_BOTH (0x2 << 1)
#define ACPI_GPIO_EDGE_TRIGGERED 0x0001
#define ACPI_GPIO_IO_RESTRICTION_MASK 0x0003
#define ACPI_GPIO_IO_RESTRICTION_IO 0x0000
#define ACPI_GPIO_IO_RESTRICTION_INPUT 0x0001
#define ACPI_GPIO_IO_RESTRICTION_OUTPUT 0x0002
#define ACPI_GPIO_IO_RESTRICTION_IO_PRESERVE 0x0003

#define ACPI_GPIO_PIN_PULL_DEFAULT 0x00
#define ACPI_GPIO_PIN_PULL_UP 0x01
#define ACPI_GPIO_PIN_PULL_DOWN 0x02
#define ACPI_GPIO_PIN_PULL_NONE 0x03

#define ACPI_GPIO_OUTPUT_DRIVE_DEFAULT 0xFFFF
#define ACPI_GPIO_DEBOUNCE_TIMEOUT_DEFAULT 0xFFFF

//
// Simple Peripheral Bus descriptor definitions.
//

#define ACPI_SPB_BUS_I2C 1
#define ACPI_SPB_BUS_SPI 2
#define ACPI_SPB_BUS_UART 3

#define ACPI_SPB_I2C_TYPE_DATA_LENGTH 6
#define ACPI_SPB_SPI_TYPE_DATA_LENGTH 9
#define ACPI_SPB_UART_TYPE_DATA_LENGTH 10

#define ACPI_SPB_FLAG_SLAVE 0x01

#define ACPI_SPB_I2C_10_BIT_ADDRESSING 0x0001

#define ACPI_SPB_SPI_3_WIRES 0x0001
#define ACPI_SPB_SPI_DEVICE_SELECT_ACTIVE_HIGH 0x0002

#define ACPI_SPB_SPI_PHASE_FIRST 0
#define ACPI_SPB_SPI_PHASE_SECOND 1
#define ACPI_SPB_SPI_POLARITY_START_LOW 0
#define ACPI_SPB_SPI_POLARITY_START_HIGH 1

#define ACPI_SPB_UART_FLOW_CONTROL_NONE 0x00
#define ACPI_SPB_UART_FLOW_CONTROL_HARDWARE 0x01
#define ACPI_SPB_UART_FLOW_CONTROL_SOFTWARE 0x02
#define ACPI_SPB_UART_FLOW_CONTROL_MASK 0x03

#define ACPI_SPB_UART_STOP_BITS_NONE (0x0 << 2)
#define ACPI_SPB_UART_STOP_BITS_1 (0x1 << 2)
#define ACPI_SPB_UART_STOP_BITS_1_5 (0x2 << 2)
#define ACPI_SPB_UART_STOP_BITS_2 (0x3 << 2)
#define ACPI_SPB_UART_STOP_BITS_MASK (0x3 << 2)

#define ACPI_SPB_UART_DATA_BITS_5 (0x0 << 4)
#define ACPI_SPB_UART_DATA_BITS_6 (0x1 << 4)
#define ACPI_SPB_UART_DATA_BITS_7 (0x2 << 4)
#define ACPI_SPB_UART_DATA_BITS_8 (0x3 << 4)
#define ACPI_SPB_UART_DATA_BITS_9 (0x4 << 4)
#define ACPI_SPB_UART_DATA_BITS_MASK (0x7 << 4)
#define ACPI_SPB_UART_DATA_BITS_SHIFT 4

#define ACPI_SPB_UART_BIG_ENDIAN 0x0080

#define ACPI_SPB_UART_PARITY_NONE 0x00
#define ACPI_SPB_UART_PARITY_EVEN 0x01
#define ACPI_SPB_UART_PARITY_ODD 0x02
#define ACPI_SPB_UART_PARITY_MARK 0x03
#define ACPI_SPB_UART_PARITY_SPACE 0x04

#define ACPI_SPB_UART_CONTROL_DTD (1 << 2)
#define ACPI_SPB_UART_CONTROL_RI (1 << 3)
#define ACPI_SPB_UART_CONTROL_DSR (1 << 4)
#define ACPI_SPB_UART_CONTROL_DTR (1 << 5)
#define ACPI_SPB_UART_CONTROL_CTS (1 << 6)
#define ACPI_SPB_UART_CONTROL_RTS (1 << 7)

//
// Define the meaning of bits coming back from the _STA AML method.
//

#define ACPI_DEVICE_STATUS_PRESENT 0x00000001
#define ACPI_DEVICE_STATUS_ENABLED 0x00000002
#define ACPI_DEVICE_STATUS_SHOW_IN_UI 0x00000004
#define ACPI_DEVICE_STATUS_FUNCTIONING_PROPERLY 0x00000008
#define ACPI_DEVICE_STATUS_BATTERY_PRESENT 0x00000010

//
// Define the default status flags if no _STA method is found.
//

#define ACPI_DEFAULT_DEVICE_STATUS             \
    (ACPI_DEVICE_STATUS_PRESENT |              \
     ACPI_DEVICE_STATUS_ENABLED |              \
     ACPI_DEVICE_STATUS_SHOW_IN_UI |           \
     ACPI_DEVICE_STATUS_FUNCTIONING_PROPERLY | \
     ACPI_DEVICE_STATUS_BATTERY_PRESENT)

//
// Define debug port table 2 types.
//

#define DEBUG_PORT_TYPE_SERIAL  0x8000
#define DEBUG_PORT_TYPE_1394    0x8001
#define DEBUG_PORT_TYPE_USB     0x8002
#define DEBUG_PORT_TYPE_NET     0x8003

//
// Debug port table 2 sub-types.
//

#define DEBUG_PORT_SERIAL_16550 0x0000
#define DEBUG_PORT_SERIAL_16550_COMPATIBLE 0x0001
#define DEBUG_PORT_SERIAL_ARM_PL011 0x0003
#define DEBUG_PORT_SERIAL_ARM_OMAP4 0x0004

#define DEBUG_PORT_1394_STANDARD 0x0000

#define DEBUG_PORT_USB_XHCI 0x0000
#define DEBUG_PORT_USB_EHCI 0x0001

//
// Define the signature for optional 16550 UART OEM data. The string "165U".
//

#define DEBUG_PORT_16550_OEM_DATA_SIGNATURE 0x55353631

//
// Define the set of flags for the optional 16550 UART OEM data.
//

#define DEBUG_PORT_16550_OEM_FLAG_64_BYTE_FIFO                  0x00000001
#define DEBUG_PORT_16550_OEM_FLAG_TRANSMIT_TRIGGER_2_CHARACTERS 0x00000002

//
// Define Intel-specific fixed function hardware register flags and bitfields.
//

#define ACPI_FIXED_HARDWARE_INTEL 0x01

#define ACPI_FIXED_HARDWARE_INTEL_CST_HALT 0x00
#define ACPI_FIXED_HARDWARE_INTEL_CST_IO_HALT 0x01
#define ACPI_FIXED_HARDWARE_INTEL_CST_MWAIT 0x02

#define ACPI_INTEL_MWAIT_HARDWARE_COORDINATED 0x01
#define ACPI_INTEL_MWAIT_BUS_MASTER_AVOIDANCE 0x02

//
// Define Intel-specific _OSC and _PDC bits.
//

#define ACPI_OSC_INTEL_UUID {{0x4077A616, 0x47BE290C, 0x70D8BD9E, 0x53397158}}

#define ACPI_OSC_INTEL_PSTATE_MSRS (1 << 0)
#define ACPI_OSC_INTEL_SMP_C1_IO_HALT (1 << 1)
#define ACPI_OSC_INTEL_THROTTLING_MSRS (1 << 2)
#define ACPI_OSC_INTEL_SMP_INDEPENDENT (1 << 3)
#define ACPI_OSC_INTEL_C2_C3_SMP_INDEPENDENT (1 << 4)
#define ACPI_OSC_INTEL_SMP_PSTATE_PSD (1 << 5)
#define ACPI_OSC_INTEL_SMP_CSTATE_CST (1 << 6)
#define ACPI_OSC_INTEL_SMP_TSTATE_TSD (1 << 7)
#define ACPI_OSC_INTEL_SMP_C1_NATIVE (1 << 8)
#define ACPI_OSC_INTEL_SMP_C2_C3_NATIVE (1 << 9)
#define ACPI_OSC_INTEL_PSTATE_ACNT_MCNT (1 << 11)
#define ACPI_OSC_INTEL_PSTATE_COLLABORATIVE (1 << 12)
#define ACPI_OSC_INTEL_HARDWARE_DUTY_CYCLING (1 << 13)

//
// Define the generic timer global flags.
//

#define GTDT_GLOBAL_FLAG_MEMORY_MAPPED_BLOCK_PRESENT 0x00000001
#define GTDT_GLOBAL_FLAG_INTERRUPT_MODE_MASK         0x00000002
#define GTDT_GLOBAL_FLAG_INTERRUPT_MODE_EDGE         0x00000002
#define GTDT_GLOBAL_FLAG_INTERRUPT_MODE_LEVEL        0x00000000

//
// Define the generic timer flags.
//

#define GTDT_TIMER_FLAG_INTERRUPT_MODE_MASK            0x00000001
#define GTDT_TIMER_FLAG_INTERRUPT_MODE_EDGE            0x00000001
#define GTDT_TIMER_FLAG_INTERRUPT_MODE_LEVEL           0x00000000
#define GTDT_TIMER_FLAG_INTERRUPT_POLARITY_MASK        0x00000002
#define GTDT_TIMER_FLAG_INTERRUPT_POLARITY_ACTIVE_LOW  0x00000002
#define GTDT_TIMER_FLAG_INTERRUPT_POLARITY_ACTIVE_HIGH 0x00000000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes platform register locations. It is used to express
    register addresses within tables defined by ACPI.

Members:

    AddressSpaceId - Stores type ADDRESS_SPACE_TYPE defining where the data
        structure or register exists.

    RegisterBitWidth - Stores the size in bits of the given register. When
        addressing a data structure, this field must be zero.

    AccessSize - Stores the size in bytes of the access. 0 is undefined, 1 for
        byte access, 2 for word access, 3 for double-word access, and 4 for
        quad-word access.

    Address - Stores the 64-bit address of the data structure or register in
        the given address space (relative to the processor).

--*/

typedef struct _GENERIC_ADDRESS {
    UCHAR AddressSpaceId;
    UCHAR RegisterBitWidth;
    UCHAR RegisterBitOffset;
    UCHAR AccessSize;
    ULONGLONG Address;
} PACKED GENERIC_ADDRESS, *PGENERIC_ADDRESS;

/*++

Structure Description:

    This structure describes the Root System Description Pointer. This is used
    to locate the Root System Description Table or the Extended Root System
    Description Table. According to ACPI, this structure can be found on PC/AT
    systems by searching the 1 KB of the Extended BIOS data area, or the ROM
    space between 0xE0000 and 0xFFFFF.

Members:

    Signature - Stores "RSD PTR ".

    Checksum - Stores a value such that the sum of the first 20 bytes of this
        structure including the checksum sum to zero.

    OemId - Stores an OEM-supplied string that identifies the OEM.

    Revision - Stores the revision number of the structure. As of ACPI 3.0b,
        the revision number is 2.

    RsdtAddress - Stores the 32-bit physical address of the RSDT.

    Length - Stores the length of the table, in bytes.

    XsdtAddress - Stores the 64-bit physical address of the XSDT.

    ExtendedChecksum - Stores the checksum of the entire table, including both
        checksum fields.

    Reserved - These fields are reserved.

--*/

typedef struct _RSDP {
    ULONGLONG Signature;
    UCHAR Checksum;
    UCHAR OemId[6];
    UCHAR Revision;
    ULONG RsdtAddress;
    ULONG Length;
    ULONGLONG XsdtAddress;
    UCHAR ExtendedChecksum;
    UCHAR Reserved[3];
} PACKED RSDP, *PRSDP;

/*++

Structure Description:

    This structure describes the beginning of all system description tables.
    The signature field determines the content of the system description table.

Members:

    Signature - Stores the ASCII string representation of the table identifier.

    Length - Store the length of the table, in bytes, including the header.

    Revision - Stores the revision of the structure corresponding to the
        signature field for this table. Larger revision numbers are backwards
        compatible with lower revision numbers of the same signature.

    Checksum - Stores a byte such that the entire table, including the checksum
        field, must add to zero to be considered valid.

    OemId - Stores an OEM-supplied string that identifies the OEM.

    OemTableId - Stores an OEM-supplied string that the OEM uses to identify
        the particular data table. This field is particularly useful when
        defining a definition block to distinguish definition block functions.
        The OEM assigns each dissimilar table a new OEM Table ID.

    OemRevision - Stores the OEM-supplied revision number. Larger numbers are
        assumed to be newer revisions.

    CreatorId - Stores the Vendor ID of the utility that created the table. For
        tables containing Definition Blocks, this is the ID for the ASL
        compiler.

    CreatorRevision - Stores the revision of the utility that created the table.
        For tables containing Definition Blocks, this is the revision of the
        ASL compiler.

--*/

typedef struct _DESCRIPTION_HEADER {
    ULONG Signature;
    ULONG Length;
    UCHAR Revision;
    UCHAR Checksum;
    UCHAR OemId[6];
    ULONGLONG OemTableId;
    ULONG OemRevision;
    ULONG CreatorId;
    ULONG CreatorRevision;
} PACKED DESCRIPTION_HEADER, *PDESCRIPTION_HEADER;

/*++

Structure Description:

    This structure describes the Root System Description Table. The table
    provides a list of pointers to other tables. The length field of the header
    implies how many entries exist in the table.

Members:

    Header - Store the table header, including the signature 'RSDT'.

    Entries - Stores the list of 32-bit physical pointers to other ACPI tables.

--*/

typedef struct _RSDT {
    DESCRIPTION_HEADER Header;
    ULONG Entries[ANYSIZE_ARRAY];
} PACKED RSDT, *PRSDT;

/*++

Structure Description:

    This structure describes the Extended System Description Table. The table
    provides a list of pointers to other tables. The length field of the header
    implies how many entries exist in the table. This table provides identical
    functionality to the RSDT, but uses 64-bit addresses.

Members:

    Header - Store the table header, including the signature 'XSDT'.

    Entries - Stores the list of 64-bit physical pointers to other ACPI tables.

--*/

typedef struct _XSDT {
    DESCRIPTION_HEADER Header;
    ULONGLONG Entries[ANYSIZE_ARRAY];
} PACKED XSDT, *PXSDT;

/*++

Structure Description:

    This structure describes the Fixed ACPI Description Table, which defines
    various fixed hardware ACPI information vital to an ACPI-compatible OS. The
    FADT also has a pointer to the DSDT that contains the Differentiated
    Definition Block, which provides base system design information.

Members:

    Header - Store the table header, including the signature 'FACP'.

    FirmwareControlAddress - Stores the physical memory address of the FACS,
        where OSPM and firmware exchange control information.

    DsdtAddress - Stores the physical address of the DSDT.

    Reserved1 - This field is reserved.

    PreferredPowerProfile - Stores the preferred power managment profile, used
        to set default power policy during OS installation. Valid values are:

        0 - Unspecified
        1 - Desktop
        2 - Mobile
        3 - Workstation
        4 - Enterprise Server
        5 - SOHO Server
        6 - Appliance PC
        7 - Performance Server

    SciVector - Stores the system vector the SCI interrupt is wired to in
        legacy 8259 mode. On systems that do not contain the 8259, this field
        contains the Global System Interrupt number of the SCI interrupt. OSPM
        is required to treat the ACPI SCI interrupt as a sharable, level,
        active low interrupt.

    SciCommandPort - Stores the system port address of the SMI command port.

    AcpiEnable - Stores the value to write to the SMI command port to disable
        SMI ownership of the ACPI hardware registers. The OS should see the
        SCI_EN bit flip on when the firmware has fully relinquished control of
        the hardware registers.

    AcpiDisable - Stores the value to write to the SMI command port to re-enable
        SMI ownership of the ACPI hardware registers.

    S4BiosRequest - Stores the value to write to the SMI command port to enter
        the S4BIOS state. This is an alternate way to enter the S4 state where
        the firmware saves and restores the memory context. A value of 0 means
        not supported.

    PStateControl - Stores the value to write to the SMI command register to
        assume processor performance state control responsibility.

    Pm1aEventBlock - Stores the system port address of the PM1a Event Register
        Block.

    Pm1bEventBlock - Stores the system port address of the PM1b Event Register
        Block.

    Pm1aControlBlock - Stores the system port address of the PM1a Control
        Register Block.

    Pm1bControlBlock - Stores the system port address of the PM1b Control
        Register Block.

    Pm2ControlBlock - Stores the system port address of the PM2 Control
        Register Block. This field is optional.

    PmTimerBlock - Stores the system port address of the Power Management
        Timer Control Register Block.

    Gpe0Block - Stores the system port address of the General Purpose Event 0
        Register Block. Zero indicates not supported.

    Gpe1Block - Stores the system port address of the General Purpose Event 1
        Register Block. Zero indicates not supported.

    Pm1EventLength - Stores the number of bytes decoded by the PM1a and PM1b
        Event Blocks. This value is >= 4.

    Pm1ControlLength - Stores the number of bytes decoded by the PM1a and PM1b
        Control Blocks. This value is >= 2.

    Pm2ControlLength - Stores the number of bytes decoded by the PM2 Control
        Block. If supported, this value is >= 1. If not supported, this field
        is 0.

    PmTimerLength - Stores the number of bytes decoded by the PM Timer block.
        This field's value must be 4.

    Gpe0BlockLength - Stores the number of bytes decoded by the GPE0 Block. This
        value is a non-negative multiple of 2.

    Gpe1BlockLength - Stores the number of bytes decoded by the GPE1 Block. This
        value is a non-negative multiple of 2.

    Gpe1Base - Stores the offset within the ACPI general purpose event model
        where GPE1 based events start.

    CstControl - Stores the value to write to the SMI command port to indicate
        OS support for the _CST object and C States Changed notification.

    C2Latency - Stores the worst-case latency, in microseconds, to enter and
        exit a C2 state. A value > 100 indicates that the system does not
        support C2.

    C3Latency - Stores the worst-case latency, in microseconds, to enter and
        exit a C3 state. A value > 1000 indicates that the system does not
        support C3.

    FlushSize - Stores the number of flush strides that need to be read (using
        cacheable addresses) to completely flush dirty lines from any
        processor's memory caches. This field is maintained for ACPI 1.0
        compatibility, newer processors set WBINVD=1 and the OS is expected
        to flush caches that way.

    FlushStride - Stores the cache line width, in bytes, of the processor's
        memory caches. This field is ignored if WBINVD=1, and is maintained for
        ACPI 1.0 compatibility.

    DutyOffset - Stores the zero-based index of where the processor's duty
        cycle setting is within the processor's P_CNT register.

    DutyWidth - Stores the bit width of the processor's duty cycle setting in
        the P_CNT register. Each processor's duty cycle setting allows the
        software to slect a nominal processor frequency below its absolute
        frequency as defined by (BaseFrequency * DutyCycle) / (2^DutyWidth).

    DayAlarm - Stores the CMOS RAM index to the day-of-month alarm value.

    MonthAlarm - Stores the CMOS RAM index to the month-of-year alarm value.

    Century - Stores the CMOS RAM index to the century of data value.

    IaBootFlags - Stores the IA-PC Boot ArchitectureFlags.

    Reserved2 - This field is reserved.

    Flags - Stores the fixed feature flags.

    ResetRegister - Stores the address of the Reset Register. Only System I/O
        Space, System Memory space, and PCI Configuration Space (Bus 0) are
        valid. RegisterBitWidth must be 8 and RegisterBitOffset must be 0.

    ResetValue - Stores the value to write to the Reset Register port to reset
        the system.

    Reserved3 - This field is reserved.

    XFirmwareControl - Stores the 64-bit address of the FACS.

    XDsdt - Stores the 64-bit address of the DSDT.

    XPm1aEventBlock - Stores the address of the PM1a Event Register Block. This
        supercedes the original Pm1aEventBlock field.

    XPm1bEventBlock - Stores the address of the PM1b Event Register Block. This
        supercedes the original Pm1bEventBlock field.

    XPm1aControlBlock - Stores the address of the PM1a Control Register Block.
        This supercedes the original Pm1aControlBlock field.

    XPm1bControlBlock - Stores the address of the PM1b Control Register Block.
        This supercedes the original Pm1bControlBlock field.

    XPm2ControlBlock - Stores the address of the PM2 Control Register Block.
        This supercedes the original Pm2ControlBlock field.

    XPmTimerBlock - Stores the address of the PM Timer Control Register Block.
        This supercedes the original PmTimerBlock.

    XGpe0Block - Stores the address of the General Purpose Event 0 Register
        Block. This supercedes the original Gpe0Block.

    XGpe1Block - Stores the address of the General Purpose Event 1 Register
        Block. This supercedes the original Gpe1Block.

--*/

typedef struct _FADT {
    DESCRIPTION_HEADER Header;
    ULONG FirmwareControlAddress;
    ULONG DsdtAddress;
    UCHAR Reserved1;
    UCHAR PreferredPowerProfile;
    USHORT SciVector;
    ULONG SmiCommandPort;
    UCHAR AcpiEnable;
    UCHAR AcpiDisable;
    UCHAR S4BiosRequest;
    UCHAR PStateControl;
    ULONG Pm1aEventBlock;
    ULONG Pm1bEventBlock;
    ULONG Pm1aControlBlock;
    ULONG Pm1bControlBlock;
    ULONG Pm2ControlBlock;
    ULONG PmTimerBlock;
    ULONG Gpe0Block;
    ULONG Gpe1Block;
    UCHAR Pm1EventLength;
    UCHAR Pm1ControlLength;
    UCHAR Pm2ControlLength;
    UCHAR PmTimerLength;
    UCHAR Gpe0BlockLength;
    UCHAR Gpe1BlockLength;
    UCHAR Gpe1Base;
    UCHAR CstControl;
    USHORT C2Latency;
    USHORT C3Latency;
    USHORT FlushSize;
    USHORT FlushStride;
    UCHAR DutyOffset;
    UCHAR DutyWidth;
    UCHAR DayAlarm;
    UCHAR MonthAlarm;
    UCHAR Century;
    USHORT IaBootFlags;
    UCHAR Reserved2;
    ULONG Flags;
    GENERIC_ADDRESS ResetRegister;
    UCHAR ResetValue;
    UCHAR Reserved3[3];
    ULONGLONG XFirmwareControl;
    ULONGLONG XDsdt;
    GENERIC_ADDRESS XPm1aEventBlock;
    GENERIC_ADDRESS XPm1bEventBlock;
    GENERIC_ADDRESS XPm1aControlBlock;
    GENERIC_ADDRESS XPm1bControlBlock;
    GENERIC_ADDRESS XPm2ControlBlock;
    GENERIC_ADDRESS XPmTimerBlock;
    GENERIC_ADDRESS XGpe0Block;
    GENERIC_ADDRESS XGpe1Block;
} PACKED FADT, *PFADT;

/*++

Structure Description:

    This structure describes the Firmware ACPI Control Structure.

Members:

    Signature - Stores the four byte signature of this table, 'FACS'.

    Length - Stores the complete length of the structure.

    HardwareSignature - Stores the value of the system's "hardware signature"
        at last boot. This value is calulated by the BIOS on a best effort
        basis to indicate the base hardware configuration of the system. The
        OSPM uses this information when waking from an S4 state by comparing
        this signature to the one seen on boot to determine if the hardware
        configuration has changed while the system was in S4.

    FirmwareWakingVector - Stores a value superceded by the
        XFirmwareWakingVector field. Before transitioning the system into a
        global sleeping state, the OSPM fills in this field with the physical
        memory address of an OS-specific wake function. When waking up, the
        BIOS jumps to this address. On PC platforms, the address is in memory
        below 1MB and the address is jumped to in real mode. If the address
        were 0x12345, the real mode address jumped to would be CS:IP =
        0x1234:0x0005. A20 will not have been enabled.

    GlobalLock - Stores the global lock used to synchronize access to the
        shared hardware resources between the OSPM and external firmware.
        See FACS_GLOBAL_LOCK_* definitions.

    Flags - Stores a bitfield of flags. See FACS_FLAG_* definitions.

    XFirmwareWakingVector - Stores the 64-bit physical address of the OSPM's
        waking vector. Before transitioning the system into a global sleeping
        state, the OSPM fills in this field with the physical memory address of
        an OS-specific wake function. When waking up, the BIOS jumps to this
        address in either 32-bit or 64-bit mode. If the platform supports
        64-bit mode, firmware inspects the OSPM flags during POST. If the
        64BIT_WAKE_F flag is set, the platform firmware creates a 64-bit
        execution environment. Otherwise, the platform creates a 32-bit
        execution environment. For a 64-bit execution environment, interrupts
        must be disabled (EFLAGS.IF is zero), long mode is enabled, paging
        mode is enabled and physical memory for the waking vector is identity
        mapped (to a single page), and selectors are set to flat. For a 32-bit
        execution environment, interrupts are also disabled, memory address
        translation is disabled, and the segment registers are set flat.

    Version - Stores the value 2, the current version of this table.

    Reserved - Stores some padding bytes used for alignment.

    OspmFlags - Stores OSPM-enabled firmware control flags. Platform firmware
        initializes this to zero. See FACS_OSPM_FLAG_* definitions.

--*/

typedef struct _FACS {
    ULONG Signature;
    ULONG Length;
    ULONGLONG HardwareSignature;
    ULONG FirmwareWakingVector;
    ULONG GlobalLock;
    ULONGLONG XFirmwareWakingVector;
    UCHAR Version;
    UCHAR Reserved[3];
    ULONG OspmFlags;
} PACKED FACS, *PFACS;

/*++

Structure Description:

    This structure describes the interrupt model information for systems with
    an APIC or SAPIC implementation.

Members:

    Header - Stores the table header, including the signature, 'APIC'.

    ApicAddress - Stores the 32-bit physical address at which each processor
        can access its local APIC.

    Flags - Stores APIC flags. The only flag currently defined is bit 0, which
        indicates that the system is a dual 8259 compatible PC.

    ApicStructures - Stores a list of APIC structures describing local APICs,
        IOAPICs, NMI sources, etc.

--*/

typedef struct _MADT {
    DESCRIPTION_HEADER Header;
    ULONG ApicAddress;
    ULONG Flags;
    // ApicStructures[n].
} PACKED MADT, *PMADT;

/*++

Structure Description:

    This structure describes an entry in the MADT whose content is not yet
    fully known.

Members:

    Type - Stores the type of entry, used to differentiate the various types
        of entries.

    Length - Stores the size of the entry, in bytes.

--*/

typedef struct _MADT_GENERIC_ENTRY {
    UCHAR Type;
    UCHAR Length;
} PACKED MADT_GENERIC_ENTRY, *PMADT_GENERIC_ENTRY;

/*++

Structure Description:

    This structure describes a local APIC unit in the MADT.

Members:

    Type - Stores 0 to indicate a Processor Local APIC structure.

    Length - Stores 8, the size of this structure.

    AcpiProcessorId - Stores the Processor ID for which this processor is listed
        in the ACPI Processor declaration operator.

    ApicId - Stores the processor's local APIC ID.

    Flags - Stores flags governing this APIC. See MADT_LOCAL_APIC_FLAG_*.

--*/

typedef struct _MADT_LOCAL_APIC {
    UCHAR Type;
    UCHAR Length;
    UCHAR AcpiProcessorId;
    UCHAR ApicId;
    ULONG Flags;
} PACKED MADT_LOCAL_APIC, *PMADT_LOCAL_APIC;

/*++

Structure Description:

    This structure describes an IO APIC in the MADT.

Members:

    Type - Stores 1 to indicate that this is an IOAPIC description.

    Length - Stores 12, the size of this structure.

    IoApicId - Stores the IO APIC's ID.

    Reserved - This field is reserved.

    IoApicAddress - Stores the unique 32-bit physical address to access this
        IO APIC. Each IO APIC resides at a unique address.

    GsiBase - Stores the Global System Interrupt number where this IO APIC's
        interrupt inputs start. The number of interrupts is determined by the
        IO APIC's MaxRedirEntry register.

--*/

typedef struct _MADT_IO_APIC {
    UCHAR Type;
    UCHAR Length;
    UCHAR IoApicId;
    UCHAR Reserved;
    ULONG IoApicAddress;
    ULONG GsiBase;
} PACKED MADT_IO_APIC, *PMADT_IO_APIC;

/*++

Structure Description:

    This structure describes a local APIC unit in the MADT.

Members:

    Type - Stores 2 to indicate an Interrupt Override structure.

    Length - Stores 10, the size of this structure.

    Bus - Stores the bus type, which is always 0 for ISA.

    Irq - Stores the source 8259 PIC interrupt number being altered. Valid
        values are 0 through 15.

    Gsi - Stores the Global System Interrupt number corresponding to the IRQ
        number.

    Flags - Stores a bitfield of flags. See MADT_INTERRUPT_* definitions.

--*/

typedef struct _MADT_INTERRUPT_OVERRIDE {
    UCHAR Type;
    UCHAR Length;
    UCHAR Bus;
    UCHAR Irq;
    ULONG Gsi;
    USHORT Flags;
} PACKED MADT_INTERRUPT_OVERRIDE, *PMADT_INTERRUPT_OVERRIDE;

/*++

Structure Description:

    This structure describes a GIC CPU interface unit in the MADT.

Members:

    Type - Stores a value to indicate a Processor GIC CPU interface structure
        (0xB).

    Length - Stores the size of this structure, 40.

    Reserved - Stores a reserved value which must be zero.

    GicId - Store the local GIC's hardware ID.

    AcpiProcessorId - Stores the Processor ID for which this processor is listed
        in the ACPI Processor declaration operator.

    Flags - Stores flags governing this GIC CPU interface. See
        MADT_LOCAL_GIC_FLAG_*.

    ParkingProtocolVersion - Stores the version of the ARM processor parking
        protocol implemented.

    PerformanceInterruptGsi - Stores the GSI of the performance interrupt.

    ParkedAddress - Stores the physical address of the processor's parking
        protocol mailbox.

    BaseAddress - Stores the physical address of the GIC CPU interface. If the
        "local interrupt controller address" field is provided, this field is
        ignored.

--*/

typedef struct _MADT_GIC {
    UCHAR Type;
    UCHAR Length;
    USHORT Reserved;
    ULONG GicId;
    ULONG AcpiProcessorId;
    ULONG Flags;
    ULONG ParkingProtocolVersion;
    ULONG PerformanceInterruptGsi;
    ULONGLONG ParkedAddress;
    ULONGLONG BaseAddress;
} PACKED MADT_GIC, *PMADT_GIC;

/*++

Structure Description:

    This structure describes a GIC distributor unit.

Members:

    Type - Stores 0xC to indicate that this is a GIC distributor description.

    Length - Stores 24, the size of this structure.

    Reserved - Stores a reserved field that must be zero.

    GicId - Stores the hardware ID of the GIC distributor unit.

    BaseAddress - Stores the physical address of the distributor base.

    GsiBase - Stores the Global System Interrupt number where this IO APIC's
        interrupt inputs start. The number of interrupts is determined by the
        IO APIC's MaxRedirEntry register.

    Reserved2 - Stores another reserved value that must be zero.

--*/

typedef struct _MADT_GIC_DISTRIBUTOR {
    UCHAR Type;
    UCHAR Length;
    USHORT Reserved;
    ULONG GicId;
    ULONGLONG BaseAddress;
    ULONG GsiBase;
    ULONG Reserved2;
} PACKED MADT_GIC_DISTRIBUTOR, *PMADT_GIC_DISTRIBUTOR;

/*++

Structure Description:

    This structure describes the debug port table, revision 2.

Members:

    Header - Stores the standard ACPI table header.

    DeviceInformationOffset - Stores the offset in bytes from the beginning of
        the table to the beginning of the device information structure.

    DeviceInformationCount - Stores the number of device information structures
        that are in the array starting at the device information offset.

--*/

typedef struct _DEBUG_PORT_TABLE2 {
    DESCRIPTION_HEADER Header;
    ULONG DeviceInformationOffset;
    ULONG DeviceInformationCount;
} PACKED DEBUG_PORT_TABLE2, *PDEBUG_PORT_TABLE2;

/*++

Structure Description:

    This structure describes the debug device information contained within
    the debug port table, revision 2. Following this structure is an array of
    generic addresses, an array of sizes for each generic address, an ASCII
    ACPI namespace string, and OEM-specific data.

Members:

    Revision - Stores the revision of the structure, currently 0.

    Length - Stores the length of this structure including the namespace string
        and OEM data.

    GenericAddressCount - Stores the number of generic address registers in
        the array that follows this structure.

    NamespaceStringLength - Stores the length of the ASCII null-terminated
        string identifying the device in the ACPI namespace.

    NamespaceStringOffset - Stores the offset in bytes from the beginning of
        this structure to the namespace string.

    OemDataLength - Stores the length of the OEM data.

    OemDataOffset - Stores the offset in bytes from the beginning of this
        structure to the OEM data.

    PortType - Stores the debug port type. See DEBUG_PORT_TYPE_* definitions.

    PortSubType - Stores the port sub-type. See DEBUG_PORT_* definitions.

    Reserved - Stores a reserved value that must be zero.

    BaseAddressRegisterOffset - Stores the offset in bytes from the beginning
        of this structure to the array of generic address structures.

    AddressSizeOffset - Stores the offset in bytes from the beginning of this
        structure to the array of sizes that correspond to each generic
        address structure.

--*/

typedef struct _DEBUG_DEVICE_INFORMATION {
    UCHAR Revision;
    USHORT Length;
    UCHAR GenericAddressCount;
    USHORT NamespaceStringLength;
    USHORT NamespaceStringOffset;
    USHORT OemDataLength;
    USHORT OemDataOffset;
    USHORT PortType;
    USHORT PortSubType;
    USHORT Reserved;
    USHORT BaseAddressRegisterOffset;
    USHORT AddressSizeOffset;
} PACKED DEBUG_DEVICE_INFORMATION, *PDEBUG_DEVICE_INFORMATION;

/*++

Structure Description:

    This structure describes the debug port table, revision 2.

Members:

    Signature - Stores a constant signature used for verification of the
        contents of the structure. Set to DEBUG_PORT_16550_OEM_DATA_SIGNATURE.

    BaseBaud - Stores the baud rate for a divisor of 1.

    RegisterOffset - Stores the offset from the base of the region where the
        16550-compatible registers start.

    RegisterShift - Stores the amount to shift the standard 16550 register
        numbers by to get correct offsets.

    Flags - Stores a bitmask of flags for the device. See
        DEBUG_PORT_16550_OEM_FLAG_* for definitions.

--*/

typedef struct _DEBUG_PORT_16550_OEM_DATA {
    ULONG Signature;
    ULONG BaseBaud;
    USHORT RegisterOffset;
    USHORT RegisterShift;
    ULONG Flags;
} PACKED DEBUG_PORT_16550_OEM_DATA, *PDEBUG_PORT_16550_OEM_DATA;

/*++

Structure Description:

    This structure defines the system's Generic Timer information.

Members:

    Header - Stores the table header, including the signature, 'GTDT'.

    CounterBlockAddress - Stores the physical address of the counter block.

    GlobalFlags - Stores a bitmask of global GTDT flags. See GTDT_GLOBAL_FLAG_*
        for definitions.

    SecurePl1Gsi - Stores the optional GSI of the secure PL1 physical timer.
        Stores 0 if not provided.

    SecurePl1Flags - Stores a bitmask of timer flags. See GTDT_TIMER_FLAG_* for
        definitions.

    NonSecurePl1Gsi - Stores the GSI of the non-secure PL1 physical timer.

    NonSecurePl1Flags - Stores a bitmask of timer flags. See GTDT_TIMER_FLAG_*
        for definitions.

    VirtualTimerGsi - Stores the GSI of the virtual timer.

    VirtualTimerFlags - Stores a bitmask of timer flags. See GTDT_TIMER_FLAG_*
        for definitions.

    NonSecurePl2Gsi - Stores the GSI of the non-secure PL2 physical timer.

    NonSecurePl2Flags - Stores a bitmask of timer flags. See GTDT_TIMER_FLAG_*
        for definitions.

--*/

typedef struct _GTDT {
    DESCRIPTION_HEADER Header;
    ULONGLONG CounterBlockAddress;
    ULONG GlobalFlags;
    ULONG SecurePl1Gsi;
    ULONG SecurePl1Flags;
    ULONG NonSecurePl1Gsi;
    ULONG NonSecurePl1Flags;
    ULONG VirtualTimerGsi;
    ULONG VirtualTimerFlags;
    ULONG NonSecurePl2Gsi;
    ULONG NonSecurePl2Flags;
} PACKED GTDT, *PGTDT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
