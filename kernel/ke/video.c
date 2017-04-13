/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    video.c

Abstract:

    This module implements support for basic printing to the screen.

Author:

    Evan Green 7-Mar-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "keinit.h"
#include <minoca/lib/basevid.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

BASE_VIDEO_CONTEXT KeVideoContext;

//
// ------------------------------------------------------------------ Functions
//

VOID
KeVideoPrintString (
    ULONG XCoordinate,
    ULONG YCoordinate,
    PCSTR String
    )

/*++

Routine Description:

    This routine prints a null-terminated string to the screen at the
    specified location.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    String - Supplies the string to print.

Return Value:

    None.

--*/

{

    VidPrintString(&KeVideoContext, XCoordinate, YCoordinate, String);
    return;
}

VOID
KeVideoPrintHexInteger (
    ULONG XCoordinate,
    ULONG YCoordinate,
    ULONG Number
    )

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location.

Arguments:

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Number - Supplies the signed integer to print.

Return Value:

    None.

--*/

{

    VidPrintHexInteger(&KeVideoContext, XCoordinate, YCoordinate, Number);
    return;
}

VOID
KeVideoPrintInteger (
    ULONG XCoordinate,
    ULONG YCoordinate,
    LONG Number
    )

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Number - Supplies the signed integer to print.

Return Value:

    None.

--*/

{

    VidPrintInteger(&KeVideoContext, XCoordinate, YCoordinate, Number);
    return;
}

VOID
KeVideoClearScreen (
    LONG MinimumX,
    LONG MinimumY,
    LONG MaximumX,
    LONG MaximumY
    )

/*++

Routine Description:

    This routine clears a portion of the video screen.

Arguments:

    MinimumX - Supplies the minimum X coordinate of the rectangle to clear,
        inclusive.

    MinimumY - Supplies the minimum Y coordinate of the rectangle to clear,
        inclusive.

    MaximumX - Supplies the maximum X coordinate of the rectangle to clear,
        exclusive.

    MaximumY - Supplies the maximum Y coordinate of the rectangle to clear,
        exclusive.

Return Value:

    None.

--*/

{

    if (KeVideoContext.FrameBuffer == NULL) {
        return;
    }

    VidClearScreen(&KeVideoContext, MinimumX, MinimumY, MaximumX, MaximumY);
    return;
}

KSTATUS
KeVideoGetDimensions (
    PULONG Width,
    PULONG Height,
    PULONG CellWidth,
    PULONG CellHeight,
    PULONG Columns,
    PULONG Rows
    )

/*++

Routine Description:

    This routine returns the dimensions of the kernel's video frame buffer.

Arguments:

    Width - Supplies an optional pointer where the width in pixels will be
        returned. For text-based frame buffers, this will be equal to the
        number of text columns.

    Height - Supplies an optional pointer where the height in pixels will be
        returned. For text-based frame buffers, this will be equal to the
        number of text rows.

    CellWidth - Supplies an optional pointer where the width in pixels of a
        text character will be returned on success. For text-based frame
        buffers, 1 will be returned.

    CellHeight - Supplies an optional pointer where the height in pixels of a
        text character will be returned on success. For text-based frame
        buffers, 1 will be returned.

    Columns - Supplies an optional pointer where the number of text columns
        will be returned.

    Rows - Supplies an optional pointer where the number of text rows will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_INITIALIZED if there is no frame buffer.

--*/

{

    if (KeVideoContext.FrameBuffer == NULL) {
        return STATUS_NOT_INITIALIZED;
    }

    if (Width != NULL) {
        *Width = KeVideoContext.Width;
    }

    if (Height != NULL) {
        *Height = KeVideoContext.Height;
    }

    if (CellWidth != NULL) {
        *CellWidth = KeVideoContext.Font->CellWidth;
    }

    if (CellHeight != NULL) {
        *CellHeight = KeVideoContext.Font->CellHeight;
    }

    if (Columns != NULL) {
        *Columns = KeVideoContext.Columns;
    }

    if (Rows != NULL) {
        *Rows = KeVideoContext.Rows;
    }

    return STATUS_SUCCESS;
}

KSTATUS
KepInitializeBaseVideo (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes the built in base video library, which is used in
    case of emergencies to display to the screen.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    Status code.

--*/

{

    PSYSTEM_RESOURCE_FRAME_BUFFER FrameBuffer;
    PSYSTEM_RESOURCE_HEADER GenericHeader;
    KSTATUS Status;

    GenericHeader = KepGetSystemResource(SystemResourceFrameBuffer, FALSE);
    if (GenericHeader == NULL) {
        Status = STATUS_SUCCESS;
        goto InitializeBaseVideoEnd;
    }

    FrameBuffer = (PSYSTEM_RESOURCE_FRAME_BUFFER)GenericHeader;
    Status = VidInitialize(&KeVideoContext, FrameBuffer);
    if (!KSUCCESS(Status)) {
        goto InitializeBaseVideoEnd;
    }

    Status = STATUS_SUCCESS;

InitializeBaseVideoEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

