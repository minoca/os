/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    dwarf.c

Abstract:

    This module implements support for parsing DWARF symbols, versions 2+.

Author:

    Evan Green 2-Dec-2015

Environment:

    Debug

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/types.h>
#include <minoca/status.h>
#include <minoca/im.h>
#include "dwarfp.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

VOID
DwarfUnloadSymbols (
    PDEBUG_SYMBOLS Symbols
    );

INT
DwarfpProcessDebugInfo (
    PDWARF_CONTEXT Context
    );

INT
DwarfpProcessCompilationUnit (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit
    );

INT
DwarfpProcessDie (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessCompileUnit (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessBaseType (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessTypeRelation (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessSubrangeType (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessStructureUnionEnumerationType (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessMember (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessEnumerator (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessSubroutineType (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessSubprogram (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessVariable (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

INT
DwarfpProcessGenericBlock (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

//
// -------------------------------------------------------------------- Globals
//

DEBUG_SYMBOL_INTERFACE DwarfSymbolInterface = {
    DwarfLoadSymbols,
    DwarfUnloadSymbols,
    DwarfStackUnwind
};

//
// ------------------------------------------------------------------ Functions
//

INT
DwarfLoadSymbols (
    PSTR Filename,
    IMAGE_MACHINE_TYPE MachineType,
    ULONG Flags,
    PVOID HostContext,
    PDEBUG_SYMBOLS *Symbols
    )

/*++

Routine Description:

    This routine loads DWARF symbols for the given file.

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
    PDWARF_CONTEXT Context;
    PDEBUG_SYMBOLS DwarfSymbols;
    FILE *File;
    IMAGE_BUFFER ImageBuffer;
    IMAGE_INFORMATION ImageInformation;
    KSTATUS KStatus;
    size_t Read;
    PDWARF_DEBUG_SECTIONS Sections;
    struct stat Stat;
    INT Status;

    DwarfSymbols = NULL;
    Status = stat(Filename, &Stat);
    if (Status != 0) {
        Status = errno;
        return Status;
    }

    //
    // Allocate and initialize the top level data structures.
    //

    AllocationSize = sizeof(DEBUG_SYMBOLS) + sizeof(DWARF_CONTEXT);
    DwarfSymbols = malloc(AllocationSize);
    if (DwarfSymbols == NULL) {
        Status = ENOMEM;
        goto LoadSymbolsEnd;
    }

    memset(DwarfSymbols, 0, AllocationSize);
    INITIALIZE_LIST_HEAD(&(DwarfSymbols->SourcesHead));
    DwarfSymbols->Filename = strdup(Filename);
    DwarfSymbols->SymbolContext = DwarfSymbols + 1;
    DwarfSymbols->Interface = &DwarfSymbolInterface;
    DwarfSymbols->HostContext = HostContext;
    Context = DwarfSymbols->SymbolContext;
    Context->SourcesHead = &(DwarfSymbols->SourcesHead);
    Context->Flags = Flags;
    INITIALIZE_LIST_HEAD(&(Context->UnitList));
    Context->FileData = malloc(Stat.st_size);
    if (Context->FileData == NULL) {
        Status = errno;
        goto LoadSymbolsEnd;
    }

    Context->FileSize = Stat.st_size;

    //
    // Read in the file.
    //

    File = fopen(Filename, "rb");
    if (File == NULL) {
        Status = errno;
        goto LoadSymbolsEnd;
    }

    Read = fread(Context->FileData, 1, Stat.st_size, File);
    fclose(File);
    if (Read != Stat.st_size) {
        DWARF_ERROR("Read only %d of %d bytes.\n", Read, Stat.st_size);
        Status = errno;
        goto LoadSymbolsEnd;
    }

    //
    // Fill in the image information, and check against the desired machine
    // type if set before going to all the trouble of fully loading symbols.
    //

    ImageBuffer.Context = NULL;
    ImageBuffer.Data = Context->FileData;
    ImageBuffer.Size = Context->FileSize;
    KStatus = ImGetImageInformation(&ImageBuffer, &ImageInformation);
    if (!KSUCCESS(KStatus)) {
        Status = ENOEXEC;
        goto LoadSymbolsEnd;
    }

    DwarfSymbols->ImageBase = ImageInformation.ImageBase;
    DwarfSymbols->Machine = ImageInformation.Machine;
    DwarfSymbols->ImageFormat = ImageInformation.Format;
    if ((MachineType != ImageMachineTypeUnknown) &&
        (MachineType != DwarfSymbols->Machine)) {

        DWARF_ERROR("DWARF: File %s has machine type %d, expecting %d.\n",
                    Filename,
                    DwarfSymbols->Machine,
                    MachineType);

        Status = ENOEXEC;
        goto LoadSymbolsEnd;
    }

    //
    // Find the important DWARF sections.
    //

    Sections = &(Context->Sections);
    ImGetImageSection(&ImageBuffer,
                      ".debug_info",
                      &(Sections->Info.Data),
                      NULL,
                      &(Sections->Info.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_abbrev",
                      &(Sections->Abbreviations.Data),
                      NULL,
                      &(Sections->Abbreviations.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_str",
                      &(Sections->Strings.Data),
                      NULL,
                      &(Sections->Strings.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_loc",
                      &(Sections->Locations.Data),
                      NULL,
                      &(Sections->Locations.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_aranges",
                      &(Sections->Aranges.Data),
                      NULL,
                      &(Sections->Aranges.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_ranges",
                      &(Sections->Ranges.Data),
                      NULL,
                      &(Sections->Ranges.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_macinfo",
                      &(Sections->Macros.Data),
                      NULL,
                      &(Sections->Macros.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_line",
                      &(Sections->Lines.Data),
                      NULL,
                      &(Sections->Lines.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_pubnames",
                      &(Sections->PubNames.Data),
                      NULL,
                      &(Sections->PubNames.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_pubtypes",
                      &(Sections->PubTypes.Data),
                      NULL,
                      &(Sections->PubTypes.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_types",
                      &(Sections->Types.Data),
                      NULL,
                      &(Sections->Types.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".debug_frame",
                      &(Sections->Frame.Data),
                      NULL,
                      &(Sections->Frame.Size),
                      NULL);

    ImGetImageSection(&ImageBuffer,
                      ".eh_frame",
                      &(Sections->EhFrame.Data),
                      &(Sections->EhFrameAddress),
                      &(Sections->EhFrame.Size),
                      NULL);

    if ((Sections->Info.Data == NULL) ||
        (Sections->Abbreviations.Data == NULL)) {

        Status = EINVAL;
        goto LoadSymbolsEnd;
    }

    //
    // Parse the .debug_info section, which contains most of the good bits.
    //

    Status = DwarfpProcessDebugInfo(Context);
    if (Status != 0) {
        goto LoadSymbolsEnd;
    }

    Status = 0;

LoadSymbolsEnd:
    if (Status != 0) {
        if (DwarfSymbols != NULL) {
            DwarfUnloadSymbols(DwarfSymbols);
            DwarfSymbols = NULL;
        }
    }

    *Symbols = DwarfSymbols;
    return Status;
}

VOID
DwarfUnloadSymbols (
    PDEBUG_SYMBOLS Symbols
    )

/*++

Routine Description:

    This routine frees all memory associated with an instance of debugging
    symbols, including the symbols structure itsefl.

Arguments:

    Symbols - Supplies a pointer to the debugging symbols.

Return Value:

    None.

--*/

{

    PDWARF_CONTEXT Context;
    PDATA_SYMBOL DataSymbol;
    PENUMERATION_MEMBER Enumeration;
    PFUNCTION_SYMBOL Function;
    PSOURCE_LINE_SYMBOL Line;
    PSTRUCTURE_MEMBER Member;
    PVOID Next;
    PSOURCE_FILE_SYMBOL SourceFile;
    PTYPE_SYMBOL Type;
    PDWARF_COMPILATION_UNIT Unit;

    Context = Symbols->SymbolContext;

    //
    // Destroy all the sources.
    //

    while (!LIST_EMPTY(Context->SourcesHead)) {
        SourceFile = LIST_VALUE(Context->SourcesHead->Next,
                                SOURCE_FILE_SYMBOL,
                                ListEntry);

        while (!LIST_EMPTY(&(SourceFile->TypesHead))) {
            Type = LIST_VALUE(SourceFile->TypesHead.Next,
                              TYPE_SYMBOL,
                              ListEntry);

            if (Type->Type == DataTypeStructure) {
                Member = Type->U.Structure.FirstMember;
                while (Member != NULL) {
                    Next = Member->NextMember;
                    free(Member);
                    Member = Next;
                }

            } else if (Type->Type == DataTypeEnumeration) {
                Enumeration = Type->U.Enumeration.FirstMember;
                while (Enumeration != NULL) {
                    Next = Enumeration->NextMember;
                    free(Enumeration);
                    Enumeration = Next;
                }
            }

            LIST_REMOVE(&(Type->ListEntry));
            free(Type);
        }

        while (!LIST_EMPTY(&(SourceFile->FunctionsHead))) {
            Function = LIST_VALUE(SourceFile->FunctionsHead.Next,
                                  FUNCTION_SYMBOL,
                                  ListEntry);

            while (!LIST_EMPTY(&(Function->ParametersHead))) {
                DataSymbol = LIST_VALUE(Function->ParametersHead.Next,
                                        DATA_SYMBOL,
                                        ListEntry);

                LIST_REMOVE(&(DataSymbol->ListEntry));
                free(DataSymbol);
            }

            while (!LIST_EMPTY(&(Function->LocalsHead))) {
                DataSymbol = LIST_VALUE(Function->LocalsHead.Next,
                                        DATA_SYMBOL,
                                        ListEntry);

                LIST_REMOVE(&(DataSymbol->ListEntry));
                free(DataSymbol);
            }

            LIST_REMOVE(&(Function->ListEntry));
            free(Function);
        }

        while (!LIST_EMPTY(&(SourceFile->DataSymbolsHead))) {
            DataSymbol = LIST_VALUE(SourceFile->DataSymbolsHead.Next,
                                    DATA_SYMBOL,
                                    ListEntry);

            LIST_REMOVE(&(DataSymbol->ListEntry));
            free(DataSymbol);
        }

        while (!LIST_EMPTY(&(SourceFile->SourceLinesHead))) {
            Line = LIST_VALUE(SourceFile->SourceLinesHead.Next,
                              SOURCE_LINE_SYMBOL,
                              ListEntry);

            LIST_REMOVE(&(Line->ListEntry));
            free(Line);
        }

        LIST_REMOVE(&(SourceFile->ListEntry));
        free(SourceFile);
    }

    //
    // Destroy all the compilation units.
    //

    if (Context->UnitList.Next != NULL) {
        while (!LIST_EMPTY(&(Context->UnitList))) {
            Unit = LIST_VALUE(Context->UnitList.Next,
                              DWARF_COMPILATION_UNIT,
                              ListEntry);

            LIST_REMOVE(&(Unit->ListEntry));
            Unit->ListEntry.Next = NULL;
            DwarfpDestroyCompilationUnit(Context, Unit);
        }
    }

    if (Context->FileData != NULL) {
        free(Context->FileData);
        Context->FileData = NULL;
    }

    Context->FileSize = 0;
    if (Symbols->Filename != NULL) {
        free(Symbols->Filename);
    }

    free(Symbols);
    return;
}

PSOURCE_FILE_SYMBOL
DwarfpFindSource (
    PDWARF_CONTEXT Context,
    PSTR Directory,
    PSTR FileName,
    BOOL Create
    )

/*++

Routine Description:

    This routine searches for a source file symbol matching the given directory
    and file name.

Arguments:

    Context - Supplies a pointer to the application context.

    Directory - Supplies a pointer to the source directory.

    FileName - Supplies a pointer to the source file name.

    Create - Supplies a boolean indicating if a source file should be
        created if none is found.

Return Value:

    Returns a pointer to a source file symbol on success.

    NULL if no such file exists.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSOURCE_FILE_SYMBOL File;

    CurrentEntry = Context->SourcesHead->Next;
    while (CurrentEntry != Context->SourcesHead) {
        File = LIST_VALUE(CurrentEntry, SOURCE_FILE_SYMBOL, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Check the directory, being careful since one or both might be NULL.
        //

        if (Directory != NULL) {
            if (File->SourceDirectory == NULL) {
                continue;
            }

            if (strcmp(File->SourceDirectory, Directory) != 0) {
                continue;
            }

        } else if (File->SourceDirectory != NULL) {
            continue;
        }

        if (strcmp(File->SourceFile, FileName) == 0) {
            return File;
        }
    }

    if (Create == FALSE) {
        return NULL;
    }

    File = malloc(sizeof(SOURCE_FILE_SYMBOL));
    if (File == NULL) {
        return NULL;
    }

    memset(File, 0, sizeof(SOURCE_FILE_SYMBOL));
    INITIALIZE_LIST_HEAD(&(File->SourceLinesHead));
    INITIALIZE_LIST_HEAD(&(File->DataSymbolsHead));
    INITIALIZE_LIST_HEAD(&(File->FunctionsHead));
    INITIALIZE_LIST_HEAD(&(File->TypesHead));
    File->SourceDirectory = Directory;
    File->SourceFile = FileName;
    INSERT_BEFORE(&(File->ListEntry), Context->SourcesHead);
    return File;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DwarfpProcessDebugInfo (
    PDWARF_CONTEXT Context
    )

/*++

Routine Description:

    This routine processes the .debug_info section of DWARF symbols.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PUCHAR Bytes;
    PDWARF_DIE Die;
    PUCHAR InfoStart;
    DWARF_LOADING_CONTEXT LoadState;
    ULONGLONG Size;
    INT Status;
    PDWARF_COMPILATION_UNIT Unit;

    Bytes = Context->Sections.Info.Data;
    InfoStart = Bytes;
    Size = Context->Sections.Info.Size;
    Status = 0;
    Unit = NULL;
    memset(&LoadState, 0, sizeof(DWARF_LOADING_CONTEXT));
    Context->LoadingContext = &LoadState;

    //
    // Load up and visit all the compilation units.
    //

    while (Size != 0) {
        Unit = malloc(sizeof(DWARF_COMPILATION_UNIT));
        if (Unit == NULL) {
            Status = errno;
            goto ProcessDebugInfoEnd;
        }

        memset(Unit, 0, sizeof(DWARF_COMPILATION_UNIT));
        INITIALIZE_LIST_HEAD(&(Unit->DieList));
        DwarfpReadCompilationUnit(&Bytes, &Size, Unit);
        if ((Context->Flags & DWARF_CONTEXT_DEBUG) != 0) {
            DWARF_PRINT("Compilation Unit %x: %s Version %d UnitLength %I64x "
                        "AbbrevOffset %I64x AddressSize %d DIEs %x\n",
                        Bytes - InfoStart,
                        Unit->Is64Bit ? "64-bit" : "32-bit",
                        Unit->Version,
                        Unit->UnitLength,
                        Unit->AbbreviationOffset,
                        Unit->AddressSize,
                        Unit->Dies - InfoStart);
        }

        Status = DwarfpLoadCompilationUnit(Context, Unit);
        if (Status != 0) {
            goto ProcessDebugInfoEnd;
        }

        //
        // Now visit the compilation unit now that the DIE tree has been formed.
        //

        Status = DwarfpProcessCompilationUnit(Context, Unit);
        if (Status != 0) {
            DWARF_ERROR("DWARF: Failed to process compilation unit.\n");
            goto ProcessDebugInfoEnd;
        }

        while (!LIST_EMPTY(&(Unit->DieList))) {
            Die = LIST_VALUE(Unit->DieList.Next, DWARF_DIE, ListEntry);
            LIST_REMOVE(&(Die->ListEntry));
            Die->ListEntry.Next = NULL;
            DwarfpDestroyDie(Context, Die);
        }

        INSERT_BEFORE(&(Unit->ListEntry), &(Context->UnitList));
        Unit = NULL;
    }

    Status = 0;

ProcessDebugInfoEnd:
    Context->LoadingContext = NULL;
    if (Unit != NULL) {
        DwarfpDestroyCompilationUnit(Context, Unit);
    }

    return Status;
}

INT
DwarfpProcessCompilationUnit (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit
    )

/*++

Routine Description:

    This routine processes the a DWARF compilation unit.

Arguments:

    Context - Supplies a pointer to the application context.

    Unit - Supplies a pointer to the compilation unit to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDWARF_DIE Die;
    PDWARF_LOADING_CONTEXT LoadState;
    INT Status;

    Status = 0;
    LoadState = Context->LoadingContext;

    assert((LoadState->CurrentFile == NULL) &&
           (LoadState->CurrentFunction == NULL) &&
           (LoadState->CurrentType == NULL));

    LoadState->CurrentUnit = Unit;
    CurrentEntry = Unit->DieList.Next;
    while (CurrentEntry != &(Unit->DieList)) {
        Die = LIST_VALUE(CurrentEntry, DWARF_DIE, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        assert(Die->Parent == NULL);

        Status = DwarfpProcessDie(Context, Die);
        if (Status != 0) {
            break;
        }
    }

    LoadState->CurrentUnit = NULL;
    return Status;
}

INT
DwarfpProcessDie (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes the a DWARF Debug Information Entry.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;

    switch (Die->Tag) {
    case DwarfTagCompileUnit:
        Status = DwarfpProcessCompileUnit(Context, Die);
        break;

    case DwarfTagBaseType:
        Status = DwarfpProcessBaseType(Context, Die);
        break;

    case DwarfTagTypedef:
    case DwarfTagPointerType:
    case DwarfTagArrayType:
    case DwarfTagVolatileType:
    case DwarfTagRestrictType:
    case DwarfTagConstType:
    case DwarfTagReferenceType:
        Status = DwarfpProcessTypeRelation(Context, Die);
        break;

    case DwarfTagSubrangeType:
        Status = DwarfpProcessSubrangeType(Context, Die);
        break;

    case DwarfTagStructureType:
    case DwarfTagUnionType:
    case DwarfTagEnumerationType:
    case DwarfTagClassType:
        Status = DwarfpProcessStructureUnionEnumerationType(Context, Die);
        break;

    case DwarfTagMember:
        Status = DwarfpProcessMember(Context, Die);
        break;

    case DwarfTagEnumerator:
        Status = DwarfpProcessEnumerator(Context, Die);
        break;

    case DwarfTagSubprogram:
        Status = DwarfpProcessSubprogram(Context, Die);
        break;

    case DwarfTagFormalParameter:
    case DwarfTagVariable:
        Status = DwarfpProcessVariable(Context, Die);
        break;

    case DwarfTagSubroutineType:
        Status = DwarfpProcessSubroutineType(Context, Die);
        break;

    case DwarfTagNamespace:
    case DwarfTagLexicalBlock:
        Status = DwarfpProcessGenericBlock(Context, Die);
        break;

    default:
        Status = 0;
        break;
    }

    if (Status != 0) {
        DWARF_ERROR("DWARF: Failed to process DIE %x.\n",
                    DWARF_DIE_ID(Context, Die));
    }

    return Status;
}

INT
DwarfpProcessChildDies (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes the child DIEs of a given DIE.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE whose children should be processed.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDWARF_DIE Child;
    PLIST_ENTRY CurrentEntry;
    INT Status;

    CurrentEntry = Die->ChildList.Next;
    while (CurrentEntry != &(Die->ChildList)) {
        Child = LIST_VALUE(CurrentEntry, DWARF_DIE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Status = DwarfpProcessDie(Context, Child);
        if (Status != 0) {
            break;
        }
    }

    return Status;
}

INT
DwarfpProcessCompileUnit (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a compile unit DIE.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDWARF_LOADING_CONTEXT LoadingContext;
    BOOL Result;
    PSOURCE_FILE_SYMBOL SourceFile;
    INT Status;

    LoadingContext = Context->LoadingContext;
    SourceFile = DwarfpFindSource(Context,
                                  DwarfpGetStringAttribute(Die, DwarfAtCompDir),
                                  DwarfpGetStringAttribute(Die, DwarfAtName),
                                  TRUE);

    if (SourceFile == NULL) {
        return ENOMEM;
    }

    SourceFile->Identifier = DWARF_DIE_ID(Context, Die);
    SourceFile->SymbolContext = LoadingContext->CurrentUnit;

    //
    // Get the starting PC for the compilation unit. There might not be one
    // if this compilation unit has no code (only data).
    //

    Result = DwarfpGetAddressAttribute(Die,
                                       DwarfAtLowPc,
                                       &(SourceFile->StartAddress));

    if (Result != FALSE) {
        SourceFile->EndAddress = SourceFile->StartAddress + 1;
        DwarfpGetAddressAttribute(Die,
                                  DwarfAtHighPc,
                                  &(SourceFile->EndAddress));
    }

    //
    // Update the low and high PC values in the compilation unit structure.
    // They're used by the location list search routine, for instance.
    //

    assert((LoadingContext->CurrentUnit != NULL) &&
           (LoadingContext->CurrentUnit->LowPc == 0));

    LoadingContext->CurrentUnit->LowPc = SourceFile->StartAddress;
    LoadingContext->CurrentUnit->HighPc = SourceFile->EndAddress;

    //
    // Set the current file as this one, and process all children.
    //

    assert(LoadingContext->CurrentFile == NULL);

    LoadingContext->CurrentFile = SourceFile;
    Status = DwarfpProcessChildDies(Context, Die);
    if (Status != 0) {
        goto ProcessCompileUnitEnd;
    }

    //
    // Process the line numbers if there are any.
    //

    Status = DwarfpProcessStatementList(Context, Die);
    if (Status != 0) {
        goto ProcessCompileUnitEnd;
    }

ProcessCompileUnitEnd:

    assert(LoadingContext->CurrentFile == SourceFile);

    LoadingContext->CurrentFile = NULL;
    return Status;
}

INT
DwarfpProcessBaseType (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a base type DIE.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONGLONG Encoding;
    PDWARF_LOADING_CONTEXT LoadingContext;
    DATA_TYPE_NUMERIC Numeric;
    PTYPE_SYMBOL PreviousType;
    BOOL Result;
    ULONGLONG Size;
    INT Status;
    PTYPE_SYMBOL Type;

    LoadingContext = Context->LoadingContext;
    memset(&Numeric, 0, sizeof(DATA_TYPE_NUMERIC));
    Result = DwarfpGetIntegerAttribute(Die, DwarfAtEncoding, &Encoding);
    if (Result != FALSE) {
        switch ((ULONG)Encoding) {
        case DwarfAteAddress:
            Numeric.BitSize = LoadingContext->CurrentUnit->AddressSize *
                              BITS_PER_BYTE;

            break;

        case DwarfAteBoolean:
        case DwarfAteUnsigned:
        case DwarfAteUnsignedChar:
            break;

        case DwarfAteFloat:
            Numeric.Float = TRUE;
            break;

        case DwarfAteSigned:
        case DwarfAteSignedChar:
            Numeric.Signed = TRUE;
            break;

        //
        // Treat unhandled types like integers.
        //

        case DwarfAteComplexFloat:
        case DwarfAteImaginaryFloat:
        case DwarfAtePackedDecimal:
        case DwarfAteNumericString:
        case DwarfAteEdited:
        case DwarfAteSignedFixed:
        case DwarfAteUnsignedFixed:
        case DwarfAteDecimalFloat:
        case DwarfAteUtf:
        default:
            DWARF_ERROR("DWARF: Unknown base type encoding %d.\n",
                        (ULONG)Encoding);

            break;
        }

    } else {
        DWARF_ERROR("DWARF: Failed to get base type attribute.\n");
        return 0;
    }

    Result = DwarfpGetIntegerAttribute(Die, DwarfAtByteSize, &Size);
    if (Result != FALSE) {
        Size *= BITS_PER_BYTE;

    } else {
        Result = DwarfpGetIntegerAttribute(Die, DwarfAtBitSize, &Size);
    }

    if (Result == FALSE) {
        DWARF_ERROR("DWARF: Unknown base type size.\n");
        return 0;
    }

    Numeric.BitSize = Size;
    Type = malloc(sizeof(TYPE_SYMBOL));
    if (Type == NULL) {
        return ENOMEM;
    }

    memset(Type, 0, sizeof(TYPE_SYMBOL));
    Type->ParentSource = LoadingContext->CurrentFile;
    PreviousType = LoadingContext->CurrentType;
    LoadingContext->CurrentType = Type;
    Type->ParentFunction = LoadingContext->CurrentFunction;
    Type->Name = DwarfpGetStringAttribute(Die, DwarfAtName);
    Type->TypeNumber = DWARF_DIE_ID(Context, Die);
    Type->Type = DataTypeNumeric;
    memcpy(&(Type->U.Numeric), &Numeric, sizeof(DATA_TYPE_NUMERIC));
    INSERT_BEFORE(&(Type->ListEntry),
                  &(LoadingContext->CurrentFile->TypesHead));

    Status = DwarfpProcessChildDies(Context, Die);

    assert(LoadingContext->CurrentType == Type);

    LoadingContext->CurrentType = PreviousType;
    return Status;
}

INT
DwarfpProcessTypeRelation (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a typedef, pointer, or array.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDWARF_LOADING_CONTEXT LoadingContext;
    PTYPE_SYMBOL PreviousType;
    DATA_TYPE_RELATION Relation;
    BOOL Result;
    INT Status;
    PTYPE_SYMBOL Type;

    LoadingContext = Context->LoadingContext;
    memset(&Relation, 0, sizeof(DATA_TYPE_RELATION));
    if (Die->Tag == DwarfTagPointerType) {
        Relation.Pointer = TRUE;
    }

    //
    // Get the type information that corresponds to this reference.
    //

    Result = DwarfpGetTypeReferenceAttribute(Context,
                                             Die,
                                             DwarfAtType,
                                             &(Relation.OwningFile),
                                             &(Relation.TypeNumber));

    if (Result == FALSE) {
        DWARF_ERROR("DWARF: Unable to resolve type.\n");
        return EINVAL;
    }

    Type = malloc(sizeof(TYPE_SYMBOL));
    if (Type == NULL) {
        return ENOMEM;
    }

    memset(Type, 0, sizeof(TYPE_SYMBOL));
    Type->ParentSource = LoadingContext->CurrentFile;
    PreviousType = LoadingContext->CurrentType;
    LoadingContext->CurrentType = Type;
    Type->ParentFunction = LoadingContext->CurrentFunction;
    Type->Name = DwarfpGetStringAttribute(Die, DwarfAtName);
    Type->TypeNumber = DWARF_DIE_ID(Context, Die);
    Type->Type = DataTypeRelation;
    memcpy(&(Type->U.Relation), &Relation, sizeof(DATA_TYPE_RELATION));
    INSERT_BEFORE(&(Type->ListEntry),
                  &(LoadingContext->CurrentFile->TypesHead));

    Status = DwarfpProcessChildDies(Context, Die);

    assert(LoadingContext->CurrentType == Type);

    LoadingContext->CurrentType = PreviousType;
    return Status;
}

INT
DwarfpProcessSubrangeType (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a subrange type DIE.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDWARF_LOADING_CONTEXT LoadingContext;
    BOOL Result;
    INT Status;
    ULONGLONG UpperBound;

    LoadingContext = Context->LoadingContext;

    //
    // Try to get the upper bound of the array. If there is no upper bound,
    // then make the array into a pointer.
    //

    Result = DwarfpGetIntegerAttribute(Die, DwarfAtUpperBound, &UpperBound);
    if (Result == FALSE) {
        LoadingContext->CurrentType->U.Relation.Pointer = TRUE;
        return 0;
    }

    if (LoadingContext->CurrentType != NULL) {
        if (LoadingContext->CurrentType->Type != DataTypeRelation) {
            DWARF_ERROR("DWARF: Subrange type on a non-relation data type.\n");
            return EINVAL;
        }

        LoadingContext->CurrentType->U.Relation.Array.Maximum = UpperBound + 1;
        if (UpperBound == MAX_ULONGLONG) {
            LoadingContext->CurrentType->U.Relation.Array.MaxUlonglong = TRUE;
        }

    } else {
        DWARF_ERROR("DWARF: Subrange type not inside a type.\n");
        return EINVAL;
    }

    Status = DwarfpProcessChildDies(Context, Die);
    return Status;
}

INT
DwarfpProcessStructureUnionEnumerationType (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a structure, union, or enumeration DIE.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDWARF_LOADING_CONTEXT LoadingContext;
    PTYPE_SYMBOL PreviousType;
    BOOL Result;
    ULONGLONG Size;
    INT Status;
    PTYPE_SYMBOL Type;

    LoadingContext = Context->LoadingContext;

    //
    // Get the size. If this is a declaration, there might not be one.
    //

    Result = DwarfpGetIntegerAttribute(Die, DwarfAtByteSize, &Size);
    if (Result == FALSE) {
        Size = 0;
    }

    Type = malloc(sizeof(TYPE_SYMBOL));
    if (Type == NULL) {
        return ENOMEM;
    }

    memset(Type, 0, sizeof(TYPE_SYMBOL));
    Type->ParentSource = LoadingContext->CurrentFile;
    PreviousType = LoadingContext->CurrentType;
    LoadingContext->CurrentType = Type;
    Type->ParentFunction = LoadingContext->CurrentFunction;
    Type->Name = DwarfpGetStringAttribute(Die, DwarfAtName);
    Type->TypeNumber = DWARF_DIE_ID(Context, Die);
    if ((Die->Tag == DwarfTagStructureType) ||
        (Die->Tag == DwarfTagUnionType) ||
        (Die->Tag == DwarfTagClassType)) {

        Type->Type = DataTypeStructure;
        Type->U.Structure.SizeInBytes = Size;

    } else {

        assert(Die->Tag == DwarfTagEnumerationType);

        Type->Type = DataTypeEnumeration;
        Type->U.Enumeration.SizeInBytes = Size;
    }

    INSERT_BEFORE(&(Type->ListEntry),
                  &(LoadingContext->CurrentFile->TypesHead));

    Status = DwarfpProcessChildDies(Context, Die);

    assert(LoadingContext->CurrentType == Type);

    LoadingContext->CurrentType = PreviousType;
    return Status;
}

INT
DwarfpProcessMember (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a structure or union member.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONGLONG BitOffset;
    ULONGLONG BitSize;
    PDWARF_LOADING_CONTEXT LoadingContext;
    PDWARF_ATTRIBUTE_VALUE LocationAttribute;
    DWARF_LOCATION_CONTEXT LocationContext;
    PSTRUCTURE_MEMBER Member;
    PSTRUCTURE_MEMBER PreviousMember;
    BOOL Result;
    INT Status;
    ULONGLONG StorageSize;
    PTYPE_SYMBOL Structure;

    BitOffset = 0;
    BitSize = 0;
    LoadingContext = Context->LoadingContext;

    //
    // Try to get the bit size, and if it's not there try to get the byte size.
    //

    Result = DwarfpGetIntegerAttribute(Die, DwarfAtBitSize, &BitSize);
    if (Result == FALSE) {
        Result = DwarfpGetIntegerAttribute(Die, DwarfAtByteSize, &BitSize);
        if (Result != FALSE) {
            BitSize *= BITS_PER_BYTE;
        }
    }

    //
    // Get the bit offset. Try for a data bit offset, and fall back to the
    // older bit offset if not found.
    //

    Result = DwarfpGetIntegerAttribute(Die, DwarfAtDataBitOffset, &BitOffset);
    if (Result == FALSE) {
        Result = DwarfpGetIntegerAttribute(Die, DwarfAtBitOffset, &BitOffset);
        if (Result != FALSE) {

            //
            // If there's a bit offset and a bit size, there needs to be a byte
            // size to determine storage unit size.
            //

            Result = DwarfpGetIntegerAttribute(Die,
                                               DwarfAtByteSize,
                                               &StorageSize);

            if (Result == FALSE) {
                DWARF_ERROR("DWARF: BitOffset with no ByteOffset.\n");
                return EINVAL;
            }

            StorageSize *= BITS_PER_BYTE;

            //
            // The old bit offset definition defines the highest order bit in
            // use as an offset from the storage unit size. Turn that around
            // into an offset from the start of the member.
            //

            assert(BitOffset + BitSize <= StorageSize);

            BitOffset = StorageSize - (BitOffset + BitSize);
        }
    }

    //
    // Look for the data member location. This is not necessarily set for
    // unions.
    //

    LocationAttribute = DwarfpGetAttribute(Die, DwarfAtDataMemberLocation);
    if (LocationAttribute != NULL) {
        memset(&LocationContext, 0, sizeof(DWARF_LOCATION_CONTEXT));
        LocationContext.Unit = LoadingContext->CurrentUnit;
        LocationContext.StackSize = 1;
        Status = DwarfpGetLocation(Context,
                                   &LocationContext,
                                   LocationAttribute);

        if (Status != 0) {
            DwarfpDestroyLocationContext(Context, &LocationContext);
            DWARF_ERROR("DWARF: Failed to evaluate member location.\n");
            return Status;
        }

        assert((LocationContext.Location.BitSize == 0) &&
               (LocationContext.Location.NextPiece == NULL));

        if ((LocationContext.Location.Form == DwarfLocationKnownValue) ||
            (LocationContext.Location.Form == DwarfLocationMemory)) {

            BitOffset += LocationContext.Location.Value.Value * BITS_PER_BYTE;

        } else {
            DwarfpDestroyLocationContext(Context, &LocationContext);
            DWARF_ERROR("DWARF: Unsupported member location %d.\n",
                        LocationContext.Location.Form);

            return EINVAL;
        }

        DwarfpDestroyLocationContext(Context, &LocationContext);
    }

    //
    // Get the type of the member.
    //

    Member = malloc(sizeof(STRUCTURE_MEMBER));
    if (Member == NULL) {
        return ENOMEM;
    }

    memset(Member, 0, sizeof(STRUCTURE_MEMBER));
    Member->Name = DwarfpGetStringAttribute(Die, DwarfAtName);
    Member->BitOffset = BitOffset;
    Member->BitSize = BitSize;
    Result = DwarfpGetTypeReferenceAttribute(Context,
                                             Die,
                                             DwarfAtType,
                                             &(Member->TypeFile),
                                             &(Member->TypeNumber));

    if (Result == FALSE) {
        free(Member);
        DWARF_ERROR("DWARF: Unable to resolve type for member.\n");
        return EINVAL;
    }

    //
    // Add the member to the list.
    //

    Structure = LoadingContext->CurrentType;

    assert((Structure != NULL) && (Structure->Type == DataTypeStructure));

    PreviousMember = Structure->U.Structure.FirstMember;
    if (PreviousMember == NULL) {
        Structure->U.Structure.FirstMember = Member;

    } else {
        while (PreviousMember->NextMember != NULL) {
            PreviousMember = PreviousMember->NextMember;
        }

        PreviousMember->NextMember = Member;
    }

    Structure->U.Structure.MemberCount += 1;
    return 0;
}

INT
DwarfpProcessEnumerator (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes an enumerator value.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PENUMERATION_MEMBER Enumeration;
    PTYPE_SYMBOL EnumeratorType;
    PDWARF_LOADING_CONTEXT LoadingContext;
    PENUMERATION_MEMBER Previous;
    BOOL Result;
    ULONGLONG Value;

    LoadingContext = Context->LoadingContext;
    Result = DwarfpGetIntegerAttribute(Die, DwarfAtConstValue, &Value);
    if (Result == FALSE) {
        DWARF_ERROR("DWARF: Enumerator with no value.\n");
        return EINVAL;
    }

    //
    // Get the type of the member.
    //

    Enumeration = malloc(sizeof(ENUMERATION_MEMBER));
    if (Enumeration == NULL) {
        return ENOMEM;
    }

    memset(Enumeration, 0, sizeof(ENUMERATION_MEMBER));
    Enumeration->Name = DwarfpGetStringAttribute(Die, DwarfAtName);
    Enumeration->Value = Value;

    //
    // Add the member to the list.
    //

    EnumeratorType = LoadingContext->CurrentType;

    assert((EnumeratorType != NULL) &&
           (EnumeratorType->Type == DataTypeEnumeration));

    Previous = EnumeratorType->U.Enumeration.FirstMember;
    if (Previous == NULL) {
        EnumeratorType->U.Enumeration.FirstMember = Enumeration;

    } else {
        while (Previous->NextMember != NULL) {
            Previous = Previous->NextMember;
        }

        Previous->NextMember = Enumeration;
    }

    EnumeratorType->U.Enumeration.MemberCount += 1;
    return 0;
}

INT
DwarfpProcessSubroutineType (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a subroutine type (function pointer).

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDWARF_LOADING_CONTEXT LoadingContext;
    PTYPE_SYMBOL PreviousType;
    PTYPE_SYMBOL Type;

    LoadingContext = Context->LoadingContext;

    assert(Die->Tag == DwarfTagSubroutineType);

    Type = malloc(sizeof(TYPE_SYMBOL));
    if (Type == NULL) {
        return ENOMEM;
    }

    memset(Type, 0, sizeof(TYPE_SYMBOL));
    Type->ParentSource = LoadingContext->CurrentFile;
    PreviousType = LoadingContext->CurrentType;
    LoadingContext->CurrentType = Type;
    Type->ParentFunction = LoadingContext->CurrentFunction;
    Type->Name = DwarfpGetStringAttribute(Die, DwarfAtName);
    Type->TypeNumber = DWARF_DIE_ID(Context, Die);
    Type->Type = DataTypeFunctionPointer;
    Type->U.FunctionPointer.SizeInBytes =
                                      LoadingContext->CurrentUnit->AddressSize;

    INSERT_BEFORE(&(Type->ListEntry),
                  &(LoadingContext->CurrentFile->TypesHead));

    //
    // Process the child DIEs here to support getting the actual signature of
    // the function pointer.
    //

    assert(LoadingContext->CurrentType == Type);

    LoadingContext->CurrentType = PreviousType;
    return 0;
}

INT
DwarfpProcessSubprogram (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a subprogram (function) DIE.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONGLONG Declaration;
    PFUNCTION_SYMBOL Function;
    PDWARF_LOADING_CONTEXT LoadingContext;
    PFUNCTION_SYMBOL PreviousFunction;
    BOOL Result;
    INT Status;

    LoadingContext = Context->LoadingContext;

    //
    // Ignore function declarations.
    //

    Declaration = 0;
    DwarfpGetIntegerAttribute(Die, DwarfAtDeclaration, &Declaration);
    if (Declaration != FALSE) {
        return 0;
    }

    //
    // Also ignore inlined functions. It seems that even a value of 0
    // (indicating not inlined) results in no low-pc value.
    //

    if (DwarfpGetIntegerAttribute(Die, DwarfAtInline, &Declaration) != FALSE) {
        return 0;
    }

    Function = malloc(sizeof(FUNCTION_SYMBOL));
    if (Function == NULL) {
        return ENOMEM;
    }

    memset(Function, 0, sizeof(FUNCTION_SYMBOL));
    INITIALIZE_LIST_HEAD(&(Function->ParametersHead));
    INITIALIZE_LIST_HEAD(&(Function->LocalsHead));
    Function->ParentSource = LoadingContext->CurrentFile;
    Result = DwarfpGetTypeReferenceAttribute(Context,
                                             Die,
                                             DwarfAtType,
                                             &(Function->ReturnTypeOwner),
                                             &(Function->ReturnTypeNumber));

    if (Result == FALSE) {
        free(Function);
        DWARF_ERROR("DWARF: Failed to get return type.\n");
        return EINVAL;
    }

    PreviousFunction = LoadingContext->CurrentFunction;
    LoadingContext->CurrentFunction = Function;
    Function->Name = DwarfpGetStringAttribute(Die, DwarfAtName);
    Result = DwarfpGetAddressAttribute(Die,
                                       DwarfAtLowPc,
                                       &(Function->StartAddress));

    if (Result == FALSE) {
        DWARF_ERROR("DWARF: Warning: Failed to get low pc for function %s.\n",
                    Function->Name);
    }

    DwarfpGetAddressAttribute(Die,
                              DwarfAtHighPc,
                              &(Function->EndAddress));

    if ((Function->EndAddress < Function->StartAddress) &&
        (Function->StartAddress != 0)) {

        Function->EndAddress = Function->StartAddress + 1;
    }

    INSERT_BEFORE(&(Function->ListEntry),
                  &(LoadingContext->CurrentFile->FunctionsHead));

    Status = DwarfpProcessChildDies(Context, Die);

    assert(LoadingContext->CurrentFunction == Function);

    LoadingContext->CurrentFunction = PreviousFunction;
    return Status;
}

INT
DwarfpProcessVariable (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a variable or formal parameter DIE.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN AllocationSize;
    PDWARF_COMPLEX_DATA_SYMBOL DwarfSymbol;
    PDWARF_LOADING_CONTEXT LoadingContext;
    PDWARF_ATTRIBUTE_VALUE Location;
    BOOL Result;
    PDWARF_COMPILATION_UNIT Unit;
    PDATA_SYMBOL Variable;

    LoadingContext = Context->LoadingContext;
    Unit = LoadingContext->CurrentUnit;
    Location = DwarfpGetAttribute(Die, DwarfAtLocation);

    //
    // Ignore variables with no location (optimized away probably).
    //

    if (Location == NULL) {
        return 0;
    }

    if ((Location->Form != DwarfFormExprLoc) &&
        (!DWARF_BLOCK_FORM(Location->Form)) &&
        (!DWARF_SECTION_OFFSET_FORM(Location->Form, Unit))) {

        DWARF_ERROR("DWARF: Variable with bad location form %d.\n",
                    Location->Form);

        return EINVAL;
    }

    AllocationSize = sizeof(DATA_SYMBOL) + sizeof(DWARF_COMPLEX_DATA_SYMBOL);
    Variable = malloc(AllocationSize);
    if (Variable == NULL) {
        return ENOMEM;
    }

    memset(Variable, 0, AllocationSize);
    Variable->ParentSource = LoadingContext->CurrentFile;
    Variable->ParentFunction = LoadingContext->CurrentFunction;
    Result = DwarfpGetTypeReferenceAttribute(Context,
                                             Die,
                                             DwarfAtType,
                                             &(Variable->TypeOwner),
                                             &(Variable->TypeNumber));

    if (Result == FALSE) {
        DWARF_ERROR("DWARF: Failed to get variable type.\n");
        free(Variable);
        return EINVAL;
    }

    Variable->Name = DwarfpGetStringAttribute(Die, DwarfAtName);
    DwarfSymbol = (PDWARF_COMPLEX_DATA_SYMBOL)(Variable + 1);
    Variable->LocationType = DataLocationComplex;
    Variable->Location.Complex = DwarfSymbol;
    DwarfSymbol->Unit = LoadingContext->CurrentUnit;
    memcpy(&(DwarfSymbol->LocationAttribute),
           Location,
           sizeof(DWARF_ATTRIBUTE_VALUE));

    assert(LIST_EMPTY(&(Die->ChildList)));

    if (Die->Tag == DwarfTagFormalParameter) {

        assert(LoadingContext->CurrentFunction != NULL);

        INSERT_BEFORE(&(Variable->ListEntry),
                      &(LoadingContext->CurrentFunction->ParametersHead));

    } else {
        if (LoadingContext->CurrentFunction != NULL) {
            INSERT_BEFORE(&(Variable->ListEntry),
                          &(LoadingContext->CurrentFunction->LocalsHead));

        } else {
            INSERT_BEFORE(&(Variable->ListEntry),
                          &(LoadingContext->CurrentFile->DataSymbolsHead));
        }
    }

    return 0;
}

INT
DwarfpProcessGenericBlock (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine processes a generic block, including a lexical block or a
    namespace. It simply recurses into its children.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return DwarfpProcessChildDies(Context, Die);
}

