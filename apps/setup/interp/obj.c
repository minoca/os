/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    obj.c

Abstract:

    This module handles low level object manipulation for setup.

Author:

    Evan Green 14-Oct-2015

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

#include "../setup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
SetupGutObject (
    PSETUP_OBJECT Object
    );

VOID
SetupDestroyList (
    PSETUP_OBJECT List
    );

VOID
SetupDestroyDict (
    PSETUP_OBJECT Dict
    );

VOID
SetupDestroyDictEntry (
    PSETUP_DICT_ENTRY Entry
    );

COMPARISON_RESULT
SetupCompareObjects (
    PSETUP_OBJECT Left,
    PSETUP_OBJECT Right
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR SetupObjectTypeNames[SetupObjectCount] = {
    "INVALID",
    "integer",
    "string",
    "dict",
    "list",
    "ref"
};

//
// ------------------------------------------------------------------ Functions
//

PSETUP_OBJECT
SetupCreateInteger (
    LONGLONG Value
    )

/*++

Routine Description:

    This routine creates a new integer object.

Arguments:

    Value - Supplies the initial value.

Return Value:

    Returns a pointer to the new integer on success.

    NULL on allocation failure.

--*/

{

    PSETUP_INT Int;

    Int = malloc(sizeof(SETUP_OBJECT));
    if (Int == NULL) {
        return NULL;
    }

    memset(Int, 0, sizeof(SETUP_OBJECT));
    Int->Header.Type = SetupObjectInteger;
    Int->Header.ReferenceCount = 1;
    Int->Value = Value;
    return (PSETUP_OBJECT)Int;
}

PSETUP_OBJECT
SetupCreateString (
    PSTR InitialValue,
    ULONG Size
    )

/*++

Routine Description:

    This routine creates a new string object.

Arguments:

    InitialValue - Supplies an optional pointer to the initial value.

    Size - Supplies the size of the initial value.

Return Value:

    Returns a pointer to the new string on success.

    NULL on allocation failure.

--*/

{

    ULONG AllocateSize;
    PSETUP_STRING String;

    String = malloc(sizeof(SETUP_OBJECT));
    if (String == NULL) {
        return NULL;
    }

    memset(String, 0, sizeof(SETUP_OBJECT));
    AllocateSize = Size + 1;
    String->String = malloc(AllocateSize);
    if (String->String == NULL) {
        free(String);
        return NULL;
    }

    memcpy(String->String, InitialValue, Size);
    String->String[AllocateSize - 1] = '\0';
    String->Size = Size;
    String->Header.Type = SetupObjectString;
    String->Header.ReferenceCount = 1;
    return (PSETUP_OBJECT)String;
}

INT
SetupStringAdd (
    PSETUP_OBJECT Left,
    PSETUP_OBJECT Right,
    PSETUP_OBJECT *Result
    )

/*++

Routine Description:

    This routine adds two strings together, concatenating them.

Arguments:

    Left - Supplies a pointer to the left side of the operation.

    Right - Supplies a pointer to the right side of the operation.

    Result - Supplies a pointer where the result will be returned on success.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    ULONG Size;
    PSETUP_OBJECT String;

    assert((Left->Header.Type == SetupObjectString) &&
           (Right->Header.Type == SetupObjectString));

    String = SetupCreateString(NULL, 0);
    if (String == NULL) {
        return ENOMEM;
    }

    //
    // The size does not account for the null terminator.
    //

    Size = Left->String.Size + Right->String.Size;
    String->String.String = malloc(Size + 1);
    if (String->String.String == NULL) {
        SetupObjectReleaseReference(String);
        return ENOMEM;
    }

    memcpy(String->String.String, Left->String.String, Left->String.Size);
    memcpy(String->String.String + Left->String.Size,
           Right->String.String,
           Right->String.Size);

    String->String.String[Size] = '\0';
    String->String.Size = Size;
    *Result = String;
    return 0;
}

PSETUP_OBJECT
SetupCreateList (
    PSETUP_OBJECT *InitialValues,
    ULONG Size
    )

/*++

Routine Description:

    This routine creates a new empty list object.

Arguments:

    InitialValues - Supplies an optional pointer to the initial values to set
        on the list.

    Size - Supplies the number of entries in the initial values array.

Return Value:

    Returns a pointer to the new string on success.

    NULL on allocation failure.

--*/

{

    ULONG Index;
    PSETUP_LIST List;

    List = malloc(sizeof(SETUP_OBJECT));
    if (List == NULL) {
        return NULL;
    }

    memset(List, 0, sizeof(SETUP_OBJECT));
    List->Header.Type = SetupObjectList;
    List->Header.ReferenceCount = 1;
    if (Size != 0) {
        List->Array = malloc(Size * sizeof(PVOID));
        if (List->Array == NULL) {
            free(List);
            return NULL;
        }

        List->Count = Size;
        if (InitialValues != NULL) {
            memcpy(List->Array, InitialValues, Size * sizeof(PVOID));
            for (Index = 0; Index < Size; Index += 1) {
                if (List->Array[Index] != NULL) {
                    SetupObjectAddReference(List->Array[Index]);
                }
            }

        } else {
            memset(List->Array, 0, Size * sizeof(PVOID));
        }
    }

    return (PSETUP_OBJECT)List;
}

PSETUP_OBJECT
SetupListLookup (
    PSETUP_OBJECT List,
    ULONG Index
    )

/*++

Routine Description:

    This routine looks up the value at a particular list index.

Arguments:

    List - Supplies a pointer to the list.

    Index - Supplies the index to lookup.

Return Value:

    Returns a pointer to the list element with an increased reference count on
    success.

    NULL if the object at that index does not exist.

--*/

{

    PSETUP_OBJECT Object;

    assert(List->Header.Type == SetupObjectList);

    if (Index >= List->List.Count) {
        return NULL;
    }

    Object = List->List.Array[Index];
    if (Object != NULL) {
        SetupObjectAddReference(Object);
    }

    return Object;
}

INT
SetupListSetElement (
    PSETUP_OBJECT ListObject,
    ULONG Index,
    PSETUP_OBJECT Object
    )

/*++

Routine Description:

    This routine sets the given list index to the given object.

Arguments:

    ListObject - Supplies a pointer to the list.

    Index - Supplies the index to set.

    Object - Supplies a pointer to the object to set at that list index. The
        reference count on the object will be increased on success.

Return Value:

    0 on success.

    Returns an error number on allocation failure.

--*/

{

    PSETUP_LIST List;
    PVOID NewBuffer;

    assert(ListObject->Header.Type == SetupObjectList);

    List = (PSETUP_LIST)ListObject;
    if (List->Count <= Index) {
        NewBuffer = realloc(List->Array, sizeof(PVOID) * (Index + 1));
        if (NewBuffer == NULL) {
            return ENOMEM;
        }

        List->Array = NewBuffer;
        memset(&(List->Array[List->Count]),
               0,
               sizeof(PVOID) * (Index + 1 - List->Count));

        List->Count = Index + 1;
    }

    if (List->Array[Index] != NULL) {
        SetupObjectReleaseReference(List->Array[Index]);
    }

    List->Array[Index] = Object;
    if (Object != NULL) {
        SetupObjectAddReference(Object);
    }

    return 0;
}

INT
SetupListAdd (
    PSETUP_OBJECT Destination,
    PSETUP_OBJECT Addition
    )

/*++

Routine Description:

    This routine adds two lists together, storing the result in the first.

Arguments:

    Destination - Supplies a pointer to the destination. The list elements will
        be added to this list.

    Addition - Supplies the list containing the elements to add.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    ULONG Index;
    PSETUP_LIST LeftList;
    PSETUP_OBJECT *NewArray;
    ULONG NewSize;
    PSETUP_LIST RightList;

    LeftList = (PSETUP_LIST)Destination;
    RightList = (PSETUP_LIST)Addition;

    assert((LeftList->Header.Type == SetupObjectList) &&
           (RightList->Header.Type == SetupObjectList));

    NewSize = LeftList->Count + RightList->Count;
    NewArray = realloc(LeftList->Array, NewSize * sizeof(PVOID));
    if (NewArray == NULL) {
        return ENOMEM;
    }

    LeftList->Array = NewArray;
    for (Index = LeftList->Count; Index < NewSize; Index += 1) {
        NewArray[Index] = RightList->Array[Index - LeftList->Count];
        if (NewArray[Index] != NULL) {
            SetupObjectAddReference(NewArray[Index]);
        }
    }

    LeftList->Count = NewSize;
    return 0;
}

PSETUP_OBJECT
SetupCreateDict (
    PSETUP_OBJECT Source
    )

/*++

Routine Description:

    This routine creates a new empty dictionary object.

Arguments:

    Source - Supplies an optional pointer to a dictionary to copy.

Return Value:

    Returns a pointer to the new string on success.

    NULL on allocation failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSETUP_OBJECT Dict;
    PSETUP_DICT_ENTRY Entry;
    INT Status;

    Dict = malloc(sizeof(SETUP_OBJECT));
    if (Dict == NULL) {
        return NULL;
    }

    memset(Dict, 0, sizeof(SETUP_OBJECT));
    Dict->Header.Type = SetupObjectDict;
    Dict->Header.ReferenceCount = 1;
    INITIALIZE_LIST_HEAD(&(Dict->Dict.EntryList));
    if (Source != NULL) {

        assert(Source->Header.Type == SetupObjectDict);

        CurrentEntry = Source->Dict.EntryList.Next;
        while (CurrentEntry != &(Source->Dict.EntryList)) {
            Entry = LIST_VALUE(CurrentEntry, SETUP_DICT_ENTRY, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            Status = SetupDictSetElement(Dict, Entry->Key, Entry->Value, NULL);
            if (Status != 0) {
                SetupObjectReleaseReference(Dict);
                return NULL;
            }
        }
    }

    return Dict;
}

INT
SetupDictSetElement (
    PSETUP_OBJECT DictObject,
    PSETUP_OBJECT Key,
    PSETUP_OBJECT Value,
    PSETUP_OBJECT **LValue
    )

/*++

Routine Description:

    This routine adds or assigns a given value for a specific key.

Arguments:

    DictObject - Supplies a pointer to the dictionary.

    Key - Supplies a pointer to the key. This cannot be NULL. A reference will
        be added to the key if it is saved in the dictionary.

    Value - Supplies a pointer to the value. A reference will be added.

    LValue - Supplies an optional pointer where an LValue pointer will be
        returned on success. The caller can use the return of this pointer to
        assign into the dictionary element later.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSETUP_DICT_ENTRY Entry;

    assert(DictObject->Header.Type == SetupObjectDict);
    assert(Key != NULL);

    if ((Key->Header.Type != SetupObjectInteger) &&
        (Key->Header.Type != SetupObjectString)) {

        fprintf(stderr,
                "Cannot add type %s as dictionary key.\n",
                SetupObjectTypeNames[Key->Header.Type]);

        return EINVAL;
    }

    Entry = SetupDictLookup(DictObject, Key);

    //
    // If there is no entry, replace it.
    //

    if (Entry == NULL) {
        Entry = malloc(sizeof(SETUP_DICT_ENTRY));
        if (Entry == NULL) {
            return ENOMEM;
        }

        memset(Entry, 0, sizeof(SETUP_DICT_ENTRY));
        Entry->Key = Key;
        SetupObjectAddReference(Key);
        INSERT_BEFORE(&(Entry->ListEntry), &(DictObject->Dict.EntryList));
    }

    SetupObjectAddReference(Value);
    if (Entry->Value != NULL) {
        SetupObjectReleaseReference(Entry->Value);
    }

    Entry->Value = Value;
    if (LValue != NULL) {
        *LValue = &(Entry->Value);
    }

    return 0;
}

PSETUP_DICT_ENTRY
SetupDictLookup (
    PSETUP_OBJECT DictObject,
    PSETUP_OBJECT Key
    )

/*++

Routine Description:

    This routine attempts to find an entry in the given dictionary for a
    specific key.

Arguments:

    DictObject - Supplies a pointer to the dictionary to query.

    Key - Supplies a pointer to the key object to search.

Return Value:

    Returns a pointer to the dictionary entry on success.

    NULL if the key was not found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSETUP_DICT Dict;
    PSETUP_DICT_ENTRY Entry;

    assert(DictObject->Header.Type == SetupObjectDict);

    Dict = (PSETUP_DICT)DictObject;
    CurrentEntry = Dict->EntryList.Next;
    while (CurrentEntry != &(Dict->EntryList)) {
        Entry = LIST_VALUE(CurrentEntry, SETUP_DICT_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (SetupCompareObjects(Entry->Key, Key) == ComparisonResultSame) {
            return Entry;
        }
    }

    return NULL;
}

INT
SetupDictAdd (
    PSETUP_OBJECT Destination,
    PSETUP_OBJECT Addition
    )

/*++

Routine Description:

    This routine adds two dictionaries together, returning the result in the
    left one.

Arguments:

    Destination - Supplies a pointer to the dictionary to add to.

    Addition - Supplies the entries to add.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSETUP_DICT_ENTRY Entry;
    INT Status;

    assert(Destination->Header.Type == SetupObjectDict);
    assert(Addition->Header.Type == SetupObjectDict);

    CurrentEntry = Addition->Dict.EntryList.Next;
    while (CurrentEntry != &(Addition->Dict.EntryList)) {
        Entry = LIST_VALUE(CurrentEntry, SETUP_DICT_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Status = SetupDictSetElement(Destination,
                                     Entry->Key,
                                     Entry->Value,
                                     NULL);

        if (Status != 0) {
            return Status;
        }
    }

    return 0;
}

PSETUP_OBJECT
SetupCreateReference (
    PSETUP_OBJECT ReferenceTo
    )

/*++

Routine Description:

    This routine creates a reference object.

Arguments:

    ReferenceTo - Supplies a pointer to the object to refer to.

Return Value:

    Returns a pointer to the object on success.

    NULL on failure.

--*/

{

    PSETUP_OBJECT Reference;

    assert(ReferenceTo != NULL);

    Reference = malloc(sizeof(SETUP_OBJECT));
    if (Reference == NULL) {
        return NULL;
    }

    memset(Reference, 0, sizeof(SETUP_OBJECT));
    Reference->Header.Type = SetupObjectInteger;
    Reference->Header.ReferenceCount = 1;
    Reference->Reference.Value = ReferenceTo;
    SetupObjectAddReference(ReferenceTo);
    return Reference;
}

PSETUP_OBJECT
SetupObjectCopy (
    PSETUP_OBJECT Source
    )

/*++

Routine Description:

    This routine creates a deep copy of the given object.

Arguments:

    Source - Supplies the source to copy from.

Return Value:

    Returns a pointer to the new object on success.

    NULL on failure.

--*/

{

    PSETUP_OBJECT NewObject;
    PSETUP_OBJECT Object;

    Object = Source;
    switch (Object->Header.Type) {
    case SetupObjectInteger:
        NewObject = SetupCreateInteger(Object->Integer.Value);
        break;

    case SetupObjectString:
        NewObject = SetupCreateString(Object->String.String,
                                      Object->String.Size);

        break;

    case SetupObjectList:
        NewObject = SetupCreateList(Object->List.Array, Object->List.Count);
        break;

    case SetupObjectDict:
        NewObject = SetupCreateDict(Object);
        break;

    case SetupObjectReference:
        NewObject = SetupCreateReference(Object->Reference.Value);
        break;

    default:

        assert(FALSE);

        NewObject = NULL;
    }

    return NewObject;
}

BOOL
SetupObjectGetBooleanValue (
    PSETUP_OBJECT Object
    )

/*++

Routine Description:

    This routine converts an object to a boolean value.

Arguments:

    Object - Supplies a pointer to the object to booleanize.

Return Value:

    TRUE if the object is non-zero or non-empty.

    FALSE if the object is zero or empty.

--*/

{

    BOOL Result;

    if (Object->Header.Type == SetupObjectReference) {
        Object = Object->Reference.Value;
    }

    switch (Object->Header.Type) {
    case SetupObjectInteger:
        Result = (Object->Integer.Value != 0);
        break;

    case SetupObjectString:
        Result = (Object->String.Size != 0);
        break;

    case SetupObjectList:
        Result = (Object->List.Count != 0);
        break;

    case SetupObjectDict:
        Result = !LIST_EMPTY(&(Object->Dict.EntryList));
        break;

    default:

        assert(FALSE);

        Result = FALSE;
    }

    return Result;
}

VOID
SetupObjectAddReference (
    PSETUP_OBJECT Object
    )

/*++

Routine Description:

    This routine adds a reference to the given setup object.

Arguments:

    Object - Supplies a pointer to the object to add a reference to.

Return Value:

    None.

--*/

{

    PSETUP_OBJECT_HEADER Header;

    Header = &(Object->Header);

    assert(Header->Type != SetupObjectInvalid);
    assert((Header->ReferenceCount != 0) &&
           (Header->ReferenceCount < 0x10000000));

    Header->ReferenceCount += 1;
    return;
}

VOID
SetupObjectReleaseReference (
    PSETUP_OBJECT Object
    )

/*++

Routine Description:

    This routine releases a reference from the given setup object. If the
    reference count its zero, the object is destroyed.

Arguments:

    Object - Supplies a pointer to the object to release.

Return Value:

    None.

--*/

{

    PSETUP_OBJECT_HEADER Header;

    Header = &(Object->Header);

    assert(Header->Type != SetupObjectInvalid);
    assert((Header->ReferenceCount != 0) &&
           (Header->ReferenceCount < 0x10000000));

    Header->ReferenceCount -= 1;
    if (Header->ReferenceCount == 0) {
        SetupGutObject(Object);
        free(Header);
    }

    return;
}

VOID
SetupPrintObject (
    PSETUP_OBJECT Object,
    ULONG RecursionDepth
    )

/*++

Routine Description:

    This routine prints an object.

Arguments:

    Object - Supplies a pointer to the object to print.

    RecursionDepth - Supplies the recursion depth.

Return Value:

    None.

--*/

{

    PSETUP_OBJECT *Array;
    ULONG Count;
    PLIST_ENTRY CurrentEntry;
    PSETUP_DICT_ENTRY Entry;
    ULONG Index;
    ULONG ReferenceCount;
    ULONG Size;
    PSTR String;
    SETUP_OBJECT_TYPE Type;

    if (Object == NULL) {
        printf("0");
        return;
    }

    if (Object->Header.Type == SetupObjectReference) {
        Object = Object->Reference.Value;
    }

    Type = Object->Header.Type;

    //
    // Avoid infinite recursion.
    //

    if (Object->Header.ReferenceCount == (ULONG)-1) {
        if (Type == SetupObjectList) {
            printf("[...]");

        } else {

            assert(Type == SetupObjectDict);

            printf("{...}");
        }

        return;
    }

    //
    // Trash the reference count as a visit marker.
    //

    ReferenceCount = Object->Header.ReferenceCount;
    Object->Header.ReferenceCount = (ULONG)-1;
    switch (Type) {
    case SetupObjectInteger:
        printf("%I64d", Object->Integer.Value);
        break;

    case SetupObjectString:
        if (Object->String.Size == 0) {
            printf("\"\"");

        } else {
            String = Object->String.String;
            Size = Object->String.Size;
            printf("\"");
            while (Size != 0) {
                switch (*String) {
                case '\r':
                    printf("\\r");
                    break;

                case '\n':
                    printf("\\n");
                    break;

                case '\v':
                    printf("\\v");
                    break;

                case '\t':
                    printf("\\t");
                    break;

                case '\f':
                    printf("\\f");
                    break;

                case '\b':
                    printf("\\b");
                    break;

                case '\a':
                    printf("\\a");
                    break;

                case '\\':
                    printf("\\\\");
                    break;

                case '"':
                    printf("\\\"");
                    break;

                default:
                    if ((*String < ' ') || ((UCHAR)*String >= 0x80)) {
                        printf("\\x%02X", (UCHAR)*String);

                    } else {
                        printf("%c", *String);
                    }

                    break;
                }

                String += 1;
                Size -= 1;
            }

            printf("\"");
        }

        break;

    case SetupObjectList:
        printf("[");
        Array = Object->List.Array;
        Count = Object->List.Count;
        for (Index = 0; Index < Count; Index += 1) {
            SetupPrintObject(Array[Index], RecursionDepth + 1);
            if (Index < Count - 1) {
                printf(", ");
                if (Count >= 5) {
                    printf("\n%*s", RecursionDepth + 1, "");
                }
            }
        }

        printf("]");
        break;

    case SetupObjectDict:
        printf("{");
        CurrentEntry = Object->Dict.EntryList.Next;
        while (CurrentEntry != &(Object->Dict.EntryList)) {
            Entry = LIST_VALUE(CurrentEntry, SETUP_DICT_ENTRY, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            SetupPrintObject(Entry->Key, RecursionDepth + 1);
            printf(" : ");
            SetupPrintObject(Entry->Value, RecursionDepth + 1);
            if (CurrentEntry != &(Object->Dict.EntryList)) {
                printf("\n%*s", RecursionDepth + 1, "");
            }
        }

        printf("}");
        break;

    default:

        assert(FALSE);

        break;
    }

    Object->Header.ReferenceCount = ReferenceCount;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
SetupGutObject (
    PSETUP_OBJECT Object
    )

/*++

Routine Description:

    This routine destroys the inner contents of the object.

Arguments:

    Object - Supplies a pointer to the object to release.

Return Value:

    None.

--*/

{

    switch (Object->Header.Type) {
    case SetupObjectInteger:
        break;

    case SetupObjectString:
        if (Object->String.String != NULL) {
            free(Object->String.String);
        }

        break;

    case SetupObjectList:
        SetupDestroyList(Object);
        break;

    case SetupObjectDict:
        SetupDestroyDict(Object);
        break;

    case SetupObjectReference:
        SetupObjectReleaseReference(Object->Reference.Value);
        Object->Reference.Value = NULL;
        break;

    default:

        assert(FALSE);

        break;
    }

    Object->Header.Type = SetupObjectInvalid;
    return;
}

VOID
SetupDestroyList (
    PSETUP_OBJECT List
    )

/*++

Routine Description:

    This routine destroys a setup list object.

Arguments:

    List - Supplies a pointer to the list.

Return Value:

    None.

--*/

{

    PSETUP_OBJECT *Array;
    ULONG Index;

    Array = List->List.Array;
    for (Index = 0; Index < List->List.Count; Index += 1) {
        if (Array[Index] != NULL) {
            SetupObjectReleaseReference(Array[Index]);
        }
    }

    if (Array != NULL) {
        free(Array);
    }

    return;
}

VOID
SetupDestroyDict (
    PSETUP_OBJECT Dict
    )

/*++

Routine Description:

    This routine destroys a setup dictionary object.

Arguments:

    Dict - Supplies a pointer to the dictionary.

Return Value:

    None.

--*/

{

    PSETUP_DICT_ENTRY Entry;

    while (!LIST_EMPTY(&(Dict->Dict.EntryList))) {
        Entry = LIST_VALUE(Dict->Dict.EntryList.Next,
                           SETUP_DICT_ENTRY,
                           ListEntry);

        SetupDestroyDictEntry(Entry);
    }

    return;
}

VOID
SetupDestroyDictEntry (
    PSETUP_DICT_ENTRY Entry
    )

/*++

Routine Description:

    This routine removes and destroys a setup dictionary entry.

Arguments:

    Entry - Supplies a pointer to the dictionary entry.

Return Value:

    None.

--*/

{

    if (Entry->ListEntry.Next != NULL) {
        LIST_REMOVE(&(Entry->ListEntry));
        Entry->ListEntry.Next = NULL;
    }

    SetupObjectReleaseReference(Entry->Key);
    if (Entry->Value != NULL) {
        SetupObjectReleaseReference(Entry->Value);
    }

    return;
}

COMPARISON_RESULT
SetupCompareObjects (
    PSETUP_OBJECT Left,
    PSETUP_OBJECT Right
    )

/*++

Routine Description:

    This routine compares two objects to each other.

Arguments:

    Left - Supplies a pointer to the left object to compare.

    Right - Supplies a pointer to the right object to compare.

Return Value:

    Returns a comparison result. If the objects are not of the same type then
    their types will be compared. Otherwise, their values will be compared.

--*/

{

    ULONG LeftSize;
    PUCHAR LeftString;
    COMPARISON_RESULT Result;
    ULONG RightSize;
    PUCHAR RightString;

    if (Left->Header.Type == SetupObjectReference) {
        Left = Left->Reference.Value;
    }

    if (Right->Header.Type == SetupObjectReference) {
        Right = Right->Reference.Value;
    }

    if (Left->Header.Type < Right->Header.Type) {
        return ComparisonResultAscending;

    } else if (Left->Header.Type > Right->Header.Type) {
        return ComparisonResultDescending;
    }

    Result = ComparisonResultSame;
    switch (Left->Header.Type) {
    case SetupObjectInteger:
        if (Left->Integer.Value < Right->Integer.Value) {
            Result = ComparisonResultAscending;

        } else if (Left->Integer.Value > Right->Integer.Value) {
            Result = ComparisonResultDescending;
        }

        break;

    case SetupObjectString:

        //
        // The strings always have a null byte afterwards, so that byte can be
        // used in the comparison.
        //

        LeftString = (PUCHAR)(Left->String.String);
        LeftSize = Left->String.Size;
        RightString = (PUCHAR)(Right->String.String);
        RightSize = Right->String.Size;
        Result = ComparisonResultSame;
        while ((LeftSize != 0) && (RightSize != 0)) {
            if (*LeftString < *RightString) {
                Result = ComparisonResultAscending;
                break;

            } else if (*LeftString > *RightString) {
                Result = ComparisonResultDescending;
                break;
            }

            LeftString += 1;
            RightString += 1;
            LeftSize -= 1;
            RightSize -= 1;
        }

        if (Result == ComparisonResultSame) {
            if (RightSize != 0) {
                Result = ComparisonResultAscending;

            } else if (LeftSize != 0) {
                Result = ComparisonResultDescending;
            }
        }

        break;

    //
    // List comparison is possible but currently not needed.
    //

    case SetupObjectList:

        assert(FALSE);

        break;

    //
    // Dictionaries compare poorly.
    //

    case SetupObjectDict:

        assert(FALSE);

        break;

    default:

        assert(FALSE);

        break;
    }

    return Result;
}

