/*++

Copyright (c) 2014 Evan Green

Module Name:

    dsdt.dsl

Abstract:

    This module implements the ACPI Differentiated System Descriptor Table
    (DSDT) for the Integrator/CP board.

Author:

    Evan Green 22-Sep-2013

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
        Device(KBD) {
            Name(_HID, "APL0050")
            Name(_UID, 0)
            Method (_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(
                    ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                    NonCacheable, ReadWrite,
                    0x00000000,
                    0x18000000,
                    0x18000FFF,
                    0x00000000,
                    0x00001000)

                Interrupt(, Level, ActiveHigh,) {3}
            })
        }

        Device(MOU) {
            Name(_HID, "APL0050")
            Name(_UID, 0)
            Method (_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(
                    ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                    NonCacheable, ReadWrite,
                    0x00000000,
                    0x19000000,
                    0x19000FFF,
                    0x00000000,
                    0x00001000)

                Interrupt(, Level, ActiveHigh,) {4}
            })
        }

        Device(NIC) {
            Name(_HID, "SMC91C1")
            Name(_UID, 0)
            Method (_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(
                    ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                    NonCacheable, ReadWrite,
                    0x00000000,
                    0xC8000000,
                    0xC8000FFF,
                    0x00000000,
                    0x00001000)

                Interrupt(, Level, ActiveHigh,) {27}
            })
        }
    }

    Name (\_S3, Package (0x04) {
        0x01,
        0x01,
        Zero,
        Zero
    })

    Name (\_S4, Package (0x04) {
        Zero,
        Zero,
        Zero,
        Zero
    })

    Name (\_S5, Package (0x04) {
        Zero,
        Zero,
        Zero,
        Zero
    })
}

