/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    regacces.c

Abstract:

    This module implements basic register access functionality.

Author:

    Evan Green 31-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/crash.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
UCHAR
HlIoPortInByte (
    USHORT InputPort
    )

/*++

Routine Description:

    This routine gets one byte from the specified legacy I/O port.

Arguments:

    InputPort - Supplies the port to receive a byte from.

Return Value:

    Returns the data at that port.

--*/

{

    KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                  HL_CRASH_NO_IO_PORTS,
                  InputPort,
                  0,
                  1);

    return 0;
}

KERNEL_API
VOID
HlIoPortOutByte (
    USHORT OutputPort,
    BYTE OutputData
    )

/*++

Routine Description:

    This routine sends one byte to the specified legacy I/O port.

Arguments:

    OutputPort - Supplies the port to send a byte to.

    OutputData - Supplies the data to send.

Return Value:

    None.

--*/

{

    KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                  HL_CRASH_NO_IO_PORTS,
                  OutputPort,
                  OutputData,
                  1);

    return;
}

KERNEL_API
USHORT
HlIoPortInShort (
    USHORT InputPort
    )

/*++

Routine Description:

    This routine gets one 16 bit value from the specified legacy I/O port.

Arguments:

    InputPort - Supplies the port to receive a byte from.

Return Value:

    Returns the data at that port.

--*/

{

    KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                  HL_CRASH_NO_IO_PORTS,
                  InputPort,
                  0,
                  2);

    return 0;
}

KERNEL_API
VOID
HlIoPortOutShort (
    USHORT OutputPort,
    USHORT OutputData
    )

/*++

Routine Description:

    This routine sends one 16-bit value to the specified legacy I/O port.

Arguments:

    OutputPort - Supplies the port to send a byte to.

    OutputData - Supplies the data to send.

Return Value:

    None.

--*/

{

    KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                  HL_CRASH_NO_IO_PORTS,
                  OutputPort,
                  OutputData,
                  2);

    return;
}

KERNEL_API
ULONG
HlIoPortInLong (
    USHORT InputPort
    )

/*++

Routine Description:

    This routine gets a 32-bit value from the specified legacy I/O port.

Arguments:

    InputPort - Supplies the port to receive a long from.

Return Value:

    Returns the data at that port.

--*/

{

    KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                  HL_CRASH_NO_IO_PORTS,
                  InputPort,
                  0,
                  4);

    return 0;
}

KERNEL_API
VOID
HlIoPortOutLong (
    USHORT OutputPort,
    ULONG OutputData
    )

/*++

Routine Description:

    This routine sends one 32-bit to the specified legacy I/O port.

Arguments:

    OutputPort - Supplies the port to send a long to.

    OutputData - Supplies the data to send.

Return Value:

    None.

--*/

{

    KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                  HL_CRASH_NO_IO_PORTS,
                  OutputPort,
                  OutputData,
                  4);

    return;
}

KERNEL_API
ULONG
HlReadRegister32 (
    PVOID RegisterAddress
    )

/*++

Routine Description:

    This routine performs a 32-bit memory register read. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to read.

Return Value:

    Returns the value at the given register.

--*/

{

    RtlMemoryBarrier();
    return *((volatile ULONG *)RegisterAddress);
}

KERNEL_API
VOID
HlWriteRegister32 (
    PVOID RegisterAddress,
    ULONG Value
    )

/*++

Routine Description:

    This routine performs a 32-bit memory register write. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    RtlMemoryBarrier();
    *((volatile ULONG *)RegisterAddress) = Value;
    RtlMemoryBarrier();
    return;
}

KERNEL_API
USHORT
HlReadRegister16 (
    PVOID RegisterAddress
    )

/*++

Routine Description:

    This routine performs a 16-bit memory register read. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to read.

Return Value:

    Returns the value at the given register.

--*/

{

    RtlMemoryBarrier();
    return *((volatile USHORT *)RegisterAddress);
}

KERNEL_API
VOID
HlWriteRegister16 (
    PVOID RegisterAddress,
    USHORT Value
    )

/*++

Routine Description:

    This routine performs a 16-bit memory register write. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    RtlMemoryBarrier();
    *((volatile USHORT *)RegisterAddress) = Value;
    RtlMemoryBarrier();
    return;
}

KERNEL_API
UCHAR
HlReadRegister8 (
    PVOID RegisterAddress
    )

/*++

Routine Description:

    This routine performs an 8-bit memory register read. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to read.

Return Value:

    Returns the value at the given register.

--*/

{

    RtlMemoryBarrier();
    return *((volatile UCHAR *)RegisterAddress);
}

KERNEL_API
VOID
HlWriteRegister8 (
    PVOID RegisterAddress,
    UCHAR Value
    )

/*++

Routine Description:

    This routine performs an 8-bit memory register write. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    RtlMemoryBarrier();
    *((volatile UCHAR *)RegisterAddress) = Value;
    RtlMemoryBarrier();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

