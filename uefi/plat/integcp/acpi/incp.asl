/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    incp.asl

Abstract:

    This module implements the platform specific Integrator/CP table.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

//
// ---------------------------------------------------------------- Definitions
//

[0004]                          Signature : "INCP"
[0004]                       Table Length : 00000058
[0001]                           Revision : 01
[0001]                           Checksum : 00
[0006]                             Oem ID : "Minoca"
[0008]                       Oem Table ID : "Minoca  "
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20110623

//
// PL110 Physical Address
//

                                   UINT64 : 00000000C0000000

//
// Interrupt controller physical address.
//

                                   UINT64 : 0000000014000000

//
// Interrupt controller GSI base.
//

                                   UINT32 : 00000000

//
// Timer physical address.
//

                                   UINT64 : 0000000013000000

//
// Timer GSIs.
//

                                   UINT32 : 00000005
                                   UINT32 : 00000006
                                   UINT32 : 00000007

//
// Debug UART physical address.
//

                                   UINT64 : 0000000016000000

//
// Debug UART frequency
//

                                   UINT32 : 00E10000

