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
