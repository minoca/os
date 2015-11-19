/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    cif.c

Abstract:

    This module implements support for interfacing the Chalk interpreter with
    C functions and structures.

Author:

    Evan Green 19-Oct-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

INT
ChalkConvertDictToStructure (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Dict,
    PCHALK_C_STRUCTURE_MEMBER Members,
    PVOID Structure
    )

/*++

Routine Description:

    This routine converts the contents of a dictionary into a C structure in
    a mechanical way.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Dict - Supplies a pointer to the dictionary.

    Members - Supplies a pointer to an array of structure member definitions.
        This array must be terminated by a zeroed out member (type invalid,
        key null).

    Structure - Supplies a pointer to the structure to set up according to the
        dictionary. Any strings created by this function, even in failure,
        must be freed by the caller.

Return Value:

    0 on success.

    ENOENT if a required member was not found in the dictionary.

    Returns an error number on failure.

--*/

{

    PCHALK_DICT_ENTRY DictEntry;
    LONGLONG Int;
    CHALK_STRING LocalKeyString;
    ULONG Mask;
    PVOID Pointer;
    ULONG Shift;
    ULONG Size;
    INT Status;
    PSTR String;
    PCHALK_OBJECT Value;

    if (Dict->Header.Type == ChalkObjectReference) {
        Dict = Dict->Reference.Value;
    }

    assert(Dict->Header.Type == ChalkObjectDict);

    memset(&LocalKeyString, 0, sizeof(LocalKeyString));
    LocalKeyString.Header.Type = ChalkObjectString;

    //
    // Loop across all the members.
    //

    while (Members->Key != NULL) {
        LocalKeyString.String = Members->Key;
        LocalKeyString.Size = strlen(Members->Key);
        Pointer = Structure + Members->Offset;
        DictEntry = ChalkDictLookup(Dict, (PCHALK_OBJECT)&LocalKeyString);
        if (DictEntry == NULL) {
            if (Members->Required != FALSE) {
                fprintf(stderr,
                        "Error: Member %s is required.\n",
                        Members->Key);

                return ENOENT;
            }

            Members += 1;
            continue;
        }

        Value = DictEntry->Value;

        //
        // Check for compatibility and perform the write.
        //

        switch (Members->Type) {
        case ChalkCInt8:
        case ChalkCUint8:
        case ChalkCInt16:
        case ChalkCUint16:
        case ChalkCInt32:
        case ChalkCUint32:
        case ChalkCInt64:
        case ChalkCUint64:
        case ChalkCFlag32:
            if (Value->Header.Type != ChalkObjectInteger) {
                fprintf(stderr,
                        "Error: Member %s must be an integer.\n",
                        Members->Key);

                return EINVAL;
            }

            Int = Value->Integer.Value;
            switch (Members->Type) {
            case ChalkCInt8:
                *((PCHAR)Pointer) = Int;
                break;

            case ChalkCUint8:
                *((PUCHAR)Pointer) = Int;
                break;

            case ChalkCInt16:
                *((PSHORT)Pointer) = Int;
                break;

            case ChalkCUint16:
                *((PUSHORT)Pointer) = Int;
                break;

            case ChalkCInt32:
                *((PLONG)Pointer) = Int;
                break;

            case ChalkCUint32:
                *((PULONG)Pointer) = Int;
                break;

            case ChalkCInt64:
                *((PLONGLONG)Pointer) = Int;
                break;

            case ChalkCUint64:
                *((PULONGLONG)Pointer) = Int;
                break;

            case ChalkCFlag32:
                Mask = Members->U.Mask;
                if (Mask == 0) {
                    break;
                }

                Shift = 0;
                while ((Mask & (1 << Shift)) == 0) {
                    Shift += 1;
                }

                *((PULONG)Pointer) &= ~Mask;
                *((PULONG)Pointer) |= (Int << Shift) & Mask;
                break;

            default:

                assert(FALSE);

                return EINVAL;
            }

            break;

        case ChalkCString:
        case ChalkCByteArray:
            if (Value->Header.Type != ChalkObjectString) {
                fprintf(stderr,
                        "Error: Member %s must be a string.\n",
                        Members->Key);

                return EINVAL;
            }

            Size = Value->String.Size;
            if (Members->Type == ChalkCString) {
                String = malloc(Size + 1);
                if (String == NULL) {
                    return ENOMEM;
                }

            } else {

                assert(Members->Type == ChalkCByteArray);

                String = Pointer;
                if (Size > Members->U.Size) {
                    Size = Members->U.Size;
                }
            }

            memcpy(String, Value->String.String, Size);
            if (Members->Type == ChalkCString) {
                String[Size] = '\0';
                *((PSTR *)Pointer) = String;

            } else if (Size < Members->U.Size) {
                String[Size] = '\0';
            }

            break;

        case ChalkCSubStructure:
        case ChalkCStructurePointer:
            if (Value->Header.Type == ChalkObjectReference) {
                Value = Value->Reference.Value;
            }

            if (Value->Header.Type != ChalkObjectDict) {
                fprintf(stderr,
                        "Error: Member %s must be a dictionary.\n",
                        Members->Key);

                return EINVAL;
            }

            //
            // Recurse into the substructure.
            //

            if (Members->Type == ChalkCStructurePointer) {
                Pointer = *((PVOID *)Pointer);
            }

            Status = ChalkConvertDictToStructure(Interpreter,
                                                 Value,
                                                 Members->U.SubStructure,
                                                 Pointer);

            if (Status != 0) {
                return Status;
            }

            break;

        default:

            assert(FALSE);

            return EINVAL;
        }

        Members += 1;
    }

    return 0;
}

INT
ChalkConvertStructureToDict (
    PCHALK_INTERPRETER Interpreter,
    PVOID Structure,
    PCHALK_C_STRUCTURE_MEMBER Members,
    PCHALK_OBJECT Dict
    )

/*++

Routine Description:

    This routine converts the contents of a C structure into a dictionary in a
    mechanical way.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Structure - Supplies a pointer to the structure to get values from. Copies
        of any strings within the structure are made, and do not need to be
        preserved after this function returns.

    Members - Supplies a pointer to an array of structure member definitions.
        This array must be terminated by a zeroed out member (type invalid,
        key null).

    Dict - Supplies a pointer to the dictionary to set members for.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    LONGLONG Integer;
    PCHALK_OBJECT Key;
    ULONG Mask;
    PVOID Pointer;
    ULONG Shift;
    ULONG Size;
    INT Status;
    PSTR String;
    PCHALK_OBJECT Value;

    if (Dict->Header.Type == ChalkObjectReference) {
        Dict = Dict->Reference.Value;
    }

    assert(Dict->Header.Type == ChalkObjectDict);

    //
    // Loop across all the members.
    //

    while (Members->Key != NULL) {
        Key = ChalkCreateString(Members->Key, strlen(Members->Key));
        if (Key == NULL) {
            return ENOMEM;
        }

        Pointer = Structure + Members->Offset;
        switch (Members->Type) {
        case ChalkCInt8:
        case ChalkCUint8:
        case ChalkCInt16:
        case ChalkCUint16:
        case ChalkCInt32:
        case ChalkCUint32:
        case ChalkCInt64:
        case ChalkCUint64:
        case ChalkCFlag32:
            switch (Members->Type) {
            case ChalkCInt8:
                Integer = *((PCHAR)Pointer);
                break;

            case ChalkCUint8:
                Integer = *((PUCHAR)Pointer);
                break;

            case ChalkCInt16:
                Integer = *((PSHORT)Pointer);
                break;

            case ChalkCUint16:
                Integer = *((PUSHORT)Pointer);
                break;

            case ChalkCInt32:
                Integer = *((PLONG)Pointer);
                break;

            case ChalkCUint32:
                Integer = *((PULONG)Pointer);
                break;

            case ChalkCInt64:
                Integer = *((PLONGLONG)Pointer);
                break;

            case ChalkCUint64:
                Integer = *((PULONGLONG)Pointer);
                break;

            case ChalkCFlag32:
                Integer = *((PULONG)Pointer);
                Shift = 0;
                Mask = Members->U.Mask;
                while ((Mask & (1 << Shift)) == 0) {
                    Shift += 1;
                }

                Integer = (Integer & Mask) >> Shift;
                break;

            default:

                assert(FALSE);

                Status = EINVAL;
                goto ConvertStructureToDictEnd;
            }

            Value = ChalkCreateInteger(Integer);
            break;

        case ChalkCString:
        case ChalkCByteArray:
            String = Pointer;
            if (Members->Type == ChalkCString) {
                String = *((PSTR *)Pointer);
            }

            Size = strlen(String);
            Value = ChalkCreateString(String, Size);
            break;

        case ChalkCStructurePointer:
            Pointer = *((PVOID *)Pointer);

            //
            // Fall through.
            //

        case ChalkCSubStructure:
            Value = ChalkCreateDict(NULL);
            if (Value == NULL) {
                Status = ENOMEM;
                goto ConvertStructureToDictEnd;
            }

            Status = ChalkConvertStructureToDict(Interpreter,
                                                 Pointer,
                                                 Members->U.SubStructure,
                                                 Value);

            if (Status != 0) {
                goto ConvertStructureToDictEnd;
            }

            break;

        default:

            assert(FALSE);

            Status = EINVAL;
            goto ConvertStructureToDictEnd;
        }

        if (Value == NULL) {
            Status = ENOMEM;
            goto ConvertStructureToDictEnd;
        }

        Status = ChalkDictSetElement(Dict, Key, Value, NULL);
        if (Status != 0) {
            Status = ENOMEM;
            goto ConvertStructureToDictEnd;
        }

        Key = NULL;
        Members += 1;
    }

ConvertStructureToDictEnd:
    if (Key != NULL) {
        ChalkObjectReleaseReference(Key);
    }

    return Status;
}

INT
ChalkReadStringsList (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT List,
    PSTR **StringsArray
    )

/*++

Routine Description:

    This routine converts a list of strings into an array of null-terminated
    C strings. Items that are not strings are ignored.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    List - Supplies a pointer to the list.

    StringsArray - Supplies a pointer where the array of strings will be
        returned. This will be one giant allocation, the caller simply needs to
        free the array itself to free all the internal strings.

Return Value:

    0 on success.

    EINVAL if the given object was not a list or contained something other than
    strings.

    ENOMEM on allocation failure.

--*/

{

    UINTN AllocationSize;
    PSTR *Array;
    PSTR Buffer;
    UINTN Count;
    UINTN Index;
    PCHALK_OBJECT Item;

    if (List->Header.Type == ChalkObjectReference) {
        List = List->Reference.Value;
    }

    if (List->Header.Type != ChalkObjectList) {
        return EINVAL;
    }

    //
    // Go through once to count the strings and size.
    //

    AllocationSize = 0;
    Count = 0;
    for (Index = 0; Index < List->List.Count; Index += 1) {
        Item = List->List.Array[Index];
        if ((Item == NULL) || (Item->Header.Type != ChalkObjectString)) {
            continue;
        }

        Count += 1;
        AllocationSize += Item->String.Size + 1;
    }

    AllocationSize += (Count + 1) * sizeof(PVOID);
    Array = malloc(AllocationSize);
    if (Array == NULL) {
        return ENOMEM;
    }

    Buffer = (PSTR)(Array + Count + 1);
    Array[Count] = NULL;

    //
    // Go through again and copy them out.
    //

    Count = 0;
    for (Index = 0; Index < List->List.Count; Index += 1) {
        Item = List->List.Array[Index];
        if ((Item == NULL) || (Item->Header.Type != ChalkObjectString)) {
            continue;
        }

        Array[Count] = Buffer;
        memcpy(Buffer, Item->String.String, Item->String.Size);
        Buffer[Item->String.Size] = '\0';
        Count += 1;
        Buffer += Item->String.Size + 1;
    }

    *StringsArray = Array;
    return 0;
}

INT
ChalkWriteStringsList (
    PCHALK_INTERPRETER Interpreter,
    PSTR *StringsArray,
    PCHALK_OBJECT List
    )

/*++

Routine Description:

    This routine converts an array of C strings into a list of string objects.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    StringsArray - Supplies a pointer to a NULL-terminated array of C strings.

    List - Supplies a pointer to the list to add the strings to.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    PCHALK_OBJECT NewString;
    UINTN Size;
    INT Status;
    PSTR String;

    assert(List->Header.Type == ChalkObjectList);

    while (*StringsArray != NULL) {
        String = *StringsArray;
        Size = strlen(String);
        NewString = ChalkCreateString(String, Size);
        if (NewString == NULL) {
            return ENOMEM;
        }

        Status = ChalkListSetElement(List, List->List.Count, NewString);
        ChalkObjectReleaseReference(NewString);
        if (Status != 0) {
            return Status;
        }

        StringsArray += 1;
    }

    return 0;
}

PCHALK_OBJECT
ChalkDictLookupCStringKey (
    PCHALK_OBJECT Dict,
    PSTR Key
    )

/*++

Routine Description:

    This routine looks up a dictionary object with the given C string key.

Arguments:

    Dict - Supplies a pointer to the dictionary to look in.

    Key - Supplies a pointer to the key string to look up.

Return Value:

    Returns a pointer to the value object for the given key on success. Note
    that the reference count on this object is not increased.

    NULL if no value for the given key exists.

--*/

{

    PCHALK_DICT_ENTRY DictEntry;
    CHALK_STRING FakeString;
    PCHALK_OBJECT Value;

    if (Dict->Header.Type == ChalkObjectReference) {
        Dict = Dict->Reference.Value;
    }

    assert(Dict->Header.Type == ChalkObjectDict);

    FakeString.Header.Type = ChalkObjectString;
    FakeString.Header.ReferenceCount = 0;
    FakeString.String = Key;
    FakeString.Size = strlen(Key);
    DictEntry = ChalkDictLookup(Dict, (PCHALK_OBJECT)&FakeString);
    if (DictEntry == NULL) {
        return NULL;
    }

    //
    // Skip through references for convenience.
    //

    Value = DictEntry->Value;
    if ((Value != NULL) &&
        (Value->Header.Type == ChalkObjectReference)) {

        Value = Value->Reference.Value;
    }

    return Value;
}

//
// --------------------------------------------------------- Internal Functions
//

