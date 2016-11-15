/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rk32.asl

Abstract:

    This module implements the platform specific RK32 table.

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

//
// ---------------------------------------------------------------- Definitions
//

[0004]                          Signature : "Rk32"
[0004]                       Table Length : 00000000
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

                                   UINT64 : 00000000FF6B0000
                                   UINT64 : 00000000FF6B0020
                                   UINT64 : 00000000FF6B0040
                                   UINT64 : 00000000FF6B0060
                                   UINT64 : 00000000FF6B0080
                                   UINT64 : 00000000FF6B00A0
                                   UINT64 : 00000000FF810000
                                   UINT64 : 00000000FF810020
//
// Timer GSIs.
//

                                   UINT32 : 00000062
                                   UINT32 : 00000063
                                   UINT32 : 00000064
                                   UINT32 : 00000065
                                   UINT32 : 00000066
                                   UINT32 : 00000067
                                   UINT32 : 00000068
                                   UINT32 : 00000069

//
// Timer count-down mask. Timers 5 and 7 count up.
//

                                   UINT32 : 0000005F

//
// Timer enabled mask. Enable all for use by the OS.
//

                                   UINT32 : 000000FF

