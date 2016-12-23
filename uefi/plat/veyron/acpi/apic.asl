/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    apic.asl

Abstract:

    This module implements the Multiple APIC description table for the RK3288
    Veyron.

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

//
// ---------------------------------------------------------------- Definitions
//

[0004]                          Signature : "APIC"    [Multiple APIC Description Table (MADT)]
[0004]                       Table Length : 00000146
[0001]                           Revision : 01
[0001]                           Checksum : B0
[0006]                             Oem ID : "Minoca"
[0008]                       Oem Table ID : "Minoca  "
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20110623

[0004]                 Local Apic Address : FFC02000
[0004]              Flags (decoded below) : 00000000
                      PC-AT Compatibility : 0

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 28
[0002]                           Reserved : 0000
[0004]              Local GIC Hardware ID : 00000500
[0004]                      Processor UID : 00000000
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 00000000
[0008]                     Parked Address : 0000000000080000
[0008]                       Base Address : 0000000000000000

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 28
[0002]                           Reserved : 0000
[0004]              Local GIC Hardware ID : 00000501
[0004]                      Processor UID : 00000001
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 00000000
[0008]                     Parked Address : 0000000000081000
[0008]                       Base Address : 0000000000000000

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 28
[0002]                           Reserved : 0000
[0004]              Local GIC Hardware ID : 00000502
[0004]                      Processor UID : 00000002
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 00000000
[0008]                     Parked Address : 0000000000082000
[0008]                       Base Address : 0000000000000000

[0001]                      Subtable Type : 0B [Generic Interrupt Controller]
[0001]                             Length : 28
[0002]                           Reserved : 0000
[0004]              Local GIC Hardware ID : 00000503
[0004]                      Processor UID : 00000003
[0004]              Flags (decoded below) : 00000001
                        Processor Enabled : 1
[0004]           Parking Protocol Version : 00000000
[0004]              Performance Interrupt : 00000000
[0008]                     Parked Address : 0000000000083000
[0008]                       Base Address : 0000000000000000

[0001]                      Subtable Type : 0C [Generic Interrupt Distributor]
[0001]                             Length : 18
[0002]                           Reserved : 0000
[0004]              Local GIC Hardware ID : 00000000
[0008]                       Base Address : 00000000FFC01000
[0004]                     Interrupt Base : 00000000
[0004]                           Reserved : 00000000

