/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved.

Module Name:

    dsdt.dsl

Abstract:

    This module implements the ACPI Differentiated System Descriptor Table
    (DSDT) for the Texas Instruments PandaBoard.

Author:

    Evan Green 10-Jul-2015

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
        Device(DWC0) {
            Name(_HID, EISAID("DWC0000"))
            Name(_UID, 0)

            /*
             * Define the operation region to access the DWC configuration
             * space.
             */

            OperationRegion(DWCR, SystemMemory, 0xFF580000, 0x104)
            Field(DWCR, DWordAcc, NoLock, Preserve) {
                Offset(0x8),
                SKP1, 1,
                AHBB, 3,
                AHBW, 1,
                Offset(0xC),
                SKP2, 8,
                USRP, 1,
                UHNP, 1,
                Offset(0x24),
                RXFS, 16,
                Offset(0x28),
                NPFO, 16,
                NPFS, 16,
                Offset(0x100),
                PDFO, 16,
                PDFS, 16,
            }

            /*
             * Set the AHB configuration register to have a burst length of 16,
             * the receive FIFO to 516 bytes, the non-periodic transmit FIFO to
             * 128 bytes, and the periodic transmit FIFO to 256 bytes. The
             * Veyron's DWC USB controller allows dynamic FIFO sizes and the
             * maximum FIFO depth is greater than the total FIFO sizes
             * programmed here. Despite the Hardware 2 Configuration register's
             * claims that this device supports SRP and HNP, it does not. Also
             * clear those bits in the USB configuration register.
             */

            Method(_INI, 0) {
                Store(0x204, RXFS)
                Store(0x204, NPFO)
                Store(0x80, NPFS)
                Store(0x288, PDFO)
                Store(0x100, PDFS)
                Store(0x0, USRP)
                Store(0x0, UHNP)
                Store(0x7, AHBB)
                Store(0x0, AHBW)
            }

            Method (_STA, 0, NotSerialized) {
                    Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF580000,
                            0xFF590FFF,
                            0x00000000,
                            0x00011000
                )
                Interrupt(, Level, ActiveHigh,) { 55 }
            })
        }

        Device(DWC1) {
            Name(_HID, EISAID("DWC0000"))
            Name(_UID, 0)

            /*
             * Define the operation region to access the DWC configuration
             * space.
             */

            OperationRegion(DWCR, SystemMemory, 0xFF540000, 0x104)
            Field(DWCR, DWordAcc, NoLock, Preserve) {
                Offset(0x8),
                SKP1, 1,
                AHBB, 3,
                AHBW, 1,
                Offset(0xC),
                SKP2, 8,
                USRP, 1,
                UHNP, 1,
                Offset(0x24),
                RXFS, 16,
                Offset(0x28),
                NPFO, 16,
                NPFS, 16,
                Offset(0x100),
                PDFO, 16,
                PDFS, 16,
            }

            /*
             * Set the AHB configuration register to have a burst length of 16,
             * the receive FIFO to 516 bytes, the non-periodic transmit FIFO to
             * 128 bytes, and the periodic transmit FIFO to 256 bytes. The
             * Veyron's DWC USB controller allows dynamic FIFO sizes and the
             * maximum FIFO depth is greater than the total FIFO sizes
             * programmed here. This controller accurately describes its mode.
             * The USB configuration register does not need modifications.
             */

            Method(_INI, 0) {
                Store(0x204, RXFS)
                Store(0x204, NPFO)
                Store(0x80, NPFS)
                Store(0x288, PDFO)
                Store(0x100, PDFS)
                Store(0x7, AHBB)
                Store(0x0, AHBW)
            }

            Method (_STA, 0, NotSerialized) {
                    Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF540000,
                            0xFF550FFF,
                            0x00000000,
                            0x00011000
                )
                Interrupt(, Level, ActiveHigh,) { 57 }
            })
        }

        Device(SDMC) {
            Name(_HID, "RKC0D40")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF0C0000,
                            0xFF0C0FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {64}
            })
        }

        Device(EMMC) {
            Name(_HID, "RKC0D40")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF0F0000,
                            0xFF0F0FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {67}
            })
        }

        Device(GPI0) {
            Name(_HID, "RKC0002")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF750000,
                            0xFF750FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {113}
            })
        }

        Device(GPI1) {
            Name(_HID, "RKC0002")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF780000,
                            0xFF780FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {114}
            })
        }

        Device(GPI2) {
            Name(_HID, "RKC0002")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF790000,
                            0xFF790FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {115}
            })
        }

        Device(GPI3) {
            Name(_HID, "RKC0002")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF7A0000,
                            0xFF7A0FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {116}
            })
        }

        Device(GPI4) {
            Name(_HID, "RKC0002")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF7B0000,
                            0xFF7B0FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {117}
            })
        }

        Device(GPI5) {
            Name(_HID, "RKC0002")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF7C0000,
                            0xFF7C0FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {118}
            })
        }

        Device(GPI6) {
            Name(_HID, "RKC0002")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF7D0000,
                            0xFF7D0FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {119}
            })
        }

        Device(GPI7) {
            Name(_HID, "RKC0002")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF7E0000,
                            0xFF7E0FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {120}
            })
        }

        Device(GPI8) {
            Name(_HID, "RKC0002")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF7F0000,
                            0xFF7F0FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {121}
            })
        }

        Device(SPI0) {
            Name(_HID, "RKC0001")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0xFF110000,
                            0xFF110FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {76}
            })

            Device(GOEC) {
                Name(_HID, "GOO0001")
                Name(_UID, 0)
                Method(_STA, 0, NotSerialized) {
                    Return(0x0F)
                }

                Name(_CRS, ResourceTemplate() {
                    SPISerialBus(0x1, , , 8, , 3000000, ClockPolarityLow,
                                 ClockPhaseFirst, "\\_SB_SPI0", , ,)

                    GpioInt(Level, ActiveLow, Shared, PullUp, ,
                            "\\_SB_GPI7") {7}
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

