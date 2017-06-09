/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stabs.c

Abstract:

    This module implements routines necessary for reading and translating the
    STABS debugging symbol information.

Author:

    Evan Green 26-Jun-2012

Environment:

    Debug client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "symbols.h"
#include "stabs.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

//
// --------------------------------------------------------------------- Macros
//

#ifdef DEBUG_STABS

#define STABS_DEBUG(...) DbgOut(__VA_ARGS__)

#else

#define STABS_DEBUG(...)

#endif

//
// ---------------------------------------------------------------- Definitions
//

#define BUILTIN_TYPE_BOOL (-16)
#define BUILTIN_TYPE_BOOL_STRING "@s1;r-16;0;1;"

#define STABS_POINTER_SIZE 4

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
DbgpStabsUnloadSymbols (
    PDEBUG_SYMBOLS Symbols
    );

BOOL
DbgpLoadRawStabs (
    PSTR Filename,
    PDEBUG_SYMBOLS Symbols
    );

BOOL
DbgpPopulateStabs (
    PDEBUG_SYMBOLS Symbols
    );

BOOL
DbgpParseLocalSymbolStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    );

PSTR
DbgpCreateType (
    PDEBUG_SYMBOLS Symbols,
    PSTR TypeName,
    PSOURCE_FILE_SYMBOL TypeOwner,
    LONG TypeNumber,
    PSTR String
    );

PSTR
DbgpParseEnumerationMember (
    PSTR String,
    PENUMERATION_MEMBER Member
    );

PSTR
DbgpParseStructureMember (
    PDEBUG_SYMBOLS Symbols,
    PSTR String,
    PSTRUCTURE_MEMBER Member
    );

PSTR
DbgpParseRange (
    PSTR String,
    PDATA_RANGE Range
    );

BOOL
DbgpRangeToNumericType (
    PDATA_RANGE Range,
    PDATA_TYPE_NUMERIC Numeric
    );

PSTR
DbgpGetTypeNumber (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PSTR String,
    PSOURCE_FILE_SYMBOL *OwningFile,
    PLONG TypeNumber
    );

BOOL
DbgpParseSourceFileStab (
    PDEBUG_SYMBOLS Symbols,
    PRAW_STAB Stab,
    PSTR StabString,
    BOOL Include
    );

BOOL
DbgpParseSourceLineStab (
    PDEBUG_SYMBOLS Symbols,
    PRAW_STAB Stab,
    PSTR StabString
    );

BOOL
DbgpParseFunctionStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    );

BOOL
DbgpParseFunctionParameterStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    );

BOOL
DbgpParseRegisterVariableStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    );

BOOL
DbgpParseBraceStab (
    PDEBUG_SYMBOLS Symbols,
    PRAW_STAB Stab,
    PSTR StabString
    );

BOOL
DbgpParseStaticSymbolStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    );

BOOL
DbgpResolveCrossReferences (
    PSTAB_CONTEXT State
    );

LONG
DbgpGetFileSize (
    FILE *File
    );

ULONG
DbgpStabsGetFramePointerRegister (
    PDEBUG_SYMBOLS Symbols
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

DEBUG_SYMBOL_INTERFACE DbgStabsSymbolInterface = {
    DbgpStabsLoadSymbols,
    DbgpStabsUnloadSymbols,
    NULL,
    NULL,
    NULL,
    NULL
};

//
// Basic memory leak detection code. Disabled by default.
//

#if 0

typedef struct _MEM_ALLOCATION {
    BOOL Valid;
    PVOID Allocation;
    PSTR File;
    ULONG Line;
    ULONG Size;
} MEM_ALLOCATION, *PMEM_ALLOCATION;

#define MAX_MEMORY_ALLOCATIONS 100000

BOOL LeakStructureInitialized = FALSE;
MEM_ALLOCATION LeakStructure[MAX_MEMORY_ALLOCATIONS];

PVOID
MyMalloc (
    ULONG Size,
    PSTR File,
    ULONG Line
    )

{

    PVOID Allocation;
    ULONG Index;

    Allocation = malloc(Size);
    if (Allocation == NULL) {
        goto MyMallocEnd;
    }

    if (LeakStructureInitialized == FALSE) {
        for (Index = 0; Index < MAX_MEMORY_ALLOCATIONS; Index += 1) {
            LeakStructure[Index].Valid = FALSE;
        }

        LeakStructureInitialized = TRUE;
    }

    for (Index = 0; Index < MAX_MEMORY_ALLOCATIONS; Index += 1) {
        if (LeakStructure[Index].Valid == FALSE) {
            LeakStructure[Index].Valid = TRUE;
            LeakStructure[Index].Allocation = Allocation;
            LeakStructure[Index].File = File;
            LeakStructure[Index].Line = Line;
            LeakStructure[Index].Size = Size;
            break;
        }
    }

    assert(Index != MAX_MEMORY_ALLOCATIONS);

MyMallocEnd:
    return Allocation;
}

VOID
MyFree (
    PVOID Allocation
    )

{

    ULONG Index;

    assert(LeakStructureInitialized != FALSE);

    for (Index = 0; Index < MAX_MEMORY_ALLOCATIONS; Index += 1) {
        if ((LeakStructure[Index].Valid != FALSE) &&
            (LeakStructure[Index].Allocation == Allocation)) {

            LeakStructure[Index].Valid = FALSE;
            LeakStructure[Index].Allocation = NULL;
            break;
        }
    }

    assert(Index != MAX_MEMORY_ALLOCATIONS);
}

VOID
PrintMemoryLeaks (
    )

{

    ULONG Index;

    for (Index = 0; Index < MAX_MEMORY_ALLOCATIONS; Index += 1) {
        if (LeakStructure[Index].Valid != FALSE) {
            DbgOut("Leak: %08x Size %d: %s, Line %d\n",
                   LeakStructure[Index].Allocation,
                   LeakStructure[Index].Size,
                   LeakStructure[Index].File,
                   LeakStructure[Index].Line);
        }
    }
}

#define MALLOC(_Size) MyMalloc(_Size, __FILE__, __LINE__)
#define FREE(_Allocation) MyFree(_Allocation)

#else

#define MALLOC(_x) malloc(_x)
#define FREE(_x) free(_x)

#endif

//
// ------------------------------------------------------------------ Functions
//

INT
DbgpStabsLoadSymbols (
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
    BOOL Result;
    PSTAB_CONTEXT StabState;
    PDEBUG_SYMBOLS StabSymbols;
    INT Status;

    AllocationSize = sizeof(DEBUG_SYMBOLS) + sizeof(STAB_CONTEXT);
    StabSymbols = MALLOC(AllocationSize);
    if (StabSymbols == NULL) {
        Status = ENOMEM;
        goto LoadSymbolsEnd;
    }

    //
    // Load the raw stab data from the file into memory.
    //

    memset(StabSymbols, 0, AllocationSize);
    StabSymbols->Interface = &DbgStabsSymbolInterface;
    StabSymbols->SymbolContext = StabSymbols + 1;
    StabSymbols->HostContext = HostContext;
    StabState = StabSymbols->SymbolContext;
    INITIALIZE_LIST_HEAD(&(StabState->CrossReferenceListHead));
    StabState->CurrentModule = StabSymbols;
    Result = DbgpLoadRawStabs(Filename, StabSymbols);
    if (Result == FALSE) {
        Status = EINVAL;
        goto LoadSymbolsEnd;
    }

    //
    // Verify the machine type, if supplied.
    //

    if ((MachineType != ImageMachineTypeUnknown) &&
        (MachineType != StabSymbols->Machine)) {

        DbgOut("Image machine type %d mismatches expected %d.\n",
               MachineType,
               StabSymbols->Machine);

        Status = EINVAL;
        goto LoadSymbolsEnd;
    }

    //
    // Parse through the stabs and initialize internal data structures.
    //

    Result = DbgpPopulateStabs(StabSymbols);
    if (Result == FALSE) {
        Status = EINVAL;
        DbgOut("Failure populating stabs.\n");
        goto LoadSymbolsEnd;
    }

    //
    // Attempt to load COFF symbols for PE images, or ELF symbols for ELF
    // images.
    //

    if (StabSymbols->ImageFormat == ImagePe32) {
        Result = DbgpLoadCoffSymbols(StabSymbols, Filename);
        if (Result == FALSE) {
            Status = EINVAL;
            goto LoadSymbolsEnd;
        }

    } else if (StabSymbols->ImageFormat == ImageElf32) {
        Result = DbgpLoadElfSymbols(StabSymbols, Filename);
        if (Result == FALSE) {
            Status = EINVAL;
            goto LoadSymbolsEnd;
        }
    }

    Status = 0;

LoadSymbolsEnd:
    if (Status != 0) {
        if (StabSymbols != NULL) {
            DbgpStabsUnloadSymbols(StabSymbols);
            StabSymbols = NULL;
        }
    }

    *Symbols = StabSymbols;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DbgpStabsUnloadSymbols (
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

    PENUMERATION_MEMBER CurrentEnumerationMember;
    PLIST_ENTRY CurrentFunctionEntry;
    PLIST_ENTRY CurrentGlobalEntry;
    PLIST_ENTRY CurrentLineEntry;
    PLIST_ENTRY CurrentLocalEntry;
    PLIST_ENTRY CurrentParameterEntry;
    PLIST_ENTRY CurrentSourceEntry;
    PSTRUCTURE_MEMBER CurrentStructureMember;
    PLIST_ENTRY CurrentTypeEntry;
    PDATA_TYPE_ENUMERATION Enumeration;
    PFUNCTION_SYMBOL Function;
    PDATA_SYMBOL GlobalVariable;
    PDATA_SYMBOL LocalVariable;
    PENUMERATION_MEMBER NextEnumerationMember;
    PLIST_ENTRY NextFunctionEntry;
    PLIST_ENTRY NextLineEntry;
    PLIST_ENTRY NextLocalEntry;
    PLIST_ENTRY NextParameterEntry;
    PSTRUCTURE_MEMBER NextStructureMember;
    PDATA_SYMBOL Parameter;
    PSOURCE_FILE_SYMBOL SourceFile;
    PSOURCE_LINE_SYMBOL SourceLine;
    PSTAB_CONTEXT StabState;
    PDATA_TYPE_STRUCTURE Structure;
    PTYPE_SYMBOL TypeSymbol;

    if (Symbols == NULL) {
        return;
    }

    StabState = Symbols->SymbolContext;

    assert(LIST_EMPTY(&(StabState->CrossReferenceListHead)));
    assert(StabState->IncludeStack == NULL);

    if (Symbols->Filename != NULL) {
        FREE(Symbols->Filename);
    }

    if (StabState->RawStabs != NULL) {
        FREE(StabState->RawStabs);
    }

    if (StabState->RawStabStrings != NULL) {
        FREE(StabState->RawStabStrings);
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

        //
        // Free types.
        //

        CurrentTypeEntry = SourceFile->TypesHead.Next;
        while ((CurrentTypeEntry != &(SourceFile->TypesHead)) &&
               (CurrentTypeEntry != NULL)) {

            TypeSymbol = LIST_VALUE(CurrentTypeEntry,
                                    TYPE_SYMBOL,
                                    ListEntry);

            CurrentTypeEntry = CurrentTypeEntry->Next;

            //
            // If the type is a structure, free all structure members.
            //

            if (TypeSymbol->Type == DataTypeStructure) {
                Structure = &(TypeSymbol->U.Structure);
                CurrentStructureMember = Structure->FirstMember;
                while (CurrentStructureMember != NULL) {
                    NextStructureMember = CurrentStructureMember->NextMember;
                    if (CurrentStructureMember->Name != NULL) {
                        FREE(CurrentStructureMember->Name);
                    }

                    FREE(CurrentStructureMember);
                    CurrentStructureMember = NextStructureMember;
                }
            }

            //
            // If the type is an enumeration, free all enumeration members.
            //

            if (TypeSymbol->Type == DataTypeEnumeration) {
                Enumeration = &(TypeSymbol->U.Enumeration);
                CurrentEnumerationMember = Enumeration->FirstMember;
                while (CurrentEnumerationMember != NULL) {
                    NextEnumerationMember =
                                        CurrentEnumerationMember->NextMember;

                    if (CurrentEnumerationMember->Name != NULL) {
                        FREE(CurrentEnumerationMember->Name);
                    }

                    FREE(CurrentEnumerationMember);
                    CurrentEnumerationMember = NextEnumerationMember;
                }
            }

            if (TypeSymbol->Name != NULL) {
                FREE(TypeSymbol->Name);
            }

            FREE(TypeSymbol);
        }

        //
        // Free functions.
        //

        CurrentFunctionEntry = SourceFile->FunctionsHead.Next;
        while (CurrentFunctionEntry != &(SourceFile->FunctionsHead)) {
            Function = LIST_VALUE(CurrentFunctionEntry,
                                  FUNCTION_SYMBOL,
                                  ListEntry);

            //
            // Free function parameters.
            //

            CurrentParameterEntry = Function->ParametersHead.Next;
            while (CurrentParameterEntry != &(Function->ParametersHead)) {
                Parameter = LIST_VALUE(CurrentParameterEntry,
                                       DATA_SYMBOL,
                                       ListEntry);

                if (Parameter->Name != NULL) {
                    FREE(Parameter->Name);
                }

                NextParameterEntry = CurrentParameterEntry->Next;
                FREE(Parameter);
                CurrentParameterEntry = NextParameterEntry;
            }

            if (Function->Name != NULL) {
                FREE(Function->Name);
            }

            //
            // Free function local variables.
            //

            CurrentLocalEntry = Function->LocalsHead.Next;
            while (CurrentLocalEntry != &(Function->LocalsHead)) {
                LocalVariable = LIST_VALUE(CurrentLocalEntry,
                                           DATA_SYMBOL,
                                           ListEntry);

                if (LocalVariable->Name != NULL) {
                    FREE(LocalVariable->Name);
                }

                NextLocalEntry = CurrentLocalEntry->Next;

                assert(NextLocalEntry != NULL);

                FREE(LocalVariable);
                CurrentLocalEntry = NextLocalEntry;
            }

            assert(LIST_EMPTY(&(Function->FunctionsHead)));

            NextFunctionEntry = CurrentFunctionEntry->Next;
            FREE(Function);
            CurrentFunctionEntry = NextFunctionEntry;
        }

        //
        // Free source lines.
        //

        CurrentLineEntry = SourceFile->SourceLinesHead.Next;
        while (CurrentLineEntry != &(SourceFile->SourceLinesHead)) {
            SourceLine = LIST_VALUE(CurrentLineEntry,
                                    SOURCE_LINE_SYMBOL,
                                    ListEntry);

            NextLineEntry = CurrentLineEntry->Next;
            FREE(SourceLine);
            CurrentLineEntry = NextLineEntry;
        }

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
DbgpLoadRawStabs (
    PSTR Filename,
    PDEBUG_SYMBOLS Symbols
    )

/*++

Routine Description:

    This routine loads the raw ".stab" and ".stabstr" sections into memory. The
    caller must remember to free any memory allocated here.

Arguments:

    Filename - Supplies the name of the binary to load STABS sections from.

    Symbols - Supplies a pointer to the structure where the buffers and sizes
        should be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    LONG BytesRead;
    FILE *File;
    PVOID FileBuffer;
    LONG FileSize;
    IMAGE_BUFFER ImageBuffer;
    IMAGE_INFORMATION Information;
    BOOL Result;
    ULONG SectionSize;
    PVOID SectionSource;
    PSTAB_CONTEXT StabState;
    KSTATUS Status;

    FileBuffer = NULL;
    StabState = Symbols->SymbolContext;
    memset(&ImageBuffer, 0, sizeof(IMAGE_BUFFER));
    SectionSource = NULL;
    Symbols->Filename = NULL;
    StabState->RawStabs = NULL;
    StabState->RawStabStrings = NULL;

    //
    // Determine the file size and load the file into memory.
    //

    File = fopen(Filename, "rb");
    if (File == NULL) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    FileSize = DbgpGetFileSize(File);
    if (FileSize <= 0) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    FileBuffer = MALLOC(FileSize);
    if (FileBuffer == NULL) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    BytesRead = fread(FileBuffer, 1, FileSize, File);
    if (BytesRead != FileSize) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    //
    // Save the filename into the debug symbols.
    //

    Symbols->Filename = MALLOC(strlen(Filename) + 1);
    if (Symbols->Filename == NULL) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    strcpy(Symbols->Filename, Filename);
    ImageBuffer.Data = FileBuffer;
    ImageBuffer.Size = FileSize;

    //
    // Get and save the relevant image information.
    //

    Status = ImGetImageInformation(&ImageBuffer, &Information);
    if (!KSUCCESS(Status)) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    Symbols->ImageFormat = Information.Format;
    Symbols->Machine = Information.Machine;
    Symbols->ImageBase = Information.ImageBase;

    //
    // Attempt to get the stabs section. If successful, allocate a new buffer
    // and copy it over.
    //

    Result = ImGetImageSection(&ImageBuffer,
                               ".stab",
                               &SectionSource,
                               NULL,
                               &SectionSize,
                               NULL);

    if ((Result == FALSE) || (SectionSize == 0) || (SectionSource == NULL)) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    StabState->RawStabs = MALLOC(SectionSize);
    if (StabState->RawStabs == NULL) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    memcpy(StabState->RawStabs, SectionSource, SectionSize);
    StabState->RawStabsSize = SectionSize;

    //
    // Attempt to get the stab strings section.
    //

    Result = ImGetImageSection(&ImageBuffer,
                               ".stabstr",
                               &SectionSource,
                               NULL,
                               &SectionSize,
                               NULL);

    if ((Result == FALSE) || (SectionSize == 0) || (SectionSource == NULL)) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    StabState->RawStabStrings = MALLOC(SectionSize);
    if (StabState->RawStabStrings == NULL) {
        Result = FALSE;
        goto LoadRawStabsEnd;
    }

    memcpy(StabState->RawStabStrings, SectionSource, SectionSize);
    StabState->RawStabStringsSize = SectionSize;
    Result = TRUE;

LoadRawStabsEnd:
    if (Result == FALSE) {
        if (StabState->RawStabs != NULL) {
            FREE(StabState->RawStabs);
            StabState->RawStabs = NULL;
            StabState->RawStabsSize = 0;
        }

        if (StabState->RawStabStrings != NULL) {
            FREE(StabState->RawStabStrings);
            StabState->RawStabStrings = NULL;
            StabState->RawStabStringsSize = 0;
        }

        if (Symbols->Filename != NULL) {
            FREE(Symbols->Filename);
            Symbols->Filename = NULL;
        }
    }

    if (FileBuffer != NULL) {
        FREE(FileBuffer);
    }

    if (File != NULL) {
        fclose(File);
    }

    return Result;
}

BOOL
DbgpPopulateStabs (
    PDEBUG_SYMBOLS Symbols
    )

/*++

Routine Description:

    This routine parses through stab data, setting up various data structures
    to represent the stabs that the rest of the debugging system can understand.

Arguments:

    Symbols - Supplies a pointer to the symbols data. The raw stab data should
        already be loaded. Also returns the initialized data.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG ByteCount;
    PCROSS_REFERENCE_ENTRY CrossReference;
    PLIST_ENTRY CurrentCrossReferenceEntry;
    BOOL Include;
    ULONG Index;
    BOOL IsSourceFileStab;
    PSTR Name;
    PSTR NameEnd;
    PRAW_STAB RawStab;
    BOOL Result;
    PSTAB_CONTEXT StabState;
    PSTR StabString;

    Name = NULL;
    StabState = Symbols->SymbolContext;

    //
    // Validate parameters.
    //

    if ((Symbols == NULL) || (StabState->RawStabs == NULL) ||
        (StabState->RawStabsSize == 0) || (StabState->RawStabStrings == NULL) ||
        (StabState->RawStabStringsSize == 0)) {

        Result = FALSE;
        goto PopulateStabsEnd;
    }

    //
    // Initialize module structures if not done yet.
    //

    if ((Symbols->SourcesHead.Next == NULL) ||
        (Symbols->SourcesHead.Previous == NULL)) {

        INITIALIZE_LIST_HEAD(&(Symbols->SourcesHead));
    }

    //
    // Loop over stabs.
    //

    ByteCount = sizeof(RAW_STAB);
    Index = 0;
    RawStab = StabState->RawStabs;
    while (ByteCount <= StabState->RawStabsSize) {
        Name = NULL;
        STABS_DEBUG("%d: Index: 0x%x, Type: %d, Other: %d, Desc: %d, "
                    "Value: 0x%x\n",
                    Index,
                    RawStab->StringIndex,
                    RawStab->Type,
                    RawStab->Other,
                    RawStab->Description,
                    RawStab->Value);

        if ((RawStab->StringIndex > 0) &&
            (RawStab->StringIndex < StabState->RawStabStringsSize)) {

            StabString = StabState->RawStabStrings + RawStab->StringIndex;
            STABS_DEBUG("String: %s\n",
                        StabState->RawStabStrings + RawStab->StringIndex);

            //
            // If the stab has a string, it probably starts with a name. Get
            // that name here to avoid duplicating that code in each function.
            // A source file may have a colon in the drive letter that is not
            // the name delimiter, so avoid parsing those.
            //

            IsSourceFileStab = (RawStab->Type == STAB_SOURCE_FILE) ||
                               (RawStab->Type == STAB_INCLUDE_BEGIN) ||
                               (RawStab->Type == STAB_INCLUDE_PLACEHOLDER) ||
                               (RawStab->Type == STAB_INCLUDE_NAME);

            if (IsSourceFileStab == FALSE) {

                //
                // Get the first single (but not double) colon.
                //

                NameEnd = StabString;
                while (TRUE) {
                    NameEnd = strchr(NameEnd, ':');
                    if ((NameEnd == NULL) || (*(NameEnd + 1) != ':')) {
                        break;
                    }

                    NameEnd += 2;
                }

                if (NameEnd != NULL) {
                    Name = MALLOC(NameEnd - StabString + 1);
                    if (Name == NULL) {
                        Result = FALSE;
                        goto PopulateStabsEnd;
                    }

                    strncpy(Name, StabString, NameEnd - StabString);
                    Name[NameEnd - StabString] = '\0';
                    StabString = NameEnd + 1;
                }
            }

        } else {
            StabString = NULL;
        }

        Include = FALSE;
        switch (RawStab->Type) {
        case STAB_FUNCTION:
            Result = DbgpParseFunctionStab(Symbols, Name, RawStab, StabString);
            break;

        case STAB_FUNCTION_PARAMETER:
            Result = DbgpParseFunctionParameterStab(Symbols,
                                                    Name,
                                                    RawStab,
                                                    StabString);

            break;

        case STAB_REGISTER_VARIABLE:
            Result = DbgpParseRegisterVariableStab(Symbols,
                                                   Name,
                                                   RawStab,
                                                   StabString);

            break;

        case STAB_LOCAL_SYMBOL:
            Result = DbgpParseLocalSymbolStab(Symbols,
                                              Name,
                                              RawStab,
                                              StabString);

            break;

        case STAB_BSS_SYMBOL:
        case STAB_GLOBAL_SYMBOL:
        case STAB_STATIC:
            Result = DbgpParseStaticSymbolStab(Symbols,
                                               Name,
                                               RawStab,
                                               StabString);

            break;

        case STAB_INCLUDE_BEGIN:
        case STAB_INCLUDE_PLACEHOLDER:

            //
            // Set include to true and fall through to source file processing.
            //

            Include = TRUE;

        case STAB_INCLUDE_NAME:
        case STAB_SOURCE_FILE:

            //
            // A source file has no colon following the name, so the name
            // parsing code above will not have found anything.
            //

            assert(Name == NULL);

            Result = DbgpParseSourceFileStab(Symbols,
                                             RawStab,
                                             StabString,
                                             Include);

            break;

        case STAB_SOURCE_LINE:

            assert(Name == NULL);

            Result = DbgpParseSourceLineStab(Symbols, RawStab, StabString);
            break;

        case STAB_LEFT_BRACE:
        case STAB_RIGHT_BRACE:

            assert(Name == NULL);

            Result = DbgpParseBraceStab(Symbols, RawStab, StabString);
            break;

        default:
            if (Name != NULL) {
                FREE(Name);
            }

            Result = TRUE;
            break;
        }

        if (Result == FALSE) {
            printf("Failed to load STAB: ");
            printf("%d: Index: 0x%x, Type: %x, Other: %d, Desc: %d, "
                   "Value: 0x%x\n - %s\n",
                   Index,
                   RawStab->StringIndex,
                   RawStab->Type,
                   RawStab->Other,
                   RawStab->Description,
                   RawStab->Value,
                   StabString);
        }

        RawStab += 1;
        Index += 1;
        ByteCount += sizeof(RAW_STAB);
    }

    //
    // Send down a closing source file stab in case the last file was an
    // assembly file (they don't always close themselves).
    //

    Result = DbgpParseSourceFileStab(Symbols, NULL, NULL, FALSE);
    if (Result == FALSE) {
        goto PopulateStabsEnd;
    }

    Result = TRUE;

PopulateStabsEnd:

    //
    // Free any remaining cross references.
    //

    CurrentCrossReferenceEntry = StabState->CrossReferenceListHead.Next;
    while (CurrentCrossReferenceEntry != &(StabState->CrossReferenceListHead)) {
        CrossReference = LIST_VALUE(CurrentCrossReferenceEntry,
                                    CROSS_REFERENCE_ENTRY,
                                    ListEntry);

        CurrentCrossReferenceEntry = CurrentCrossReferenceEntry->Next;
        LIST_REMOVE(&(CrossReference->ListEntry));
        FREE(CrossReference);
    }

    if (Result == FALSE) {
        if (Name != NULL) {
            FREE(Name);
        }
    }

    return Result;
}

BOOL
DbgpParseLocalSymbolStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    )

/*++

Routine Description:

    This routine parses through a local symbol stab, updating the output symbol
    information as well as the parse state.

Arguments:

    Symbols - Supplies a pointer to the symbols data. The raw stab data should
        already be loaded. Also returns the initialized data.

    Name - Supplies the name of the local symbol, or NULL if a name could not
        be parsed.

    Stab - Supplies a pointer to the stab of type STAB_SOURCE_FILE.

    StabString - Supplies a pointer to the stab's string, or NULL if the stab
        has no string.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR Contents;
    PDATA_SYMBOL NewLocal;
    BOOL Result;
    PSTAB_CONTEXT State;
    LONG TypeNumber;
    PSOURCE_FILE_SYMBOL TypeOwner;

    NewLocal = NULL;

    //
    // Parameter checking.
    //

    if ((StabString == NULL) || (Symbols == NULL) || (Stab == NULL)) {
        Result = FALSE;
        goto ParseLocalSymbolStabEnd;
    }

    Contents = StabString;
    if ((Contents == NULL) || (*Contents == '\0')) {
        Result = FALSE;
        goto ParseLocalSymbolStabEnd;
    }

    //
    // A 't' or 'T' next means this symbol is a type. ('T' specifies a typedef,
    // struct or union).
    //

    if ((*Contents == 't') || (*Contents == 'T')) {
        Contents += 1;

        //
        // Sometimes Tt seems to get emitted, which seems to be no different
        // really.
        //

        if (*Contents == 't') {
            Contents += 1;
        }

        Contents = DbgpGetTypeNumber(Symbols,
                                     Name,
                                     Contents,
                                     &TypeOwner,
                                     &TypeNumber);

        assert(Contents != NULL);

        if (Contents == NULL) {
            Result = FALSE;
            goto ParseLocalSymbolStabEnd;
        }

    //
    // A digit, -, or ( indicates that a type immediately follows the name. The
    // default behavior is to treat this as a local variable.
    //

    } else if ((*Contents == '-') || (*Contents == '(') ||
               ((*Contents >= '0') && (*Contents <= '9'))) {

        State = Symbols->SymbolContext;

        //
        // If there is no current source file or function, then it makes very
        // little sense to have a local variable. Fail here if that's the case.
        //

        if ((State->CurrentSourceFile == NULL) ||
            (State->CurrentFunction == NULL)) {

            Result = FALSE;
            goto ParseLocalSymbolStabEnd;
        }

        //
        // Allocate space for a new local variable symbol.
        //

        NewLocal = MALLOC(sizeof(DATA_SYMBOL));
        if (NewLocal == NULL) {
            Result = FALSE;
            goto ParseLocalSymbolStabEnd;
        }

        memset(NewLocal, 0, sizeof(DATA_SYMBOL));
        Contents = DbgpGetTypeNumber(Symbols,
                                     NULL,
                                     Contents,
                                     &(NewLocal->TypeOwner),
                                     &(NewLocal->TypeNumber));

        if (Contents == NULL) {
            Result = FALSE;
            goto ParseLocalSymbolStabEnd;
        }

        //
        // Initialize the new local and insert it into the current function's
        // locals list.
        //

        NewLocal->ParentSource = State->CurrentSourceFile;
        NewLocal->ParentFunction = State->CurrentFunction;
        NewLocal->Name = Name;
        NewLocal->LocationType = DataLocationIndirect;
        NewLocal->Location.Indirect.Offset = (LONG)Stab->Value;
        NewLocal->Location.Indirect.Register =
                                     DbgpStabsGetFramePointerRegister(Symbols);

        NewLocal->MinimumValidExecutionAddress = State->MaxBraceAddress;
        INSERT_BEFORE(&(NewLocal->ListEntry),
                      &(State->CurrentFunction->LocalsHead));
    }

    Result = TRUE;

ParseLocalSymbolStabEnd:
    if (Result == FALSE) {
        if (NewLocal != NULL) {
            FREE(NewLocal);
        }
    }

    return Result;
}

PSTR
DbgpCreateType (
    PDEBUG_SYMBOLS Symbols,
    PSTR TypeName,
    PSOURCE_FILE_SYMBOL TypeOwner,
    LONG TypeNumber,
    PSTR String
    )

/*++

Routine Description:

    This routine creates a new type based on the partial stab string and the
    stab parsing state.

Arguments:

    Symbols - Supplies a pointer to the current module.

    TypeName - Supplies the name of the type.

    TypeOwner - Supplies a pointer to the source file this type was defined in.

    TypeNumber - Supplies the type number for this type.

    String - Supplies the stab string starting at the type definition.

Return Value:

    Returns the string advanced beyond the type definition, or NULL on failure.

--*/

{

    PCROSS_REFERENCE_ENTRY CrossReference;
    PENUMERATION_MEMBER CurrentEnumerationMember;
    PSTRUCTURE_MEMBER CurrentStructureMember;
    PSTR EndString;
    DATA_TYPE_ENUMERATION Enumeration;
    PSTR EnumerationString;
    LONG FloatSize;
    LONG FloatType;
    PVOID LocalData;
    ULONG LocalDataSize;
    ENUMERATION_MEMBER LocalEnumerationMember;
    STRUCTURE_MEMBER LocalStructureMember;
    PENUMERATION_MEMBER NewEnumerationMember;
    PSTRUCTURE_MEMBER NewStructureMember;
    PTYPE_SYMBOL NewType;
    PENUMERATION_MEMBER NextEnumerationMember;
    PSTRUCTURE_MEMBER NextStructureMember;
    DATA_TYPE_NUMERIC Numeric;
    PSTAB_CONTEXT ParseState;
    DATA_RANGE Range;
    DATA_TYPE_RELATION Relation;
    BOOL Result;
    DATA_TYPE_STRUCTURE Structure;
    PSTR StructureString;
    DATA_TYPE_TYPE Type;
    LONG TypeSize;
    INT ValuesRead;

    CurrentEnumerationMember = NULL;
    CurrentStructureMember = NULL;
    LocalData = NULL;
    LocalDataSize = 0;
    NewType = NULL;
    Type = DataTypeInvalid;
    TypeSize = 0;
    memset(&Enumeration, 0, sizeof(DATA_TYPE_ENUMERATION));
    memset(&Structure, 0, sizeof(DATA_TYPE_STRUCTURE));

    //
    // An '@' means there are attributes that must be parsed.
    //

    if (*String == '@') {
        String += 1;
        switch (*String) {

        //
        // The 's' attribute specifies the type's size in bits.
        //

        case 's':
            String += 1;
            ValuesRead = sscanf(String, "%d", &TypeSize);
            if (ValuesRead != 1) {
                String = NULL;
                goto CreateTypeEnd;
            }

            break;

        default:
            break;
        }

        //
        // Advance the string past the attribute.
        //

        while ((*String != ';') && (*String != '\0')) {
            String += 1;
        }

        if ((*String == '\0') || (*(String + 1) == '\0')) {
            String = NULL;
            goto CreateTypeEnd;
        }

        String += 1;
    }

    //
    // A type descriptor that begins with 'k' indicates a constant value.
    // Constants are ignored and treated like everything else here. Skip the k.
    //

    if (*String == 'k') {
        String += 1;
    }

    //
    // A type descriptor that begins with 'B' indicates a volatile variable.
    // Volatiles are treated the same as any other variable, so swallow this
    // specifier.
    //

    if (*String == 'B') {
        String += 1;
    }

    //
    // A type descriptor that begins with 'x' indicates a cross reference to
    // a type that may or may not be created yet. Create a record of this cross
    // reference that will get resolved once all the symbols in the file have
    // been parsed.
    //

    if (*String == 'x') {
        String += 1;
        CrossReference = MALLOC(sizeof(CROSS_REFERENCE_ENTRY));
        if (CrossReference == NULL) {
            String = NULL;
            goto CreateTypeEnd;
        }

        CrossReference->ReferringTypeName = TypeName;
        CrossReference->ReferringTypeNumber = TypeNumber;
        CrossReference->ReferringTypeSource = TypeOwner;
        CrossReference->ReferenceString = String;
        ParseState = Symbols->SymbolContext;
        INSERT_BEFORE(&(CrossReference->ListEntry),
                      &(ParseState->CrossReferenceListHead));

        //
        // Find the end of the cross reference, marked by a single colon (but
        // not a double colon). Sometimes there's not a colon, the string just
        // ends.
        //

        EndString = String;
        while (TRUE) {
            EndString = strchr(EndString, ':');
            if ((EndString == NULL) || (*(EndString + 1) != ':')) {
                break;
            }

            EndString += 2;
        }

        //
        // If there was a colon, get past it. If there was no colon, then just
        // get the end of the string.
        //

        if (EndString != NULL) {
            EndString += 1;

        } else {
            EndString = strchr(String, '\0');
        }

        String = EndString;
        goto CreateTypeEnd;
    }

    //
    // A '*', digit, or '-' indicates that this type is either a pointer or
    // equivalent to another type. An 'a' indicates that this type is an array.
    // An 'f' indicates this variable is a function.
    // & is a C++ reference, # is a C++ method.
    //

    if ((*String == '*') ||
        (*String == '&') ||
        (*String == '-') ||
        (*String == '(') ||
        (*String == 'a') ||
        (*String == 'f') ||
        (*String == '#') ||
        ((*String >= '0') && (*String <= '9'))) {

        memset(&Relation, 0, sizeof(DATA_TYPE_RELATION));
        if ((*String == '*') || (*String == '&')) {
            Relation.Pointer = STABS_POINTER_SIZE;
            String += 1;
        }

        //
        // An array has an 'a', then a range specifying the index range.
        //

        if (*String == 'a') {

            //
            // Advance beyond the 'a', 'r', and get the index type. It's
            // assumed the index type is always an integer of some sort, but
            // additional type definitions could be hidden in here, so it's
            // necessary to kick off that chain if that's the case.
            //

            String += 1;
            if (*String != 'r') {
                String = NULL;
                goto CreateTypeEnd;
            }

            String += 1;
            String = DbgpGetTypeNumber(Symbols,
                                       NULL,
                                       String,
                                       NULL,
                                       NULL);

            if (String == NULL) {
                goto CreateTypeEnd;
            }

            //
            // Advance past the semicolon and parse the range
            //

            String += 1;
            String = DbgpParseRange(String, &(Relation.Array));
            if (String == NULL) {
                goto CreateTypeEnd;
            }

            //
            // An array of length zero is really just a pointer.
            //

            if ((Relation.Array.Minimum == 0) &&
                (Relation.Array.Maximum == -1)) {

                Relation.Pointer = STABS_POINTER_SIZE;
                Relation.Array.Maximum = 0;
            }
        }

        //
        // An 'f' indicates this type is a function, # is a C++ method.
        //

        if ((*String == 'f') || (*String == '#')) {
            Relation.Function = TRUE;
            String += 1;
        }

        //
        // Get the type that this type relates to.
        //

        String = DbgpGetTypeNumber(Symbols,
                                   NULL,
                                   String,
                                   &(Relation.OwningFile),
                                   &(Relation.TypeNumber));

        if (String == NULL) {
            goto CreateTypeEnd;
        }

        if ((Relation.Array.Maximum != 0) || (Relation.Array.Minimum != 0)) {
            STABS_DEBUG("New Relational Array Type: %s:(%s,%d). Pointer: %d, "
                        "Reference Type: (%s, %d)\n"
                        "\tArray Range = [%I64i, %I64i], MaxUlonglong? %d\n",
                        TypeName,
                        TypeOwner->SourceFile,
                        TypeNumber,
                        Relation.Pointer,
                        Relation.OwningFile->SourceFile,
                        Relation.TypeNumber,
                        Relation.Array.Minimum,
                        Relation.Array.Maximum,
                        Relation.Array.MaxUlonglong);

        } else {
            STABS_DEBUG("New Relational Type: %s:(%s,%d). Pointer: %d, "
                        "Reference Type: (%s, %d)\n",
                        TypeName,
                        TypeOwner->SourceFile,
                        TypeNumber,
                        Relation.Pointer,
                        Relation.OwningFile->SourceFile,
                        Relation.TypeNumber);
        }

        //
        // Set the local and type pointer.
        //

        Type = DataTypeRelation;
        LocalData = (PVOID)&Relation;
        LocalDataSize = sizeof(DATA_TYPE_RELATION);

    //
    // An 'r' indicates that this type is a subrange of another type, and is an
    // integer type.
    //

    } else if (*String == 'r') {

        //
        // Skip past the type descriptor.
        //

        while ((*String != ';') && (*String != '\0')) {
            String += 1;
        }

        if (*String == '\0') {
            String = NULL;
            goto CreateTypeEnd;
        }

        //
        // Parse the range parameters. Ranges take the form 'rType;min;max;',
        // where Type is the original type the subrange is taken from, min and
        // max are the ranges minimum and maximum, inclusive.
        //

        String += 1;
        String = DbgpParseRange(String, &Range);
        if (String == NULL) {
            goto CreateTypeEnd;
        }

        //
        // Estimate the type based on the range given.
        //

        Result = DbgpRangeToNumericType(&Range, &Numeric);
        if (Result == FALSE) {
            String = NULL;
            goto CreateTypeEnd;
        }

        //
        // If an explicit size was specifed using '@' attributes, plug those in
        // now.
        //

        if (TypeSize != 0) {
            Numeric.BitSize = TypeSize;
        }

        STABS_DEBUG("New Numeric Type: %s:(%s,%d). Float: %d, Signed: %d, "
                    "Size: %d\n",
                    TypeName,
                    TypeOwner->SourceFile,
                    TypeNumber,
                    Numeric.Float,
                    Numeric.Signed,
                    Numeric.BitSize);

        //
        // Set the local type and pointer.
        //

        Type = DataTypeNumeric;
        LocalData = (PVOID)&Numeric;
        LocalDataSize = sizeof(DATA_TYPE_NUMERIC);

    //
    // An 'R' indicates a floating point type.
    //

    } else if (*String == 'R') {
        ValuesRead = sscanf(String, "R%d;%d", &FloatType, &FloatSize);
        if (ValuesRead != 2) {
            String = NULL;
            goto CreateTypeEnd;
        }

        //
        // A float type of 1 is a 32-bit single precision, and 2 is a 64-bit
        // double precision. With any other float type, take the next field
        // to be the size of the float in bytes.
        //

        Numeric.Float = TRUE;
        if (FloatType == 1) {
            Numeric.BitSize = 32;

        } else if (FloatType == 2) {
            Numeric.BitSize = 64;

        } else {
            Numeric.BitSize = 8 * FloatSize;
        }

        STABS_DEBUG("New Float Type: %s:(%s,%d). Size: %d\n",
                    TypeName,
                    TypeOwner->SourceFile,
                    TypeNumber,
                    Numeric.BitSize);

        //
        // Set the local type and pointer.
        //

        Type = DataTypeNumeric;
        LocalData = (PVOID)&Numeric;
        LocalDataSize = sizeof(DATA_TYPE_NUMERIC);

    //
    // An 's' indicates that this type is a structure. A 'u' indicates a union,
    // which is treated the same as a structure
    //

    } else if ((*String == 's') || (*String == 'u')) {

        //
        // Immediately following the 's' is the size of the structure in bytes.
        //

        String += 1;
        ValuesRead = sscanf(String, "%d", &(Structure.SizeInBytes));
        if (ValuesRead != 1) {
            String = NULL;
            goto CreateTypeEnd;
        }

        //
        // Skip past the 's' and the size in bytes.
        //

        while ((*String >= '0') && (*String <= '9')) {
            String += 1;
        }

        STABS_DEBUG("New Structure Type: %s:(%s,%d). Size: %d\n",
                    TypeName,
                    TypeOwner->SourceFile,
                    TypeNumber,
                    Structure.SizeInBytes);

        //
        // Parse through each structure member, updating the main string each
        // time a structure member was successfully found.
        //

        StructureString = String;
        while (StructureString != NULL) {
            StructureString = DbgpParseStructureMember(Symbols,
                                                       StructureString,
                                                       &LocalStructureMember);

            if (StructureString != NULL) {

                //
                // Allocate a new structure member and copy the local data into
                // it.
                //

                NewStructureMember = MALLOC(sizeof(STRUCTURE_MEMBER));
                if (NewStructureMember == NULL) {
                    if (LocalStructureMember.Name != NULL) {
                        FREE(LocalStructureMember.Name);
                    }

                    String = NULL;
                    goto CreateTypeEnd;
                }

                memcpy(NewStructureMember,
                       &LocalStructureMember,
                       sizeof(STRUCTURE_MEMBER));

                //
                // Link up the new structure member.
                //

                if (CurrentStructureMember == NULL) {
                    Structure.FirstMember = NewStructureMember;

                } else {
                    CurrentStructureMember->NextMember = NewStructureMember;
                }

                CurrentStructureMember = NewStructureMember;
                CurrentStructureMember->NextMember = NULL;
                Structure.MemberCount += 1;
                String = StructureString;
                STABS_DEBUG("\t+%d, %d: %s (%s, %d)\n",
                            LocalStructureMember.BitOffset,
                            LocalStructureMember.BitSize,
                            LocalStructureMember.Name,
                            LocalStructureMember.TypeFile->SourceFile,
                            LocalStructureMember.TypeNumber);
            }
        }

        //
        // Move past the ending semicolon for the structure definition.
        //

        String += 1;

        //
        // Set the local type and data pointer.
        //

        Type = DataTypeStructure;
        LocalData = (PVOID)&Structure;
        LocalDataSize = sizeof(DATA_TYPE_STRUCTURE);

    //
    // An 'e' indicates that this type is an enumeration.
    //

    } else if (*String == 'e') {
        STABS_DEBUG("New Enumeration Type: %s:(%s,%d)\n",
                    TypeName,
                    TypeOwner->SourceFile,
                    TypeNumber);

        String += 1;
        EnumerationString = String;
        while (EnumerationString != NULL) {
            EnumerationString = DbgpParseEnumerationMember(
                                                    EnumerationString,
                                                    &LocalEnumerationMember);

            if (EnumerationString != NULL) {

                //
                // Allocate space for the new member and copy the data to that
                // new buffer.
                //

                NewEnumerationMember = MALLOC(sizeof(ENUMERATION_MEMBER));
                if (NewEnumerationMember == NULL) {
                    if (LocalEnumerationMember.Name != NULL) {
                        FREE(LocalEnumerationMember.Name);
                    }

                    String = NULL;
                    goto CreateTypeEnd;
                }

                memcpy(NewEnumerationMember,
                       &LocalEnumerationMember,
                       sizeof(ENUMERATION_MEMBER));

                //
                // Link up the new enumeration member.
                //

                if (CurrentEnumerationMember == NULL) {
                    Enumeration.FirstMember = NewEnumerationMember;

                } else {
                    CurrentEnumerationMember->NextMember = NewEnumerationMember;
                }

                CurrentEnumerationMember = NewEnumerationMember;
                NewEnumerationMember->NextMember = NULL;
                Enumeration.MemberCount += 1;
                String = EnumerationString;
                STABS_DEBUG("\t%s = %d\n",
                            LocalEnumerationMember.Name,
                            LocalEnumerationMember.Value);
            }
        }

        //
        // Skip over the ending semicolon.
        //

        String += 1;

        //
        // Assume all enumerations are 4 bytes since there's no other
        // information to go on.
        //

        Enumeration.SizeInBytes = 4;

        //
        // Set the local type and data pointer.
        //

        Type = DataTypeEnumeration;
        LocalData = (PVOID)&Enumeration;
        LocalDataSize = sizeof(DATA_TYPE_ENUMERATION);

    } else {

        //
        // This case indicates an unexpected type descriptor was encountered.
        // Fail the creation here.
        //

        String = NULL;
        goto CreateTypeEnd;
    }

    //
    // If a new type was successfully parsed, create the memory for it and add
    // it to the current module's type list.
    //

    if (Type != DataTypeInvalid) {

        assert(LocalData != NULL);
        assert(LocalDataSize != 0);

        NewType = MALLOC(sizeof(TYPE_SYMBOL));
        if (NewType == NULL) {
            String = NULL;
            goto CreateTypeEnd;
        }

        memset(NewType, 0, sizeof(TYPE_SYMBOL));
        memcpy(&(NewType->U), LocalData, LocalDataSize);
        NewType->ParentSource = TypeOwner;
        NewType->ParentFunction = NULL;
        NewType->Name = TypeName;
        NewType->TypeNumber = TypeNumber;
        NewType->Type = Type;
        INSERT_AFTER(&(NewType->ListEntry),
                     &(TypeOwner->TypesHead));
    }

CreateTypeEnd:
    if (String == NULL) {

        //
        // Clean up partial structure members.
        //

        CurrentStructureMember = Structure.FirstMember;
        while (CurrentStructureMember != NULL) {
            NextStructureMember = CurrentStructureMember->NextMember;
            if (CurrentStructureMember->Name != NULL) {
                FREE(CurrentStructureMember->Name);
            }

            FREE(CurrentStructureMember);
            CurrentStructureMember = NextStructureMember;
        }

        if (NewType != NULL) {
            FREE(NewType);
        }

        //
        // Clean up partial enumeration members.
        //

        CurrentEnumerationMember = Enumeration.FirstMember;
        while (CurrentEnumerationMember != NULL) {
            NextEnumerationMember = CurrentEnumerationMember->NextMember;
            if (CurrentEnumerationMember->Name != NULL) {
                FREE(CurrentEnumerationMember->Name);
            }

            FREE(CurrentEnumerationMember);
            CurrentEnumerationMember = NextEnumerationMember;
        }
    }

    return String;
}

PSTR
DbgpParseEnumerationMember (
    PSTR String,
    PENUMERATION_MEMBER Member
    )

/*++

Routine Description:

    This routine parses an enumeration member from a stab string. The caller is
    responsible for freeing memory that will be allocated to hold the name
    parameter for the enumeration member.

Arguments:

    String - Supplies the string containing the structure member.

    Member - Supplies a pointer to where the enumeration data should be written.
        It is assumed this structure has been allocated by the caller.

Return Value:

    Returns the String parameter, advanced by as much as the enumeration member
    took up, or NULL on failure.

--*/

{

    PSTR CurrentPosition;
    PSTR NameEnd;

    if ((String == NULL) || (Member == NULL)) {
        return NULL;
    }

    //
    // A semicolon indicates the end of the enumeration definition has been
    // reached.
    //

    if (*String == ';') {
        return NULL;
    }

    //
    // Zip past the end of the string. Unlike a structure member, in this case
    // double colons are not a concern, any colon ends the name.
    //

    CurrentPosition = String;
    while ((*CurrentPosition != '\0') && (*CurrentPosition != ':')) {
        CurrentPosition += 1;
    }

    if (*CurrentPosition == '\0') {
        return NULL;
    }

    //
    // Save the location of the end of the name, and get the value.
    //

    NameEnd = CurrentPosition;
    Member->Value = strtoll(NameEnd + 1, NULL, 10);

    //
    // The enumeration member is terminated with a comma. Find the end.
    //

    while ((*CurrentPosition != '\0') && (*CurrentPosition != ',')) {
        CurrentPosition += 1;
    }

    if (*CurrentPosition == '\0') {
        return NULL;
    }

    CurrentPosition += 1;
    Member->Name = MALLOC(NameEnd - String + 1);
    if (Member->Name == NULL) {
        return NULL;
    }

    strncpy(Member->Name, String, NameEnd - String);
    Member->Name[NameEnd - String] = '\0';
    return CurrentPosition;
}

PSTR
DbgpParseStructureMember (
    PDEBUG_SYMBOLS Symbols,
    PSTR String,
    PSTRUCTURE_MEMBER Member
    )

/*++

Routine Description:

    This routine parses a structure member from a stab string. The caller is
    responsible for freeing memory that will be allocated to hold the name
    parameter for the structure member.

Arguments:

    Symbols - Supplies a pointer to the current module.

    String - Supplies the string containing the structure member.

    Member - Supplies a pointer to where the structure data should be written.
        It is assumed this structure has been allocated by the caller.

Return Value:

    Returns the String parameter, advanced by the amount the structure member
    took up, or NULL on failure.

--*/

{

    PSTR CurrentPosition;
    PSTR NameEnd;
    INT ValuesRead;

    CurrentPosition = String;

    //
    // Parameter checking.
    //

    if ((Symbols == NULL) || (String == NULL) || (Member == NULL) ||
        (Symbols == NULL) || (*String == ';')) {

        return NULL;
    }

    //
    // Zip past the string name. Two colons can still be part of a name, so make
    // sure to terminate only on a single colon.
    //

    while (*CurrentPosition != '\0') {
        if (*CurrentPosition == ':') {
            if ((*(CurrentPosition - 1) != ':') &&
                (*(CurrentPosition + 1) != ':')) {

                break;
            }
        }

        CurrentPosition += 1;
    }

    if (*CurrentPosition == '\0') {
        return NULL;
    }

    //
    // Save the location of the end of the Name, and get the type associated
    // with this member.
    //

    NameEnd = CurrentPosition;
    CurrentPosition += 1;
    CurrentPosition = DbgpGetTypeNumber(Symbols,
                                        NULL,
                                        CurrentPosition,
                                        &(Member->TypeFile),
                                        &(Member->TypeNumber));

    if (CurrentPosition == NULL) {
        return NULL;
    }

    //
    // Get the bit offset into the structure and bit size for this structure
    // member.
    //

    ValuesRead = sscanf(CurrentPosition,
                        ",%d,%d",
                        &(Member->BitOffset),
                        &(Member->BitSize));

    if (ValuesRead != 2) {
        return NULL;
    }

    //
    // Find the end of the string.
    //

    while ((*CurrentPosition != '\0') && (*CurrentPosition != ';')) {
        CurrentPosition += 1;
    }

    if (*CurrentPosition == '\0') {
        return NULL;
    }

    CurrentPosition += 1;

    //
    // Allocate memory for the name string and copy the name over.
    //

    Member->Name = MALLOC(NameEnd - String + 1);
    if (Member->Name == NULL) {
        return NULL;
    }

    strncpy(Member->Name, String, NameEnd - String);
    Member->Name[NameEnd - String] = '\0';
    return CurrentPosition;
}

PSTR
DbgpParseRange (
    PSTR String,
    PDATA_RANGE Range
    )

/*++

Routine Description:

    This routine parses a range type from a string and puts it into a structure
    allocated by the caller.

Arguments:

    String - Supplies the string containing the range.

    Range - Supplies a pointer to the range structure where the information is
        returned.

Return Value:

    Returns the amount the String parameter was advanced to parse the range, or
    NULL on failure.

--*/

{

    PSTR MaximumEnd;
    PSTR MaximumStart;
    CHAR MaximumString[MAX_RANGE_STRING];
    CHAR MinimumString[MAX_RANGE_STRING];
    ULONG StringSize;

    if ((String == NULL) || (Range == NULL)) {
        return NULL;
    }

    //
    // Find the boundaries of the two strings, and copy them into separate
    // buffers.
    //

    MaximumStart = strchr(String, ';');
    if (MaximumStart == NULL) {
        return NULL;
    }

    StringSize = MaximumStart - String;
    if ((StringSize >= MAX_RANGE_STRING) || (StringSize == 0)) {
        return NULL;
    }

    strncpy(MinimumString, String, StringSize);
    MinimumString[StringSize] = '\0';
    MaximumStart += 1;
    MaximumEnd = strchr(MaximumStart, ';');
    if (MaximumEnd == NULL) {
        return NULL;
    }

    StringSize = MaximumEnd - MaximumStart;
    if ((StringSize >= MAX_RANGE_STRING) || (StringSize == 0)) {
        return NULL;
    }

    strncpy(MaximumString, MaximumStart, StringSize);
    MaximumString[StringSize] = '\0';

    //
    // Determine if the maximum is the max of a ULONGLONG, which cannot be
    // stored in the given structure.
    //

    if (strcmp(MaximumString, "01777777777777777777777") == 0) {
        Range->MaxUlonglong = TRUE;

    } else {
        Range->MaxUlonglong = FALSE;
    }

    //
    // Read the values in.
    //

    Range->Minimum = strtoll(MinimumString, NULL, 0);
    Range->Maximum = strtoll(MaximumString, NULL, 0);
    if (Range->MaxUlonglong == TRUE) {
        Range->Maximum = 0;
    }

    //
    // There also seems to be a problem with signed 64 bit extremes. Check those
    // explicitly.
    //

    if (strcmp(MaximumString, "0777777777777777777777") == 0) {
        Range->Maximum = MAX_LONGLONG;
    }

    if (strcmp(MinimumString, "01000000000000000000000") == 0) {
        Range->Minimum = MIN_LONGLONG;
    }

    return MaximumEnd + 1;
}

BOOL
DbgpRangeToNumericType (
    PDATA_RANGE Range,
    PDATA_TYPE_NUMERIC Numeric
    )

/*++

Routine Description:

    This routine estimates a numeric type based on the given range by comparing
    the range to well known values.

Arguments:

    Range - Supplies a pointer to the range structure.

    Numeric - Supplies a pointer to a preallocated structure where the type
        information should be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    if ((Range == NULL) || (Numeric == NULL)) {
        return FALSE;
    }

    if (Range->Minimum < 0) {
        Numeric->Signed = TRUE;

    } else {
        Numeric->Signed = FALSE;
    }

    //
    // Find the first range that fits.
    //

    if (Range->Maximum <= MAX_UCHAR) {
        Numeric->BitSize = sizeof(UCHAR) * BITS_PER_BYTE;

    } else if (Range->Maximum <= MAX_USHORT) {
        Numeric->BitSize = sizeof(USHORT) * BITS_PER_BYTE;

    } else if (Range->Maximum <= MAX_ULONG) {
        Numeric->BitSize = sizeof(ULONG) * BITS_PER_BYTE;

    } else if (Range->Maximum <= MAX_ULONGLONG) {
        Numeric->BitSize = sizeof(ULONGLONG) * BITS_PER_BYTE;
    }

    //
    // A maximum range of 0 and a positive minimum range indicates that the type
    // is floating point. The size is indicated by the minimum, in bytes.
    //

    if ((Range->Maximum == 0) && (Range->Minimum > 0)) {
        Numeric->Float = TRUE;
        Numeric->BitSize = Range->Minimum * 8;

    } else {
        Numeric->Float = FALSE;
    }

    return TRUE;
}

PSTR
DbgpGetTypeNumber (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PSTR String,
    PSOURCE_FILE_SYMBOL *OwningFile,
    PLONG TypeNumber
    )

/*++

Routine Description:

    This routine parses a type reference string, returning the file number and
    type number. In its parsing it may add new types as it discovers their
    definitions.

Arguments:

    Symbols - Supplies a pointer to the current module.

    Name - Supplies the name of the type. If the type is about to be defined,
        this parameter is passed along an attributed to the new type. Otherwise
        it is unused.

    String - Supplies the string containing the type number.

    OwningFile - Supplies a pointer where the file owning this symbol is
        returned on success.

    TypeNumber - Supplies a pointer where the type number is returned.

Return Value:

    Returns the string advanced past the type, or NULL on any failure.

--*/

{

    PINCLUDE_STACK_ELEMENT CurrentIncludeElement;
    PSTR EndBuiltinString;
    PSTR EndString;
    ULONG IncludeFileNumber;
    INT MatchedItems;
    PSOURCE_FILE_SYMBOL Owner;
    PSOURCE_FILE_SYMBOL PotentialOwner;
    PSTAB_CONTEXT State;
    LONG Type;

    if ((String == NULL) || (*String == '\0')) {
        return NULL;
    }

    EndString = String;
    State = Symbols->SymbolContext;

    //
    // The form is either simply a type number or "(x,y)", where x specifies a
    // file number in the include stack, and y is the type number.
    //

    if (String[0] == '(') {
        MatchedItems = sscanf(String,
                              "(%d,%d)",
                              &IncludeFileNumber,
                              &Type);

        if (MatchedItems != 2) {
            return NULL;
        }

        while (*EndString != ')') {
            EndString += 1;
        }

        EndString += 1;

    } else {
        IncludeFileNumber = 0;
        MatchedItems = sscanf(String, "%d", &Type);
        if (MatchedItems != 1) {
            return NULL;
        }

        if (*EndString == '-') {
            EndString += 1;
        }

        while ((*EndString >= '0') && (*EndString <= '9')) {
            EndString += 1;
        }

        if (*EndString == ';') {
            EndString += 1;
        }
    }

    //
    // Based on the include file number, get the owning source file.
    //

    if (IncludeFileNumber == 0) {
        Owner = State->CurrentSourceFile;

    } else if (IncludeFileNumber <= State->MaxIncludeIndex) {
        Owner = NULL;
        CurrentIncludeElement = State->IncludeStack;
        while (CurrentIncludeElement != NULL) {
            PotentialOwner = CurrentIncludeElement->IncludeFile;
            if (CurrentIncludeElement->Index == IncludeFileNumber) {
                Owner = PotentialOwner;
                break;
            }

            CurrentIncludeElement = CurrentIncludeElement->NextElement;
        }

        if (Owner == NULL) {
            return NULL;
        }

    } else {
        DbgOut("Invalid Include Number: (%d, %d), Max include: %d\n",
               IncludeFileNumber,
               Type,
               State->MaxIncludeIndex);

        return NULL;
    }

    //
    // If the type being referenced is also being defined, create that type now.
    //

    if (*EndString == '=') {
        EndString += 1;
        EndString = DbgpCreateType(Symbols,
                                   Name,
                                   Owner,
                                   Type,
                                   EndString);

        if (EndString == NULL) {
            return NULL;
        }
    }

    //
    // If a builtin type is being referenced, create that builtin type now.
    //

    if (Type < 0) {
        if (Type == BUILTIN_TYPE_BOOL) {
            EndBuiltinString = DbgpCreateType(Symbols,
                                              NULL,
                                              Owner,
                                              BUILTIN_TYPE_BOOL,
                                              BUILTIN_TYPE_BOOL_STRING);

            assert(EndBuiltinString != NULL);
        }
    }

    //
    // If the owning file and type parameter were passed, set those variables
    // now.
    //

    if (OwningFile != NULL) {
        *OwningFile = Owner;
    }

    if (TypeNumber != NULL) {
        *TypeNumber = Type;
    }

    return EndString;
}

BOOL
DbgpParseSourceFileStab (
    PDEBUG_SYMBOLS Symbols,
    PRAW_STAB Stab,
    PSTR StabString,
    BOOL Include
    )

/*++

Routine Description:

    This routine parses through a source or begin include file stab, updating
    the output symbol information as well as the parse state.

Arguments:

    Symbols - Supplies a pointer to the symbols data. The raw stab data should
        already be loaded, and the parse state pointer should not be null.
        Also returns the initialized data.

    Stab - Supplies a pointer to the stab of type STAB_SOURCE_FILE,
        STAB_INCLUDE_BEGIN, or STAB_INCLUDE_PLACEHOLDER.

    StabString - Supplies a pointer to the stab's string, or NULL if the stab
        has no string.

    Include - Supplies whether or not this is an include file or a main source
        file.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PINCLUDE_STACK_ELEMENT CurrentIncludeElement;
    PLIST_ENTRY CurrentSourceEntry;
    ULONGLONG EndAddress;
    LONG FilenameCompare;
    BOOL FoundExistingFile;
    PINCLUDE_STACK_ELEMENT NewIncludeElement;
    PSOURCE_FILE_SYMBOL NewSource;
    PINCLUDE_STACK_ELEMENT NextIncludeElement;
    BOOL PathFullySpecified;
    PSOURCE_FILE_SYMBOL PotentialSource;
    BOOL Result;
    PSTAB_CONTEXT State;
    ULONG StringLength;

    NewSource = NULL;
    PathFullySpecified = FALSE;

    //
    // Parameter checking.
    //

    if ((Symbols == NULL) || (Symbols->SymbolContext == NULL) ||
        ((Stab != NULL) &&
         (Stab->Type != STAB_SOURCE_FILE) &&
         (Stab->Type != STAB_INCLUDE_BEGIN) &&
         (Stab->Type != STAB_INCLUDE_PLACEHOLDER) &&
         (Stab->Type != STAB_INCLUDE_NAME))) {

        return FALSE;
    }

    State = Symbols->SymbolContext;

    //
    // The current source file, line or function may be terminated, so
    // calculate the best estimate to use for where those lines/files/functions
    // end.
    //

    EndAddress = 0;
    if (Stab != NULL) {
        EndAddress = Stab->Value;
    }

    //
    // Figure out if the path was completely specified in the string or if
    // the current directory should be examined as well.
    //

    if ((StabString != NULL) &&
        ((StabString[0] == '/') || (strchr(StabString, ':') != NULL))) {

        PathFullySpecified = TRUE;
    }

    //
    // If the file is an include file, attempt to find it in the existing
    // includes.
    //

    FoundExistingFile = FALSE;
    if (StabString != NULL) {
        CurrentSourceEntry = Symbols->SourcesHead.Next;
        while (CurrentSourceEntry != &(Symbols->SourcesHead)) {
            PotentialSource = LIST_VALUE(CurrentSourceEntry,
                                         SOURCE_FILE_SYMBOL,
                                         ListEntry);

            assert(CurrentSourceEntry != NULL);
            assert(PotentialSource->SourceFile != NULL);

            CurrentSourceEntry = CurrentSourceEntry->Next;

            //
            // If the identifiers don't line up, skip it.
            //

            if (Stab->Value != PotentialSource->Identifier) {
                continue;
            }

            //
            // Compare the file names as well.
            //

            FilenameCompare = strcmp(StabString, PotentialSource->SourceFile);
            if (FilenameCompare != 0) {
                continue;
            }

            NewSource = PotentialSource;
            FoundExistingFile = TRUE;
            break;
        }
    }

    //
    // Wrap up the current source file if it's not an include.
    //

    if (Include == FALSE) {

        //
        // Remember how far the current file has come.
        //

        if ((State->CurrentSourceLineFile != NULL) &&
            (EndAddress > State->CurrentSourceLineFile->EndAddress)) {

            State->CurrentSourceLineFile->EndAddress = EndAddress;
        }

        if ((State->CurrentSourceFile != NULL) &&
            (EndAddress > State->CurrentSourceFile->EndAddress)) {

            State->CurrentSourceFile->EndAddress = EndAddress;
        }

        if ((Stab == NULL) || (Stab->Type != STAB_INCLUDE_NAME)) {
            if (State->CurrentSourceFile != NULL) {

                //
                // The source line file should always be valid if the current
                // source file is.
                //

                assert(State->CurrentSourceLineFile != NULL);

                //
                // Start by resolving all the cross references in this file.
                //

                Result = DbgpResolveCrossReferences(State);
                if (Result == FALSE) {
                    goto ParseSourceFileStabEnd;
                }

                State->CurrentSourceFile = NULL;
                State->CurrentSourceLineFile = NULL;
            }

            //
            // Wrap up the current function.
            //

            if (State->CurrentFunction != NULL) {
                State->CurrentFunction->EndAddress = EndAddress;
                State->CurrentFunction = NULL;
            }

            //
            // Reset the include stack. Each source file has its own include
            // stack.
            //

            CurrentIncludeElement = State->IncludeStack;
            while (CurrentIncludeElement != NULL) {
                NextIncludeElement = CurrentIncludeElement->NextElement;
                FREE(CurrentIncludeElement);
                CurrentIncludeElement = NextIncludeElement;
            }

            State->IncludeStack = NULL;
            State->MaxBraceAddress = 0;
            State->MaxIncludeIndex = 0;
        }

        //
        // Wrap up the current line, even for include names.
        //

        if (State->CurrentSourceLine != NULL) {
            State->CurrentSourceLine->End = EndAddress;
            if (State->CurrentSourceLine->Start > EndAddress) {
                State->CurrentSourceLine->End = State->CurrentSourceLine->Start;
            }

            State->CurrentSourceLine = NULL;
        }
    }

    //
    // If the stab has no string, it terminates the current source file. Since
    // that's what just happened, there is nothing more to do. Include Name
    // stabs should not use this mechanism.
    //

    if ((StabString == NULL) || (*StabString == '\0')) {
        if ((Stab != NULL) && (Stab->Type == STAB_INCLUDE_NAME)) {
            Result = FALSE;

        } else {
            Result = TRUE;
        }

        goto ParseSourceFileStabEnd;
    }

    //
    // If the stab has a slash at the end, it is a source directory stab.
    // Update the current directory with this new stab's string.
    //

    StringLength = strlen(StabString);
    if (StabString[StringLength - 1] == '/') {
        State->CurrentSourceDirectory = StabString;
        Result = TRUE;
        goto ParseSourceFileStabEnd;
    }

    //
    // If a new source file hasn't been allocated by this point, allocate one
    // now.
    //

    if (NewSource == NULL) {
        NewSource = MALLOC(sizeof(SOURCE_FILE_SYMBOL));
    }

    if (NewSource == NULL) {
        Result = FALSE;
        goto ParseSourceFileStabEnd;
    }

    if (FoundExistingFile == FALSE) {
        memset(NewSource, 0, sizeof(SOURCE_FILE_SYMBOL));
        if (PathFullySpecified == FALSE) {
            NewSource->SourceDirectory = State->CurrentSourceDirectory;
        }

        NewSource->SourceFile = StabString;
        INITIALIZE_LIST_HEAD(&(NewSource->SourceLinesHead));
        INITIALIZE_LIST_HEAD(&(NewSource->DataSymbolsHead));
        INITIALIZE_LIST_HEAD(&(NewSource->FunctionsHead));
        INITIALIZE_LIST_HEAD(&(NewSource->TypesHead));
        NewSource->StartAddress = Stab->Value;

        //
        // The stab value is used to match EXCL stabs to the includes (BINCL)
        // they reference.
        //

        NewSource->Identifier = Stab->Value;
    }

    //
    // If the file is an include, add it to the include stack.
    //

    if (Include != FALSE) {
        State->MaxIncludeIndex += 1;
        if (FoundExistingFile == FALSE) {
            NewSource->StartAddress = 0;
            NewSource->EndAddress = 0;
        }

        NewIncludeElement = MALLOC(sizeof(INCLUDE_STACK_ELEMENT));
        if (NewIncludeElement == NULL) {
            Result = FALSE;
            goto ParseSourceFileStabEnd;
        }

        NewIncludeElement->IncludeFile = NewSource;
        NewIncludeElement->Index = State->MaxIncludeIndex;
        NewIncludeElement->NextElement = State->IncludeStack;
        State->IncludeStack = NewIncludeElement;
    }

    //
    // Add the new source file to the current module at the front of the list.
    //

    assert(State->CurrentModule != NULL);

    if (FoundExistingFile == FALSE) {
        INSERT_AFTER(&(NewSource->ListEntry),
                     &(State->CurrentModule->SourcesHead));
    }

    //
    // Include name stabs only affect source lines, not other symbols. All other
    // non-include source file stabs affect the main source file.
    //

    if (Stab->Type == STAB_INCLUDE_NAME) {

        assert(State->CurrentSourceFile != NULL);

        if (NewSource->StartAddress == 0) {
            NewSource->StartAddress = State->CurrentSourceFile->StartAddress;
        }

        State->CurrentSourceLineFile = NewSource;

    } else if (Include == FALSE) {
        State->CurrentSourceFile = NewSource;
        State->CurrentSourceLineFile = NewSource;
        State->MaxBraceAddress = NewSource->StartAddress;
    }

    Result = TRUE;

ParseSourceFileStabEnd:
    return Result;
}

BOOL
DbgpParseSourceLineStab (
    PDEBUG_SYMBOLS Symbols,
    PRAW_STAB Stab,
    PSTR StabString
    )

/*++

Routine Description:

    This routine parses through a source line stab, updating the output symbol
    information as well as the parse state.

Arguments:

    Symbols - Supplies a pointer to the symbols data. The raw stab data should
        already be loaded, and the parse state pointer should not be null.
        Also returns the initialized data.

    Stab - Supplies a pointer to the stab of type STAB_SOURCE_LINE.

    StabString - Supplies a pointer to the stab's string, or NULL if the stab
        has no string.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONGLONG Address;
    PSOURCE_LINE_SYMBOL NewLine;
    BOOL Result;
    PSTAB_CONTEXT State;

    //
    // Parameter checking.
    //

    if ((Symbols == NULL) ||
        (Symbols->SymbolContext == NULL) ||
        (Stab == NULL) ||
        (Stab->Type != STAB_SOURCE_LINE)) {

        return FALSE;
    }

    State = Symbols->SymbolContext;
    if (State->CurrentSourceLineFile == NULL) {
        return FALSE;
    }

    //
    // Skip line zero stabs.
    //

    if (Stab->Description == 0) {
        return TRUE;
    }

    //
    // Allocate a new source line.
    //

    NewLine = MALLOC(sizeof(SOURCE_LINE_SYMBOL));
    if (NewLine == NULL) {
        Result = FALSE;
        goto ParseSourceLineEnd;
    }

    memset(NewLine, 0, sizeof(SOURCE_LINE_SYMBOL));

    //
    // Fill in the line information.
    //

    NewLine->ParentSource = State->CurrentSourceLineFile;
    NewLine->LineNumber = Stab->Description;
    Address = Stab->Value;
    if (State->CurrentFunction != NULL) {
        Address += State->CurrentFunction->StartAddress;
    }

    NewLine->Start = Address;

    //
    // If there was a previous source line active, end it here.
    //

    if (State->CurrentSourceLine != NULL) {
        State->CurrentSourceLine->End = Address;
        State->CurrentSourceLine = NULL;
    }

    //
    // Add the line to the list, and set the current state.
    //

    INSERT_BEFORE(&(NewLine->ListEntry),
                  &(State->CurrentSourceLineFile->SourceLinesHead));

    State->CurrentSourceLine = NewLine;
    Result = TRUE;

ParseSourceLineEnd:
    if (Result == FALSE) {
        if (NewLine != NULL) {
            FREE(NewLine);
        }
    }

    return Result;
}

BOOL
DbgpParseFunctionStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    )

/*++

Routine Description:

    This routine parses through a function stab, updating the output symbol
    information as well as the parse state.

Arguments:

    Symbols - Supplies a pointer to the symbols data. The raw stab data should
        already be loaded, and the parse state pointer should not be null.
        Also returns the initialized data.

    Name - Supplies the name of the new function, or NULL if a name could not be
        parsed.

    Stab - Supplies a pointer to the stab.

    StabString - Supplies a pointer to the stab's string, or NULL if the stab
        has no string.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG EndAddress;
    PFUNCTION_SYMBOL NewFunction;
    BOOL Result;
    PSTR ReturnTypeString;
    PSTAB_CONTEXT State;

    NewFunction = NULL;

    //
    // Parameter checking.
    //

    if ((Symbols == NULL) || (Symbols->SymbolContext == NULL) ||
        (Stab == NULL) || (Stab->Type != STAB_FUNCTION)) {

        return FALSE;
    }

    State = Symbols->SymbolContext;

    //
    // If the string is NULL, the current function is ending. Also make sure to
    // end an open source line, if present.
    //

    if ((StabString == NULL) || (*StabString == '\0')) {
        EndAddress = Stab->Value;
        if (State->CurrentFunction != NULL) {
            EndAddress += State->CurrentFunction->StartAddress;
        }

        if (State->CurrentSourceLine != NULL) {
            State->CurrentSourceLine->End = EndAddress;
            State->CurrentSourceLine = NULL;
        }

        if (State->CurrentFunction != NULL) {
            State->CurrentFunction->EndAddress = EndAddress;
            State->CurrentFunction = NULL;
        }

        State->MaxBraceAddress = 0;
        Result = TRUE;
        goto ParseFunctionStabEnd;
    }

    //
    // This is a new function. Allocate space for it and initialize the list
    // heads.
    //

    NewFunction = MALLOC(sizeof(FUNCTION_SYMBOL));
    if (NewFunction == NULL) {
        Result = FALSE;
        goto ParseFunctionStabEnd;
    }

    memset(NewFunction, 0, sizeof(FUNCTION_SYMBOL));
    INITIALIZE_LIST_HEAD(&(NewFunction->ParametersHead));
    INITIALIZE_LIST_HEAD(&(NewFunction->LocalsHead));
    INITIALIZE_LIST_HEAD(&(NewFunction->FunctionsHead));
    NewFunction->Name = Name;

    //
    // Get the return type.
    //

    ReturnTypeString = StabString;
    if ((*ReturnTypeString != 'F') && (*ReturnTypeString != 'f')) {
        Result = FALSE;
        goto ParseFunctionStabEnd;
    }

    ReturnTypeString += 1;
    ReturnTypeString = DbgpGetTypeNumber(Symbols,
                                         NULL,
                                         ReturnTypeString,
                                         &(NewFunction->ReturnTypeOwner),
                                         &(NewFunction->ReturnTypeNumber));

    if (ReturnTypeString == NULL) {
        Result = FALSE;
        goto ParseFunctionStabEnd;
    }

    NewFunction->FunctionNumber = Stab->Description;
    NewFunction->ParentSource = State->CurrentSourceFile;
    NewFunction->StartAddress = Stab->Value;

    //
    // Insert the function into the current source file's list of functions.
    //

    INSERT_BEFORE(&(NewFunction->ListEntry),
                  &(State->CurrentSourceFile->FunctionsHead));

    State->CurrentFunction = NewFunction;
    State->MaxBraceAddress = NewFunction->StartAddress;
    Result = TRUE;

ParseFunctionStabEnd:
    if (Result == FALSE) {
        if (NewFunction != NULL) {
            FREE(NewFunction);
        }
    }

    return Result;
}

BOOL
DbgpParseFunctionParameterStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    )

/*++

Routine Description:

    This routine parses through a function parameter stab, updating the output
    symbol information as well as the parse state.

Arguments:

    Symbols - Supplies a pointer to the symbols data. The raw stab data should
        already be loaded, and the parse state pointer should not be null.
        Also returns the initialized data. The current function in the state
        should also not be NULL.

    Name - Supplies the name of the parameter, or NULL if a name could not be
        parsed.

    Stab - Supplies a pointer to the stab of type STAB_FUNCTION_PARAMETER.

    StabString - Supplies a pointer to the stab's string, or NULL if the stab
        has no string.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PDATA_SYMBOL NewParameter;
    PSTR ParameterTypeString;
    BOOL Result;
    PSTAB_CONTEXT State;

    NewParameter = NULL;

    //
    // Parameter checking.
    //

    if ((Symbols == NULL) || (Stab == NULL) ||
        ((Stab->Type != STAB_FUNCTION_PARAMETER) &&
         (Stab->Type != STAB_REGISTER_VARIABLE)) ||
        (Symbols->SymbolContext == NULL)) {

        return FALSE;
    }

    State = Symbols->SymbolContext;

    //
    // Create the new parameter on the heap.
    //

    NewParameter = MALLOC(sizeof(DATA_SYMBOL));
    if (NewParameter == NULL) {
        Result = FALSE;
        goto ParseFunctionParameterStabEnd;
    }

    memset(NewParameter, 0, sizeof(DATA_SYMBOL));
    NewParameter->Name = Name;

    //
    // Get the parameter type.
    //

    ParameterTypeString = StabString;
    if (*ParameterTypeString == 'P') {
        NewParameter->LocationType = DataLocationRegister;
        NewParameter->Location.Register = STAB_REGISTER_TO_GENERAL(Stab->Value);

    } else if (*ParameterTypeString == 'p') {
        NewParameter->LocationType = DataLocationIndirect;
        NewParameter->Location.Indirect.Offset = (LONG)Stab->Value;
        NewParameter->Location.Indirect.Register =
                                     DbgpStabsGetFramePointerRegister(Symbols);

    } else {
        Result = FALSE;
        goto ParseFunctionParameterStabEnd;
    }

    ParameterTypeString += 1;
    ParameterTypeString = DbgpGetTypeNumber(Symbols,
                                            NULL,
                                            ParameterTypeString,
                                            &(NewParameter->TypeOwner),
                                            &(NewParameter->TypeNumber));

    if (ParameterTypeString == NULL) {
        Result = FALSE;
        goto ParseFunctionParameterStabEnd;
    }

    NewParameter->ParentFunction = State->CurrentFunction;

    //
    // Insert the parameter into the current function's parameter list.
    //

    if (State->CurrentFunction != NULL) {
        INSERT_BEFORE(&(NewParameter->ListEntry),
                      &(State->CurrentFunction->ParametersHead));

        NewParameter = NULL;
    }

    Result = TRUE;

ParseFunctionParameterStabEnd:
    if (NewParameter != NULL) {
        FREE(NewParameter);
    }

    return Result;
}

BOOL
DbgpParseRegisterVariableStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    )

/*++

Routine Description:

    This routine parses through a register variable stab, updating the output
    symbol information as well as the parse state and current function.

Arguments:

    Symbols - Supplies a pointer to the symbols data. The raw stab data should
        already be loaded, and the parse state pointer should not be null.
        Also returns the initialized data. The current function in the state
        should also not be NULL.

    Name - Supplies the name of the register variable, or NULL if a name could
        not be parsed.

    Stab - Supplies a pointer to the stab of type STAB_REGISTER_VARIABLE.

    StabString - Supplies a pointer to the stab's string, or NULL if the stab
        has no string.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PDATA_SYMBOL NewLocal;
    BOOL Result;
    PSTAB_CONTEXT State;
    CHAR VariableFlavor;
    PSTR VariableType;

    NewLocal = NULL;

    //
    // Parameter checking.
    //

    if ((Symbols == NULL) || (Stab == NULL) ||
        (Stab->Type != STAB_REGISTER_VARIABLE) ||
        (Symbols->SymbolContext == NULL)) {

        return FALSE;
    }

    State = Symbols->SymbolContext;
    if (State->CurrentFunction == NULL) {
        return FALSE;
    }

    //
    // A capital P indicates that this is actually a parameter that was passed
    // solely through a register. Parse this as a parameter if that's the case.
    // A lowercase p indicates that this parameter was passed in through the
    // stack, but is later put into a register. These types get treated as as
    // separate variables, one on the stack and one in a register.
    //

    VariableFlavor = *StabString;
    if (VariableFlavor == 'P') {
        Result = DbgpParseFunctionParameterStab(Symbols,
                                                Name,
                                                Stab,
                                                StabString);

        goto ParseRegisterVariableStabEnd;
    }

    //
    // Create the new local variable on the heap.
    //

    NewLocal = MALLOC(sizeof(DATA_SYMBOL));
    if (NewLocal == NULL) {
        Result = FALSE;
        goto ParseRegisterVariableStabEnd;
    }

    memset(NewLocal, 0, sizeof(DATA_SYMBOL));
    NewLocal->Name = Name;

    //
    // Validate the parameter type.
    //

    if (VariableFlavor != 'r') {
        Result = FALSE;
        goto ParseRegisterVariableStabEnd;
    }

    VariableType = StabString + 1;
    VariableType = DbgpGetTypeNumber(Symbols,
                                     NULL,
                                     VariableType,
                                     &(NewLocal->TypeOwner),
                                     &(NewLocal->TypeNumber));

    if (VariableType == NULL) {
        Result = FALSE;
        goto ParseRegisterVariableStabEnd;
    }

    NewLocal->ParentFunction = State->CurrentFunction;
    NewLocal->ParentSource = State->CurrentSourceFile;
    NewLocal->LocationType = DataLocationRegister;
    NewLocal->Location.Register = STAB_REGISTER_TO_GENERAL(Stab->Value);
    NewLocal->MinimumValidExecutionAddress = State->MaxBraceAddress;

    //
    // Insert the variable into the current function's locals list.
    //

    INSERT_BEFORE(&(NewLocal->ListEntry),
                  &(State->CurrentFunction->LocalsHead));

    Result = TRUE;

ParseRegisterVariableStabEnd:
    if (Result == FALSE) {
        if (NewLocal != NULL) {
            FREE(NewLocal);
        }
    }

    return Result;
}

BOOL
DbgpParseBraceStab (
    PDEBUG_SYMBOLS Symbols,
    PRAW_STAB Stab,
    PSTR StabString
    )

/*++

Routine Description:

    This routine parses through a left or right brace stab, updating the parse
    state. This information is used primarily for repeated local variable
    definitions.

Arguments:

    Symbols - Supplies a pointer to the symbols data. The raw stab data should
        already be loaded, and the parse state pointer should not be null.
        The current function in the state should also not be NULL.

    Stab - Supplies a pointer to the stab of type STAB_LEFT_BRACE or
        STAB_RIGHT_BRACE.

    StabString - Supplies a pointer to the stab's string, or NULL if the stab
        has no string.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONGLONG Address;
    PSTAB_CONTEXT State;

    if ((Symbols == NULL) || (Stab == NULL) ||
        (Symbols->SymbolContext == NULL) ||
        ((Stab->Type != STAB_LEFT_BRACE) && (Stab->Type != STAB_RIGHT_BRACE))) {

        return FALSE;
    }

    State = Symbols->SymbolContext;
    if (State->CurrentFunction == NULL) {
        return FALSE;
    }

    Address = State->CurrentFunction->StartAddress + (LONG)Stab->Value;
    if (Address > State->MaxBraceAddress) {
        State->MaxBraceAddress = Address;
    }

    return TRUE;
}

BOOL
DbgpParseStaticSymbolStab (
    PDEBUG_SYMBOLS Symbols,
    PSTR Name,
    PRAW_STAB Stab,
    PSTR StabString
    )

/*++

Routine Description:

    This routine parses through a static (global) stab, updating the parse
    state and symbol information.

Arguments:

    Symbols - Supplies a pointer to the symbols data. The raw stab data should
        already be loaded, and the parse state pointer should not be null.

    Name - Supplies the name of the static symbol, or NULL if a name could not
        be parsed.

    Stab - Supplies a pointer to the stab of type STAB_STATIC.

    StabString - Supplies a pointer to the stab's string, or NULL if the stab
        has no string.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PDATA_SYMBOL NewStatic;
    BOOL Result;
    PSTAB_CONTEXT State;
    PSTR StaticScope;
    PSTR StaticType;

    NewStatic = NULL;

    //
    // Parameter checking.
    //

    if ((Symbols == NULL) ||
        (Stab == NULL) ||
        (Symbols->SymbolContext == NULL) ||
        ((Stab->Type != STAB_STATIC) &&
         (Stab->Type != STAB_GLOBAL_SYMBOL) &&
         (Stab->Type != STAB_BSS_SYMBOL)) ) {

        return FALSE;
    }

    State = Symbols->SymbolContext;

    //
    // Create the new static variable on the heap.
    //

    NewStatic = MALLOC(sizeof(DATA_SYMBOL));
    if (NewStatic == NULL) {
        Result = FALSE;
        goto ParseStaticSymbolStabEnd;
    }

    memset(NewStatic, 0, sizeof(DATA_SYMBOL));
    NewStatic->Name = Name;

    //
    // Get the scope of the static variable. S indicates file scope static, V
    // indicates function scope static. G indicates a global variable.
    //

    StaticScope = StabString;
    if ((*StaticScope != 'S') &&
        (*StaticScope != 'V') &&
        (*StaticScope != 'G')) {

        Result = FALSE;
        goto ParseStaticSymbolStabEnd;
    }

    NewStatic->ParentSource = State->CurrentSourceFile;
    NewStatic->LocationType = DataLocationAbsoluteAddress;
    NewStatic->Location.Address = Stab->Value;
    NewStatic->MinimumValidExecutionAddress = 0;
    StaticType = StaticScope + 1;
    StaticType = DbgpGetTypeNumber(Symbols,
                                   NULL,
                                   StaticType,
                                   &(NewStatic->TypeOwner),
                                   &(NewStatic->TypeNumber));

    if (StaticType == NULL) {
        Result = FALSE;
        goto ParseStaticSymbolStabEnd;
    }

    //
    // Add the file to the correct symbol list, depending on the scope.
    //

    if ((*StaticScope == 'S') || (*StaticScope == 'G')) {
        INSERT_BEFORE(&(NewStatic->ListEntry),
                      &(State->CurrentSourceFile->DataSymbolsHead));

    } else {

        assert(*StaticScope == 'V');

        if (State->CurrentFunction != NULL) {
            NewStatic->ParentFunction = State->CurrentFunction;
            NewStatic->MinimumValidExecutionAddress =
                                          State->CurrentFunction->StartAddress;

            INSERT_BEFORE(&(NewStatic->ListEntry),
                          &(State->CurrentFunction->LocalsHead));

        } else {
            INSERT_BEFORE(&(NewStatic->ListEntry),
                          &(State->CurrentSourceFile->DataSymbolsHead));
        }
    }

    Result = TRUE;

ParseStaticSymbolStabEnd:
    if (Result == FALSE) {
        if (NewStatic != NULL) {
            FREE(NewStatic);
        }
    }

    return Result;
}

BOOL
DbgpResolveCrossReferences (
    PSTAB_CONTEXT State
    )

/*++

Routine Description:

    This routine loops through the unresolved cross reference list in the
    stab parse state and creates types with resolved references. If the
    reference type cannot be found, an empty one is created.

Arguments:

    State - Supplies a pointer to the current parse state. The current
        source file should not be NULL.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    PCROSS_REFERENCE_ENTRY CrossReference;
    PLIST_ENTRY CurrentEntry;
    PTYPE_SYMBOL CurrentType;
    PLIST_ENTRY CurrentTypeEntry;
    BOOL Match;
    PSTR Name;
    PSTR NameEnd;
    ULONG NameLength;
    PSTR NameStart;
    PTYPE_SYMBOL NewType;
    PDATA_TYPE_ENUMERATION NewTypeEnumeration;
    PDATA_TYPE_RELATION NewTypeRelation;
    PDATA_TYPE_STRUCTURE NewTypeStructure;
    BOOL Result;

    Name = NULL;
    NewType = NULL;
    Result = TRUE;
    CurrentEntry = State->CrossReferenceListHead.Next;
    while (CurrentEntry != &(State->CrossReferenceListHead)) {
        CrossReference = LIST_VALUE(CurrentEntry,
                                    CROSS_REFERENCE_ENTRY,
                                    ListEntry);

        //
        // Allocate the new type that will be generated for this reference.
        //

        NewType = MALLOC(sizeof(TYPE_SYMBOL));
        if (NewType == NULL) {
            Result = FALSE;
            goto ResolveCrossReferencesEnd;
        }

        memset(NewType, 0, sizeof(TYPE_SYMBOL));

        //
        // Find the end of the reference name, marked by one colon. Two colons
        // do not mark the end of the reference name.
        //

        NameStart = CrossReference->ReferenceString + 1;
        NameEnd = NameStart;
        while (TRUE) {
            NameEnd = strchr(NameEnd, ':');
            if ((NameEnd == NULL) || (*(NameEnd + 1) != ':')) {
                break;
            }

            NameEnd += 2;
        }

        //
        // Create a copy of the name.
        //

        if (NameEnd != NULL) {
            NameLength = (UINTN)NameEnd - (UINTN)NameStart;

        } else {
            NameLength = strlen(NameStart);
        }

        Name = MALLOC(NameLength + 1);
        if (Name == NULL) {
            Result = FALSE;
            goto ResolveCrossReferencesEnd;
        }

        strncpy(Name, NameStart, NameLength);
        Name[NameLength] = '\0';

        //
        // Loop through all the types in the source file, checking for a match
        // of both the name and type.
        //

        Match = FALSE;
        CurrentTypeEntry = CrossReference->ReferringTypeSource->TypesHead.Next;
        while (CurrentTypeEntry !=
                           &(CrossReference->ReferringTypeSource->TypesHead)) {

            CurrentType = LIST_VALUE(CurrentTypeEntry,
                                     TYPE_SYMBOL,
                                     ListEntry);

            switch (CrossReference->ReferenceString[0]) {
            case 's':
            case 'u':
                if ((CurrentType->Name != NULL) &&
                    (CurrentType->Type == DataTypeStructure) &&
                    (strcmp(Name, CurrentType->Name) == 0)) {

                    Match = TRUE;
                }

                break;

            case 'e':
                if ((CurrentType->Name != NULL) &&
                    (CurrentType->Type == DataTypeEnumeration) &&
                    (strcmp(Name, CurrentType->Name) == 0)) {

                    Match = TRUE;
                }

                break;

            default:

                //
                // Unknown reference type.
                //

                Result = FALSE;
                goto ResolveCrossReferencesEnd;
            }

            //
            // If a match happened, resolve the reference and stop looping
            // through types.
            //

            if (Match != FALSE) {
                NewType->Type = DataTypeRelation;
                NewTypeRelation = &(NewType->U.Relation);
                NewTypeRelation->Pointer = 0;
                NewTypeRelation->OwningFile = CurrentType->ParentSource;
                NewTypeRelation->TypeNumber = CurrentType->TypeNumber;
                NewTypeRelation->Array.Minimum = 0;
                NewTypeRelation->Array.Maximum = 0;
                NewTypeRelation->Function = FALSE;
                break;
            }

            CurrentTypeEntry = CurrentTypeEntry->Next;
        }

        //
        // Initialize the new type.
        //

        NewType->Name = NULL;
        NewType->ParentSource = CrossReference->ReferringTypeSource;
        NewType->ParentFunction = NULL;
        NewType->TypeNumber = CrossReference->ReferringTypeNumber;

        //
        // If a match was not found, this type is going to have to become the
        // reference itself.
        //

        if (Match == FALSE) {
            NewType->Name = Name;
            Name = NULL;
            switch (CrossReference->ReferenceString[0]) {
            case 'u':
            case 's':
                NewType->Type = DataTypeStructure;
                NewTypeStructure = &(NewType->U.Structure);
                NewTypeStructure->SizeInBytes = 0;
                NewTypeStructure->MemberCount = 0;
                NewTypeStructure->FirstMember = NULL;
                break;

            case 'e':
                NewType->Type = DataTypeEnumeration;
                NewTypeEnumeration = &(NewType->U.Enumeration);
                NewTypeEnumeration->MemberCount = 0;
                NewTypeEnumeration->FirstMember = NULL;
                NewTypeEnumeration->SizeInBytes = 4;
                break;

            default:

                //
                // Unknown type reference.
                //

                Result = FALSE;
                goto ResolveCrossReferencesEnd;
            }
        }

        INSERT_BEFORE(&(NewType->ListEntry),
                      &(CrossReference->ReferringTypeSource->TypesHead));

        CurrentEntry = CurrentEntry->Next;
        LIST_REMOVE(&(CrossReference->ListEntry));
        FREE(CrossReference);
        if (Name != NULL) {
            FREE(Name);
            Name = NULL;
        }
    }

    //
    // Assert that the list has been completely emptied.
    //

    assert(State->CrossReferenceListHead.Next ==
                                             &(State->CrossReferenceListHead));

ResolveCrossReferencesEnd:
    if (Name != NULL) {
        FREE(Name);
    }

    if (Result == FALSE) {
        if (NewType != NULL) {
            if (NewType->Name != NULL) {
                FREE(NewType->Name);
            }

            FREE(NewType);
        }
    }

    return Result;
}

LONG
DbgpGetFileSize (
    FILE *File
    )

/*++

Routine Description:

    This routine determines the size of an opened file.

Arguments:

    File - Supplies the file handle.

Return Value:

    Returns the file length.

--*/

{

    struct stat Stat;

    if (fstat(fileno(File), &Stat) != 0) {
        return -1;
    }

    return Stat.st_size;
}

ULONG
DbgpStabsGetFramePointerRegister (
    PDEBUG_SYMBOLS Symbols
    )

/*++

Routine Description:

    This routine returns the frame pointer register for use in indirect
    addresses.

Arguments:

    Symbols - Supplies a pointer to the symbols.

Return Value:

    Returns the machine-dependent frame pointer register.

--*/

{

    PSTAB_CONTEXT ParseState;

    if (Symbols->Machine == ImageMachineTypeX86) {
        return X86RegisterEbp;

    } else if (Symbols->Machine == ImageMachineTypeX64) {
        return X64RegisterRbp;

    } else if (Symbols->Machine == ImageMachineTypeArm32) {
        ParseState = Symbols->SymbolContext;

        //
        // If the current function has the thumb bit set, then use the thumb
        // frame pointer register.
        //

        if ((ParseState != NULL) && (ParseState->CurrentFunction != NULL) &&
            ((ParseState->CurrentFunction->StartAddress & 0x1) != 0)) {

            return ArmRegisterR7;
        }

        //
        // Return R11, the ARM frame pointer register.
        //

        return ArmRegisterR11;
    }

    assert(FALSE);

    return 0;
}

