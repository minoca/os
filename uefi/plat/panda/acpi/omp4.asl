/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omp4.asl

Abstract:

    This module implements the platform specific TI OMAP4 table.

Author:

    Evan Green 31-Mar-2014

Environment:

    Firmware

--*/

//
// ---------------------------------------------------------------- Definitions
//

[0004]                          Signature : "OMP4"
[0004]                       Table Length : 000000D0
[0001]                           Revision : 01
[0001]                           Checksum : 00
[0006]                             Oem ID : "Minoca"
[0008]                       Oem Table ID : "Minoca  "
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20110623

//
// Timer physical addresses.
//

                                   UINT64 : 000000004A318000
                                   UINT64 : 0000000048032000
                                   UINT64 : 0000000048034000
                                   UINT64 : 0000000048036000
                                   UINT64 : 0000000040138000
                                   UINT64 : 000000004013A000
                                   UINT64 : 000000004013C000
                                   UINT64 : 000000004013E000
                                   UINT64 : 000000004803E000
                                   UINT64 : 0000000048086000
                                   UINT64 : 0000000048088000

//
// Timer GSIs.
//

                                   UINT32 : 00000045
                                   UINT32 : 00000046
                                   UINT32 : 00000047
                                   UINT32 : 00000048
                                   UINT32 : 00000049
                                   UINT32 : 0000004A
                                   UINT32 : 0000004B
                                   UINT32 : 0000004C
                                   UINT32 : 0000004D
                                   UINT32 : 0000004E
                                   UINT32 : 0000004F

//
// Debug UART physical address.
//

                                   UINT64 : 0000000048020000

//
// Wakeup clock physical address.
//

                                   UINT64 : 000000004A307800

//
// L4 clock physical address.
//

                                   UINT64 : 000000004A009400

//
// Audio clock physical address.
//

                                   UINT64 : 000000004A004500

//
// PL310 register base address.
//

                                   UINT64 : 0000000048242000
