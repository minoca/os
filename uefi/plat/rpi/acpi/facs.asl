/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    facs.asl

Abstract:

    This module implements the Firmware ACPI Control Structure table.

Author:

    Evan Green 26-Mar-2014

Environment:

    Firmware

--*/

//
// ---------------------------------------------------------------- Definitions
//

[0004]                          Signature : "FACS"
[0004]                             Length : 00000040
[0004]                 Hardware Signature : 00000000
[0004]          32 Firmware Waking Vector : 00000000
[0004]                        Global Lock : 00000000
[0004]              Flags (decoded below) : 00000000
                   S4BIOS Support Present : 0
               64-bit Wake Supported (V2) : 0
[0008]          64 Firmware Waking Vector : 0000000000000000
[0001]                            Version : 02
[0003]                           Reserved : 000000
[0004]          OspmFlags (decoded below) : 00000000
            64-bit Wake Env Required (V2) : 0

