/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwarf.h

Abstract:

    This header contains definitions for the DWARF 2+ symbol format.

Author:

    Evan Green 2-Dec-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Parser definitions.
//

//
// Set this flag to print all the DWARF entities processed.
//

#define DWARF_CONTEXT_DEBUG 0x00000001

//
// Set this flag to print all the abbreviations.
//

#define DWARF_CONTEXT_DEBUG_ABBREVIATIONS 0x00000002

//
// Set this flag to print all the line number table information.
//

#define DWARF_CONTEXT_DEBUG_LINE_NUMBERS 0x00000004

//
// Set this flag to print unwinding information.
//

#define DWARF_CONTEXT_DEBUG_FRAMES 0x00000008

//
// Set this flag to print just the unwinding results.
//

#define DWARF_CONTEXT_VERBOSE_UNWINDING 0x00000010

//
// Define the maximum currently implemented depth of the stack. Bump this up if
// applications seem to be heavily using the DWARF expression stack.
//

#define DWARF_EXPRESSION_STACK_SIZE 20

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DWARF_TAG {
    DwarfTagArrayType = 0x01,
    DwarfTagClassType = 0x02,
    DwarfTagEntryPoint = 0x03,
    DwarfTagEnumerationType = 0x04,
    DwarfTagFormalParameter = 0x05,
    DwarfTagImportedDeclaration = 0x08,
    DwarfTagLabel = 0x0A,
    DwarfTagLexicalBlock = 0x0B,
    DwarfTagMember = 0x0D,
    DwarfTagPointerType = 0x0F,
    DwarfTagReferenceType = 0x10,
    DwarfTagCompileUnit = 0x11,
    DwarfTagStringType = 0x12,
    DwarfTagStructureType = 0x13,
    DwarfTagSubroutineType = 0x15,
    DwarfTagTypedef = 0x16,
    DwarfTagUnionType = 0x17,
    DwarfTagUnspecifiedParameters = 0x18,
    DwarfTagVariant = 0x19,
    DwarfTagCommonBlock = 0x1A,
    DwarfTagCommonInclusion = 0x1B,
    DwarfTagInheritance = 0x1C,
    DwarfTagInlinedSubroutine = 0x1D,
    DwarfTagModule = 0x1E,
    DwarfTagPointerToMemberType = 0x1F,
    DwarfTagSetType = 0x20,
    DwarfTagSubrangeType = 0x21,
    DwarfTagWithStatement = 0x22,
    DwarfTagAccessDeclaration = 0x23,
    DwarfTagBaseType = 0x24,
    DwarfTagCatchBlock = 0x25,
    DwarfTagConstType = 0x26,
    DwarfTagConstant = 0x27,
    DwarfTagEnumerator = 0x28,
    DwarfTagFileType = 0x29,
    DwarfTagFriend = 0x2A,
    DwarfTagNameList = 0x2B,
    DwarfTagNameListItem = 0x2C,
    DwarfTagPackedType = 0x2D,
    DwarfTagSubprogram = 0x2E,
    DwarfTagTemplateTypeParameter = 0x2F,
    DwarfTagTemplateValueParameter = 0x30,
    DwarfTagThrownType = 0x31,
    DwarfTagTryBlock = 0x32,
    DwarfTagVariantPart = 0x33,
    DwarfTagVariable = 0x34,
    DwarfTagVolatileType = 0x35,
    DwarfTagDwarfProcedure = 0x36,
    DwarfTagRestrictType = 0x37,
    DwarfTagInterfaceType = 0x38,
    DwarfTagNamespace = 0x39,
    DwarfTagImportedModule = 0x3A,
    DwarfTagUnspecifiedType = 0x3B,
    DwarfTagPartialUnit = 0x3C,
    DwarfTagImportedUnit = 0x3D,
    DwarfTagCondition = 0x3F,
    DwarfTagSharedType = 0x40,
    DwarfTagTypeUnit = 0x41,
    DwarfTagRvalueReferenceType = 0x42,
    DwarfTagTemplateAlias = 0x43,
    DwarfTagLowUser = 0x4080,
    DwarfTagHighUser = 0xFFFF
} DWARF_TAG, *PDWARF_TAG;

typedef enum _DWARF_CHILDREN_VALUE {
    DwarfChildrenNo = 0x00,
    DwarfChildrenYes = 0x01
} DWARF_CHILDREN_VALUE, *PDWARF_CHILDREN_VALUE;

typedef enum _DWARF_ATTRIBUTE {
    DwarfAtSibling = 0x01,
    DwarfAtLocation = 0x02,
    DwarfAtName = 0x03,
    DwarfAtOrdering = 0x09,
    DwarfAtByteSize = 0x0B,
    DwarfAtBitOffset = 0x0C,
    DwarfAtBitSize = 0x0D,
    DwarfAtStatementList = 0x10,
    DwarfAtLowPc = 0x11,
    DwarfAtHighPc = 0x12,
    DwarfAtLanguage = 0x13,
    DwarfAtDiscr = 0x15,
    DwarfAtDiscrValue = 0x16,
    DwarfAtVisibility = 0x17,
    DwarfAtImport = 0x18,
    DwarfAtStringLength = 0x19,
    DwarfAtCommonReference = 0x1A,
    DwarfAtCompDir = 0x1B,
    DwarfAtConstValue = 0x1C,
    DwarfAtContainingType = 0x1D,
    DwarfAtDefaultValue = 0x1E,
    DwarfAtInline = 0x20,
    DwarfAtIsOptional = 0x21,
    DwarfAtLowerBound = 0x22,
    DwarfAtProducer = 0x25,
    DwarfAtPrototyped = 0x27,
    DwarfAtReturnAddress = 0x2A,
    DwarfAtStartScope = 0x2C,
    DwarfAtBitStride = 0x2E,
    DwarfAtUpperBound = 0x2F,
    DwarfAtAbstractOrigin = 0x31,
    DwarfAtAccessibility = 0x32,
    DwarfAtAddressClass = 0x33,
    DwarfAtArtificial = 0x34,
    DwarfAtBaseTypes = 0x35,
    DwarfAtCallingConvention = 0x36,
    DwarfAtCount = 0x37,
    DwarfAtDataMemberLocation = 0x38,
    DwarfAtDeclColumn = 0x39,
    DwarfAtDeclFile = 0x3A,
    DwarfAtDeclLine = 0x3B,
    DwarfAtDeclaration = 0x3C,
    DwarfAtDiscrList = 0x3D,
    DwarfAtEncoding = 0x3E,
    DwarfAtExternal = 0x3F,
    DwarfAtFrameBase = 0x40,
    DwarfAtFriend = 0x41,
    DwarfAtIdentifierCase = 0x42,
    DwarfAtMacroInfo = 0x43,
    DwarfAtNameListItem = 0x44,
    DwarfAtPriority = 0x45,
    DwarfAtSegment = 0x46,
    DwarfAtSpecification = 0x47,
    DwarfAtStaticLink = 0x48,
    DwarfAtType = 0x49,
    DwarfAtUseLocation = 0x4A,
    DwarfAtVariableParameter = 0x4B,
    DwarfAtVirtuality = 0x4C,
    DwarfAtVtableElementLocation = 0x4D,
    DwarfAtAllocated = 0x4E,
    DwarfAtAssociated = 0x4F,
    DwarfAtDataLocation = 0x50,
    DwarfAtByteStride = 0x51,
    DwarfAtEntryPc = 0x52,
    DwarfAtUseUtf8 = 0x53,
    DwarfAtExtension = 0x54,
    DwarfAtRanges = 0x55,
    DwarfAtTrampoline = 0x56,
    DwarfAtCallColumn = 0x57,
    DwarfAtCallFile = 0x58,
    DwarfAtCallLine = 0x59,
    DwarfAtDescription = 0x5A,
    DwarfAtBinaryScale = 0x5B,
    DwarfAtDecimalScale = 0x5C,
    DwarfAtSmall = 0x5D,
    DwarfAtDecimalSign = 0x5E,
    DwarfAtDigitCount = 0x5F,
    DwarfAtPictureString = 0x60,
    DwarfAtMutable = 0x61,
    DwarfAtThreadsScaled = 0x62,
    DwarfAtExplicit = 0x63,
    DwarfAtObjectPointer = 0x64,
    DwarfAtEndianity = 0x65,
    DwarfAtElemental = 0x66,
    DwarfAtPure = 0x67,
    DwarfAtRecursive = 0x68,
    DwarfAtSignature = 0x69,
    DwarfAtMainSubprogram = 0x6A,
    DwarfAtDataBitOffset = 0x6B,
    DwarfAtConstExpression = 0x6C,
    DwarfAtEnumClass = 0x6D,
    DwarfAtLinkageName = 0x6E,
    DwarfAtLowUser = 0x2000,
    DwarfAtHighUser = 0x3FFF
} DWARF_ATTRIBUTE, *PDWARF_ATTRIBUTE;

typedef enum _DWARF_FORM {
    DwarfFormAddress = 0x01,
    DwarfFormBlock2 = 0x03,
    DwarfFormBlock4 = 0x04,
    DwarfFormData2 = 0x05,
    DwarfFormData4 = 0x06,
    DwarfFormData8 = 0x07,
    DwarfFormString = 0x08,
    DwarfFormBlock = 0x09,
    DwarfFormBlock1 = 0x0A,
    DwarfFormData1 = 0x0B,
    DwarfFormFlag = 0x0C,
    DwarfFormSData = 0x0D,
    DwarfFormStringPointer = 0x0E,
    DwarfFormUData = 0x0F,
    DwarfFormRefAddress = 0x10,
    DwarfFormRef1 = 0x11,
    DwarfFormRef2 = 0x12,
    DwarfFormRef4 = 0x13,
    DwarfFormRef8 = 0x14,
    DwarfFormRefUData = 0x15,
    DwarfFormIndirect = 0x16,
    DwarfFormSecOffset = 0x17,
    DwarfFormExprLoc = 0x18,
    DwarfFormFlagPresent = 0x19,
    DwarfFormRefSig8 = 0x20
} DWARF_FORM, *PDWARF_FORM;

typedef enum _DWARF_OP {
    DwarfOpAddress = 0x03,
    DwarfOpDereference = 0x06,
    DwarfOpConst1U = 0x08,
    DwarfOpConst1S = 0x09,
    DwarfOpConst2U = 0x0A,
    DwarfOpConst2S = 0x0B,
    DwarfOpConst4U = 0x0C,
    DwarfOpConst4S = 0x0D,
    DwarfOpConst8U = 0x0E,
    DwarfOpConst8S = 0x0F,
    DwarfOpConstU = 0x10,
    DwarfOpConstS = 0x11,
    DwarfOpDup = 0x12,
    DwarfOpDrop = 0x13,
    DwarfOpOver = 0x14,
    DwarfOpPick = 0x15,
    DwarfOpSwap = 0x16,
    DwarfOpRot = 0x17,
    DwarfOpXDeref = 0x18,
    DwarfOpAbs = 0x19,
    DwarfOpAnd = 0x1A,
    DwarfOpDiv = 0x1B,
    DwarfOpMinus = 0x1C,
    DwarfOpMod = 0x1D,
    DwarfOpMul = 0x1E,
    DwarfOpNeg = 0x1F,
    DwarfOpNot = 0x20,
    DwarfOpOr = 0x21,
    DwarfOpPlus = 0x22,
    DwarfOpPlusUConst = 0x23,
    DwarfOpShl = 0x24,
    DwarfOpShr = 0x25,
    DwarfOpShra = 0x26,
    DwarfOpXor = 0x27,
    DwarfOpBra = 0x28,
    DwarfOpEq = 0x29,
    DwarfOpGe = 0x2A,
    DwarfOpGt = 0x2B,
    DwarfOpLe = 0x2C,
    DwarfOpLt = 0x2D,
    DwarfOpNe = 0x2E,
    DwarfOpSkip = 0x2F,
    DwarfOpLit0 = 0x30,
    DwarfOpLit31 = 0x4F,
    DwarfOpReg0 = 0x50,
    DwarfOpReg31 = 0x6F,
    DwarfOpBreg0 = 0x70,
    DwarfOpBreg31 = 0x8F,
    DwarfOpRegX = 0x90,
    DwarfOpFbreg = 0x91,
    DwarfOpBregX = 0x92,
    DwarfOpPiece = 0x93,
    DwarfOpDerefSize = 0x94,
    DwarfOpXDerefSize = 0x95,
    DwarfOpNop = 0x96,
    DwarfOpPushObjectAddress = 0x97,
    DwarfOpCall2 = 0x98,
    DwarfOpCall4 = 0x99,
    DwarfOpCallRef = 0x9A,
    DwarfOpFormTlsAddress = 0x9B,
    DwarfOpCallFrameCfa = 0x9C,
    DwarfOpBitPiece = 0x9D,
    DwarfOpImplicitValue = 0x9E,
    DwarfOpStackValue = 0x9F,
    DwarfOpLowUser = 0xE0,
    DwarfOpGnuPushTlsAddress = 0xE0,
    DwarfOpGnuUninit = 0xF0,
    DwarfOpGnuEncodedAddr = 0xF1,
    DwarfOpGnuImplicitPointer = 0xF2,
    DwarfOpGnuEntryValue = 0xF3,
    DwarfOpGnuConstType = 0xF4,
    DwarfOpGnuRegvalType = 0xF5,
    DwarfOpGnuDerefType = 0xF6,
    DwarfOpGnuConvert = 0xF7,
    DwarfOpGnuReinterpret = 0xF9,
    DwarfOpGnuParameterRef = 0xFA,
    DwarfOpGnuAddrIndex = 0xFB,
    DwarfOpGnuConstIndex = 0xFC,
    DwarfOpHighUser = 0xFF
} DWARF_OP, *PDWARF_OP;

typedef enum _DWARF_BASE_TYPE_ATTRIBUTE {
    DwarfAteAddress = 0x01,
    DwarfAteBoolean = 0x02,
    DwarfAteComplexFloat = 0x03,
    DwarfAteFloat = 0x04,
    DwarfAteSigned = 0x05,
    DwarfAteSignedChar = 0x06,
    DwarfAteUnsigned = 0x07,
    DwarfAteUnsignedChar = 0x08,
    DwarfAteImaginaryFloat = 0x09,
    DwarfAtePackedDecimal = 0x0A,
    DwarfAteNumericString = 0x0B,
    DwarfAteEdited = 0x0C,
    DwarfAteSignedFixed = 0x0D,
    DwarfAteUnsignedFixed = 0x0E,
    DwarfAteDecimalFloat = 0x0F,
    DwarfAteUtf = 0x10,
    DwarfAteLowUser = 0x80,
    DwarfAteHighUser = 0xFF
} DWARF_BASE_TYPE_ATTRIBUTE, *PDWARF_BASE_TYPE_ATTRIBUTE;

typedef enum _DWARF_DECIMAL_SIGN {
    DwarfDsUnsigned = 0x01,
    DwarfDsLeadingOverpunch = 0x02,
    DwarfDsTrailingOverpunch = 0x03,
    DwarfDsLeadingSeparate = 0x04,
    DwarfDsTrailingSeparate = 0x05,
} DWARF_DECIMAL_SIGN, *PDWARF_DECIMAL_SIGN;

typedef enum _DWARF_ENDIANITY {
    DwarfEndDefault = 0x00,
    DwarfEndBig = 0x01,
    DwarfEndLittle = 0x02,
    DwarfEndLowUser = 0x40,
    DwarfEndHighUser = 0xFF
} DWARF_ENDIANITY, *PDWARF_ENDIANITY;

typedef enum _DWARF_ACCESSIBILITY {
    DwarfAccessPublic = 0x01,
    DwarfAccessProtected = 0x02,
    DwarfAccessPrivate = 0x03,
} DWARF_ACCESSIBILITY, *PDWARF_ACCESSIBILITY;

typedef enum _DWARF_VISIBILITY {
    DwarfVisLocal = 0x01,
    DwarfVisExported = 0x02,
    DwarfVisQualified = 0x03
} DWARF_VISIBILITY, *PDWARF_VISIBILITY;

typedef enum _DWARF_VIRTUALITY {
    DwarfVirtualityNone = 0x00,
    DwarfVirtualityVirtual = 0x01,
    DwarfVirtualityPureVirtual = 0x02
} DWARF_VIRTUALITY, *PDWARF_VIRTUALITY;

typedef enum _DWARF_LANGUAGE {
    DwarfLanguageC89 = 0x0001,
    DwarfLanguageC = 0x0002,
    DwarfLanguageAda83 = 0x0003,
    DwarfLanguageCPlusPlus = 0x0004,
    DwarfLanguageCobol74 = 0x0005,
    DwarfLanguageCobol85 = 0x0006,
    DwarfLanguageFortran77 = 0x0007,
    DwarfLanguageFortran90 = 0x0008,
    DwarfLanguagePascal83 = 0x0009,
    DwarfLanguageModula2 = 0x000A,
    DwarfLanguageJava = 0x000B,
    DwarfLanguageC99 = 0x000C,
    DwarfLanguageAda95 = 0x000D,
    DwarfLanguageFortran95 = 0x000E,
    DwarfLanguagePli = 0x000F,
    DwarfLanguageObjC = 0x0010,
    DwarfLanguageObjCPlusPlus = 0x0011,
    DwarfLanguageUpc = 0x0012,
    DwarfLanguageD = 0x0013,
    DwarfLanguagePython = 0x0014,
    DwarfLanguageLowUser = 0x8000,
    DwarfLanguageHighUser = 0xFFFF
} DWARF_LANGUAGE, *PDWARF_LANGUAGE;

typedef enum _DWARF_IDENTIFIER_CASE {
    DwarfIdCaseSensitive = 0x00,
    DwarfIdUpCase = 0x01,
    DwarfIdDownCase = 0x02,
    DwarfIdCaseInsensitive = 0x03
} DWARF_IDENTIFIER_CASE, *PDWARF_IDENTIFIER_CASE;

typedef enum _DWARF_CALLING_CONVENTION {
    DwarfCcNormal = 0x01,
    DwarfCcProgram = 0x02,
    DwarfCcNoCall = 0x03,
    DwarfCcLowUser = 0x40,
    DwarfCcHighUser = 0xFF
} DWARF_CALLING_CONVENTION, *PDWARF_CALLING_CONVENTION;

typedef enum _DWARF_INLINE_CODE {
    DwarfInlNotInlined = 0x00,
    DwarfInlInlined = 0x01,
    DwarfInlDeclaredNotInlined = 0x02,
    DwarfInlDeclaredInlined = 0x03
} DWARF_INLINE_CODE, *PDWARF_INLINE_CODE;

typedef enum _DWARF_ARRAY_ORDERING {
    DwarfOrdRowMajor = 0x00,
    DwarfOrdColumnMajor = 0x01
} DWARF_ARRAY_ORDERING, *PDWARF_ARRAY_ORDERING;

typedef enum _DWARF_DISCRIMINANT_LIST {
    DwarfDscLabel = 0x00,
    DwarfDscRange = 0x01
} DWARF_DISCRIMINANT_LIST, *PDWARF_DISCRIMINANT_LIST;

typedef enum _DWARF_LINE_STANDARD_OP {
    DwarfLnsCopy = 0x01,
    DwarfLnsAdvancePc = 0x02,
    DwarfLnsAdvanceLine = 0x03,
    DwarfLnsSetFile = 0x04,
    DwarfLnsSetColumn = 0x05,
    DwarfLnsNegateStatement = 0x06,
    DwarfLnsSetBasicBlock = 0x07,
    DwarfLnsConstAddPc = 0x08,
    DwarfLnsFixedAdvancePc = 0x09,
    DwarfLnsSetPrologueEnd = 0x0A,
    DwarfLnsSetEpilogueBegin = 0x0B,
    DwarfLnsSetIsa = 0x0C,
} DWARF_LINE_STANDARD_OP, *PDWARF_LINE_STANDARD_OP;

typedef enum _DWARF_LINE_EXTENDED_OP {
    DwarfLneEndSequence = 0x01,
    DwarfLneSetAddress = 0x02,
    DwarfLneDefineFile = 0x03,
    DwarfLneSetDiscriminator = 0x04,
    DwarfLneLowUser = 0x80,
    DwarfLneHighUser = 0xFF
} DWARF_LINE_EXTENDED_OP, *PDWARF_LINE_EXTENDED_OP;

typedef enum _DWARF_MACRO_INFORMATION {
    DwarfMacInfoDefine = 0x01,
    DwarfMacInfoUndefine = 0x02,
    DwarfMacInfoStartFile = 0x03,
    DwarfMacInfoEndFile = 0x04,
    DwarfMacInfoVendorExt = 0xFF
} DWARF_MACRO_INFORMATION, *PDWARF_MACRO_INFORMATION;

typedef enum _DWARF_CALL_FRAME_ENCODING {
    DwarfCfaNop = 0x00,
    DwarfCfaSetLoc = 0x01,
    DwarfCfaAdvanceLoc1 = 0x02,
    DwarfCfaAdvanceLoc2 = 0x03,
    DwarfCfaAdvanceLoc4 = 0x04,
    DwarfCfaOffsetExtended = 0x05,
    DwarfCfaRestoreExtended = 0x06,
    DwarfCfaUndefined = 0x07,
    DwarfCfaSameValue = 0x08,
    DwarfCfaRegister = 0x09,
    DwarfCfaRememberState = 0x0A,
    DwarfCfaRestoreState = 0x0B,
    DwarfCfaDefCfa = 0x0C,
    DwarfCfaDefCfaRegister = 0x0D,
    DwarfCfaDefCfaOffset = 0x0E,
    DwarfCfaDefCfaExpression = 0x0F,
    DwarfCfaExpression = 0x10,
    DwarfCfaOffsetExtendedSf = 0x11,
    DwarfCfaDefCfaSf = 0x12,
    DwarfCfaDefCfaOffsetSf = 0x13,
    DwarfCfaValOffset = 0x14,
    DwarfCfaValOffsetSf = 0x15,
    DwarfCfaValExpression = 0x16,
    DwarfCfaLowUser = 0x1C,
    DwarfCfaHighUser = 0x3F,
    DwarfCfaAdvanceLoc = 0x40,
    DwarfCfaOffset = 0x80,
    DwarfCfaRestore = 0xC0,
    DwarfCfaHighMask = 0xC0
} DWARF_CALL_FRAME_ENCODING, *PDWARF_CALL_FRAME_ENCODING;

typedef enum _DWARF_ADDRESS_ENCODING {
    DwarfPeAbsolute = 0x00,
    DwarfPeLeb128 = 0x01,
    DwarfPeUdata2 = 0x02,
    DwarfPeUdata4 = 0x03,
    DwarfPeUdata8 = 0x04,
    DwarfPeSigned = 0x08,
    DwarfPeSleb128 = 0x09,
    DwarfPeSdata2 = 0x0A,
    DwarfPeSdata4 = 0x0B,
    DwarfPeSdata8 = 0x0C,
    DwarfPeTypeMask = 0x0F,
    DwarfPePcRelative = 0x10,
    DwarfPeTextRelative = 0x20,
    DwarfPeDataRelative = 0x30,
    DwarfPeFunctionRelative = 0x40,
    DwarfPeAligned = 0x50,
    DwarfPeModifierMask = 0x70,
    DwarfPeIndirect = 0x80,
    DwarfPeOmit = 0xFF,
} DWARF_ADDRESS_ENCODING, *PDWARF_ADDRESS_ENCODING;

//
// Parser data types.
//

typedef enum _DWARF_LOCATION_TYPE {
    DwarfLocationInvalid,
    DwarfLocationMemory,
    DwarfLocationRegister,
    DwarfLocationKnownValue,
    DwarfLocationKnownData,
    DwarfLocationUndefined,
} DWARF_LOCATION_TYPE, *PDWARF_LOCATION_TYPE;

/*++

Structure Description:

    This structure describes a single DWARF debug section.

Members:

    Data - Stores a pointer to the data.

    Size - Stores the size of the section in bytes.

--*/

typedef struct _DWARF_SECTION {
    PVOID Data;
    ULONG Size;
} DWARF_SECTION, *PDWARF_SECTION;

/*++

Structure Description:

    This structure contains the various debug sections used in DWARF symbols.

Members:

    Info - Stores the primary .debug_info section.

    Abbreviations - Stores the .debug_abbrev abbreviations table.

    Strings - Stores the .debug_str string table.

    Locations - Stores the .debug_loc locations section.

    Aranges - Stores the .debug_aranges section.

    Ranges - Stores the .debug_ranges section.

    Macros - Stores the .debug_macinfo preprocessor macros information section.

    Lines - Stores the .debug_line line number information.

    PubNames - Store the .debug_pubnames name information.

    PubTypes - Stores the .debug_pubtypes type name information.

    Types - Stores the .debug_types type information, new in DWARF4.

    Frame - Stores the .debug_frame section.

    EhFrame - Stores the .eh_frame section.

    EhFrameAddress - Stores the virtual address of the .eh_frame section.

--*/

typedef struct _DWARF_DEBUG_SECTIONS {
    DWARF_SECTION Info;
    DWARF_SECTION Abbreviations;
    DWARF_SECTION Strings;
    DWARF_SECTION Locations;
    DWARF_SECTION Aranges;
    DWARF_SECTION Ranges;
    DWARF_SECTION Macros;
    DWARF_SECTION Lines;
    DWARF_SECTION PubNames;
    DWARF_SECTION PubTypes;
    DWARF_SECTION Types;
    DWARF_SECTION Frame;
    DWARF_SECTION EhFrame;
    ULONGLONG EhFrameAddress;
} DWARF_DEBUG_SECTIONS, *PDWARF_DEBUG_SECTIONS;

/*++

Structure Description:

    This structure contains the context for a DWARF symbol table.

Members:

    Flags - Stores a bitfield of flags. See DWARF_CONTEXT_* definitions.

    FileData - Stores a pointer to the file data.

    FileSize - Stores the size of the file data in bytes.

    Sections - Stores pointers to the various DWARF debug sections.

    UnitList - Stores the head of the list of Compilation Units.

    SourcesHead - Stores a pointer to the head of the list of source file
        symbols.

    LoadingContext - Stores a pointer to internal state used during the load of
        the module. This is of type DWARF_LOADING_CONTEXT.

--*/

typedef struct _DWARF_CONTEXT {
    ULONG Flags;
    PSTR FileData;
    UINTN FileSize;
    DWARF_DEBUG_SECTIONS Sections;
    LIST_ENTRY UnitList;
    PLIST_ENTRY SourcesHead;
    PVOID LoadingContext;
} DWARF_CONTEXT, *PDWARF_CONTEXT;

//
// Location support stuctures
//

/*++

Structure Description:

    This union contains the different forms of a DWARF location.

Members:

    Address - Stores the target memory address location form.

    Register - Stores the register address form.

    Value - Stores the direct value, rather than the location.

    Buffer - Stores a pointer and size of a buffer that contains the direct
        value, rather than the location.

--*/

typedef union _DWARF_LOCATION_UNION {
    ULONGLONG Address;
    ULONG Register;
    ULONGLONG Value;
    DWARF_SECTION Buffer;
} DWARF_LOCATION_UNION, *PDWARF_LOCATION_UNION;

typedef struct _DWARF_LOCATION DWARF_LOCATION, *PDWARF_LOCATION;
typedef struct _DWARF_COMPILATION_UNIT
    DWARF_COMPILATION_UNIT, *PDWARF_COMPILATION_UNIT;

/*++

Structure Description:

    This structure describes a DWARF location.

Members:

    ListEntry - Stores pointers to the next and previous pieces of the
        complete location if this location is part of a list.

    Form - Stores the location form.

    Value - Stores the location value union.

    BitSize - Stores the size of this piece, or 0 if this describes the entire
        object.

    BitOffset - Stores the offset from the start of the source data in bits if
        this piece is offset in some way.

    NextPiece - Stores a pointer to the next piece of the object if it's a
        composite description.

--*/

struct _DWARF_LOCATION {
    DWARF_LOCATION_TYPE Form;
    DWARF_LOCATION_UNION Value;
    ULONG BitSize;
    ULONG BitOffset;
    PDWARF_LOCATION NextPiece;
};

/*++

Structure Description:

    This structure describes the context needed to compute a DWARF location.

Members:

    Stack - Stores the DWARF expression stack. Element zero is the first
        pushed and last popped.

    StackSize - Stores the number of valid elements on the expression stack.

    Unit - Stores a pointer to the compilation unit the expression lives in.

    AddressSize - Stores the size of a target address.

    Pc - Stores the current value of the instruction pointer, which may be
        needed for computing the frame base.

    ObjectAddress - Stores the base address of the object being evaluated. This
        is the value pushed if a push object address op is executed.

    TlsBase - Stores the thread local storage base region for this thread and
        module. This value is added if a Form TLS Address op is executed.

    CurrentFunction - Stores a pointer to the current function.

    Location - Stores the final location of the entity. This may end up being a
        list.

    Constant - Stores a boolean indicating whether or not the expression is
        constant or depends on machine state.

--*/

typedef struct _DWARF_LOCATION_CONTEXT {
    ULONGLONG Stack[DWARF_EXPRESSION_STACK_SIZE];
    ULONG StackSize;
    PDWARF_COMPILATION_UNIT Unit;
    UCHAR AddressSize;
    ULONGLONG Pc;
    ULONGLONG ObjectAddress;
    ULONGLONG TlsBase;
    PFUNCTION_SYMBOL CurrentFunction;
    DWARF_LOCATION Location;
    BOOL Constant;
} DWARF_LOCATION_CONTEXT, *PDWARF_LOCATION_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
DwarfLoadSymbols (
    PSTR Filename,
    IMAGE_MACHINE_TYPE MachineType,
    ULONG Flags,
    PVOID HostContext,
    PDEBUG_SYMBOLS *Symbols
    );

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

INT
DwarfStackUnwind (
    PDEBUG_SYMBOLS Symbols,
    ULONGLONG DebasedPc,
    PSTACK_FRAME Frame
    );

/*++

Routine Description:

    This routine attempts to unwind the stack by one frame.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus loaded
        base difference of the module).

    Frame - Supplies a pointer where the basic frame information for this
        frame will be returned.

Return Value:

    0 on success.

    EOF if there are no more stack frames.

    Returns an error code on failure.

--*/

