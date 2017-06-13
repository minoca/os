/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    string.c

Abstract:

    This module implements support for the string object in Chalk.

Author:

    Evan Green 29-May-2016

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <ctype.h>
#include <stdio.h>
#include "chalkp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
CkpStringFromCharacter (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringFromByte (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringByteAt (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringCharacterAt (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringContains (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringStartsWith (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringEndsWith (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringRightIndexOf (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringIndexOf (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringIterate (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringIteratorValue (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringLower (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringUpper (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringLength (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringJoinList (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringSplit (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringRightSplit (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringReplace (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringCompare (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringAdd (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringMultiply (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringSlice (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringSliceCharacters (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpStringRepresentation (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

CK_VALUE
CkpStringCreateFromCharacterRange (
    PCK_VM Vm,
    PCK_STRING Source,
    UINTN Start,
    UINTN Count
    );

CK_VALUE
CkpStringSliceBytes (
    PCK_VM Vm,
    PCK_STRING Source,
    UINTN Start,
    UINTN Count
    );

//
// -------------------------------------------------------------------- Globals
//

CK_PRIMITIVE_DESCRIPTION CkStringPrimitives[] = {
    {"byteAt@1", 1, CkpStringByteAt},
    {"charAt@1", 1, CkpStringCharacterAt},
    {"contains@1", 1, CkpStringContains},
    {"startsWith@1", 1, CkpStringStartsWith},
    {"endsWith@1", 1, CkpStringEndsWith},
    {"rindexOf@1", 1, CkpStringRightIndexOf},
    {"indexOf@1", 1, CkpStringIndexOf},
    {"iterate@1", 1, CkpStringIterate},
    {"iteratorValue@1", 1, CkpStringIteratorValue},
    {"lower@0", 0, CkpStringLower},
    {"upper@0", 0, CkpStringUpper},
    {"length@0", 0, CkpStringLength},
    {"joinList@1", 1, CkpStringJoinList},
    {"split@2", 2, CkpStringSplit},
    {"rsplit@2", 2, CkpStringRightSplit},
    {"replace@3", 3, CkpStringReplace},
    {"compare@1", 1, CkpStringCompare},
    {"sliceChars@1", 1, CkpStringSliceCharacters},
    {"__add@1", 1, CkpStringAdd},
    {"__mul@1", 1, CkpStringMultiply},
    {"__slice@1", 1, CkpStringSlice},
    {"__str@0", 0, CkpStringToString},
    {"__repr@0", 0, CkpStringRepresentation},
    {NULL, 0, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkStringStaticPrimitives[] = {
    {"fromCharacter@1", 1, CkpStringFromCharacter},
    {"fromByte@1", 1, CkpStringFromByte},
    {NULL, 0, NULL}
};

PCSTR CkStringEscapes = "0??????abtnvfr";

//
// ------------------------------------------------------------------ Functions
//

CK_VALUE
CkpStringCreate (
    PCK_VM Vm,
    PCSTR Text,
    UINTN Length
    )

/*++

Routine Description:

    This routine creates a new string object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Text - Supplies a pointer to the value of the string. A copy of this memory
        will be made.

    Length - Supplies the length of the string, not including the null
        terminator.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

{

    PCK_STRING String;
    CK_VALUE Value;

    String = CkpStringAllocate(Vm, Length);
    if (String == NULL) {
        return CK_NULL_VALUE;
    }

    if ((Length != 0) && (Text != NULL)) {
        memcpy((PSTR)(String->Value), Text, Length);
    }

    CkpStringHash(String);
    CK_OBJECT_VALUE(Value, String);
    return Value;
}

CK_VALUE
CkpStringCreateFromInteger (
    PCK_VM Vm,
    CK_INTEGER Integer
    )

/*++

Routine Description:

    This routine creates a new string object based on an integer.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Integer - Supplies the integer to convert.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

{

    CHAR Buffer[21];
    LONG Length;

    //
    // The max integer length for a 64-bit value is "-9223372036854775808".
    //

    Length = snprintf(Buffer, sizeof(Buffer), "%lld", (LONGLONG)Integer);
    return CkpStringCreate(Vm, Buffer, Length);
}

CK_VALUE
CkpStringCreateFromIndex (
    PCK_VM Vm,
    PCK_STRING Source,
    UINTN Index
    )

/*++

Routine Description:

    This routine creates a new string from a single UTF-8 codepoint at the
    given byte index into the source string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Source - Supplies the source string to index into.

    Index - Supplies the byte index to read from.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

{

    CHAR Buffer[2];
    INT Character;

    CK_ASSERT((Source->Header.Type == CkObjectString) &&
              (Index < Source->Length));

    Character = CkpUtf8Decode((PUCHAR)(Source->Value) + Index,
                              Source->Length - Index);

    //
    // If UTF8 decoding failed, just treat it as a raw byte.
    //

    if (Character == -1) {
        Buffer[0] = Source->Value[Index];
        Buffer[1] = '\0';
        return CkpStringCreate(Vm, Buffer, 1);
    }

    return CkpStringCreateFromCharacter(Vm, Character);
}

CK_VALUE
CkpStringCreateFromCharacter (
    PCK_VM Vm,
    INT Character
    )

/*++

Routine Description:

    This routine creates a new string object based on a UTF-8 codepoint.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Character - Supplies the UTF-8 codepoint to convert.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

{

    UCHAR Buffer[5];
    UINTN Length;

    Length = CkpUtf8Encode(Character, Buffer);
    Buffer[Length] = '\0';
    return CkpStringCreate(Vm, (PSTR)Buffer, Length);
}

CK_VALUE
CkpStringFormat (
    PCK_VM Vm,
    PSTR Format,
    ...
    )

/*++

Routine Description:

    This routine creates a new string object based on a formatted string. This
    formatting is much simpler than printf-style formatting. The only format
    specifiers are '$', which specifies a C string, or '@', which specifies a
    string object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Format - Supplies a pointer to the format string.

    ... - Supplies the remainder of the arguments, which depend on the format
        string.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

{

    va_list ArgumentList;
    PSTR Current;
    UINTN Length;
    PCK_STRING NewString;
    PSTR Out;
    PSTR Parameter;
    PCK_STRING StringParameter;
    UINTN TotalLength;
    CK_VALUE Value;

    va_start(ArgumentList, Format);
    Current = Format;
    TotalLength = 0;
    while (*Current != '\0') {
        if (*Current == '$') {
            Parameter = va_arg(ArgumentList, PSTR);
            if (Parameter == NULL) {
                Parameter = "(null)";
            }

            TotalLength += strlen(Parameter);

        } else if (*Current == '@') {
            Value = va_arg(ArgumentList, CK_VALUE);
            if (CK_IS_OBJECT(Value)) {
                StringParameter = CK_AS_STRING(Value);
                if (StringParameter->Header.Type == CkObjectString) {
                    TotalLength += StringParameter->Length;
                }
            }

        } else {
            TotalLength += 1;
        }

        Current += 1;
    }

    va_end(ArgumentList);
    NewString = CkpStringAllocate(Vm, TotalLength);
    if (NewString == NULL) {
        return CK_NULL_VALUE;
    }

    va_start(ArgumentList, Format);
    Current = Format;
    Out = (PSTR)(NewString->Value);
    while (*Current != '\0') {
        if (*Current == '$') {
            Parameter = va_arg(ArgumentList, PSTR);
            if (Parameter == NULL) {
                Parameter = "(null)";
            }

            Length = strlen(Parameter);
            memcpy(Out, Parameter, Length);
            Out += Length;

        } else if (*Current == '@') {
            Value = va_arg(ArgumentList, CK_VALUE);
            if (CK_IS_OBJECT(Value)) {
                StringParameter = CK_AS_STRING(Value);
                if (StringParameter->Header.Type == CkObjectString) {
                    Length = StringParameter->Length;
                    memcpy(Out, StringParameter->Value, Length);
                    Out += Length;
                }
            }

        } else {
            *Out = *Current;
            Out += 1;
        }

        Current += 1;
    }

    va_end(ArgumentList);
    CkpStringHash(NewString);
    CK_OBJECT_VALUE(Value, NewString);
    return Value;
}

UINTN
CkpStringFindLast (
    PCK_STRING Haystack,
    PCK_STRING Needle
    )

/*++

Routine Description:

    This routine searches for the last instance of a given substring within a
    string.

Arguments:

    Haystack - Supplies a pointer to the string to search.

    Needle - Supplies a pointer to the string to search for.

Return Value:

    Returns the index of the last instance of the needle within the haystack on
    success.

    (UINTN)-1 if the needle could not be found in the haystack.

--*/

{

    UINTN Index;
    UINTN LastIndex;

    Index = 0;
    LastIndex = (UINTN)-1;
    while (TRUE) {
        Index = CkpStringFind(Haystack, Index, Needle);
        if (Index == (UINTN)-1) {
            Index = LastIndex;
            break;
        }

        LastIndex = Index;
        Index += Needle->Length;
    }

    return LastIndex;
}

UINTN
CkpStringFind (
    PCK_STRING Haystack,
    UINTN Offset,
    PCK_STRING Needle
    )

/*++

Routine Description:

    This routine searches for a given substring within a string.

Arguments:

    Haystack - Supplies a pointer to the string to search.

    Offset - Supplies the offset from within the haystack to begin searching.

    Needle - Supplies a pointer to the string to search for.

Return Value:

    Returns the index of the needle within the haystack on success.

    (UINTN)-1 if the needle could not be found in the haystack.

--*/

{

    CHAR Character;
    UINTN Index;
    CHAR LastCharacter;
    INT Match;
    UINTN NeedleEnd;
    UINTN Shift[MAX_UCHAR + 1];

    //
    // An empty needle is always right there.
    //

    if (Needle->Length == 0) {
        return 0;

    //
    // If the needle is only one byte wide, just search for the byte without
    // all the fanciness.
    //

    } else if (Needle->Length == 1) {
        Character = *(Needle->Value);
        for (Index = Offset; Index < Haystack->Length; Index += 1) {
            if (Haystack->Value[Index] == Character) {
                return Index;
            }
        }

        return (UINTN)-1;
    }

    if (Needle->Length > Haystack->Length) {
        return (UINTN)-1;
    }

    NeedleEnd = Needle->Length - 1;

    //
    // Use the Boyer-Moore-Horspool string matching algorithm. Start by
    // assuming the every character is not in the needle at all, and thus the
    // search can be advanced by the entire length of the needle.
    //

    for (Index = 0; Index <= MAX_UCHAR; Index += 1) {
        Shift[Index] = Needle->Length;
    }

    //
    // For each character in the needle, record how far it is from the end,
    // which represents how far the query can advance if that character is
    // found in the query.
    //

    for (Index = 0; Index < NeedleEnd; Index += 1) {
        Character = Needle->Value[Index];
        Shift[(UCHAR)Character] = NeedleEnd - Index;
    }

    LastCharacter = Needle->Value[NeedleEnd];
    Index = Offset;
    while (Index <= Haystack->Length - Needle->Length) {

        //
        // Check the last character in the needle. If it matches, see if the
        // whole string matches.
        //

        Character = Haystack->Value[Index + NeedleEnd];
        if (Character == LastCharacter) {
            Match = CkCompareMemory(Haystack->Value + Index,
                                    Needle->Value,
                                    Needle->Length);

            if (Match == 0) {
                return Index;
            }
        }

        Index += Shift[(UCHAR)Character];
    }

    return (UINTN)-1;
}

INT
CkpUtf8EncodeSize (
    INT Character
    )

/*++

Routine Description:

    This routine returns the number of bytes required to enocde the given
    codepoint.

Arguments:

    Character - Supplies the UTF8 codepoint to get the length for.

Return Value:

    Returns the number of bytes needed to encode that codepoint, or 0 if the
    codepoint is invalid.

--*/

{

    if (Character < 0) {
        return 0;
    }

    if (Character <= 0x7F) {
        return 1;
    }

    if (Character <= 0x7FF) {
        return 2;
    }

    if (Character <= 0xFFFF) {
        return 3;
    }

    if (Character <= CK_MAX_UTF8) {
        return 4;
    }

    return 0;
}

INT
CkpUtf8Encode (
    INT Character,
    PUCHAR Bytes
    )

/*++

Routine Description:

    This routine encodes the given UTF8 character into the given byte stream.

Arguments:

    Character - Supplies the UTF8 codepoint to encode.

    Bytes - Supplies a pointer where the bytes for the given codepoint will be
        returned.

Return Value:

    Returns the number of bytes used to encode that codepoint, or 0 if the
    codepoint is invalid.

--*/

{

    if (Character < 0) {
        return 0;
    }

    if (Character <= 0x7F) {
        *Bytes = Character;
        return 1;
    }

    if (Character <= 0x7FF) {
        Bytes[0] = 0xC0 | (Character >> 6);
        Bytes[1] = 0x80 | (Character & 0x3F);
        return 2;
    }

    if (Character <= 0xFFFF) {
        Bytes[0] = 0xE0 | (Character >> 12);
        Bytes[1] = 0x80 | ((Character >> 6) & 0x3F);
        Bytes[2] = 0x80 | (Character & 0x3F);
        return 3;
    }

    if (Character <= CK_MAX_UTF8) {
        Bytes[0] = 0xF0 | (Character >> 18);
        Bytes[1] = 0x80 | ((Character >> 12) & 0x3F);
        Bytes[2] = 0x80 | ((Character >> 6) & 0x3F);
        Bytes[3] = 0x80 | (Character & 0x3F);
        return 4;
    }

    return 0;
}

INT
CkpUtf8DecodeSize (
    UCHAR Byte
    )

/*++

Routine Description:

    This routine determines the number of bytes in the given UTF-8 sequence
    given the first byte.

Arguments:

    Byte - Supplies the first byte of the sequence.

Return Value:

    Returns the number of bytes needed to decode that codepoint, or 0 if the
    first byte is not the beginning of a valid UTF-8 sequence.

--*/

{

    //
    // Check for a byte that's in the middle of a UTF-8 sequence, and reject it.
    //

    if ((Byte & 0xC0) == 0x80) {
        return 0;
    }

    if ((Byte & 0xF8) == 0xF0) {
        return 4;
    }

    if ((Byte & 0xF0) == 0xE0) {
        return 3;
    }

    if ((Byte & 0xE0) == 0xC0) {
        return 2;
    }

    return 1;
}

INT
CkpUtf8Decode (
    PUCHAR Bytes,
    UINTN Length
    )

/*++

Routine Description:

    This routine decodes the given UTF8 byte sequence into a codepoint.

Arguments:

    Bytes - Supplies a pointer to the byte stream.

    Length - Supplies the remaining length of the bytestream.

Return Value:

    Returns the decoded codepoint on success.

    -1 if the bytestream is invalid.

--*/

{

    INT Character;
    INT Remaining;

    CK_ASSERT(Length != 0);

    if (*Bytes <= 0x7F) {
        return *Bytes;
    }

    if ((*Bytes & 0xE0) == 0xC0) {
        Character = *Bytes & 0x1F;
        Remaining = 1;

    } else if ((*Bytes & 0xF0) == 0xE0) {
        Character = *Bytes & 0x0F;
        Remaining = 2;

    } else if ((*Bytes & 0xF8) == 0xF0) {
        Character = *Bytes & 0x07;
        Remaining = 3;

    } else {
        return -1;
    }

    if (Remaining > Length - 1) {
        return -1;
    }

    while (Remaining != 0) {
        Bytes += 1;
        Remaining -= 1;
        if ((*Bytes & 0x80) == 0) {
            return -1;
        }

        Character = (Character << 6) | (*Bytes & 0x3F);
    }

    return Character;
}

PCK_STRING
CkpStringAllocate (
    PCK_VM Vm,
    UINTN Length
    )

/*++

Routine Description:

    This routine allocates a new string object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Length - Supplies the length of the string in bytes, not including the null
        terminator.

Return Value:

    Returns a pointer to the newly allocated string object on success.

    NULL on allocation failure.

--*/

{

    PCK_STRING String;

    String = CkAllocate(Vm, sizeof(CK_STRING) + Length + 1);
    if (String == NULL) {
        return NULL;
    }

    CkpInitializeObject(Vm,
                        &(String->Header),
                        CkObjectString,
                        Vm->Class.String);

    String->Length = Length;
    String->Value = (PSTR)(String + 1);
    ((PSTR)String->Value)[Length] = '\0';
    return String;
}

VOID
CkpStringHash (
    PCK_STRING String
    )

/*++

Routine Description:

    This routine hashes a string. The hash value is saved in the string object.

Arguments:

    String - Supplies a pointer to the string.

Return Value:

    None.

--*/

{

    ULONG Hash;
    UINTN Index;

    //
    // Use the FNV-1a hash, as it's pretty fast and has good collision
    // avoidance.
    //

    Hash = 0x811C9DC5;
    for (Index = 0; Index < String->Length; Index += 1) {
        Hash ^= String->Value[Index];
        Hash *= 0x1000193;
    }

    String->Hash = Hash;
    return;
}

CK_VALUE
CkpStringFake (
    PCK_STRING FakeStringObject,
    PCSTR String,
    UINTN Length
    )

/*++

Routine Description:

    This routine initializes a temporary string object, usually used as a local
    variable in a C function. It's important that this string not get saved
    anywhere that might stick around after this fake string goes out of scope.

Arguments:

    FakeStringObject - Supplies a pointer to the string object storage to
        initialize.

    String - Supplies a pointer to the string to use.

    Length - Supplies the length of the string in bytes, not including the null
        terminator.

Return Value:

    Returns a string value for the fake string.

--*/

{

    CK_VALUE Value;

    FakeStringObject->Header.Type = CkObjectString;
    FakeStringObject->Header.Next = NULL;
    FakeStringObject->Header.Class = NULL;
    FakeStringObject->Length = Length;
    FakeStringObject->Value = String;
    CkpStringHash(FakeStringObject);
    CK_OBJECT_VALUE(Value, FakeStringObject);
    return Value;
}

//
// --------------------------------------------------------- Internal Functions
//

//
// String primitives
//

BOOL
CkpStringFromCharacter (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine converts a UTF8 codepoint into a string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INTEGER CodePoint;

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected an integer");
        return FALSE;
    }

    CodePoint = CK_AS_INTEGER(Arguments[1]);
    if ((CodePoint < 0) || (CodePoint > CK_MAX_UTF8)) {
        CkpRuntimeError(Vm, "ValueError", "Invalid UTF8 code point");
        return FALSE;
    }

    Arguments[0] = CkpStringCreateFromCharacter(Vm, CodePoint);
    return TRUE;
}

BOOL
CkpStringFromByte (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine converts a single byte into a string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INTEGER Value;

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected an integer");
        return FALSE;
    }

    Value = CK_AS_INTEGER(Arguments[1]);
    Arguments[0] = CkpStringCreate(Vm, (PCSTR)&Value, 1);
    return TRUE;
}

BOOL
CkpStringByteAt (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the byte of the string at the given index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Index;
    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);
    Index = CkpGetIndex(Vm, Arguments[1], String->Length);
    if (Index == MAX_UINTN) {
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0], (UCHAR)(String->Value[Index]));
    return TRUE;
}

BOOL
CkpStringCharacterAt (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the UTF8 codepoint at the given byte offset.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    INT Character;
    INTN Index;
    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);
    Index = CkpGetIndex(Vm, Arguments[1], String->Length);
    if (Index == MAX_UINTN) {
        return FALSE;
    }

    Character = CkpUtf8Decode((PUCHAR)(String->Value) + Index,
                              String->Length - Index);

    if (Character < 0) {
        CkpRuntimeError(Vm, "ValueError", "Invalid UTF-8 character");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0], Character);
    return TRUE;
}

BOOL
CkpStringContains (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines if one string is present within another.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_STRING Haystack;
    UINTN Index;
    PCK_STRING Needle;

    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    Haystack = CK_AS_STRING(Arguments[0]);
    Needle = CK_AS_STRING(Arguments[1]);
    Index = CkpStringFind(Haystack, 0, Needle);
    if (Index == (UINTN)-1) {
        Arguments[0] = CkZeroValue;

    } else {
        Arguments[0] = CkOneValue;
    }

    return TRUE;
}

BOOL
CkpStringStartsWith (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines if one string starts with another.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_STRING Haystack;
    PCK_STRING Needle;

    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    Haystack = CK_AS_STRING(Arguments[0]);
    Needle = CK_AS_STRING(Arguments[1]);
    if ((Needle->Length > Haystack->Length) ||
        (CkCompareMemory(Needle->Value, Haystack->Value, Needle->Length) !=
         0)) {

        Arguments[0] = CkZeroValue;

    } else {
        Arguments[0] = CkOneValue;
    }

    return TRUE;
}

BOOL
CkpStringEndsWith (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines if one string ends with another.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    INT Compare;
    PCK_STRING Haystack;
    PCSTR HaystackEnd;
    PCK_STRING Needle;

    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    Haystack = CK_AS_STRING(Arguments[0]);
    Needle = CK_AS_STRING(Arguments[1]);
    if (Needle->Length > Haystack->Length) {
        Arguments[0] = CkZeroValue;

    } else {
        HaystackEnd = Haystack->Value + Haystack->Length - Needle->Length;
        Compare = CkCompareMemory(HaystackEnd, Needle->Value, Needle->Length);
        if (Compare == 0) {
            Arguments[0] = CkOneValue;

        } else {
            Arguments[0] = CkZeroValue;
        }
    }

    return TRUE;
}

BOOL
CkpStringIndexOf (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine finds the index of the given string within the receiver.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_STRING Haystack;
    UINTN Index;
    PCK_STRING Needle;

    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    Haystack = CK_AS_STRING(Arguments[0]);
    Needle = CK_AS_STRING(Arguments[1]);
    Index = CkpStringFind(Haystack, 0, Needle);
    if (Index == (UINTN)-1) {
        CK_INT_VALUE(Arguments[0], -1);

    } else {
        CK_INT_VALUE(Arguments[0], Index);
    }

    return TRUE;
}

BOOL
CkpStringRightIndexOf (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine finds the last index of the given string within the receiver.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_STRING Haystack;
    UINTN Index;
    PCK_STRING Needle;

    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    Haystack = CK_AS_STRING(Arguments[0]);
    Needle = CK_AS_STRING(Arguments[1]);
    Index = CkpStringFindLast(Haystack, Needle);
    if (Index == (UINTN)-1) {
        CK_INT_VALUE(Arguments[0], -1);

    } else {
        CK_INT_VALUE(Arguments[0], Index);
    }

    return TRUE;
}

BOOL
CkpStringIterate (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine initializes or advances a string iterator, which iterates
    over UTF8 characters.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INTEGER Index;
    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);

    //
    // Initialize a new iterator.
    //

    if (CK_IS_NULL(Arguments[1])) {
        if (String->Length == 0) {
            Arguments[0] = CkNullValue;
            return TRUE;
        }

        CK_INT_VALUE(Arguments[0], 0);
        return TRUE;
    }

    //
    // Advance the iterator.
    //

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected an integer");
        return FALSE;
    }

    Index = CK_AS_INTEGER(Arguments[1]);
    if (Index < 0) {
        Arguments[0] = CkNullValue;
        return TRUE;
    }

    do {
        Index += 1;
        if (Index >= String->Length) {
            Arguments[0] = CkNullValue;
            return TRUE;
        }

    } while ((String->Value[Index] & 0xC0) == 0x80);

    CK_INT_VALUE(Arguments[0], Index);
    return TRUE;
}

BOOL
CkpStringIteratorValue (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the iterator value for the given iterator.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    return CkpStringCharacterAt(Vm, Arguments);
}

BOOL
CkpStringLower (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns a new string that converts all uppercase ASCII
    characters into lowercase characters.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_STRING Copy;
    UINTN Index;
    PCK_STRING Source;
    PSTR String;

    Source = CK_AS_STRING(Arguments[0]);
    Arguments[0] = CkpStringCreate(Vm, Source->Value, Source->Length);
    if (CK_IS_NULL(Arguments[0])) {
        return TRUE;
    }

    Copy = CK_AS_STRING(Arguments[0]);
    String = (PSTR)(Copy->Value);
    for (Index = 0; Index < Copy->Length; Index += 1) {
        *String = tolower(*String);
        String += 1;
    }

    return TRUE;
}

BOOL
CkpStringUpper (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns a new string that converts all lowercase ASCII
    characters into uppercase characters.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_STRING Copy;
    UINTN Index;
    PCK_STRING Source;
    PSTR String;

    Source = CK_AS_STRING(Arguments[0]);
    Arguments[0] = CkpStringCreate(Vm, Source->Value, Source->Length);
    if (CK_IS_NULL(Arguments[0])) {
        return TRUE;
    }

    Copy = CK_AS_STRING(Arguments[0]);
    String = (PSTR)(Copy->Value);
    for (Index = 0; Index < Copy->Length; Index += 1) {
        *String = toupper(*String);
        String += 1;
    }

    return TRUE;
}

BOOL
CkpStringLength (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the length of the string in bytes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);
    CK_INT_VALUE(Arguments[0], String->Length);
    return TRUE;
}

BOOL
CkpStringJoinList (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns a new string containing all the strings in the given
    list, separated by this string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PSTR Buffer;
    PCK_STRING Element;
    UINTN Index;
    PCK_LIST List;
    PCK_STRING Result;
    UINTN Size;
    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);
    if (!CK_IS_LIST(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a list");
        return FALSE;
    }

    List = CK_AS_LIST(Arguments[1]);

    //
    // Figure out how big the final string will be.
    //

    Size = 0;
    for (Index = 0; Index < List->Elements.Count; Index += 1) {
        if (!CK_IS_STRING(List->Elements.Data[Index])) {
            CkpRuntimeError(Vm,
                            "TypeError",
                            "Element %u is not a string",
                            Index);

            return FALSE;
        }

        Element = CK_AS_STRING(List->Elements.Data[Index]);
        Size += Element->Length;
        if (Index < List->Elements.Count - 1) {
            Size += String->Length;
        }
    }

    //
    // Optimization: if there's only one element in the list, just return that
    // element. This has to happen after the check to make sure it's a string.
    //

    if (List->Elements.Count == 1) {
        Arguments[0] = List->Elements.Data[0];
        return TRUE;
    }

    //
    // Allocate the string, then copy the members over.
    //

    Result = CkpStringAllocate(Vm, Size);
    if (Result == NULL) {
        return FALSE;
    }

    Buffer = (PSTR)(Result->Value);
    for (Index = 0; Index < List->Elements.Count; Index += 1) {
        Element = CK_AS_STRING(List->Elements.Data[Index]);
        CkCopy(Buffer, Element->Value, Element->Length);
        Buffer += Element->Length;
        if (Index < List->Elements.Count - 1) {
            CkCopy(Buffer, String->Value, String->Length);
            Buffer += String->Length;
        }
    }

    CK_ASSERT(Buffer == Result->Value + Result->Length);

    CkpStringHash(Result);
    CK_OBJECT_VALUE(Arguments[0], Result);
    return TRUE;
}

BOOL
CkpStringSplit (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine splits the given string based on the separator.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_VALUE Element;
    UINTN EndIndex;
    UINTN Index;
    PCK_LIST List;
    UINTN ListIndex;
    CK_INTEGER MaxSplit;
    UINTN NextIndex;
    PCK_STRING Separator;
    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);

    //
    // This routine takes a separator string, and a max count.
    //

    if (((!CK_IS_STRING(Arguments[1])) && (!CK_IS_NULL(Arguments[1]))) ||
        (!CK_IS_INTEGER(Arguments[2]))) {

        CkpRuntimeError(Vm, "TypeError", "Expected a string and an integer");
        return FALSE;
    }

    Separator = NULL;
    if (CK_IS_STRING(Arguments[1])) {
        Separator = CK_AS_STRING(Arguments[1]);
    }

    MaxSplit = CK_AS_INTEGER(Arguments[2]);
    if (MaxSplit == -1LL) {
        MaxSplit = String->Length;
    }

    List = CkpListCreate(Vm, 0);
    if (List == NULL) {
        return FALSE;
    }

    CkpPushRoot(Vm, &(List->Header));
    ListIndex = 0;
    Index = 0;

    //
    // If there's no separator, then it's a slightly different loop because
    // an empty string results in an empty list.
    //

    if (Separator == NULL) {
        while (Index < String->Length) {
            while ((Index < String->Length) &&
                   (isspace(String->Value[Index]))) {

                Index += 1;
            }

            if (Index == String->Length) {
                break;
            }

            if (ListIndex >= MaxSplit) {
                NextIndex = String->Length;

            } else {
                NextIndex = Index;
                while ((NextIndex < String->Length) &&
                       (!isspace(String->Value[NextIndex]))) {

                    NextIndex += 1;
                }
            }

            Element = CkpStringCreate(Vm,
                                      String->Value + Index,
                                      NextIndex - Index);

            CkpListInsert(Vm, List, Element, ListIndex);
            ListIndex += 1;
            Index = NextIndex;
        }

    } else {
        while (TRUE) {
            if (ListIndex >= MaxSplit) {
                NextIndex = (UINTN)-1;

            } else {
                NextIndex = CkpStringFind(String, Index, Separator);
            }

            EndIndex = NextIndex;
            if (NextIndex == (UINTN)-1) {
                EndIndex = String->Length;
            }

            Element = CkpStringCreate(Vm,
                                      String->Value + Index,
                                      EndIndex - Index);

            CkpListInsert(Vm, List, Element, ListIndex);
            ListIndex += 1;
            Index = NextIndex;
            if (Index == (UINTN)-1) {
                break;
            }

            Index += Separator->Length;
        }
    }

    CK_OBJECT_VALUE(Arguments[0], List);
    CkpPopRoot(Vm);
    return TRUE;
}

BOOL
CkpStringRightSplit (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine splits the given string based on the separator, from the right.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_VALUE Element;
    UINTN EndIndex;
    UINTN Index;
    PCK_LIST List;
    UINTN ListIndex;
    CK_INTEGER MaxSplit;
    UINTN NextIndex;
    UINTN OriginalLength;
    PCK_STRING Separator;
    UINTN StartIndex;
    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);

    //
    // This routine takes a separator string, and a max count.
    //

    if (((!CK_IS_STRING(Arguments[1])) && (!CK_IS_NULL(Arguments[1]))) ||
        (!CK_IS_INTEGER(Arguments[2]))) {

        CkpRuntimeError(Vm, "TypeError", "Expected a string and an integer");
        return FALSE;
    }

    Separator = NULL;
    if (CK_IS_STRING(Arguments[1])) {
        Separator = CK_AS_STRING(Arguments[1]);
    }

    MaxSplit = CK_AS_INTEGER(Arguments[2]);
    if (MaxSplit == -1LL) {
        MaxSplit = String->Length;
    }

    List = CkpListCreate(Vm, 0);
    if (List == NULL) {
        return FALSE;
    }

    CkpPushRoot(Vm, &(List->Header));
    ListIndex = 0;
    Index = String->Length - 1;
    EndIndex = (UINTN)-1;
    OriginalLength = String->Length;

    //
    // If there's no separator, then it's a slightly different loop because
    // an empty string results in an empty list.
    //

    if (Separator == NULL) {
        while (Index != EndIndex) {
            while ((Index != EndIndex) &&
                   (isspace(String->Value[Index]))) {

                Index -= 1;
            }

            if (Index == EndIndex) {
                break;
            }

            if (ListIndex >= MaxSplit) {
                NextIndex = EndIndex;

            } else {
                NextIndex = Index;
                while ((NextIndex != EndIndex) &&
                       (!isspace(String->Value[NextIndex]))) {

                    NextIndex -= 1;
                }
            }

            Element = CkpStringCreate(Vm,
                                      String->Value + NextIndex + 1,
                                      Index - NextIndex);

            CkpListInsert(Vm, List, Element, 0);
            ListIndex += 1;
            Index = NextIndex;
        }

    } else {

        //
        // This one's tricky because the primary index, which is on the right,
        // is inclusive. The lower index (next) is exclusive.
        //

        while (TRUE) {
            if (ListIndex >= MaxSplit) {
                NextIndex = (UINTN)-1;

            } else {
                String->Length = Index + 1;
                NextIndex = CkpStringFindLast(String, Separator);
                String->Length = OriginalLength;
            }

            if (NextIndex == (UINTN)-1) {
                StartIndex = -1;

            } else {
                StartIndex = NextIndex + Separator->Length - 1;
            }

            Element = CkpStringCreate(Vm,
                                      String->Value + StartIndex + 1,
                                      Index - StartIndex);

            CkpListInsert(Vm, List, Element, 0);
            ListIndex += 1;
            Index = NextIndex;
            if (Index == EndIndex) {
                break;
            }

            Index -= 1;
        }
    }

    CK_OBJECT_VALUE(Arguments[0], List);
    CkpPopRoot(Vm);
    return TRUE;
}

BOOL
CkpStringReplace (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine replaces all occurrences of one string within another.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Index;
    CK_INTEGER MaxReplace;
    PCK_STRING New;
    UINTN NextIndex;
    PCK_STRING Old;
    PSTR Out;
    UINTN ReplaceIndex;
    PCK_STRING Result;
    UINTN Size;
    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);

    //
    // The arguments are the old string, the new string, and the max
    // replacement count.
    //

    if ((!CK_IS_STRING(Arguments[1])) ||
        (!CK_IS_STRING(Arguments[2])) ||
        (!CK_IS_INTEGER(Arguments[3]))) {

        CkpRuntimeError(Vm, "TypeError", "Expected two strings and an integer");
        return FALSE;
    }

    Old = CK_AS_STRING(Arguments[1]);
    New = CK_AS_STRING(Arguments[2]);
    MaxReplace = CK_AS_INTEGER(Arguments[3]);

    //
    // Determine the size of the new string by counting occurrences of the old
    // string.
    //

    Size = String->Length;
    Index = 0;
    ReplaceIndex = 0;
    while (ReplaceIndex < (UINTN)MaxReplace) {
        Index = CkpStringFind(String, Index, Old);
        if (Index == (UINTN)-1) {
            break;
        }

        Index += Old->Length;
        Size += New->Length - Old->Length;
        ReplaceIndex += 1;
    }

    Result = CkpStringAllocate(Vm, Size);
    if (Result == NULL) {
        return FALSE;
    }

    Out = (PSTR)(Result->Value);

    //
    // Now create the resulting string.
    //

    Index = 0;
    ReplaceIndex = 0;
    while (TRUE) {

        //
        // If the replacement count is hit, pretend like no more instances were
        // found.
        //

        if (ReplaceIndex >= (UINTN)MaxReplace) {
            NextIndex = -1;

        } else {
            NextIndex = CkpStringFind(String, Index, Old);
        }

        //
        // If there are no more instances, copy the remainder of the string and
        // break.
        //

        if (NextIndex == (UINTN)-1) {
            CkCopy(Out, String->Value + Index, String->Length - Index);

            CK_ASSERT(Out + (String->Length - Index) ==
                      Result->Value + Result->Length);

            break;
        }

        //
        // Copy up to the next instance.
        //

        CkCopy(Out, String->Value + Index, NextIndex - Index);
        Out += NextIndex - Index;

        //
        // Copy the new string.
        //

        CkCopy(Out, New->Value, New->Length);
        Out += New->Length;

        //
        // Advance beyond the old string.
        //

        Index = NextIndex + Old->Length;
        ReplaceIndex += 1;
    }

    CkpStringHash(Result);
    CK_OBJECT_VALUE(Arguments[0], Result);
    return TRUE;
}

BOOL
CkpStringCompare (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine compares two strings.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN CommonLength;
    PCSTR Left;
    PCK_STRING LeftString;
    PCSTR Right;
    PCK_STRING RightString;

    LeftString = CK_AS_STRING(Arguments[0]);
    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    RightString = CK_AS_STRING(Arguments[1]);
    Left = LeftString->Value;
    Right = RightString->Value;

    //
    // Compare the minimum of their two lengths. Byte-wise compare works on
    // UTF-8 strings.
    //

    CommonLength = LeftString->Length;
    if (CommonLength > RightString->Length) {
        CommonLength = RightString->Length;
    }

    while (CommonLength != 0) {
        if (*Left != *Right) {
            CK_INT_VALUE(Arguments[0],
                         (unsigned char)*Left - (unsigned char )*Right);

            return TRUE;
        }

        Left += 1;
        Right += 1;
        CommonLength -= 1;
    }

    //
    // If they're the same length and they got this far, they completely match.
    //

    if (LeftString->Length == RightString->Length) {
        CK_INT_VALUE(Arguments[0], 0);
        return TRUE;
    }

    //
    // If the left is at the end, then 0 - Right.
    //

    if (Left == LeftString->Value + LeftString->Length) {
        CK_INT_VALUE(Arguments[0], 0 - (unsigned char)*Right);
        return TRUE;
    }

    //
    // The right must be at the end. Return Left - 0.
    //

    CK_INT_VALUE(Arguments[0], (unsigned char)*Left);
    return TRUE;
}

BOOL
CkpStringAdd (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine concatenates two strings.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_STRING Left;
    PCK_STRING Result;
    PCK_STRING Right;

    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    Left = CK_AS_STRING(Arguments[0]);
    Right = CK_AS_STRING(Arguments[1]);
    if (Left->Length == 0) {
        Arguments[0] = Arguments[1];
        return TRUE;

    } else if (Right->Length == 0) {
        return TRUE;
    }

    Result = CkpStringAllocate(Vm, Left->Length + Right->Length);
    if (Result == NULL) {
        return FALSE;
    }

    CkCopy((PSTR)(Result->Value), Left->Value, Left->Length);
    CkCopy((PSTR)(Result->Value + Left->Length), Right->Value, Right->Length);
    CkpStringHash(Result);
    CK_OBJECT_VALUE(Arguments[0], Result);
    return TRUE;
}

BOOL
CkpStringMultiply (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine multiplies a string and an integer, which duplicates the
    string as many times as the integer specifies. Multipliers less than or
    equal to zero result in the empty string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INTEGER Count;
    PSTR Current;
    UINTN Index;
    PCK_STRING Result;
    UINTN ResultLength;
    PCK_STRING Source;
    UINTN SourceLength;

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected an integer");
        return FALSE;
    }

    Count = CK_AS_INTEGER(Arguments[1]);
    if (Count <= 0) {
        Arguments[0] = CkpStringCreate(Vm, NULL, 0);
        return TRUE;
    }

    Source = CK_AS_STRING(Arguments[0]);
    SourceLength = Source->Length;
    ResultLength = Count * SourceLength;
    if (ResultLength / Count != SourceLength) {
        CkpRuntimeError(Vm, "ValueError", "Value too big");
        return FALSE;
    }

    Result = CkpStringAllocate(Vm, ResultLength);
    if (Result == NULL) {
        return FALSE;
    }

    Current = (PSTR)(Result->Value);
    for (Index = 0; Index < Count; Index += 1) {
        CkCopy(Current, Source->Value, SourceLength);
        Current += SourceLength;
    }

    CkpStringHash(Result);
    CK_OBJECT_VALUE(Arguments[0], Result);
    return TRUE;
}

BOOL
CkpStringSlice (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns a substring slice of a given string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Count;
    INTN Start;
    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);
    if (CK_IS_INTEGER(Arguments[1])) {
        Start = CK_AS_INTEGER(Arguments[1]);
        if (Start < 0) {
            Start += String->Length;
        }

        if ((Start < 0) || (Start >= String->Length)) {
            CkpRuntimeError(Vm, "IndexError", "String index out of range");
            return FALSE;
        }

        Count = 1;

    } else {
        if (!CK_IS_RANGE(Arguments[1])) {
            CkpRuntimeError(Vm, "TypeError", "Expected an integer or range");
            return FALSE;
        }

        Count = String->Length;
        Start = CkpGetRange(Vm, CK_AS_RANGE(Arguments[1]), &Count);
        if (Start == MAX_UINTN) {
            return FALSE;
        }
    }

    Arguments[0] = CkpStringSliceBytes(Vm, String, Start, Count);
    return TRUE;
}

BOOL
CkpStringSliceCharacters (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns a substring slice of a given string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Count;
    INTN Start;
    PCK_STRING String;

    String = CK_AS_STRING(Arguments[0]);
    if (CK_IS_INTEGER(Arguments[1])) {
        Start = CK_AS_INTEGER(Arguments[1]);
        if (Start < 0) {
            Start += String->Length;
        }

        if ((Start < 0) || (Start >= String->Length)) {
            CkpRuntimeError(Vm, "IndexError", "String index out of range");
            return FALSE;
        }

        Count = 1;

    } else {
        if (!CK_IS_RANGE(Arguments[1])) {
            CkpRuntimeError(Vm, "TypeError", "Expected an integer or range");
            return FALSE;
        }

        Count = String->Length;
        Start = CkpGetRange(Vm, CK_AS_RANGE(Arguments[1]), &Count);
        if (Start == MAX_UINTN) {
            return FALSE;
        }
    }

    Arguments[0] = CkpStringCreateFromCharacterRange(Vm, String, Start, Count);
    return TRUE;
}

BOOL
CkpStringToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine implements the no-op function of converting a string to a
    string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    return TRUE;
}

BOOL
CkpStringRepresentation (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine implements the string representation function, which can be
    evaluated back into the interpreter.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PSTR Destination;
    CHAR Digit;
    PCUCHAR End;
    UINTN Length;
    PCK_STRING Result;
    PCUCHAR Source;
    PCK_STRING SourceString;

    SourceString = CK_AS_STRING(Arguments[0]);

    //
    // Figure out how long the string is.
    //

    Source = (PCUCHAR)(SourceString->Value);
    End = Source + SourceString->Length;
    Length = 2;
    while (Source < End) {
        if (*Source == '"') {
            Length += 2;

        } else if ((*Source >= ' ') && (*Source < 0x7F)) {
            Length += 1;

        } else if ((*Source <= '\r') &&
                   (CkStringEscapes[(INT)*Source] != '?')) {

            Length += 2;

        } else {
            Length += 4;
        }

        Source += 1;
    }

    Result = CkpStringAllocate(Vm, Length);
    if (Result == NULL) {
        return FALSE;
    }

    Destination = (PSTR)(Result->Value);
    Source = (PCUCHAR)(SourceString->Value);
    *Destination = '"';
    Destination += 1;
    while (Source < End) {
        if (*Source == '"') {
            *Destination = '\\';
            Destination += 1;
            *Destination = '"';

        } else if ((*Source >= ' ') && (*Source < 0x7F)) {
            *Destination = *Source;

        } else if ((*Source <= '\r') &&
                   (CkStringEscapes[(INT)*Source] != '?')) {

            *Destination = '\\';
            Destination += 1;
            *Destination = CkStringEscapes[(INT)*Source];

        } else {
            *Destination = '\\';
            Destination += 1;
            *Destination = 'x';
            Destination += 1;
            Digit = *Source >> 4;
            if (Digit > 9) {
                *Destination = 'A' + Digit - 0xA;

            } else {
                *Destination = '0' + Digit;
            }

            Destination += 1;
            Digit = *Source & 0x0F;
            if (Digit > 9) {
                *Destination = 'A' + Digit - 0xA;

            } else {
                *Destination = '0' + Digit;
            }
        }

        Destination += 1;
        Source += 1;
    }

    *Destination = '"';
    Destination += 1;

    CK_ASSERT(Destination == Result->Value + Result->Length);

    CkpStringHash(Result);
    CK_OBJECT_VALUE(Arguments[0], Result);
    return TRUE;
}

CK_VALUE
CkpStringCreateFromCharacterRange (
    PCK_VM Vm,
    PCK_STRING Source,
    UINTN Start,
    UINTN Count
    )

/*++

Routine Description:

    This routine creates a new string object based on a portion of another
    string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Source - Supplies a pointer to the source string.

    Start - Supplies the starting index to slice from.

    Count - Supplies the number of characters to slice.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

{

    INT Character;
    UINTN CurrentIndex;
    PUCHAR From;
    UINTN Index;
    UINTN Length;
    PCK_STRING NewString;
    UINTN SourceLength;
    PUCHAR To;
    CK_VALUE Value;

    CK_ASSERT(Source->Header.Type == CkObjectString);

    //
    // Reuse the old string if the whole thing is being copied.
    //

    if ((Start == 0) && (Count >= Source->Length)) {
        CK_OBJECT_VALUE(Value, Source);
        return Value;
    }

    From = (PUCHAR)(Source->Value);
    SourceLength = Source->Length;
    Length = 0;
    for (Index = 0; Index < Count; Index += 1) {
        CurrentIndex = Start + Index;

        CK_ASSERT(CurrentIndex < SourceLength);

        Length += CkpUtf8DecodeSize(From[CurrentIndex]);
    }

    NewString = CkpStringAllocate(Vm, Length);
    if (NewString == NULL) {
        return CK_NULL_VALUE;
    }

    To = (PUCHAR)(NewString->Value);
    for (Index = 0; Index < Count; Index += 1) {
        CurrentIndex = Start + Index;
        Character = CkpUtf8Decode(From + CurrentIndex,
                                  SourceLength - CurrentIndex);

        if (Character != -1) {
            To += CkpUtf8Encode(Character, To);
        }
    }

    CkpStringHash(NewString);
    CK_OBJECT_VALUE(Value, NewString);
    return Value;
}

CK_VALUE
CkpStringSliceBytes (
    PCK_VM Vm,
    PCK_STRING Source,
    UINTN Start,
    UINTN Count
    )

/*++

Routine Description:

    This routine creates a new string object based on a portion of another
    string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Source - Supplies a pointer to the source string.

    Start - Supplies the starting byte index to slice from.

    Count - Supplies the number of bytes to slice.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

{

    CK_VALUE Value;

    CK_ASSERT(Source->Header.Type == CkObjectString);
    CK_ASSERT((Start <= Source->Length) && (Start + Count <= Source->Length));

    //
    // Reuse the old string if the whole thing is being copied.
    //

    if ((Start == 0) && (Count >= Source->Length)) {
        CK_OBJECT_VALUE(Value, Source);
        return Value;
    }

    return CkpStringCreate(Vm, Source->Value + Start, Count);
}

