/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    coff.c

Abstract:

    This module handles parsing COFF symbol tables, used in PE images.

Author:

    Evan Green 5-Sep-2012

Environment:

    Debug

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "pe.h"
#include "symbols.h"
#include "stabs.h"

#include <assert.h>
#include <errno.h>
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

typedef struct _COFF_SECTION COFF_SECTION, *PCOFF_SECTION;
struct _COFF_SECTION {
    PCOFF_SECTION Next;
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

VOID
DbgpCoffFreeSymbols (
    PDEBUG_SYMBOLS Symbols
    );

BOOL
DbgpLoadCoffSymbolTable (
    PDEBUG_SYMBOLS Symbols,
    PSTR Filename,
    PCOFF_SECTION *SectionList
    );

BOOL
DbgpParseCoffSymbolTable (
    PDEBUG_SYMBOLS Symbols,
    PCOFF_SECTION SectionList
    );

PSTR
DbgpGetCoffSymbolName (
    PCOFF_SYMBOL Symbol,
    PDEBUG_SYMBOLS SymbolData,
    BOOL TruncateLeadingUnderscore
    );

BOOL
DbgpCreateOrUpdateCoffSymbol (
    PDEBUG_SYMBOLS Symbols,
    PCOFF_SYMBOL CoffSymbol,
    PSTR Name,
    ULONGLONG Value
    );

//
// -------------------------------------------------------------------- Globals
//

DEBUG_SYMBOL_INTERFACE DbgCoffSymbolInterface = {
    DbgpCoffLoadSymbols,
    DbgpCoffFreeSymbols,
    NULL,
    NULL,
    NULL,
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

INT
DbgpCoffLoadSymbols (
    PSTR Filename,
    IMAGE_MACHINE_TYPE MachineType,
    ULONG Flags,
    PVOID HostContext,
    PDEBUG_SYMBOLS *Symbols
    )

/*++

Routine Description:

    This routine loads debugging symbol information from the specified file.

Arguments:

    Filename - Supplies the name of the binary to load symbols from.

    MachineType - Supplies the required machine type of the image. Set to
        unknown to allow the symbol library to load a file with any machine
        type.

    Flags - Supplies a bitfield of flags governing the behavior during load.
        These flags are specific to each symbol library.

    HostContext - Supplies the value to store in the host context field of the
        debug symbols.

    Symbols - Supplies an optional pointer where a pointer to the symbols will
        be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN AllocationSize;
    PDEBUG_SYMBOLS CoffSymbols;
    BOOL Result;
    INT Status;

    AllocationSize = sizeof(DEBUG_SYMBOLS) + sizeof(STAB_CONTEXT);
    CoffSymbols = MALLOC(AllocationSize);
    if (CoffSymbols == NULL) {
        Status = ENOMEM;
        goto CoffLoadSymbolsEnd;
    }

    memset(CoffSymbols, 0, AllocationSize);
    INITIALIZE_LIST_HEAD(&(CoffSymbols->SourcesHead));
    CoffSymbols->Interface = &DbgCoffSymbolInterface;
    CoffSymbols->SymbolContext = CoffSymbols + 1;
    CoffSymbols->HostContext = HostContext;
    Result = DbgpLoadCoffSymbols(CoffSymbols, Filename);
    if (Result == FALSE) {
        Status = EINVAL;
        goto CoffLoadSymbolsEnd;
    }

    Status = 0;

CoffLoadSymbolsEnd:
    if (Status != 0) {
        if (CoffSymbols != NULL) {
            DbgpCoffFreeSymbols(CoffSymbols);
            CoffSymbols = NULL;
        }
    }

    *Symbols = CoffSymbols;
    return 0;
}

BOOL
DbgpLoadCoffSymbols (
    PDEBUG_SYMBOLS Symbols,
    PSTR Filename
    )

/*++

Routine Description:

    This routine loads COFF symbols into a pre-existing set of debug symbols.

Arguments:

    Symbols - Supplies a pointer to debug symbols that are assumed to have
        already been allocated and initialized.

    Filename - Supplies the name of the file to load COFF symbols for.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PCOFF_SECTION CurrentSection;
    PCOFF_SECTION NextSection;
    BOOL Result;
    PCOFF_SECTION SectionList;

    SectionList = NULL;
    Result = DbgpLoadCoffSymbolTable(Symbols, Filename, &SectionList);
    if (Result == FALSE) {
        goto LoadCoffSymbolsEnd;
    }

    Result = DbgpParseCoffSymbolTable(Symbols, SectionList);
    if (Result == FALSE) {
        goto LoadCoffSymbolsEnd;
    }

LoadCoffSymbolsEnd:

    //
    // Free the section list if one was created.
    //

    CurrentSection = SectionList;
    while (CurrentSection != NULL) {
        NextSection = CurrentSection->Next;
        FREE(CurrentSection);
        CurrentSection = NextSection;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DbgpCoffFreeSymbols (
    PDEBUG_SYMBOLS Symbols
    )

/*++

Routine Description:

    This routine frees all memory associated with an instance of debugging
    symbols. Once called, the pointer passed in should not be dereferenced
    again by the caller.

Arguments:

    Symbols - Supplies a pointer to the debugging symbols.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentFunctionEntry;
    PLIST_ENTRY CurrentGlobalEntry;
    PLIST_ENTRY CurrentSourceEntry;
    PFUNCTION_SYMBOL Function;
    PDATA_SYMBOL GlobalVariable;
    PLIST_ENTRY NextFunctionEntry;
    PSOURCE_FILE_SYMBOL SourceFile;
    PSTAB_CONTEXT StabContext;

    StabContext = Symbols->SymbolContext;
    if (Symbols->Filename != NULL) {
        FREE(Symbols->Filename);
    }

    if (StabContext->RawSymbolTable != NULL) {
        FREE(StabContext->RawSymbolTable);
    }

    if (StabContext->RawSymbolTableStrings != NULL) {
        FREE(StabContext->RawSymbolTableStrings);
    }

    //
    // Free Source files.
    //

    CurrentSourceEntry = Symbols->SourcesHead.Next;
    while ((CurrentSourceEntry != &(Symbols->SourcesHead)) &&
           (CurrentSourceEntry != NULL)) {

        SourceFile = LIST_VALUE(CurrentSourceEntry,
                                SOURCE_FILE_SYMBOL,
                                ListEntry);

        assert(LIST_EMPTY(&(SourceFile->TypesHead)));

        //
        // Free functions.
        //

        CurrentFunctionEntry = SourceFile->FunctionsHead.Next;
        while (CurrentFunctionEntry != &(SourceFile->FunctionsHead)) {
            Function = LIST_VALUE(CurrentFunctionEntry,
                                  FUNCTION_SYMBOL,
                                  ListEntry);

            assert(LIST_EMPTY(&(Function->ParametersHead)));
            assert(LIST_EMPTY(&(Function->LocalsHead)));
            assert(LIST_EMPTY(&(Function->FunctionsHead)));

            if (Function->Name != NULL) {
                FREE(Function->Name);
            }

            NextFunctionEntry = CurrentFunctionEntry->Next;
            FREE(Function);
            CurrentFunctionEntry = NextFunctionEntry;
        }

        assert(LIST_EMPTY(&(SourceFile->SourceLinesHead)));

        //
        // Free global/static symbols.
        //

        CurrentGlobalEntry = SourceFile->DataSymbolsHead.Next;
        while (CurrentGlobalEntry != &(SourceFile->DataSymbolsHead)) {
            GlobalVariable = LIST_VALUE(CurrentGlobalEntry,
                                        DATA_SYMBOL,
                                        ListEntry);

            if (GlobalVariable->Name != NULL) {
                FREE(GlobalVariable->Name);
            }

            CurrentGlobalEntry = CurrentGlobalEntry->Next;
            FREE(GlobalVariable);
        }

        CurrentSourceEntry = CurrentSourceEntry->Next;
        FREE(SourceFile);
    }

    FREE(Symbols);
    return;
}

BOOL
DbgpLoadCoffSymbolTable (
    PDEBUG_SYMBOLS Symbols,
    PSTR Filename,
    PCOFF_SECTION *SectionList
    )

/*++

Routine Description:

    This routine loads the raw COFF symbol table out of the file.

Arguments:

    Symbols - Supplies a pointer to debug symbols that are assumed to have
        already been allocated and initialized.

    Filename - Supplies the name of the file to load COFF symbols for.

    SectionList - Supplies a pointer that will receive a pointer to the list of
        sections in the COFF file. Most COFF symbol values are relative to a
        section like .text or .bss.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG BytesRead;
    PIMAGE_SECTION_HEADER CurrentSection;
    FILE *File;
    PVOID FileBuffer;
    ULONG FileSize;
    PCOFF_SECTION FirstSection;
    ULONG ImageBase;
    IMAGE_BUFFER ImageBuffer;
    PCOFF_SECTION NewSectionEntry;
    PCOFF_SECTION NextSectionEntry;
    PIMAGE_NT_HEADERS PeHeader;
    BOOL Result;
    ULONG SectionIndex;
    PUCHAR Source;
    ULONG SourceSize;
    PSTAB_CONTEXT StabContext;

    StabContext = Symbols->SymbolContext;
    CurrentSection = NULL;
    FileBuffer = NULL;
    FirstSection = NULL;
    memset(&ImageBuffer, 0, sizeof(IMAGE_BUFFER));
    StabContext->RawSymbolTable = NULL;
    StabContext->RawSymbolTableStrings = NULL;

    //
    // Determine the file size and load the file into memory.
    //

    File = fopen(Filename, "rb");
    if (File == NULL) {
        Result = FALSE;
        goto LoadCoffSymbolTableEnd;
    }

    FileSize = DbgpGetFileSize(File);
    if (FileSize <= 0) {
        Result = FALSE;
        goto LoadCoffSymbolTableEnd;
    }

    FileBuffer = MALLOC(FileSize);
    if (FileBuffer == NULL) {
        Result = FALSE;
        goto LoadCoffSymbolTableEnd;
    }

    BytesRead = fread(FileBuffer, 1, FileSize, File);
    if (BytesRead != FileSize) {
        Result = FALSE;
        goto LoadCoffSymbolTableEnd;
    }

    ImageBuffer.Data = FileBuffer;
    ImageBuffer.Size = FileSize;

    //
    // Get the PE headers to determine the location of the symbol table.
    //

    Result = ImpPeGetHeaders(&ImageBuffer, &PeHeader);
    if (Result == FALSE) {
        goto LoadCoffSymbolTableEnd;
    }

    Source = FileBuffer;
    Source += PeHeader->FileHeader.PointerToSymbolTable;
    SourceSize = PeHeader->FileHeader.NumberOfSymbols * sizeof(COFF_SYMBOL);

    //
    // Allocate space for the symbol table and copy it in.
    //

    StabContext->RawSymbolTableSize = SourceSize;
    StabContext->RawSymbolTable = MALLOC(SourceSize);
    if (StabContext->RawSymbolTable == NULL) {
        Result = FALSE;
        goto LoadCoffSymbolTableEnd;
    }

    memcpy(StabContext->RawSymbolTable, Source, SourceSize);

    //
    // Find the string table, which is right after the symbol table, allocate
    // memory for it, and copy it in. Note that the first four bytes contain
    // the total size of the string table, but those four bytes should be
    // treated as 0 when reading strings from the string table.
    //

    Source += SourceSize;
    SourceSize = *((PULONG)Source);
    StabContext->RawSymbolTableStringsSize = SourceSize;
    StabContext->RawSymbolTableStrings = MALLOC(SourceSize);
    if (StabContext->RawSymbolTableStrings == NULL) {
        Result = FALSE;
        goto LoadCoffSymbolTableEnd;
    }

    memcpy(StabContext->RawSymbolTableStrings, Source, SourceSize);

    //
    // Set the first four bytes to 0.
    //

    *((PULONG)(StabContext->RawSymbolTableStrings)) = 0;

    //
    // Create the section list.
    //

    ImageBase = PeHeader->OptionalHeader.ImageBase;
    CurrentSection = (PIMAGE_SECTION_HEADER)(PeHeader + 1);
    for (SectionIndex = 0;
         SectionIndex < PeHeader->FileHeader.NumberOfSections;
         SectionIndex += 1) {

        //
        // Skip the section if its not even loaded into memory.
        //

        if ((CurrentSection->Characteristics &
             IMAGE_SCN_MEM_DISCARDABLE) != 0) {

            CurrentSection += 1;
            continue;
        }

        //
        // Allocate space for the new entry and fill it out. Sections according
        // to COFF symbols are 1 based.
        //

        NewSectionEntry = MALLOC(sizeof(COFF_SECTION));
        if (NewSectionEntry == NULL) {
            Result = FALSE;
            goto LoadCoffSymbolTableEnd;
        }

        RtlZeroMemory(NewSectionEntry, sizeof(COFF_SECTION));
        NewSectionEntry->SectionIndex = SectionIndex + 1;
        NewSectionEntry->SectionAddress = ImageBase +
                                          CurrentSection->VirtualAddress;

        //
        // Link the new section at the head of the list.
        //

        NewSectionEntry->Next = FirstSection;
        FirstSection = NewSectionEntry;

        //
        // Advance to the next section.
        //

        CurrentSection += 1;
    }

    Result = TRUE;

LoadCoffSymbolTableEnd:
    if (FileBuffer != NULL) {
        FREE(FileBuffer);
    }

    if (Result == FALSE) {
        if (StabContext->RawSymbolTable != NULL) {
            FREE(StabContext->RawSymbolTable);
        }

        if (StabContext->RawSymbolTableStrings != NULL) {
            FREE(StabContext->RawSymbolTableStrings);
        }

        //
        // Free all section entries.
        //

        if (FirstSection != NULL) {
            NewSectionEntry = FirstSection;
            while (NewSectionEntry != NULL) {
                NextSectionEntry = NewSectionEntry->Next;
                FREE(NewSectionEntry);
                NewSectionEntry = NextSectionEntry;
            }
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
DbgpParseCoffSymbolTable (
    PDEBUG_SYMBOLS Symbols,
    PCOFF_SECTION SectionList
    )

/*++

Routine Description:

    This routine parses COFF symbol tables and combines them with existing
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

    ULONG BytesRead;
    PCOFF_SECTION CurrentSection;
    PSTR Name;
    PSTAB_CONTEXT StabContext;
    PCOFF_SYMBOL Symbol;
    BOOL SymbolCreated;
    ULONGLONG SymbolValue;

    BytesRead = 0;
    StabContext = Symbols->SymbolContext;

    //
    // Validate that the symbol tables are there.
    //

    if ((Symbols == NULL) || (StabContext->RawSymbolTable == NULL) ||
        (StabContext->RawSymbolTableStrings == NULL)) {

        return FALSE;
    }

    Symbol = (PCOFF_SYMBOL)(StabContext->RawSymbolTable);
    while (BytesRead + sizeof(COFF_SYMBOL) <= StabContext->RawSymbolTableSize) {
        SymbolValue = 0;

        //
        // Attempt to find the section matching this symbol. If none can be
        // found, ignore the symbol. Negative section indices are possible, but
        // these symbols are usually not that useful.
        //

        if ((SHORT)Symbol->Section > 0) {
            CurrentSection = SectionList;
            while (CurrentSection != NULL) {
                if (CurrentSection->SectionIndex == Symbol->Section) {
                    break;
                }

                CurrentSection = CurrentSection->Next;
            }

            if (CurrentSection != NULL) {
                SymbolValue = Symbol->Value + CurrentSection->SectionAddress;
            }
        }

        //
        // Skip all symbols except class 2 symbols, which represent C_EXT
        // external symbols.
        //

        if (Symbol->Class != 2) {
            SymbolValue = 0;
        }

        //
        // If a valid value was found for the symbol, attempt to get its name.
        //

        if (SymbolValue != 0) {
            Name = DbgpGetCoffSymbolName(Symbol, Symbols, TRUE);

            //
            // If the symbol has a valid name, attempt to add it.
            //

            if (Name != NULL) {
                SymbolCreated = DbgpCreateOrUpdateCoffSymbol(Symbols,
                                                             Symbol,
                                                             Name,
                                                             SymbolValue);

                if (SymbolCreated == FALSE) {
                    FREE(Name);
                }
            }
        }

        //
        // Skip over any extra data this symbol may have in multiples of
        // COFF symbol entries.
        //

        if (Symbol->AuxCount != 0) {
            BytesRead += Symbol->AuxCount * sizeof(COFF_SYMBOL);
            Symbol += Symbol->AuxCount;
        }

        //
        // Go to the next symbol in the table.
        //

        BytesRead += sizeof(COFF_SYMBOL);
        Symbol += 1;
    }

    return TRUE;
}

PSTR
DbgpGetCoffSymbolName (
    PCOFF_SYMBOL Symbol,
    PDEBUG_SYMBOLS SymbolData,
    BOOL TruncateLeadingUnderscore
    )

/*++

Routine Description:

    This routine gets the name of the given COFF symbol. The caller is
    responsible for freeing memory returned here.

Arguments:

    Symbol - Supplies a pointer to the COFF symbol to get the name of.

    SymbolData - Supplies a pointer to debug symbols containing a valid
        symbol table and string table.

    TruncateLeadingUnderscore - Supplies a boolean indicating whether or not
        to truncate leading underscores at the beginning of symbol names. If
        this flag is TRUE and the symbol name is found to begin with an
        underscore, this underscore will be removed.

Return Value:

    Returns a pointer to a newly allocated buffer containing the name of the
    symbol on success.

    NULL on failure or if the symbol has no name.

--*/

{

    ULONG Length;
    PSTR Name;
    PSTAB_CONTEXT StabContext;
    PSTR StringTable;

    StabContext = SymbolData->SymbolContext;
    Name = NULL;
    StringTable = StabContext->RawSymbolTableStrings;

    //
    // If the symbol name has its zeroes field zeroed, then use the offset
    // into the symbol table.
    //

    if (Symbol->Zeroes == 0) {
        if (StringTable == NULL) {
            goto GetCoffSymbolNameEnd;
        }

        if (Symbol->Offset >= StabContext->RawSymbolTableStringsSize) {
            goto GetCoffSymbolNameEnd;
        }

        Length = strlen(StringTable + Symbol->Offset);
        Name = MALLOC(Length + 1);
        if (Name == NULL) {
            goto GetCoffSymbolNameEnd;
        }

        strcpy(Name, StringTable + Symbol->Offset);
        Name[Length] = '\0';

    //
    // If the symbol name does not have its zeroes field zeroed, then the name
    // is baked right into the symbol. Note that it is only NULL terminated if
    // there is room for the NULL terminator.
    //

    } else {
        Name = MALLOC(COFF_SYMBOL_NAME_LENGTH + 1);
        if (Name == NULL) {
            goto GetCoffSymbolNameEnd;
        }

        strncpy(Name, Symbol->Name, COFF_SYMBOL_NAME_LENGTH);
        Name[COFF_SYMBOL_NAME_LENGTH] = '\0';
    }

    //
    // If leading underscores are to be truncated, look for that now.
    //

    if ((TruncateLeadingUnderscore != FALSE) && (Name != NULL) &&
        (Name[0] == '_')) {

        strcpy(Name, Name + 1);
    }

GetCoffSymbolNameEnd:
    return Name;
}

BOOL
DbgpCreateOrUpdateCoffSymbol (
    PDEBUG_SYMBOLS Symbols,
    PCOFF_SYMBOL CoffSymbol,
    PSTR Name,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine adds a symbol to the debug symbols or updates a value if its
    currently set as NULL given the COFF symbol.

Arguments:

    Symbols - Supplies a pointer to the debug symbols where the new symbol will
        be stored.

    CoffSymbol - Supplies a pointer to the new COFF symbol to be put in.

    Name - Supplies a pointer to the symbol name.

    Value - Supplies a pointer to the symbol value.

Return Value:

    TRUE if a new symbol was created and the Name buffer should not be freed.

    FALSE if an existing symbol was updated, or no symbol was generated at all.

--*/

{

    PSOURCE_FILE_SYMBOL CurrentSource;
    PLIST_ENTRY CurrentSourceEntry;
    PSOURCE_FILE_SYMBOL FunctionParent;
    PFUNCTION_SYMBOL NewFunction;
    SYMBOL_SEARCH_RESULT Result;
    PSYMBOL_SEARCH_RESULT ResultPointer;
    BOOL SymbolAdded;

    RtlZeroMemory(&Result, sizeof(SYMBOL_SEARCH_RESULT));
    Result.Variety = SymbolResultInvalid;
    SymbolAdded = FALSE;

    //
    // A symbol with type 0x20 indicates a function.
    //

    if ((CoffSymbol->Type & 0xF0) == 0x20) {
        ResultPointer = DbgFindFunctionSymbol(Symbols, Name, 0, &Result);

        //
        // If the function does not exist, create it. For now, only create new
        // functions, don't update existing ones.
        //

        if (ResultPointer == NULL) {

            //
            // Attempt to find the source file this belongs under. Loop through
            // all source files looking for one whose address range contains
            // this function.
            //

            CurrentSourceEntry = Symbols->SourcesHead.Next;
            FunctionParent = NULL;
            while (CurrentSourceEntry != &(Symbols->SourcesHead)) {
                CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                           SOURCE_FILE_SYMBOL,
                                           ListEntry);

                if ((CurrentSource->StartAddress <= Value) &&
                    (CurrentSource->EndAddress > Value)) {

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
                goto CreateOrUpdateCoffSymbolEnd;
            }

            NewFunction = MALLOC(sizeof(FUNCTION_SYMBOL));
            if (NewFunction == NULL) {
                goto CreateOrUpdateCoffSymbolEnd;
            }

            RtlZeroMemory(NewFunction, sizeof(FUNCTION_SYMBOL));
            NewFunction->ParentSource = FunctionParent;
            NewFunction->Name = Name;
            NewFunction->FunctionNumber = 1000;
            INITIALIZE_LIST_HEAD(&(NewFunction->ParametersHead));
            INITIALIZE_LIST_HEAD(&(NewFunction->LocalsHead));
            INITIALIZE_LIST_HEAD(&(NewFunction->FunctionsHead));
            NewFunction->StartAddress = Value;
            NewFunction->EndAddress = Value + 0x20;
            NewFunction->ReturnTypeNumber = 0;
            NewFunction->ReturnTypeOwner = NULL;

            //
            // Insert the function into the current source file's list of
            // functions.
            //

            INSERT_BEFORE(&(NewFunction->ListEntry),
                          &(FunctionParent->FunctionsHead));

            SymbolAdded = TRUE;
        }

    //
    // Assume everything that's not a function is data, a global.
    //

    } else {
        ResultPointer = DbgFindDataSymbol(Symbols, Name, 0, &Result);

        //
        // If it exists and it's current value is NULL, update it. For now,
        // only update, do not create globals.
        //

        if ((ResultPointer != NULL) &&
            (Result.U.DataResult->LocationType ==
             DataLocationAbsoluteAddress) &&
            (Result.U.DataResult->Location.Address == 0)) {

            Result.U.DataResult->Location.Address = Value;
        }
    }

CreateOrUpdateCoffSymbolEnd:
    return SymbolAdded;
}

