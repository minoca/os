/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    imnative.c

Abstract:

    This module implements support for the native ELF image format only.

Author:

    Evan Green 26-May-2017

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Set this define to indicate that the private image mux functions should go
// directly to the appropriate ELF function. This avoids having to compile in
// code for a bunch of unused image formats. It's also a smidge faster.
//

#define WANT_IM_NATIVE 1

//
// Add in image.c so that it's compiled while observing WANT_IM_NATIVE being
// defined.
//

#include "image.c"

//
// --------------------------------------------------------------------- Macros
//

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

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
ImGetImageInformation (
    PIMAGE_BUFFER Buffer,
    PIMAGE_INFORMATION Information
    )

/*++

Routine Description:

    This routine gets various pieces of information about an image. This is the
    generic form that can get information from any supported image type.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    Information - Supplies a pointer to the information structure that will be
        filled out by this function. It is assumed the memory pointed to here
        is valid.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_UNKNOWN_IMAGE_FORMAT if the image is unknown or corrupt.

--*/

{

    LOADED_IMAGE Image;
    KSTATUS Status;

    Status = STATUS_UNKNOWN_IMAGE_FORMAT;
    RtlZeroMemory(Information, sizeof(IMAGE_INFORMATION));
    RtlZeroMemory(&Image, sizeof(LOADED_IMAGE));
    Status = ImpGetImageSize(NULL, &Image, Buffer, NULL);
    if (KSUCCESS(Status)) {
        Information->Format = Image.Format;
        Information->Machine = Image.Machine;
        Information->EntryPoint = (UINTN)(Image.EntryPoint);
        Information->ImageBase = (UINTN)(Image.PreferredLowestAddress);
        goto GetImageInformationEnd;
    }

GetImageInformationEnd:
    return Status;
}

BOOL
ImGetImageSection (
    PIMAGE_BUFFER Buffer,
    PSTR SectionName,
    PVOID *Section,
    PULONGLONG VirtualAddress,
    PULONG SectionSizeInFile,
    PULONG SectionSizeInMemory
    )

/*++

Routine Description:

    This routine gets a pointer to the given section in a PE image given a
    memory mapped file.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    SectionName - Supplies the name of the desired section.

    Section - Supplies a pointer where the pointer to the section will be
        returned.

    VirtualAddress - Supplies a pointer where the virtual address of the section
        will be returned, if applicable.

    SectionSizeInFile - Supplies a pointer where the size of the section as it
        appears in the file will be returned.

    SectionSizeInMemory - Supplies a pointer where the size of the section as it
        appears after being loaded in memory will be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    BOOL Result;

    Result = ImpGetSection(Buffer,
                           SectionName,
                           Section,
                           VirtualAddress,
                           SectionSizeInFile,
                           SectionSizeInMemory);

    return Result;
}

IMAGE_FORMAT
ImGetImageFormat (
    PIMAGE_BUFFER Buffer
    )

/*++

Routine Description:

    This routine determines the file format for an image mapped in memory.

Arguments:

    Buffer - Supplies a pointer to the image buffer to determine the type of.

Return Value:

    Returns the file format of the image.

--*/

{

    PVOID ElfHeader;

    if (ImpGetHeader(Buffer, (PVOID)&ElfHeader) != FALSE) {
        return ImageNative;
    }

    //
    // Unknown image format.
    //

    return ImageUnknownFormat;
}

//
// --------------------------------------------------------- Internal Functions
//

