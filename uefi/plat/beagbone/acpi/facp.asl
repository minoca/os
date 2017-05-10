/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    facp.asl

Abstract:

    This module implements the Fixed ACPI Description Table (FADT).

Author:

    Evan Green 26-Mar-2014

Environment:

    Firmware

--*/

//
// ---------------------------------------------------------------- Definitions
//

[0004]                          Signature : "FACP"    [Fixed ACPI Description Table (FADT)]
[0004]                       Table Length : 0000010C
[0001]                           Revision : 05
[0001]                           Checksum : 64
[0006]                             Oem ID : "Minoca"
[0008]                       Oem Table ID : "Minoca  "
[0004]                       Oem Revision : 00000000
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20131115

[0004]                       FACS Address : 00000000
[0004]                       DSDT Address : 00000001
[0001]                              Model : 00
[0001]                         PM Profile : 00 [Unspecified]
[0002]                      SCI Interrupt : 0000
[0004]                   SMI Command Port : 00000000
[0001]                  ACPI Enable Value : 00
[0001]                 ACPI Disable Value : 00
[0001]                     S4BIOS Command : 00
[0001]                    P-State Control : 00
[0004]           PM1A Event Block Address : 00000000
[0004]           PM1B Event Block Address : 00000000
[0004]         PM1A Control Block Address : 00000000
[0004]         PM1B Control Block Address : 00000000
[0004]          PM2 Control Block Address : 00000000
[0004]             PM Timer Block Address : 00000000
[0004]                 GPE0 Block Address : 00000000
[0004]                 GPE1 Block Address : 00000000
[0001]             PM1 Event Block Length : 00
[0001]           PM1 Control Block Length : 00
[0001]           PM2 Control Block Length : 00
[0001]              PM Timer Block Length : 00
[0001]                  GPE0 Block Length : 00
[0001]                  GPE1 Block Length : 00
[0001]                   GPE1 Base Offset : 00
[0001]                       _CST Support : 00
[0002]                         C2 Latency : 0000
[0002]                         C3 Latency : 0000
[0002]                     CPU Cache Size : 0000
[0002]                 Cache Flush Stride : 0000
[0001]                  Duty Cycle Offset : 00
[0001]                   Duty Cycle Width : 00
[0001]                RTC Day Alarm Index : 00
[0001]              RTC Month Alarm Index : 00
[0001]                  RTC Century Index : 00
[0002]         Boot Flags (decoded below) : 00
            Legacy Devices Supported (V2) : 0
         8042 Present on ports 60/64 (V2) : 0
                     VGA Not Present (V4) : 1
                   MSI Not Supported (V4) : 1
             PCIe ASPM Not Supported (V4) : 1
                CMOS RTC Not Present (V5) : 1
[0001]                           Reserved : 00
[0004]              Flags (decoded below) : 00100000
   WBINVD instruction is operational (V1) : 0
           WBINVD flushes all caches (V1) : 0
                 All CPUs support C1 (V1) : 0
               C2 works on MP system (V1) : 0
         Control Method Power Button (V1) : 0
         Control Method Sleep Button (V1) : 0
     RTC wake not in fixed reg space (V1) : 0
         RTC can wake system from S4 (V1) : 0
                     32-bit PM Timer (V1) : 0
                   Docking Supported (V1) : 0
            Reset Register Supported (V2) : 0
                         Sealed Case (V3) : 0
                 Headless - No Video (V3) : 0
     Use native instr after SLP_TYPx (V3) : 0
           PCIEXP_WAK Bits Supported (V4) : 0
                  Use Platform Timer (V4) : 0
            RTC_STS valid on S4 wake (V4) : 0
             Remote Power-on capable (V4) : 0
              Use APIC Cluster Model (V4) : 0
  Use APIC Physical Destination Mode (V4) : 0
                    Hardware Reduced (V5) : 1
                   Low Power S0 Idle (V5) : 0

[0012]                     Reset Register : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 08
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0001]               Value to cause reset : 00
[0003]                           Reserved : 000000
[0008]                       FACS Address : 0000000000000000
[0008]                       DSDT Address : 0000000000000001
[0012]                   PM1A Event Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                   PM1B Event Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                 PM1A Control Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                 PM1B Control Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                  PM2 Control Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                     PM Timer Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                         GPE0 Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]                         GPE1 Block : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]             Sleep Control Register : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

[0012]              Sleep Status Register : [Generic Address Structure]
[0001]                           Space ID : 01 [SystemIO]
[0001]                          Bit Width : 00
[0001]                         Bit Offset : 00
[0001]               Encoded Access Width : 00 [Undefined/Legacy]
[0008]                            Address : 0000000000000000

