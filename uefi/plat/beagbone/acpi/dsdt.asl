/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved.

Module Name:

    dsdt.dsl

Abstract:

    This module implements the ACPI Differentiated System Descriptor Table
    (DSDT) for the Texas Instruments BeagleBone Black.

Author:

    Evan Green 6-Jan-2015

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
        Device(SOCD) {
            Name(_HID, "TEX3359")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x44E00000,
                            0x44E01FFF,
                            0x00000000,
                            0x00002000)

                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x44E10000,
                            0x44E11FFF,
                            0x00000000,
                            0x00002000)
            })
        }
    }

    Scope(\_SB.SOCD) {
        Device(SDMC) {
            Name(_HID, "TISD4502")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x48060000,
                            0x48060FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {64}
            })
        }

        Device(NIC0) {
            Name(_HID, "TIET3350")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x4A100000,
                            0x4A103FFF,
                            0x00000000,
                            0x00004000)

                Interrupt(, Level, ActiveHigh,) {42}
                Interrupt(, Level, ActiveHigh,) {41}
            })
        }

        Device(USB0) {
            Name(_HID, "TEX3003")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x47400000,
                            0x47407FFF,
                            0x00000000,
                            0x00008000)

                Interrupt(, Level, ActiveHigh,) {17}
                Interrupt(, Level, ActiveHigh,) {18}
                Interrupt(, Level, ActiveHigh,) {19}
            })
        }

        Device(I2C0) {
            Name(_HID, "TEX3001")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x44E0B000,
                            0x44E0BFFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {70}
            })

            Device(PMIC) {
                Name(_HID, "TEX3002")
                Name(_UID, 0)
                Method(_STA, 0, NotSerialized) {
                    Return(0x0F)
                }

                Name(_CRS, ResourceTemplate() {
                    I2CSerialBus(0x24, ControllerInitiated, 100000,
                                 AddressingMode7Bit, "\\_SB_SOCDI2C0", , , , )

                    Interrupt(, Level, ActiveLow,) {7}
                })
            }
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

