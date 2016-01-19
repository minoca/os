/*++

Copyright (c) 2014 Evan Green

Module Name:

    dsdt.dsl

Abstract:

    This module implements the ACPI Differentiated System Descriptor Table
    (DSDT) for the Texas Instruments PandaBoard.

Author:

    Evan Green 26-Mar-2014

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
        Device(EHCI) {
            Name(_HID, EISAID("PNP0D20"))
            Name(_UID, 0)
            Method (_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x4A064C00,
                            0x4A064FFF,
                            0x00000000,
                            0x00000400)

                Interrupt(, Level, ActiveHigh,) {109}
            })
        }

        Device(GPI1) {
            Name(_HID, "TEX4006")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x4A310000,
                            0x4A310FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {61}
            })
        }

        Device(GPI2) {
            Name(_HID, "TEX4006")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x48055000,
                            0x48055FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {62}
            })
        }

        Device(GPI3) {
            Name(_HID, "TEX4006")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x48057000,
                            0x48057FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {63}
            })
        }

        Device(GPI4) {
            Name(_HID, "TEX4006")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x48059000,
                            0x48059FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {64}
            })
        }

        Device(GPI5) {
            Name(_HID, "TEX4006")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x4805B000,
                            0x4805BFFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {65}
            })
        }

        Device(GPI6) {
            Name(_HID, "TEX4006")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x4805D000,
                            0x4805DFFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {66}
            })
        }

        Device(SDMC) {
            Name(_HID, "TEX4004")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x4809C000,
                            0x4809CFFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {115}
            })
        }

    }

    Name(\_S3, Package (0x04) {
        0x01,
        0x01,
        Zero,
        Zero
    })

    Name(\_S4, Package (0x04) {
        Zero,
        Zero,
        Zero,
        Zero
    })

    Name(\_S5, Package (0x04) {
        Zero,
        Zero,
        Zero,
        Zero
    })
}

