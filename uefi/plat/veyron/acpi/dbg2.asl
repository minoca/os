/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbg2.asl

Abstract:

    This module implements the Debug Port Table 2.

Author:

    Evan Green 31-Mar-2014

Environment:

    Firmware

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// TODO: Switch to EHCI to avoid a dependence on the servo board.
//

[0004]                          Signature : "DBG2"    [Debug Port table type 2]
[0004]                       Table Length : 00000064
[0001]                           Revision : 01
[0001]                           Checksum : 00
[0006]                             Oem ID : "MINOCA"
[0008]                       Oem Table ID : "MINOCA  "
[0004]                       Oem Revision : 00000000
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20131115

[0004]                        Info Offset : 0000002C
[0004]                         Info Count : 00000001

[0001]                           Revision : 01
[0002]                             Length : 0038
[0001]                     Register Count : 01
[0002]                    Namepath Length : 0009
[0002]                    Namepath Offset : 0028
[0002]                    OEM Data Length : 0000 [Optional field not present]
[0002]                    OEM Data Offset : 0000 [Optional field not present]
[0002]                          Port Type : 8000
[0002]                       Port Subtype : 0000
[0002]                           Reserved : 0000
[0002]                Base Address Offset : 0016
[0002]                Address Size Offset : 0022

[0012]              Base Address Register : [Generic Address Structure]
[0001]                           Space ID : 00 [SystemMemory]
[0001]                          Bit Width : 32
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 03 [DWord Access:32]
[0008]                            Address : 00000000FF690000

[0004]                       Address Size : 00001000

[0009]                           Namepath : "."

//
// OEM Data Table:
//
// Signature: 0x55353631 '165U'
// BaseBaud: 0x0016DA00
// RegisterOffset: 0x0000
// RegisterShift: 0x0002
// Flags: 0x00000002
//

[0016]                           OEM Data : 31 36 35 55 00 DA 16 00 00 00 02 00 02 00 00 00

