/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    musbhw.h

Abstract:

    This header contains hardware definitions for the Mentor Graphics USB 2.0
    OTG controller.

Author:

    Evan Green 11-Sep-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns the proper non-indexed endpoint setup register for the
// given endpoint. The endpoint control registers include device address,
// hub address, and hub port.
//

#define MUSB_ENDPOINT_SETUP(_Register, _Index) \
    (MUSB_ENDPOINT_SETUP_OFFSET + ((_Index) << 3) + (_Register))

//
// This macro returns the proper non-indexed endpoint control/status register
// for the given endpoint.
//

#define MUSB_ENDPOINT_CONTROL(_Register, _Index) \
    (MUSB_ENDPOINT_CONTROL_OFFSET + (_Index << 4) + ((_Register) - 0x10))

//
// This macro returns the register value for the given FIFO.
//

#define MUSB_FIFO_REGISTER(_Index) (MusbFifo0 + ((_Index) << 2))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of endpoints.
//

#define MUSB_MAX_ENDPOINTS 16

//
// Define the non-indexed register offsets.
//

#define MUSB_ENDPOINT_SETUP_OFFSET 0x80
#define MUSB_ENDPOINT_CONTROL_OFFSET 0x100

//
// Define soft reset register bits.
//

#define MUSB_SOFT_RESET_SOFT_RESET 0x1

//
// Define endpoint info register bits.
//

#define MUSB_ENDPOINT_INFO_TX_COUNT_MASK 0x0F
#define MUSB_ENDPOINT_INFO_RX_COUNT_MASK 0xF0
#define MUSB_ENDPOINT_INFO_RX_COUNT_SHIFT 4

//
// Define power register bits.
//

#define MUSB_POWER_ENTER_SUSPEND 0x01
#define MUSB_POWER_SUSPEND 0x02
#define MUSB_POWER_RESUME 0x04
#define MUSB_POWER_RESET 0x08
#define MUSB_POWER_HIGH_SPEED 0x10
#define MUSB_POWER_HIGH_SPEED_ENABLE 0x20
#define MUSB_POWER_SOFT_CONNECT 0x40
#define MUSB_POWER_ISO_UPDATE 0x80

//
// Define device control register bits
//

#define MUSB_DEVICE_CONTROL_SESSION 0x01
#define MUSB_DEVICE_CONTROL_HOST_REQUEST 0x02
#define MUSB_DEVICE_CONTROL_HOST 0x04
#define MUSB_DEVICE_CONTROL_VBUS_SEND 0x08
#define MUSB_DEVICE_CONTROL_VBUS_AVALID 0x10
#define MUSB_DEVICE_CONTROL_LOW_SPEED 0x20
#define MUSB_DEVICE_CONTROL_FULL_SPEED 0x40
#define MUSB_DEVICE_CONTROL_DEVICE 0x80

//
// Define the TX Type register bits.
//

#define MUSB_TXTYPE_TARGET_ENDPOINT_MASK 0x0F
#define MUSB_TXTYPE_PROTOCOL_CONTROL 0x00
#define MUSB_TXTYPE_PROTOCOL_ISOCHRONOUS 0x10
#define MUSB_TXTYPE_PROTOCOL_BULK 0x20
#define MUSB_TXTYPE_PROTOCOL_INTERRUPT 0x30
#define MUSB_TXTYPE_PROTOCOL_MASK 0x30
#define MUSB_TXTYPE_SPEED_HIGH 0x40
#define MUSB_TXTYPE_SPEED_FULL 0x80
#define MUSB_TXTYPE_SPEED_LOW 0xC0
#define MUSB_TXTYPE_SPEED_MASK 0xC0

//
// Define endpoint 0 control/status register bits.
//

#define MUSB_EP0_CONTROL_RX_PACKET_READY 0x0001
#define MUSB_EP0_CONTROL_TX_PACKET_READY 0x0002
#define MUSB_EP0_CONTROL_RX_STALL 0x0004
#define MUSB_EP0_CONTROL_SETUP_PACKET 0x0008
#define MUSB_EP0_CONTROL_ERROR 0x0010
#define MUSB_EP0_CONTROL_REQUEST_PACKET 0x0020
#define MUSB_EP0_CONTROL_STATUS_PACKET 0x0040
#define MUSB_EP0_CONTROL_NAK_TIMEOUT 0x0080
#define MUSB_EP0_CONTROL_FLUSH_FIFO 0x0100
#define MUSB_EP0_CONTROL_DATA_TOGGLE 0x0200
#define MUSB_EP0_CONTROL_DATA_TOGGLE_WRITE 0x0400

#define MUSB_EP0_CONTROL_ERROR_MASK \
    (MUSB_EP0_CONTROL_RX_STALL | MUSB_EP0_CONTROL_ERROR | \
     MUSB_EP0_CONTROL_NAK_TIMEOUT)

//
// Define TX control/status register bits.
//

#define MUSB_TX_CONTROL_PACKET_READY 0x0001
#define MUSB_TX_CONTROL_FIFO_NOT_EMPTY 0x0002
#define MUSB_TX_CONTROL_ERROR 0x0004
#define MUSB_TX_CONTROL_FLUSH_FIFO 0x0008
#define MUSB_TX_CONTROL_RX_STALL 0x0020
#define MUSB_TX_CONTROL_CLEAR_TOGGLE 0x0040
#define MUSB_TX_CONTROL_NAK_TIMEOUT 0x0080
#define MUSB_TX_CONTROL_DATA_TOGGLE 0x0100
#define MUSB_TX_CONTROL_DATA_TOGGLE_WRITE 0x0200
#define MUSB_TX_CONTROL_DMA_MODE 0x0400
#define MUSB_TX_CONTROL_FORCE_DATA_TOGGLE 0x0800
#define MUSB_TX_CONTROL_DMA_ENABLE 0x1000
#define MUSB_TX_CONTROL_TRANSMIT_MODE 0x2000
#define MUSB_TX_CONTROL_ISOCHRONOUS 0x4000
#define MUSB_TX_CONTROL_AUTO_SET 0x8000

#define MUSB_TX_CONTROL_ERROR_MASK \
    (MUSB_TX_CONTROL_ERROR | \
     MUSB_TX_CONTROL_RX_STALL | \
     MUSB_TX_CONTROL_NAK_TIMEOUT)

//
// Define RX control/status register bits.
//

#define MUSB_RX_CONTROL_PACKET_READY 0x0001
#define MUSB_RX_CONTROL_FIFO_FULL 0x0002
#define MUSB_RX_CONTROL_OVERRUN 0x0004
#define MUSB_RX_CONTROL_ERROR 0x0004
#define MUSB_RX_CONTROL_DATA_ERROR_NAK_TIMEOUT 0x0008
#define MUSB_RX_CONTROL_FLUSH_FIFO 0x0010
#define MUSB_RX_CONTROL_SEND_STALL 0x0020
#define MUSB_RX_CONTROL_REQUEST_PACKET 0x0020
#define MUSB_RX_CONTROL_SENT_STALL 0x0040
#define MUSB_RX_CONTROL_RX_STALL 0x0040
#define MUSB_RX_CONTROL_CLEAR_TOGGLE 0x0080
#define MUSB_RX_CONTROL_DATA_TOGGLE 0x0200
#define MUSB_RX_CONTROL_DATA_TOGGLE_WRITE 0x0400
#define MUSB_RX_CONTROL_DMA_MODE 0x0800
#define MUSB_RX_CONTROL_DISABLE_NYET 0x1000
#define MUSB_RX_CONTROL_PID_ERROR 0x1000
#define MUSB_RX_CONTROL_DMA_ENABLE 0x2000
#define MUSB_RX_CONTROL_ISOCHRONOUS 0x4000
#define MUSB_RX_CONTROL_AUTO_REQUEST 0x4000
#define MUSB_RX_CONTROL_AUTO_CLEAR 0x8000

#define MUSB_RX_CONTROL_ERROR_MASK \
    (MUSB_RX_CONTROL_ERROR | MUSB_RX_CONTROL_DATA_ERROR_NAK_TIMEOUT | \
     MUSB_RX_CONTROL_RX_STALL)

//
// Define common USB interrupt bits.
//

#define MUSB_USB_INTERRUPT_SUSPEND 0x01
#define MUSB_USB_INTERRUPT_RESUME 0x02
#define MUSB_USB_INTERRUPT_RESET_BABBLE 0x04
#define MUSB_USB_INTERRUPT_SOF 0x08
#define MUSB_USB_INTERRUPT_CONNECT 0x10
#define MUSB_USB_INTERRUPT_DISCONNECT 0x20
#define MUSB_USB_INTERRUPT_SESSION 0x40
#define MUSB_USB_INTERRUPT_VBUS_ERROR 0x80

//
// Store the shift value to convert frames to microframes.
//

#define MUSB_MICROFRAMES_PER_FRAME 8
#define MUSB_MICROFRAMES_PER_FRAME_SHIFT 3

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the base set of registers that can always be accessed.
//

typedef enum _MUSB_REGISTER {
    MusbFunctionAddress = 0x00,
    MusbPower = 0x01,
    MusbInterruptTx = 0x02,
    MusbInterruptRx = 0x04,
    MusbInterruptEnableTx = 0x06,
    MusbInterruptEnableRx = 0x08,
    MusbInterruptUsb = 0x0A,
    MusbInterruptEnableUsb = 0x0B,
    MusbFrame = 0x0C,
    MusbIndex = 0x0E,
    MusbTestMode = 0x0F,
    MusbFifo0 = 0x20,
    MusbDeviceControl = 0x60,
    MusbEndpointInfo = 0x78,
    MusbRamInfo = 0x79,
    MusbLinkInfo = 0x7A,
    MusbVpLength = 0x7B,
    MusbHighSpeedEof1 = 0x7C,
    MusbFullSpeedEof1 = 0x7D,
    MusbLowSpeedEof1 = 0x7E,
    MusbSoftReset = 0x7F,
} MUSB_REGISTER, *PMUSB_REGISTER;

//
// Define the offsets for indexed registers. Software writes to the index
// register and then can access this set of registers for the desired endpoint.
//

typedef enum _MUSB_INDEXED_REGISTER {
    MusbTxMaxPacketSize = 0x10,
    MusbTxControlStatus = 0x12,
    MusbRxMaxPacketSize = 0x14,
    MusbRxControlStatus = 0x16,
    MusbCount = 0x18,
    MusbTxType = 0x1A,
    MusbNakLimit = 0x1B,
    MusbTxInterval = 0x1B,
    MusbRxType = 0x1C,
    MusbRxInterval = 0x1D,
    MusbConfigData = 0x1F,
    MusbTxFifoSize = 0x62,
    MusbRxFifoSize = 0x63,
    MusbTxFifoAddress = 0x64,
    MusbRxFifoAddress = 0x66
} MUSB_INDEXED_REGISTER, *PMUSB_INDEXED_REGISTER;

//
// Define the offsets for the non-indexed endpoint setup registers, with 0
// being the start of the register region.
//

typedef enum _MUSB_ENDPOINT_SETUP_REGISTER {
    MusbTxFunctionAddress = 0x00,
    MusbTxHubAddress = 0x02,
    MusbTxHubPort = 0x03,
    MusbRxFunctionAddress = 0x04,
    MusbRxHubAddress = 0x06,
    MusbRxHubPort = 0x07
} MUSB_ENDPOINT_SETUP_REGISTER, *PMUSB_ENDPOINT_SETUP_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
