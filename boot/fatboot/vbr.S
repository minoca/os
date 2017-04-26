/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    vbr.S

Abstract:

    This module implements the Volume Boot Record code used to bootstrap the
    operating system. This code lives on the first sector of the partition or
    disk.

Author:

    Evan Green 19-Jun-2012

Environment:

    MBR

--*/

##
## ---------------------------------------------------------------- Definitions
##

##
## Define the segment where the BIOS loads this MBR code.
##

.equ BOOT_ADDRESS,          0x7C00

##
## Define the boot stack, with some room for locals.
##

.equ BOOT_STACK,            (BOOT_ADDRESS - 0x8)
.equ STACK_SIZE,            0x3000

##
## Define the size of a disk sector.
##

.equ SECTOR_SIZE,           0x200

##
## Define the physical address where the second stage loader is loaded.
##

.equ LOADER_ADDRESS,        0x7E00

##
## Define the segment descriptors within the initial GDT.
##

.equ CS_DESCRIPTOR,         0x08
.equ DS_DESCRIPTOR,         0x10

##
## ----------------------------------------------------------------------- Code
##

##
## .text specifies that this code belongs in the executable section. This is
## the only section in the MBR code, data also lives in the text section.
##
## .code16 specifies that this is 16-bit real mode code.
##

.text
.code16

##
## Stick this in the .init section so it ends up at the front of the binary.
##

.section .init

##
## .globl allows this label to be visible to the linker.
##

.globl _start

_start:
    jmp     AfterFileSystemParameters   # Jump over the FS parameters next.
    nop                                 # A free byte. Go nuts!

##
## These are the well defined locations where the LBA of this sector lives. If
## this is a partition, then this contains the partition offset.
##

.org 0x5C

BootCodeStartSector:
    .long 0

BootCodeSectorCount:
    .byte 0

AfterFileSystemParameters:
    cli                                 # Disable interrupts for stack setup.
    xorl    %eax, %eax                  # Zero out AX.
    movl    %eax, %esp                  # Zero out ESP.
    movw    %ax, %ds                    # Zero out DS.
    movw    %ax, %es                    # Zero out ES.
    movw    %ax, %ss                    # Zero out SS.
    movw    $BOOT_STACK, %si            # Load SI with the stack pointer.
    movw    %si, %sp                    # Set it as the stack.
    movw    %si, %bp                    # Also set it as the base.
    sti                                 # Re-enable interrupts.

    ##
    ## Check to see if INT 13 extensions are supported. If CF is cleared, BX
    ## changes to 0xAA55, and the lowest bit in CX is set, then they are
    ## supported.
    ##

    movb    %dl, (%bp)                  # Save the drive number.
    movb    $0x41, %ah                  # Set AH.
    movw    $0x55AA, %bx                # Set BX to the magic value.
    int     $0x13                       # Call the BIOS.
    jb      DiskReadError               # Fail if CF is set.
    cmpw    $0xAA55, %bx                # See if BX changed to 0xAA55.
    jnz     DiskReadError               # Fail if BX wasn't set to the magic.
    test    $0x0001, %cx                # See if the lowest bit of CX is set.
    jz      DiskReadError               # Fail if lowest bit of CX is not set.

    ##
    ## Get the boot sector offset and count.
    ##

    xorw    %cx, %cx
    movb    BootCodeSectorCount, %cl    # Get the sector count.
    cmpb    $0, %cl                     # Compare to zero.
    jz      BootCodeFailure             # Fail if a count is not set.
    movl    BootCodeStartSector, %eax   # Load the start sector.
    incl    %eax

    ##
    ## Perform an INT 13 extended read of the boot sectors. The extended read
    ## takes a disk packet that looks like this:
    ## Offset 0, size 1: Size of packet (16 or 24 bytes).
    ## Offset 1, size 1: Reserved (0).
    ## Offset 2, size 2: Number of blocks to transfer.
    ## Offset 4, size 4: Transfer buffer.
    ## Offset 8, size 8: Absolute starting sector.
    ## Offset 0x10, size 8: 64-bit flat address of transfer buffer. Only used
    ## if the value at offset 4 is 0xFFFFFFFF (which it is not in this case).
    ##
    ## Remember that things need to be pushed on in reverse.
    ##

    pushl   $0                          # Push starting sector high.
    pushl   %eax                        # Push the starting sector low.
    pushl   $LOADER_ADDRESS             # Push the transfer buffer.
    pushw   %cx                         # Push the sector count.
    pushw   $0x0010                     # Push reserved and packet size.
    movb    $0x42, %ah                  # Function 0x42, extended read.
    movb    (%bp), %dl                  # Load the drive number.
    movw    %sp, %si                    # SI points to the disk packet address.
    int     $0x13                       # Read the first sector.
    jb      DiskReadError               # Fail if it didn't read.
    addw    $0x10, %sp                  # Pop the parameters off.

    ##
    ## Enable the A20 address line. The 8088 processor in the original PC only
    ## had 20 address lines, and when it reached its topmost address at 1MB it
    ## would silently wrap around. When the 80286 was released it was designed
    ## to be compatible with programs written for the 8088, which had come to
    ## rely on this wrapping behavior. The 8042 keyboard controller had an
    ## extra pin, so the gate to control the A20 line behavior on newer (286)
    ## processors was stored there. The keyboard status register is read from
    ## port 0x64. If bit 2 is set (00000010b) the input buffer is full, and the
    ## controller is not accepting commands. The CPU can control the keyboard
    ## controller by writing to port 0x64. Writing 0xd1 corresponds to "Write
    ## Output port". Writing 0xDF to port 0x60, the keyboard command register,
    ## tells the controller to enable the A20 line (as it sets bit 2, which
    ## controlled that line).
    ##

    call    WaitForKeyboardController   # Wait for not busy.
    jnz     AfterA20Line                # Jump out if stuck.
    cli                                 # Disable interrupts.
    movb    $0xD1, %al                  # Send command 'Write output port'.
    outb    %al, $0x64                  # Write 0xD1 to port 0x64.
    call    WaitForKeyboardController   # Wait for not busy.
    movb    $0xDF, %al                  # Set the mask.
    outb    %al, $0x60                  # Write 0xDF to port 0x60.
    call    WaitForKeyboardController   # Wait for not busy again.
    movb    $0xFF, %al                  # Clear the command.
    outb    %al, $0x64                  # Write 0xFF to port 0x60.
    call    WaitForKeyboardController   # One last time.
    sti                                 # Re-enable interrupts.

AfterA20Line:

    ##
    ## Jump off to the remainder of the boot sectors.
    ##

    movb    (%bp), %dl                  # Load the drive number.
    jmp     $0, $LOADER_ADDRESS         # Rock and roll.

    ##
    ## In error cases down here, print a message and die sadly.
    ##

DiskReadError:
    movw    $ReadFailureMessage, %si
    jmp     PrintStringAndDie

BootCodeFailure:
    movw    $BootCodeFailureMessage, %si
    jmp     PrintStringAndDie

    ##
    ## Print a null-terminated string, and then end it.
    ##

PrintStringAndDie:
    cld
    lodsb                               # Load SI into AL.
    cmp     $0, %al                     # Is this the null terminator?
    jz      Die                         # If so, then go quietly into the night.
    movw    $0x0007, %bx                # Display normal white on black.
    movb    $0x0E, %ah                  # Print character function.
    int     $0x10                       # Print the character.
    jmp     PrintStringAndDie           # Loop printing more characters.

    ##
    ## This is the end of the line.
    ##

Die:
    hlt                                 # Stop.
    jmp     Die                         # Die again forever.

##
## This routine waits for the keyboard controller's busy bit to clear.
##

WaitForKeyboardController:
    xorw    %cx, %cx                    # Zero out CX.

WaitForKeyboardControllerLoop:
    inb     $0x64, %al                  # Read port 0x64.
    jmp     WaitForKeyboardControllerDelay  # Add in a tiny bit of delay.

WaitForKeyboardControllerDelay:
    and     $0x2, %al                   # Isolate the second bit.
    loopne  WaitForKeyboardControllerLoop   # Loop if it's still set.
    and     $0x2, %al                   # Set the flags for return.
    ret                                 # Return.

##
## Define the messages that are printed from the MBR.
##

ReadFailureMessage:
    .asciz "Disk Read Error"

BootCodeFailureMessage:
    .asciz "Boot code lost"

##
## Define the signature that the BIOS uses to determine this disk is bootable.
##

.org 0x1FE

    .byte   0x55
    .byte   0xAA

##
## This portion onward is on the second sector. It is loaded by the first
## sector above, it is the destination of LOADER_ADDRESS.
##

.org 0x200

    ##
    ## Clear the screen, since it's easy here.
    ##

    pushw   %dx
    movw    $0x0003, %ax                # BH = Clear Screen Function. BL = Text.
    int     $0x10                       # Clear the screen.
    popw    %dx

##
## Prepare to switch from 16-bit Real Mode to 32-bit Protected Mode.
## Segment registers turn into descriptors (indices into the Global
## Descriptor Table) which must be set up prior to switching into protected
## mode.
##

    cli                                 # Disable interrupts.
    lgdt    Gdt                         # Load the GDT.
    movl    %cr0, %eax                  # Get the CR0 control register.
    orl     $0x1, %eax                  # Set bit 0 to enable Protected Mode.
    movl    %eax, %cr0                  # Set CR0.
    movw    $DS_DESCRIPTOR, %ax         # Load the data descriptor.
    movw    %ax, %ds                    # Set the data segment.
    movw    %ax, %ss                    # Set the stack segment as well.
    movw    %ax, %es                    # Set up ES.
    movw    %ax, %fs                    # Set up FS.
    movw    %ax, %gs                    # Set up GS.

##
## A long jump is required to complete the switch to protected mode. Long jumps
## specify the segment descriptor and an offset from that segment.
##

    ljmp   $CS_DESCRIPTOR, $ProtectedModeCode

##
## The .code32 assembler directive lets the compiler know that it should
## generate 32-bit instructions. Load the segment descriptors, set up the
## initial stack, and jump to loader code.
##

.code32

ProtectedModeCode:
    andl    $0x000000FF, %edx           # Clear all but dl.
    pushl   %edx                        # Push boot drive number.
    movl    BootCodeStartSector, %eax   # Load the start sector.
    pushl   $0                          # Push the high partition offset.
    pushl   %eax                        # Push the low partition offset.
    pushl   $STACK_SIZE                 # Push stack size parameter.
    pushl   $BOOT_STACK                 # Push stack location.
    xorl    %ebp, %ebp                  # Zero out stack base.

##
## Jump to the loader image. Control will not return from here.
##

    call    BoMain                      # Control should not return... but
    int     $0x18                       # if it does, "Press a key to reboot".
    jmp     Die                         # And loop forever.

##
## --------------------------------------------------------- Internal Functions
##

##
## ---------------------------------------------------------------- Definitions
##

##
## The GDT must be loaded from an address aligned on 8 bytes.
##

.align 8

##
## GDT Table. The GDT table sets up the segmentation features of the processor
## and privilege levels. In this case set up two descriptors, a code segment
## and a data segment, both of which simply span the entire address space.
## This way the kernel essentially has full control over the whole address
## space. A GDT entry is 8 bytes long, and looks as follows:
##     USHORT limit_low;       // The lower 16 bits of the limit.
##     USHORT base_low;        // The lower 16 bits of the base.
##     UCHAR base_middle;      // The next 8 bits of the base.
##     UCHAR access;           // Access flags, described below.
##     UCHAR granularity;      // Defined below.
##     UCHAR base_high;        // The high 8 bits of the base.
##
## The granularity byte has the following fields:
##     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
##     |     |     |     |     |                       |
##     |  G  |  D  |  0  |  A  | Segment length 19:16  |
##
##     G - Granularity. 0 = 1 byte, 1 = 1 KByte.
##
##     D - Operand Size. 0 = 16 bit, 1 = 32 bit.
##
##     0 - Always zero.
##
##     A - Available for system use (always zero).
##
## The access byte has the following fields:
##     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
##     |     |           |     |                       |
##     |  P  |    DPL    | DT  |         Type          |
##
##     P - Is segment present (1 = Yes)
##
##     DPL - Descriptor privilege level: Ring 0-3. Zero is the highest
##           privilege, 3 is the lowest (least privileged).
##
##     Type - Segment type: code segment / data segment.
##

Gdt:
    .word   (3 * 8) - 1                         # GDT table limit
    .long   GdtTable                            # GDT table location

GdtTable:
    .long   0x0                         # The first GDT entry is called the
    .long   0x0                         # null descriptor, it is essentially
                                        # unused by the processor.

##
## Kernel code segment descriptor
##

    .word   0xFFFF                      # Limit 15:0
    .word   0x0                         # Base 15:0
    .byte   0x0                         # Base 23:16
    .byte   0x9A                        # Access: Present, Ring 0, Code Segment
    .byte   0xCF                        # Granularity: 1Kb, 32-bit operands
    .byte   0x00                        # Base 31:24

##
## Kernel data segment descriptor
##

    .word   0xFFFF                      # Limit 15:0
    .word   0x0                         # Base 15:0
    .byte   0x0                         # Base 23:16
    .byte   0x92                        # Access: Present, Ring 0, Data Segment
    .byte   0xCF                        # Granularity: 1Kb, 32-bit operands
    .byte   0x00                        # Base 31:24

