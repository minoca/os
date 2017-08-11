/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efia.S

Abstract:

    This module implements AMD64 assembly support routines for UEFI support.

Author:

    Evan Green 11-Aug-2017

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/x64.inc>

//
// ---------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// UINTN
// BopEfiGetStackPointer (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the value of the stack register. Note that this can only
    be used as an approximate value, since as soon as this function returns
    the stack pointer changes.

Arguments:

    None.

Return Value:

    Returns the current stack pointer.

--*/

FUNCTION(BopEfiGetStackPointer)
    movq    %rsp, %rax              # Get the stack pointer.
    retq                            # Return.

END_FUNCTION(BopEfiGetStackPointer)

//
// VOID
// BopEfiSaveInitialState (
//     VOID
//     )
//

/*++

Routine Description:

    This routine saves the initial CPU state as passed to the application. This
    state is restored when making EFI calls.

Arguments:

    None.

Return Value:

    None. The original contents are saved in globals.

--*/

FUNCTION(BopEfiSaveInitialState)
    xorl    %eax, %eax                          # Zero rax.
    movw    %cs, %ax                            # Get CS.
    movl    %eax, BoFirmwareCs@GOTPCREL(%rip)   # Save CS.
    movw    %ds, %ax                            # Get DS.
    movl    %eax, BoFirmwareDs@GOTPCREL(%rip)   # Save DS.
    movw    %es, %ax                            # Get ES.
    movl    %eax, BoFirmwareEs@GOTPCREL(%rip)   # Save ES.
    movw    %fs, %ax                            # Get FS.
    movl    %eax, BoFirmwareFs@GOTPCREL(%rip)   # Save FS.
    movw    %gs, %ax                            # Get GS.
    movl    %eax, BoFirmwareGs@GOTPCREL(%rip)   # Save GS.
    movw    %ss, %ax                            # Get SS.
    movl    %eax, BoFirmwareSs@GOTPCREL(%rip)   # Save SS.
    pushfq                                      # Push rflags.
    popq    %rax                                # Pop RAX.
    movq    %rax, BoFirmwareRflags@GOTPCREL(%rip)   # Save eflags.
    leaq    BoFirmwareIdt@GOTPCREL(%rip), %rax  # Get the IDT save address.
    sidt    (%rax)                              # Save the IDT.
    leaq    BoFirmwareGdt@GOTPCREL(%rip), %rax  # Get the GDT save address.
    sgdt    (%rax)                              # Save the GDT.
    cli                                         # Disable interrupts.
    retq                                        # Return.

END_FUNCTION(BopEfiSaveInitialState)

//
// VOID
// BopEfiRestoreFirmwareContext (
//     VOID
//     )
//

/*++

Routine Description:

    This routine restores the processor context set when the EFI application
    was started. This routine is called right before an EFI firmware call is
    made. It is not possible to debug through this function, as the IDT is
    swapped out.

Arguments:

    None.

Return Value:

    None. The OS loader context is saved in globals.

--*/

FUNCTION(BopEfiRestoreFirmwareContext)

    //
    // Start by saving the OS context.
    //

    xorl    %eax, %eax                          # Zero eax.
    movw    %cs, %ax                            # Get CS.
    movl    %eax, BoLoaderCs@GOTPCREL(%rip)     # Save CS.
    movw    %ds, %ax                            # Get DS.
    movl    %eax, BoLoaderDs@GOTPCREL(%rip)     # Save DS.
    movw    %es, %ax                            # Get ES.
    movl    %eax, BoLoaderEs@GOTPCREL(%rip)     # Save ES.
    movw    %fs, %ax                            # Get FS.
    movl    %eax, BoLoaderFs@GOTPCREL(%rip)     # Save FS.
    movw    %gs, %ax                            # Get GS.
    movl    %eax, BoLoaderGs@GOTPCREL(%rip)     # Save GS.
    movw    %ss, %ax                            # Get SS.
    movl    %eax, BoLoaderSs@GOTPCREL(%rip)     # Save SS.
    pushfq                                      # Push rflags.
    popq    %rax                                # Pop RAX.
    movq    %rax, BoLoaderRflags@GOTPCREL(%rip) # Save rflags.
    leaq    BoLoaderIdt@GOTPCREL(%rip), %rax    # Get the IDT save address.
    sidt    (%rax)                              # Save the IDT.
    leaq    BoLoaderGdt@GOTPCREL(%rip), %rax    # Get the GDT save address.
    sgdt    (%rax)                              # Save the GDT.
    cli                                         # Disable interrupts.

    //
    // Restore the firmware context.
    //

    movl    BoFirmwareDs@GOTPCREL(%rip), %ecx   # Get DS.
    movl    BoFirmwareCs@GOTPCREL(%rip), %eax   # Get CS.
    pushq   %rax                                # Push CS.
    movq    BopEfiRestoreFirmwareContextJump@GOTPCREL(%rip), %rax
    pushq   %rax                                # Push eax.
    leaq    BoFirmwareGdt@GOTPCREL(%rip), %rax  # Get the GDT.
    lgdt    (%rax)                              # Load the GDT. Do a jump.
    retfq                                       # "Return" immediately below.

BopEfiRestoreFirmwareContextJump:
    movw    %cx, %ds                            # Load DS.
    movl    BoFirmwareEs@GOTPCREL(%rip), %eax   # Get ES.
    movw    %ax, %es                            # Set ES.
    movl    BoFirmwareFs@GOTPCREL(%rip), %eax   # Get FS.
    movw    %ax, %fs                            # Set FS.
    movl    BoFirmwareGs@GOTPCREL(%rip), %eax   # Get GS.
    movw    %ax, %gs                            # Set GS.
    movl    BoFirmwareSs@GOTPCREL(%rip), %eax   # Get SS.
    movw    %ax, %ss                            # Set SS.
    leaq    BoFirmwareIdt@GOTPCREL(%rip), %rax  # Get the IDT.
    lidt    (%rax)                              # Restore the IDT.
    movq    BoFirmwareRflags@GOTPCREL(%rip), %rax  # Get Rflags.
    pushq   %rax                                # Push the flags.
    popfq                                       # Pop flags, enable interrupts.
    retq                                        # Return.

END_FUNCTION(BopEfiRestoreFirmwareContext)

//
// VOID
// BopEfiRestoreApplicationContext (
//     VOID
//     )
//

/*++

Routine Description:

    This routine restores the boot application context. This routine is called
    after an EFI call to restore the processor state set up by the OS loader.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(BopEfiRestoreApplicationContext)
    cli                                         # Disable interrupts.
    movl    BoLoaderDs@GOTPCREL(%rip), %ecx     # Get DS.
    movl    BoLoaderCs@GOTPCREL(%rip), %eax     # Get CS.
    pushq   %rax                                # Push CS.
    movq    BopEfiRestoreApplicationContextJump@GOTPCREL(%rip), %rax
    pushq   %rax                                # Push eax.
    leaq    BoLoaderGdt@GOTPCREL(%rip), %rax    # Get the GDT.
    lgdt    (%rax)                              # Load the GDT.
    retfq                                       # "Return" immediately below.

BopEfiRestoreApplicationContextJump:
    movw    %cx, %ds                            # Load DS.
    movl    BoLoaderEs@GOTPCREL(%rip), %eax     # Get ES.
    movw    %ax, %es                            # Set ES.
    movl    BoLoaderFs@GOTPCREL(%rip), %eax     # Get FS.
    movw    %ax, %fs                            # Set FS.
    movl    BoLoaderGs@GOTPCREL(%rip), %eax     # Get GS.
    movw    %ax, %gs                            # Set GS.
    movl    BoLoaderSs@GOTPCREL(%rip), %eax     # Get SS.
    movw    %ax, %ss                            # Set SS.
    leaq    BoLoaderIdt@GOTPCREL(%rip), %rax    # Get the IDT.
    lidt    (%rax)                              # Restore the IDT. No debugging.
    movq    BoLoaderRflags@GOTPCREL(%rip), %rax # Get Rflags.
    pushq   %rax                                # Push the flags.
    popfq                                       # Pop flags. Enable interrupts.
    retq                                        # Return.

END_FUNCTION(BopEfiRestoreApplicationContext)

//
// --------------------------------------------------------- Internal Functions
//

