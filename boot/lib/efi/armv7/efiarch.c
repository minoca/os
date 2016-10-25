/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efiarch.c

Abstract:

    This module implements CPU architecture support for UEFI in the boot loader.

Author:

    Evan Green 27-Mar-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/uefi/uefi.h>
#include <minoca/kernel/arm.h>
#include "firmware.h"
#include "efisup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the supposed size of the stack EFI hands to the boot application.
//

#define EFI_STACK_SIZE 0x4000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Architecture assembly routine being borrowed from other parts of the loader.
//

VOID
BoInitializeExceptionStacks (
    PVOID ExceptionStacksBase,
    ULONG ExceptionStackSize
    );

BOOL
BoDisableInterrupts (
    VOID
    );

VOID
BoEnableInterrupts (
    VOID
    );

BOOL
BoAreInterruptsEnabled (
    VOID
    );

VOID
BoCpuid (
    PARM_CPUID Features
    );

//
// Internal assembly routines.
//

VOID
BopEfiSaveFirmwareExceptionStacks (
    VOID
    );

VOID
BopEfiRestoreFirmwareExceptionStacks (
    VOID
    );

//
// Internal C functions
//

VOID
BopEfiSaveInitialState (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

ULONG BoFirmwareControlRegister;
PVOID BoFirmwareVectorBaseRegister;
ULONG BoFirmwareIrqStack;
ULONG BoFirmwareIrqLink;
ULONG BoFirmwareFiqStack;
ULONG BoFirmwareFiqLink;
ULONG BoFirmwareUndefStack;
ULONG BoFirmwareUndefLink;
ULONG BoFirmwareAbortStack;
ULONG BoFirmwareAbortLink;

ARM_INTERRUPT_TABLE BoFirmwareInterruptTable;
BOOL BoFirmwareInterruptsEnabled = FALSE;
BOOL BoVectorBaseValid = FALSE;

ULONG BoApplicationControlRegister;
PVOID BoApplicationVectorBaseRegister;

extern ULONG BoExceptionStacks[];
extern ARM_INTERRUPT_TABLE BoArmInterruptTable;

//
// ------------------------------------------------------------------ Functions
//

VOID
BopEfiArchInitialize (
    PVOID *TopOfStack,
    PULONG StackSize
    )

/*++

Routine Description:

    This routine performs early architecture specific initialization of an EFI
    application.

Arguments:

    TopOfStack - Supplies a pointer where an approximation of the top of the
        stack will be returned.

    StackSize - Supplies a pointer where the stack size will be returned.

Return Value:

    None.

--*/

{

    UINTN StackTop;

    StackTop = BopEfiGetStackPointer();
    StackTop = ALIGN_RANGE_UP(StackTop, EFI_PAGE_SIZE);
    *TopOfStack = (PVOID)StackTop;
    *StackSize = EFI_STACK_SIZE;
    BopEfiSaveInitialState();
    return;
}

VOID
BopEfiRestoreFirmwareContext (
    VOID
    )

/*++

Routine Description:

    This routine restores the processor context set when the EFI application
    was started. This routine is called right before an EFI firmware call is
    made. It is not possible to debug through this function.

Arguments:

    None.

Return Value:

    None. The OS loader context is saved in globals.

--*/

{

    BoApplicationControlRegister = ArGetSystemControlRegister();
    ArSetSystemControlRegister(BoFirmwareControlRegister);

    //
    // If the VBAR register is supported, then just set that back to the
    // firmware's value.
    //

    if (BoVectorBaseValid != FALSE) {
        BoApplicationVectorBaseRegister = ArGetVectorBaseAddress();
        ArSetVectorBaseAddress(BoFirmwareVectorBaseRegister);

    //
    // Copy the firmware exception vectors back into place.
    //

    } else {
        if ((BoFirmwareControlRegister & MMU_HIGH_EXCEPTION_VECTORS) != 0) {
            RtlCopyMemory((PVOID)EXCEPTION_VECTOR_ADDRESS,
                          &BoFirmwareInterruptTable,
                          sizeof(ARM_INTERRUPT_TABLE));

        } else {
            RtlCopyMemory((PVOID)EXCEPTION_VECTOR_LOW_ADDRESS,
                          &BoFirmwareInterruptTable,
                          sizeof(ARM_INTERRUPT_TABLE));
        }
    }

    BopEfiRestoreFirmwareExceptionStacks();
    if (BoFirmwareInterruptsEnabled != FALSE) {
        BoEnableInterrupts();
    }

    return;
}

VOID
BopEfiRestoreApplicationContext (
    VOID
    )

/*++

Routine Description:

    This routine restores the boot application context. This routine is called
    after an EFI call to restore the processor state set up by the OS loader.

Arguments:

    None.

Return Value:

    None.

--*/

{

    BoFirmwareInterruptsEnabled = BoDisableInterrupts();
    ArSetSystemControlRegister(BoApplicationControlRegister);

    //
    // Restore VBAR if that mechanism is supported.
    //

    if (BoVectorBaseValid != FALSE) {
        ArSetVectorBaseAddress(BoApplicationVectorBaseRegister);

    //
    // Copy the application exception vectors back in place.
    //

    } else {
        if ((BoApplicationControlRegister & MMU_HIGH_EXCEPTION_VECTORS) != 0) {
            RtlCopyMemory((PVOID)EXCEPTION_VECTOR_ADDRESS,
                          &BoArmInterruptTable,
                          sizeof(ARM_INTERRUPT_TABLE));

        } else {
            RtlCopyMemory((PVOID)EXCEPTION_VECTOR_LOW_ADDRESS,
                          &BoArmInterruptTable,
                          sizeof(ARM_INTERRUPT_TABLE));
        }
    }

    BoInitializeExceptionStacks(BoExceptionStacks, EXCEPTION_STACK_SIZE);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
BopEfiSaveInitialState (
    VOID
    )

/*++

Routine Description:

    This routine saves the initial CPU state as passed to the application. This
    state is restored when making EFI calls.

Arguments:

    None.

Return Value:

    None. The original contents are saved in globals.

--*/

{

    ARM_CPUID CpuInformation;

    BoFirmwareControlRegister = ArGetSystemControlRegister();
    BoCpuid(&CpuInformation);
    if ((CpuInformation.ProcessorFeatures[1] &
         CPUID_PROCESSOR1_SECURITY_EXTENSION_MASK) !=
        CPUID_PROCESSOR1_SECURITY_EXTENSION_UNSUPPORTED) {

        BoVectorBaseValid = TRUE;
        BoFirmwareVectorBaseRegister = ArGetVectorBaseAddress();

    } else {

        //
        // Save the contents of the firmware vector tables.
        //

        if ((BoFirmwareControlRegister & MMU_HIGH_EXCEPTION_VECTORS) != 0) {
            RtlCopyMemory(&BoFirmwareInterruptTable,
                          (PVOID)EXCEPTION_VECTOR_ADDRESS,
                          sizeof(ARM_INTERRUPT_TABLE));

        } else {
            RtlCopyMemory(&BoFirmwareInterruptTable,
                          (PVOID)EXCEPTION_VECTOR_LOW_ADDRESS,
                          sizeof(ARM_INTERRUPT_TABLE));
        }
    }

    BoFirmwareInterruptsEnabled = BoDisableInterrupts();
    BopEfiSaveFirmwareExceptionStacks();
    return;
}

