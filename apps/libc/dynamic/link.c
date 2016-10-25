/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    link.c

Abstract:

    This module implements dynamic linker functionality.

Author:

    Evan Green 4-Aug-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <link.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the context used by the dl_iterate_phdr routine.

Members:

    ReturnValue - Supplies the last return value returned by the callback.

    Callback - Supplies the callback function to call.

    Context - Supplies the context pointer to hand to the callback.

--*/

typedef struct _ITERATE_PHDR_CONTEXT {
    int ReturnValue;
    __dl_iterate_phdr_cb_t Callback;
    void *Context;
} ITERATE_PHDR_CONTEXT, *PITERATE_PHDR_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ClpImageIteratorCallback (
    PLOADED_IMAGE Image,
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
dl_iterate_phdr (
    __dl_iterate_phdr_cb_t Callback,
    void *Context
    )

/*++

Routine Description:

    This routine iterates over all of the currently loaded images in the
    process.

Arguments:

    Callback - Supplies a pointer to a function to call for each image loaded,
        including the main executable. The header parameter points at a
        structure containing the image information. The size parameter
        describes the size of the header structure, and the context parameter
        is passed directly through from this routine.

    Context - Supplies an opaque pointer that is passed directly along to the
        callback.

Return Value:

    Returns the last value returned from the callback.

--*/

{

    ITERATE_PHDR_CONTEXT LocalContext;

    LocalContext.ReturnValue = 0;
    LocalContext.Callback = Callback;
    LocalContext.Context = Context;
    OsIterateImages(ClpImageIteratorCallback, &LocalContext);
    return LocalContext.ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ClpImageIteratorCallback (
    PLOADED_IMAGE Image,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called for each loaded image in the process.

Arguments:

    Image - Supplies a pointer to the loaded image.

    Context - Supplies the context pointer that was passed into the iterate
        request function.

Return Value:

    None.

--*/

{

    ElfW(Ehdr) *ElfHeader;
    struct dl_phdr_info Info;
    PITERATE_PHDR_CONTEXT Parameters;

    Parameters = Context;
    Info.dlpi_addr = Image->BaseDifference;
    Info.dlpi_name = Image->FileName;
    ElfHeader = Image->LoadedImageBuffer;
    if (!IS_ELF(*ElfHeader)) {
        return;
    }

    Info.dlpi_phdr = Image->LoadedImageBuffer + ElfHeader->e_phoff;
    Info.dlpi_phnum = ElfHeader->e_phnum;
    Parameters->Callback(&Info, sizeof(Info), Parameters->Context);
    return;
}

