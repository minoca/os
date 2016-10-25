/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bioscall.c

Abstract:

    This module implements support for calling the BIOS back in real mode.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "biosfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define BIOS_CALL_CODE_PAGE  0x1000
#define BIOS_CALL_STACK_PAGE 0x2000
#define BIOS_CALL_DATA_PAGE  0x3000

#define BIOS_CALL_STACK_OFFSET 0x0FFC

#define LONG_JUMP_32_SIZE 7
#define LONG_JUMP_16_SIZE 5

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

extern UINT8 EfiBiosCallTemplate;
extern UINT8 EfiBiosCallTemplateLongJump;
extern UINT8 EfiBiosCallTemplateLongJump2;
extern UINT8 EfiBiosCallTemplateLongJump3;
extern UINT8 EfiBiosCallTemplateIntInstruction;
extern UINT8 EfiBiosCallTemplateEnd;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipCreateBiosCallContext (
    PBIOS_CALL_CONTEXT Context,
    UINT8 InterruptNumber
    )

/*++

Routine Description:

    This routine initializes a standard real mode context for making a BIOS
    call via software interrupt (ie an int 0x10 call). It does not actually
    execute the context, it only initializes the data structures.

Arguments:

    Context - Supplies a pointer to the context structure that will be
        initialized. It is assumed this memory is already allocated.

    InterruptNumber - Supplies the interrupt number that will be called.

Return Value:

    Status code.

--*/

{

    UINT8 *Destination;
    UINT8 *InterruptCallLocation;
    UINT16 LongJumpDestination;
    UINT8 *LongJumpLocation;
    UINT32 Offset;
    UINT8 *Source;

    //
    // Allocate code, stack, and data pages.
    //

    Context->CodePage = (VOID *)BIOS_CALL_CODE_PAGE;
    Context->DataPage = (VOID *)BIOS_CALL_DATA_PAGE;
    Context->StackPage = (VOID *)BIOS_CALL_STACK_PAGE;

    //
    // Copy the template code into the code page.
    //

    Source = &EfiBiosCallTemplate;
    Destination = (UINT8 *)Context->CodePage;
    while (Source != &EfiBiosCallTemplateEnd) {
        *Destination = *Source;
        Destination += 1;
        Source += 1;
    }

    //
    // Fix up the interrupt call.
    //

    Offset = (UINTN)(&EfiBiosCallTemplateIntInstruction) -
             (UINTN)(&EfiBiosCallTemplate);

    InterruptCallLocation = Context->CodePage + Offset + 1;
    *InterruptCallLocation = InterruptNumber;

    //
    // Fix up the first long jump, which is in 32-bit protected mode going to
    // 16-bit protected mode code.
    //

    Offset = (UINTN)(&EfiBiosCallTemplateLongJump) -
             (UINTN)(&EfiBiosCallTemplate);

    LongJumpLocation = Context->CodePage + Offset;
    LongJumpDestination = (UINTN)LongJumpLocation + LONG_JUMP_32_SIZE;
    *((UINT32 *)(LongJumpLocation + 1)) = LongJumpDestination;

    //
    // Fix up the second long jump, which is in 16-bit protected mode going to
    // 16-bit real mode code.
    //

    Offset = (UINTN)(&EfiBiosCallTemplateLongJump2) -
             (UINTN)(&EfiBiosCallTemplate);

    LongJumpLocation = Context->CodePage + Offset;
    LongJumpDestination = (UINTN)LongJumpLocation + LONG_JUMP_16_SIZE;
    *((UINT16 *)(LongJumpLocation + 1)) = LongJumpDestination;
    *((UINT16 *)(LongJumpLocation + 3)) = 0;

    //
    // Fix up the third long jump, which is in 16-bit real mode going to 32-bit
    // protected mode code.
    //

    Offset = (UINTN)(&EfiBiosCallTemplateLongJump3) -
             (UINTN)(&EfiBiosCallTemplate);

    LongJumpLocation = Context->CodePage + Offset;
    LongJumpDestination = (UINTN)LongJumpLocation + LONG_JUMP_16_SIZE;
    *((UINT16 *)(LongJumpLocation + 1)) = LongJumpDestination;

    //
    // Initialize the registers.
    //

    Context->Cs = 0;
    Context->Ds = 0;
    Context->Es = 0;
    Context->Fs = 0;
    Context->Gs = 0;
    Context->Ss = 0;
    Context->Eflags = DEFAULT_FLAGS;
    Context->Eip = (UINTN)Context->CodePage;
    Context->Esp = (UINTN)Context->StackPage + BIOS_CALL_STACK_OFFSET;
    return EFI_SUCCESS;
}

VOID
EfipDestroyBiosCallContext (
    PBIOS_CALL_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys a created BIOS call context.

Arguments:

    Context - Supplies a pointer to the context structure that will be
        freed.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

