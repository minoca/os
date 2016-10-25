/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    earlypci.h

Abstract:

    This header contains definitions for early PCI configuration space access
    for BIOSes that need to access PCI config Operation Regions before PCI is
    ready.

Author:

    Evan Green 16-Dec-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

ULONGLONG
AcpipEarlyReadPciConfigurationSpace (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize
    );

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

VOID
AcpipEarlyWritePciConfigurationSpace (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize,
    ULONGLONG Value
    );

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

