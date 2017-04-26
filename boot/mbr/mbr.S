/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mbr.S

Abstract:

    This module implements the Master Boot Record code used to bootstrap the
    operating system. This code lives on the first sector of the disk.

Author:

    Evan Green 4-Feb-2014

Environment:

    MBR

--*/

##
## ---------------------------------------------------------------- Definitions
##

##
## Define the segment where the BIOS loads this MBR code.
##

.equ BOOT_ADDRESS,           0x7C00

##
## Define the destination address the MBR copies itself to.
##

.equ RELOCATED_ADDRESS,      0x0600

##
## Define the size of a disk sector.
##

.equ SECTOR_SIZE,            0x0200

##
## Define the (relocated) location of the partition table.
##

.equ PARTITION_TABLE_ADDRESS, (RELOCATED_ADDRESS + 0x1BE)

##
## Define the number of partition table entries.
##

.equ PARTITION_ENTRY_COUNT,  4

##
## Define the size of a partition table entry.
##

.equ PARTITION_ENTRY_SIZE, 0x10

##
## Define the number of times to try and read the disk.
##

.equ DISK_RETRY_COUNT, 0x06

##
## Define offsets into a partition table entry.
##

.equ PARTITION_START_HEAD, 0x1
.equ PARTITION_START_SECTOR, 0x2
.equ PARTITION_START_CYLINDER, 0x3
.equ PARTITION_LBA_OFFSET, 0x8

##
## Define the address of the boot signature.
##

.equ BOOT_SIGNATURE_ADDRESS, (BOOT_ADDRESS + 0x1FE)

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
## Stick this in the .init section so it ends up at the correct address, since
## the build machinery sets the .init section address rather than .text.
##

.section .init

##
## .globl allows this label to be visible to the linker.
##

.globl _start

_start:

    ##
    ## Skip the space for the BIOS parameter block in case things get altered
    ## there.
    ##

    jmp AfterBiosParameterBlock
    nop

.org 0x40

AfterBiosParameterBlock:

    ##
    ## Copy this code to a location less in the way.
    ##

    xorw    %ax, %ax                    # Zero out AX.
    movw    %ax, %ds                    # Zero out DS.
    movw    %ax, %es                    # Zero out ES.
    movw    %ax, %ss                    # Zero out SS.
    movw    $BOOT_ADDRESS, %si          # Load SI with the source address.
    movw    $BOOT_ADDRESS, %sp          # Also make this the top of the stack.
    movw    $RELOCATED_ADDRESS, %di     # Loads DI with the destination address.
    movw    $SECTOR_SIZE, %cx           # Load the number of bytes to copy.
    cld                                 # Decrement CX on repeats.
    rep movsb                           # Copy CX bytes of DS:[SI] to ES:[DI].

    ##
    ## Jump to the relocated code.
    ##

    push    %ax                         # Push a zero code segment.
    push    $AfterMove                  # Push the destination.
    retf                                # "Return" to the relocated code.

AfterMove:
    sti                                 # Enable interrupts so disk reads work.

    ##
    ## Search for the active partition.
    ##

    movw    $PARTITION_ENTRY_COUNT, %cx     # Load the number of table entries.
    movw    $PARTITION_TABLE_ADDRESS, %bp   # Load the first partition entry.

BootPartitionLoop:
    cmpb    $0, (%bp)                   # Compare the boot flag to zero.
    jl      DriveReadLoop               # Jump if 0x80 or above (ie negative).
    jnz     InvalidPartitionTable       # If it was 0x1-0x79, that's invalid.
    addw    $PARTITION_ENTRY_SIZE, %bp  # Move to the next table entry.
    loop    BootPartitionLoop           # Loop to compare the next entry.

    ##
    ## No entries were found to be bootable. Call INT 0x18, which is "load
    ## ROM-BASIC". This doesn't exist, so most BIOSes just display something
    ## like "press a key to reboot".
    ##

    int    $0x18

DriveReadLoop:
    movb    %dl, 0x12(%bp)              # Write boot drive number into a local.
    pushw   %bp                         # Save the base pointer.
    movb    $DISK_RETRY_COUNT, 0x11(%bp) # Initialize the retry count.
    movb    $0, 0x10(%bp)               # Mark extensions as not supported.

    ##
    ## Check to see if INT 13 extensions are supported. If CF is cleared, BX
    ## changes to 0xAA55, and the lowest bit in CX is set, then they are
    ## supported.
    ##

    movb    $0x41, %ah                  # Set AH.
    movw    $0x55AA, %bx                # Set BX to the magic value.
    int     $0x13                       # Call the BIOS.
    popw    %bp                         # Restore the original BP.
    jb      AfterInt13ExtensionsCheck   # Jump over the rest of the check.
    cmpw    $0xAA55, %bx                # See if BX changed to 0xAA55.
    jnz     AfterInt13ExtensionsCheck   # Jump over the rest of the check.
    test    $0x0001, %cx                # See if the lowest bit of CX is set.
    jz      AfterInt13ExtensionsCheck   # Jump over if not.
    incb    0x10(%bp)                   # Set the extensions flag as enabled.

AfterInt13ExtensionsCheck:
    pushal                              # Push all 32 bit registers.
    cmpb    $0, 0x10(%bp)               # Check the INT13 extensions flag.
    jz      ReadWithoutExtensions       # If not set, go to the old read.

    ##
    ## Perform an INT 13 extended read of the first sector of the partition,
    ## and load it into memory where the MBR was loaded. The extended read
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
    pushl   PARTITION_LBA_OFFSET(%bp)   # Push the starting sector low.
    pushl   $BOOT_ADDRESS               # Push the transfer buffer.
    pushw   $1                          # Push the sector count.
    pushw   $0x0010                     # Push reserved and packet size.
    movb    $0x42, %ah                  # Function 0x42, extended read.
    movb    0x12(%bp), %dl              # Load the drive number.
    movw    %sp, %si                    # SI points to the disk packet address.
    int     $0x13                       # Read the sector from the disk.

    ##
    ## Check the status of the extended read.
    ##

    lahf                                # Load the flags into AH.
    addw    $0x10, %sp                  # Pop the disk packet off the stack.
    sahf                                # Restore the flags into AH.
    jmp     AfterDiskRead               # Jump to the next part.

    ##
    ## Perform an INT 13 regular read because extensions are not supported.
    ## AH: Function 2 (read sectors).
    ## AL: Sector count.
    ## ES:BX: Transfer buffer.
    ## DL: Disk drive number.
    ## DH: Head number.
    ## CL: Sector number plus low two bits of the cylinder number.
    ## CH: High bits of the cylinder number.
    ##

ReadWithoutExtensions:
    movw    $0x0201, %ax                # Load function 2, sector count 1.
    movw    $BOOT_ADDRESS, %bx          # Load the buffer address.
    movb    0x12(%bp), %dl              # Load the drive number.
    movb    PARTITION_START_HEAD(%bp), %dh      # Load the head number.
    movb    PARTITION_START_SECTOR(%bp), %cl    # Load the sector number.
    movb    PARTITION_START_CYLINDER(%bp), %ch  # Load the cylinder number.
    int     $0x13

AfterDiskRead:
    popal                               # Restore all 32 bit registers.
    jnb     DiskReadSuccess             # If ok, jump out of the disk read loop.

    ##
    ## The disk read failed. Decrement the retry counter and try again.
    ##

    decb    0x11(%bp)                   # Decrement retry counter.
    jnz     ResetDrive                  # Try again if not zero.

    ##
    ## If this was already drive 0x80, then fail. If it wasn't already 0x80,
    ## then try this all again with drive 0x80, the default BIOS boot disk.
    ##

    cmpb    $0x80, 0x12(%bp)            # Compare the drive number to 0x80.
    jz      DiskReadError               # Give up if it's already 0x80.
    movb    $0x80, %dl                  # Set the drive to 0x80.
    jmp     DriveReadLoop               # Try it all again.

    ##
    ## Call INT 13, function 0 to reset the drive. This takes the drive number
    ## in dl.
    ##

ResetDrive:
    pushw   %bp                         # Save BP.
    xorb    %ah, %ah                    # Zero out AH.
    movb    0x12(%bp), %dl              # Load the drive number.
    int     $0x13                       # Reset the drive.
    popw    %bp                         # Restore BP.
    jmp     AfterInt13ExtensionsCheck   # Try the read again.

DiskReadSuccess:
    cmpw     $0xAA55, BOOT_SIGNATURE_ADDRESS    # See if the partition can boot.
    jnz     PartitionNotBootable        # Fail if the signature is not there.

    pushw   0x12(%bp)                   # Push the drive number.

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

    ##
    ## Jump to the Volume Boot Record this code worked so hard to load. Set
    ## DL to the drive number and DS:SI to point to the partition table entry.
    ##

AfterA20Line:
    popw    %dx                         # Restore drive number.
    xorb    %dh, %dh                    # Clear DH.
    movw    %bp, %si                    # Set SI.
    jmp     $0, $BOOT_ADDRESS           # Blow this popsicle stand.

    ##
    ## In error cases down here, print a message and die sadly.
    ##

DiskReadError:
    movw    $ReadFailureMessage, %si
    jmp     PrintStringAndDie

InvalidPartitionTable:
    movw    $InvalidPartitionTableMessage, %si
    jmp     PrintStringAndDie

PartitionNotBootable:
    movw    $NoOsMessage, %si
    jmp     PrintStringAndDie

    ##
    ## Print a null-terminated string, and then end it.
    ##

PrintStringAndDie:
    cld                                 # Clear the direction flag.
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
    .asciz "MBR Read Error"

NoOsMessage:
    .asciz "No OS"

InvalidPartitionTableMessage:
    .asciz "Invalid partition table"

.if 0

PrintShort:
    pushw   %ax
    movb    %ah, %al
    call PrintByte
    popw    %ax
    call PrintByte
    movb    $' ', %al
    call PrintCharacter
    ret

PrintByte:
    pushw   %ax
    shr     $4, %al
    cmp     $0xA, %al
    sbb     $0x69, %al
    das
    call    PrintCharacter
    popw    %ax
    ror     $4, %al
    shr     $4, %al
    cmp     $0xA, %al
    sbb     $0x69, %al
    das

PrintCharacter:
    movw    $0x0007, %bx                # Display normal white on black.
    movb    $0x0E, %ah                  # Print character function.
    int     $0x10                       # Print the character.
    ret

.endif

##
## Define the signature that the BIOS uses to determine this disk is bootable.
##

.org 0x1FE

    .byte   0x55
    .byte   0xAA

