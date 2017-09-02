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
    movq    BoFirmwareCs@GOTPCREL(%rip), %rcx   # Get address of global.
    movl    %eax, (%rcx)                        # Save CS.
    movw    %ds, %ax                            # Get DS.
    movq    BoFirmwareDs@GOTPCREL(%rip), %rcx   # Get address of global.
    movl    %eax, (%rcx)                        # Save DS.
    movw    %es, %ax                            # Get ES.
    movq    BoFirmwareEs@GOTPCREL(%rip), %rcx   # Get address of global.
    movl    %eax, (%rcx)                        # Save ES.
    movw    %fs, %ax                            # Get FS.
    movq    BoFirmwareFs@GOTPCREL(%rip), %rcx   # Get address of global.
    movl    %eax, (%rcx)                        # Save FS.
    movw    %gs, %ax                            # Get GS.
    movq    BoFirmwareGs@GOTPCREL(%rip), %rcx   # Get address of global.
    movl    %eax, (%rcx)                        # Save GS.
    movw    %ss, %ax                            # Get SS.
    movq    BoFirmwareSs@GOTPCREL(%rip), %rcx   # Get address of global.
    movl    %eax, (%rcx)                        # Save SS.
    pushfq                                      # Push rflags.
    popq    %rax                                # Pop RAX.
    movq    BoFirmwareRflags@GOTPCREL(%rip), %rcx   # Get address of global.
    movl    %eax, (%rcx)                        # Save RFLAGS.
    movq    BoFirmwareIdt@GOTPCREL(%rip), %rax  # Get the IDT save address.
    sidt    (%rax)                              # Save the IDT.
    movq    BoFirmwareGdt@GOTPCREL(%rip), %rax  # Get the GDT save address.
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
    movq    BoLoaderCs@GOTPCREL(%rip), %rcx     # Get address of global.
    movl    %eax, (%rcx)                        # Save CS.
    movw    %ds, %ax                            # Get DS.
    movq    BoLoaderDs@GOTPCREL(%rip), %rcx     # Get address of global.
    movl    %eax, (%rcx)                        # Save DS.
    movw    %es, %ax                            # Get ES.
    movq    BoLoaderEs@GOTPCREL(%rip), %rcx     # Get address of global.
    movl    %eax, (%rcx)                        # Save ES.
    movw    %fs, %ax                            # Get FS.
    movq    BoLoaderFs@GOTPCREL(%rip), %rcx     # Get address of global.
    movl    %eax, (%rcx)                        # Save FS.
    movw    %gs, %ax                            # Get GS.
    movq    BoLoaderGs@GOTPCREL(%rip), %rcx     # Get address of global.
    movl    %eax, (%rcx)                        # Save GS.
    movw    %ss, %ax                            # Get SS.
    movq    BoLoaderSs@GOTPCREL(%rip), %rcx     # Get address of global.
    movl    %eax, (%rcx)                        # Save SS.
    pushfq                                      # Push rflags.
    popq    %rax                                # Pop RAX.
    movq    BoLoaderRflags@GOTPCREL(%rip), %rcx # Get address of global.
    movq    %rax, (%rcx)                        # Save RFLAGS.
    movq    BoLoaderIdt@GOTPCREL(%rip), %rax    # Get the IDT save address.
    sidt    (%rax)                              # Save the IDT.
    movq    BoLoaderGdt@GOTPCREL(%rip), %rax    # Get the GDT save address.
    sgdt    (%rax)                              # Save the GDT.
    cli                                         # Disable interrupts.

    //
    // Restore the firmware context.
    //

    movq    BoFirmwareDs@GOTPCREL(%rip), %rcx   # Get DS address.
    movl    (%rcx), %ecx                        # Dereference to get value.
    movq    BoFirmwareCs@GOTPCREL(%rip), %rax   # Get CS address.
    movl    (%rax), %eax                        # Dereference to get value.
    pushq   %rax                                # Push CS.
    movq    BopEfiRestoreFirmwareContextJump@GOTPCREL(%rip), %rax
    pushq   %rax                                # Push eax.
    movq    BoFirmwareGdt@GOTPCREL(%rip), %rax  # Get the GDT.
    lgdt    (%rax)                              # Load the GDT. Do a jump.
    retfq                                       # "Return" immediately below.

BopEfiRestoreFirmwareContextJump:
    movw    %cx, %ds                            # Load DS.
    movq    BoFirmwareEs@GOTPCREL(%rip), %rax   # Get ES address.
    movl    (%rax), %eax                        # Dereference to get value.
    movw    %ax, %es                            # Set ES.
    movq    BoFirmwareFs@GOTPCREL(%rip), %rax   # Get FS address.
    movl    (%rax), %eax                        # Dereference to get value.
    movw    %ax, %fs                            # Set FS.
    movq    BoFirmwareGs@GOTPCREL(%rip), %rax   # Get GS address.
    movl    (%rax), %eax                        # Dereference to get value.
    movw    %ax, %gs                            # Set GS.
    movq    BoFirmwareSs@GOTPCREL(%rip), %rax   # Get SS address.
    movl    (%rax), %eax                        # Dereference to get value.
    movw    %ax, %ss                            # Set SS.
    movq    BoFirmwareIdt@GOTPCREL(%rip), %rax  # Get the IDT.
    lidt    (%rax)                              # Restore the IDT.
    movq    BoFirmwareRflags@GOTPCREL(%rip), %rax  # Get Rflags.
    movq    (%rax), %rax                        # Dereference to get value.
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
    movq    BoLoaderDs@GOTPCREL(%rip), %rcx     # Get DS.
    movl    (%rcx), %ecx                        # Dereference to get value.
    movq    BoLoaderCs@GOTPCREL(%rip), %rax     # Get CS.
    movl    (%rax), %eax                        # Dereference to get value.
    pushq   %rax                                # Push CS.
    movq    BopEfiRestoreApplicationContextJump@GOTPCREL(%rip), %rax
    pushq   %rax                                # Push eax.
    movq    BoLoaderGdt@GOTPCREL(%rip), %rax    # Get the GDT.
    lgdt    (%rax)                              # Load the GDT.
    retfq                                       # "Return" immediately below.

BopEfiRestoreApplicationContextJump:
    movw    %cx, %ds                            # Load DS.
    movq    BoLoaderEs@GOTPCREL(%rip), %rax     # Get ES address.
    movl    (%rax), %eax                        # Dereference to get value.
    movw    %ax, %es                            # Set ES.
    movq    BoLoaderFs@GOTPCREL(%rip), %rax     # Get FS address.
    movl    (%rax), %eax                        # Dereference to get value.
    movw    %ax, %fs                            # Set FS.
    movq    BoLoaderGs@GOTPCREL(%rip), %rax     # Get GS address.
    movl    (%rax), %eax                        # Dereference to get value.
    movw    %ax, %gs                            # Set GS.
    movq    BoLoaderSs@GOTPCREL(%rip), %rax     # Get SS address.
    movl    (%rax), %eax                        # Dereference to get value.
    movw    %ax, %ss                            # Set SS.
    movq    BoLoaderIdt@GOTPCREL(%rip), %rax    # Get the IDT.
    lidt    (%rax)                              # Restore the IDT. No debugging.
    movq    BoLoaderRflags@GOTPCREL(%rip), %rax # Get Rflags address.
    movq    (%rax), %rax                        # Dereference to get value.
    pushq   %rax                                # Push the flags.
    popfq                                       # Pop flags. Enable interrupts.
    retq                                        # Return.

END_FUNCTION(BopEfiRestoreApplicationContext)

//
// --------------------------------------------------------- Internal Functions
//

