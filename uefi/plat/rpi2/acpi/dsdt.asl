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

            /*
             * Define the operation region to access the DWC configuration
             * space.
             */

            OperationRegion(DWCR, SystemMemory, 0x3F980000, 0x104)
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
             * Set the AHB configuration register to have a single burst length
             * and to wait on all writes. Also set the receive FIFO to 774
             * bytes, the non-periodic transmit FIFO to 256 bytes, and the
             * periodic transmit FIFO to 512 bytes. The Raspberry Pi's DWC USB
             * controller allows dynamic FIFO sizes and the maximum FIFO depth
             * is greater than the total FIFO sizes programmed here. Lastly,
             * the host is both SRP and HNP capable.
             */

            Method(_INI, 0) {
                Store(0x306, RXFS)
                Store(0x306, NPFO)
                Store(0x100, NPFS)
                Store(0x406, PDFO)
                Store(0x200, PDFS)
                Store(0x1, USRP)
                Store(0x1, UHNP)
                Store(0x0, AHBB)
                Store(0x1, AHBW)
            }

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

        Device(BDMA) {
            Name(_HID, "BCM0000")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x3F007000,
                            0x3F007FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {16}
                Interrupt(, Level, ActiveHigh,) {17}
                Interrupt(, Level, ActiveHigh,) {18}
                Interrupt(, Level, ActiveHigh,) {19}
                Interrupt(, Level, ActiveHigh,) {20}
                Interrupt(, Level, ActiveHigh,) {21}
                Interrupt(, Level, ActiveHigh,) {22}
                Interrupt(, Level, ActiveHigh,) {23}
                Interrupt(, Level, ActiveHigh,) {24}
                Interrupt(, Level, ActiveHigh,) {25}
                Interrupt(, Level, ActiveHigh,) {26}
                Interrupt(, Level, ActiveHigh,) {27}
                Interrupt(, Level, ActiveHigh,) {28}
            })
        }

        Device(GPI0) {
            Name(_HID, "BCM0001")
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x3F200000,
                            0x3F200FFF,
                            0x00000000,
                            0x00001000)

                Interrupt(, Level, ActiveHigh,) {52}
            })
        }
    }

    //
    // Stick things that use system DMA underneath the DMA controller.
    //

    Scope(\_SB.BDMA) {
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
                FixedDMA(11, 0, Width32Bit, )
            })
        }

        Device(BPWM) {
            Name(_HID, EISAID("BCM0002"))
            Name(_UID, 0)
            Method(_STA, 0, NotSerialized) {
                Return(0x0F)
            }

            Name(_CRS, ResourceTemplate() {
                DWordMemory(ResourceConsumer, PosDecode, MinFixed, MaxFixed,
                            NonCacheable, ReadWrite,
                            0x00000000,
                            0x3F20C000,
                            0x3F20CFFF,
                            0x00000000,
                            0x00001000)

                //
                // PWM owns request line 5, but also must use channel 5.
                //

                FixedDMA(5, 5, Width32Bit, )
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
