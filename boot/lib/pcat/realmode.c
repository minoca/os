/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    realmode.c

Abstract:

    This module implements functionality for switching in and out of real mode,
    used for BIOS calls.

Author:

    Evan Green 18-Jul-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86.h>
#include "realmode.h"

//
// ---------------------------------------------------------------- Definitions
//

#define REAL_MODE_CODE_PAGE  0x1000
#define REAL_MODE_STACK_PAGE 0x2000
#define REAL_MODE_DATA_PAGE  0x3000

#define DEFAULT_STACK_OFFSET 0x0FFC

#define LONG_JUMP_32_SIZE 7
#define LONG_JUMP_16_SIZE 5

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
FwpRealModeAllocatePage (
    REAL_MODE_PAGE_TYPE Type,
    PREAL_MODE_PAGE Page
    );

VOID
FwpRealModeFreePage (
    PREAL_MODE_PAGE Page
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

BOOL FwCodePageAllocated;
BOOL FwDataPageAllocated;
BOOL FwStackPageAllocated;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
FwpRealModeCreateBiosCallContext (
    PREAL_MODE_CONTEXT Context,
    UCHAR InterruptNumber
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

    PUCHAR Destination;
    PUCHAR InterruptCallLocation;
    USHORT LongJumpDestination;
    PUCHAR LongJumpLocation;
    ULONG Offset;
    PUCHAR Source;
    KSTATUS Status;

    //
    // Initailize the code, stack and data pages.
    //

    Context->CodePage.Type = RealModePageInvalid;
    Context->StackPage.Type = RealModePageInvalid;
    Context->DataPage.Type = RealModePageInvalid;

    //
    // Allocate code, stack, and data pages.
    //

    Status = FwpRealModeAllocatePage(RealModePageCode, &(Context->CodePage));
    if (!KSUCCESS(Status)) {
        goto CreateBiosCallContextEnd;
    }

    Status = FwpRealModeAllocatePage(RealModePageStack, &(Context->StackPage));
    if (!KSUCCESS(Status)) {
        goto CreateBiosCallContextEnd;
    }

    Status = FwpRealModeAllocatePage(RealModePageData, &(Context->DataPage));
    if (!KSUCCESS(Status)) {
        goto CreateBiosCallContextEnd;
    }

    //
    // Copy the template code into the code page.
    //

    Source = &FwpRealModeBiosCallTemplate;
    Destination = (PUCHAR)Context->CodePage.Page;
    while (Source != &FwpRealModeBiosCallTemplateEnd) {
        *Destination = *Source;
        Destination += 1;
        Source += 1;
    }

    //
    // Fix up the interrupt call.
    //

    Offset = (UINTN)(&FwpRealModeBiosCallTemplateIntInstruction) -
             (UINTN)(&FwpRealModeBiosCallTemplate);

    InterruptCallLocation = Context->CodePage.Page + Offset + 1;
    *InterruptCallLocation = InterruptNumber;

    //
    // Fix up the first long jump, which is in 32-bit protected mode going to
    // 16-bit protected mode code.
    //

    Offset = (UINTN)(&FwpRealModeBiosCallTemplateLongJump) -
             (UINTN)(&FwpRealModeBiosCallTemplate);

    LongJumpLocation = Context->CodePage.Page + Offset;
    LongJumpDestination = (UINTN)LongJumpLocation + LONG_JUMP_32_SIZE;
    *((PULONG)(LongJumpLocation + 1)) = LongJumpDestination;

    //
    // Fix up the second long jump, which is in 16-bit protected mode going to
    // 16-bit real mode code.
    //

    Offset = (UINTN)(&FwpRealModeBiosCallTemplateLongJump2) -
             (UINTN)(&FwpRealModeBiosCallTemplate);

    LongJumpLocation = Context->CodePage.Page + Offset;
    LongJumpDestination = (UINTN)LongJumpLocation + LONG_JUMP_16_SIZE;
    *((PUSHORT)(LongJumpLocation + 1)) = LongJumpDestination;
    *((PUSHORT)(LongJumpLocation + 3)) = 0;

    //
    // Fix up the third long jump, which is in 16-bit real mode going to 32-bit
    // protected mode code.
    //

    Offset = (UINTN)(&FwpRealModeBiosCallTemplateLongJump3) -
             (UINTN)(&FwpRealModeBiosCallTemplate);

    LongJumpLocation = Context->CodePage.Page + Offset;
    LongJumpDestination = (UINTN)LongJumpLocation + LONG_JUMP_16_SIZE;
    *((PUSHORT)(LongJumpLocation + 1)) = LongJumpDestination;

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
    Context->Eip = Context->CodePage.RealModeAddress;
    Context->Esp = Context->StackPage.RealModeAddress + DEFAULT_STACK_OFFSET;
    Status = STATUS_SUCCESS;

CreateBiosCallContextEnd:
    if (!KSUCCESS(Status)) {
        FwpRealModeDestroyBiosCallContext(Context);
    }

    return Status;
}

VOID
FwpRealModeReinitializeBiosCallContext (
    PREAL_MODE_CONTEXT Context
    )

/*++

Routine Description:

    This routine reinitializes a BIOS call context in order to use the context
    for a second BIOS call. It will reinitialize for the same interrupt number
    as specified upon creation.

Arguments:

    Context - Supplies a pointer to the context structure that will be
        reinitialized.

Return Value:

    None.

--*/

{

    //
    // Initialize the important registers.
    //

    Context->Cs = 0;
    Context->Ds = 0;
    Context->Es = 0;
    Context->Fs = 0;
    Context->Gs = 0;
    Context->Ss = 0;
    Context->Eflags = DEFAULT_FLAGS;
    Context->Eip = Context->CodePage.RealModeAddress;
    Context->Esp = Context->StackPage.RealModeAddress + DEFAULT_STACK_OFFSET;
    return;
}

VOID
FwpRealModeDestroyBiosCallContext (
    PREAL_MODE_CONTEXT Context
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

    if (Context->CodePage.Type != RealModePageInvalid) {
        FwpRealModeFreePage(&(Context->CodePage));
    }

    if (Context->StackPage.Type != RealModePageInvalid) {
        FwpRealModeFreePage(&(Context->StackPage));
    }

    if (Context->DataPage.Type != RealModePageInvalid) {
        FwpRealModeFreePage(&(Context->DataPage));
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
FwpRealModeAllocatePage (
    REAL_MODE_PAGE_TYPE Type,
    PREAL_MODE_PAGE Page
    )

/*++

Routine Description:

    This routine allocates a page of memory to be used when in virtual 8086
    mode. This page must be in the first megabyte of memory (or if paging is
    enabled it must be mapped in the first megabyte) since 16-bit code can
    only address the first megabyte.

Arguments:

    Type - Supplies the type of memory the caller wants to allocate.

    Page - Supplies a pointer that will receive the allocated real mode page.

Return Value:

    Status code.

--*/

{

    //
    // This function just returns the same page for every call, depending on the
    // type requested. This code is NOT multithread safe.
    //

    switch (Type) {
    case RealModePageCode:
        if (FwCodePageAllocated != FALSE) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        FwCodePageAllocated = TRUE;
        Page->Type = RealModePageCode;
        Page->RealModeAddress = REAL_MODE_CODE_PAGE;
        Page->Page = (PVOID)REAL_MODE_CODE_PAGE;
        break;

    case RealModePageStack:
        if (FwStackPageAllocated != FALSE) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        FwStackPageAllocated = TRUE;
        Page->Type = RealModePageStack;
        Page->RealModeAddress = REAL_MODE_STACK_PAGE;
        Page->Page = (PVOID)REAL_MODE_STACK_PAGE;
        break;

    case RealModePageData:
        if (FwDataPageAllocated != FALSE) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        FwDataPageAllocated = TRUE;
        Page->Type = RealModePageData;
        Page->RealModeAddress = REAL_MODE_DATA_PAGE;
        Page->Page = (PVOID)REAL_MODE_DATA_PAGE;
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

VOID
FwpRealModeFreePage (
    PREAL_MODE_PAGE Page
    )

/*++

Routine Description:

    This routine frees a page of real mode memory.

Arguments:

    Page - Supplies a pointer to the page to be freed.

Return Value:

    Status code.

--*/

{

    //
    // Since the allocate function always returns the same pages, all that's
    // needed here is to twiddle the globals. Again, this code is NOT multi-
    // thread safe.
    //

    switch (Page->Type) {
    case RealModePageCode:
        FwCodePageAllocated = FALSE;
        break;

    case RealModePageStack:
        FwStackPageAllocated = FALSE;
        break;

    case RealModePageData:
        FwDataPageAllocated = FALSE;
        break;

    default:
        break;
    }

    Page->Type = RealModePageInvalid;
    return;
}

