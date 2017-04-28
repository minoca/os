/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwread.c

Abstract:

    This module implements support for reading DWARF structures.

Author:

    Evan Green 4-Dec-2015

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

INT
DwarfpIndexAbbreviations (
    PDWARF_CONTEXT Context,
    ULONGLONG Offset,
    PUCHAR **AbbreviationsIndex,
    PUINTN IndexSize,
    PUINTN MaxAttributes
    );

INT
DwarfpReadDie (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit,
    PUCHAR *Data,
    PUCHAR Abbreviation,
    PDWARF_DIE Die
    );

INT
DwarfpReadFormValue (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit,
    PUCHAR *Data,
    PDWARF_ATTRIBUTE_VALUE Attribute
    );

VOID
DwarfpPrintFormValue (
    PDWARF_ATTRIBUTE_VALUE Attribute
    );

PSTR
DwarfpGetTagName (
    DWARF_TAG Tag
    );

PSTR
DwarfpGetAttributeName (
    DWARF_ATTRIBUTE Attribute
    );

PSTR
DwarfpGetFormName (
    DWARF_FORM Form
    );

PSTR
DwarfpGetHasChildrenName (
    DWARF_CHILDREN_VALUE Value
    );

PDWARF_DIE
DwarfpFindDie (
    PDWARF_COMPILATION_UNIT Unit,
    PUCHAR DieStart
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR DwarfTagNames[] = {
    "NullTag",
    "DwarfTagArrayType",
    "DwarfTagClassType",
    "DwarfTagEntryPoint",
    "DwarfTagEnumerationType",
    "DwarfTagFormalParameter",
    NULL,
    NULL,
    "DwarfTagImportedDeclaration",
    NULL,
    "DwarfTagLabel",
    "DwarfTagLexicalBlock",
    NULL,
    "DwarfTagMember",
    NULL,
    "DwarfTagPointerType",
    "DwarfTagReferenceType",
    "DwarfTagCompileUnit",
    "DwarfTagStringType",
    "DwarfTagStructureType",
    NULL,
    "DwarfTagSubroutineType",
    "DwarfTagTypedef",
    "DwarfTagUnionType",
    "DwarfTagUnspecifiedParameters",
    "DwarfTagVariant",
    "DwarfTagCommonBlock",
    "DwarfTagCommonInclusion",
    "DwarfTagInheritance",
    "DwarfTagInlinedSubroutine",
    "DwarfTagModule",
    "DwarfTagPointerToMemberType",
    "DwarfTagSetType",
    "DwarfTagSubrangeType",
    "DwarfTagWithStatement",
    "DwarfTagAccessDeclaration",
    "DwarfTagBaseType",
    "DwarfTagCatchBlock",
    "DwarfTagConstType",
    "DwarfTagConstant",
    "DwarfTagEnumerator",
    "DwarfTagFileType",
    "DwarfTagFriend",
    "DwarfTagNameList",
    "DwarfTagNameListItem",
    "DwarfTagPackedType",
    "DwarfTagSubprogram",
    "DwarfTagTemplateTypeParameter",
    "DwarfTagTemplateValueParameter",
    "DwarfTagThrownType",
    "DwarfTagTryBlock",
    "DwarfTagVariantPart",
    "DwarfTagVariable",
    "DwarfTagVolatileType",
    "DwarfTagDwarfProcedure",
    "DwarfTagRestrictType",
    "DwarfTagInterfaceType",
    "DwarfTagNamespace",
    "DwarfTagImportedModule",
    "DwarfTagUnspecifiedType",
    "DwarfTagPartialUnit",
    "DwarfTagImportedUnit",
    NULL,
    "DwarfTagCondition",
    "DwarfTagSharedType",
    "DwarfTagTypeUnit",
    "DwarfTagRvalueReferenceType",
    "DwarfTagTemplateAlias",
};

PSTR DwarfHasChildrenNames[] = {
    "NoChildren",
    "HasChildren",
};

PSTR DwarfAttributeNames[] = {
    "DwarfAtNull",
    "DwarfAtSibling",
    "DwarfAtLocation",
    "DwarfAtName",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "DwarfAtOrdering",
    NULL,
    "DwarfAtByteSize",
    "DwarfAtBitOffset",
    "DwarfAtBitSize",
    NULL,
    NULL,
    "DwarfAtStatementList",
    "DwarfAtLowPc",
    "DwarfAtHighPc",
    "DwarfAtLanguage",
    NULL,
    "DwarfAtDiscr",
    "DwarfAtDiscrValue",
    "DwarfAtVisibility",
    "DwarfAtImport",
    "DwarfAtStringLength",
    "DwarfAtCommonReference",
    "DwarfAtCompDir",
    "DwarfAtConstValue",
    "DwarfAtContainingType",
    "DwarfAtDefaultValue",
    NULL,
    "DwarfAtInline",
    "DwarfAtIsOptional",
    "DwarfAtLowerBound",
    NULL,
    NULL,
    "DwarfAtProducer",
    NULL,
    "DwarfAtPrototyped",
    NULL,
    NULL,
    "DwarfAtReturnAddress",
    NULL,
    "DwarfAtStartScope",
    NULL,
    "DwarfAtBitStride",
    "DwarfAtUpperBound",
    NULL,
    "DwarfAtAbstractOrigin",
    "DwarfAtAccessibility",
    "DwarfAtAddressClass",
    "DwarfAtArtificial",
    "DwarfAtBaseTypes",
    "DwarfAtCallingConvention",
    "DwarfAtCount",
    "DwarfAtDataMemberLocation",
    "DwarfAtDeclColumn",
    "DwarfAtDeclFile",
    "DwarfAtDeclLine",
    "DwarfAtDeclaration",
    "DwarfAtDiscrList",
    "DwarfAtEncoding",
    "DwarfAtExternal",
    "DwarfAtFrameBase",
    "DwarfAtFriend",
    "DwarfAtIdentifierCase",
    "DwarfAtMacroInfo",
    "DwarfAtNameListItem",
    "DwarfAtPriority",
    "DwarfAtSegment",
    "DwarfAtSpecification",
    "DwarfAtStaticLink",
    "DwarfAtType",
    "DwarfAtUseLocation",
    "DwarfAtVariableParameter",
    "DwarfAtVirtuality",
    "DwarfAtVtableElementLocation",
    "DwarfAtAllocated",
    "DwarfAtAssociated",
    "DwarfAtDataLocation",
    "DwarfAtByteStride",
    "DwarfAtEntryPc",
    "DwarfAtUseUtf8",
    "DwarfAtExtension",
    "DwarfAtRanges",
    "DwarfAtTrampoline",
    "DwarfAtCallColumn",
    "DwarfAtCallFile",
    "DwarfAtCallLine",
    "DwarfAtDescription",
    "DwarfAtBinaryScale",
    "DwarfAtDecimalScale",
    "DwarfAtSmall",
    "DwarfAtDecimalSign",
    "DwarfAtDigitCount",
    "DwarfAtPictureString",
    "DwarfAtMutable",
    "DwarfAtThreadsScaled",
    "DwarfAtExplicit",
    "DwarfAtObjectPointer",
    "DwarfAtEndianity",
    "DwarfAtElemental",
    "DwarfAtPure",
    "DwarfAtRecursive",
    "DwarfAtSignature",
    "DwarfAtMainSubprogram",
    "DwarfAtDataBitOffset",
    "DwarfAtConstExpression",
    "DwarfAtEnumClass",
    "DwarfAtLinkageName",
};

PSTR DwarfFormNames[] = {
    "DwarfFormNull",
    "DwarfFormAddress",
    NULL,
    "DwarfFormBlock2",
    "DwarfFormBlock4",
    "DwarfFormData2",
    "DwarfFormData4",
    "DwarfFormData8",
    "DwarfFormString",
    "DwarfFormBlock",
    "DwarfFormBlock1",
    "DwarfFormData1",
    "DwarfFormFlag",
    "DwarfFormSData",
    "DwarfFormStringPointer",
    "DwarfFormUData",
    "DwarfFormRefAddress",
    "DwarfFormRef1",
    "DwarfFormRef2",
    "DwarfFormRef4",
    "DwarfFormRef8",
    "DwarfFormRefUData",
    "DwarfFormIndirect",
    "DwarfFormSecOffset",
    "DwarfFormExprLoc",
    "DwarfFormFlagPresent",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "DwarfFormRefSig8"
};

//
// ------------------------------------------------------------------ Functions
//

VOID
DwarfpReadCompilationUnit (
    PUCHAR *Data,
    PULONGLONG Size,
    PDWARF_COMPILATION_UNIT Unit
    )

/*++

Routine Description:

    This routine reads a DWARF compilation unit header, and pieces it out into
    a structure.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the header.
        On output this pointer will be advanced past the header and the DIEs,
        meaning it will point at the next compilation unit.

    Size - Supplies a pointer that on input contains the size of the section.
        On output this will be decreased by the amount that the data was
        advanced.

    Unit - Supplies a pointer where the header information will be filled in.

Return Value:

    None.

--*/

{

    ULONGLONG Delta;
    ULONGLONG RemainingSize;

    Unit->Start = *Data;
    RemainingSize = *Size;
    DwarfpReadInitialLength(Data, &(Unit->Is64Bit), &(Unit->UnitLength));
    Unit->Version = DwarfpRead2(Data);
    Unit->AbbreviationOffset = DWARF_READN(Data, Unit->Is64Bit);
    Unit->AddressSize = DwarfpRead1(Data);
    Unit->Dies = *Data;

    //
    // Advance past the DIEs themselves as well.
    //

    *Data = Unit->Start + Unit->UnitLength + sizeof(ULONG);
    if (Unit->Is64Bit != FALSE) {
        *Data += sizeof(ULONGLONG);
    }

    Unit->DiesEnd = *Data;
    Delta = *Data - Unit->Start;

    assert(Delta <= RemainingSize);

    RemainingSize -= Delta;
    *Size = RemainingSize;
    return;
}

INT
DwarfpLoadCompilationUnit (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit
    )

/*++

Routine Description:

    This routine processes all the DIEs within a DWARF compilation unit.

Arguments:

    Context - Supplies a pointer to the application context.

    Unit - Supplies a pointer to the compilation unit.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PUCHAR Abbreviation;
    DWARF_LEB128 AbbreviationNumber;
    PUCHAR *Abbreviations;
    UINTN AbbreviationsCount;
    UINTN AllocationSize;
    ULONG Depth;
    PDWARF_DIE Die;
    PUCHAR DieBytes;
    PUCHAR End;
    UINTN MaxAttributes;
    PDWARF_DIE Parent;
    INT Status;

    Abbreviations = NULL;
    Depth = 0;
    Die = NULL;
    End = Unit->DiesEnd;
    Parent = NULL;

    //
    // Index the abbreviations for this compilation unit.
    //

    Status = DwarfpIndexAbbreviations(Context,
                                      Unit->AbbreviationOffset,
                                      &Abbreviations,
                                      &AbbreviationsCount,
                                      &MaxAttributes);

    if (Status != 0) {
        goto ProcessCompilationUnitEnd;
    }

    AllocationSize = sizeof(DWARF_DIE) +
                     (MaxAttributes * sizeof(DWARF_ATTRIBUTE_VALUE));

    //
    // Loop through all the DIEs.
    //

    DieBytes = Unit->Dies;
    while (DieBytes < End) {
        Die = malloc(AllocationSize);
        if (Die == NULL) {
            Status = errno;
            goto ProcessCompilationUnitEnd;
        }

        memset(Die, 0, AllocationSize);
        INITIALIZE_LIST_HEAD(&(Die->ChildList));
        Die->Capacity = MaxAttributes;
        Die->Attributes = (PDWARF_ATTRIBUTE_VALUE)(Die + 1);
        Die->Start = DieBytes;
        Die->Depth = Depth;
        AbbreviationNumber = DwarfpReadLeb128(&DieBytes);
        Die->AbbreviationNumber = AbbreviationNumber;
        Die->Parent = Parent;

        //
        // Skip null entries.
        //

        if (AbbreviationNumber == 0) {
            if ((Context->Flags & DWARF_CONTEXT_DEBUG) != 0) {
                DWARF_PRINT("  <%x> Null Entry\n",
                            Die->Start - (PUCHAR)(Context->Sections.Info.Data));
            }

            if (Depth != 0) {
                Depth -= 1;
                Parent = Parent->Parent;
            }

            continue;
        }

        if ((AbbreviationNumber >= AbbreviationsCount) ||
            (Abbreviations[AbbreviationNumber] == NULL)) {

            DWARF_ERROR("DWARF: Bad abbreviation number %I64d\n",
                        AbbreviationNumber);

            Status = EINVAL;
            goto ProcessCompilationUnitEnd;
        }

        Abbreviation = Abbreviations[AbbreviationNumber];
        Status = DwarfpReadDie(Context,
                               Unit,
                               &DieBytes,
                               Abbreviation,
                               Die);

        if (Status != 0) {
            DWARF_ERROR("DWARF: Invalid DIE.\n");
            goto ProcessCompilationUnitEnd;
        }

        if (Parent != NULL) {
            INSERT_BEFORE(&(Die->ListEntry), &(Parent->ChildList));

        } else {
            INSERT_BEFORE(&(Die->ListEntry), &(Unit->DieList));
        }

        if ((Die->Flags & DWARF_DIE_HAS_CHILDREN) != 0) {
            Depth += 1;
            Parent = Die;
        }
    }

    Die = NULL;
    Status = 0;

ProcessCompilationUnitEnd:
    if (Abbreviations != NULL) {
        free(Abbreviations);
    }

    if (Die != NULL) {
        DwarfpDestroyDie(Context, Die);
    }

    return Status;
}

VOID
DwarfpDestroyCompilationUnit (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit
    )

/*++

Routine Description:

    This routine destroys a compilation unit. It's assumed it's already off the
    list.

Arguments:

    Context - Supplies a pointer to the application context.

    Unit - Supplies a pointer to the compilation unit to destroy.

Return Value:

    None.

--*/

{

    PDWARF_DIE Die;

    assert(Unit->ListEntry.Next == NULL);

    while (!LIST_EMPTY(&(Unit->DieList))) {
        Die = LIST_VALUE(Unit->DieList.Next, DWARF_DIE, ListEntry);
        LIST_REMOVE(&(Die->ListEntry));
        Die->ListEntry.Next = NULL;
        DwarfpDestroyDie(Context, Die);
    }

    free(Unit);
    return;
}

VOID
DwarfpDestroyDie (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine destroys a Debug Information Entry.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to destroy.

Return Value:

    None.

--*/

{

    PDWARF_DIE Child;

    assert(Die->ListEntry.Next == NULL);

    while (!LIST_EMPTY(&(Die->ChildList))) {
        Child = LIST_VALUE(Die->ChildList.Next, DWARF_DIE, ListEntry);
        LIST_REMOVE(&(Child->ListEntry));
        Child->ListEntry.Next = NULL;
        DwarfpDestroyDie(Context, Child);
    }

    free(Die);
    return;
}

PSTR
DwarfpGetStringAttribute (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute
    )

/*++

Routine Description:

    This routine returns the given attribute with type string.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

Return Value:

    Returns a pointer to the string on success.

    NULL if no such attribute exists, or its type is not a string.

--*/

{

    PDWARF_ATTRIBUTE_VALUE Value;

    Value = DwarfpGetAttribute(Context, Die, Attribute);
    if ((Value != NULL) &&
        ((Value->Form == DwarfFormString) ||
         (Value->Form == DwarfFormStringPointer))) {

        return Value->Value.String;
    }

    return NULL;
}

BOOL
DwarfpGetAddressAttribute (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PULONGLONG Address
    )

/*++

Routine Description:

    This routine returns the given attribute, ensuring it is of type address.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    Address - Supplies a pointer where the address is returned on success.

Return Value:

    TRUE if an address was retrieved.

    FALSE if no address was retrieved or it was not of type address.

--*/

{

    PDWARF_ATTRIBUTE_VALUE Value;

    Value = DwarfpGetAttribute(Context, Die, Attribute);
    if ((Value != NULL) && (Value->Form == DwarfFormAddress)) {
        *Address = Value->Value.Address;
        return TRUE;
    }

    return FALSE;
}

BOOL
DwarfpGetIntegerAttribute (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PULONGLONG Integer
    )

/*++

Routine Description:

    This routine returns the given attribute, ensuring it is of type integer
    (data or flag).

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    Integer - Supplies a pointer where the integer is returned on success.

Return Value:

    TRUE if an address was retrieved.

    FALSE if no address was retrieved or it was not of type address.

--*/

{

    BOOL Result;
    PDWARF_ATTRIBUTE_VALUE Value;

    Result = FALSE;
    Value = DwarfpGetAttribute(Context, Die, Attribute);
    if (Value != NULL) {
        Result = TRUE;
        switch (Value->Form) {
        case DwarfFormData1:
        case DwarfFormData2:
        case DwarfFormData4:
        case DwarfFormData8:
        case DwarfFormSData:
        case DwarfFormUData:
            *Integer = Value->Value.UnsignedConstant;
            break;

        case DwarfFormFlag:
        case DwarfFormFlagPresent:
            *Integer = Value->Value.Flag;
            break;

        default:
            Result = FALSE;
            break;
        }
    }

    return Result;
}

BOOL
DwarfpGetTypeReferenceAttribute (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PSOURCE_FILE_SYMBOL *File,
    PLONG Identifier
    )

/*++

Routine Description:

    This routine reads a given attribute and converts that reference into a
    symbol type reference tuple.

Arguments:

    Context - Supplies a pointer to the parsing context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    File - Supplies a pointer where a pointer to the file will be returned on
        success.

    Identifier - Supplies a pointer where the type identifier will be returned
        on success.

Return Value:

    TRUE if a value was retrieved.

    FALSE if no value was retrieved or it was not of type reference.

--*/

{

    PDWARF_LOADING_CONTEXT LoadingContext;
    BOOL Result;
    ULONGLONG TypeOffset;

    LoadingContext = Context->LoadingContext;

    assert(LoadingContext != NULL);

    Result = DwarfpGetLocalReferenceAttribute(Context,
                                              Die,
                                              Attribute,
                                              &TypeOffset);

    if (Result != FALSE) {

        assert(LoadingContext->CurrentFile != NULL);

        *File = LoadingContext->CurrentFile;

        //
        // Make the local reference, which is an offset from the start of the
        // compilation unit header, global to the entire debug info section.
        //

        TypeOffset += (PVOID)(LoadingContext->CurrentUnit->Start) -
                      Context->Sections.Info.Data;

        *Identifier = TypeOffset;

    } else {
        Result = DwarfpGetGlobalReferenceAttribute(Context,
                                                   Die,
                                                   DwarfAtType,
                                                   &TypeOffset);

        if (Result == FALSE) {

            //
            // Void types don't have a type attribute.
            //

            *File = NULL;
            *Identifier = -1;
            return TRUE;
        }

        //
        // Consider supporting this type that is global to the whole .debug_info
        // section. The tricky bit is figuring out which source file
        // (compilation unit) the offset belongs to.
        //

        assert(FALSE);

        return FALSE;
    }

    return TRUE;
}

PDWARF_DIE
DwarfpGetDieReferenceAttribute (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute
    )

/*++

Routine Description:

    This routine returns a pointer to the DIE referred to by the given
    attribute.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

Return Value:

    Returns a pointer to the DIE on success.

    NULL on failure.

--*/

{

    PDWARF_LOADING_CONTEXT LoadingContext;
    ULONGLONG Offset;
    BOOL Result;
    PDWARF_DIE ResultDie;

    ResultDie = NULL;
    Result = DwarfpGetLocalReferenceAttribute(Context,
                                              Die,
                                              Attribute,
                                              &Offset);

    if (Result != FALSE) {
        LoadingContext = Context->LoadingContext;
        ResultDie = DwarfpFindDie(LoadingContext->CurrentUnit,
                                  LoadingContext->CurrentUnit->Start + Offset);
    }

    return ResultDie;
}

BOOL
DwarfpGetLocalReferenceAttribute (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PULONGLONG Offset
    )

/*++

Routine Description:

    This routine returns the given attribute, ensuring it is of type reference.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    Offset - Supplies a pointer where the DIE offset is returned on success.

Return Value:

    TRUE if a value was retrieved.

    FALSE if no value was retrieved or it was not of type reference.

--*/

{

    BOOL Result;
    PDWARF_ATTRIBUTE_VALUE Value;

    Result = FALSE;
    Value = DwarfpGetAttribute(Context, Die, Attribute);
    if (Value != NULL) {
        Result = TRUE;
        switch (Value->Form) {
        case DwarfFormRef1:
        case DwarfFormRef2:
        case DwarfFormRef4:
        case DwarfFormRef8:
        case DwarfFormRefUData:
        case DwarfFormData1:
        case DwarfFormData2:
        case DwarfFormData4:
        case DwarfFormData8:
        case DwarfFormUData:
            *Offset = Value->Value.Offset;
            break;

        default:
            Result = FALSE;
            break;
        }
    }

    return Result;
}

BOOL
DwarfpGetGlobalReferenceAttribute (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PULONGLONG Offset
    )

/*++

Routine Description:

    This routine returns the given attribute, ensuring it is of type reference
    address.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    Offset - Supplies a pointer where the DIE offset is returned on success.

Return Value:

    TRUE if a value was retrieved.

    FALSE if no value was retrieved or it was not of type reference.

--*/

{

    PDWARF_ATTRIBUTE_VALUE Value;

    Value = DwarfpGetAttribute(Context, Die, Attribute);
    if ((Value != NULL) && (Value->Form == DwarfFormRefAddress)) {
        *Offset = Value->Value.Offset;
        return TRUE;
    }

    return FALSE;
}

PVOID
DwarfpGetRangeList (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute
    )

/*++

Routine Description:

    This routine looks up the given attribute as a range list pointer.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

Return Value:

    Returns a pointer within the .debug_ranges structure on success.

    NULL if there was no attribute, the attribute was not of the right type, or
    there is no .debug_ranges section.

--*/

{

    PDWARF_LOADING_CONTEXT LoadingContext;
    PDWARF_ATTRIBUTE_VALUE Value;

    LoadingContext = Context->LoadingContext;
    Value = DwarfpGetAttribute(Context, Die, Attribute);
    if ((Value == NULL) ||
        (Value->Value.Offset >= Context->Sections.Ranges.Size)) {

        return NULL;
    }

    if (!DWARF_SECTION_OFFSET_FORM(Value->Form, LoadingContext->CurrentUnit)) {
        return NULL;
    }

    return Context->Sections.Ranges.Data + Value->Value.Offset;
}

PDWARF_ATTRIBUTE_VALUE
DwarfpGetAttribute (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute
    )

/*++

Routine Description:

    This routine returns the requested attribute from a DIE. This will follow
    a Specification attribute if needed.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

Return Value:

    Returns a pointer to the attribute value on success.

    NULL if no such attribute exists.

--*/

{

    UINTN Index;
    PDWARF_DIE Specification;

    for (Index = 0; Index < Die->Count; Index += 1) {
        if (Die->Attributes[Index].Name == Attribute) {
            return &(Die->Attributes[Index]);
        }
    }

    //
    // Avoid infinite recursion.
    //

    if (Attribute == DwarfAtSpecification) {
        return NULL;
    }

    //
    // Try to find a specification attribute. It's currently expensive to find
    // one, so the result is cached.
    //

    Specification = Die->Specification;
    if (Specification == NULL) {

        //
        // See if there's a specification attribute, and go search that DIE if
        // so.
        //

        Specification = DwarfpGetDieReferenceAttribute(Context,
                                                       Die,
                                                       DwarfAtSpecification);

        Die->Specification = Specification;
    }

    //
    // If there's a specification attribute, return all its attributes as if
    // they were present here.
    //

    if (Specification != NULL) {
        return DwarfpGetAttribute(Context, Specification, Attribute);
    }

    return NULL;
}

INT
DwarfpSearchLocationList (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit,
    UINTN Offset,
    ULONGLONG Pc,
    PUCHAR *LocationExpression,
    PUINTN LocationExpressionSize
    )

/*++

Routine Description:

    This routine searches a location list and returns the expression that
    matches the given PC value.

Arguments:

    Context - Supplies a pointer to the DWARF symbol context.

    Unit - Supplies a pointer to the current compilation unit.

    Offset - Supplies the byte offset into the location list section of the
        list to search.

    Pc - Supplies the current PC value to match against.

    LocationExpression - Supplies a pointer where a pointer to the location
        expression that matched will be returned.

    LocationExpressionSize - Supplies a pointer where the size of the location
        expression will be returned on success.

Return Value:

    0 if an expression matched.

    EAGAIN if the locations section is missing.

    ENOENT if none of the entries matched the current PC value.

--*/

{

    ULONGLONG Base;
    PUCHAR Bytes;
    ULONGLONG End;
    USHORT Length;
    ULONGLONG Start;

    Base = Unit->LowPc;
    if (Context->Sections.Locations.Size == 0) {
        DWARF_ERROR("DWARF: Missing .debug_loc section.\n");
        return EAGAIN;
    }

    assert(Context->Sections.Locations.Size > Offset);

    Bytes = Context->Sections.Locations.Data + Offset;
    while (TRUE) {
        if (Unit->AddressSize == 8) {
            Start = DwarfpRead8(&Bytes);
            End = DwarfpRead8(&Bytes);

            //
            // If the start is the max address, then it's a base address update.
            //

            if (Start == MAX_ULONGLONG) {
                Base = End;
                continue;
            }

        } else {

            assert(Unit->AddressSize == 4);

            Start = DwarfpRead4(&Bytes);
            End = DwarfpRead4(&Bytes);
            if (Start == MAX_ULONG) {
                Base = End;
                continue;
            }
        }

        //
        // If the start and end are both zero, this is a termination entry.
        //

        if ((Start == 0) && (End == 0)) {
            break;
        }

        Length = DwarfpRead2(&Bytes);

        //
        // If the PC fits in these bounds, then return the entry contents.
        //

        if ((Pc >= Start + Base) && (Pc < End + Base)) {
            *LocationExpression = Bytes;
            *LocationExpressionSize = Length;
            return 0;
        }

        Bytes += Length;
    }

    return ENOENT;
}

VOID
DwarfpGetRangeSpan (
    PDWARF_CONTEXT Context,
    PVOID Ranges,
    PDWARF_COMPILATION_UNIT Unit,
    PULONGLONG Start,
    PULONGLONG End
    )

/*++

Routine Description:

    This routine runs through a range list to figure out the maximum and
    minimum values.

Arguments:

    Context - Supplies a pointer to the application context.

    Ranges - Supplies the range list pointer.

    Unit - Supplies the current compilation unit.

    Start - Supplies a pointer where the lowest address in the range will be
        returned.

    End - Supplies a pointer where the first address just beyond the range will
        be returned.

Return Value:

    None.

--*/

{

    ULONGLONG Base;
    PUCHAR Bytes;
    BOOL Is64Bit;
    ULONGLONG Max;
    ULONGLONG Min;
    ULONGLONG RangeEnd;
    ULONGLONG RangeStart;

    Bytes = Ranges;
    Min = MAX_ULONGLONG;
    Max = 0;
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

        if (Min > RangeStart + Base) {
            Min = RangeStart + Base;
        }

        if (Max < RangeEnd + Base) {
            Max = RangeEnd + Base;
        }
    }

    *Start = Min;
    *End = Max;
    return;
}

DWARF_LEB128
DwarfpReadLeb128 (
    PUCHAR *Data
    )

/*++

Routine Description:

    This routine reads a DWARF unsigned LEB128 variable length encoded value.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

{

    UCHAR Byte;
    DWARF_LEB128 Result;
    ULONG Shift;

    //
    // LEB128 numbers encode 7 bits in each byte, with the upper bit signifying
    // whether there are more bytes (1) or this is the last byte (0).
    //

    Result = 0;
    Shift = 0;
    while (TRUE) {
        Byte = DwarfpRead1(Data);
        Result |= ((ULONGLONG)(Byte & 0x7F)) << Shift;
        Shift += 7;
        if ((Byte & 0x80) == 0) {
            break;
        }
    }

    //
    // Anything larger than a 64-bit integer would overflow the current
    // data representation. 64 doesn't divide by 7, so move to the next integer
    // that does.
    //

    assert(Shift <= 70);

    return Result;
}

DWARF_SLEB128
DwarfpReadSleb128 (
    PUCHAR *Data
    )

/*++

Routine Description:

    This routine reads a DWARF signed LEB128 variable length encoded value.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

{

    UCHAR Byte;
    DWARF_SLEB128 Result;
    ULONG Shift;

    //
    // LEB128 numbers encode 7 bits in each byte, with the upper bit signifying
    // whether there are more bytes (1) or this is the last byte (0).
    //

    Result = 0;
    Shift = 0;
    while (TRUE) {
        Byte = DwarfpRead1(Data);
        Result |= ((ULONGLONG)(Byte & 0x7F)) << Shift;
        Shift += 7;
        if ((Byte & 0x80) == 0) {
            break;
        }
    }

    //
    // If the high order bit of the last byte is set, sign extend to the
    // remainder of the value.
    //

    if ((Shift < (sizeof(DWARF_SLEB128) * BITS_PER_BYTE)) &&
        ((Byte & 0x40) != 0)) {

        Result |= -(1LL << Shift);
    }

    //
    // Anything larger than a 64-bit integer would overflow the current
    // data representation. 64 doesn't divide by 7, so move to the next integer
    // that does.
    //

    assert(Shift <= 70);

    return Result;
}

VOID
DwarfpReadInitialLength (
    PUCHAR *Data,
    PBOOL Is64Bit,
    PULONGLONG Value
    )

/*++

Routine Description:

    This routine reads an initial length member from a DWARF header. The
    initial length is either 32-bits for 32-bit sections, or 96-bits for
    64-bit sections.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the header.
        On output this pointer will be advanced past the initial length.

    Is64Bit - Supplies a pointer where a boolean will be returned indicating
        whether or not this is a 64-bit section. Most sections, even in 64-bit
        code, are not.

    Value - Supplies a pointer where the actual initial length value will be
        returned.

Return Value:

    None.

--*/

{

    *Is64Bit = FALSE;
    *Value = DwarfpRead4(Data);
    if (*Value == 0xFFFFFFFF) {
        *Is64Bit = TRUE;
        *Value = DwarfpRead8(Data);
    }

    return;
}

UCHAR
DwarfpRead1 (
    PUCHAR *Data
    )

/*++

Routine Description:

    This routine reads a byte from the DWARF data stream and advances the
    stream.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

{

    UCHAR Value;

    Value = *(*Data);
    *Data += sizeof(UCHAR);
    return Value;
}

USHORT
DwarfpRead2 (
    PUCHAR *Data
    )

/*++

Routine Description:

    This routine reads two bytes from the DWARF data stream and advances the
    stream.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

{

    USHORT Value;

    Value = *((PUSHORT)*Data);
    *Data += sizeof(USHORT);
    return Value;
}

ULONG
DwarfpRead4 (
    PUCHAR *Data
    )

/*++

Routine Description:

    This routine reads four bytes from the DWARF data stream and advances the
    stream.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

{

    ULONG Value;

    Value = *((PULONG)*Data);
    *Data += sizeof(ULONG);
    return Value;
}

ULONGLONG
DwarfpRead8 (
    PUCHAR *Data
    )

/*++

Routine Description:

    This routine reads eight bytes from the DWARF data stream and advances the
    stream.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

{

    ULONGLONG Value;

    Value = *((PULONGLONG)*Data);
    *Data += sizeof(ULONGLONG);
    return Value;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DwarfpIndexAbbreviations (
    PDWARF_CONTEXT Context,
    ULONGLONG Offset,
    PUCHAR **AbbreviationsIndex,
    PUINTN IndexSize,
    PUINTN MaxAttributes
    )

/*++

Routine Description:

    This routine creates an array of pointers to abbreviation numbers for
    the abbreviations in a compilation unit. The index makes abbreviation
    lookup instant instead of O(N).

Arguments:

    Context - Supplies a pointer to tha parsing context.

    Offset - Supplies the offset into the abbreviation section where
        abbreviations for this compilation unit begin.

    AbbreviationsIndex - Supplies a pointer where an array of pointers will be
        returned, indexed by abbreviation number. The caller is responsible
        for freeing this memory.

    IndexSize - Supplies a pointer where the number of elements in the array
        will be returned.

    MaxAttributes - Supplies a pointer where the maximum number of attributes
        in any DIE template found will be returned.

Return Value:

    0 on success.

    ENOMEM on failure.

--*/

{

    PUCHAR *Array;
    PUCHAR Bytes;
    UINTN Count;
    DWARF_LEB128 Form;
    UCHAR HasChildren;
    UINTN MaxIndex;
    DWARF_LEB128 Name;
    PVOID NewArray;
    UINTN NewSize;
    DWARF_LEB128 Number;
    UINTN Size;
    DWARF_LEB128 Tag;

    *MaxAttributes = 0;
    Bytes = Context->Sections.Abbreviations.Data + Offset;
    if (Offset >= Context->Sections.Abbreviations.Size) {
        return ERANGE;
    }

    MaxIndex = 0;
    Size = 64;
    Array = malloc(Size * sizeof(PVOID));
    if (Array == NULL) {
        return ENOMEM;
    }

    memset(Array, 0, Size * sizeof(PVOID));
    if ((Context->Flags & DWARF_CONTEXT_DEBUG_ABBREVIATIONS) != 0) {
        DWARF_PRINT("Abbreviations at offset %I64x:\n", Offset);
    }

    //
    // Loop processing abbreviations.
    //

    while (TRUE) {
        Number = DwarfpReadLeb128(&Bytes);
        if (Number == 0) {
            break;
        }

        //
        // Reallocate the array if needed.
        //

        if (Number >= Size) {
            NewSize = Size * 2;
            while (NewSize <= Number) {
                NewSize *= 2;
            }

            NewArray = realloc(Array, NewSize * sizeof(PVOID));
            if (NewArray == NULL) {
                free(Array);
                return ENOMEM;
            }

            memset(NewArray + (Size * sizeof(PVOID)),
                   0,
                   (NewSize - Size) * sizeof(PVOID));

            Size = NewSize;
            Array = NewArray;
        }

        //
        // Abbreviations should not collide.
        //

        assert(Array[Number] == NULL);

        Array[Number] = Bytes;
        if (Number > MaxIndex) {
            MaxIndex = Number;
        }

        //
        // Get past the tag and the children byte.
        //

        Tag = DwarfpReadLeb128(&Bytes);
        HasChildren = DwarfpRead1(&Bytes);
        if ((Context->Flags & DWARF_CONTEXT_DEBUG_ABBREVIATIONS) != 0) {
            DWARF_PRINT("  %I64d: %s %s\n",
                        Number,
                        DwarfpGetTagName(Tag),
                        DwarfpGetHasChildrenName(HasChildren));
        }

        //
        // Now get past the attributes specifications.
        //

        Count = 0;
        while (TRUE) {
            Name = DwarfpReadLeb128(&Bytes);
            Form = DwarfpReadLeb128(&Bytes);
            if ((Name == 0) && (Form == 0)) {
                break;
            }

            if ((Context->Flags & DWARF_CONTEXT_DEBUG_ABBREVIATIONS) != 0) {
                DWARF_PRINT("    %s %s\n",
                            DwarfpGetAttributeName(Name),
                            DwarfpGetFormName(Form));
            }

            Count += 1;
        }

        if (Count > *MaxAttributes) {
            *MaxAttributes = Count;
        }
    }

    *AbbreviationsIndex = Array;
    *IndexSize = MaxIndex + 1;
    return 0;
}

INT
DwarfpReadDie (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit,
    PUCHAR *Data,
    PUCHAR Abbreviation,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine reads a single Debug Information Unit using the abbreviation
    template.

Arguments:

    Context - Supplies a pointer to the application context.

    Unit - Supplies a pointer to the compilation unit.

    Data - Supplies a pointer that on input contains a pointer to the values.
        On output this pointer will be advanced past the contents.

    Abbreviation - Supplies a pointer to the abbreviation, which marks out the
        form of the data.

    Die - Supplies a pointer where the DIE is returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDWARF_ATTRIBUTE_VALUE Attribute;
    DWARF_FORM Form;
    DWARF_CHILDREN_VALUE HasChildren;
    DWARF_ATTRIBUTE Name;
    INT Status;
    DWARF_LEB128 Tag;

    //
    // Get past the tag and the children byte.
    //

    Tag = DwarfpReadLeb128(&Abbreviation);
    Die->Tag = Tag;
    HasChildren = DwarfpRead1(&Abbreviation);
    if ((Context->Flags & DWARF_CONTEXT_DEBUG) != 0) {
        DWARF_PRINT("  <%x><%x> %s %s Abbrev. %I64d\n",
                    Die->Depth,
                    (PVOID)(Die->Start) - Context->Sections.Info.Data,
                    DwarfpGetTagName(Tag),
                    DwarfpGetHasChildrenName(HasChildren),
                    Die->AbbreviationNumber);
    }

    if (HasChildren == DwarfChildrenYes) {
        Die->Flags |= DWARF_DIE_HAS_CHILDREN;
    }

    Attribute = &(Die->Attributes[Die->Count]);

    //
    // Read in each attribute value.
    //

    while (TRUE) {
        Name = DwarfpReadLeb128(&Abbreviation);
        Form = DwarfpReadLeb128(&Abbreviation);
        if ((Name == 0) && (Form == 0)) {
            break;
        }

        assert(Die->Count < Die->Capacity);

        Attribute->Name = Name;
        Attribute->Form = Form;
        if ((Context->Flags & DWARF_CONTEXT_DEBUG) != 0) {
            DWARF_PRINT("    <%x> %s : ",
                        *Data - (PUCHAR)(Context->Sections.Info.Data),
                        DwarfpGetAttributeName(Name));
        }

        Status = DwarfpReadFormValue(Context, Unit, Data, Attribute);
        if (Status != 0) {
            DWARF_ERROR("DWARF: Failed to read attribute.\n");
            continue;
        }

        if ((Context->Flags & DWARF_CONTEXT_DEBUG) != 0) {
            DwarfpPrintFormValue(Attribute);

            //
            // Print the expression if it's an expression or it's a block and
            // has a known name.
            //

            if ((Attribute->Form == DwarfFormExprLoc) ||
                ((DWARF_BLOCK_FORM(Attribute->Form)) &&
                 ((Attribute->Name == DwarfAtDataLocation) ||
                  (Attribute->Name == DwarfAtDataMemberLocation)))) {

                DWARF_PRINT(" (");
                DwarfpPrintExpression(Context,
                                      Unit->AddressSize,
                                      Unit,
                                      Attribute->Value.Block.Data,
                                      Attribute->Value.Block.Size);

                DWARF_PRINT(")");
            }

            DWARF_PRINT("\n");
        }

        Die->Count += 1;
        Attribute += 1;
    }

    return 0;
}

INT
DwarfpReadFormValue (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit,
    PUCHAR *Data,
    PDWARF_ATTRIBUTE_VALUE Attribute
    )

/*++

Routine Description:

    This routine reads a DWARF attribute based on its form.

Arguments:

    Context - Supplies a pointer to the application context.

    Unit - Supplies a pointer to the compilation unit.

    Data - Supplies a pointer that on input contains a pointer to the values.
        On output this pointer will be advanced past the contents.

    Attribute - Supplies a pointer to the attribute to read the value for. The
        form is expected to have already been filled in.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;
    PDWARF_FORM_VALUE Value;

    Status = 0;
    Value = &(Attribute->Value);
    switch (Attribute->Form) {
    case DwarfFormAddress:
        if (Unit->AddressSize == 8) {
            Value->Address = DwarfpRead8(Data);

        } else {

            assert(Unit->AddressSize == 4);

            Value->Address = DwarfpRead4(Data);
        }

        break;

    //
    // This form is just a generic block of bytes, but its size can be defined
    // in a number of different ways.
    //

    case DwarfFormBlock1:
    case DwarfFormBlock2:
    case DwarfFormBlock4:
    case DwarfFormBlock:
        switch (Attribute->Form) {
        case DwarfFormBlock1:
            Value->Block.Size = DwarfpRead1(Data);
            break;

        case DwarfFormBlock2:
            Value->Block.Size = DwarfpRead2(Data);
            break;

        case DwarfFormBlock4:
            Value->Block.Size = DwarfpRead4(Data);
            break;

        case DwarfFormBlock:
            Value->Block.Size = DwarfpReadLeb128(Data);
            break;

        default:

            assert(FALSE);

            break;
        }

        Value->Block.Data = *Data;
        *Data += Value->Block.Size;
        break;

    //
    // Then there are the constants.
    //

    case DwarfFormData1:
        Value->UnsignedConstant = DwarfpRead1(Data);
        break;

    case DwarfFormData2:
        Value->UnsignedConstant = DwarfpRead2(Data);
        break;

    case DwarfFormData4:
        Value->UnsignedConstant = DwarfpRead4(Data);
        break;

    case DwarfFormData8:
    case DwarfFormRefSig8:
        Value->UnsignedConstant = DwarfpRead8(Data);
        break;

    case DwarfFormSData:
        Value->SignedConstant = DwarfpReadSleb128(Data);
        break;

    case DwarfFormUData:
        Value->UnsignedConstant = DwarfpReadLeb128(Data);
        break;

    //
    // The expression location form uses the same members as the block. It
    // represents a DWARF expression.
    //

    case DwarfFormExprLoc:
        Value->Block.Size = DwarfpReadLeb128(Data);
        Value->Block.Data = *Data;
        *Data += Value->Block.Size;
        break;

    //
    // Handle the flag forms.
    //

    case DwarfFormFlag:
        Value->Flag = DwarfpRead1(Data);
        break;

    case DwarfFormFlagPresent:
        Value->Flag = TRUE;
        break;

    //
    // The pointers to other sections all look the same from a data perspective.
    //

    case DwarfFormSecOffset:
    case DwarfFormRefAddress:
        Value->Offset = DWARF_READN(Data, Unit->Is64Bit);
        break;

    //
    // Handle references to other DIEs within this compilation unit.
    //

    case DwarfFormRef1:
        Value->Offset = DwarfpRead1(Data);
        break;

    case DwarfFormRef2:
        Value->Offset = DwarfpRead2(Data);
        break;

    case DwarfFormRef4:
        Value->Offset = DwarfpRead4(Data);
        break;

    case DwarfFormRef8:
        Value->Offset = DwarfpReadLeb128(Data);
        break;

    case DwarfFormRefUData:
        Value->Offset = DwarfpReadLeb128(Data);
        break;

    case DwarfFormString:
        Value->String = (PSTR)*Data;
        *Data += strlen((PSTR)*Data) + 1;
        break;

    case DwarfFormStringPointer:
        Value->Offset = DWARF_READN(Data, Unit->Is64Bit);

        assert(Value->Offset < Context->Sections.Strings.Size);

        Value->String = Context->Sections.Strings.Data + Value->Offset;
        break;

    default:
        DWARF_ERROR("DWARF: Unknown form %d.\n", Attribute->Form);
        Status = EINVAL;
        break;
    }

    return Status;
}

VOID
DwarfpPrintFormValue (
    PDWARF_ATTRIBUTE_VALUE Attribute
    )

/*++

Routine Description:

    This routine prints a form value.

Arguments:

    Attribute - Supplies a pointer to the attribute, whose form and value
        should already be filled in.

Return Value:

    None.

--*/

{

    PUCHAR Bytes;
    UINTN Index;
    PDWARF_FORM_VALUE Value;

    Value = &(Attribute->Value);
    switch (Attribute->Form) {
    case DwarfFormAddress:
        DWARF_PRINT("%I64x", Value->Address);
        break;

    //
    // This form is just a generic block of bytes, but its size can be defined
    // in a number of different ways.
    //

    case DwarfFormBlock1:
    case DwarfFormBlock2:
    case DwarfFormBlock4:
    case DwarfFormBlock:
        DWARF_PRINT("%I64u byte block: ", Value->Block.Size);
        Bytes = Value->Block.Data;
        for (Index = 0; Index < Value->Block.Size; Index += 1) {
            DWARF_PRINT("%02x ", Bytes[Index]);
        }

        break;

    //
    // Then there are the constants.
    //

    case DwarfFormData1:
    case DwarfFormData2:
    case DwarfFormData4:
    case DwarfFormData8:
    case DwarfFormUData:
        DWARF_PRINT("%I64u", Value->UnsignedConstant);
        break;

    case DwarfFormRefSig8:
        DWARF_PRINT("TypeSig %I64x", Value->TypeSignature);
        break;

    case DwarfFormSData:
        DWARF_PRINT("%+I64d", Value->SignedConstant);
        break;

    //
    // The expression location form uses the same members as the block. It
    // represents a DWARF expression.
    //

    case DwarfFormExprLoc:
        DWARF_PRINT("%I64u byte expression: ", Value->Block.Size);
        Bytes = Value->Block.Data;
        for (Index = 0; Index < Value->Block.Size; Index += 1) {
            DWARF_PRINT("%02x ", Bytes[Index]);
        }

        break;

    //
    // Handle the flag forms.
    //

    case DwarfFormFlag:
    case DwarfFormFlagPresent:
        DWARF_PRINT("%d", Value->Flag);
        break;

    //
    // The pointers to other sections all look the same from a data perspective.
    //

    case DwarfFormSecOffset:
        DWARF_PRINT("SectionOffset %I64x", Value->Offset);
        break;

    case DwarfFormRefAddress:
        DWARF_PRINT("RefAddress %I64x", Value->Offset);
        break;

    case DwarfFormRef1:
    case DwarfFormRef2:
    case DwarfFormRef4:
    case DwarfFormRef8:
    case DwarfFormRefUData:
        DWARF_PRINT("<%I64x>", Value->Offset);
        break;

    case DwarfFormString:
    case DwarfFormStringPointer:
        DWARF_PRINT("\"%s\"", Value->String);
        break;

    default:
        DWARF_ERROR("DWARF: Unknown form %d.\n", Attribute->Form);
        break;
    }

    return;
}

PSTR
DwarfpGetTagName (
    DWARF_TAG Tag
    )

/*++

Routine Description:

    This routine returns the string description of a given dwarf tag.

Arguments:

    Tag - Supplies the tag to convert.

Return Value:

    Returns the tag name string.

--*/

{

    PSTR Name;

    Name = NULL;
    if (Tag <= DwarfTagTemplateAlias) {
        Name = DwarfTagNames[Tag];

    } else if ((Tag >= DwarfTagLowUser) && (Tag <= DwarfTagHighUser)) {
        Name = "DwarfTagUser";
    }

    if (Name == NULL) {
        Name = "DwarfTagUNKNOWN";
    }

    return Name;
}

PSTR
DwarfpGetAttributeName (
    DWARF_ATTRIBUTE Attribute
    )

/*++

Routine Description:

    This routine returns the string description of a given dwarf attribute.

Arguments:

    Attribute - Supplies the attribute to convert.

Return Value:

    Returns the attribute name string.

--*/

{

    PSTR Name;

    Name = NULL;
    if (Attribute <= DwarfAtLinkageName) {
        Name = DwarfAttributeNames[Attribute];

    } else if ((Attribute >= DwarfAtLowUser) &&
               (Attribute <= DwarfAtHighUser)) {

        Name = "DwarfAtUser";
    }

    if (Name == NULL) {
        Name = "DwarfAtUNKNOWN";
    }

    return Name;
}

PSTR
DwarfpGetFormName (
    DWARF_FORM Form
    )

/*++

Routine Description:

    This routine returns the string description of a given dwarf form.

Arguments:

    Form - Supplies the form to convert.

Return Value:

    Returns the form name string.

--*/

{

    PSTR Name;

    Name = NULL;
    if (Form <= DwarfFormRefSig8) {
        Name = DwarfFormNames[Form];
    }

    if (Name == NULL) {
        Name = "DwarfFormUNKNOWN";
    }

    return Name;
}

PSTR
DwarfpGetHasChildrenName (
    DWARF_CHILDREN_VALUE Value
    )

/*++

Routine Description:

    This routine returns the string description of a given dwarf form.

Arguments:

    Value - Supplies the value to convert.

Return Value:

    Returns the has children name string.

--*/

{

    if (Value <= DwarfChildrenYes) {
        return DwarfHasChildrenNames[Value];
    }

    return "DwarfChildrenINVALID";
}

PDWARF_DIE
DwarfpFindDie (
    PDWARF_COMPILATION_UNIT Unit,
    PUCHAR DieStart
    )

/*++

Routine Description:

    This routine finds a pointer to the DIE that starts at the given offset.

Arguments:

    Unit - Supplies a pointer to the compilation unit to search through.

    DieStart - Supplies the start of raw DIE.

Return Value:

    Returns a pointer to the read DIE on success.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDWARF_DIE Die;
    PLIST_ENTRY ListHead;

    if ((DieStart < Unit->Dies) || (DieStart >= Unit->DiesEnd)) {
        return NULL;
    }

    //
    // Search backwards through the list. The tree is sorted by offset, so the
    // DIE is going to be in the first element of each list whose start is not
    // greater than the pointer. This search is linear in the worst case where
    // the tree is one long chain of elements, and log(n) in the best case
    // where the tree is nicely balanced with evenly thick branches.
    //

    ListHead = &(Unit->DieList);
    while (!LIST_EMPTY(ListHead)) {

        //
        // Find the element with the largest start that is less than or equal
        // to the DIE being searched for.
        //

        CurrentEntry = ListHead->Previous;
        while (CurrentEntry != ListHead) {
            Die = LIST_VALUE(CurrentEntry, DWARF_DIE, ListEntry);
            if (Die->Start <= DieStart) {

                //
                // Return if this is the DIE being searched for.
                //

                if (Die->Start == DieStart) {
                    return Die;
                }

                break;
            }

            CurrentEntry = CurrentEntry->Previous;
        }

        //
        // In the unexpected case where all DIEs are greater than the one
        // being searched for (which shouldn't happen), bail.
        //

        if (CurrentEntry == ListHead) {

            assert(FALSE);

            break;
        }

        //
        // Now search in all the children of this DIE.
        //

        ListHead = &(Die->ChildList);
    }

    return NULL;
}

