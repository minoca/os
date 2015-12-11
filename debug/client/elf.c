/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    elf.c

Abstract:

    This module handles parsing ELF symbol tables for the debugger.

Author:

    Evan Green 14-Sep-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/im.h>
#include "elf.h"
#include "symbols.h"
#include "dbgext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MALLOC(_x) malloc(_x)
#define FREE(_x) free(_x)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _ELF_SECTION ELF_SECTION, *PELF_SECTION;
struct _ELF_SECTION {
    PELF_SECTION Next;
    LONG SectionIndex;
    ULONG SectionAddress;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

LONG
DbgpGetFileSize (
    FILE *File
    );

BOOL
DbgpLoadElfSymbolTable (
    PDEBUG_SYMBOLS Symbols,
    PSTR Filename,
    PELF_SECTION *SectionList
    );

BOOL
DbgpParseElfSymbolTable (
    PDEBUG_SYMBOLS Symbols,
    PELF_SECTION SectionList
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

BOOL
DbgpLoadElfSymbols (
    PDEBUG_SYMBOLS Symbols,
    PSTR Filename
    )

/*++

Routine Description:

    This routine loads COFF symbols into a pre-existing set of ELF symbols.

Arguments:

    Symbols - Supplies a pointer to debug symbols that are assumed to have
        already been allocated and initialized.

    Filename - Supplies the name of the file to load ELF symbols for.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PELF_SECTION FirstSection;
    PELF_SECTION NextSection;
    BOOL Result;

    FirstSection = NULL;
    Result = DbgpLoadElfSymbolTable(Symbols, Filename, &FirstSection);
    if (Result == FALSE) {
        DbgOut("Error reading ELF symbol table.\n");
        goto LoadElfSymbolsEnd;
    }

    Result = DbgpParseElfSymbolTable(Symbols, FirstSection);
    if (Result == FALSE) {
        DbgOut("Error parsing ELF symbol table.\n");
        goto LoadElfSymbolsEnd;
    }

LoadElfSymbolsEnd:
    while (FirstSection != NULL) {
        NextSection = FirstSection->Next;
        FREE(FirstSection);
        FirstSection = NextSection;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
DbgpLoadElfSymbolTable (
    PDEBUG_SYMBOLS Symbols,
    PSTR Filename,
    PELF_SECTION *SectionList
    )

/*++

Routine Description:

    This routine loads the raw ELF symbol table out of the file.

Arguments:

    Symbols - Supplies a pointer to debug symbols that are assumed to have
        already been allocated and initialized.

    Filename - Supplies the name of the file to load ELF symbols for.

    SectionList - Supplies a pointer that will receive a pointer to the list of
        sections in the ELF file. Most ELF symbol values are relative to a
        section like .text or .bss.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG BytesRead;
    PELF32_SECTION_HEADER CurrentSection;
    PELF32_HEADER ElfHeader;
    FILE *File;
    PVOID FileBuffer;
    ULONG FileSize;
    PELF_SECTION FirstSection;
    PELF32_SECTION_HEADER FirstSectionHeader;
    IMAGE_BUFFER ImageBuffer;
    PELF_SECTION NewSection;
    PELF_SECTION NextSection;
    BOOL Result;
    ULONG SectionIndex;
    PUCHAR Source;
    PELF32_SECTION_HEADER StringSection;
    PELF32_SECTION_HEADER SymbolSection;

    CurrentSection = NULL;
    FileBuffer = NULL;
    FirstSection = NULL;
    memset(&ImageBuffer, 0, sizeof(IMAGE_BUFFER));
    SymbolSection = NULL;
    Symbols->RawSymbolTable = NULL;
    Symbols->RawSymbolTableStrings = NULL;
    *SectionList = NULL;

    //
    // Determine the file size and load the file into memory.
    //

    File = fopen(Filename, "rb");
    if (File == NULL) {
        Result = FALSE;
        goto LoadElfSymbolTableEnd;
    }

    FileSize = DbgpGetFileSize(File);
    if (FileSize <= 0) {
        Result = FALSE;
        goto LoadElfSymbolTableEnd;
    }

    FileBuffer = MALLOC(FileSize);
    if (FileBuffer == NULL) {
        Result = FALSE;
        goto LoadElfSymbolTableEnd;
    }

    BytesRead = fread(FileBuffer, 1, FileSize, File);
    if (BytesRead != FileSize) {
        Result = FALSE;
        goto LoadElfSymbolTableEnd;
    }

    ImageBuffer.Data = FileBuffer;
    ImageBuffer.Size = FileSize;

    //
    // Get the ELF headers to determine the location of the sections.
    //

    Result = ImpElfGetHeader(&ImageBuffer, &ElfHeader);
    if (Result == FALSE) {
        goto LoadElfSymbolTableEnd;
    }

    FirstSectionHeader =
                     (PELF32_SECTION_HEADER)((PUCHAR)FileBuffer +
                                             ElfHeader->SectionHeaderOffset);

    CurrentSection = FirstSectionHeader;
    for (SectionIndex = 0;
         SectionIndex < ElfHeader->SectionHeaderCount;
         SectionIndex += 1) {

        //
        // Remember the symbols section.
        //

        if (CurrentSection->SectionType == ELF_SECTION_TYPE_SYMBOLS) {
            SymbolSection = CurrentSection;
        }

        //
        // Skip non-loadable sections.
        //

        if ((CurrentSection->Flags & ELF_SECTION_FLAG_LOAD) == 0) {
            CurrentSection += 1;
            continue;
        }

        //
        // Record this section and its address for easy access later.
        //

        NewSection = MALLOC(sizeof(ELF_SECTION));
        if (NewSection == NULL) {
            Result = FALSE;
            goto LoadElfSymbolTableEnd;
        }

        RtlZeroMemory(NewSection, sizeof(ELF_SECTION));
        NewSection->SectionIndex = SectionIndex;
        NewSection->SectionAddress = CurrentSection->VirtualAddress;
        NewSection->Next = FirstSection;
        FirstSection = NewSection;
        NewSection = NULL;
        CurrentSection += 1;
    }

    //
    // If no symbol section was found, error out.
    //

    if (SymbolSection == NULL) {
        Result = FALSE;
        goto LoadElfSymbolTableEnd;
    }

    //
    // Get the string table for this symbol table, stored in the Link field.
    //

    if ((SymbolSection->Link == 0) ||
        (SymbolSection->Link >= ElfHeader->SectionHeaderCount)) {

        Result = FALSE;
        goto LoadElfSymbolTableEnd;
    }

    StringSection = FirstSectionHeader + SymbolSection->Link;
    if (StringSection->SectionType != ELF_SECTION_TYPE_STRINGS) {
        Result = FALSE;
        goto LoadElfSymbolTableEnd;
    }

    Symbols->RawSymbolTableSize = SymbolSection->Size;
    Symbols->RawSymbolTableStringsSize = StringSection->Size;
    Symbols->RawSymbolTable = MALLOC(Symbols->RawSymbolTableSize);
    if (Symbols->RawSymbolTable == NULL) {
        Result = FALSE;
        goto LoadElfSymbolTableEnd;
    }

    Symbols->RawSymbolTableStrings = MALLOC(Symbols->RawSymbolTableStringsSize);
    if (Symbols->RawSymbolTableStrings == NULL) {
        Result = FALSE;
        goto LoadElfSymbolTableEnd;
    }

    Source = (PUCHAR)FileBuffer + SymbolSection->Offset;
    RtlCopyMemory(Symbols->RawSymbolTable, Source, Symbols->RawSymbolTableSize);
    Source = (PUCHAR)FileBuffer + StringSection->Offset;
    RtlCopyMemory(Symbols->RawSymbolTableStrings,
                  Source,
                  Symbols->RawSymbolTableStringsSize);

    Result = TRUE;

LoadElfSymbolTableEnd:
    if (FileBuffer != NULL) {
        FREE(FileBuffer);
    }

    if (Result == FALSE) {
        if (Symbols->RawSymbolTable != NULL) {
            FREE(Symbols->RawSymbolTable);
        }

        if (Symbols->RawSymbolTableStrings != NULL) {
            FREE(Symbols->RawSymbolTableStrings);
        }

        //
        // Free all section entries.
        //

        while (FirstSection != NULL) {
            NextSection = FirstSection->Next;
            FREE(FirstSection);
            FirstSection = NextSection;
        }

    //
    // If successful, return the section list.
    //

    } else {
        *SectionList = FirstSection;
    }

    if (File != NULL) {
        fclose(File);
    }

    return Result;
}

BOOL
DbgpParseElfSymbolTable (
    PDEBUG_SYMBOLS Symbols,
    PELF_SECTION SectionList
    )

/*++

Routine Description:

    This routine parses ELF symbol tables and combines them with existing
    debug symbols.

Arguments:

    Symbols - Supplies a pointer to debug symbols that are assumed to have
        already been allocated and initialized. The raw symbol tables and
        string table are expected to be valid.

    SectionList - Supplies a list of all loadable section in the image. Most
        COFF symbols are relative to a section, so this is needed to determine
        the real address.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PELF_SECTION CurrentSection;
    PSOURCE_FILE_SYMBOL CurrentSource;
    PLIST_ENTRY CurrentSourceEntry;
    PELF32_SYMBOL CurrentSymbol;
    PSOURCE_FILE_SYMBOL FunctionParent;
    PFUNCTION_SYMBOL NewFunction;
    SYMBOL_SEARCH_RESULT Result;
    PSYMBOL_SEARCH_RESULT ResultPointer;
    BOOL Status;
    ULONG SymbolAddress;
    PELF32_SYMBOL SymbolEnd;
    PSTR SymbolName;
    PSTR SymbolNameCopy;
    ELF32_SYMBOL_TYPE SymbolType;

    Status = TRUE;
    NewFunction = NULL;
    SymbolNameCopy = NULL;
    SymbolEnd = (PELF32_SYMBOL)((PUCHAR)Symbols->RawSymbolTable +
                                Symbols->RawSymbolTableSize);

    CurrentSymbol = (PELF32_SYMBOL)Symbols->RawSymbolTable;
    while (CurrentSymbol + 1 <= SymbolEnd) {
        Result.Variety = SymbolResultInvalid;
        SymbolAddress = 0;
        SymbolType = ELF32_EXTRACT_SYMBOL_TYPE(CurrentSymbol->Information);
        SymbolName = (PSTR)Symbols->RawSymbolTableStrings +
                     CurrentSymbol->NameOffset;

        //
        // Create a copy of the symbol name.
        //

        SymbolNameCopy = MALLOC(strlen(SymbolName) + 1);
        if (SymbolNameCopy == NULL) {
            Status = FALSE;
            goto ParseElfSymbolTableEnd;
        }

        strcpy(SymbolNameCopy, SymbolName);

        //
        // Attempt to find the section address this symbol relates to.
        //

        if (CurrentSymbol->SectionIndex != 0) {
            CurrentSection = SectionList;
            while (CurrentSection != NULL) {
                if (CurrentSection->SectionIndex ==
                    CurrentSymbol->SectionIndex) {

                    SymbolAddress = CurrentSection->SectionAddress;
                    break;
                }

                CurrentSection = CurrentSection->Next;
            }
        }

        switch (SymbolType) {
        case ElfSymbolFunction:
            SymbolAddress += CurrentSymbol->Value;

            //
            // Don't add symbols with no value.
            //

            if (SymbolAddress == 0) {
                break;
            }

            ResultPointer =
                        DbgFindFunctionSymbol(Symbols, SymbolName, 0, &Result);

            //
            // If the function does not exist, create it. For now, only create
            // new functions, don't update existing ones.
            //

            if (ResultPointer == NULL) {

                //
                // Attempt to find the source file this belongs under. Loop
                // through all source files looking for one whose address range
                // contains this function.
                //

                CurrentSourceEntry = Symbols->SourcesHead.Next;
                FunctionParent = NULL;
                while (CurrentSourceEntry != &(Symbols->SourcesHead)) {
                    CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                               SOURCE_FILE_SYMBOL,
                                               ListEntry);

                    if ((CurrentSource->StartAddress <= SymbolAddress) &&
                        (CurrentSource->EndAddress > SymbolAddress)) {

                        FunctionParent = CurrentSource;
                        break;
                    }

                    CurrentSourceEntry = CurrentSourceEntry->Next;
                }

                //
                // If a parent source could not be found, there's nowhere to add
                // this function to.
                //

                if (FunctionParent == NULL) {
                    break;
                }

                NewFunction = MALLOC(sizeof(FUNCTION_SYMBOL));
                if (NewFunction == NULL) {
                    Status = FALSE;
                    break;
                }

                RtlZeroMemory(NewFunction, sizeof(FUNCTION_SYMBOL));
                NewFunction->ParentSource = FunctionParent;
                NewFunction->Name = SymbolNameCopy;
                NewFunction->FunctionNumber = 1000;
                INITIALIZE_LIST_HEAD(&(NewFunction->ParametersHead));
                INITIALIZE_LIST_HEAD(&(NewFunction->LocalsHead));
                NewFunction->StartAddress = SymbolAddress;
                NewFunction->EndAddress = SymbolAddress + 0x20;
                NewFunction->ReturnTypeNumber = 0;
                NewFunction->ReturnTypeOwner = NULL;

                //
                // Insert the function into the current source file's list of
                // functions.
                //

                INSERT_BEFORE(&(NewFunction->ListEntry),
                              &(FunctionParent->FunctionsHead));

                //
                // Null out the allocated variables so they don't get freed by
                // the end of this function.
                //

                NewFunction = NULL;
                SymbolNameCopy = NULL;
            }

            break;

        case ElfSymbolObject:
            SymbolAddress = CurrentSymbol->Value;
            ResultPointer = DbgFindDataSymbol(Symbols, SymbolName, 0, &Result);

            //
            // If it exists and it's current value is NULL, update it. For now,
            // only update, do not create globals.
            //

            if ((ResultPointer != NULL) &&
                (Result.U.DataResult->LocationType ==
                 DataLocationAbsoluteAddress) &&
                (Result.U.DataResult->Location.Address == (UINTN)NULL)) {

                Result.U.DataResult->Location.Address = SymbolAddress;
            }

            break;

        //
        // Ignore other symbol types.
        //

        default:
            break;
        }

        //
        // Free the symbol name copy if it wasn't used.
        //

        if (SymbolNameCopy != NULL) {
            FREE(SymbolNameCopy);
            SymbolNameCopy = NULL;
        }

        //
        // Break on failure.
        //

        if (Status == FALSE) {
            break;
        }

        CurrentSymbol += 1;
    }

ParseElfSymbolTableEnd:
    if (SymbolNameCopy != NULL) {
        FREE(SymbolNameCopy);
    }

    if (NewFunction != NULL) {
        FREE(NewFunction);
    }

    return Status;
}

