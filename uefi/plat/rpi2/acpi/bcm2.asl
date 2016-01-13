/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    bcm2.asl

Abstract:

    This module implements the platform specific BCM2709 table.

Author:

    Chris Stevens 17-Mar-2015

Environment:

    Firmware

--*/

//
// ---------------------------------------------------------------- Definitions
//

[0004]                          Signature : "BCM2"
[0004]                       Table Length : 00000070
[0001]                           Revision : 01
[0001]                           Checksum : 00
[0006]                             Oem ID : "Minoca"
[0008]                       Oem Table ID : "Minoca  "
[0004]                       Oem Revision : 00000001
[0004]                    Asl Compiler ID : "INTL"
[0004]              Asl Compiler Revision : 20110623

//
// APB clock frequency.
//

                                   UINT64 : 000000000EE6B280

//
// Interrupt controller physical address.
//

                                   UINT64 : 000000003F00B200

//
// Interrupt controller GSI base.
//

                                   UINT64 : 0000000000000000

//
// ARM Timer physical address.
//

                                   UINT64 : 000000003F00B400

//
// ARM Timer GSI.
//

                                   UINT32 : 00000040

//
// Debug UART physical address.
//

                                   UINT64 : 000000003F201000

//
// Debug UART clock frequency.
//

                                   UINT32 : 002DC6C0

//
// System Timer physical address.
//

                                   UINT64 : 000000003F003000

//
// System Timer frequency.
//

                                   UINT64 : 00000000000F4240

//
// System Timer GSI Base.
//

                                   UINT32 : 00000000

//
// Mailbox physical address.
//

                                   UINT64 : 000000003F00B880
