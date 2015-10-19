/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    teststab.c

Abstract:

    This module tests the debugging symbol subcomponent.

Author:

    Evan Green 26-Jul-2012

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <minoca/types.h>
#include <minoca/status.h>
#include <minoca/im.h>
#include "../stabs.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TESTSTABS_USAGE "Usage: teststabs [-f] [-g] [-l] [-s] [-t] [-v] " \
                        "[-r Query] [-a Address] <file.exe> \n"           \
                        "Options:\n"                                      \
                        "    -f  Print functions\n"                       \
                        "    -g  Print globals/statics\n"                 \
                        "    -l  Print local variables\n"                 \
                        "    -s  Print source lines\n"                    \
                        "    -t  Print types\n"                           \
                        "    -r  Search for a symbol by name.\n"          \
                        "    -a  Search for a symbol by address.\n"
//
// ----------------------------------------------- Internal Function Prototypes
//

PTYPE_SYMBOL
DbgGetType (
    PSOURCE_FILE_SYMBOL SourceFile,
    LONG TypeNumber
    );

BOOL
DbgpStringMatch (
    PSTR Query,
    PSTR PossibleMatch
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR I386RegisterNames[] = {
    "eax",
    "ecx",
    "edx",
    "ebx",
    "esp",
    "ebp",
    "esi",
    "edi"
};

PSTR ArmRegisterNames[] = {
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

//
// Unused global to satisfy the linker.
//

LOADED_MODULE_LIST ModuleList;

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
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

    ULONG ArgumentIndex;
    PENUMERATION_MEMBER CurrentEnumerationMember;
    PLIST_ENTRY CurrentFunctionEntry;
    PLIST_ENTRY CurrentGlobalEntry;
    PLIST_ENTRY CurrentLineEntry;
    PLIST_ENTRY CurrentLocalEntry;
    PLIST_ENTRY CurrentParameterEntry;
    PSOURCE_FILE_SYMBOL CurrentSource;
    PLIST_ENTRY CurrentSourceEntry;
    PSTRUCTURE_MEMBER CurrentStructureMember;
    PTYPE_SYMBOL CurrentType;
    PLIST_ENTRY CurrentTypeEntry;
    PDATA_TYPE_ENUMERATION Enumeration;
    PFUNCTION_SYMBOL Function;
    ULONG FunctionsProcessed;
    PDATA_SYMBOL GlobalVariable;
    PSTR ImageName;
    ULONG LinesProcessed;
    PDATA_SYMBOL LocalVariable;
    ULONG MemberCount;
    PTYPE_SYMBOL MemberType;
    PDATA_TYPE_NUMERIC Numeric;
    PDATA_SYMBOL Parameter;
    CHAR PointerCharacter;
    BOOL PrintFunctions;
    BOOL PrintGlobals;
    BOOL PrintLocalVariables;
    BOOL PrintSourceFiles;
    BOOL PrintSourceLines;
    BOOL PrintTypes;
    BOOL PrintVerbose;
    PSTR QueryAddress;
    PSTR QueryString;
    PSTR RegisterName;
    PDATA_TYPE_RELATION Relation;
    PTYPE_SYMBOL RelativeType;
    BOOL Result;
    PSYMBOL_SEARCH_RESULT ResultValid;
    PSTR ReturnTypeSource;
    ULONGLONG SearchAddress;
    SYMBOL_SEARCH_RESULT SearchResult;
    ULONG SourceFilesProcessed;
    PSOURCE_LINE_SYMBOL SourceLine;
    PDATA_TYPE_STRUCTURE Structure;
    PDEBUG_SYMBOLS Symbols;
    PSTR TypeName;
    ULONG TypeSize;
    ULONG TypesProcessed;

    ImageName = NULL;
    PrintFunctions = FALSE;
    PrintGlobals = FALSE;
    PrintLocalVariables = FALSE;
    PrintSourceFiles = FALSE;
    PrintSourceLines = FALSE;
    PrintTypes = FALSE;
    PrintVerbose = FALSE;
    QueryAddress = NULL;
    QueryString = NULL;
    if (ArgumentCount < 2) {
        printf(TESTSTABS_USAGE);
        return -1;
    }

    //
    // Parse arguments.
    //

    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        if (Arguments[ArgumentIndex][0] == '-') {
            switch (Arguments[ArgumentIndex][1]) {
            case 'a':
            case 'A':
                if (ArgumentIndex + 1 > ArgumentCount) {
                    printf("Error: Specify an address query with -a!\n");
                    return -1;
                }

                QueryAddress = Arguments[ArgumentIndex + 1];
                ArgumentIndex += 1;
                break;

            case 'f':
            case 'F':
                PrintFunctions = TRUE;
                PrintSourceFiles = TRUE;
                break;

            case 'g':
            case 'G':
                PrintGlobals = TRUE;
                PrintSourceFiles = TRUE;
                break;

            case 'l':
            case 'L':
                PrintLocalVariables = TRUE;
                PrintSourceFiles = TRUE;
                PrintFunctions = TRUE;
                break;

            case 'r':
            case 'R':
                if (ArgumentIndex + 1 > ArgumentCount) {
                    printf("Error: Specify a search query with -r!\n");
                    return -1;
                }

                QueryString = Arguments[ArgumentIndex + 1];
                ArgumentIndex += 1;
                break;

            case 's':
            case 'S':
                PrintSourceLines = TRUE;
                PrintSourceFiles = TRUE;
                break;

            case 't':
            case 'T':
                PrintTypes = TRUE;
                PrintSourceFiles = TRUE;
                break;

            case 'v':
            case 'V':
                PrintVerbose = TRUE;
                PrintSourceFiles = TRUE;
                break;

            default:
                printf("Invalid argument \"%s\".\n", Arguments[ArgumentIndex]);
            }

        } else {
            ImageName = Arguments[ArgumentIndex];
        }
    }

    if (ImageName == NULL) {
        printf("Error: Specify an image!\n");
    }

    if (PrintVerbose != FALSE) {
        printf("Loading symbols...");
    }

    Symbols = DbgLoadSymbols(ImageName, NULL);
    if (PrintVerbose != FALSE) {
        printf("Done\n");
    }

    if (Symbols == NULL) {
        printf("Error loading symbols\n");
        Result = FALSE;
        goto MainEnd;
    }

    if ((Symbols->Machine != ImageMachineTypeX86) &&
        (Symbols->Machine != ImageMachineTypeArm32)) {

        Result = FALSE;
        goto MainEnd;
    }

    //
    // Loop over all source files, printing information.
    //

    SourceFilesProcessed = 0;
    CurrentSourceEntry = Symbols->SourcesHead.Next;
    while (CurrentSourceEntry != &(Symbols->SourcesHead)) {
        CurrentSource = LIST_VALUE(CurrentSourceEntry,
                                   SOURCE_FILE_SYMBOL,
                                   ListEntry);

        if (PrintSourceFiles != FALSE) {
            printf("%d: ", SourceFilesProcessed);
            if (CurrentSource->SourceDirectory != NULL) {
                printf(CurrentSource->SourceDirectory);
            }

            printf("%s, 0x%08I64x - 0x%08I64x\n",
                   CurrentSource->SourceFile,
                   CurrentSource->StartAddress,
                   CurrentSource->EndAddress);
        }

        //
        // Loop through all global variables.
        //

        CurrentGlobalEntry = CurrentSource->DataSymbolsHead.Next;
        while (CurrentGlobalEntry != &(CurrentSource->DataSymbolsHead)) {
            GlobalVariable = LIST_VALUE(CurrentGlobalEntry,
                                        DATA_SYMBOL,
                                        ListEntry);

            if (PrintGlobals != FALSE) {
                printf("   Global %s: (%s,%d) at 0x%08I64x\n",
                       GlobalVariable->Name,
                       GlobalVariable->TypeOwner->SourceFile,
                       GlobalVariable->TypeNumber,
                       GlobalVariable->Address);

            }

            CurrentGlobalEntry = CurrentGlobalEntry->Next;
        }

        //
        // Print out all the functions in this source, if desired.
        //

        FunctionsProcessed = 0;
        CurrentFunctionEntry = CurrentSource->FunctionsHead.Next;
        while (CurrentFunctionEntry != &(CurrentSource->FunctionsHead)) {
            if (CurrentFunctionEntry == NULL) {
                printf("***ERROR: List entry %d in Functions was NULL***\n",
                       FunctionsProcessed);

                assert(CurrentFunctionEntry != NULL);
            }

            Function = LIST_VALUE(CurrentFunctionEntry,
                                  FUNCTION_SYMBOL,
                                  ListEntry);

            assert(Function->ParentSource != NULL);
            assert(Function->Name != NULL);
            assert(Function->EndAddress > Function->StartAddress);

            if (PrintFunctions != FALSE) {

                assert(Function->ParentSource != NULL);

                if (Function->ReturnTypeOwner != NULL) {
                    ReturnTypeSource = Function->ReturnTypeOwner->SourceFile;

                } else {
                    ReturnTypeSource = "NONE";
                }

                printf("   Function %d in %s: (%s, %d) %s: 0x%08I64x - "
                       "0x%08I64x\n",
                       (ULONG)Function->FunctionNumber,
                       Function->ParentSource->SourceFile,
                       ReturnTypeSource,
                       Function->ReturnTypeNumber,
                       Function->Name,
                       Function->StartAddress,
                       Function->EndAddress);
            }

            //
            // Print function parameters.
            //

            CurrentParameterEntry = Function->ParametersHead.Next;
            while (CurrentParameterEntry != &(Function->ParametersHead)) {
                Parameter = LIST_VALUE(CurrentParameterEntry,
                                       DATA_SYMBOL,
                                       ListEntry);

                if (Parameter->Location == DataLocationStackOffset) {
                    if (PrintFunctions != FALSE) {
                        printf("      +%d %s: (%s, %d)\n",
                               Parameter->StackOffset,
                               Parameter->Name,
                               Parameter->TypeOwner->SourceFile,
                               Parameter->TypeNumber);
                    }

                } else {

                    assert(Parameter->Location == DataLocationRegister);
                    assert(Parameter->Register >= 0);

                    if (PrintFunctions != FALSE) {
                        RegisterName = NULL;
                        if (Symbols->Machine == ImageMachineTypeX86) {
                            RegisterName =
                                      I386RegisterNames[Parameter->Register];

                        } else {

                            assert(Symbols->Machine == ImageMachineTypeArm32);

                            RegisterName =
                                       ArmRegisterNames[Parameter->Register];
                        }

                        printf("      @%s %s: (%s, %d)\n",
                               RegisterName,
                               Parameter->Name,
                               Parameter->TypeOwner->SourceFile,
                               Parameter->TypeNumber);
                    }

                }

                assert(Parameter->ParentFunction == Function);

                CurrentParameterEntry = CurrentParameterEntry->Next;
            }

            //
            // Print local variables.
            //

            CurrentLocalEntry = Function->LocalsHead.Next;
            while (CurrentLocalEntry != &(Function->LocalsHead)) {

                assert(CurrentLocalEntry != NULL);

                LocalVariable = LIST_VALUE(CurrentLocalEntry,
                                           DATA_SYMBOL,
                                           ListEntry);

                if (LocalVariable->Location == DataLocationRegister) {

                    assert(LocalVariable->Register >= 0);

                    if (PrintLocalVariables != FALSE) {
                        RegisterName = NULL;
                        if (Symbols->Machine == ImageMachineTypeX86) {
                            RegisterName =
                                  I386RegisterNames[LocalVariable->Register];

                        } else {

                            assert(Symbols->Machine == ImageMachineTypeArm32);

                            RegisterName =
                                   ArmRegisterNames[LocalVariable->Register];
                        }

                        printf("         Local %s (%s, %d)  @%s, Valid at "
                               "0x%08I64x\n",
                               LocalVariable->Name,
                               LocalVariable->TypeOwner->SourceFile,
                               LocalVariable->TypeNumber,
                               RegisterName,
                               LocalVariable->MinimumValidExecutionAddress);
                    }

                } else if (LocalVariable->Location == DataLocationStackOffset) {
                    if (PrintLocalVariables != FALSE) {
                        printf("         Local %s (%s, %d)  offset %d, Valid "
                               "at 0x%08I64x\n",
                               LocalVariable->Name,
                               LocalVariable->TypeOwner->SourceFile,
                               LocalVariable->TypeNumber,
                               LocalVariable->StackOffset,
                               LocalVariable->MinimumValidExecutionAddress);
                    }

                }

                CurrentLocalEntry = CurrentLocalEntry->Next;
            }

            FunctionsProcessed += 1;
            CurrentFunctionEntry = CurrentFunctionEntry->Next;
        }

        //
        // Print out all source lines in this file, if desired.
        //

        LinesProcessed = 0;
        CurrentLineEntry = CurrentSource->SourceLinesHead.Next;
        while (CurrentLineEntry != &(CurrentSource->SourceLinesHead)) {

            assert(CurrentLineEntry != NULL);

            SourceLine = LIST_VALUE(CurrentLineEntry,
                                    SOURCE_LINE_SYMBOL,
                                    ListEntry);

            if (SourceLine->AbsoluteAddress == FALSE) {
                if (PrintSourceLines != FALSE) {
                    printf("   Line %d of file %s in function %s: %08I64x - "
                           "%08I64x\n",
                           SourceLine->LineNumber,
                           SourceLine->ParentSource->SourceFile,
                           SourceLine->ParentFunction->Name,
                           SourceLine->StartOffset,
                           SourceLine->EndOffset);
                }

            } else {
                if (PrintSourceLines != FALSE) {
                    printf("   Line %d of file %s with absolute address "
                           "%08I64x - %08I64x\n",
                           SourceLine->LineNumber,
                           SourceLine->ParentSource->SourceFile,
                           SourceLine->StartOffset,
                           SourceLine->EndOffset);
                }
            }

            assert(SourceLine->EndOffset >= SourceLine->StartOffset);

            CurrentLineEntry = CurrentLineEntry->Next;
            LinesProcessed += 1;
        }

        //
        // Print out all types in this source, if desired.
        //

        TypesProcessed = 0;
        CurrentTypeEntry = CurrentSource->TypesHead.Next;
        while (CurrentTypeEntry != &(CurrentSource->TypesHead)) {
            if (CurrentTypeEntry == NULL) {
                printf("***ERROR: List entry %d in Types was NULL***\n",
                       TypesProcessed);

                assert(CurrentTypeEntry != NULL);
            }

            CurrentType = LIST_VALUE(CurrentTypeEntry,
                                     TYPE_SYMBOL,
                                     ListEntry);

            assert(CurrentType->ParentSource != NULL);
            assert(CurrentType->Data != NULL);

            TypeName = CurrentType->Name;
            if (TypeName == NULL) {
                TypeName = "";
            }

            switch (CurrentType->Type) {
            case DataTypeRelation:
                Relation = (PDATA_TYPE_RELATION)CurrentType->Data;
                PointerCharacter = ' ';
                if (Relation->Pointer != FALSE) {
                    PointerCharacter = '*';
                }

                assert(Relation->OwningFile != NULL);

                if (PrintTypes != FALSE) {
                    printf("   %d: %s:(%s,%d). Reference Type: %c(%s, %d)",
                           TypesProcessed,
                           TypeName,
                           CurrentType->ParentSource->SourceFile,
                           CurrentType->TypeNumber,
                           PointerCharacter,
                           Relation->OwningFile->SourceFile,
                           Relation->TypeNumber);

                    if (Relation->Function != FALSE) {
                        printf(" FUNCTION");
                    }
                }

                if ((Relation->Array.Minimum != 0LL) ||
                    (Relation->Array.Maximum != 0LL)) {

                    if (PrintTypes != FALSE) {
                        printf(" Array [%I64i, %I64i]",
                               Relation->Array.Minimum,
                               Relation->Array.Maximum);
                    }

                }

                if (PrintTypes != FALSE) {
                    printf("\n");
                }

                RelativeType = DbgGetType(Relation->OwningFile,
                                          Relation->TypeNumber);

                if (RelativeType == NULL) {
                    printf("Error: Unable to resolve relation type (%s, %d).\n",
                           Relation->OwningFile->SourceFile,
                           Relation->TypeNumber);

                    assert(RelativeType != NULL);

                }

                break;

            case DataTypeNumeric:
                Numeric = (PDATA_TYPE_NUMERIC)CurrentType->Data;
                if (PrintTypes != FALSE) {
                    printf("   %d: %s:(%s,%d). Numeric: %d bits, ",
                          TypesProcessed,
                          TypeName,
                          CurrentType->ParentSource->SourceFile,
                          CurrentType->TypeNumber,
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
                Structure = (PDATA_TYPE_STRUCTURE)CurrentType->Data;
                MemberCount = 0;
                if (PrintTypes != FALSE) {
                    printf("   %d: %s:(%s,%d). Structure: %d Bytes, %d "
                           "Members\n",
                           TypesProcessed,
                           TypeName,
                           CurrentType->ParentSource->SourceFile,
                           CurrentType->TypeNumber,
                           Structure->SizeInBytes,
                           Structure->MemberCount);
                }

                if (Structure != NULL) {
                    CurrentStructureMember = Structure->FirstMember;

                    assert((CurrentStructureMember == NULL) ||
                           (CurrentStructureMember->TypeFile != NULL));

                } else {
                    CurrentStructureMember = NULL;
                }

                while (CurrentStructureMember != NULL) {
                    if (PrintTypes != FALSE) {
                        printf("      +%d, %d: %s (%s, %d)\n",
                               CurrentStructureMember->BitOffset,
                               CurrentStructureMember->BitSize,
                               CurrentStructureMember->Name,
                               CurrentStructureMember->TypeFile->SourceFile,
                               CurrentStructureMember->TypeNumber);
                    }

                    MemberType = DbgGetType(CurrentStructureMember->TypeFile,
                                            CurrentStructureMember->TypeNumber);

                    if (MemberType == NULL) {
                        printf("Error: Unable to resolve structure member "
                               "type from (%s, %d).\n",
                               CurrentStructureMember->TypeFile,
                               CurrentStructureMember->TypeNumber);

                        assert(MemberType != NULL);

                    }

                    MemberCount += 1;
                    CurrentStructureMember = CurrentStructureMember->NextMember;
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
                Enumeration = (PDATA_TYPE_ENUMERATION)CurrentType->Data;
                MemberCount = 0;
                if (PrintTypes != FALSE) {
                    printf("   %d: %s:(%s,%d). Enumeration: %d Members\n",
                           TypesProcessed,
                           TypeName,
                           CurrentType->ParentSource->SourceFile,
                           CurrentType->TypeNumber,
                           Enumeration->MemberCount);
                }

                assert(Enumeration->FirstMember != NULL);

                CurrentEnumerationMember = Enumeration->FirstMember;
                while (CurrentEnumerationMember != NULL) {

                    assert(CurrentEnumerationMember->Name != NULL);

                    if (PrintTypes != FALSE) {
                        printf("      %s = %I64d\n",
                               CurrentEnumerationMember->Name,
                               CurrentEnumerationMember->Value);
                    }

                    MemberCount += 1;
                    CurrentEnumerationMember =
                                        CurrentEnumerationMember->NextMember;
                }

                if (MemberCount != Enumeration->MemberCount) {
                    printf("   ***ERROR: Enumeration Member Count does not "
                           "match actual number of structure members. "
                           "Enumeration reported %d, but %d were found.***\n",
                           Enumeration->MemberCount,
                           MemberCount);
                }

                break;

            default:
                printf("Unknown type %d for symbol (%s, %d)\n",
                       CurrentType->Type,
                       CurrentType->ParentSource->SourceFile,
                       CurrentType->TypeNumber);

                assert(FALSE);
            }

            CurrentTypeEntry = CurrentTypeEntry->Next;
            TypesProcessed += 1;
        }

        SourceFilesProcessed += 1;
        CurrentSourceEntry = CurrentSourceEntry->Next;
    }

    //
    // Search for symbols in the module.
    //

    if (QueryString != NULL) {
        printf("\nSearching through data symbols\n");
    }

    SearchResult.Variety = SymbolResultInvalid;
    FunctionsProcessed = 0;
    while (TRUE) {
        ResultValid = DbgpFindSymbolInModule(Symbols,
                                             QueryString,
                                             &SearchResult);

        if (ResultValid == NULL) {
            break;
        }

        switch (SearchResult.Variety) {
        case SymbolResultType:
            TypeSize = DbgGetTypeSize(SearchResult.TypeResult, 0);
            printf("%d Type: ", FunctionsProcessed);
            DbgPrintTypeName(SearchResult.TypeResult);
            printf(" (size: %d) = ", TypeSize);
            DbgPrintTypeDescription(SearchResult.TypeResult, 4, 10);
            printf("\n");
            break;

        case SymbolResultData:
            printf("%d Data Symbol: %s in %s%s \t\t0x%I64x\n",
                   FunctionsProcessed,
                   SearchResult.DataResult->Name,
                   SearchResult.DataResult->ParentSource->SourceDirectory,
                   SearchResult.DataResult->ParentSource->SourceFile,
                   SearchResult.DataResult->Address);

            break;

        case SymbolResultFunction:
            printf("%d Function Symbol: %s in %s%s \t\t0x%I64x - 0x%I64x\n",
                   FunctionsProcessed,
                   SearchResult.FunctionResult->Name,
                   SearchResult.FunctionResult->ParentSource->SourceDirectory,
                   SearchResult.FunctionResult->ParentSource->SourceFile,
                   SearchResult.FunctionResult->StartAddress,
                   SearchResult.FunctionResult->EndAddress);

            printf("\t");
            DbgPrintFunctionPrototype(SearchResult.FunctionResult, NULL, 0);
            printf("\n");
            break;

        default:
            printf("INVALID RESULT\n");
            break;
        }

        FunctionsProcessed += 1;
        if (FunctionsProcessed >= 1000) {
            break;
        }
    }

    //
    // Search based on address.
    //

    if (QueryAddress != NULL) {
        printf("Searching by address\n");
        SearchAddress = strtoull(QueryAddress, NULL, 0);
        if (SearchAddress == 0) {
            printf("Warning: Address was probably not parsed. Searching at "
                   "0.\n");
        }

        SearchResult.Variety = SymbolResultInvalid;
        while (DbgLookupSymbol(Symbols, SearchAddress, &SearchResult) != NULL) {
            switch (SearchResult.Variety) {
            case SymbolResultData:

                assert(SearchResult.DataResult->Location ==
                       DataLocationAbsoluteAddress);

                printf("Data matched 0x%I64x: %s in %s%s at 0x%I64x\n",
                       SearchAddress,
                       SearchResult.DataResult->Name,
                       SearchResult.DataResult->ParentSource->SourceDirectory,
                       SearchResult.DataResult->ParentSource->SourceFile,
                       SearchResult.DataResult->Address);

                break;

            case SymbolResultFunction:
                printf(
                    "Function matched 0x%I64x: %s in %s%s at 0x%I64x - "
                    "0x%I64x\n",
                    SearchAddress,
                    SearchResult.FunctionResult->Name,
                    SearchResult.FunctionResult->ParentSource->SourceDirectory,
                    SearchResult.FunctionResult->ParentSource->SourceFile,
                    SearchResult.FunctionResult->StartAddress,
                    SearchResult.FunctionResult->EndAddress);

                break;

            default:
                printf("INVALID RESULT\n");
                break;
            }
        }

        //
        // Get a source line based on an address.
        //

        SourceLine = DbgLookupSourceLine(Symbols, SearchAddress);
        if (SourceLine != NULL) {
            printf("Address 0x%I64x: at %s, Line %d.\n",
                   SearchAddress,
                   SourceLine->ParentSource->SourceFile,
                   SourceLine->LineNumber);
        }
    }

    Result = TRUE;

MainEnd:
    if (Symbols != NULL) {
        if (PrintVerbose != FALSE) {
            printf("\nCleaning up...");
        }

        DbgFreeSymbols(Symbols);
        if (PrintVerbose != FALSE) {
            printf("Done!\n");
        }
    }

    if (Result == FALSE) {
        return -1;
    }

    printf("Stabs test passed.\n");
    return 0;
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
