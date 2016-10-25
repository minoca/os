/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pe.c

Abstract:

    This module implements functionality for manipulating Portable Executable
    (PE) binaries.

Author:

    Evan Green 13-Oct-2012

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "imp.h"
#include "pe.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

BOOL
ImpPeGetHeaders (
    PIMAGE_BUFFER Buffer,
    PIMAGE_NT_HEADERS *PeHeaders
    )

/*++

Routine Description:

    This routine returns a pointer to the PE image headers given a buffer
    containing the executable image mapped in memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to get the headers from.

    PeHeaders - Supplies a pointer where the location of the PE headers will
        be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    PIMAGE_DOS_HEADER DosHeader;

    //
    // Read the DOS header to find out where the PE headers are located.
    //

    DosHeader = ImpReadBuffer(NULL, Buffer, 0, sizeof(IMAGE_DOS_HEADER));
    if (DosHeader == NULL) {
        return FALSE;
    }

    if (DosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return FALSE;
    }

    *PeHeaders = ImpReadBuffer(NULL,
                               Buffer,
                               DosHeader->e_lfanew,
                               sizeof(IMAGE_NT_HEADERS));

    if (*PeHeaders == NULL) {
        return FALSE;
    }

    //
    // Perform a few basic checks on the headers to make sure they're valid.
    //

    if (((*PeHeaders)->FileHeader.Characteristics &
         IMAGE_FILE_EXECUTABLE_IMAGE) == 0) {

        return FALSE;
    }

    if ((*PeHeaders)->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return FALSE;
    }

    if ((*PeHeaders)->FileHeader.NumberOfSections == 0) {
        return FALSE;
    }

    return TRUE;
}

BOOL
ImpPeGetSection (
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

    Buffer - Supplies a pointer to the file buffer.

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

    ULONG ItemsScanned;
    KSTATUS KStatus;
    BOOL Match;
    PSTR Name;
    ULONG Offset;
    PIMAGE_NT_HEADERS PeHeaders;
    BOOL Result;
    PVOID ReturnSection;
    ULONG ReturnSectionFileSize;
    ULONG ReturnSectionMemorySize;
    ULONG ReturnSectionVirtualAddress;
    PIMAGE_SECTION_HEADER SectionHeader;
    ULONG SectionIndex;
    PSTR StringTable;
    ULONG StringTableSize;
    PULONG StringTableSizePointer;

    ReturnSection = NULL;
    ReturnSectionFileSize = 0;
    ReturnSectionMemorySize = 0;
    ReturnSectionVirtualAddress = (UINTN)NULL;
    StringTable = NULL;
    StringTableSize = 0;
    if (SectionName == NULL) {
        Result = FALSE;
        goto GetSectionEnd;
    }

    Result = ImpPeGetHeaders(Buffer, &PeHeaders);
    if (Result == FALSE) {
        goto GetSectionEnd;
    }

    //
    // Read in the string table as well.
    //

    if (PeHeaders->FileHeader.PointerToSymbolTable != 0) {
        Offset = PeHeaders->FileHeader.PointerToSymbolTable +
                 (PeHeaders->FileHeader.NumberOfSymbols * sizeof(COFF_SYMBOL));

        StringTableSizePointer = ImpReadBuffer(NULL, Buffer, Offset, 4);
        if (StringTableSizePointer == NULL) {
            Result = FALSE;
            goto GetSectionEnd;
        }

        StringTableSize = *StringTableSizePointer;
        StringTable = ImpReadBuffer(NULL, Buffer, Offset, StringTableSize);
        if (StringTable == NULL) {
            Result = FALSE;
            goto GetSectionEnd;
        }
    }

    //
    // Loop through all sections looking for the desired one.
    //

    SectionHeader = (PIMAGE_SECTION_HEADER)(PeHeaders + 1);
    for (SectionIndex = 0;
         SectionIndex < PeHeaders->FileHeader.NumberOfSections;
         SectionIndex += 1) {

        if (SectionHeader->Name[0] == '/') {
            if (StringTable == NULL) {
                Result = FALSE;
                goto GetSectionEnd;
            }

            KStatus = RtlStringScan((PSTR)(SectionHeader->Name + 1),
                                    IMAGE_SIZEOF_SHORT_NAME - 1,
                                    "%d",
                                    sizeof("%d"),
                                    CharacterEncodingAscii,
                                    &ItemsScanned,
                                    &Offset);

            if ((!KSUCCESS(KStatus)) || (ItemsScanned != 1)) {
                Result = FALSE;
                goto GetSectionEnd;
            }

            Name = StringTable + Offset;
            Match = RtlAreStringsEqual(Name,
                                       SectionName,
                                       StringTableSize - Offset);

        } else {
            Match = RtlAreStringsEqual((PSTR)SectionHeader->Name,
                                       SectionName,
                                       IMAGE_SIZEOF_SHORT_NAME);
        }

        //
        // If the name matches, return that section.
        //

        if (Match != FALSE) {
            ReturnSection = ImpReadBuffer(NULL,
                                          Buffer,
                                          SectionHeader->PointerToRawData,
                                          SectionHeader->SizeOfRawData);

            if (ReturnSection == NULL) {
                Result = FALSE;
                goto GetSectionEnd;
            }

            ReturnSectionFileSize = SectionHeader->SizeOfRawData;
            ReturnSectionMemorySize = SectionHeader->Misc.VirtualSize;

            //
            // The file size seems to always be rounded up to 0x200. Give the
            // more accurate number if possible.
            //

            if (ReturnSectionMemorySize < ReturnSectionFileSize) {
                ReturnSectionFileSize = ReturnSectionMemorySize;
            }

            ReturnSectionVirtualAddress = SectionHeader->VirtualAddress +
                                          PeHeaders->OptionalHeader.ImageBase;

            break;
        }

        SectionHeader += 1;
    }

    Result = TRUE;

GetSectionEnd:
    if (Section != NULL) {
        *Section = ReturnSection;
    }

    if (VirtualAddress != NULL) {
        *VirtualAddress = ReturnSectionVirtualAddress;
    }

    if (SectionSizeInFile != NULL) {
        *SectionSizeInFile = ReturnSectionFileSize;
    }

    if (SectionSizeInMemory != NULL) {
        *SectionSizeInMemory = ReturnSectionMemorySize;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

