/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rootbus.c

Abstract:

    This module implements support for the PCI root bus on PC/AT systems
    through I/O ports CF8/CFC.

Author:

    Evan Green 17-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "pci.h"
#include <minoca/kernel/ioport.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro creates the address value used to read from or write to PCI
// configuration space. All parameters should be UCHARs.
//

#define PCI_CONFIG_ADDRESS(_Bus, _Device, _Function, _Register) \
    (((ULONG)(_Bus) << 16) | ((ULONG)(_Device) << 11) |         \
     ((ULONG)(_Function) << 8) | ((_Register) & 0xFF) | 0x80000000)

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

ULONGLONG
PcipRootReadConfig (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize
    )

/*++

Routine Description:

    This routine reads from PCI Configuration Space on the root PCI bus.

Arguments:

    Bus - Supplies the bus number to read from.

    Device - Supplies the device number to read from. Valid values are 0 to 31.

    Function - Supplies the PCI function to read from. Valid values are 0 to 7.

    Register - Supplies the configuration register to read from.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

Return Value:

    Returns the value read from the bus, or 0xFFFFFFFF on error.

--*/

{

    ULONG Address;
    ULONGLONG Value;

    //
    // Create the configuration address and write it into the address port.
    //

    Address = PCI_CONFIG_ADDRESS(Bus, Device, Function, Register);
    HlIoPortOutLong(PCI_ROOT_CONFIG_ADDRESS, Address);

    //
    // Read the data at that address.
    //

    switch (AccessSize) {
    case sizeof(UCHAR):
        Value = HlIoPortInByte(PCI_ROOT_CONFIG_DATA);
        break;

    case sizeof(USHORT):
        Value = HlIoPortInShort(PCI_ROOT_CONFIG_DATA);
        break;

    case sizeof(ULONG):
        Value = HlIoPortInLong(PCI_ROOT_CONFIG_DATA);
        break;

    case sizeof(ULONGLONG):
        Value = HlIoPortInLong(PCI_ROOT_CONFIG_DATA);
        HlIoPortOutLong(PCI_ROOT_CONFIG_ADDRESS, Address + 4);
        Value |= ((ULONGLONG)HlIoPortInLong(PCI_ROOT_CONFIG_DATA)) << 32;
        break;

    default:

        ASSERT(FALSE);

        Value = -1;
        break;
    }

    return Value;
}

VOID
PcipRootWriteConfig (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine writes to PCI Configuration Space on the PCI root bus.

Arguments:

    Bus - Supplies the bus number to write to.

    Device - Supplies the device number to write to. Valid values are 0 to 31.

    Function - Supplies the PCI function to write to. Valid values are 0 to 7.

    Register - Supplies the configuration register to write to.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies the value to write to the register.

Return Value:

    None.

--*/

{

    ULONG Address;

    //
    // Create the configuration address and write it into the address port.
    //

    Address = PCI_CONFIG_ADDRESS(Bus, Device, Function, Register);
    HlIoPortOutLong(PCI_ROOT_CONFIG_ADDRESS, Address);

    //
    // Write the data at that address.
    //

    switch (AccessSize) {
    case sizeof(UCHAR):
        HlIoPortOutByte(PCI_ROOT_CONFIG_DATA, (UCHAR)Value);
        break;

    case sizeof(USHORT):
        HlIoPortOutShort(PCI_ROOT_CONFIG_DATA, (USHORT)Value);
        break;

    case sizeof(ULONG):
        HlIoPortOutLong(PCI_ROOT_CONFIG_DATA, (ULONG)Value);
        break;

    case sizeof(ULONGLONG):
        HlIoPortOutLong(PCI_ROOT_CONFIG_DATA, (ULONG)Value);
        HlIoPortOutLong(PCI_ROOT_CONFIG_ADDRESS, Address + 4);
        HlIoPortOutLong(PCI_ROOT_CONFIG_DATA, (ULONG)(Value >> 32));
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

