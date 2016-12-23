/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information..

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

                //
                // PRCM
                //

                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x44E00000,
                            0x44E01FFF,
                            0x00000000,
                            0x00002000)

                //
                // CONTROL module
                //

                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x44E10000,
                            0x44E11FFF,
                            0x00000000,
                            0x00002000)

                //
                // Cortex M3 Code.
                //

                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x44D00000,
                            0x44D03FFF,
                            0x00000000,
                            0x00004000)

                //
                // Cortex M3 Data.
                //

                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x44D80000,
                            0x44D81FFF,
                            0x00000000,
                            0x00002000)

                //
                // Mailbox
                //

                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x480C8000,
                            0x480C8FFF,
                            0x00000000,
                            0x00001000)


                //
                // OCMC RAM
                //

                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x40300000,
                            0x4030FFFF,
                            0x00000000,
                            0x00010000)


                //
                // EMIF0
                //

                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x4C000000,
                            0x4C000FFF,
                            0x00000000,
                            0x00001000)

                //
                // Wake M3 interrupt
                //

                Interrupt(, Level, ActiveHigh,) {78}

                //
                // Mailbox interrupt
                //

                Interrupt(, Level, ActiveHigh,) {77}
            })
        }
    }

    Scope(\_SB.SOCD) {
        Device(EDMA) {
            Name(_HID, "TEX3006")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x49000000,
                            0x49005FFF,
                            0x00000000,
                            0x00006000)

                Interrupt(, Level, ActiveHigh,) {12} // Completion interrupt
                Interrupt(, Level, ActiveHigh,) {13} // Mem. Protect interrupt
                Interrupt(, Level, ActiveHigh,) {14} // Error interrupt
            })
        }

        Device(NIC0) {
            Name(_HID, "TEX3005")
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

    //
    // Stick things that use system DMA underneath the DMA controller.
    //

    Scope(\_SB.SOCD.EDMA) {
        Device(MMC0) {
            Name(_HID, "TEX3004")
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
                FixedDMA(24, 24, Width32Bit, ) // TX
                FixedDMA(25, 25, Width32Bit, ) // RX
            })
        }

        Device(MMC1) {
            Name(_HID, "TEX3004")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x481D8000,
                            0x481D8FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {28}
                FixedDMA(2, 2, Width32Bit, ) // TX
                FixedDMA(3, 3, Width32Bit, ) // RX
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

