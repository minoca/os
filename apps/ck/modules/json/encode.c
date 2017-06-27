/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    encode.c

Abstract:

    This module implements the Chalk JSON encoder.

Author:

    Evan Green 19-May-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jsonp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the stack index of the result.
//

#define CK_JSON_DECODE_RESULT 3

//
// Define the stack index of the dictionary used to determine if an object has
// already been encoded.
//

#define CK_JSON_CHECK_DICT 4

//
// Define the initial size of the JSON decoder buffer.
//

#define CK_JSON_INITIAL_BUFFER_SIZE 256

//
// Define the maximum decoded integer size, which is the length required for
// -9223372036854775808 (or 0x8000000000000000 in hex).
//

#define CK_JSON_MAX_INTEGER_SIZE 20

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the context for the JSON encoder.

Members:

    Vm - Stores a pointer to the Chalk interpreter.

    Indent - Stores the indentation to use.

    NameSeparator - Stores the separator between keys and values.

    ListSeparator - Stores the separator between list elements.

    NameSeparatorLength - Stores the length of the name separator, not
        including the null terminator.

    ListSeparatorLength - Stores the length of the list separator, not
        including the null terminator.

    Recursion - Stores the current recursion level.

    Result - Stores the resulting output string.

    Length - Stores the length of the output in bytes.

    Capacity - Stores the maximum capacity of the result buffer before it must
        be reloaded.

--*/

typedef struct _CK_JSON_ENCODER {
    PCK_VM Vm;
    INT Indent;
    PCSTR NameSeparator;
    PCSTR ListSeparator;
    UINTN NameSeparatorLength;
    UINTN ListSeparatorLength;
    INTN Recursion;
    PSTR Result;
    UINTN Length;
    UINTN Capacity;
} CK_JSON_ENCODER, *PCK_JSON_ENCODER;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
CkpJsonEncodeObject (
    PCK_JSON_ENCODER Encoder
    );

INT
CkpJsonEncodeDict (
    PCK_JSON_ENCODER Encoder
    );

INT
CkpJsonEncodeList (
    PCK_JSON_ENCODER Encoder
    );

INT
CkpJsonEncodeInteger (
    PCK_JSON_ENCODER Encoder,
    CK_INTEGER Integer
    );

INT
CkpJsonEncodeString (
    PCK_JSON_ENCODER Encoder,
    PCSTR String,
    UINTN Length
    );

INT
CkpJsonEncodeRawString (
    PCK_JSON_ENCODER Encoder,
    PCSTR String,
    UINTN Length
    );

INT
CkpJsonEncoderRecursionCheck (
    PCK_JSON_ENCODER Encoder
    );

VOID
CkpJsonEnccoderRecursionUnwind (
    PCK_JSON_ENCODER Encoder
    );

INT
CkpJsonPrintIndentation (
    PCK_JSON_ENCODER Encoder
    );

INT
CkpJsonUtf8Decode (
    PUCHAR Bytes,
    PUINTN InIndex,
    UINTN Length
    );

INT
CkpJsonEnsureBuffer (
    PCK_JSON_ENCODER Encoder,
    UINTN Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpJsonEncode (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the entry point into the JSON encoder. It takes
    two arguments: the object to encode, and the amount to indent objects by.
    If indent is less than or equal to zero, then the object will be encoded
    with no whitespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns a string containing the JSON representation of the object on
    success.

    Raises an exception on failure.

--*/

{

    CK_JSON_ENCODER Encoder;
    CK_INTEGER Indent;
    INT Status;

    if (!CkCheckArgument(Vm, 2, CkTypeInteger)) {
        return;
    }

    if (!CkEnsureStack(Vm, 15)) {
        return;
    }

    Indent = CkGetInteger(Vm, 2);
    memset(&Encoder, 0, sizeof(CK_JSON_ENCODER));
    Encoder.Vm = Vm;
    Encoder.Indent = Indent;
    if ((Encoder.Indent != Indent) || (Indent < 0)) {
        Encoder.Indent = 0;
    }

    if (Encoder.Indent != 0) {
        Encoder.NameSeparator = ": ";
        Encoder.ListSeparator = ", ";
        Encoder.NameSeparatorLength = 2;
        Encoder.ListSeparatorLength = 2;

    } else {
        Encoder.NameSeparator = ":";
        Encoder.ListSeparator = ",";
        Encoder.NameSeparatorLength = 1;
        Encoder.ListSeparatorLength = 1;
    }

    //
    // Create a result buffer.
    //

    Encoder.Result = CkPushStringBuffer(Vm, CK_JSON_INITIAL_BUFFER_SIZE);
    if (Encoder.Result == NULL) {
        return;
    }

    Encoder.Length = 0;
    Encoder.Capacity = CK_JSON_INITIAL_BUFFER_SIZE;

    //
    // Create a dict to search for recursive entries with.
    //

    CkPushDict(Vm);

    //
    // Push the element to dump on the stack.
    //

    CkPushValue(Vm, 1);
    Status = CkpJsonEncodeObject(&Encoder);
    if (Status != 0) {
        return;
    }

    //
    // Pop the check dictionary.
    //

    CkStackPop(Vm);

    //
    // Finalize the result string and set it as the return value.
    //

    CkFinalizeString(Vm, CK_JSON_DECODE_RESULT, Encoder.Length);
    CkStackReplace(Vm, 0);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
CkpJsonEncodeObject (
    PCK_JSON_ENCODER Encoder
    )

/*++

Routine Description:

    This routine dumps an object into JSON format. The object is assumed to be
    on top of the stack.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

Return Value:

    0 on success.

    Non-zero if an exception occurred.

--*/

{

    UINTN Length;
    INT Result;
    PCSTR String;
    CK_API_TYPE Type;
    PCK_VM Vm;

    Vm = Encoder->Vm;
    Type = CkGetType(Vm, -1);
    switch (Type) {
    case CkTypeInteger:
        Result = CkpJsonEncodeInteger(Encoder, CkGetInteger(Encoder->Vm, -1));
        break;

    case CkTypeString:
        String = CkGetString(Encoder->Vm, -1, &Length);
        Result = CkpJsonEncodeString(Encoder, String, Length);
        break;

    case CkTypeNull:
        Result = CkpJsonEncodeRawString(Encoder, "null", 4);
        break;

    case CkTypeDict:
        Result = CkpJsonEncodeDict(Encoder);
        break;

    case CkTypeList:
        Result = CkpJsonEncodeList(Encoder);
        break;

    default:
        CkRaiseBasicException(Vm,
                              "TypeError",
                              "Type cannot be converted to JSON");

        return -1;
    }

    if (Result == 0) {
        CkStackPop(Vm);
    }

    return Result;
}

INT
CkpJsonEncodeDict (
    PCK_JSON_ENCODER Encoder
    )

/*++

Routine Description:

    This routine dumps a dictionary into JSON format.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

Return Value:

    0 on success.

    Non-zero if an exception occurred.

--*/

{

    INT Status;
    PCK_VM Vm;

    Status = CkpJsonEncoderRecursionCheck(Encoder);
    if (Status != 0) {
        return Status;
    }

    //
    // Add the open curly.
    //

    Status = CkpJsonEnsureBuffer(Encoder, 1);
    if (Status != 0) {
        return Status;
    }

    Vm = Encoder->Vm;
    Encoder->Result[Encoder->Length] = '{';
    Encoder->Length += 1;
    if (Encoder->Indent != 0) {
        CkpJsonPrintIndentation(Encoder);
    }

    //
    // Iterate over each element in the dictionary.
    //

    CkPushNull(Vm);
    if (CkDictIterate(Vm, -2) != FALSE) {
        while (TRUE) {

            //
            // Push the key up to the top and dump it. The dump routine pops it.
            //

            CkPushValue(Vm, -2);
            Status = CkpJsonEncodeObject(Encoder);
            if (Status != 0) {
                return Status;
            }

            //
            // Add the colon.
            //

            Status = CkpJsonEncodeRawString(Encoder,
                                            Encoder->NameSeparator,
                                            Encoder->NameSeparatorLength);

            if (Status != 0) {
                return Status;
            }

            //
            // Dump the value, popping it off.
            //

            Status = CkpJsonEncodeObject(Encoder);
            if (Status != 0) {
                return Status;
            }

            //
            // Pop off the original key.
            //

            CkStackPop(Vm);

            //
            // If there are no more elements, stop.
            //

            if (CkDictIterate(Vm, -2) == FALSE) {
                break;
            }

            //
            // Print a separator.
            //

            Status = CkpJsonEncodeRawString(Encoder,
                                            Encoder->ListSeparator,
                                            Encoder->ListSeparatorLength);

            if (Status != 0) {
                return Status;
            }

            if (Encoder->Indent != 0) {
                CkpJsonPrintIndentation(Encoder);
            }
        }
    }

    //
    // Pop the used up iterator.
    //

    CkStackPop(Vm);
    CkpJsonEnccoderRecursionUnwind(Encoder);

    //
    // Potentially print a newline and then print the object terminator.
    //

    if (Encoder->Indent != 0) {
        CkpJsonPrintIndentation(Encoder);
    }

    Status = CkpJsonEnsureBuffer(Encoder, 1);
    if (Status != 0) {
        return Status;
    }

    Encoder->Result[Encoder->Length] = '}';
    Encoder->Length += 1;
    return 0;
}

INT
CkpJsonEncodeList (
    PCK_JSON_ENCODER Encoder
    )

/*++

Routine Description:

    This routine dumps a list into JSON format.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

Return Value:

    0 on success.

    Non-zero if an exception occurred.

--*/

{

    UINTN Index;
    UINTN ListSize;
    INT Status;
    PCK_VM Vm;

    Status = CkpJsonEncoderRecursionCheck(Encoder);
    if (Status != 0) {
        return Status;
    }

    //
    // Add the open curly.
    //

    Status = CkpJsonEnsureBuffer(Encoder, 1);
    if (Status != 0) {
        return Status;
    }

    Vm = Encoder->Vm;
    Encoder->Result[Encoder->Length] = '[';
    Encoder->Length += 1;
    if (Encoder->Indent != 0) {
        CkpJsonPrintIndentation(Encoder);
    }

    //
    // Iterate over each element in the dictionary.
    //

    ListSize = CkListSize(Vm, -1);
    for (Index = 0; Index < ListSize; Index += 1) {
        CkListGet(Vm, -1, Index);
        Status = CkpJsonEncodeObject(Encoder);
        if (Status != 0) {
            return Status;
        }

        //
        // If this is not the last element, print a separator.
        //

        if (Index != ListSize - 1) {
            Status = CkpJsonEncodeRawString(Encoder,
                                            Encoder->ListSeparator,
                                            Encoder->ListSeparatorLength);

            if (Status != 0) {
                return Status;
            }

            if (Encoder->Indent != 0) {
                CkpJsonPrintIndentation(Encoder);
            }
        }
    }

    CkpJsonEnccoderRecursionUnwind(Encoder);

    //
    // Potentially print a newline and then print the object terminator.
    //

    if (Encoder->Indent != 0) {
        CkpJsonPrintIndentation(Encoder);
    }

    Status = CkpJsonEnsureBuffer(Encoder, 1);
    if (Status != 0) {
        return Status;
    }

    Encoder->Result[Encoder->Length] = ']';
    Encoder->Length += 1;
    return 0;
}

INT
CkpJsonEncodeInteger (
    PCK_JSON_ENCODER Encoder,
    CK_INTEGER Integer
    )

/*++

Routine Description:

    This routine dumps an integer into JSON format.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

    Integer - Supplies the value to dump.

Return Value:

    0 on success.

    Non-zero if an exception occurred.

--*/

{

    INT Length;
    INT Status;

    Status = CkpJsonEnsureBuffer(Encoder, CK_JSON_MAX_INTEGER_SIZE);
    if (Status != 0) {
        return Status;
    }

    Length = snprintf(Encoder->Result + Encoder->Length,
                      Encoder->Capacity - Encoder->Length,
                      "%lld",
                      Integer);

    if (Length <= 0) {
        CkRaiseBasicException(Encoder->Vm,
                              "ValueError",
                              "Could not convert integer");

        return -1;
    }

    Encoder->Length += Length;
    return 0;
}

INT
CkpJsonEncodeString (
    PCK_JSON_ENCODER Encoder,
    PCSTR String,
    UINTN Length
    )

/*++

Routine Description:

    This routine dumps a string into JSON format.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

    String - Supplies the string to convert.

    Length - Supplies the length of the string in bytes.

Return Value:

    0 on success.

    Non-zero if an exception occurred.

--*/

{

    INT Character;
    INT High;
    UINTN InIndex;
    UINTN OutIndex;
    UINTN OutLength;
    INT Status;

    OutLength = Length;

    //
    // The result is going to be at least the number of bytes plus two quotes.
    //

    OutLength += 2;
    Status = CkpJsonEnsureBuffer(Encoder, OutLength);
    if (Status != 0) {
        return Status;
    }

    //
    // Add the first quote.
    //

    OutIndex = Encoder->Length;
    Encoder->Result[OutIndex] = '"';
    OutIndex += 1;
    for (InIndex = 0; InIndex < Length; InIndex += 1) {

        //
        // Handle ASCII.
        //

        Character = (UCHAR)(String[InIndex]);
        if (Character < 0x80) {

            //
            // Double quote and reverse solidus need to be escaped as they mean
            // something to JSON.
            //

            if ((Character == '"') || (Character == '\\')) {
                OutLength += 1;
                Status = CkpJsonEnsureBuffer(Encoder, OutLength);
                if (Status != 0) {
                    return Status;
                }

                Encoder->Result[OutIndex] = '\\';
                Encoder->Result[OutIndex + 1] = Character;
                OutIndex += 2;

            //
            // Control characters should also be escaped.
            //

            } else if (Character < ' ') {
                if ((Character == '\b') || (Character == '\f') ||
                    (Character == '\n') || (Character == '\r') ||
                    (Character == '\t')) {

                    OutLength += 1;
                    Status = CkpJsonEnsureBuffer(Encoder, OutLength);
                    if (Status != 0) {
                        return Status;
                    }

                    switch (Character) {
                    case '\b':
                        Character = 'b';
                        break;

                    case '\f':
                        Character = 'f';
                        break;

                    case '\n':
                        Character = 'n';
                        break;

                    case '\r':
                        Character = 'r';
                        break;

                    case '\t':
                    default:
                        Character = 't';
                        break;
                    }

                    Encoder->Result[OutIndex] = '\\';
                    Encoder->Result[OutIndex + 1] = Character;
                    OutIndex += 2;

                //
                // Use the unicode escape.
                //

                } else {
                    OutLength += 5;
                    Status = CkpJsonEnsureBuffer(Encoder, OutLength);
                    if (Status != 0) {
                        return Status;
                    }

                    snprintf(Encoder->Result + OutIndex,
                             Encoder->Capacity - OutIndex,
                             "\\u%04X",
                             Character);

                    OutIndex += 6;
                }

            //
            // Just a regular old character.
            //

            } else {
                Encoder->Result[OutIndex] = Character;
                OutIndex += 1;
            }

        //
        // Decode a full blown UTF-8 character.
        //

        } else {
            Character = CkpJsonUtf8Decode((PUCHAR)String, &InIndex, Length);
            if (Character < 0) {
                CkRaiseBasicException(Encoder->Vm,
                                      "ValueError",
                                      "Invalid UTF-8 string");

                return -1;
            }

            //
            // If it's in the basic bilingual plane, it just needs \uHHHH.
            //

            if (Character < 0x10000) {
                OutLength += 5;
                Status = CkpJsonEnsureBuffer(Encoder, OutLength);
                if (Status != 0) {
                    return Status;
                }

                snprintf(Encoder->Result + OutIndex,
                         Encoder->Capacity - OutIndex,
                         "\\u%04X",
                         Character);

                OutIndex += 6;

            //
            // Create a Unicode surrogate pair, which will come out as:
            // \uHHHH\uHHHH
            //

            } else {
                Character -= 0x10000;
                OutLength += 11;
                Status = CkpJsonEnsureBuffer(Encoder, OutLength);
                if (Status != 0) {
                    return Status;
                }

                High = 0xD800 + ((Character >> 10) & 0x3FF);
                Character = 0xDC00 + (Character & 0x3FF);
                snprintf(Encoder->Result + OutIndex,
                         Encoder->Capacity - OutIndex,
                         "\\u%04X\\u%04X",
                         High,
                         Character);

                OutIndex += 12;
            }
        }
    }

    Encoder->Result[OutIndex] = '"';
    OutIndex += 1;

    assert((OutIndex <= Encoder->Capacity) &&
           (OutIndex >= Encoder->Length + 2));

    Encoder->Length = OutIndex;
    return 0;
}

INT
CkpJsonEncodeRawString (
    PCK_JSON_ENCODER Encoder,
    PCSTR String,
    UINTN Length
    )

/*++

Routine Description:

    This routine dumps a string recognized inherently by JSON, such as null,
    true, or false.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

    String - Supplies the string to convert.

    Length - Supplies the length of the string in bytes.

Return Value:

    0 on success.

    Non-zero if an exception occurred.

--*/

{

    INT Status;

    Status = CkpJsonEnsureBuffer(Encoder, Length);
    if (Status != 0) {
        return Status;
    }

    memcpy(Encoder->Result + Encoder->Length, String, Length);
    Encoder->Length += Length;
    return 0;
}

INT
CkpJsonEncoderRecursionCheck (
    PCK_JSON_ENCODER Encoder
    )

/*++

Routine Description:

    This routine checks to see if the given object is already on the stack of
    things being dumped, and raises an exception if so.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

Return Value:

    0 on success.

    Non-zero if an exception occurred.

--*/

{

    PCK_VM Vm;

    Vm = Encoder->Vm;
    Encoder->Recursion += 1;
    if (Encoder->Recursion > CK_JSON_MAX_RECURSION) {
        CkRaiseBasicException(Vm,
                              "ValueError",
                              "Maximum recursion depth exceeded");

        return -1;
    }

    //
    // Ensure the interpreter has enough stack available for the non-recursive
    // part of any element that might come along.
    //

    if (!CkEnsureStack(Vm, 10)) {
        return -1;
    }

    //
    // Check to see if the element at the top of the stack is already in the
    // dictionary.
    //

    CkPushValue(Vm, -1);
    if (CkDictGet(Vm, CK_JSON_CHECK_DICT) != FALSE) {
        CkRaiseBasicException(Vm, "ValueError", "Circular reference detected");
        return -1;
    }

    //
    // Add the item to the dictionary. The value doesn't matter.
    //

    CkPushValue(Vm, -1);
    CkPushInteger(Vm, 1);
    CkDictSet(Vm, CK_JSON_CHECK_DICT);
    return 0;
}

VOID
CkpJsonEnccoderRecursionUnwind (
    PCK_JSON_ENCODER Encoder
    )

/*++

Routine Description:

    This routine unwinds after recursion checks on an element are no longer
    necessary. The element that was processed should be on the top of the stack.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

Return Value:

    None.

--*/

{

    assert(Encoder->Recursion > 0);

    Encoder->Recursion -= 1;
    CkPushValue(Encoder->Vm, -1);
    CkDictRemove(Encoder->Vm, CK_JSON_CHECK_DICT);
    return;
}

INT
CkpJsonPrintIndentation (
    PCK_JSON_ENCODER Encoder
    )

/*++

Routine Description:

    This routine prints a newline and the number of spaces corresponding to
    the current indentation level.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

Return Value:

    0 on success.

    Non-zero on allocation failure.

--*/

{

    UINTN Count;
    INT Length;
    INT Status;

    Count = Encoder->Recursion * Encoder->Indent;
    Status = CkpJsonEnsureBuffer(Encoder, Count + 1);
    if (Status != 0) {
        return Status;
    }

    Length = snprintf(Encoder->Result + Encoder->Length,
                      Encoder->Capacity - Encoder->Length,
                      "\n%*s",
                      (int)Count,
                      "");

    if (Length > 0) {
        Encoder->Length += Length;
    }

    return 0;
}

INT
CkpJsonUtf8Decode (
    PUCHAR Bytes,
    PUINTN InIndex,
    UINTN Length
    )

/*++

Routine Description:

    This routine decodes the given UTF8 byte sequence into a codepoint.

Arguments:

    Bytes - Supplies a pointer to the byte stream.

    InIndex - Supplies a pointer that on input contains the index to the
        character to decode. On output points to the last byte of the character.
        That is, it's advanced character size minus one.

    Length - Supplies the remaining length of the bytestream.

Return Value:

    Returns the decoded codepoint on success.

    -1 if the bytestream is invalid.

--*/

{

    INT Character;
    INT Remaining;

    CK_ASSERT(Length != 0);

    Bytes += *InIndex;
    Length -= *InIndex;
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

    if (Remaining > Length) {
        return -1;
    }

    *InIndex += Remaining;
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

INT
CkpJsonEnsureBuffer (
    PCK_JSON_ENCODER Encoder,
    UINTN Size
    )

/*++

Routine Description:

    This routine ensures that there is the given amount of buffer space
    available in the JSON encoder.

Arguments:

    Encoder - Supplies a pointer to the encoder context.

    Size - Supplies the number of bytes needed, not including a null terminator.

Return Value:

    0 on success.

    Non-zero if an exception occurred.

--*/

{

    PSTR NewBuffer;
    UINTN NewCapacity;

    //
    // Usually the buffer will already have space.
    //

    Size += 1;
    if (Size <= Encoder->Capacity - Encoder->Length) {
        return 0;
    }

    //
    // Watch out for overflow.
    //

    if ((Encoder->Capacity >= MAX_UINTN / 2) ||
        (Encoder->Length + Size < Encoder->Length)) {

        return -1;
    }

    NewCapacity = Encoder->Capacity * 2;
    while (NewCapacity - Encoder->Length < Size) {

        assert(NewCapacity > Encoder->Capacity);

        NewCapacity *= 2;
    }

    //
    // Allocate a new string with the new buffer size, copy the old data over,
    // and put the result into the proper location on the stack.
    //

    NewBuffer = CkPushStringBuffer(Encoder->Vm, NewCapacity);
    if (NewBuffer == NULL) {
        return -1;
    }

    memcpy(NewBuffer, Encoder->Result, Encoder->Length);
    CkStackReplace(Encoder->Vm, CK_JSON_DECODE_RESULT);
    Encoder->Result = NewBuffer;
    Encoder->Capacity = NewCapacity;
    return 0;
}

