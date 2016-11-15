/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am33.asl

Abstract:

    This module implements the platform specific TI AM335x table.

Author:

    Evan Green 6-Jan-2015

Environment:

    Firmware

--*/

//
// ---------------------------------------------------------------- Definitions
//

[0004]                          Signature : "AM33"
[0004]                       Table Length : 00000098
[0001]                           Revision : 01
[0001]                           Checksum : 00
[0006]                             Oem ID : "Minoca"
[0008]                       Oem Table ID : "Minoca  "
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20110623

//
// Timer physical addresses
//

                                   UINT64 : 0000000044E05000
                                   UINT64 : 0000000044E31000
                                   UINT64 : 0000000048040000
                                   UINT64 : 0000000048042000
                                   UINT64 : 0000000048044000
                                   UINT64 : 0000000048046000
                                   UINT64 : 0000000048048000
                                   UINT64 : 000000004804A000

//
// Timer GSIs
//

                                   UINT32 : 00000042
                                   UINT32 : 00000043
                                   UINT32 : 00000044
                                   UINT32 : 00000045
                                   UINT32 : 0000005C
                                   UINT32 : 0000005D
                                   UINT32 : 0000005E
                                   UINT32 : 0000005F

//
// Interrupt line count
//

                                   UINT32 : 00000080

//
// Interrupt controller base
//

                                   UINT64 : 0000000048200000

//
// PRCM base
//

                                   UINT64 : 0000000044E00000

