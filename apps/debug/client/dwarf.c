/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/im.h>
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
DwarfReadDataSymbol (
    PDEBUG_SYMBOLS Symbols,
    PDATA_SYMBOL Symbol,
    ULONGLONG DebasedPc,
    PVOID Data,
    ULONG DataSize,
    PSTR Location,
    ULONG LocationSize
    );

INT
DwarfGetAddressOfDataSymbol (
    PDEBUG_SYMBOLS Symbols,
    PDATA_SYMBOL Symbol,
    ULONGLONG DebasedPc,
    PULONGLONG Address
    );

BOOL
DwarfpCheckRange (
    PDEBUG_SYMBOLS Symbols,
    PSOURCE_FILE_SYMBOL Source,
    ULONGLONG Address,
    PVOID Ranges
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

PSOURCE_FILE_SYMBOL
DwarfpCreateSource (
    PDWARF_CONTEXT Context,
    PSTR Directory,
    PSTR FileName
    );

VOID
DwarfpDestroyFunction (
    PFUNCTION_SYMBOL Function
    );

//
// -------------------------------------------------------------------- Globals
//

DEBUG_SYMBOL_INTERFACE DwarfSymbolInterface = {
    DwarfLoadSymbols,
    DwarfUnloadSymbols,
    DwarfStackUnwind,
    DwarfReadDataSymbol,
    DwarfGetAddressOfDataSymbol,
    DwarfpCheckRange
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

            DwarfpDestroyFunction(Function);
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

INT
DwarfReadDataSymbol (
    PDEBUG_SYMBOLS Symbols,
    PDATA_SYMBOL Symbol,
    ULONGLONG DebasedPc,
    PVOID Data,
    ULONG DataSize,
    PSTR Location,
    ULONG LocationSize
    )

/*++

Routine Description:

    This routine reads the contents of a data symbol.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    Symbol - Supplies a pointer to the data symbol to read.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus the
        loaded base difference of the module).

    Data - Supplies a pointer to the buffer where the symbol data will be
        returned on success.

    DataSize - Supplies the size of the data buffer in bytes.

    Location - Supplies a pointer where the symbol location will be described
        in text on success.

    LocationSize - Supplies the size of the location buffer in bytes.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR Comma;
    PDWARF_COMPLEX_DATA_SYMBOL Complex;
    PDWARF_CONTEXT Context;
    PDWARF_LOCATION CurrentLocation;
    DWARF_LOCATION_CONTEXT LocationContext;
    ULONG MaxBit;
    CHAR PieceLocation[32];
    INT Printed;
    ULONG Size;
    INT Status;
    ULONGLONG Value;

    Comma = "";
    Context = Symbols->SymbolContext;

    assert(Symbol->LocationType == DataLocationComplex);

    Complex = Symbol->Location.Complex;
    memset(&LocationContext, 0, sizeof(DWARF_LOCATION_CONTEXT));
    memset(Data, 0, DataSize);
    LocationContext.Unit = Complex->Unit;
    LocationContext.CurrentFunction = Symbol->ParentFunction;
    LocationContext.Pc = DebasedPc;
    Status = DwarfpGetLocation(Context,
                               &LocationContext,
                               &(Complex->LocationAttribute));

    if (Status != 0) {
        if (Status != ENOENT) {
            DWARF_ERROR("DWARF: Failed to get location for symbol %s: %s.\n",
                        Symbol->Name,
                        strerror(Status));
        }

        goto ReadDataSymbolEnd;
    }

    CurrentLocation = &(LocationContext.Location);
    while (CurrentLocation != NULL) {

        //
        // Figure out the size to copy, without regard to the source size. Note
        // that if multiple bitwise fields came together, this loop would need
        // to be adjusted to take into account (as well as not clobber) the
        // previous bits.
        //

        Size = DataSize;
        if (CurrentLocation->BitSize != 0) {
            Size = CurrentLocation->BitSize / BITS_PER_BYTE;
            if (Size > DataSize) {
                Size = DataSize;
            }
        }

        switch (CurrentLocation->Form) {
        case DwarfLocationMemory:
            Status = DwarfTargetRead(Context,
                                     CurrentLocation->Value.Address,
                                     Size,
                                     0,
                                     Data);

            if (Status != 0) {
                DWARF_ERROR("DWARF: Cannot read %d bytes at %I64x.\n",
                            Size,
                            CurrentLocation->Value.Address);

                goto ReadDataSymbolEnd;
            }

            snprintf(PieceLocation,
                     sizeof(PieceLocation),
                     "[0x%llx]",
                     CurrentLocation->Value.Address);

            break;

        case DwarfLocationRegister:
            if (Size > Complex->Unit->AddressSize) {
                Size = Complex->Unit->AddressSize;
            }

            Status = DwarfTargetReadRegister(Context,
                                             CurrentLocation->Value.Register,
                                             &Value);

            if (Status != 0) {
                DWARF_ERROR("DWARF: Failed to get register %d.\n",
                            CurrentLocation->Value.Register);

                goto ReadDataSymbolEnd;
            }

            memcpy(Data, &Value, Size);
            snprintf(
                PieceLocation,
                sizeof(PieceLocation),
                "@%s",
                DwarfGetRegisterName(Context, CurrentLocation->Value.Register));

            break;

        case DwarfLocationKnownValue:
            Value = CurrentLocation->Value.Value;
            if (Size > sizeof(ULONGLONG)) {
                Size = sizeof(ULONGLONG);
            }

            memcpy(Data, &Value, Size);
            strncpy(PieceLocation, "<const>", sizeof(PieceLocation));
            break;

        case DwarfLocationKnownData:
            if (Size > CurrentLocation->Value.Buffer.Size) {
                Size = CurrentLocation->Value.Buffer.Size;
            }

            memcpy(Data, CurrentLocation->Value.Buffer.Data, Size);
            strncpy(PieceLocation, "<const>", sizeof(PieceLocation));
            break;

        case DwarfLocationUndefined:
            strncpy(PieceLocation, "<undef>", sizeof(PieceLocation));
            break;

        default:

            assert(FALSE);

            Status = EINVAL;
            goto ReadDataSymbolEnd;
        }

        //
        // Shift the buffer over if needed. Again, this doesn't cut it for bit
        // fields.
        //

        if (CurrentLocation->BitOffset != 0) {
            memmove(Data,
                    Data + (CurrentLocation->BitOffset / BITS_PER_BYTE),
                    Size);
        }

        if (LocationSize > 1) {
            if ((CurrentLocation->BitOffset != 0) ||
                (CurrentLocation->BitSize != 0)) {

                MaxBit = CurrentLocation->BitOffset + CurrentLocation->BitSize;
                Printed = snprintf(Location,
                                   LocationSize,
                                   "%s%s[%d:%d]",
                                   Comma,
                                   PieceLocation,
                                   MaxBit,
                                   CurrentLocation->BitOffset);

            } else {
                Printed = snprintf(Location,
                                   LocationSize,
                                   "%s%s",
                                   Comma,
                                   PieceLocation);
            }

            if (Printed > 0) {
                Location += Printed;
                LocationSize -= Printed;
            }
        }

        Comma = ",";
        Data += Size;
        DataSize -= Size;
        CurrentLocation = CurrentLocation->NextPiece;
    }

    if (LocationSize != 0) {
        *Location = '\0';
    }

ReadDataSymbolEnd:
    DwarfpDestroyLocationContext(Context, &LocationContext);
    return Status;
}

INT
DwarfGetAddressOfDataSymbol (
    PDEBUG_SYMBOLS Symbols,
    PDATA_SYMBOL Symbol,
    ULONGLONG DebasedPc,
    PULONGLONG Address
    )

/*++

Routine Description:

    This routine gets the memory address of a data symbol.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    Symbol - Supplies a pointer to the data symbol to read.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus the
        loaded base difference of the module).

    Address - Supplies a pointer where the address of the data symbol will be
        returned on success.

Return Value:

    0 on success.

    ENOENT if the data symbol is not currently valid.

    ERANGE if the data symbol is not stored in memory.

    Other error codes on other failures.

--*/

{

    PDWARF_COMPLEX_DATA_SYMBOL Complex;
    PDWARF_CONTEXT Context;
    PDWARF_LOCATION CurrentLocation;
    DWARF_LOCATION_CONTEXT LocationContext;
    INT Status;

    Context = Symbols->SymbolContext;

    assert(Symbol->LocationType == DataLocationComplex);

    Complex = Symbol->Location.Complex;
    memset(&LocationContext, 0, sizeof(DWARF_LOCATION_CONTEXT));
    LocationContext.Unit = Complex->Unit;
    LocationContext.CurrentFunction = Symbol->ParentFunction;
    LocationContext.Pc = DebasedPc;
    Status = DwarfpGetLocation(Context,
                               &LocationContext,
                               &(Complex->LocationAttribute));

    if (Status != 0) {
        if (Status != ENOENT) {
            DWARF_ERROR("DWARF: Failed to get location for symbol %s: %s.\n",
                        Symbol->Name,
                        strerror(Status));
        }

        goto GetAddressOfDataSymbolEnd;
    }

    CurrentLocation = &(LocationContext.Location);
    switch (CurrentLocation->Form) {
    case DwarfLocationMemory:
        *Address = CurrentLocation->Value.Address;
        Status = 0;
        break;

    default:
        Status = ERANGE;
        break;
    }

GetAddressOfDataSymbolEnd:
    DwarfpDestroyLocationContext(Context, &LocationContext);
    return Status;
}

BOOL
DwarfpCheckRange (
    PDEBUG_SYMBOLS Symbols,
    PSOURCE_FILE_SYMBOL Source,
    ULONGLONG Address,
    PVOID Ranges
    )

/*++

Routine Description:

    This routine determines whether the given address is actually in range of
    the given ranges. This is used for things like inline functions that have
    several discontiguous address ranges.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    Source - Supplies a pointer to the compilation unit the given object is in.

    Address - Supplies the address to query.

    Ranges - Supplies the opaque pointer to the range list information.

Return Value:

    TRUE if the address is within the range list for the object.

    FALSE if the address is not within the range list for the object.

--*/

{

    ULONGLONG Base;
    PUCHAR Bytes;
    BOOL Is64Bit;
    ULONGLONG RangeEnd;
    ULONGLONG RangeStart;
    PDWARF_COMPILATION_UNIT Unit;

    Bytes = Ranges;
    Unit = Source->SymbolContext;
    Is64Bit = Unit->Is64Bit;
    Base = Unit->LowPc;
    while (TRUE) {
        RangeStart = DWARF_READN(&Bytes, Is64Bit);
        RangeEnd = DWARF_READN(&Bytes, Is64Bit);
        if ((RangeStart == 0) && (RangeEnd == 0)) {
            break;
        }

        //
        // If the first value is the max address, then the second value is a
        // new base.
        //

        if (((Is64Bit != FALSE) && (RangeStart == MAX_ULONGLONG)) ||
            ((Is64Bit == FALSE) && (RangeStart == MAX_ULONG))) {

            Base = RangeEnd;
            continue;
        }

        if ((Address >= RangeStart + Base) && (Address < RangeEnd + Base)) {
            return TRUE;
        }
    }

    return FALSE;
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
    PSTR Potential;
    BOOL PotentialDirectory;
    PSTR Search;
    BOOL SearchDirectory;

    CurrentEntry = Context->SourcesHead->Next;
    while (CurrentEntry != Context->SourcesHead) {
        File = LIST_VALUE(CurrentEntry, SOURCE_FILE_SYMBOL, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Check the concatenation of the directory and the file.
        //

        Potential = File->SourceDirectory;
        PotentialDirectory = TRUE;
        if (Potential == NULL) {
            Potential = File->SourceFile;
            PotentialDirectory = FALSE;
        }

        Search = Directory;
        SearchDirectory = TRUE;
        if (Search == NULL) {
            Search = FileName;
            SearchDirectory = FALSE;
        }

        while (TRUE) {

            //
            // If it's the end of the line for both, then it's a match.
            //

            if ((*Search == '\0') && (*Potential == '\0') &&
                (SearchDirectory == FALSE) && (PotentialDirectory == FALSE)) {

                return File;
            }

            if ((*Search == '\0') && (SearchDirectory != FALSE)) {
                if ((*Potential == '/') || (*Potential == '\\')) {
                    Potential += 1;
                }

                Search = FileName;
                SearchDirectory = FALSE;
            }

            if ((*Potential == '\0') && (PotentialDirectory != FALSE)) {
                if ((*Search == '/') || (*Search == '\\')) {
                    Search += 1;
                }

                Potential = File->SourceFile;
                PotentialDirectory = FALSE;
            }

            if (*Search != *Potential) {
                break;
            }

            Search += 1;
            Potential += 1;
        }
    }

    if (Create == FALSE) {
        return NULL;
    }

    return DwarfpCreateSource(Context, Directory, FileName);
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
    case DwarfTagInlinedSubroutine:
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
    PDWARF_COMPILATION_UNIT Unit;

    LoadingContext = Context->LoadingContext;
    Unit = LoadingContext->CurrentUnit;
    SourceFile = DwarfpCreateSource(
                        Context,
                        DwarfpGetStringAttribute(Context, Die, DwarfAtCompDir),
                        DwarfpGetStringAttribute(Context, Die, DwarfAtName));

    if (SourceFile == NULL) {
        return ENOMEM;
    }

    SourceFile->Identifier = DWARF_DIE_ID(Context, Die);
    SourceFile->SymbolContext = Unit;

    //
    // Get the starting PC for the compilation unit. There might not be one
    // if this compilation unit has no code (only data).
    //

    Result = DwarfpGetAddressAttribute(Context,
                                       Die,
                                       DwarfAtLowPc,
                                       &(Unit->LowPc));

    if (Result != FALSE) {
        SourceFile->StartAddress = Unit->LowPc;
        Unit->HighPc = Unit->LowPc + 1;
        Result = DwarfpGetAddressAttribute(Context,
                                           Die,
                                           DwarfAtHighPc,
                                           &(Unit->HighPc));

        if (Result == FALSE) {

            //
            // DWARF4 also allows constant forms for high PC, in which case
            // it's an offset from low PC.
            //

            Result = DwarfpGetIntegerAttribute(Context,
                                               Die,
                                               DwarfAtHighPc,
                                               &(Unit->HighPc));

            if (Result != FALSE) {
                Unit->HighPc += Unit->LowPc;
            }
        }

        SourceFile->EndAddress = Unit->HighPc;
    }

    Unit->Ranges = DwarfpGetRangeList(Context, Die, DwarfAtRanges);
    if (Unit->Ranges != NULL) {
        DwarfpGetRangeSpan(Context,
                           Unit->Ranges,
                           Unit,
                           &(SourceFile->StartAddress),
                           &(SourceFile->EndAddress));
    }

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
    Result = DwarfpGetIntegerAttribute(Context,
                                       Die,
                                       DwarfAtEncoding,
                                       &Encoding);

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
            break;
        }

    } else {
        DWARF_ERROR("DWARF: Failed to get base type attribute.\n");
        return 0;
    }

    Result = DwarfpGetIntegerAttribute(Context, Die, DwarfAtByteSize, &Size);
    if (Result != FALSE) {
        Size *= BITS_PER_BYTE;

    } else {
        Result = DwarfpGetIntegerAttribute(Context, Die, DwarfAtBitSize, &Size);
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
    Type->Name = DwarfpGetStringAttribute(Context, Die, DwarfAtName);
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
        Relation.Pointer = LoadingContext->CurrentUnit->AddressSize;
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
    Type->Name = DwarfpGetStringAttribute(Context, Die, DwarfAtName);
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

    Result = DwarfpGetIntegerAttribute(Context,
                                       Die,
                                       DwarfAtUpperBound,
                                       &UpperBound);

    if (Result == FALSE) {
        LoadingContext->CurrentType->U.Relation.Pointer =
                                      LoadingContext->CurrentUnit->AddressSize;

        return 0;
    }

    if (LoadingContext->CurrentType != NULL) {
        if (LoadingContext->CurrentType->Type != DataTypeRelation) {
            DWARF_ERROR("DWARF: Subrange type on a non-relation data type.\n");
            return EINVAL;
        }

        LoadingContext->CurrentType->U.Relation.Array.Maximum = UpperBound;
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

    Result = DwarfpGetIntegerAttribute(Context, Die, DwarfAtByteSize, &Size);
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
    Type->Name = DwarfpGetStringAttribute(Context, Die, DwarfAtName);
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

    Result = DwarfpGetIntegerAttribute(Context, Die, DwarfAtBitSize, &BitSize);
    if (Result == FALSE) {
        Result = DwarfpGetIntegerAttribute(Context,
                                           Die,
                                           DwarfAtByteSize,
                                           &BitSize);

        if (Result != FALSE) {
            BitSize *= BITS_PER_BYTE;
        }
    }

    //
    // Get the bit offset. Try for a data bit offset, and fall back to the
    // older bit offset if not found.
    //

    Result = DwarfpGetIntegerAttribute(Context,
                                       Die,
                                       DwarfAtDataBitOffset,
                                       &BitOffset);

    if (Result == FALSE) {
        Result = DwarfpGetIntegerAttribute(Context,
                                           Die,
                                           DwarfAtBitOffset,
                                           &BitOffset);

        if (Result != FALSE) {

            //
            // If there's a bit offset and a bit size, there needs to be a byte
            // size to determine storage unit size.
            //

            Result = DwarfpGetIntegerAttribute(Context,
                                               Die,
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

    LocationAttribute = DwarfpGetAttribute(Context,
                                           Die,
                                           DwarfAtDataMemberLocation);

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
    Member->Name = DwarfpGetStringAttribute(Context, Die, DwarfAtName);
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
    Result = DwarfpGetIntegerAttribute(Context, Die, DwarfAtConstValue, &Value);
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
    Enumeration->Name = DwarfpGetStringAttribute(Context, Die, DwarfAtName);
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
    Type->Name = DwarfpGetStringAttribute(Context, Die, DwarfAtName);
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

    PDWARF_DIE Abstract;
    ULONG AllocationSize;
    ULONGLONG Declaration;
    PDWARF_FUNCTION_SYMBOL DwarfFunction;
    PDWARF_ATTRIBUTE_VALUE FrameBase;
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
    DwarfpGetIntegerAttribute(Context, Die, DwarfAtDeclaration, &Declaration);
    if (Declaration != FALSE) {
        return 0;
    }

    //
    // Ignore abstract inline functions. They'll be created later with their
    // instantiations.
    //

    Result = DwarfpGetIntegerAttribute(Context,
                                       Die,
                                       DwarfAtInline,
                                       &Declaration);

    if (Result != FALSE) {
        return 0;
    }

    //
    // If this is an inlined instance, go get its abstract origin to flesh out
    // the information.
    //

    Abstract = DwarfpGetDieReferenceAttribute(Context,
                                              Die,
                                              DwarfAtAbstractOrigin);

    AllocationSize = sizeof(FUNCTION_SYMBOL) + sizeof(DWARF_FUNCTION_SYMBOL);
    Function = malloc(AllocationSize);
    if (Function == NULL) {
        return ENOMEM;
    }

    memset(Function, 0, AllocationSize);
    DwarfFunction = (PDWARF_FUNCTION_SYMBOL)(Function + 1);
    Function->SymbolContext = DwarfFunction;
    DwarfFunction->Unit = LoadingContext->CurrentUnit;
    INITIALIZE_LIST_HEAD(&(Function->ParametersHead));
    INITIALIZE_LIST_HEAD(&(Function->LocalsHead));
    INITIALIZE_LIST_HEAD(&(Function->FunctionsHead));
    Function->ParentSource = LoadingContext->CurrentFile;
    Result = DwarfpGetTypeReferenceAttribute(Context,
                                             Die,
                                             DwarfAtType,
                                             &(Function->ReturnTypeOwner),
                                             &(Function->ReturnTypeNumber));

    if ((Result == FALSE) && (Abstract != NULL)) {
        Result = DwarfpGetTypeReferenceAttribute(Context,
                                                 Abstract,
                                                 DwarfAtType,
                                                 &(Function->ReturnTypeOwner),
                                                 &(Function->ReturnTypeNumber));
    }

    if (Result == FALSE) {
        free(Function);
        DWARF_ERROR("DWARF: Failed to get return type.\n");
        return EINVAL;
    }

    PreviousFunction = LoadingContext->CurrentFunction;
    LoadingContext->CurrentFunction = Function;
    Function->Name = DwarfpGetStringAttribute(Context, Die, DwarfAtName);
    if ((Function->Name == NULL) && (Abstract != NULL)) {
        Function->Name = DwarfpGetStringAttribute(Context,
                                                  Abstract,
                                                  DwarfAtName);
    }

    //
    // Get the function bounds, which is a low/high PC or a set of ranges.
    // There's no need to check the abstract origin since function locations
    // are always a concrete thing.
    //

    Result = DwarfpGetAddressAttribute(Context,
                                       Die,
                                       DwarfAtLowPc,
                                       &(Function->StartAddress));

    if (Result != FALSE) {
        Result = DwarfpGetAddressAttribute(Context,
                                           Die,
                                           DwarfAtHighPc,
                                           &(Function->EndAddress));

        if (Result == FALSE) {

            //
            // DWARF4 also allows constant forms for high PC, in which case
            // it's an offset from low PC.
            //

            Result = DwarfpGetIntegerAttribute(Context,
                                               Die,
                                               DwarfAtHighPc,
                                               &(Function->EndAddress));

            if (Result != FALSE) {
                Function->EndAddress += Function->StartAddress;
            }
        }
    }

    Function->Ranges = DwarfpGetRangeList(Context, Die, DwarfAtRanges);
    if (Function->Ranges != NULL) {
        DwarfpGetRangeSpan(Context,
                           Function->Ranges,
                           DwarfFunction->Unit,
                           &(Function->StartAddress),
                           &(Function->EndAddress));
    }

    if ((Function->EndAddress < Function->StartAddress) &&
        (Function->StartAddress != 0)) {

        Function->EndAddress = Function->StartAddress + 1;
    }

    FrameBase = DwarfpGetAttribute(Context, Die, DwarfAtFrameBase);
    if (FrameBase != NULL) {
        memcpy(&(DwarfFunction->FrameBase),
               FrameBase,
               sizeof(DWARF_ATTRIBUTE_VALUE));
    }

    if (PreviousFunction != NULL) {
        INSERT_BEFORE(&(Function->ListEntry),
                      &(PreviousFunction->FunctionsHead));

        Function->ParentFunction = PreviousFunction;

    } else {
        INSERT_BEFORE(&(Function->ListEntry),
                      &(LoadingContext->CurrentFile->FunctionsHead));
    }

    if (Abstract != NULL) {
        Status = DwarfpProcessChildDies(Context, Abstract);
        if (Status != 0) {
            DWARF_ERROR("DWARF: Failed to process abstract child dies.\n");
        }
    }

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
    Location = DwarfpGetAttribute(Context, Die, DwarfAtLocation);

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

    Variable->Name = DwarfpGetStringAttribute(Context, Die, DwarfAtName);
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

PSOURCE_FILE_SYMBOL
DwarfpCreateSource (
    PDWARF_CONTEXT Context,
    PSTR Directory,
    PSTR FileName
    )

/*++

Routine Description:

    This routine creates a new source file symbol.

Arguments:

    Context - Supplies a pointer to the application context.

    Directory - Supplies a pointer to the source directory.

    FileName - Supplies a pointer to the source file name.

Return Value:

    Returns a pointer to a source file symbol on success.

    NULL if no such file exists.

--*/

{

    PSOURCE_FILE_SYMBOL File;

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

VOID
DwarfpDestroyFunction (
    PFUNCTION_SYMBOL Function
    )

/*++

Routine Description:

    This routine destroys a function symbol.

Arguments:

    Function - Supplies a pointer to the function to destroy.

Return Value:

    None.

--*/

{

    PDATA_SYMBOL DataSymbol;
    PFUNCTION_SYMBOL SubFunction;

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

    while (!LIST_EMPTY(&(Function->FunctionsHead))) {
        SubFunction = LIST_VALUE(Function->FunctionsHead.Next,
                                 FUNCTION_SYMBOL,
                                 ListEntry);

        DwarfpDestroyFunction(SubFunction);
    }

    LIST_REMOVE(&(Function->ListEntry));
    free(Function);
    return;
}

