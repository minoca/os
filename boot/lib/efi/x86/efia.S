/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efia.S

Abstract:

    This module implements assembly support routines for UEFI support.

Author:

    Evan Green 10-Feb-2014

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/x86.inc>

//
// ---------------------------------------------------------------------- Code
//

//
// .text specifies that this code belongs in the executable section.
//
// .code32 specifies that this is 32-bit protected mode code.
//

.text
.code32

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
    mov     %esp, %eax              # Get the stack pointer.
    ret                             # Return.

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
    xorl    %eax, %eax              # Zero eax.
    movw    %cs, %ax                # Get CS.
    movl    %eax, BoFirmwareCs      # Save CS.
    movw    %ds, %ax                # Get DS.
    movl    %eax, BoFirmwareDs      # Save DS.
    movw    %es, %ax                # Get ES.
    movl    %eax, BoFirmwareEs      # Save ES.
    movw    %fs, %ax                # Get FS.
    movl    %eax, BoFirmwareFs      # Save FS.
    movw    %gs, %ax                # Get GS.
    movl    %eax, BoFirmwareGs      # Save GS.
    movw    %ss, %ax                # Get SS.
    movl    %eax, BoFirmwareSs      # Save SS.
    pushfl                          # Push eflags.
    popl    %eax                    # Pop EAX.
    movl    %eax, BoFirmwareEflags  # Save eflags.
    movl    $BoFirmwareIdt, %eax    # Get the IDT save address.
    sidt    (%eax)                  # Save the IDT.
    movl    $BoFirmwareGdt, %eax    # Get the GDT save address.
    sgdt    (%eax)                  # Save the GDT.
    cli                             # Disable interrupts.
    ret                             # Return.

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

    xorl    %eax, %eax              # Zero eax.
    movw    %cs, %ax                # Get CS.
    movl    %eax, BoLoaderCs        # Save CS.
    movw    %ds, %ax                # Get DS.
    movl    %eax, BoLoaderDs        # Save DS.
    movw    %es, %ax                # Get ES.
    movl    %eax, BoLoaderEs        # Save ES.
    movw    %fs, %ax                # Get FS.
    movl    %eax, BoLoaderFs        # Save FS.
    movw    %gs, %ax                # Get GS.
    movl    %eax, BoLoaderGs        # Save GS.
    movw    %ss, %ax                # Get SS.
    movl    %eax, BoLoaderSs        # Save SS.
    pushfl                          # Push eflags.
    popl    %eax                    # Pop EAX.
    movl    %eax, BoLoaderEflags    # Save eflags.
    movl    $BoLoaderIdt, %eax      # Get the IDT save address.
    sidt    (%eax)                  # Save the IDT.
    movl    $BoLoaderGdt, %eax      # Get the GDT save address.
    sgdt    (%eax)                  # Save the GDT.
    cli                             # Disable interrupts.

    //
    // Restore the firmware context.
    //

    movl    BoFirmwareDs, %ecx      # Get DS.
    movl    BoFirmwareCs, %eax      # Get CS.
    pushl   %eax                    # Push CS.
    movl    $BopEfiRestoreFirmwareContextJump, %eax # Get the jump destination.
    pushl   %eax                    # Push eax.
    movl    $BoFirmwareGdt, %eax    # Get the GDT.
    lgdt    (%eax)                  # Load the GDT. It's not offical yet.
    retf                            # "Return" back to CS and next instruction.

BopEfiRestoreFirmwareContextJump:
    movw    %cx, %ds                # Load DS so more loads can be done.
    movl    BoFirmwareEs, %eax      # Get ES.
    movw    %ax, %es                # Set ES.
    movl    BoFirmwareFs, %eax      # Get FS.
    movw    %ax, %fs                # Set FS.
    movl    BoFirmwareGs, %eax      # Get GS.
    movw    %ax, %gs                # Set GS.
    movl    BoFirmwareSs, %eax      # Get SS.
    movw    %ax, %ss                # Set SS.
    movl    $BoFirmwareIdt, %eax    # Get the IDT.
    lidt    (%eax)                  # Restore the IDT. No debugging past here.
    movl    BoFirmwareEflags, %eax  # Get Eflags.
    pushl   %eax                    # Push the flags.
    popfl                           # Pop flags. Interrupts could be enabled.
    ret                             # Return.

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
    cli                             # Disable interrupts.
    movl    BoLoaderDs, %ecx        # Get DS.
    movl    BoLoaderCs, %eax        # Get CS.
    pushl   %eax                    # Push CS.
    movl    $BopEfiRestoreApplicationContextJump, %eax # Get the destination.
    pushl   %eax                    # Push eax.
    movl    $BoLoaderGdt, %eax      # Get the GDT.
    lgdt    (%eax)                  # Load the GDT. It's not offical yet.
    retf                            # Return back to the new GDT and CS.

BopEfiRestoreApplicationContextJump:
    movw    %cx, %ds                # Load DS so more loads can be done.
    movl    BoLoaderEs, %eax        # Get ES.
    movw    %ax, %es                # Set ES.
    movl    BoLoaderFs, %eax        # Get FS.
    movw    %ax, %fs                # Set FS.
    movl    BoLoaderGs, %eax        # Get GS.
    movw    %ax, %gs                # Set GS.
    movl    BoLoaderSs, %eax        # Get SS.
    movw    %ax, %ss                # Set SS.
    movl    $BoLoaderIdt, %eax      # Get the IDT.
    lidt    (%eax)                  # Restore the IDT. No debugging past here.
    movl    BoLoaderEflags, %eax    # Get Eflags.
    pushl   %eax                    # Push the flags.
    popfl                           # Pop flags. Interrupts could be enabled.
    ret                             # Return.

END_FUNCTION(BopEfiRestoreApplicationContext)

//
// --------------------------------------------------------- Internal Functions
//

