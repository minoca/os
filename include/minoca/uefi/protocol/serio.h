/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    serio.h

Abstract:

    This header contains definitions for the UEFI Serial I/O protocol.

Author:

    Evan Green 8-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_SERIAL_IO_PROTOCOL_GUID                         \
    {                                                       \
        0xBB25CF6F, 0xF1D4, 0x11D2,                         \
        {0x9A, 0x0C, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0xFD}    \
    }

//
// Define Control bits, grouped by read only, write only, and read write
//

//
// Read Only control bits
//

#define EFI_SERIAL_CLEAR_TO_SEND        0x00000010
#define EFI_SERIAL_DATA_SET_READY       0x00000020
#define EFI_SERIAL_RING_INDICATE        0x00000040
#define EFI_SERIAL_CARRIER_DETECT       0x00000080
#define EFI_SERIAL_INPUT_BUFFER_EMPTY   0x00000100
#define EFI_SERIAL_OUTPUT_BUFFER_EMPTY  0x00000200

//
// Write Only control bits
//

#define EFI_SERIAL_REQUEST_TO_SEND      0x00000002
#define EFI_SERIAL_DATA_TERMINAL_READY  0x00000001

//
// Read/Write control bits
//

#define EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE     0x00001000
#define EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE     0x00002000
#define EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE 0x00004000

//
// Protocol revision information
//

#define EFI_SERIAL_IO_PROTOCOL_REVISION 0x00010000
#define SERIAL_IO_INTERFACE_REVISION EFI_SERIAL_IO_PROTOCOL_REVISION

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_SERIAL_IO_PROTOCOL EFI_SERIAL_IO_PROTOCOL;

//
// EFI1.1 definition
//

typedef EFI_SERIAL_IO_PROTOCOL SERIAL_IO_INTERFACE;

typedef enum {
    DefaultParity,
    NoParity,
    EvenParity,
    OddParity,
    MarkParity,
    SpaceParity
} EFI_PARITY_TYPE;

typedef enum {
    DefaultStopBits,
    OneStopBit,
    OneFiveStopBits,
    TwoStopBits
} EFI_STOP_BITS_TYPE;

typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_RESET) (
    EFI_SERIAL_IO_PROTOCOL *This
    );

/*++

Routine Description:

    This routine resets the serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS if the device was reset.

    EFI_DEVICE_ERROR if the device could not be reset.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_SET_ATTRIBUTES) (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT64 BaudRate,
    UINT32 ReceiveFifoDepth,
    UINT32 Timeout,
    EFI_PARITY_TYPE Parity,
    UINT8 DataBits,
    EFI_STOP_BITS_TYPE StopBits
    );

/*++

Routine Description:

    This routine sets the baud rate, receive FIFO depth, transmit/receive
    timeout, parity, data bits, and stop bits on a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BaudRate - Supplies the desired baud rate. A value of zero will use the
        default interface speed.

    ReceiveFifoDepth - Supplies the requested depth of the receive FIFO. A
        value of zero uses the default FIFO size.

    Timeout - Supplies the timeout in microseconds for attempting to receive
        a single character. A timeout of zero uses the default timeout.

    Parity - Supplies the type of parity to use on the device.

    DataBits - Supplies the number of bits per byte on the serial device. A
        value of zero uses a default value.

    StopBits - Supplies the number of stop bits to use on the serial device.
        A value of zero uses a default value.

Return Value:

    EFI_SUCCESS if the device was reset.

    EFI_DEVICE_ERROR if the device could not be reset.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_SET_CONTROL_BITS) (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT32 Control
    );

/*++

Routine Description:

    This routine sets the control bits on a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Control - Supplies the control bits to set.

Return Value:

    EFI_SUCCESS if the new control bits were set.

    EFI_UNSUPPORTED if the serial device does not support this operation.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_GET_CONTROL_BITS) (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT32 *Control
    );

/*++

Routine Description:

    This routine gets the control bits on a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Control - Supplies a pointer where the current control bits will be
        returned.

Return Value:

    EFI_SUCCESS if the new control bits were set.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_WRITE) (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine writes data to a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer. On output, the number of bytes successfully written will be
        returned.

    Buffer - Supplies a pointer to the data to write.

Return Value:

    EFI_SUCCESS if the data was written

    EFI_DEVICE_ERROR if the device is not functioning properly.

    EFI_TIMEOUT if the operation timed out before the data could be written.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_READ) (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine reads data from a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer. On output, the number of bytes successfully read will be
        returned.

    Buffer - Supplies a pointer where the read data will be returned on success.

Return Value:

    EFI_SUCCESS if the data was read.

    EFI_DEVICE_ERROR if the device is not functioning properly.

    EFI_TIMEOUT if the operation timed out before the data could be read.

--*/

/*++

Structure Description:

    This structure defines the current mode for a serial device. These values
    are read-only and are updated by using the set attributes function.

Members:

    ControlMask - Stores the mask of control bits the device supports. The
        device must always support the Input Empty bit.

    TimeOut - Stores the number of microseconds to wait before timing out on a
        read or write operation.

    BaudRate - Stores the current baud rate, or zero to indicate the device
        runs at it designated speed.

    ReceiveFifoDepth - Stores the current receive FIFO depth.

    DataBits - Stores the number of data bits in a byte.

    Parity - Stores the current device parity.

    StopBits - Stores the stop bit type configured on the device.

--*/

typedef struct {
    UINT32 ControlMask;
    UINT32 Timeout;
    UINT64 BaudRate;
    UINT32 ReceiveFifoDepth;
    UINT32 DataBits;
    UINT32 Parity;
    UINT32 StopBits;
} EFI_SERIAL_IO_MODE;

/*++

Structure Description:

    This structure defines the UEFI Serial I/O protocol.

Members:

    Revision - Stores the revision to which this protocol instance adheres. All
        future revisions must be backwards compatible.

    Reset - Store a pointer to a function used to reset the device.

    SetAttributes - Stores a pointer to a function used to set device
        attributes.

    SetControl - Stores a pointer to a function used to set the control bits.

    GetControl - Stores a pointer to a function used to get the control bits.

    Write - Stores a pointer to a function used to transmit data.

    Read - Stores a pointer to a function used to receive data.

    Mode - Stores a pointer to the current mode.

--*/

struct _EFI_SERIAL_IO_PROTOCOL {
    UINT32 Revision;
    EFI_SERIAL_RESET Reset;
    EFI_SERIAL_SET_ATTRIBUTES SetAttributes;
    EFI_SERIAL_SET_CONTROL_BITS SetControl;
    EFI_SERIAL_GET_CONTROL_BITS GetControl;
    EFI_SERIAL_WRITE Write;
    EFI_SERIAL_READ Read;
    EFI_SERIAL_IO_MODE *Mode;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
