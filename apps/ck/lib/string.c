/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

#include <stdarg.h>
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

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

CK_VALUE
CkpStringCreate (
    PCK_VM Vm,
    PSTR Text,
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

    PCK_STRING_OBJECT String;
    CK_VALUE Value;

    String = CkpStringAllocate(Vm, Length);
    if (String == NULL) {
        return CK_NULL_VALUE;
    }

    if ((Length != 0) && (Text != NULL)) {
        memcpy(String->Value, Text, Length);
    }

    CkpStringHash(String);
    CK_OBJECT_VALUE(Value, String);
    return Value;
}

CK_VALUE
CkpStringCreateFromRange (
    PCK_VM Vm,
    PCK_STRING_OBJECT Source,
    UINTN Start,
    UINTN Count,
    LONG Step
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

    Step - Supplies the whether to increment (1) or decrement (-1).

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
    PCK_STRING_OBJECT NewString;
    UINTN SourceLength;
    PUCHAR To;
    CK_VALUE Value;

    CK_ASSERT(Source->Header.Type == CkObjectString);

    From = (PUCHAR)(Source->Value);
    SourceLength = Source->Length;
    Length = 0;
    for (Index = 0; Index < Count; Index += 1) {
        CurrentIndex = Start + (Index * Step);

        CK_ASSERT(CurrentIndex < SourceLength);

        Length += CkpUtf8DecodeSize(From[CurrentIndex]);
    }

    NewString = CkpStringAllocate(Vm, Length);
    if (NewString == NULL) {
        return CK_NULL_VALUE;
    }

    To = (PUCHAR)(NewString->Value);
    for (Index = 0; Index < Count; Index += 1) {
        CurrentIndex = Start + (Index * Step);
        Character = CkpUtf8Decode(From + Index, SourceLength - Index);
        if (Character != -1) {
            To += CkpUtf8Encode(Character, To);
        }
    }

    CkpStringHash(NewString);
    CK_OBJECT_VALUE(Value, NewString);
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
    PCK_STRING_OBJECT Source,
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
    PCK_STRING_OBJECT NewString;
    PSTR Out;
    PSTR Parameter;
    PCK_STRING_OBJECT StringParameter;
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
    Out = NewString->Value;
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
CkpStringFind (
    PCK_STRING_OBJECT Haystack,
    PCK_STRING_OBJECT Needle
    )

/*++

Routine Description:

    This routine searches for a given substring within a string.

Arguments:

    Haystack - Supplies a pointer to the string to search.

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
    Index = 0;
    while (Index < Haystack->Length - Needle->Length) {

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
        Bytes[0] = 0xC0 | ((Character & 0x7C0) >> 6);
        Bytes[1] = 0x80 | (Character & 0x3F);
        return 2;
    }

    if (Character <= 0xFFFF) {
        Bytes[0] = 0xE0 | ((Character & 0xF000) >> 12);
        Bytes[1] = 0x80 | ((Character & 0xFC0) >> 6);
        Bytes[2] = 0x80 | (Character & 0x3F);
        return 3;
    }

    if (Character <= CK_MAX_UTF8) {
        Bytes[0] = 0xF0 | ((Character & 0x1C0000) >> 18);
        Bytes[1] = 0x80 | ((Character & 0x3F00) >> 12);
        Bytes[2] = 0x80 | ((Character & 0xFC0) >> 6);
        Bytes[3] = 0x80 | (Character & 0x3F);
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

PCK_STRING_OBJECT
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

    PCK_STRING_OBJECT String;

    String = CkAllocate(Vm, sizeof(CK_STRING_OBJECT) + Length + 1);
    if (String == NULL) {
        return NULL;
    }

    CkpInitializeObject(Vm,
                        &(String->Header),
                        CkObjectString,
                        Vm->Class.String);

    String->Length = Length;
    String->Value = (PSTR)(String + 1);
    String->Value[Length] = '\0';
    return String;
}

VOID
CkpStringHash (
    PCK_STRING_OBJECT String
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

//
// --------------------------------------------------------- Internal Functions
//

