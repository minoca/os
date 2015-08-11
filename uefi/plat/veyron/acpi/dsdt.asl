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
             * the receive FIFO to 520 bytes, the non-periodic transmit FIFO to
             * 128 bytes, and the periodic transmit FIFO to 256 bytes. The
             * Veyron's DWC USB controller allows dynamic FIFO sizes and the
             * maximum FIFO depth is greater than the total FIFO sizes 
             * programmed here.
             */
            
            Method(_INI, 0) {                        
                Store(0x208, RXFS)
                Store(0x208, NPFO)
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
            Name(_HID, "RK320D40")
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

