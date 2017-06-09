/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    symbols.c

Abstract:

    This module implements symbol translation helper routines used by the
    debugger.

Author:

    Evan Green 2-Jul-2012

Environment:

    Debugger client

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
#include "dwarf.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MEMBER_NAME_SPACE 17

#define MAX_RELATION_TYPE_DEPTH 50

//
// ----------------------------------------------- Internal Function Prototypes
//

PFUNCTION_SYMBOL
DbgpMatchFunctionAddress (
    PDEBUG_SYMBOLS Module,
    ULONGLONG Address,
    PFUNCTION_SYMBOL Function
    );

BOOL
DbgpStringMatch (
    PSTR Query,
    PSTR PossibleMatch
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the set of known symbol libraries.
//

PSYMBOLS_LOAD DbgSymbolLoaders[] = {
    DwarfLoadSymbols,
    DbgpStabsLoadSymbols,
    DbgpElfLoadSymbols,
    DbgpCoffLoadSymbols,
    NULL
};

//
// Define a default void type that has a source file of NULL (this is unique)
// and a type number of -1.
//

TYPE_SYMBOL DbgVoidType = {
    {NULL, NULL},
    NULL,
    -1,
    "void",
    NULL,
    DataTypeNumeric,
    {
        {
            FALSE,
            NULL,
            -1
        }
    }
};

//
// Define the machine register names.
//

PSTR DbgX86RegisterSymbolNames[] = {
    "eax",
    "ecx",
    "edx",
    "ebx",
    "esp",
    "ebp",
    "esi",
    "edi",
    "eip",
    "eflags",
    "cs",
    "ss",
    "ds",
    "es",
    "fs",
    "gs",
    "st0",
    "st1",
    "st2",
    "st3",
    "st4",
    "st5",
    "st6",
    "st7",
    "xmm0",
    "xmm1",
    "xmm2",
    "xmm3",
    "xmm4",
    "xmm5",
    "xmm6",
    "xmm7",
};

PSTR DbgX64RegisterSymbolNames[] = {
    "rax",
    "rdx",
    "rcx",
    "rbx",
    "rsi",
    "rdi",
    "rbp",
    "rsp",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
    "r15",
    "rip",
    "xmm0",
    "xmm1",
    "xmm2",
    "xmm3",
    "xmm4",
    "xmm5",
    "xmm6",
    "xmm7",
    "xmm8",
    "xmm9",
    "xmm10",
    "xmm11",
    "xmm12",
    "xmm13",
    "xmm14",
    "xmm15",
    "st0",
    "st1",
    "st2",
    "st3",
    "st4",
    "st5",
    "st6",
    "st7",
    "mm0",
    "mm1",
    "mm2",
    "mm3",
    "mm4",
    "mm5",
    "mm6",
    "mm7",
    "rflags",
    "es",
    "cs",
    "ss",
    "ds",
    "fs",
    "gs",
    NULL,
    NULL,
    "fsbase",
    "gsbase",
    NULL,
    NULL,
    "tr",
    "ldtr",
    "mxcsr",
    "fcw",
    "fsw",
    "xmm16",
    "xmm17",
    "xmm18",
    "xmm19",
    "xmm20",
    "xmm21",
    "xmm22",
    "xmm23",
    "xmm24",
    "xmm25",
    "xmm26",
    "xmm27",
    "xmm28",
    "xmm29",
    "xmm30",
    "xmm31"
};

PSTR DbgArmRegisterSymbolNames[] = {
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "sp",
    "lr",
    "pc",
    "f0"
    "f1",
    "f2",
    "f3",
    "f4",
    "f5",
    "f6",
    "f7",
    "fps",
    "cpsr"
};

PSTR DbgArmVfpRegisterSymbolNames[] = {
    "d0",
    "d1",
    "d2",
    "d3",
    "d4",
    "d5",
    "d6",
    "d7",
    "d8",
    "d9",
    "d10",
    "d11",
    "d12",
    "d13",
    "d14",
    "d15",
    "d16",
    "d17",
    "d18",
    "d19",
    "d20",
    "d21",
    "d22",
    "d23",
    "d24",
    "d25",
    "d26",
    "d27",
    "d28",
    "d29",
    "d30",
    "d31",
};

//
// ------------------------------------------------------------------ Functions
//

INT
DbgLoadSymbols (
    PSTR Filename,
    IMAGE_MACHINE_TYPE MachineType,
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

    HostContext - Supplies the value to store in the host context field of the
        debug symbols.

    Symbols - Supplies an optional pointer where a pointer to the symbols will
        be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSYMBOLS_LOAD *LoadFunction;
    struct stat Stat;
    INT Status;

    //
    // Don't go through the whole process if the file isn't even there.
    //

    if (stat(Filename, &Stat) != 0) {
        return errno;
    }

    LoadFunction = &(DbgSymbolLoaders[0]);
    Status = ENOSYS;
    while (*LoadFunction != NULL) {
        Status = (*LoadFunction)(Filename,
                                 MachineType,
                                 0,
                                 HostContext,
                                 Symbols);

        if (Status == 0) {
            break;
        }

        LoadFunction += 1;
    }

    return Status;
}

VOID
DbgUnloadSymbols (
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

    Symbols->Interface->Unload(Symbols);
    return;
}

VOID
DbgPrintFunctionPrototype (
    PFUNCTION_SYMBOL Function,
    PSTR ModuleName,
    ULONGLONG Address
    )

/*++

Routine Description:

    This routine prints a C function prototype directly to the screen.

Arguments:

    Function - Supplies a pointer to the function symbol to print.

    ModuleName - Supplies an optional string containing the module name.

    Address - Supplies the final address of the function.

Return Value:

    None (information is printed directly to the standard output).

--*/

{

    PDATA_SYMBOL CurrentParameter;
    PTYPE_SYMBOL CurrentParameterType;
    BOOL FirstParameter;
    PLIST_ENTRY ParameterEntry;
    PTYPE_SYMBOL ReturnType;

    if (Function == NULL) {
        return;
    }

    ReturnType = DbgGetType(Function->ReturnTypeOwner,
                            Function->ReturnTypeNumber);

    DbgPrintTypeName(ReturnType);
    if (ModuleName != NULL) {
        DbgOut(" %s!%s (", ModuleName, Function->Name);

    } else {
        DbgOut(" %s (", Function->Name);
    }

    ParameterEntry = Function->ParametersHead.Next;
    FirstParameter = TRUE;
    while (ParameterEntry != &(Function->ParametersHead)) {
        CurrentParameter = LIST_VALUE(ParameterEntry,
                                      DATA_SYMBOL,
                                      ListEntry);

        if (FirstParameter == FALSE) {
            DbgOut(", ");
        }

        CurrentParameterType = DbgGetType(CurrentParameter->TypeOwner,
                                          CurrentParameter->TypeNumber);

        if (CurrentParameterType == NULL) {
            DbgOut("UNKNOWN_TYPE");

        } else {
            DbgPrintTypeName(CurrentParameterType);
        }

        DbgOut(" %s", CurrentParameter->Name);
        FirstParameter = FALSE;
        ParameterEntry = ParameterEntry->Next;
    }

    DbgOut("); 0x%I64x", Address);
    return;
}

VOID
DbgPrintTypeName (
    PTYPE_SYMBOL Type
    )

/*++

Routine Description:

    This routine prints a type name, formatted with any array an pointer
    decorations.

Arguments:

    Type - Supplies a pointer to the type to print information about.

Return Value:

    None (information is printed directly to the standard output).

--*/

{

    PDATA_TYPE_RELATION RelationData;
    PTYPE_SYMBOL Relative;

    switch (Type->Type) {
    case DataTypeStructure:
        if (Type->Name == NULL) {
            DbgOut("struct (anon)");

        } else {
            DbgOut("struct %s", Type->Name);
        }

        break;

    case DataTypeEnumeration:
        if ((Type->Name == NULL) || (*(Type->Name) == '\0') ||
            (strcmp(Type->Name, " ") == 0)) {

            DbgOut("(unnamed enum)");

        } else {
            DbgOut(Type->Name);
        }

        break;

    case DataTypeNumeric:
        if ((Type->Name == NULL) || (*(Type->Name) == '\0') ||
            (strcmp(Type->Name, " ") == 0)) {

            DbgOut("(unnamed numeric)");

        } else {
            DbgOut(Type->Name);
        }

        break;

    case DataTypeRelation:
        if ((Type->Name == NULL) || (*(Type->Name) == '\0')) {
            RelationData = &(Type->U.Relation);
            Relative = DbgGetType(RelationData->OwningFile,
                                  RelationData->TypeNumber);

            DbgPrintTypeName(Relative);
            if (RelationData->Array.Minimum != RelationData->Array.Maximum) {

                assert(RelationData->Array.Maximum >
                       RelationData->Array.Minimum);

                if (RelationData->Array.Minimum != 0) {
                    DbgOut("[%I64d:%I64d]",
                           RelationData->Array.Minimum,
                           RelationData->Array.Maximum + 1);

                } else {
                    DbgOut("[%I64d]", RelationData->Array.Maximum + 1);
                }
            }

            if (RelationData->Pointer != 0) {
                DbgOut("*");
            }

        } else {
            DbgOut(Type->Name);
        }

        break;

    case DataTypeFunctionPointer:
        DbgOut("(Function pointer)");
        break;

    default:

        assert(FALSE);

        return;
    }

    return;
}

ULONG
DbgGetTypeSize (
    PTYPE_SYMBOL Type,
    ULONG RecursionDepth
    )

/*++

Routine Description:

    This routine determines the size in bytes of a given type.

Arguments:

    Type - Supplies a pointer to the type to get the size of.

    RecursionDepth - Supplies the function recursion depth. Supply zero here.

Return Value:

    Returns the size of the type in bytes. On error or on querying a void type,
    0 is returned.

--*/

{

    ULONGLONG ArraySize;
    PDATA_TYPE_NUMERIC NumericData;
    PDATA_TYPE_RELATION RelationData;
    PTYPE_SYMBOL Relative;
    PDATA_TYPE_STRUCTURE StructureData;

    if (Type == NULL) {
        return 0;
    }

    switch (Type->Type) {
    case DataTypeEnumeration:
        return Type->U.Enumeration.SizeInBytes;

    case DataTypeNumeric:

        //
        // For a numeric type, return the size rounded up to the nearest byte.
        //

        NumericData = &(Type->U.Numeric);
        return (NumericData->BitSize + BITS_PER_BYTE - 1) / BITS_PER_BYTE;

    case DataTypeStructure:
        StructureData = &(Type->U.Structure);
        return StructureData->SizeInBytes;

    case DataTypeRelation:
        RelationData = &(Type->U.Relation);
        Relative = DbgGetType(RelationData->OwningFile,
                              RelationData->TypeNumber);

        ArraySize = 1;
        if (Relative == NULL) {

            assert(Relative != NULL);

            return 0;
        }

        if (RecursionDepth >= MAX_RELATION_TYPE_DEPTH) {
            DbgOut("Infinite recursion of type %s (%s, %d) to %s (%s, %d) "
                   "...\n",
                   Type->Name,
                   Type->ParentSource->SourceFile,
                   Type->TypeNumber,
                   Relative->Name,
                   Relative->ParentSource->SourceFile,
                   Relative->TypeNumber);

            return 0;
        }

        //
        // If it is an array, all subsequent values must be multiplied by
        // the array length.
        //

        if (RelationData->Array.Minimum != RelationData->Array.Maximum) {

            assert(RelationData->Array.Maximum >
                   RelationData->Array.Minimum);

            ArraySize = (RelationData->Array.Maximum + 1 -
                         RelationData->Array.Minimum);
        }

        //
        // If in the end the relation is a pointer, then the data is only
        // as big as that pointer (or an array of them).
        //

        if (RelationData->Pointer != 0) {
            return ArraySize * RelationData->Pointer;
        }

        //
        // If its relation is itself, stop now.
        //

        if (Relative == Type) {
            return 0;
        }

        //
        // Recurse to get the size of the underlying type.
        //

        return ArraySize * DbgGetTypeSize(Relative, RecursionDepth + 1);

    case DataTypeFunctionPointer:
        return Type->U.FunctionPointer.SizeInBytes;

    default:
        return 0;
    }

    //
    // Execution should never get here.
    //

    assert(FALSE);

    return 0;
}

VOID
DbgPrintTypeDescription (
    PTYPE_SYMBOL Type,
    ULONG SpaceLevel,
    ULONG RecursionDepth
    )

/*++

Routine Description:

    This routine prints a description of the structure of a given type.

Arguments:

    Type - Supplies a pointer to the type to print information about.

    SpaceLevel - Supplies the number of spaces to print after every newline.
        Used for nesting types.

    RecursionDepth - Supplies how many times this should recurse on structure
        members. If 0, only the name of the type is printed.

Return Value:

    None (information is printed directly to the standard output).

--*/

{

    ULONG BitRemainder;
    ULONG Bytes;
    PDATA_TYPE_ENUMERATION EnumerationData;
    PENUMERATION_MEMBER EnumerationMember;
    PSTR MemberName;
    CHAR MemberNameBuffer[256];
    PTYPE_SYMBOL MemberType;
    PDATA_TYPE_NUMERIC NumericData;
    PDATA_TYPE_RELATION RelationData;
    PTYPE_SYMBOL RelativeType;
    PDATA_TYPE_STRUCTURE StructureData;
    PSTRUCTURE_MEMBER StructureMember;

    //
    // Print only the type name if the recursion depth has reached 0.
    //

    if (RecursionDepth == 0) {
        DbgPrintTypeName(Type);
        return;
    }

    switch (Type->Type) {
    case DataTypeNumeric:
        NumericData = &(Type->U.Numeric);
        if (NumericData->Float != FALSE) {
            DbgOut("%d bit floating point number.", NumericData->BitSize);

        } else if (NumericData->Signed == FALSE) {
            DbgOut("U");
        }

        DbgOut("Int%d", NumericData->BitSize);
        break;

    case DataTypeRelation:

        //
        // Get the type this relation refers to.
        //

        RelationData = &(Type->U.Relation);
        RelativeType = DbgGetType(RelationData->OwningFile,
                                  RelationData->TypeNumber);

        //
        // If it cannot be found, this is an error.
        //

        if (RelativeType == NULL) {
            DbgOut("DANGLING RELATION %s, %d\n",
                   RelationData->OwningFile->SourceFile,
                   RelationData->TypeNumber);

            assert(RelativeType != NULL);

            return;
        }

        //
        // If it's a reference to itself, it's a void.
        //

        if (RelativeType == Type) {
            DbgOut("void type.");

        //
        // If the type is neither a pointer nor an array, print the description
        // of this type. This recurses until we actually print the description
        // of something that's *not* a relation, hit an array, or hit a pointer.
        // Note that simply following relations does not count against the
        // recursion depth since these types merely equal each other. This is
        // why the recursion depth is not decreased.
        //

        } else if ((RelationData->Array.Minimum ==
                    RelationData->Array.Maximum) &&
                   (RelationData->Pointer == 0)) {

            DbgPrintTypeDescription(RelativeType,
                                    SpaceLevel,
                                    RecursionDepth - 1);

        //
        // If the relation is a pointer or an array, print out that information
        // and do not recurse.
        //

        } else {

            //
            // Print the pointer symbol if this type is a pointer to another
            // type.
            //

            if (RelationData->Pointer != 0) {
                DbgOut("*");
            }

            //
            // Print the type's name. If this type has no name, this function
            // will follow the reference to a type that does have a name.
            //

            DbgPrintTypeName(RelativeType);

            //
            // If the type is an array, print that information.
            //

            if (RelationData->Array.Minimum != RelationData->Array.Maximum) {
                DbgOut("[");
                if (RelationData->Array.Minimum != 0) {
                    DbgOut("%I64d:", RelationData->Array.Minimum);
                }

                DbgOut("%I64d]", RelationData->Array.Maximum + 1);
            }

        }

        break;

    case DataTypeEnumeration:
        SpaceLevel += 2;
        DbgOut("enum {\n");
        EnumerationData = &(Type->U.Enumeration);
        EnumerationMember = EnumerationData->FirstMember;
        while (EnumerationMember != NULL) {
            DbgOut("%*s", SpaceLevel, "");
            MemberName = EnumerationMember->Name;
            if (MemberName == NULL) {
                MemberName = "";
            }

            DbgOut("%-*s = %I64d\n",
                   MEMBER_NAME_SPACE,
                   MemberName,
                   EnumerationMember->Value);

            EnumerationMember = EnumerationMember->NextMember;
        }

        SpaceLevel -= 2;
        DbgOut("%*s", SpaceLevel, "");
        DbgOut("}");
        break;

    case DataTypeStructure:
        DbgOut("struct {\n");
        SpaceLevel += 2;
        StructureData = &(Type->U.Structure);
        StructureMember = StructureData->FirstMember;
        while (StructureMember != NULL) {
            Bytes = StructureMember->BitOffset / BITS_PER_BYTE;
            BitRemainder = StructureMember->BitOffset % BITS_PER_BYTE;
            DbgOut("%*s", SpaceLevel, "");
            MemberName = StructureMember->Name;
            if (MemberName == NULL) {
                MemberName = "";
            }

            if (BitRemainder != 0) {
                snprintf(MemberNameBuffer,
                         sizeof(MemberNameBuffer),
                         "%s:%d",
                         MemberName,
                         BitRemainder);

                MemberNameBuffer[sizeof(MemberNameBuffer) - 1] = '\0';
                MemberName = MemberNameBuffer;
            }

            DbgOut("+0x%03x  %-*s", Bytes, MEMBER_NAME_SPACE, MemberName);
            DbgOut(" : ");
            MemberType = DbgGetType(StructureMember->TypeFile,
                                    StructureMember->TypeNumber);

            if (MemberType == NULL) {
                DbgOut("DANGLING REFERENCE %s, %d\n",
                       StructureMember->TypeFile->SourceFile,
                       StructureMember->TypeNumber);

                assert(MemberType != NULL);

                StructureMember = StructureMember->NextMember;
                continue;
            }

            DbgPrintTypeDescription(MemberType, SpaceLevel, RecursionDepth - 1);
            DbgOut("\n");
            StructureMember = StructureMember->NextMember;
        }

        SpaceLevel -= 2;
        DbgOut("%*s}", SpaceLevel, "");
        if (SpaceLevel == 0) {
            DbgOut("\nType Size: %d Bytes.", StructureData->SizeInBytes);
        }

        break;

    case DataTypeFunctionPointer:
        DbgOut("(*)()");
        break;

    default:

        assert(FALSE);

        break;
    }
}

INT
DbgGetMemberOffset (
    PTYPE_SYMBOL StructureType,
    PSTR FieldName,
    PULONG FieldOffset,
    PULONG FieldSize
    )

/*++

Routine Description:

    This routine returns the given field's offset (in bits) within the
    given structure.

Arguments:

    StructureType - Supplies a pointer to a symbol structure type.

    FieldName - Supplies a string containing the name of the field whose offset
        will be returned.

    FieldOffset - Supplies a pointer that will receive the bit offset of the
        given field name within the given structure.

    FieldSize - Supplies a pointer that will receive the size of the field in
        bits.

Return Value:

    0 on success.

    ENOENT if no such field name exists.

    Other error codes on other errors.

--*/

{

    ULONG Index;
    INT Result;
    PDATA_TYPE_STRUCTURE StructureData;
    PSTRUCTURE_MEMBER StructureMember;

    //
    // Parameter checking.
    //

    if ((StructureType == NULL) ||
        (StructureType->Type != DataTypeStructure) ||
        (FieldOffset == NULL)) {

        return EINVAL;
    }

    //
    // Search for the field within the structure.
    //

    Result = ENOENT;
    StructureData = &(StructureType->U.Structure);
    StructureMember = StructureData->FirstMember;
    for (Index = 0; Index < StructureData->MemberCount; Index += 1) {
        if ((StructureMember->Name != NULL) &&
            (strcmp(FieldName, StructureMember->Name) == 0)) {

            if (FieldOffset != NULL) {
                *FieldOffset = StructureMember->BitOffset;
            }

            if (FieldSize != NULL) {
                *FieldSize = StructureMember->BitSize;
            }

            Result = 0;
            break;
        }

        StructureMember = StructureMember->NextMember;
    }

    if (Result != 0) {
        DbgOut("GetMemberOffset: %s has no member %s.\n",
               StructureType->Name,
               FieldName);
    }

    return Result;
}

PTYPE_SYMBOL
DbgSkipTypedefs (
    PTYPE_SYMBOL Type
    )

/*++

Routine Description:

    This routine skips all relation types that aren't pointers or arrays.

Arguments:

    Type - Supplies a pointer to the type to get to the bottom of.

Return Value:

    NULL if the type ended up being void or not found.

    Returns a pointer to the root type on success.

--*/

{

    ULONG MaxCount;
    PDATA_TYPE_RELATION Relation;
    PTYPE_SYMBOL RelativeType;

    if (Type->Type != DataTypeRelation) {
        return Type;
    }

    Relation = &(Type->U.Relation);

    //
    // Loop scanning through typedefs.
    //

    MaxCount = 50;
    while ((MaxCount != 0) &&
           (Type->Type == DataTypeRelation) &&
           (Relation->Array.Minimum == Relation->Array.Maximum) &&
           (Relation->Pointer == 0)) {

        RelativeType = DbgGetType(Relation->OwningFile, Relation->TypeNumber);
        if ((RelativeType == NULL) || (RelativeType == Type)) {
            return NULL;
        }

        Type = RelativeType;
        Relation = &(Type->U.Relation);
        MaxCount -= 1;
    }

    if (MaxCount == 0) {
        return NULL;
    }

    return Type;
}

PTYPE_SYMBOL
DbgGetType (
    PSOURCE_FILE_SYMBOL SourceFile,
    LONG TypeNumber
    )

/*++

Routine Description:

    This routine looks up a type symbol based on the type number and the source
    file the type is in.

Arguments:

    SourceFile - Supplies a pointer to the source file containing the type.

    TypeNumber - Supplies the type number to look up.

Return Value:

    Returns a pointer to the type on success, or NULL on error.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PTYPE_SYMBOL CurrentType;

    if (SourceFile == NULL) {

        assert(TypeNumber == -1);

        return &DbgVoidType;
    }

    CurrentEntry = SourceFile->TypesHead.Next;
    while (CurrentEntry != &(SourceFile->TypesHead)) {
        CurrentType = LIST_VALUE(CurrentEntry, TYPE_SYMBOL, ListEntry);
        if (CurrentType->TypeNumber == TypeNumber) {
            return CurrentType;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    DbgOut("Error: Failed to look up type %s:%x\n",
           SourceFile->SourceFile,
           TypeNumber);

    return NULL;
}

PSOURCE_LINE_SYMBOL
DbgLookupSourceLine (
    PDEBUG_SYMBOLS Module,
    ULONGLONG Address
    )

/*++

Routine Description:

    This routine looks up a source line in a given module based on the address.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Address - Supplies the query address to search the source line symbols for.

Return Value:

    If a successful match is found, returns a pointer to the source line symbol.
    If a source line matching the address could not be found or an error
    occured, returns NULL.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSOURCE_LINE_SYMBOL CurrentLine;
    PSOURCE_FILE_SYMBOL CurrentSource;
    PLIST_ENTRY CurrentSourceEntry;

    //
    // Parameter checking.
    //

    if (Module == NULL) {
        return NULL;
    }

    //
    // Begin searching. Loop over all source files in the module.
    //

    CurrentSourceEntry = Module->SourcesHead.Next;
    CurrentEntry = NULL;
    while (CurrentSourceEntry != &(Module->SourcesHead)) {
        CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                   SOURCE_FILE_SYMBOL,
                                   ListEntry);

        if (CurrentEntry == NULL) {
            CurrentEntry = CurrentSource->SourceLinesHead.Next;
        }

        //
        // Loop over every source line in the current source file.
        //

        while (CurrentEntry != &(CurrentSource->SourceLinesHead)) {
            CurrentLine = LIST_VALUE(CurrentEntry,
                                     SOURCE_LINE_SYMBOL,
                                     ListEntry);

            if ((Address >= CurrentLine->Start) &&
                (Address < CurrentLine->End)) {

                //
                // A match has been found!
                //

                return CurrentLine;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        CurrentEntry = NULL;
        CurrentSourceEntry = CurrentSourceEntry->Next;
    }

    return NULL;
}

PSYMBOL_SEARCH_RESULT
DbgLookupSymbol (
    PDEBUG_SYMBOLS Module,
    ULONGLONG Address,
    PSYMBOL_SEARCH_RESULT Input
    )

/*++

Routine Description:

    This routine looks up a symbol in a module based on the given address. It
    first searches through data symbols, then functions.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Address - Supplies the address of the symbol to look up.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

{

    //
    // Parameter checking.
    //

    if ((Module == NULL) || (Address == (INTN)NULL) || (Input == NULL)) {
        return NULL;
    }

    //
    // Start searching, depending on the input parameter. Note that fallthrough
    // *is* intended.
    //

    switch (Input->Variety) {
    case SymbolResultInvalid:
    case SymbolResultType:
    case SymbolResultData:
        if (DbgFindDataSymbol(Module, NULL, Address, Input) != NULL) {
            return Input;
        }

    case SymbolResultFunction:
        if (DbgFindFunctionSymbol(Module, NULL, Address, Input) != NULL) {
            return Input;
        }

    default:
        break;
    }

    return NULL;
}

PSYMBOL_SEARCH_RESULT
DbgpFindSymbolInModule (
    PDEBUG_SYMBOLS Module,
    PSTR Query,
    PSYMBOL_SEARCH_RESULT Input
    )

/*++

Routine Description:

    This routine searches for a symbol in a module. It first searches through
    types, then data symbols, then functions.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Query - Supplies the search string.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

{

    //
    // Parameter checking.
    //

    if ((Module == NULL) || (Query == NULL) || (Input == NULL)) {
        return NULL;
    }

    //
    // Start searching, depending on the input parameter. Note that fallthrough
    // *is* intended.
    //

    switch (Input->Variety) {
    case SymbolResultInvalid:
    case SymbolResultType:
        if (DbgFindTypeSymbol(Module, Query, Input) != NULL) {
            return Input;
        }

    case SymbolResultData:
        if (DbgFindDataSymbol(Module, Query, (INTN)NULL, Input) != NULL) {
            return Input;
        }

    case SymbolResultFunction:
        if (DbgFindFunctionSymbol(Module, Query, (INTN)NULL, Input) != NULL) {
            return Input;
        }

    default:
        break;
    }

    return NULL;
}

PSYMBOL_SEARCH_RESULT
DbgFindTypeSymbol (
    PDEBUG_SYMBOLS Module,
    PSTR Query,
    PSYMBOL_SEARCH_RESULT Input
    )

/*++

Routine Description:

    This routine searches for a type symbol in a module.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Query - Supplies the search string.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSOURCE_FILE_SYMBOL CurrentSource;
    PLIST_ENTRY CurrentSourceEntry;
    PTYPE_SYMBOL CurrentType;

    //
    // Parameter checking.
    //

    if ((Query == NULL) || (Module == NULL) || (Input == NULL)) {
        return NULL;
    }

    //
    // Initialize the search variables based on the input parameter.
    //

    CurrentEntry = NULL;
    if ((Input->Variety == SymbolResultType) && (Input->U.TypeResult != NULL)) {
        CurrentEntry = &(Input->U.TypeResult->ListEntry);
        CurrentType = LIST_VALUE(CurrentEntry, TYPE_SYMBOL, ListEntry);
        CurrentSource = CurrentType->ParentSource;
        CurrentSourceEntry = &(CurrentSource->ListEntry);
        CurrentEntry = CurrentEntry->Next;

    } else {
        CurrentSourceEntry = Module->SourcesHead.Next;
        CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                   SOURCE_FILE_SYMBOL,
                                   ListEntry);
    }

    //
    // Begin searching. Loop over all source files in the module.
    //

    while (CurrentSourceEntry != &(Module->SourcesHead)) {
        CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                   SOURCE_FILE_SYMBOL,
                                   ListEntry);

        if (CurrentEntry == NULL) {
            CurrentEntry = CurrentSource->TypesHead.Next;
        }

        //
        // Loop over every type in the current source file.
        //

        while (CurrentEntry != &(CurrentSource->TypesHead)) {
            CurrentType = LIST_VALUE(CurrentEntry,
                                     TYPE_SYMBOL,
                                     ListEntry);

            if (DbgpStringMatch(Query, CurrentType->Name) != FALSE) {

                //
                // A match has been found. Fill out the structure and return.
                //

                Input->Variety = SymbolResultType;
                Input->U.TypeResult = CurrentType;
                return Input;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        CurrentEntry = NULL;
        CurrentSourceEntry = CurrentSourceEntry->Next;
    }

    return NULL;
}

PSYMBOL_SEARCH_RESULT
DbgFindDataSymbol (
    PDEBUG_SYMBOLS Module,
    PSTR Query,
    ULONGLONG Address,
    PSYMBOL_SEARCH_RESULT Input
    )

/*++

Routine Description:

    This routine searches for a data symbol in a module based on a query string
    or address.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Query - Supplies the search string. This parameter can be NULL if searching
        by address.

    Address - Supplies the address of the symbol. Can be NULL if search by
        query string is desired.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

{

    PDATA_SYMBOL CurrentData;
    PLIST_ENTRY CurrentEntry;
    PSOURCE_FILE_SYMBOL CurrentSource;
    PLIST_ENTRY CurrentSourceEntry;

    //
    // Parameter checking.
    //

    if ((Module == NULL) || (Input == NULL)) {
        return NULL;
    }

    if ((Query == NULL) && (Address == (INTN)NULL)) {
        return NULL;
    }

    //
    // Initialize the search variables based on the input parameter.
    //

    CurrentEntry = NULL;
    if ((Input->Variety == SymbolResultData) && (Input->U.DataResult != NULL)) {
        CurrentEntry = &(Input->U.DataResult->ListEntry);
        CurrentData = LIST_VALUE(CurrentEntry, DATA_SYMBOL, ListEntry);
        CurrentSource = CurrentData->ParentSource;
        CurrentSourceEntry = &(CurrentSource->ListEntry);
        CurrentEntry = CurrentEntry->Next;

    } else {
        CurrentSourceEntry = Module->SourcesHead.Next;
        CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                   SOURCE_FILE_SYMBOL,
                                   ListEntry);
    }

    //
    // Begin searching. Loop over all source files in the module.
    //

    while (CurrentSourceEntry != &(Module->SourcesHead)) {
        CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                   SOURCE_FILE_SYMBOL,
                                   ListEntry);

        //
        // Set up the current symbol entry.
        //

        if (CurrentEntry == NULL) {
            CurrentEntry = CurrentSource->DataSymbolsHead.Next;
        }

        //
        // Loop over every data symbol in the current source file.
        //

        while (CurrentEntry != &(CurrentSource->DataSymbolsHead)) {
            CurrentData = LIST_VALUE(CurrentEntry,
                                     DATA_SYMBOL,
                                     ListEntry);

            //
            // Check for an address-based match. Only look at absolute address
            // based symbols (not stack offset or register variables).
            //

            if (Address != (INTN)NULL) {
                if ((CurrentData->LocationType ==
                     DataLocationAbsoluteAddress) &&
                    (CurrentData->Location.Address == Address)) {

                    Input->Variety = SymbolResultData;
                    Input->U.DataResult = CurrentData;
                    return Input;
                }

            } else {

                assert(Query != NULL);

                //
                // Check for a string-based match.
                //

                if (DbgpStringMatch(Query, CurrentData->Name) != FALSE) {
                    Input->Variety = SymbolResultData;
                    Input->U.DataResult = CurrentData;
                    return Input;
                }
            }

            CurrentEntry = CurrentEntry->Next;
        }

        CurrentEntry = NULL;
        CurrentSourceEntry = CurrentSourceEntry->Next;
    }

    return NULL;
}

PSYMBOL_SEARCH_RESULT
DbgFindFunctionSymbol (
    PDEBUG_SYMBOLS Module,
    PSTR Query,
    ULONGLONG Address,
    PSYMBOL_SEARCH_RESULT Input
    )

/*++

Routine Description:

    This routine searches for a function symbol in a module based on a search
    string or an address.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Query - Supplies the search string. This parameter can be NULL if searching
        by address.

    Address - Supplies the search address. This parameter can be NULL if
        searching by query string.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PFUNCTION_SYMBOL CurrentFunction;
    PSOURCE_FILE_SYMBOL CurrentSource;
    PLIST_ENTRY CurrentSourceEntry;

    //
    // Parameter checking.
    //

    if ((Module == NULL) || (Input == NULL)) {
        return NULL;
    }

    if ((Query == NULL) && (Address == (INTN)NULL)) {
        return NULL;
    }

    //
    // Initialize the search variables based on the input parameter.
    //

    CurrentEntry = NULL;
    if ((Input->Variety == SymbolResultFunction) &&
        (Input->U.FunctionResult != NULL)) {

        CurrentEntry = &(Input->U.FunctionResult->ListEntry);
        CurrentFunction = LIST_VALUE(CurrentEntry, FUNCTION_SYMBOL, ListEntry);
        CurrentSource = CurrentFunction->ParentSource;
        CurrentSourceEntry = &(CurrentSource->ListEntry);
        CurrentEntry = CurrentEntry->Next;

    } else {
        CurrentSourceEntry = Module->SourcesHead.Next;
        CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                   SOURCE_FILE_SYMBOL,
                                   ListEntry);
    }

    //
    // Begin searching. Loop over all source files in the module.
    //

    while (CurrentSourceEntry != &(Module->SourcesHead)) {
        CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                   SOURCE_FILE_SYMBOL,
                                   ListEntry);

        if (CurrentEntry == NULL) {
            CurrentEntry = CurrentSource->FunctionsHead.Next;
        }

        //
        // Loop over every function in the current source file.
        //

        while (CurrentEntry != &(CurrentSource->FunctionsHead)) {
            CurrentFunction = LIST_VALUE(CurrentEntry,
                                         FUNCTION_SYMBOL,
                                         ListEntry);

            //
            // For address based searching, determine if the function is within
            // range, and scoop out the deepest inline function if so.
            //

            if (Address != (INTN)NULL) {
                if ((Address >= CurrentFunction->StartAddress) &&
                    (Address < CurrentFunction->EndAddress)) {

                    CurrentFunction = DbgpMatchFunctionAddress(Module,
                                                               Address,
                                                               CurrentFunction);

                    if (CurrentFunction != FALSE) {
                        Input->Variety = SymbolResultFunction;
                        Input->U.FunctionResult = CurrentFunction;
                        return Input;
                    }
                }

            } else {

                //
                // Check for a string based match.
                //

                assert(Query != NULL);

                if (DbgpStringMatch(Query, CurrentFunction->Name) != FALSE) {
                    Input->Variety = SymbolResultFunction;
                    Input->U.FunctionResult = CurrentFunction;
                    return Input;
                }
            }

            CurrentEntry = CurrentEntry->Next;
        }

        CurrentEntry = NULL;
        CurrentSourceEntry = CurrentSourceEntry->Next;
    }

    return NULL;
}

PSTR
DbgGetRegisterName (
    IMAGE_MACHINE_TYPE MachineType,
    ULONG Register
    )

/*++

Routine Description:

    This routine returns a string containing the name of the given register.

Arguments:

    MachineType - Supplies the machine type.

    Register - Supplies the register number.

Return Value:

    Returns a pointer to a constant string containing the name of the register.

--*/

{

    ULONG Count;
    PSTR Name;

    Name = NULL;
    switch (MachineType) {
    case ImageMachineTypeX86:
        Count = sizeof(DbgX86RegisterSymbolNames) /
                sizeof(DbgX86RegisterSymbolNames[0]);

        if (Register < Count) {
            Name = DbgX86RegisterSymbolNames[Register];
        }

        break;

    case ImageMachineTypeX64:
        Count = sizeof(DbgX64RegisterSymbolNames) /
                sizeof(DbgX64RegisterSymbolNames[0]);

        if (Register < Count) {
            Name = DbgX64RegisterSymbolNames[Register];
        }

        break;

    case ImageMachineTypeArm32:
        Count = sizeof(DbgArmRegisterSymbolNames) /
                sizeof(DbgArmRegisterSymbolNames[0]);

        if (Register < Count) {
            Name = DbgArmRegisterSymbolNames[Register];
            break;
        }

        Count = sizeof(DbgArmVfpRegisterSymbolNames) /
                sizeof(DbgArmVfpRegisterSymbolNames[0]);

        if ((Register >= ArmRegisterD0) &&
            ((Register - ArmRegisterD0) < Count)) {

            Name = DbgArmVfpRegisterSymbolNames[Register - ArmRegisterD0];
        }

        break;

    default:
        break;
    }

    if (Name == NULL) {
        Name = "UNKNOWNREG";
    }

    return Name;
}

//
// --------------------------------------------------------- Internal Functions
//

PFUNCTION_SYMBOL
DbgpMatchFunctionAddress (
    PDEBUG_SYMBOLS Module,
    ULONGLONG Address,
    PFUNCTION_SYMBOL Function
    )

/*++

Routine Description:

    This routine determines the function corresponding to the given address.
    It may end up returning a nested (inlined) function.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Address - Supplies the search address.

    Function - Supplies the function to search within.

Return Value:

    Returns a pointer to the function containing the given address on success.

    NULL if the given function does not contain the given address, nor do any
    of its sub-functions.

--*/

{

    PSYMBOLS_CHECK_RANGE CheckRange;
    PLIST_ENTRY CurrentEntry;
    BOOL InRange;
    PFUNCTION_SYMBOL Subfunction;

    if ((Address < Function->StartAddress) ||
        (Address >= Function->EndAddress)) {

        return NULL;
    }

    if (Function->Ranges != NULL) {
        CheckRange = Module->Interface->CheckRange;
        InRange = CheckRange(Module,
                             Function->ParentSource,
                             Address,
                             Function->Ranges);

        if (InRange == FALSE) {
            return NULL;
        }
    }

    if (LIST_EMPTY(&(Function->FunctionsHead))) {
        return Function;
    }

    //
    // Traverse the inlined functions and see if any of them matches. Return
    // the deepest inline possible, or this function if none match.
    //

    CurrentEntry = Function->FunctionsHead.Next;
    while (CurrentEntry != &(Function->FunctionsHead)) {
        Subfunction = LIST_VALUE(CurrentEntry, FUNCTION_SYMBOL, ListEntry);
        Subfunction = DbgpMatchFunctionAddress(Module, Address, Subfunction);
        if (Subfunction != NULL) {
            return Subfunction;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // This function matches but none of the inner inlines do, so just return
    // this one.
    //

    return Function;
}

BOOL
DbgpStringMatch (
    PSTR Query,
    PSTR PossibleMatch
    )

/*++

Routine Description:

    This routine determines whether or not a string matches a query string. The
    query string may contain wildcard characters (*).

Arguments:

    Query - Supplies the query string. This string may contain wildcard
        characters (*) signifying zero or more arbitrary characters.

    PossibleMatch - Supplies the test string. Wildcard characters in this string
        will be treated as regular characters.

Return Value:

    Returns TRUE upon successful match. Returns FALSE if the strings do not
    match.

--*/

{

    PSTR CurrentMatch;
    PSTR CurrentQuery;
    BOOL InWildcard;
    UCHAR LowerMatch;
    UCHAR LowerQuery;

    InWildcard = FALSE;
    CurrentQuery = Query;
    CurrentMatch = PossibleMatch;
    if ((Query == NULL) || (PossibleMatch == NULL)) {
        return FALSE;
    }

    do {

        //
        // If the current query character is a wildcard, note that and advance
        // to the character after the wildcard.
        //

        if (*CurrentQuery == '*') {
            InWildcard = TRUE;
            CurrentQuery += 1;
        }

        //
        // If the entire query string has been processed, it's a match only if
        // the match string is finished as well, or a wildcard is being
        // processed.
        //

        if (*CurrentQuery == '\0') {
            if ((*CurrentMatch == '\0') || (InWildcard != FALSE)) {
                return TRUE;

            } else {
                return FALSE;
            }
        }

        //
        // If the match string ended, this must not be a match because the
        // query string hasn't ended. Whether or not search is inside a
        // wildcard is irrelevent because there must be match characters after
        // the wildcard that are not getting satisfied (if there weren't the
        // query string would be over.
        //

        if (*CurrentMatch == '\0') {
            return FALSE;
        }

        //
        // Convert to lowercase.
        //

        LowerQuery = *CurrentQuery;
        LowerMatch = *CurrentMatch;
        if ((LowerQuery >= 'A') && (LowerQuery <= 'Z')) {
            LowerQuery = LowerQuery - 'A' + 'a';
        }

        if ((LowerMatch >= 'A') && (LowerMatch <= 'Z')) {
            LowerMatch = LowerMatch - 'A' + 'a';
        }

        //
        // If the characters match, then either it's a normal match or a
        // character after the wildcard has been found. If it's the wildcard
        // case, attempt to match the rest of the string from here. If it does
        // not work, all is not lost, the correct match may be farther down the
        // string.
        //

        if (LowerQuery == LowerMatch) {
            if (InWildcard != FALSE) {
                if (DbgpStringMatch(CurrentQuery, CurrentMatch) != FALSE) {
                    return TRUE;

                } else {
                    CurrentMatch += 1;
                }

            } else {
                CurrentQuery += 1;
                CurrentMatch += 1;
            }

        //
        // If there's no match, but there's a wildcard being processed, advance
        // only the match string.
        //

        } else if (InWildcard != FALSE) {
            CurrentMatch += 1;

        //
        // It's not a match and there's no wildcard, the strings simply
        // disagree.
        //

        } else {
            return FALSE;
        }

    } while (TRUE);

    return FALSE;
}

