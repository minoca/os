/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    realmode.h

Abstract:

    This header contains definitions for switching back to 16 bit real mode
    from 32-bit (or higher) protected mode.

Author:

    Evan Green 18-Jul-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define DEFAULT_FLAGS (IA32_EFLAG_ALWAYS_1 | IA32_EFLAG_IF)

//
// --------------------------------------------------------------------- Macros
//

//
// The ADDRESS_TO_SEGMENT macro converts a 32-bit address into a segment. It is
// assumed that the address being passed is 16-byte aligned.
//

#define ADDRESS_TO_SEGMENT(_Address) ((_Address) >> 4)

//
// The SEGMENTED_TO_LINEAR macro converts a segment:offset pair into a linear
// address.
//

#define SEGMENTED_TO_LINEAR(_Selector, _Offset)  \
    (((_Selector) << 4) + (_Offset))

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _REAL_MODE_PAGE_TYPE {
    RealModePageInvalid,
    RealModePageCode,
    RealModePageData,
    RealModePageStack
} REAL_MODE_PAGE_TYPE, *PREAL_MODE_PAGE_TYPE;

/*++

Structure Description:

    This structure describes a page of memory that has been designated for use
    during real mode.

Members:

    Type - Stores the intended use of this page (code, stack, or data page).

    RealModeAddress - Stores the "real-mode" address of the page. This is a
        linear address.

    Page - Stores a pointer to the page.

--*/

typedef struct _REAL_MODE_PAGE {
    REAL_MODE_PAGE_TYPE Type;
    ULONG RealModeAddress;
    PVOID Page;
} REAL_MODE_PAGE, *PREAL_MODE_PAGE;

/*++

Structure Description:

    This structure defines a real mode context, including all code, data, and
    stack memory, and registers.

Members:

    CodePage - Stores the code page information of the real mode operation.

    DataPage - Stores the data page of the real mode operation.

    StackPage - Stores the stack page of the real mode operation.

    Registers - Stores the register state of the real mode context. Upon exit,
        these fields contain the final register values.

--*/

typedef struct _REAL_MODE_CONTEXT {
    REAL_MODE_PAGE CodePage;
    REAL_MODE_PAGE DataPage;
    REAL_MODE_PAGE StackPage;
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    ULONG Esi;
    ULONG Edi;
    ULONG Esp;
    ULONG Ebp;
    ULONG Eip;
    ULONG Eflags;
    ULONG Cs;
    ULONG Ds;
    ULONG Es;
    ULONG Fs;
    ULONG Gs;
    ULONG Ss;
} REAL_MODE_CONTEXT, *PREAL_MODE_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

extern UCHAR FwpRealModeBiosCallTemplate;
extern UCHAR FwpRealModeBiosCallTemplateLongJump;
extern UCHAR FwpRealModeBiosCallTemplateLongJump2;
extern UCHAR FwpRealModeBiosCallTemplateLongJump3;
extern UCHAR FwpRealModeBiosCallTemplateIntInstruction;
extern UCHAR FwpRealModeBiosCallTemplateEnd;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
FwpRealModeCreateBiosCallContext (
    PREAL_MODE_CONTEXT Context,
    UCHAR InterruptNumber
    );

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

VOID
FwpRealModeReinitializeBiosCallContext (
    PREAL_MODE_CONTEXT Context
    );

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

VOID
FwpRealModeDestroyBiosCallContext (
    PREAL_MODE_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys a created BIOS call context.

Arguments:

    Context - Supplies a pointer to the context structure that will be
        freed.

Return Value:

    None.

--*/

VOID
FwpRealModeExecute (
    PREAL_MODE_CONTEXT Context
    );

/*++

Routine Description:

    This routine executes 16-bit real mode code by switching the processor back
    to real mode.

Arguments:

    Context - Supplies a pointer to the context structure that will be
        executed. On return, this will contain the executed context.

Return Value:

    None.

--*/

