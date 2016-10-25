/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwhcihw.h

Abstract:

    This header contains DesignWare Hi-Speed USB 2.0 On-The-Go (HS OTG)
    hardware definitions.

    Copyright (C) 2004-2013 by Synopsis, Inc.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions, and the following disclaimer, without
       modification.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    3. The names of the above-listed copyright holders may not be used to
       endorse or promote products derived from this software without specific
       prior written permission.

    This software is provided by the copyright holders and contributors "AS IS"
    and any express or implied warranties, including, by not limited to, the
    implied warranties or mechantability and fitness for a particular purpose
    are disclained. In no event shall the copyright owner or contributors be
    liable for any direct, indirect, incidental, special, exemplary, or
    consequential damages (including, but not limited to, procurement of
    substitue goods or services; loss of use, data, or profits; or business
    interruption) however caused and on any theory of liability, whether in
    contract, strict liability, or tort (including negligence or otherwise)
    arising in any way out of the use of this software, even if advised of the
    possibility of such damage.

Author:

    Chris Stevens 27-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of host ports.
//

#define DWHCI_HOST_PORT_COUNT 1

//
// Define the flags for the OTG control register.
//

#define DWHCI_OTG_CONTROL_OTG_VERSION         (1 << 20)
#define DWHCI_OTG_CONTROL_HOST_SET_HNP_ENABLE (1 << 10)

//
// Define the flags for the AHB configuration register.
//

#define DWHCI_AHB_CONFIGURATION_DMA_REMAINDER_MODE_INCREMENTAL (0x0 << 23)
#define DWHCI_AHB_CONFIGURATION_DMA_REMAINDER_MODE_SINGLE      (0x1 << 23)
#define DWHCI_AHB_CONFIGURATION_DMA_REMAINDER_MODE_MASK        (0x1 << 23)
#define DWHCI_AHB_CONFIGURATION_DMA_REMAINDER_MODE_SHIFT       23
#define DWHCI_AHB_CONFIGURATION_NOTIFY_ALL_DMA_WRITES          (1 << 22)
#define DWHCI_AHB_CONFIGURATION_REMOTE_MEMORY_SUPPORTED        (1 << 21)
#define DWHCI_AHB_CONFIGURATION_PERIODIC_TRANSFER_EMPTY        (1 << 8)
#define DWHCI_AHB_CONFIGURATION_TRANSFER_EMPTY                 (1 << 7)
#define DWHCI_AHB_CONFIGURATION_DMA_ENABLE                     (1 << 5)
#define DWHCI_AHB_CONFIGURATION_WAIT_FOR_AXI_WRITES            (1 << 4)
#define DWHCI_AHB_CONFIGURATION_AXI_BURST_LENGTH_SINGLE        (0x0 << 1)
#define DWHCI_AHB_CONFIGURATION_AXI_BURST_LENGTH_INCREMENT_2   (0x1 << 1)
#define DWHCI_AHB_CONFIGURATION_AXI_BURST_LENGTH_INCREMENT_4   (0x3 << 1)
#define DWHCI_AHB_CONFIGURATION_AXI_BURST_LENGTH_INCREMENT_8   (0x5 << 1)
#define DWHCI_AHB_CONFIGURATION_AXI_BURST_LENGTH_INCREMENT_16  (0x7 << 1)
#define DWHCI_AHB_CONFIGURATION_AXI_BURST_LENGTH_MASK          (0xF << 1)
#define DWHCI_AHB_CONFIGURATION_AXI_BURST_LENGTH_SHIFT         1
#define DWHCI_AHB_CONFIGURATION_INTERRUPT_ENABLE               (1 << 0)

//
// Define the flags for the USB configuration register.
//

#define DWHCI_USB_CONFIGURATION_FORCE_DEVICE_MODE                (1 << 30)
#define DWHCI_USB_CONFIGURATION_FORCE_HOST_MODE                  (1 << 29)
#define DWHCI_USB_CONFIGURATION_TRANSMIT_END_DEALY               (1 << 28)
#define DWHCI_USB_CONFIGURATION_IC_TRAFFIC_PULL_REMOVE           (1 << 27)
#define DWHCI_USB_CONFIGURATION_IC_USB_CAPABLE                   (1 << 26)
#define DWHCI_USB_CONFIGURATION_ULPI_INTERRUPT_PROT_DISABLED     (1 << 25)
#define DWHCI_USB_CONFIGURATION_INDICATOR_PASS_THROUGH           (1 << 24)
#define DWHCI_USB_CONFIGURATION_INDICATOR_COMPLEMENT             (1 << 23)
#define DWHCI_USB_CONFIGURATION_TS_DLINE_PULSE_ENABLE            (1 << 22)
#define DWHCI_USB_CONFIGURATION_ULPI_INTERRUPT_VBUS_INDICATOR    (1 << 21)
#define DWHCI_USB_CONFIGURATION_ULPI_DRIVER_EXTERNAL_VBUS        (1 << 20)
#define DWHCI_USB_CONFIGURATION_ULPI_CLOCK_SUSPEND_MODE          (1 << 19)
#define DWHCI_USB_CONFIGURATION_ULPI_AUTO_RESET                  (1 << 18)
#define DWHCI_USB_CONFIGURATION_ULPI_FULL_SPEED_LOW_SPEED_SELECT (1 << 17)
#define DWHCI_USB_CONFIGURATION_OTG_UTMI_FULL_SPEED_SELECT       (1 << 16)
#define DWHCI_USB_CONFIGURATION_PHY_LOW_POWER_LOCK_SELECT        (1 << 15)
#define DWHCI_USB_CONFIGURATION_USB_TRIED_TIME_MASK              (0xF << 10)
#define DWHCI_USB_CONFIGURATION_USB_TRIED_TIME_SHIFT             10
#define DWHCI_USB_CONFIGURATION_HNP_CAPABLE                      (1 << 9)
#define DWHCI_USB_CONFIGURATION_SRP_CAPABLE                      (1 << 8)
#define DWHCI_USB_CONFIGURATION_DDR_SELECT                       (1 << 7)
#define DWHCI_USB_CONFIGURATION_PHY_SELECT                       (1 << 6)
#define DWHCI_USB_CONFIGURATION_FULL_SPEED_INTERFACE             (1 << 5)
#define DWHCI_USB_CONFIGURATION_MODE_SELECT_UTMI                 (0x0 << 4)
#define DWHCI_USB_CONFIGURATION_MODE_SELECT_ULPI                 (0x1 << 4)
#define DWHCI_USB_CONFIGURATION_MODE_SELECT_MASK                 (0x1 << 4)
#define DWHCI_USB_CONFIGURATION_MODE_SELECT_SHIFT                4
#define DWHCI_USB_CONFIGURATION_PHY_INTERFACE_16                 (1 << 3)
#define DWHCI_USB_CONFIGURATION_TIMEOUT_MASK                     (0x7 << 0)
#define DWHCI_USB_CONFIGURATION_TIMEOUT_SHIFT                    0

//
// Define the flags for the core reset register.
//

#define DWHCI_CORE_RESET_AHB_MASTER_IDLE                    (1 << 31)
#define DWHCI_CORE_RESET_DMA_REQUEST_SIGNAL                 (1 << 30)
#define DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH_ALL            (0x10 << 6)
#define DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH_PERIODIC_MASK  (0xF << 6)
#define DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH_PERIODIC_SHIFT 6
#define DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH_NON_PERIODIC   (0x0 << 6)
#define DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH_MASK           (0x1F << 6)
#define DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH_SHIFT          6
#define DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH                (1 << 5)
#define DWHCI_CORE_RESET_RECEIVE_FIFO_FLUSH                 (1 << 4)
#define DWHCI_CORE_RESET_IN_TOKEN_QUEUE_FLUSH               (1 << 3)
#define DWHCI_CORE_RESET_HOST_FRAME_COUNTER                 (1 << 2)
#define DWHCI_CORE_RESET_HOST_SOFT_RESET                    (1 << 1)
#define DWHCI_CORE_RESET_CORE_SOFT_RESET                    (1 << 0)

//
// Define the flags for the core interrupts.
//

#define DWHCI_CORE_INTERRUPT_WAKEUP                      (1 << 31)
#define DWHCI_CORE_INTERRUPT_SESSION_REQUEST             (1 << 30)
#define DWHCI_CORE_INTERRUPT_DISCONNECT                  (1 << 29)
#define DWHCI_CORE_INTERRUPT_CONNECTION_ID_STATUS        (1 << 28)
#define DWHCI_CORE_INTERRUPT_LOW_POWER_MODE_TRANSMIT     (1 << 27)
#define DWHCI_CORE_INTERRUPT_PERIODIC_FIFO_EMPTY         (1 << 26)
#define DWHCI_CORE_INTERRUPT_HOST_CHANNEL                (1 << 25)
#define DWHCI_CORE_INTERRUPT_PORT                        (1 << 24)
#define DWHCI_CORE_INTERRUPT_RESET_DETECTED              (1 << 23)
#define DWHCI_CORE_INTERRUPT_FET_SETUP                   (1 << 22)
#define DWHCI_CORE_INTERRUPT_INCOMPLETE_ISOCHRONOUS_OUT  (1 << 21)
#define DWHCI_CORE_INTERRUPT_INCOMPLETE_ISOCHRONOUS_IN   (1 << 20)
#define DWHCI_CORE_INTERRUPT_OUT_ENDPOINT                (1 << 19)
#define DWHCI_CORE_INTERRUPT_IN_ENDPOINT                 (1 << 18)
#define DWHCI_CORE_INTERRUPT_ENDPOINT_MISMATCH           (1 << 17)
#define DWHCI_CORE_INTERRUPT_RESTORE_COMPLETE            (1 << 16)
#define DWHCI_CORE_INTERRUPT_EOP_FRAME                   (1 << 15)
#define DWHCI_CORE_INTERRUPT_ISOCHRONOUS_OUT_DROP        (1 << 14)
#define DWHCI_CORE_INTERRUPT_ENUMERATION_COMPLETE        (1 << 13)
#define DWHCI_CORE_INTERRUPT_USB_RESET                   (1 << 12)
#define DWHCI_CORE_INTERRUPT_USB_SUSPEND                 (1 << 11)
#define DWHCI_CORE_INTERRUPT_EARLY_SUSPEND               (1 << 10)
#define DWHCI_CORE_INTERRUPT_I2C                         (1 << 9)
#define DWHCI_CORE_INTERRUPT_ULPI                        (1 << 8)
#define DWHCI_CORE_INTERRUPT_GLOBAL_OUT_NAK              (1 << 7)
#define DWHCI_CORE_INTERRUPT_GLOBAL_IN_NAK               (1 << 6)
#define DWHCI_CORE_INTERRUPT_NON_PERIODIC_FIFO_EMPTY     (1 << 5)
#define DWHCI_CORE_INTERRUPT_RECEIVE_FIFO_PACKET         (1 << 4)
#define DWHCI_CORE_INTERRUPT_START_OF_FRAME              (1 << 3)
#define DWHCI_CORE_INTERRUPT_OTG                         (1 << 2)
#define DWHCI_CORE_INTERRUPT_MODE_MISMATCH               (1 << 1)
#define DWHCI_CORE_INTERRUPT_HOST_MODE                   (1 << 0)

//
// Define the flags for the receive FIFO status.
//

#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_NAK                (0x1 << 17)
#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_IN                 (0x2 << 17)
#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_OUT                (0x2 << 17)
#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_COMPLETE           (0x3 << 17)
#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_SETUP_COMPLETE     (0x4 << 17)
#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_DATA_TOGGLE_ERROR  (0x5 << 17)
#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_SETUP_RECEVIED     (0x6 << 17)
#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_CHANNEL_HALTED     (0x7 << 17)
#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_MASK               (0xF << 17)
#define DWHCI_RECEIVE_FIFO_PACKET_STATUS_SHIFT              17
#define DWHCI_RECEIVE_FIFO_PID_MASK                         (0x3 << 15)
#define DWHCI_RECEIVE_FIFO_PID_SHIFT                        15
#define DWHCI_RECEIVE_FIFO_TOTAL_BYTES_MASK                 (0x7FF << 4)
#define DWHCI_RECEIVE_FIFO_TOTAL_BYTES_SHIFT                4
#define DWHCI_RECEIVE_FIFO_CHANNEL_MASK                     (0xF << 0)
#define DWHCI_RECEIVE_FIFO_CHANNEL_SHIFT                    0

//
// Define the flags for the receive FIFO size register.
//

#define DWHCI_RECEIVE_FIFO_SIZE_DEPTH_MASK  (0xFFFF << 0)
#define DWHCI_RECEIVE_FIFO_SIZE_DEPTH_SHIFT 0

//
// Define the flags for the transmit FIFO status.
//

#define DWHCI_TRANSMIT_FIFO_ODD                            (1 << 31)
#define DWHCI_TRANSMIT_FIFO_CHANNEL_MASK                   (0xF << 27)
#define DWHCI_TRANSMIT_FIFO_CHANNEL_SHIFT                  27
#define DWHCI_TRANSMIT_FIFO_TOKEN_TYPE_IN_OUT              (0x0 << 25)
#define DWHCI_TRANSMIT_FIFO_TOKEN_TYPE_ZERO_LENGTH_OUT     (0x1 << 25)
#define DWHCI_TRANSMIT_FIFO_TOKEN_TYPE_PING_COMPLETE_SPLIT (0x2 << 25)
#define DWHCI_TRANSMIT_FIFO_TOKEN_TYPE_CHANNEL_HALT        (0x3 << 25)
#define DWHCI_TRANSMIT_FIFO_TOKEN_TYPE_MASK                (0x3 << 25)
#define DWHCI_TRANSMIT_FIFO_TOKEN_TYPE_SHIFT               25
#define DWHCI_TRANSMIT_FIFO_TERMINATE                      (1 << 24)
#define DWHCI_TRANSMIT_FIFO_QUEUE_SPACE_MASK               (0xFF << 16)
#define DWHCI_TRANSMIT_FIFO_QUEUE_SPACE_SHIFT              16
#define DWHCI_TRANSMIT_FIFO_SPACE_MASK                     (0xFFFF << 0)
#define DWHCI_TRANSMIT_FIFO_SPACE_SHIFT                    0

//
// Define the flags for the transmit FIFO size registers.
//

#define DWHCI_TRANSMIT_FIFO_SIZE_DEPTH_MASK          (0xFFFF << 16)
#define DWHCI_TRANSMIT_FIFO_SIZE_DEPTH_SHIFT         16
#define DWHCI_TRANSMIT_FIFO_SIZE_START_ADDRESS_MASK  (0xFFFF << 0)
#define DWHCI_TRANSMIT_FIFO_SIZE_START_ADDRESS_SHIFT 0

//
// Define the flags for the 2nd host controller hardware register.
//

#define DWHCI_HARDWARE2_ENABLE_IC_USB                  (1 << 31)
#define DWHCI_HARDWARE2_DEVICE_TOKEN_QUEUE_DEPTH_MASK  (0x1F << 26)
#define DWHCI_HARDWARE2_DEVICE_TOKEN_QUEUE_DEPTH_SHIFT 26
#define DWHCI_HARDWARE2_PERIODIC_QUEUE_DEPTH_MASK      (0x3 << 24)
#define DWHCI_HARDWARE2_PERIODIC_QUEUE_DEPTH_SHIFT     24
#define DWHCI_HARDWARE2_NON_PERIODIC_QUEUE_DEPTH_MASK  (0x3 << 22)
#define DWHCI_HARDWARE2_NON_PERIODIC_QUEUE_DEPTH_SHIFT 22
#define DWHCI_HARDWARE2_MULTI_PROC_INT                 (0x1 << 20)
#define DWHCI_HARDWARE2_DYNAMIC_FIFO                   (0x1 << 19)
#define DWHCI_HARDWARE2_SUPPORTS_PERIODIC_ENDPOINTS    (0x1 << 18)
#define DWHCI_HARDWARE2_HOST_CHANNEL_COUNT_MASK        (0xF << 14)
#define DWHCI_HARDWARE2_HOST_CHANNEL_COUNT_SHIFT       14
#define DWHCI_HARDWARE2_DEVICE_ENDPOINT_COUNT_MASK     (0xF << 10)
#define DWHCI_HARDWARE2_DEVICE_ENDPOINT_COUNT_SHIFT    10
#define DWHCI_HARDWARE2_FULL_SPEED_NOT_SUPPORTED       (0x0 << 8)
#define DWHCI_HARDWARE2_FULL_SPEED_DEDICATED           (0x1 << 8)
#define DWHCI_HARDWARE2_FULL_SPEED_SHARED_ULPI         (0x2 << 8)
#define DWHCI_HARDWARE2_FULL_SPEED_SHARED_UTMI         (0x3 << 8)
#define DWHCI_HARDWARE2_FULL_SPEED_MASK                (0x3 << 8)
#define DWHCI_HARDWARE2_FULL_SPEED_SHIFT               8
#define DWHCI_HARDWARE2_HIGH_SPEED_NOT_SUPPORTED       (0x0 << 6)
#define DWHCI_HARDWARE2_HIGH_SPEED_UTMI                (0x1 << 6)
#define DWHCI_HARDWARE2_HIGH_SPEED_ULPI                (0x2 << 6)
#define DWHCI_HARDWARE2_HIGH_SPEED_UTMI_ULPI           (0x3 << 6)
#define DWHCI_HARDWARE2_HIGH_SPEED_MASK                (0x3 << 6)
#define DWHCI_HARDWARE2_HIGH_SPEED_SHIFT               6
#define DWHCI_HARDWARE2_POINT_TO_POINT                 (1 << 5)
#define DWHCI_HARDWARE2_ARCHITECTURE_SLAVE_ONLY        (0x0 << 3)
#define DWHCI_HARDWARE2_ARCHITECTURE_EXTERNAL_DMA      (0x1 << 3)
#define DWHCI_HARDWARE2_ARCHITECTURE_INTERNAL_DMA      (0x2 << 3)
#define DWHCI_HARDWARE2_ARCHITECTURE_MASK              (0x3 << 3)
#define DWHCI_HARDWARE2_ARCHITECTURE_SHIFT             3
#define DWHCI_HARDWARE2_MODE_HNP_SRP                   (0x0 << 0)
#define DWHCI_HARDWARE2_MODE_SRP_ONLY                  (0x1 << 0)
#define DWHCI_HARDWARE2_MODE_NO_HNP_SRP                (0x2 << 0)
#define DWHCI_HARDWARE2_MODE_SRP_DEVICE                (0x3 << 0)
#define DWHCI_HARDWARE2_MODE_NO_SRP_DEVICE             (0x4 << 0)
#define DWHCI_HARDWARE2_MODE_SRP_HOST                  (0x5 << 0)
#define DWHCI_HARDWARE2_MODE_NO_SRP_HOST               (0x6 << 0)
#define DWHCI_HARDWARE2_MODE_MASK                      (0x7 << 0)
#define DWHCI_HARDWARE2_MODE_SHIFT                     0

//
// Define flags for the 3rd host controller hardware register.
//

#define DWHCI_HARDWARE3_PACKET_COUNT_WIDTH_MASK    (0x7 << 4)
#define DWHCI_HARDWARE3_PACKET_COUNT_WIDTH_SHIFT   4
#define DWHCI_HARDWARE3_PACKET_COUNT_WIDTH_OFFSET  4
#define DWHCI_HARDWARE3_TRANSFER_SIZE_WIDTH_MASK   (0xf << 0)
#define DWHCI_HARDWARE3_TRANSFER_SIZE_WIDTH_SHIFT  0
#define DWHCI_HARDWARE3_TRANSFER_SIZE_WIDTH_OFFSET 11

//
// Define flags for the 4th host controller hardware register.
//

#define DWHCI_HARDWARE4_DMA_DYNAMIC_DESCRIPTOR_MODE          (1 << 31)
#define DWHCI_HARDWARE4_DMA_DESCRIPTOR_MODE                  (1 << 30)
#define DWHCI_HARDWARE4_IN_ENDPOINT_COUNT_MASK               (0xF << 26)
#define DWHCI_HARDWARE4_IN_ENDPOINT_COUNT_SHIFT              26
#define DWHCI_HARDWARE4_DEDICATED_FIFO_ENABLE                (1 << 25)
#define DWHCI_HARDWARE4_SESSION_END_FILTER_ENABLE            (1 << 24)
#define DWHCI_HARDWARE4_VALID_B_FILTER_ENABLE                (1 << 23)
#define DWHCI_HARDWARE4_VALID_A_FILTER_ENABLE                (1 << 22)
#define DWHCI_HARDWARE4_VALID_VBUS_FILTER_ENABLE             (1 << 21)
#define DWHCI_HARDWARE4_VALID_IDDIG_FILTER_ENABLE            (1 << 20)
#define DWHCI_HARDWARE4_MODE_CONTROL_ENDPOINT_COUNT_MASK     (0xF << 16)
#define DWHCI_HARDWARE4_MODE_CONTROL_ENDPOINT_COUNT_SHIFT    16
#define DWHCI_HARDWARE4_UTMI_PHYSICAL_DATA_WIDTH_8_BIT       (0x0 << 14)
#define DWHCI_HARDWARE4_UTMI_PHYSICAL_DATA_WIDTH_16_BIT      (0x1 << 14)
#define DWHCI_HARDWARE4_UTMI_PHYSICAL_DATA_WIDTH_8_OR_16_BIT (0x2 << 14)
#define DWHCI_HARDWARE4_UTMI_PHYSICAL_DATA_WIDTH_MASK        (0x3 << 14)
#define DWHCI_HARDWARE4_UTMI_PHYSICAL_DATA_WIDTH_SHIFT       14
#define DWHCI_HARDWARE4_PARTIAL_POWER_OFF                    (1 << 6)
#define DWHCI_HARDWARE4_MINIMUM_AHB_FREQUENCY                (1 << 5)
#define DWHCI_HARDWARE4_POWER_OPTIMIZATION                   (1 << 4)
#define DWHCI_HARDWARE4_PERIODIC_IN_ENDPOINT_COUNT_MASK      (0xF << 0)
#define DWHCI_HARDWARE4_PERIODIC_IN_ENDPOINT_COUNT_SHIFT     0

//
// Define the flags for the host configuration port.
//

#define DWHCI_HOST_CONFIGURATION_MODE_CHANGE_TIME           (1 << 31)
#define DWHCI_HOST_CONFIGURATION_PERIODIC_SCHEDULE_STATUS   (1 << 27)
#define DWHCI_HOST_CONFIGURATION_PERIODIC_SCHEDULE_ENABLE   (1 << 26)
#define DWHCI_HOST_CONFIGURATION_FRAME_LIST_ENTRIES_MASK    (0x3 << 24)
#define DWHCI_HOST_CONFIGURATION_FRAME_LIST_ENTRIES_SHIFT   24
#define DWHCI_HOST_CONFIGURATION_ENABLE_DMA_DESCRIPTOR      (1 << 23)
#define DWHCI_HOST_CONFIGURATION_RESPOND_VALID_PERIOD_MASK  (0xFF << 8)
#define DWHCI_HOST_CONFIGURATION_RESPOND_VALID_PERIOD_SHIFT 8
#define DWHCI_HOST_CONFIGURATION_ENABLE_32_KHZ_SUSPEND      (1 << 7)
#define DWHCI_HOST_CONFIGURATION_FULL_SPEED_LOW_SPEED_ONLY  (1 << 2)
#define DWHCI_HOST_CONFIGURATION_CLOCK_30_60_MHZ            (0x0 << 0)
#define DWHCI_HOST_CONFIGURATION_CLOCK_48_MHZ               (0x1 << 0)
#define DWHCI_HOST_CONFIGURATION_CLOCK_6_MHZ                (0x2 << 0)
#define DWHCI_HOST_CONFIGURATION_CLOCK_RATE_MASK            (0x3 << 0)
#define DWHCI_HOST_CONFIGURATION_CLOCK_RATE_SHIFT           0

//
// Define the flags for the frame number register.
//

#define DWHCI_FRAME_NUMBER_REMAINING_MASK  (0xFFFF << 16)
#define DWHCI_FRAME_NUMBER_REMAINING_SHIFT 16
#define DWHCI_FRAME_NUMBER_MASK            (0xFFFF << 0)
#define DWHCI_FRAME_NUMBER_SHIFT           0
#define DWHCI_FRAME_NUMBER_MAX             0x3FFF
#define DWHCI_FRAME_NUMBER_HIGH_BIT        0x2000

//
// Define the flags for the host's only port.
//

#define DWHCI_HOST_PORT_SPEED_HIGH            (0x0 << 17)
#define DWHCI_HOST_PORT_SPEED_FULL            (0x1 << 17)
#define DWHCI_HOST_PORT_SPEED_LOW             (0x2 << 17)
#define DWHCI_HOST_PORT_SPEED_MASK            (0x3 << 17)
#define DWHCI_HOST_PORT_TEST_CONTROL_MASK     (0xF << 13)
#define DWHCI_HOST_PORT_POWER                 (1 << 12)
#define DWHCI_HOST_PORT_LINE_STATE_MASK       (0x3 << 10)
#define DWHCI_HOST_PORT_RESET                 (1 << 8)
#define DWHCI_HOST_PORT_SUSPEND               (1 << 7)
#define DWHCI_HOST_PORT_RESUME                (1 << 6)
#define DWHCI_HOST_PORT_OVER_CURRENT_CHANGE   (1 << 5)
#define DWHCI_HOST_PORT_OVER_CURRENT_ACTIVE   (1 << 4)
#define DWHCI_HOST_PORT_ENABLE_CHANGE         (1 << 3)
#define DWHCI_HOST_PORT_ENABLE                (1 << 2)
#define DWHCI_HOST_PORT_CONNECT_STATUS_CHANGE (1 << 1)
#define DWHCI_HOST_PORT_CONNECT_STATUS        (1 << 0)
#define DWHCI_HOST_PORT_WRITE_TO_CLEAR_MASK  \
    (DWHCI_HOST_PORT_ENABLE |                \
     DWHCI_HOST_PORT_CONNECT_STATUS_CHANGE | \
     DWHCI_HOST_PORT_ENABLE_CHANGE |         \
     DWHCI_HOST_PORT_OVER_CURRENT_CHANGE)

#define DWHCI_HOST_PORT_INTERRUPT_MASK       \
    (DWHCI_HOST_PORT_CONNECT_STATUS_CHANGE | \
     DWHCI_HOST_PORT_ENABLE_CHANGE |         \
     DWHCI_HOST_PORT_OVER_CURRENT_CHANGE)

//
// Define the flags for the channel control registers.
//

#define DWHCI_CHANNEL_CONTROL_ENABLE                  (1 << 31)
#define DWHCI_CHANNEL_CONTROL_DISABLE                 (1 << 30)
#define DWHCI_CHANNEL_CONTROL_ODD_FRAME               (1 << 29)
#define DWHCI_CHANNEL_CONTROL_DEVICE_ADDRESS_MASK     (0x7F << 22)
#define DWHCI_CHANNEL_CONTROL_DEVICE_ADDRESS_SHIFT    22
#define DWHCI_CHANNEL_CONTROL_PACKETS_PER_FRAME_MASK  (0x3 << 20)
#define DWHCI_CHANNEL_CONTROL_PACKETS_PER_FRAME_SHIFT 20
#define DWHCI_CHANNEL_CONTROL_ENDPOINT_CONTROL        (0x0 << 18)
#define DWHCI_CHANNEL_CONTROL_ENDPOINT_ISOCHRONOUS    (0x1 << 18)
#define DWHCI_CHANNEL_CONTROL_ENDPOINT_BULK           (0x2 << 18)
#define DWHCI_CHANNEL_CONTROL_ENDPOINT_INTERRUPT      (0x3 << 18)
#define DWHCI_CHANNEL_CONTROL_ENDPOINT_TYPE_MASK      (0x3 << 18)
#define DWHCI_CHANNEL_CONTROL_ENDPOINT_TYPE_SHIFT     18
#define DWHCI_CHANNEL_CONTROL_LOW_SPEED               (1 << 17)
#define DWHCI_CHANNEL_CONTROL_ENDPOINT_DIRECTION_IN   (1 << 15)
#define DWHCI_CHANNEL_CONTROL_ENDPOINT_MASK           (0xF << 11)
#define DWHCI_CHANNEL_CONTROL_ENDPOINT_SHIFT          11
#define DWHCI_CHANNEL_CONTROL_MAX_PACKET_SIZE_WIDTH   11
#define DWHCI_CHANNEL_CONTROL_MAX_PACKET_SIZE_MASK    (0x7ff << 0)
#define DWHCI_CHANNEL_CONTROL_MAX_PACKET_SIZE_SHIFT   0

//
// Define the flags for the channel split control registers.
//

#define DWHCI_CHANNEL_SPLIT_CONTROL_ENABLE             (0x1 << 31)
#define DWHCI_CHANNEL_SPLIT_CONTROL_COMPLETE_SPLIT     (0x1 << 16)
#define DWHCI_CHANNEL_SPLIT_CONTROL_POSITION_MIDDLE    (0x0 << 14)
#define DWHCI_CHANNEL_SPLIT_CONTROL_POSITION_END       (0x1 << 14)
#define DWHCI_CHANNEL_SPLIT_CONTROL_POSITION_BEGIN     (0x2 << 14)
#define DWHCI_CHANNEL_SPLIT_CONTROL_POSITION_ALL       (0x3 << 14)
#define DWHCI_CHANNEL_SPLIT_CONTROL_POSITION_MASK      (0x3 << 14)
#define DWHCI_CHANNEL_SPLIT_CONTROL_POSITION_SHIFT     14
#define DWHCI_CHANNEL_SPLIT_CONTROL_HUB_ADDRESS_MASK   (0x7F << 7)
#define DWHCI_CHANNEL_SPLIT_CONTROL_HUB_ADDRESS_SHIFT  7
#define DWHCI_CHANNEL_SPLIT_CONTROL_PORT_ADDRESS_MASK  (0x7F << 0)
#define DWHCI_CHANNEL_SPLIT_CONTROL_PORT_ADDRESS_SHIFT 0

//
// Define the flags for the channel interrupt and interrupt mask registers.
//

#define DWHCI_CHANNEL_INTERRUPT_FRAME_LIST_ROLLOVER       (1 << 13)
#define DWHCI_CHANNEL_INTERRUPT_DMA_EXCESSIVE_TRANSACTION (1 << 12)
#define DWHCI_CHANNEL_INTERRUPT_DMA_BUFFER_NOT_AVAILABLE  (1 << 11)
#define DWHCI_CHANNEL_INTERRUPT_DATA_TOGGLE_ERROR         (1 << 10)
#define DWHCI_CHANNEL_INTERRUPT_FRAME_OVERRUN             (1 << 9)
#define DWHCI_CHANNEL_INTERRUPT_BABBLE_ERROR              (1 << 8)
#define DWHCI_CHANNEL_INTERRUPT_TRANSACTION_ERROR         (1 << 7)
#define DWHCI_CHANNEL_INTERRUPT_NOT_YET                   (1 << 6)
#define DWHCI_CHANNEL_INTERRUPT_ACK                       (1 << 5)
#define DWHCI_CHANNEL_INTERRUPT_NAK                       (1 << 4)
#define DWHCI_CHANNEL_INTERRUPT_STALL                     (1 << 3)
#define DWHCI_CHANNEL_INTERRUPT_AHB_ERROR                 (1 << 2)
#define DWHCI_CHANNEL_INTERRUPT_HALTED                    (1 << 1)
#define DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE         (1 << 0)
#define DWHCI_CHANNEL_INTERRUPT_ERROR_MASK              \
    (DWHCI_CHANNEL_INTERRUPT_AHB_ERROR |                \
     DWHCI_CHANNEL_INTERRUPT_STALL |                    \
     DWHCI_CHANNEL_INTERRUPT_TRANSACTION_ERROR |        \
     DWHCI_CHANNEL_INTERRUPT_BABBLE_ERROR |             \
     DWHCI_CHANNEL_INTERRUPT_DATA_TOGGLE_ERROR |        \
     DWHCI_CHANNEL_INTERRUPT_DMA_BUFFER_NOT_AVAILABLE | \
     DWHCI_CHANNEL_INTERRUPT_DMA_EXCESSIVE_TRANSACTION)

//
// Define the different PID codes.
//

#define DWHCI_PID_CODE_DATA_0 0
#define DWHCI_PID_CODE_DATA_1 2
#define DWHCI_PID_CODE_DATA_2 1
#define DWHCI_PID_CODE_MORE_DATA 3
#define DWHCI_PID_CODE_SETUP 3

//
// Define the flags for the channel transfer registers.
//

#define DWHCI_CHANNEL_TOKEN_PING                (1 << 31)
#define DWHCI_CHANNEL_TOKEN_PID_CODE_DATA_0     (DWHCI_PID_CODE_DATA_0 << 29)
#define DWHCI_CHANNEL_TOKEN_PID_CODE_DATA_1     (DWHCI_PID_CODE_DATA_1 << 29)
#define DWHCI_CHANNEL_TOKEN_PID_CODE_DATA_2     (DWHCI_PID_CODE_DATA_2 << 29)
#define DWHCI_CHANNEL_TOKEN_PID_CODE_MORE_DATA  (DWHCI_PID_CODE_MORE_DATA << 29)
#define DWHCI_CHANNEL_TOKEN_PID_CODE_SETUP      (DWHCI_PID_CODE_SETUP << 29)
#define DWHCI_CHANNEL_TOKEN_PID_MASK            (0x3 << 29)
#define DWHCI_CHANNEL_TOKEN_PID_SHIFT           29
#define DWHCI_CHANNEL_TOKEN_PACKET_COUNT_MASK   (0x3FF << 19)
#define DWHCI_CHANNEL_TOKEN_PACKET_COUNT_SHIFT  19
#define DWHCI_CHANNEL_TOKEN_TRANSFER_SIZE_MASK  (0x7FFFF << 0)
#define DWHCI_CHANNEL_TOKEN_TRANSFER_SIZE_SHIFT 0

//
// Define the maximum allowed transfer size and packet counts.
//

#define DWHCI_MAX_PACKET_COUNT 0x3FF
#define DWHCI_MAX_TRANSFER_SIZE 0x7FFFF

//
// Define the flags for the DWHCI power and clock configuration register.
//

#define DWHCI_POWER_AND_CLOCK_RESET_AFTER_SUSPEND       (1 << 8)
#define DWHCI_POWER_AND_CLOCK_DEEP_SLEEP                (1 << 7)
#define DWHCI_POWER_AND_CLOCK_PHY_SLEEPING              (1 << 6)
#define DWHCI_POWER_AND_CLOCK_SLEEP_CLOCK_GATING_ENABLE (1 << 5)
#define DWHCI_POWER_AND_CLOCK_PHY_SUSPENDED             (1 << 4)
#define DWHCI_POWER_AND_CLOCK_POWER_DOWN_MODULES        (1 << 3)
#define DWHCI_POWER_AND_CLOCK_POWER_CLAMP               (1 << 2)
#define DWHCI_POWER_AND_CLOCK_GATE_H_CLOCK              (1 << 1)
#define DWHCI_POWER_AND_CLOCK_STOP_P_CLOCK              (1 << 0)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the offsets to host controller registers, in bytes.
//

typedef enum _DWHCI_REGISTER {
    DwhciRegisterOtgControl               = 0x0,
    DwhciRegisterOtgInterrupt             = 0x4,
    DwhciRegisterAhbConfiguration         = 0x8,
    DwhciRegisterUsbConfiguration         = 0xC,
    DwhciRegisterCoreReset                = 0x10,
    DwhciRegisterCoreInterrupt            = 0x14,
    DwhciRegisterCoreInterruptMask        = 0x18,
    DwhciRegisterReceiveFifoStatus        = 0x1C,
    DwhciRegisterReceiveFifoStatusAndPop  = 0x20,
    DwhciRegisterReceiveFifoSize          = 0x24,
    DwhciRegisterNonPeriodicFifoSize      = 0x28,
    DwhciRegisterNonPeriodicFifoStatus    = 0x2c,
    DwhciRegisterCoreId                   = 0x40,
    DwhciRegisterHardware1                = 0x44,
    DwhciRegisterHardware2                = 0x48,
    DwhciRegisterHardware3                = 0x4C,
    DwhciRegisterHardware4                = 0x50,
    DwhciRegisterPeriodicFifoSize         = 0x100,
    DwhciRegisterHostConfiguration        = 0x400,
    DwhciRegisterFrameNumber              = 0x408,
    DwhciRegisterPeriodicFifoStatus       = 0x410,
    DwhciRegisterHostChannelInterrupt     = 0x414,
    DwhciRegisterHostChannelInterruptMask = 0x418,
    DwhciRegisterHostPort                 = 0x440,
    DwhciRegisterChannelBase              = 0x500,
    DwhciRegisterPowerAndClock            = 0xE00,
} DWHCI_REGISTER, *PDWHCI_REGISTER;

//
// Define the offsets to host controller channel registers, in byte.
//

typedef enum _DWHCI_CHANNEL_REGISTER {
    DwhciChannelRegisterControl          = 0x0,
    DwhciChannelRegisterSplitControl     = 0x4,
    DwhciChannelRegisterInterrupt        = 0x8,
    DwhciChannelRegisterInterruptMask    = 0xC,
    DwhciChannelRegisterToken            = 0x10,
    DwhciChannelRegisterDmaAddress       = 0x14,
    DwhciChannelRegisterDmaBufferAddress = 0x1C,
    DwhciChannelRegistersSize            = 0x20
} DWHCI_CHANNEL_REGISTER, *PDWHCI_CHANNEL_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

