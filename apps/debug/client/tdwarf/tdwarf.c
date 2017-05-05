/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tdwarf.c

Abstract:

    This module implements the DWARF symbol parser test program.

Author:

    Evan Green 2-Dec-2015

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/im.h>
#include "../dwarfp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TDWARF_USAGE \
    "usage: tdwarf [options] [files...]\n"                                     \
    "Options are:\n"                                                           \
    "  -A, --all -- Print everything (except debug).\n"                        \
    "  -a, --arguments -- Print function parameters.\n"                        \
    "  -D, --debug -- Enable debugging in the symbol parser.\n"                \
    "  -f, --files -- Print parsed source file information.\n"                 \
    "  -g, --globals -- Print global variables.\n"                             \
    "  -i, --lines -- Print source file lines.\n"                              \
    "  -l, --locals -- Print function local variables.\n"                      \
    "  -p, --functions -- Print function/subroutine information.\n"            \
    "  -t, --types -- Print parsed type information.\n"                        \
    "  -u, --unwind -- Print frame unwind info.\n"                             \
    "  -h, --help -- Print this help and exit.\n"                              \

#define TDWARF_OPTIONS_STRING "AaDfgilhptu"

#define TDWARF_OPTION_PRINT_FILES 0x00000001
#define TDWARF_OPTION_PRINT_TYPES 0x00000002
#define TDWARF_OPTION_PRINT_FUNCTIONS 0x00000004
#define TDWARF_OPTION_PRINT_PARAMETERS 0x00000008
#define TDWARF_OPTION_PRINT_LOCALS 0x00000010
#define TDWARF_OPTION_PRINT_GLOBALS 0x00000020
#define TDWARF_OPTION_PRINT_LINES 0x00000040
#define TDWARF_OPTION_PRINT_UNWIND 0x00000080
#define TDWARF_OPTION_DEBUG 0x00000100

#define TDWARF_OPTION_PRINT_ALL         \
    (TDWARF_OPTION_PRINT_FILES |        \
     TDWARF_OPTION_PRINT_TYPES |        \
     TDWARF_OPTION_PRINT_FUNCTIONS |    \
     TDWARF_OPTION_PRINT_PARAMETERS |   \
     TDWARF_OPTION_PRINT_LOCALS |       \
     TDWARF_OPTION_PRINT_GLOBALS |      \
     TDWARF_OPTION_PRINT_LINES |        \
     TDWARF_OPTION_PRINT_UNWIND)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Borrowing internal DWARF library routines.
//

INT
DwarfpReadCieOrFde (
    PDWARF_CONTEXT Context,
    BOOL EhFrame,
    PUCHAR *Table,
    PUCHAR End,
    PDWARF_CIE Cie,
    PDWARF_FDE Fde,
    PBOOL IsCie
    );

INT
TdwarfProcessFunction (
    PDWARF_CONTEXT Context,
    PDEBUG_SYMBOLS Symbols,
    ULONG Options,
    ULONG PrintMask,
    ULONG SpaceCount,
    ULONG FunctionIndex,
    PFUNCTION_SYMBOL Function
    );

INT
TdwarfTestDwarf (
    ULONG Options,
    PSTR FilePath
    );

INT
TdwarfTestUnwind (
    PDEBUG_SYMBOLS Symbols,
    ULONG Options
    );

INT
TdwarfProcessVariable (
    PDWARF_CONTEXT Context,
    PDEBUG_SYMBOLS Symbols,
    ULONG Options,
    ULONG PrintMask,
    ULONG SpaceCount,
    ULONG VariableIndex,
    PDATA_SYMBOL Variable
    );

VOID
TdwarfPrintAddressEncoding (
    DWARF_ADDRESS_ENCODING Encoding
    );

PTYPE_SYMBOL
TdwarfGetType (
    PSOURCE_FILE_SYMBOL File,
    LONG Number
    );

VOID
TdwarfPrintDwarfLocation (
    PDEBUG_SYMBOLS Symbols,
    PDWARF_LOCATION Location
    );

//
// -------------------------------------------------------------------- Globals
//

struct option TdwarfLongOptions[] = {
    {"all", no_argument, 0, 'A'},
    {"arguments", no_argument, 0, 'a'},
    {"debug", no_argument, 0, 'D'},
    {"files", no_argument, 0, 'f'},
    {"globals", no_argument, 0, 'g'},
    {"lines", no_argument, 0, 'i'},
    {"locals", no_argument, 0, 'l'},
    {"functions", no_argument, 0, 'p'},
    {"types", no_argument, 0, 't'},
    {"unwind", no_argument, 0, 'u'},
    {"help", no_argument, 0, 'h'},
    {NULL, 0, 0, 0},
};

PSTR DwarfAddressEncodingNames[] = {
    "DwarfPeAbsolute",
    "DwarfPeLeb128",
    "DwarfPeUdata2",
    "DwarfPeUdata4",
    "DwarfPeUdata8",
    "DwarfPeINVALID",
    "DwarfPeINVALID",
    "DwarfPeINVALID",
    "DwarfPeSigned",
    "DwarfPeSleb128",
    "DwarfPeSdata2",
    "DwarfPeSdata4",
    "DwarfPeSdata8",
    "DwarfPeINVALID",
    "DwarfPeINVALID",
    "DwarfPeINVALID",
};

TYPE_SYMBOL TdwarfVoidType;

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the test program.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of strings representing the arguments.

Return Value:

    Returns 0 on success, nonzero on failure.

--*/

{

    INT ArgumentIndex;
    INT Option;
    ULONG Options;
    INT Status;

    Options = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             TDWARF_OPTIONS_STRING,
                             TdwarfLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'A':
            Options |= TDWARF_OPTION_PRINT_ALL;
            break;

        case 'a':
            Options |= TDWARF_OPTION_PRINT_PARAMETERS;
            break;

        case 'D':
            Options |= TDWARF_OPTION_DEBUG;
            break;

        case 'f':
            Options |= TDWARF_OPTION_PRINT_FILES;
            break;

        case 'g':
            Options |= TDWARF_OPTION_PRINT_GLOBALS;
            break;

        case 'i':
            Options |= TDWARF_OPTION_PRINT_LINES;
            break;

        case 'l':
            Options |= TDWARF_OPTION_PRINT_LOCALS;
            break;

        case 'p':
            Options |= TDWARF_OPTION_PRINT_FUNCTIONS;
            break;

        case 't':
            Options |= TDWARF_OPTION_PRINT_TYPES;
            break;

        case 'u':
            Options |= TDWARF_OPTION_PRINT_UNWIND;
            break;

        case 'h':
            printf(TDWARF_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex == ArgumentCount) {
        fprintf(stderr, "Error: Argument expected.\n");
        printf(TDWARF_USAGE);
        Status = 1;
        goto MainEnd;
    }

    Status = 0;
    while (ArgumentIndex < ArgumentCount) {
        Status = TdwarfTestDwarf(Options, Arguments[ArgumentIndex]);
        if (Status != 0) {
            fprintf(stderr,
                    "Error: Failed to parse DWARF symbols for %s: %s\n",
                    Arguments[ArgumentIndex],
                    strerror(Status));

            break;
        }

        ArgumentIndex += 1;
    }

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
TdwarfTestDwarf (
    ULONG Options,
    PSTR FilePath
    )

/*++

Routine Description:

    This routine tests the DWARF parser for a given file.

Arguments:

    Options - Supplies the bitfield of options.

    FilePath - Supplies a pointer to the path to parse.

Return Value:

    Returns 0 on success, or an error number on failure.

--*/

{

    PDWARF_CONTEXT Context;
    PLIST_ENTRY DataEntry;
    PDATA_SYMBOL DataSymbol;
    ULONG DwarfFlags;
    PDATA_TYPE_ENUMERATION Enumeration;
    PENUMERATION_MEMBER EnumerationMember;
    PSOURCE_FILE_SYMBOL File;
    ULONG FileCount;
    PLIST_ENTRY FileEntry;
    PFUNCTION_SYMBOL Function;
    ULONG FunctionCount;
    PLIST_ENTRY FunctionEntry;
    ULONG GlobalCount;
    PSOURCE_LINE_SYMBOL Line;
    ULONG LineCount;
    PLIST_ENTRY LineEntry;
    PSTRUCTURE_MEMBER Member;
    ULONG MemberCount;
    PTYPE_SYMBOL MemberType;
    PDATA_TYPE_NUMERIC Numeric;
    PSTR Pointer;
    PDATA_TYPE_RELATION Relation;
    PSTR RelationFile;
    PTYPE_SYMBOL RelativeType;
    INT Status;
    PDATA_TYPE_STRUCTURE Structure;
    PDEBUG_SYMBOLS Symbols;
    PTYPE_SYMBOL Type;
    ULONG TypeCount;
    PLIST_ENTRY TypeEntry;

    Symbols = NULL;
    DwarfFlags = 0;
    if ((Options & TDWARF_OPTION_DEBUG) != 0) {
        DwarfFlags = DWARF_CONTEXT_DEBUG | DWARF_CONTEXT_DEBUG_LINE_NUMBERS |
                     DWARF_CONTEXT_DEBUG_ABBREVIATIONS;
    }

    if ((Options & TDWARF_OPTION_PRINT_UNWIND) != 0) {
        DwarfFlags |= DWARF_CONTEXT_DEBUG_FRAMES |
                      DWARF_CONTEXT_VERBOSE_UNWINDING;
    }

    Status = DwarfLoadSymbols(FilePath,
                              ImageMachineTypeUnknown,
                              DwarfFlags,
                              NULL,
                              &Symbols);

    if (Status != 0) {
        fprintf(stderr,
                "Failed to load symbols for %s: %s\n",
                FilePath,
                strerror(Status));

        goto TestDwarfEnd;
    }

    Context = Symbols->SymbolContext;
    Status = TdwarfTestUnwind(Symbols, Options);
    if (Status != 0) {
        fprintf(stderr, "Unwind test failed: %s\n", strerror(Status));
        goto TestDwarfEnd;
    }

    //
    // Iterate through all the symbols, and print what's desired.
    //

    Status = 0;
    FileCount = 0;
    FunctionCount = 0;
    LineCount = 0;
    GlobalCount = 0;
    TypeCount = 0;
    FileEntry = Symbols->SourcesHead.Next;
    while (FileEntry != &(Symbols->SourcesHead)) {
        File = LIST_VALUE(FileEntry, SOURCE_FILE_SYMBOL, ListEntry);

        //
        // Print the source file information.
        //

        if ((Options & TDWARF_OPTION_PRINT_FILES) != 0) {
            printf("%d: ", FileCount);
            if (File->SourceDirectory != NULL) {
                printf("%s/", File->SourceDirectory);
            }

            printf("%s, 0x%llx - 0x%llx\n",
                   File->SourceFile,
                   File->StartAddress,
                   File->EndAddress);
        }

        //
        // Loop through all types in the file.
        //

        TypeEntry = File->TypesHead.Next;
        while (TypeEntry != &(File->TypesHead)) {
            Type = LIST_VALUE(TypeEntry, TYPE_SYMBOL, ListEntry);

            assert(Type->ParentSource != NULL);

            switch (Type->Type) {
            case DataTypeRelation:
                Relation = &(Type->U.Relation);
                Pointer = "";
                if (Relation->Pointer != 0) {
                    Pointer = "*";
                }

                if ((Relation->OwningFile == NULL) &&
                    (Relation->TypeNumber == -1)) {

                    RelationFile = "(none)";

                } else {
                    if (Relation->OwningFile == NULL) {
                        fprintf(stderr,
                                "Error: Relation with no owning file.\n");

                        Status = EINVAL;
                        goto TestDwarfEnd;
                    }

                    RelationFile = Relation->OwningFile->SourceFile;
                }

                if ((Options & TDWARF_OPTION_PRINT_TYPES) != 0) {
                    printf("   Type %d: %s:(%s,%x). Reference Type: %s(%s, %x)",
                           TypeCount,
                           Type->Name,
                           Type->ParentSource->SourceFile,
                           Type->TypeNumber,
                           Pointer,
                           RelationFile,
                           Relation->TypeNumber);

                    if (Relation->Function != FALSE) {
                        printf(" FUNCTION");
                    }
                }

                if ((Relation->Array.Minimum != 0LL) ||
                    (Relation->Array.Maximum != 0LL)) {

                    if ((Options & TDWARF_OPTION_PRINT_TYPES) != 0) {
                        printf(" Array [%lli, %lli]",
                               Relation->Array.Minimum,
                               Relation->Array.Maximum);
                    }

                }

                if ((Options & TDWARF_OPTION_PRINT_TYPES) != 0) {
                    printf("\n");
                }

                RelativeType = TdwarfGetType(Relation->OwningFile,
                                             Relation->TypeNumber);

                if (RelativeType == NULL) {
                    printf("Error: Unable to resolve relation type (%s, %x).\n",
                           Relation->OwningFile->SourceFile,
                           Relation->TypeNumber);

                    Status = EINVAL;
                    goto TestDwarfEnd;
                }

                break;

            case DataTypeNumeric:
                Numeric = &(Type->U.Numeric);
                if ((Options & TDWARF_OPTION_PRINT_TYPES) != 0) {
                    printf("   Type %d: %s:(%s,%x). Numeric: %d bits, ",
                          TypeCount,
                          Type->Name,
                          Type->ParentSource->SourceFile,
                          Type->TypeNumber,
                          Numeric->BitSize);

                    if (Numeric->Float != FALSE) {
                        printf("Float\n");

                    } else if (Numeric->Signed != FALSE) {
                        printf("Signed\n");

                    } else {
                        printf("Unsigned\n");
                    }
                }

                break;

            case DataTypeStructure:
                Structure = &(Type->U.Structure);
                MemberCount = 0;
                if ((Options & TDWARF_OPTION_PRINT_TYPES) != 0) {
                    printf("   Type %d: %s:(%s,%d). Structure: %d Bytes, %d "
                           "Members\n",
                           TypeCount,
                           Type->Name,
                           Type->ParentSource->SourceFile,
                           Type->TypeNumber,
                           Structure->SizeInBytes,
                           Structure->MemberCount);
                }

                if (Structure != NULL) {
                    Member = Structure->FirstMember;
                    if (Member != NULL) {
                        if (Member->TypeFile == NULL) {
                            printf("Error: Dangling type\n");
                            Status = 1;
                            goto TestDwarfEnd;
                        }
                    }

                } else {
                    Member = NULL;
                }

                while (Member != NULL) {
                    if ((Options & TDWARF_OPTION_PRINT_TYPES) != 0) {
                        printf("      +%d", Member->BitOffset / BITS_PER_BYTE);
                        if ((Member->BitOffset % BITS_PER_BYTE) != 0) {
                            printf(":%d", Member->BitOffset % BITS_PER_BYTE);
                        }

                        printf(", %d: %s (%s, %x)\n",
                               Member->BitSize,
                               Member->Name,
                               Member->TypeFile->SourceFile,
                               Member->TypeNumber);
                    }

                    MemberType = TdwarfGetType(Member->TypeFile,
                                               Member->TypeNumber);

                    if (MemberType == NULL) {
                        printf("Error: Unable to resolve structure member "
                               "type from (%s, %d).\n",
                               Member->TypeFile->SourceFile,
                               Member->TypeNumber);

                        Status = EINVAL;
                        goto TestDwarfEnd;
                    }

                    MemberCount += 1;
                    Member = Member->NextMember;
                }

                if (MemberCount != Structure->MemberCount) {
                    printf("   ***ERROR: Structure Member Count does not match"
                           " actual number of structure members. Structure "
                           "reported %d, but %d were found.***\n",
                           Structure->MemberCount,
                           MemberCount);
                }

                break;

            case DataTypeEnumeration:
                Enumeration = &(Type->U.Enumeration);
                MemberCount = 0;
                if ((Options & TDWARF_OPTION_PRINT_TYPES) != 0) {
                    printf("   Type %d: %s:(%s,%x). Enumeration: %d Members\n",
                           TypeCount,
                           Type->Name,
                           Type->ParentSource->SourceFile,
                           Type->TypeNumber,
                           Enumeration->MemberCount);
                }

                assert(Enumeration->FirstMember != NULL);

                EnumerationMember = Enumeration->FirstMember;
                while (EnumerationMember != NULL) {
                    if (EnumerationMember->Name == NULL) {
                        printf("Error: Null enumeration member name.\n");
                        Status = 1;
                        goto TestDwarfEnd;
                    }

                    if ((Options & TDWARF_OPTION_PRINT_TYPES) != 0) {
                        printf("      %s = %lld\n",
                               EnumerationMember->Name,
                               EnumerationMember->Value);
                    }

                    MemberCount += 1;
                    EnumerationMember = EnumerationMember->NextMember;
                }

                if (MemberCount != Enumeration->MemberCount) {
                    printf("   ***ERROR: Enumeration Member Count does not "
                           "match actual number of structure members. "
                           "Enumeration reported %d, but %d were found.***\n",
                           Enumeration->MemberCount,
                           MemberCount);
                }

                break;

            case DataTypeFunctionPointer:
                if ((Options & TDWARF_OPTION_PRINT_TYPES) != 0) {
                    printf("   Type %d: %s(%s,%x). Function Pointer: size %d\n",
                           TypeCount,
                           Type->Name,
                           Type->ParentSource->SourceFile,
                           Type->TypeNumber,
                           Type->U.FunctionPointer.SizeInBytes);
                }

                break;

            default:
                printf("Unknown type %d for symbol (%s, %x)\n",
                       Type->Type,
                       Type->ParentSource->SourceFile,
                       Type->TypeNumber);

                Status = 1;
                goto TestDwarfEnd;
            }

            TypeEntry = TypeEntry->Next;
            TypeCount += 1;
        }

        //
        // Print out all the functions in this source, if desired.
        //

        FunctionEntry = File->FunctionsHead.Next;
        while (FunctionEntry != &(File->FunctionsHead)) {
            Function = LIST_VALUE(FunctionEntry, FUNCTION_SYMBOL, ListEntry);
            Status = TdwarfProcessFunction(Context,
                                           Symbols,
                                           Options,
                                           TDWARF_OPTION_PRINT_FUNCTIONS,
                                           3,
                                           FunctionCount,
                                           Function);

            if (Status != 0) {
                printf("Failed to print function.\n");
                goto TestDwarfEnd;
            }

            FunctionCount += 1;
            FunctionEntry = FunctionEntry->Next;
        }

        //
        // Loop through all the globals.
        //

        DataEntry = File->DataSymbolsHead.Next;
        while (DataEntry != &(File->DataSymbolsHead)) {
            DataSymbol = LIST_VALUE(DataEntry, DATA_SYMBOL, ListEntry);
            Status = TdwarfProcessVariable(Context,
                                           Symbols,
                                           Options,
                                           TDWARF_OPTION_PRINT_GLOBALS,
                                           3,
                                           GlobalCount,
                                           DataSymbol);

            if (Status != 0) {
                goto TestDwarfEnd;
            }

            if (DataSymbol->ParentFunction != NULL) {
                fprintf(stderr, "Error: Global with parent function.\n");
                Status = EINVAL;
                goto TestDwarfEnd;
            }

            GlobalCount += 1;
            DataEntry = DataEntry->Next;
        }

        //
        // Loop through all the lines.
        //

        LineEntry = File->SourceLinesHead.Next;
        while (LineEntry != &(File->SourceLinesHead)) {
            Line = LIST_VALUE(LineEntry, SOURCE_LINE_SYMBOL, ListEntry);
            if ((Options & TDWARF_OPTION_PRINT_LINES) != 0) {
                printf("   Line %u: %s/%s:%d: %llx - %llx\n",
                       LineCount,
                       Line->ParentSource->SourceDirectory,
                       Line->ParentSource->SourceFile,
                       Line->LineNumber,
                       Line->Start,
                       Line->End);
            }

            if (Line->End < Line->Start) {
                fprintf(stderr, "Error: Line end less than start.\n");
                Status = EINVAL;
                goto TestDwarfEnd;
            }

            LineCount += 1;
            LineEntry = LineEntry->Next;
        }

        FileEntry = FileEntry->Next;
        FileCount += 1;
    }

TestDwarfEnd:
    if (Symbols != NULL) {
        DbgUnloadSymbols(Symbols);
    }

    return Status;
}

INT
TdwarfProcessFunction (
    PDWARF_CONTEXT Context,
    PDEBUG_SYMBOLS Symbols,
    ULONG Options,
    ULONG PrintMask,
    ULONG SpaceCount,
    ULONG FunctionIndex,
    PFUNCTION_SYMBOL Function
    )

/*++

Routine Description:

    This routine processes and potentially prints a function.

Arguments:

    Context - Supplies a pointer to the symbol context.

    Symbols - Supplies a pointer to the debug symbols.

    Options - Supplies the bitfield of application options. See TDWARF_OPTION_*
        definitions.

    PrintMask - Supplies the mask to apply to the options to determine whether
        or not to print the function.

    SpaceCount - Supplies the depth to print at.

    FunctionIndex - Supplies the number of the function, for printing purposes.

    Function - Supplies a pointer to the function to process.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PLIST_ENTRY DataEntry;
    PDATA_SYMBOL DataSymbol;
    ULONG FunctionCount;
    PLIST_ENTRY FunctionEntry;
    ULONG LocalCount;
    ULONG ParameterCount;
    PSTR ReturnTypeSource;
    INT Status;
    PFUNCTION_SYMBOL Subfunction;

    assert(Function->ParentSource != NULL);

    if ((Options & TDWARF_OPTION_PRINT_FUNCTIONS) != 0) {
        if (Function->ReturnTypeOwner != NULL) {
            ReturnTypeSource = Function->ReturnTypeOwner->SourceFile;

        } else {
            ReturnTypeSource = "NONE";
        }

        printf("%*sFunction %d: (%s, %d) %s: 0x%08llx - 0x%08llx\n",
               SpaceCount,
               "",
               FunctionIndex,
               ReturnTypeSource,
               Function->ReturnTypeNumber,
               Function->Name,
               Function->StartAddress,
               Function->EndAddress);
    }

    //
    // Print function parameters.
    //

    ParameterCount = 0;
    DataEntry = Function->ParametersHead.Next;
    while (DataEntry != &(Function->ParametersHead)) {
        DataSymbol = LIST_VALUE(DataEntry, DATA_SYMBOL, ListEntry);
        Status = TdwarfProcessVariable(Context,
                                       Symbols,
                                       Options,
                                       TDWARF_OPTION_PRINT_PARAMETERS,
                                       5,
                                       ParameterCount,
                                       DataSymbol);

        if (Status != 0) {
            goto ProcessFunctionEnd;
        }

        if (DataSymbol->ParentFunction != Function) {
            fprintf(stderr,
                    "Error: Parameter parent is not function.\n");

            Status = EINVAL;
            goto ProcessFunctionEnd;
        }

        ParameterCount += 1;
        DataEntry = DataEntry->Next;
    }

    if ((Options & TDWARF_OPTION_PRINT_PARAMETERS) != 0) {
        printf("\n");
    }

    //
    // Print local variables.
    //

    LocalCount = 0;
    DataEntry = Function->LocalsHead.Next;
    while (DataEntry != &(Function->LocalsHead)) {
        DataSymbol = LIST_VALUE(DataEntry, DATA_SYMBOL, ListEntry);
        Status = TdwarfProcessVariable(Context,
                                       Symbols,
                                       Options,
                                       TDWARF_OPTION_PRINT_LOCALS,
                                       5,
                                       LocalCount,
                                       DataSymbol);

        if (Status != 0) {
            goto ProcessFunctionEnd;
        }

        if (DataSymbol->ParentFunction != Function) {
            fprintf(stderr,
                    "Error: Parameter parent is not function.\n");

            Status = EINVAL;
            goto ProcessFunctionEnd;
        }

        LocalCount += 1;
        DataEntry = DataEntry->Next;
    }

    //
    // Print out sub-functions (eg inlines).
    //

    FunctionCount = 0;
    FunctionEntry = Function->FunctionsHead.Next;
    while (FunctionEntry != &(Function->FunctionsHead)) {
        Subfunction = LIST_VALUE(FunctionEntry, FUNCTION_SYMBOL, ListEntry);
        FunctionEntry = FunctionEntry->Next;
        Status = TdwarfProcessFunction(Context,
                                       Symbols,
                                       Options,
                                       PrintMask,
                                       SpaceCount + 3,
                                       FunctionCount,
                                       Subfunction);

        if (Status != 0) {
            goto ProcessFunctionEnd;
        }

        FunctionCount += 1;
    }

    Status = 0;

ProcessFunctionEnd:
    return Status;
}

INT
TdwarfProcessVariable (
    PDWARF_CONTEXT Context,
    PDEBUG_SYMBOLS Symbols,
    ULONG Options,
    ULONG PrintMask,
    ULONG SpaceCount,
    ULONG VariableIndex,
    PDATA_SYMBOL Variable
    )

/*++

Routine Description:

    This routine processes and potentially prints a data symbol.

Arguments:

    Context - Supplies a pointer to the symbol context.

    Symbols - Supplies a pointer to the debug symbols.

    Options - Supplies the bitfield of application options. See TDWARF_OPTION_*
        definitions.

    PrintMask - Supplies the mask to apply to the options to determine whether
        or not to print the variable.

    SpaceCount - Supplies the depth to print at.

    VariableIndex - Supplies the number of the variable, for printing purposes.

    Variable - Supplies a pointer to the variable to process.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDWARF_ATTRIBUTE_VALUE AttributeValue;
    ULONGLONG Base;
    PDWARF_COMPLEX_DATA_SYMBOL Complex;
    USHORT Length;
    DWARF_LOCATION_CONTEXT LocationContext;
    ULONGLONG LocationEnd;
    PUCHAR LocationList;
    ULONGLONG LocationStart;
    BOOL Print;
    PSTR Register;
    INT Status;
    PSTR Type;
    PSTR TypeFile;
    PDWARF_COMPILATION_UNIT Unit;

    Status = 0;
    Print = FALSE;
    if ((Options & PrintMask) != 0) {
        Print = TRUE;
    }

    Type = "Variable";
    if ((PrintMask & TDWARF_OPTION_PRINT_PARAMETERS) != 0) {
        Type = "Parameter";

    } else if ((PrintMask & TDWARF_OPTION_PRINT_LOCALS) != 0) {
        Type = "Local";

    } else if ((PrintMask & TDWARF_OPTION_PRINT_GLOBALS) != 0) {
        Type = "Global";
    }

    TypeFile = "(none)";
    if (Variable->TypeOwner != NULL) {
        TypeFile = Variable->TypeOwner->SourceFile;
    }

    if (Print != FALSE) {
        printf("%*s%s %d: %s (%s, %x)",
               SpaceCount,
               "",
               Type,
               VariableIndex,
               Variable->Name,
               TypeFile,
               Variable->TypeNumber);
    }

    switch (Variable->LocationType) {
    case DataLocationRegister:
        Register = DbgGetRegisterName(Symbols->Machine,
                                      Variable->Location.Register);

        if (Print != FALSE) {
            printf(" @%s", Register);
        }

        break;

    case DataLocationIndirect:
        Register = DbgGetRegisterName(Symbols->Machine,
                                      Variable->Location.Indirect.Register);

        if (Print != FALSE) {
            printf(" [%s%+lld]", Register, Variable->Location.Indirect.Offset);
        }

        break;

    case DataLocationAbsoluteAddress:
        if (Print != FALSE) {
            printf(" [0x%llx]", Variable->Location.Address);
        }

        break;

    case DataLocationComplex:
        Unit = Variable->ParentSource->SymbolContext;
        memset(&LocationContext, 0, sizeof(DWARF_LOCATION_CONTEXT));
        LocationContext.Unit = Unit;
        Complex = Variable->Location.Complex;
        AttributeValue = &(Complex->LocationAttribute);

        //
        // If it's a location list, then print the location for each entry.
        //

        if (DWARF_SECTION_OFFSET_FORM(AttributeValue->Form, Unit)) {
            if (Print != FALSE) {
                printf(" Location List:");
            }

            LocationList = Context->Sections.Locations.Data +
                           AttributeValue->Value.Offset;

            //
            // Loop over all the location entries.
            //

            while (TRUE) {
                Base = LocationContext.Unit->LowPc;
                if (Complex->Unit->AddressSize == 8) {
                    LocationStart = DwarfpRead8(&LocationList);
                    LocationEnd = DwarfpRead8(&LocationList);
                    if (LocationStart == MAX_ULONGLONG) {
                        Base = LocationEnd;
                        continue;
                    }

                } else {

                    assert(Complex->Unit->AddressSize == 4);

                    LocationStart = DwarfpRead4(&LocationList);
                    LocationEnd = DwarfpRead4(&LocationList);
                    if (LocationStart == MAX_ULONG) {
                        Base = LocationEnd;
                        continue;
                    }
                }

                assert((PVOID)LocationList <=
                       Context->Sections.Locations.Data +
                       Context->Sections.Locations.Size);

                if ((LocationStart == 0) && (LocationEnd == 0)) {
                    break;
                }

                Length = DwarfpRead2(&LocationList);
                LocationList += Length;
                if (Print != FALSE) {
                    printf("\n       [%llx - %llx (%d)] ",
                           LocationStart + Base,
                           LocationEnd + Base,
                           Length);
                }

                //
                // Skip empty locations.
                //

                if (LocationStart == LocationEnd) {
                    continue;
                }

                //
                // Set the PC to the start of this region, and get the location.
                //

                LocationContext.Pc = LocationStart + Base;
                Status = DwarfpGetLocation(Context,
                                           &LocationContext,
                                           AttributeValue);

                if (Status != 0) {
                    fprintf(stderr,
                            "Error: Failed to get DWARF location of %s\n",
                            Variable->Name);

                    return Status;
                }

                if (Print != FALSE) {
                    TdwarfPrintDwarfLocation(Symbols,
                                             &(LocationContext.Location));
                }

                //
                // Reset the location context.
                //

                memset(&LocationContext, 0, sizeof(DWARF_LOCATION_CONTEXT));
                LocationContext.Unit = Variable->ParentSource->SymbolContext;
            }

        //
        // The variable location is not a location list, but just a single
        // expression. Go get it.
        //

        } else {
            Status = DwarfpGetLocation(Context,
                                       &LocationContext,
                                       AttributeValue);

            if (Status != 0) {
                fprintf(stderr,
                        "Error: Failed to get DWARF location of %s\n",
                        Variable->Name);

                return Status;
            }

            if (Print != FALSE) {
                TdwarfPrintDwarfLocation(Symbols, &(LocationContext.Location));
            }
        }

        break;

    default:
        fprintf(stderr,
                "Error: Unknown location type %d.\n",
                Variable->LocationType);

        Status = EINVAL;
        break;
    }

    if (Variable->MinimumValidExecutionAddress != 0) {
        if (Print != FALSE) {
            printf(" Valid at %llx", Variable->MinimumValidExecutionAddress);
        }
    }

    if (Print != FALSE) {
        printf("\n");
    }

    return Status;
}

INT
TdwarfTestUnwind (
    PDEBUG_SYMBOLS Symbols,
    ULONG Options
    )

/*++

Routine Description:

    This routine exercises the DWARF unwind code by asking it to unwind every
    possible frame at its highest PC.

Arguments:

    Symbols - Supplies a pointer to the loaded symbols.

    Options - Supplies the TDWARF_OPTION_* options.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    DWARF_CIE Cie;
    PDWARF_CONTEXT Context;
    BOOL EhFrame;
    PUCHAR End;
    DWARF_FDE Fde;
    STACK_FRAME Frame;
    BOOL IsCie;
    PUCHAR ObjectStart;
    ULONGLONG Pc;
    PUCHAR Start;
    INT Status;
    PUCHAR Table;

    Context = Symbols->SymbolContext;

    //
    // Get the .debug_frame or .eh_frame sections.
    //

    if (Context->Sections.Frame.Size != 0) {
        Table = Context->Sections.Frame.Data;
        End = Table + Context->Sections.Frame.Size;
        if ((Options & TDWARF_OPTION_PRINT_UNWIND) != 0) {
            printf(".debug_frame section, %ld bytes\n", (long)(End - Table));
        }

        EhFrame = FALSE;

    } else if (Context->Sections.EhFrame.Size != 0) {
        Table = Context->Sections.EhFrame.Data;
        End = Table + Context->Sections.EhFrame.Size;
        if ((Options & TDWARF_OPTION_PRINT_UNWIND) != 0) {
            printf(".eh_frame section, %ld bytes\n", (long)(End - Table));
        }

        EhFrame = TRUE;

    } else {
        Status = ENOENT;
        goto TestUnwindEnd;
    }

    Start = Table;
    memset(&Cie, 0, sizeof(DWARF_CIE));
    memset(&Fde, 0, sizeof(DWARF_FDE));

    //
    // Loop through the table and try an unwind on every FDE found.
    //

    while (Table < End) {
        ObjectStart = Table;
        Status = DwarfpReadCieOrFde(Context,
                                    EhFrame,
                                    &Table,
                                    End,
                                    &Cie,
                                    &Fde,
                                    &IsCie);

        if (Status != 0) {
            if (Status == EAGAIN) {
                if ((Options & TDWARF_OPTION_PRINT_UNWIND) != 0) {
                    printf(" Zero terminator Offset %lx.\n\n",
                           (long)(ObjectStart - Start));
                }

                continue;
            }

            goto TestUnwindEnd;
        }

        if (IsCie != FALSE) {
            if ((Options & TDWARF_OPTION_PRINT_UNWIND) != 0) {
                printf(" CIE Offset %lx Length %llx\n"
                       "  Version: %d\n"
                       "  Augmentation: \"%s\"\n"
                       "  Address Size: %d\n"
                       "  Segment Size: %d\n"
                       "  Code Alignment Factor: %llu\n"
                       "  Data Alignment Factor: %lld\n"
                       "  Return Address Register: %llu\n"
                       "  Augmentation Length: %llu\n"
                       "  Language Encoding: ",
                       (long)(ObjectStart - Start),
                       Cie.UnitLength,
                       Cie.Version,
                       Cie.Augmentation,
                       Cie.AddressSize,
                       Cie.SegmentSize,
                       Cie.CodeAlignmentFactor,
                       Cie.DataAlignmentFactor,
                       Cie.ReturnAddressRegister,
                       Cie.AugmentationLength);

                TdwarfPrintAddressEncoding(Cie.LanguageEncoding);
                printf("\n  Personality: ");
                TdwarfPrintAddressEncoding(Cie.Personality);
                printf("\n  FdeEncoding: ");
                TdwarfPrintAddressEncoding(Cie.FdeEncoding);
                printf("\n\n");
            }

            continue;

        } else {
            if ((Options & TDWARF_OPTION_PRINT_UNWIND) != 0) {
                printf("  FDE Offset %lx Length %llx CIE %lld "
                       "PC %llx - %llx\n",
                       (long)(ObjectStart - Start),
                       Fde.Length,
                       Fde.CiePointer,
                       Fde.InitialLocation,
                       Fde.InitialLocation + Fde.Range);
            }

            Pc = Fde.InitialLocation + Fde.Range - 1;
            Status = DwarfStackUnwind(Symbols, Pc, &Frame);
            if (Status != 0) {
                fprintf(stderr,
                        "Error: Failed to unwind stack for PC %llx.\n",
                        Pc);

                goto TestUnwindEnd;
            }

            if ((Options & TDWARF_OPTION_PRINT_UNWIND) != 0) {
                printf("\n");
            }
        }
    }

    Status = 0;

TestUnwindEnd:
    return Status;
}

VOID
TdwarfPrintAddressEncoding (
    DWARF_ADDRESS_ENCODING Encoding
    )

/*++

Routine Description:

    This routine prints a description of the given address encoding.

Arguments:

    Encoding - Supplies the address encoding value.

Return Value:

    None.

--*/

{

    if (Encoding == DwarfPeOmit) {
        printf("DwarfPeOmit");
        return;
    }

    printf("%s", DwarfAddressEncodingNames[Encoding & DwarfPeTypeMask]);
    switch (Encoding & DwarfPeModifierMask) {
    case DwarfPeAbsolute:
        break;

    case DwarfPePcRelative:
        printf(", DwarfPePcRelative");
        break;

    case DwarfPeTextRelative:
        printf(", DwarfPeTextRelative");
        break;

    case DwarfPeDataRelative:
        printf(", DwarfPeDataRelative");
        break;

    case DwarfPeFunctionRelative:
        printf(", DwarfPeFunctionRelative");
        break;

    case DwarfPeAligned:
        printf(", DwarfPeAligned");
        break;

    default:
        printf(", Unknown%x", Encoding & DwarfPeModifierMask);
        break;
    }

    if ((Encoding & DwarfPeIndirect) != 0) {
        printf(", DwarfPeIndirect");
    }

    return;
}

PTYPE_SYMBOL
TdwarfGetType (
    PSOURCE_FILE_SYMBOL File,
    LONG Number
    )

/*++

Routine Description:

    This routine finds a type with the given identifier.

Arguments:

    File - Supplies a pointer to the source file to search through.

    Number - Supplies the type number to find.

Return Value:

    Returns a pointer to the type on success.

    NULL if no type with the given identifier could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PTYPE_SYMBOL Type;

    if ((File == NULL) && (Number == -1)) {
        return &TdwarfVoidType;
    }

    CurrentEntry = File->TypesHead.Next;
    while (CurrentEntry != &(File->TypesHead)) {
        Type = LIST_VALUE(CurrentEntry, TYPE_SYMBOL, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Type->TypeNumber == Number) {
            return Type;
        }
    }

    return NULL;
}

VOID
TdwarfPrintDwarfLocation (
    PDEBUG_SYMBOLS Symbols,
    PDWARF_LOCATION Location
    )

/*++

Routine Description:

    This routine prints a DWARF location.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    Location - Supplies a pointer to the location to print.

Return Value:

    None.

--*/

{

    PUCHAR Bytes;
    UINTN Index;
    PSTR Register;

    while (Location != NULL) {
        switch (Location->Form) {
        case DwarfLocationMemory:
            printf(" [%llx]", Location->Value.Address);
            break;

        case DwarfLocationRegister:
            Register = DbgGetRegisterName(Symbols->Machine,
                                          Location->Value.Register);

            printf(" @%s", Register);
            break;

        case DwarfLocationKnownData:
            Bytes = Location->Value.Buffer.Data;
            printf(" Known Data ");
            for (Index = 0; Index < Location->Value.Buffer.Size; Index += 1) {
                printf("%02x ", *Bytes);
                Bytes += 1;
            }

            break;

        case DwarfLocationKnownValue:
            printf(" Known Value 0x%llx", Location->Value.Value);
            break;

        case DwarfLocationUndefined:
            printf("Undefined");
            break;

        default:

            assert(FALSE);

            break;
        }

        if (Location->BitSize != 0) {
            printf(" Piece %d bits", Location->BitSize);
            if (Location->BitOffset != 0) {
                printf(" Offset %d bits", Location->BitOffset);
            }
        }

        Location = Location->NextPiece;
        if (Location != NULL) {
            printf(" ");
        }
    }

    return;
}

//
// Routines called by the DWARF library.
//

INT
DwarfTargetRead (
    PDWARF_CONTEXT Context,
    ULONGLONG TargetAddress,
    ULONGLONG Size,
    ULONG AddressSpace,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine performs a read from target memory.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    TargetAddress - Supplies the address to read from.

    Size - Supplies the number of bytes to read.

    AddressSpace - Supplies the address space identifier. Supply 0 for normal
        memory.

    Buffer - Supplies a pointer where the read data will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    memset(Buffer, 0, Size);
    return 0;
}

INT
DwarfTargetReadRegister (
    PDWARF_CONTEXT Context,
    ULONG Register,
    PULONGLONG Value
    )

/*++

Routine Description:

    This routine reads a register value.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Register - Supplies the register to read.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    *Value = 0;
    return 0;
}

INT
DwarfTargetWriteRegister (
    PDWARF_CONTEXT Context,
    ULONG Register,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine writes a register value.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Register - Supplies the register to write.

    Value - Supplies the new value of the register.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return 0;
}

INT
DwarfTargetWritePc (
    PDWARF_CONTEXT Context,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine writes a the instruction pointer register, presumably with the
    return address.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Value - Supplies the new value of the register.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return 0;
}

PSTR
DwarfGetRegisterName (
    PDWARF_CONTEXT Context,
    ULONG Register
    )

/*++

Routine Description:

    This routine returns a string containing the name of the given register.

Arguments:

    Context - Supplies a pointer to the application context.

    Register - Supplies the register number.

Return Value:

    Returns a pointer to a constant string containing the name of the register.

--*/

{

    PDEBUG_SYMBOLS Symbols;

    Symbols = (((PDEBUG_SYMBOLS)Context) - 1);
    return DbgGetRegisterName(Symbols->Machine, Register);
}

INT
DbgOut (
    const char *Format,
    ...
    )

/*++

Routine Description:

    This routine prints a formatted string to the debugger console.

Arguments:

    Format - Supplies the printf format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vprintf(Format, Arguments);
    va_end(Arguments);
    return Result;
}

