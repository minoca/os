/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved.

Module Name:

    dsdt.dsl

Abstract:

    This module implements the ACPI Differentiated System Descriptor Table
    (DSDT) for the Broadcom 2836.

Author:

    Chris Stevens 17-Mar-2014

Environment:

    Firmware

--*/

//
// --------------------------------------------------------------------- Tables
//

DefinitionBlock (
    "dsdt.aml",
    "DSDT",
    0x01,
    "Minoca",
    "Minoca  ",
    0x1
    )

{
    Scope(\_SB) {
        Device(DWHC) {
            Name(_HID, EISAID("DWC0000"))
            Name(_UID, 0)
            Method (_STA, 0, NotSerialized) {
                    Return(0x0F)
            }
            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x3F980000,
                            0x3F990FFF,
                            0x00000000,
                            0x00011000
                )
                Interrupt(, Level, ActiveHigh,) { 9 }
            })
        }

        Device(SDMC) {
            Name(_HID, EISAID("BCM0D40"))
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x3F300000,
                            0x3F300FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {62}
            })
        }
    }

    Name (\_S3, Package (0x04)
    {
        0x01,
        0x01,
        Zero,
        Zero
    })
    Name (\_S4, Package (0x04)
    {
        Zero,
        Zero,
        Zero,
        Zero
    })
    Name (\_S5, Package (0x04)
    {
        Zero,
        Zero,
        Zero,
        Zero
    })
}
