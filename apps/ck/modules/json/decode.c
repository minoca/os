/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    decode.c

Abstract:

    This module implements the Chalk JSON decoder.

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
#include <string.h>

#include "jsonp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the context for the JSON decoder.

Members:

    Vm - Stores a pointer to the Chalk interpreter.

    Recursion - Stores the current recursion depth.

    Start - Stores the original start of the JSON string to decode.

    String - Stores the current string pointer to decode.

    End - Stores a pointer one beyond the last valid character in the input.

--*/

typedef struct _CK_JSON_DECODER {
    PCK_VM Vm;
    INTN Recursion;
    PCSTR Start;
    PCSTR String;
    PCSTR End;
} CK_JSON_DECODER, *PCK_JSON_DECODER;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
CkpJsonDecodeObject (
    PCK_JSON_DECODER Decoder
    );

INT
CkpJsonDecodeDict (
    PCK_JSON_DECODER Decoder
    );

INT
CkpJsonDecodeList (
    PCK_JSON_DECODER Decoder
    );

INT
CkpJsonDecodeString (
    PCK_JSON_DECODER Decoder
    );

INT
CkpJsonDecodeNumber (
    PCK_JSON_DECODER Decoder
    );

INT
CkpJsonDecodeHexCharacter (
    PCK_JSON_DECODER Decoder,
    PCSTR String
    );

VOID
CkpJsonUtf8Encode (
    PSTR *Out,
    INT Character
    );

INT
CkpJsonSkipSpace (
    PCK_JSON_DECODER Decoder
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpJsonDecode (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the entry point into the JSON decoder. It takes
    one argument which can be in one of two forms. It can either be a JSON
    string to decode, or a list whose first element contains a JSON string to
    decode. In list form, a substring containing the remaining data will be
    returned in the list's first element. It returns the deserialized object,
    or raises an exception.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the deserialized object on success.

    Raises an exception on failure.

--*/

{

    CK_API_TYPE ArgumentType;
    CK_JSON_DECODER Decoder;
    UINTN Length;

    memset(&Decoder, 0, sizeof(CK_JSON_DECODER));
    Decoder.Vm = Vm;
    Decoder.Recursion = 0;

    //
    // If the argument is a list, then JSON to decode is the first element in
    // the list.
    //

    ArgumentType = CkGetType(Vm, 1);
    if (ArgumentType == CkTypeList) {
        CkListGet(Vm, 1, 0);
        if (CkGetType(Vm, 2) != CkTypeString) {
            CkRaiseBasicException(Vm,
                                  "TypeError",
                                  "List element should be a string");

            return;
        }

        Decoder.String = CkGetString(Vm, 2, &Length);

    //
    // Otherwise the argument is hopefully a JSON string to decode.
    //

    } else if (CkCheckArgument(Vm, 1, CkTypeString)) {
        Decoder.String = CkGetString(Vm, 1, &Length);

    //
    // Fail if it isn't.
    //

    } else {
        return;
    }

    Decoder.Start = Decoder.String;
    Decoder.End = Decoder.String + Length;
    if (CkpJsonDecodeObject(&Decoder) != 0) {
        return;
    }

    //
    // If the argument is a list, create a remainder string and set that as the
    // first list element.
    //

    if (ArgumentType == CkTypeList) {
        CkPushString(Vm, Decoder.String, Decoder.End - Decoder.String);
        CkListSet(Vm, 1, 0);
    }

    CkStackReplace(Vm, 0);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
CkpJsonDecodeObject (
    PCK_JSON_DECODER Decoder
    )

/*++

Routine Description:

    This routine decodes a single JSON object. This is a recursive function.

Arguments:

    Decoder - Supplies a pointer to the decoder context.

Return Value:

    0 on success. The decoded object will be on top of the stack.

    Returns non-zero on failure, and an exception will have been raised into
    Chalk.

--*/

{

    PCSTR End;
    INT Status;
    PCSTR String;
    BOOL Unknown;

    Decoder->Recursion += 1;
    if (Decoder->Recursion >= CK_JSON_MAX_RECURSION) {
        CkRaiseBasicException(Decoder->Vm,
                              "ValueError",
                              "Maximum recursion depth exceeded");

        return -1;
    }

    Status = CkpJsonSkipSpace(Decoder);
    if (Status != 0) {
        return Status;
    }

    String = Decoder->String;
    End = Decoder->End;
    Unknown = FALSE;
    switch (*String) {
    case '"':
        Status = CkpJsonDecodeString(Decoder);
        break;

    case '{':
        Status = CkpJsonDecodeDict(Decoder);
        break;

    case '[':
        Status = CkpJsonDecodeList(Decoder);
        break;

    //
    // Handle a possible null.
    //

    case 'n':
        if ((String + 4 <= End) &&
            (String[1] == 'u') && (String[2] == 'l') && (String[3] == 'l')) {

            CkPushNull(Decoder->Vm);
            Decoder->String += 4;
            Status = 0;

        } else {
            Status = -1;
            Unknown = TRUE;
        }

        break;

    //
    // Handle a possible true.
    //

    case 't':
        if ((String + 4 <= End) &&
            (String[1] == 'r') && (String[2] == 'u') && (String[3] == 'e')) {

            CkPushInteger(Decoder->Vm, 1);
            Decoder->String += 4;
            Status = 0;

        } else {
            Status = -1;
            Unknown = TRUE;
        }

        break;

    //
    // Handle a possible false.
    //

    case 'f':
        if ((String + 5 <= End) && (strncmp(String + 1, "alse", 4) == 0)) {
            CkPushInteger(Decoder->Vm, 0);
            Decoder->String += 5;
            Status = 0;

        } else {
            Status = -1;
            Unknown = TRUE;
        }

        break;

    //
    // Handle a possible NaN or Infinity.
    //

    case 'N':
    case 'I':
        if (((String + 3 <= End) && (strncmp(String, "NaN", 3) == 0)) ||
            ((String + 8 <= End) && (strncmp(String, "Infinity", 8) == 0))) {

            CkRaiseBasicException(Decoder->Vm,
                                  "ValueError",
                                  "Sorry, floats are currently not supported");

            Status = -1;

        } else {
            Status = -1;
            Unknown = TRUE;
        }

        break;

    default:

        //
        // Handle a -Infinity.
        //

        if ((*String == '-') && (String + 9 <= End) &&
            (strncmp(String + 1, "Infinity", 8) == 0)) {

            CkRaiseBasicException(Decoder->Vm,
                                  "ValueError",
                                  "Sorry, floats are currently not supported");

            Status = -1;

        //
        // Handle a number.
        //

        } else if ((*String == '-') || ((*String >= '0') && (*String <= '9'))) {
            Status = CkpJsonDecodeNumber(Decoder);

        //
        // This is unknown.
        //

        } else {
            Status = -1;
            Unknown = TRUE;
        }

        break;
    }

    if (Unknown != FALSE) {
        CkRaiseBasicException(Decoder->Vm,
                              "ValueError",
                              "Invalid JSON at offset %d, character was '%c'",
                              (long)(String - Decoder->Start),
                              *String);

        return -1;
    }

    assert(Decoder->Recursion > 0);

    Decoder->Recursion -= 1;
    return Status;
}

INT
CkpJsonDecodeDict (
    PCK_JSON_DECODER Decoder
    )

/*++

Routine Description:

    This routine decodes a JSON object.

Arguments:

    Decoder - Supplies a pointer to the decoder context.

Return Value:

    0 on success. The decoded object will be on top of the stack.

    Returns non-zero on failure, and an exception will have been raised into
    Chalk.

--*/

{

    PCSTR End;
    PCSTR Start;
    INT Status;

    End = Decoder->End;
    Start = Decoder->String;

    assert((Start < End) && (*Start == '{'));

    Decoder->String += 1;
    Status = CkpJsonSkipSpace(Decoder);
    if (Status != 0) {
        return Status;
    }

    //
    // Push a new dictionary, and loop decoding keys and values.
    //

    CkPushDict(Decoder->Vm);

    //
    // Perform an initial empty dictionary check.
    //

    if (Decoder->String[0] == '}') {
        Decoder->String += 1;
        return 0;
    }

    while (TRUE) {

        //
        // Decode a key.
        //

        Status = CkpJsonDecodeObject(Decoder);
        if (Status != 0) {
            return Status;
        }

        Status = CkpJsonSkipSpace(Decoder);
        if (Status != 0) {
            return Status;
        }

        //
        // Decode a colon.
        //

        if ((Decoder->String >= End) || (Decoder->String[0] != ':')) {
            CkRaiseBasicException(Decoder->Vm,
                                  "ValueError",
                                  "Expected a '%c' at offset %ld for dict "
                                  "starting at offset %ld",
                                  ':',
                                  (long)(Decoder->String - Decoder->Start),
                                  (long)(Start - Decoder->Start));

            return -1;
        }

        Decoder->String += 1;

        //
        // Decode a value.
        //

        Status = CkpJsonDecodeObject(Decoder);
        if (Status != 0) {
            return Status;
        }

        //
        // Set the key-value pair in the dictionary, which also pops them off.
        //

        CkDictSet(Decoder->Vm, -3);

        //
        // The next character needs to either be a comma or a closing curly.
        //

        Status = CkpJsonSkipSpace(Decoder);
        if (Status != 0) {
            return Status;
        }

        if (Decoder->String[0] == '}') {
            Decoder->String += 1;
            break;
        }

        if (Decoder->String[0] != ',') {
            CkRaiseBasicException(Decoder->Vm,
                                  "ValueError",
                                  "Expected a '%c' at offset %ld for dict "
                                  "starting at offset %ld",
                                  ',',
                                  (long)(Decoder->String - Decoder->Start),
                                  (long)(Start - Decoder->Start));

            return -1;
        }

        Decoder->String += 1;
    }

    return 0;
}

INT
CkpJsonDecodeList (
    PCK_JSON_DECODER Decoder
    )

/*++

Routine Description:

    This routine decodes a JSON string.

Arguments:

    Decoder - Supplies a pointer to the decoder context.

Return Value:

    0 on success. The decoded object will be on top of the stack.

    Returns non-zero on failure, and an exception will have been raised into
    Chalk.

--*/

{

    PCSTR End;
    UINTN ListIndex;
    PCSTR Start;
    INT Status;

    End = Decoder->End;
    Start = Decoder->String;

    assert((Start < End) && (*Start == '['));

    Decoder->String += 1;
    Status = CkpJsonSkipSpace(Decoder);
    if (Status != 0) {
        return Status;
    }

    //
    // Push a new list, and loop decoding entries.
    //

    CkPushList(Decoder->Vm);

    //
    // Perform an initial empty list check.
    //

    if (Decoder->String[0] == ']') {
        Decoder->String += 1;
        return 0;
    }

    ListIndex = 0;
    while (TRUE) {

        //
        // Decode an element.
        //

        Status = CkpJsonDecodeObject(Decoder);
        if (Status != 0) {
            return Status;
        }

        CkListSet(Decoder->Vm, -2, ListIndex);
        ListIndex += 1;
        Status = CkpJsonSkipSpace(Decoder);
        if (Status != 0) {
            return Status;
        }

        if (Decoder->String[0] == ']') {
            Decoder->String += 1;
            break;
        }

        if (Decoder->String[0] != ',') {
            CkRaiseBasicException(Decoder->Vm,
                                  "ValueError",
                                  "Expected a ',' at offset %ld for list "
                                  "starting at offset %ld",
                                  (long)(Decoder->String - Decoder->Start),
                                  (long)(Start - Decoder->Start));

            return -1;
        }

        Decoder->String += 1;
    }

    return 0;
}

INT
CkpJsonDecodeString (
    PCK_JSON_DECODER Decoder
    )

/*++

Routine Description:

    This routine decodes a JSON string.

Arguments:

    Decoder - Supplies a pointer to the decoder context.

Return Value:

    0 on success. The decoded object will be on top of the stack.

    Returns non-zero on failure, and an exception will have been raised into
    Chalk.

--*/

{

    INT Character;
    PCSTR End;
    INT High;
    UINTN MaxLength;
    PSTR Out;
    PSTR OutStart;
    PCSTR Start;
    PCSTR String;

    End = Decoder->End;
    Start = Decoder->String;

    assert((Start < End) && (*Start == '"'));

    Decoder->String += 1;

    //
    // Find the end of the string. Most of the time the final string length
    // will be equal to the encoded string length. At worst it might be a
    // little shorter, but it can never be longer due to the way \uHHHH encodes
    // 2 bytes, and surrogate pairs encode at most 4.
    //

    String = Start + 1;
    while ((String < End) && (*String != '"')) {
        if (*String == '\\') {
            String += 1;
        }

        String += 1;
    }

    if (String >= End) {
        CkRaiseBasicException(Decoder->Vm,
                              "ValueError",
                              "Unterminated string starting at offset %ld",
                              (long)(Start - Decoder->Start));

        return -1;
    }

    Decoder->String = String + 1;
    End = String;
    String = Start + 1;
    MaxLength = End - String;
    Out = CkPushStringBuffer(Decoder->Vm, MaxLength);
    if (Out == NULL) {
        return -1;
    }

    OutStart = Out;
    while (String < End) {

        //
        // Handle the easy and common case: no escapes.
        //

        if (*String != '\\') {
            *Out = *String;
            String += 1;
            Out += 1;
            continue;
        }

        String += 1;
        if (String == End) {
            CkRaiseBasicException(Decoder->Vm,
                                  "ValueError",
                                  "Dangling escape for string starting at "
                                  "offset %ld",
                                  (long)(Start - Decoder->Start));

            return -1;
        }

        Character = *String;
        String += 1;
        switch (Character) {
        case 'b':
            Character = '\b';
            break;

        case 'f':
            Character = '\f';
            break;

        case 'n':
            Character = '\n';
            break;

        case 'r':
            Character = '\r';
            break;

        case 't':
            Character = '\t';
            break;

        case 'u':
            if (String + 4 > End) {
                CkRaiseBasicException(Decoder->Vm,
                                      "ValueError",
                                      "Dangling escape for string starting at "
                                      "offset %ld",
                                      (long)(Start - Decoder->Start));
            }

            Character = CkpJsonDecodeHexCharacter(Decoder, String);
            if (Character == -1) {
                return -1;
            }

            String += 4;

            //
            // Watch for a surrogate pair, which looks like \uHHHH\uHHHH, where
            // the first set is in the range 0xD800-0xDC00, and the second
            // value is in the range 0xDC00-0xE000.
            //

            if ((Character >= 0xD800) && (Character < 0xDC00) &&
                (String + 6 <= End) &&
                (String[0] == '\\') && (String[1] == 'u')) {

                String += 2;
                High = CkpJsonDecodeHexCharacter(Decoder, String);
                if (High == -1) {
                    return -1;
                }

                String += 4;

                //
                // If it's a valid surrogate pair, then create a single
                // character out of the pair of them. Otherwise, go back and
                // handle it separately.
                //

                if ((High >= 0xDC00) && (High < 0xE000)) {
                    Character = 0x10000 + ((Character - 0xD800) << 10) +
                                (High - 0xDC00);

                } else {
                    String -= 6;
                }
            }

            break;

        //
        // Other characters can be escaped just for fun.
        //

        default:
            break;
        }

        CkpJsonUtf8Encode(&Out, Character);
    }

    assert(Out - OutStart <= MaxLength);

    CkFinalizeString(Decoder->Vm, -1, Out - OutStart);
    return 0;
}

INT
CkpJsonDecodeNumber (
    PCK_JSON_DECODER Decoder
    )

/*++

Routine Description:

    This routine decodes a JSON number.

Arguments:

    Decoder - Supplies a pointer to the decoder context.

Return Value:

    0 on success. The decoded object will be on top of the stack.

    Returns non-zero on failure, and an exception will have been raised into
    Chalk.

--*/

{

    PCSTR End;
    CK_INTEGER Integer;
    BOOL Negative;
    CK_INTEGER Previous;
    PCSTR String;

    Negative = FALSE;
    String = Decoder->String;
    End = Decoder->End;

    assert(String < End);

    if (*String == '-') {
        Negative = TRUE;
        String += 1;
    }

    if ((String >= End) ||
        ((!((*String >= '0') && (*String <= '9'))) &&
         (*String != 'e') && (*String != 'E') && (*String != '.'))) {

        CkRaiseBasicException(Decoder->Vm,
                              "ValueError",
                              "Invalid number at offset %ld",
                              (long)(String - Decoder->Start));

        return -1;
    }

    Integer = 0;
    while ((String < End) && (*String >= '0') && (*String <= '9')) {
        Previous = Integer;
        Integer = (Integer * 10) + (*String - '0');
        if (Integer < Previous) {
            CkRaiseBasicException(Decoder->Vm,
                                  "ValueError",
                                  "Integer overflow");

            return -1;
        }

        String += 1;
    }

    if ((String < End) &&
        ((*String == '.') || (*String == 'e') || (*String == 'E'))) {

        CkRaiseBasicException(Decoder->Vm,
                              "ValueError",
                              "Sorry, floats are currently not supported");

        return -1;
    }

    Decoder->String = String;
    if (Negative != FALSE) {
        Integer = -Integer;
    }

    CkPushInteger(Decoder->Vm, Integer);
    return 0;
}

INT
CkpJsonDecodeHexCharacter (
    PCK_JSON_DECODER Decoder,
    PCSTR String
    )

/*++

Routine Description:

    This routine decodes a 16-bit value expressed in 4 ASCII hex digits.

Arguments:

    Decoder - Supplies a pointer to the decoder context.

    String - Supplies a pointer to the 4 digits, which are assumed to be there.

Return Value:

    Returns the character on success.

    Returns -1 on failure.

--*/

{

    INT Character;
    INT Index;

    Character = 0;
    for (Index = 0; Index < 4; Index += 1) {
        Character = Character << 4;
        if ((*String >= '0') && (*String <= '9')) {
            Character |= *String - '0';

        } else if ((*String >= 'A') && (*String <= 'F')) {
            Character |= *String - 'A' + 0xA;

        } else if ((*String >= 'a') && (*String <= 'f')) {
            Character |= *String - 'a' + 0xA;

        } else {
            CkRaiseBasicException(Decoder->Vm,
                                  "ValueError",
                                  "Invalid unicode escape at offset %ld",
                                  (long)(String - Index - 2 - Decoder->Start));

            return -1;
        }

        String += 1;
    }

    return Character;
}

VOID
CkpJsonUtf8Encode (
    PSTR *Out,
    INT Character
    )

/*++

Routine Description:

    This routine encodes a UTF-8 character.

Arguments:

    Out - Supplies a pointer that on input points to the output buffer. This
        will be advanced on output. It is assumed to be large enough to hold
        the character.

    Character - Supplies the character to encode.

Return Value:

    None.

--*/

{

    PUCHAR Bytes;

    assert((Character >= 0) && (Character <= CK_MAX_UTF8));

    Bytes = (PUCHAR)*Out;
    if (Character <= 0x7F) {
        Bytes[0] = Character;
        *Out += 1;

    } else if (Character <= 0x7FF) {
        Bytes[0] = 0xC0 | (Character >> 6);
        Bytes[1] = 0x80 | (Character & 0x3F);
        *Out += 2;

    } else if (Character <= 0xFFFF) {
        Bytes[0] = 0xE0 | (Character >> 12);
        Bytes[1] = 0x80 | ((Character >> 6) & 0x3F);
        Bytes[2] = 0x80 | (Character & 0x3F);
        *Out += 3;

    } else if (Character <= CK_MAX_UTF8) {
        Bytes[0] = 0xF0 | (Character >> 18);
        Bytes[1] = 0x80 | ((Character >> 12) & 0x3F);
        Bytes[2] = 0x80 | ((Character >> 6) & 0x3F);
        Bytes[3] = 0x80 | (Character & 0x3F);
        *Out += 4;
    }

    return;
}

INT
CkpJsonSkipSpace (
    PCK_JSON_DECODER Decoder
    )

/*++

Routine Description:

    This routine skips whitespace and comments in the JSON decoder input stream.

Arguments:

    Decoder - Supplies a pointer to the decoder context.

Return Value:

    0 on success.

    -1 if the character was not found or the stream ended, in which case an
    exception will be raised.

--*/

{

    PCSTR End;
    PCSTR String;

    String = Decoder->String;
    End = Decoder->End;

    //
    // Loop skipping blank space and comments.
    //

    while (TRUE) {

        //
        // Skip any whitespace.
        //

        while ((String < End) &&
               ((*String == ' ') || (*String == '\t') || (*String == '\n') ||
                (*String == '\r') || (*String == '\v') || (*String == '\f') ||
                (*String == '\b'))) {

            String += 1;
        }

        //
        // Allow and ignore comments.
        //

        if ((String < End) && (*String == '#')) {
            while ((String < End) && (*String != '\n')) {
                String += 1;
            }

            if (String < End) {
                String += 1;
                continue;
            }
        }

        if (String >= End) {
            CkRaiseBasicException(Decoder->Vm,
                                  "ValueError",
                                  "Unexpected end of input");

            return -1;
        }

        break;
    }

    Decoder->String = String;
    return 0;
}

