/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    objects.c

Abstract:

    This module implements Object Manager related debugger extensions.

Author:

    Evan Green 11-Sep-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/debug/dbgext.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MAX_OBJECT_NAME 512
#define ROOT_OBJECT_NAME "kernel!ObRootObject"

#define MALLOC(_x) malloc(_x)
#define FREE(_x) free(_x)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ExtpHandleObjectCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR Command,
    ULONGLONG Address
    );

INT
ExtpPrintObject (
    PDEBUGGER_CONTEXT Context,
    ULONG IndentationLevel,
    ULONGLONG ObjectAddress,
    BOOL OneLiner,
    BOOL FullPath,
    BOOL PrintChildren,
    BOOL FullyRecurse
    );

VOID
ExtpPrintIndentation (
    ULONG IndentationLevel
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ExtObject (
    PDEBUGGER_CONTEXT Context,
    PSTR Command,
    ULONG ArgumentCount,
    PSTR *ArgumentValues
    )

/*++

Routine Description:

    This routine prints out the contents of an Object:

        Address - Supplies the address of the Object.

Arguments:

    Context - Supplies a pointer to the debugger applicaton context, which is
        an argument to most of the API functions.

    Command - Supplies the subcommand entered. This parameter is unused.

    ArgumentCount - Supplies the number of arguments in the ArgumentValues
        array.

    ArgumentValues - Supplies the values of each argument. This memory will be
        reused when the function returns, so extensions must not touch this
        memory after returning from this call.

Return Value:

    0 if the debugger extension command was successful.

    Returns an error code if a failure occurred along the way.

--*/

{

    ULONG AddressSize;
    ULONG ArgumentIndex;
    ULONG BytesRead;
    ULONGLONG ObjectAddress;
    ULONGLONG RootObjectAddress;
    INT Status;

    AddressSize = DbgGetTargetPointerSize(Context);

    //
    // At least one parameter is required.
    //

    if (ArgumentCount < 2) {

        //
        // Attempt to find the root object.
        //

        Status = DbgEvaluate(Context, ROOT_OBJECT_NAME, &RootObjectAddress);
        if (Status == 0) {
             Status = DbgReadMemory(Context,
                                   TRUE,
                                   RootObjectAddress,
                                   AddressSize,
                                   &RootObjectAddress,
                                   &BytesRead);

            if ((Status != 0) || (BytesRead != AddressSize)) {
                DbgOut("Unable to find ObRootObject.\n");
                if (Status == 0) {
                    Status = EINVAL;
                }

                return Status;
            }

            ExtpHandleObjectCommand(Context, Command, RootObjectAddress);

        } else {
            DbgOut("Error: Unable to evaluate %s.\n", ROOT_OBJECT_NAME);
            return Status;
        }
    }

    //
    // Loop through each argument, evaluate the address, and print the
    // namespace tree at that object.
    //

    for (ArgumentIndex = 1;
         ArgumentIndex < ArgumentCount;
         ArgumentIndex += 1) {

        Status = DbgEvaluate(Context,
                             ArgumentValues[ArgumentIndex],
                             &ObjectAddress);

        if (Status != 0) {
            DbgOut("Failed to evaluate address at \"%s\".\n",
                   ArgumentValues[ArgumentIndex]);
        }

        ExtpHandleObjectCommand(Context, Command, ObjectAddress);
        if (ArgumentIndex != ArgumentCount - 1) {
            DbgOut("\n----\n");
        }
    }

    DbgOut("\n");
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ExtpHandleObjectCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR Command,
    ULONGLONG Address
    )

/*++

Routine Description:

    This routine handles an object command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to handle.

    Address - Supplies the address of the object to print.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Success;

    if (Command == NULL) {
        Success = ExtpPrintObject(Context,
                                  1,
                                  Address,
                                  FALSE,
                                  TRUE,
                                  FALSE,
                                  FALSE);

    } else if (strcmp(Command, "list") == 0) {
        Success = ExtpPrintObject(Context,
                                  0,
                                  Address,
                                  TRUE,
                                  FALSE,
                                  TRUE,
                                  FALSE);

    } else if (strcmp(Command, "tree") == 0) {
        Success = ExtpPrintObject(Context,
                                  0,
                                  Address,
                                  TRUE,
                                  FALSE,
                                  TRUE,
                                  TRUE);

    } else if (strcmp(Command, "help") == 0) {
        DbgOut("Valid subcommands are:\n  "
               "!object - print an object.\n  "
               "!object.list - print an object and its children.\n  "
               "!object.tree - print the entire tree underneath "
               "the given object.\n");

        Success = 0;

    } else {
        DbgOut("Error: Invalid subcommand. Run !object.help for "
               "detailed usage.\n");

        Success = 0;
    }

    return Success;
}

INT
ExtpPrintObject (
    PDEBUGGER_CONTEXT Context,
    ULONG IndentationLevel,
    ULONGLONG ObjectAddress,
    BOOL OneLiner,
    BOOL FullPath,
    BOOL PrintChildren,
    BOOL FullyRecurse
    )

/*++

Routine Description:

    This routine prints out an object.

Arguments:

    Context - Supplies a pointer to the application context.

    IndentationLevel - Supplies the current indentation level that the object
        should be printed at.

    ObjectAddress - Supplies the virtual address (in the target) of the
        object to print.

    OneLiner - Supplies a boolean indicating that only one line of text should
        be printed.

    FullPath - Supplies a boolean indicating whether the full object path
        should be printed or not.

    PrintChildren - Supplies a boolean indicating whether or not the routine
        should recurse into printing the object's direct children.

    FullyRecurse - Supplies a boolean indicating whether or not the routine
        should fully recurse to all descendents of the object.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG AddressSize;
    ULONG BytesRead;
    ULONGLONG ChildAddress;
    ULONGLONG ChildListHeadAddress;
    ULONG ChildListOffset;
    ULONGLONG CurrentListEntryAddress;
    PSTR CurrentName;
    ULONG CurrentNameSize;
    ULONGLONG CurrentObjectName;
    ULONGLONG CurrentObjectParent;
    PVOID Data;
    ULONG DataSize;
    ULONGLONG FirstChild;
    BOOL FirstWaiter;
    PSTR FullName;
    ULONG FullNameSize;
    PVOID ListEntryData;
    ULONG ListEntryDataSize;
    PTYPE_SYMBOL ListEntryType;
    ULONGLONG ListHeadAddress;
    ULONGLONG LockHeld;
    PSTR NewFullName;
    ULONGLONG NextObjectAddress;
    ULONGLONG NextSibling;
    PVOID ObjectData;
    ULONG ObjectDataSize;
    ULONGLONG ObjectParent;
    PTYPE_SYMBOL ObjectType;
    ULONGLONG ObjectTypeValue;
    ULONGLONG OwningThread;
    ULONGLONG RootObjectAddress;
    ULONG SiblingEntryOffset;
    INT Status;
    ULONGLONG WaitBlockEntryAddress;
    ULONG WaitBlockEntryListEntryOffset;
    PTYPE_SYMBOL WaitBlockEntryType;
    ULONG WaitersOffset;
    ULONG WaitQueueOffset;
    PTYPE_SYMBOL WaitQueueType;

    AddressSize = DbgGetTargetPointerSize(Context);
    CurrentName = NULL;
    Data = NULL;
    FullName = NULL;
    FullNameSize = 0;
    ListEntryData = NULL;
    ObjectData = NULL;
    RootObjectAddress = 0;
    ExtpPrintIndentation(IndentationLevel);

    //
    // Attempt to read the object header.
    //

    Status = DbgReadTypeByName(Context,
                               ObjectAddress,
                               "OBJECT_HEADER",
                               &ObjectType,
                               &ObjectData,
                               &ObjectDataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read object.\n");
        goto PrintObjectEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  ObjectType,
                                  "Type",
                                  ObjectAddress,
                                  ObjectData,
                                  ObjectDataSize,
                                  &ObjectTypeValue);

    if (Status != 0) {
        goto PrintObjectEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  ObjectType,
                                  "Name",
                                  ObjectAddress,
                                  ObjectData,
                                  ObjectDataSize,
                                  &CurrentObjectName);

    if (Status != 0) {
        goto PrintObjectEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  ObjectType,
                                  "Parent",
                                  ObjectAddress,
                                  ObjectData,
                                  ObjectDataSize,
                                  &ObjectParent);

    if (Status != 0) {
        goto PrintObjectEnd;
    }

    CurrentObjectParent = ObjectParent;
    if ((ObjectTypeValue == ObjectInvalid) ||
        (ObjectTypeValue >= ObjectMaxTypes)) {

        DbgOut("%08I64x probably not an object, has type %I64x.\n",
               ObjectAddress,
               ObjectTypeValue);

        Status = EINVAL;
        goto PrintObjectEnd;
    }

    //
    // If the full name should be printed, collect that now.
    //

    CurrentName = MALLOC(MAX_OBJECT_NAME);
    if (CurrentName == NULL) {
        DbgOut("Error: Memory allocation failure.\n");
        Status = ENOMEM;
        goto PrintObjectEnd;
    }

    if (FullPath != FALSE) {

        //
        // Attempt to find the root object.
        //

        Status = DbgEvaluate(Context, ROOT_OBJECT_NAME, &RootObjectAddress);
        if (Status == 0) {
            Status = DbgReadMemory(Context,
                                   TRUE,
                                   RootObjectAddress,
                                   AddressSize,
                                   &RootObjectAddress,
                                   &BytesRead);

            if ((Status != 0) || (BytesRead != sizeof(PVOID))) {
                DbgOut("Unable to find ObRootObject.\n");
                RootObjectAddress = 0;
                if (Status == 0) {
                    Status = EINVAL;
                }

                goto PrintObjectEnd;
            }

        } else {
            RootObjectAddress = 0;
        }

        //
        // Iterate up through the tree towards the root, prepending the object
        // name at each step.
        //

        while (TRUE) {

            //
            // Read in the current object's name string, or at least as much of
            // it as this extension cares to read.
            //

            if (CurrentObjectName == 0) {
                strncpy(CurrentName, "<noname>", MAX_OBJECT_NAME);

            } else {
                Status = DbgReadMemory(Context,
                                       TRUE,
                                       CurrentObjectName,
                                       MAX_OBJECT_NAME,
                                       CurrentName,
                                       &BytesRead);

                if (Status != 0) {
                    DbgOut("Error: Unable to read object name at 0x%08I64x.\n",
                           CurrentObjectName);

                    goto PrintObjectEnd;
                }

                //
                // Terminate the string.
                //

                if (BytesRead == MAX_OBJECT_NAME) {
                    CurrentName[MAX_OBJECT_NAME - 1] = '\0';

                } else {
                    CurrentName[BytesRead] = '\0';
                }
            }

            //
            // Create a new full path big enough to hold everything, and copy
            // it in.
            //

            CurrentNameSize = strlen(CurrentName);
            NewFullName = MALLOC(CurrentNameSize + FullNameSize + 2);
            if (NewFullName == NULL) {
                DbgOut("Error: Memory allocation failure for %d bytes.\n",
                       CurrentNameSize + FullNameSize + 2);

                Status = ENOMEM;
                goto PrintObjectEnd;
            }

            strcpy(NewFullName, "/");
            strcat(NewFullName, CurrentName);
            if (FullName != NULL) {
                strcat(NewFullName, FullName);
                FREE(FullName);
            }

            FullName = NewFullName;
            FullNameSize += CurrentNameSize + 1;

            //
            // Find the parent, read it in, and loop.
            //

            if ((CurrentObjectParent == 0) ||
                (CurrentObjectParent == RootObjectAddress)) {

                break;
            }

            assert(Data == NULL);

            Status = DbgReadType(Context,
                                 CurrentObjectParent,
                                 ObjectType,
                                 &Data,
                                 &DataSize);

            if (Status != 0) {
                DbgOut("Error reading object at 0x%08I64x.\n",
                       CurrentObjectParent);

                goto PrintObjectEnd;
            }

            Status = DbgReadIntegerMember(Context,
                                          ObjectType,
                                          "Name",
                                          ObjectAddress,
                                          Data,
                                          DataSize,
                                          &CurrentObjectName);

            if (Status != 0) {
                goto PrintObjectEnd;
            }

            Status = DbgReadIntegerMember(Context,
                                          ObjectType,
                                          "Parent",
                                          ObjectAddress,
                                          Data,
                                          DataSize,
                                          &CurrentObjectParent);

            if (Status != 0) {
                goto PrintObjectEnd;
            }

            free(Data);
            Data = NULL;
        }

    } else {

        //
        // Just read in this object's name.
        //

        if (CurrentObjectName == 0) {
            CurrentName[0] = '\0';

        } else {
            Status = DbgReadMemory(Context,
                                   TRUE,
                                   CurrentObjectName,
                                   MAX_OBJECT_NAME,
                                   CurrentName,
                                   &BytesRead);

            if (Status != 0) {
                DbgOut("Error: Unable to read object name at 0x%08I64x.\n",
                       CurrentObjectName);

                goto PrintObjectEnd;
            }

            //
            // Terminate the string.
            //

            if (BytesRead == MAX_OBJECT_NAME) {
                CurrentName[MAX_OBJECT_NAME - 1] = '\0';

            } else {
                CurrentName[BytesRead] = '\0';
            }
        }

        FullName = CurrentName;
        CurrentName = NULL;
    }

    //
    // Get some attributes.
    //

    Status = DbgReadIntegerMember(Context,
                                  ObjectType,
                                  "SiblingEntry.Next",
                                  ObjectAddress,
                                  ObjectData,
                                  ObjectDataSize,
                                  &NextSibling);

    if (Status != 0) {
        goto PrintObjectEnd;
    }

    Status = DbgGetMemberOffset(ObjectType,
                                "SiblingEntry",
                                &SiblingEntryOffset,
                                NULL);

    if (Status != 0) {
        goto PrintObjectEnd;
    }

    Status = DbgGetMemberOffset(ObjectType,
                                "ChildListHead",
                                &ChildListOffset,
                                NULL);

    if (Status != 0) {
        goto PrintObjectEnd;
    }

    Status = DbgGetMemberOffset(ObjectType,
                                "WaitQueue",
                                &WaitQueueOffset,
                                NULL);

    if (Status != 0) {
        goto PrintObjectEnd;
    }

    SiblingEntryOffset /= BITS_PER_BYTE;
    ChildListOffset /= BITS_PER_BYTE;
    WaitQueueOffset /= BITS_PER_BYTE;
    Status = DbgReadIntegerMember(Context,
                                  ObjectType,
                                  "ChildListHead.Next",
                                  ObjectAddress,
                                  ObjectData,
                                  ObjectDataSize,
                                  &FirstChild);

    if (Status != 0) {
        goto PrintObjectEnd;
    }

    Status = DbgGetTypeByName(Context, "LIST_ENTRY", &ListEntryType);
    if (Status != 0) {
        goto PrintObjectEnd;
    }

    //
    // Print out the one line version or the detailed version.
    //

    if (OneLiner != FALSE) {
        DbgOut("0x%08I64x ", ObjectAddress);
        DbgPrintTypeMember(Context,
                           ObjectAddress,
                           ObjectData,
                           ObjectDataSize,
                           ObjectType,
                           "Type",
                           0,
                           0);

        DbgOut(" %s\n", FullName);

    } else {
        DbgOut("%20s : 0x%08I64x\n", "Object", ObjectAddress);
        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : ", "Type");
        DbgPrintTypeMember(Context,
                           ObjectAddress,
                           ObjectData,
                           ObjectDataSize,
                           ObjectType,
                           "Type",
                           0,
                           0);

        DbgOut("\n");
        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : %s\n", "Name", FullName);
        ExtpPrintIndentation(IndentationLevel);
        Status = DbgReadIntegerMember(Context,
                                      ObjectType,
                                      "WaitQueue.Lock.LockHeld",
                                      ObjectAddress,
                                      ObjectData,
                                      ObjectDataSize,
                                      &LockHeld);

        if ((Status == 0) && (LockHeld != FALSE)) {
            Status = DbgReadIntegerMember(Context,
                                          ObjectType,
                                          "WaitQueue.Lock.OwningThread",
                                          ObjectAddress,
                                          ObjectData,
                                          ObjectDataSize,
                                          &OwningThread);

            if (Status == 0) {
                DbgOut("%20s : 0x%08x.\n", "Locked", OwningThread);
                ExtpPrintIndentation(IndentationLevel);
            }
        }

        //
        // Print various attributes of the object.
        //

        DbgOut("%20s : Parent 0x%08x Sibling ", "Relatives", ObjectParent);
        NextObjectAddress = NextSibling - SiblingEntryOffset;
        if (NextSibling == 0) {
            DbgOut("NULL");
            NextObjectAddress = 0;

        } else if (NextSibling == ObjectAddress + SiblingEntryOffset) {
            DbgOut("NONE");
            NextObjectAddress = 0;

        } else {
            DbgOut("0x%08I64x", NextObjectAddress);
        }

        DbgOut(" Child ");
        if (FirstChild == 0) {
            DbgOut("NULL\n");

        } else if (FirstChild == ObjectAddress + ChildListOffset) {
            DbgOut("NONE\n");

        } else {
            ChildAddress = FirstChild - ChildListOffset;
            DbgOut("0x%08I64x\n", ChildAddress);
        }

        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : ", "State");
        DbgPrintTypeMember(Context,
                           ObjectAddress,
                           ObjectData,
                           ObjectDataSize,
                           ObjectType,
                           "WaitQueue.State",
                           0,
                           0);

        DbgOut("\n");
        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : ", "Ref Count");
        DbgPrintTypeMember(Context,
                           ObjectAddress,
                           ObjectData,
                           ObjectDataSize,
                           ObjectType,
                           "ReferenceCount",
                           0,
                           0);

        DbgOut("\n");
        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : ", "Flags");
        DbgPrintTypeMember(Context,
                           ObjectAddress,
                           ObjectData,
                           ObjectDataSize,
                           ObjectType,
                           "Flags",
                           0,
                           0);

        DbgOut("\n");
        ExtpPrintIndentation(IndentationLevel);

        //
        // Print a list of all threads waiting on this object.
        //

        DbgOut("%20s : ", "Waiters");
        Status = DbgGetTypeByName(Context, "WAIT_QUEUE", &WaitQueueType);
        if (Status != 0) {
            goto PrintObjectEnd;
        }

        Status = DbgGetMemberOffset(WaitQueueType,
                                    "Waiters",
                                    &WaitersOffset,
                                    NULL);

        if (Status != 0) {
            goto PrintObjectEnd;
        }

        WaitersOffset /= BITS_PER_BYTE;
        Status = DbgGetTypeByName(Context,
                                  "WAIT_BLOCK_ENTRY",
                                  &WaitBlockEntryType);

        if (Status != 0) {
            goto PrintObjectEnd;
        }

        Status = DbgGetMemberOffset(WaitBlockEntryType,
                                    "WaitListEntry",
                                    &WaitBlockEntryListEntryOffset,
                                    NULL);

        if (Status != 0) {
            goto PrintObjectEnd;
        }

        WaitBlockEntryListEntryOffset /= BITS_PER_BYTE;
        FirstWaiter = TRUE;
        ListHeadAddress = ObjectAddress + WaitQueueOffset + WaitersOffset;
        Status = DbgReadIntegerMember(Context,
                                      ObjectType,
                                      "WaitQueue.Waiters.Next",
                                      ObjectAddress,
                                      ObjectData,
                                      ObjectDataSize,
                                      &CurrentListEntryAddress);

        if (Status != 0) {
            goto PrintObjectEnd;
        }

        while (CurrentListEntryAddress != ListHeadAddress) {
            if (FirstWaiter == FALSE) {
                DbgOut("                     : ");

            } else {
                FirstWaiter = FALSE;
            }

            WaitBlockEntryAddress = CurrentListEntryAddress -
                                    WaitBlockEntryListEntryOffset;

            DbgOut("0x%08I64x\n", WaitBlockEntryAddress);
            ExtpPrintIndentation(IndentationLevel);

            assert(ListEntryData == NULL);

            Status = DbgReadType(Context,
                                 CurrentListEntryAddress,
                                 ListEntryType,
                                 &ListEntryData,
                                 &ListEntryDataSize);

            if (Status != 0) {
                goto PrintObjectEnd;
            }

            Status = DbgReadIntegerMember(Context,
                                          ListEntryType,
                                          "Next",
                                          0,
                                          ListEntryData,
                                          ListEntryDataSize,
                                          &CurrentListEntryAddress);

            if (Status != 0) {
                goto PrintObjectEnd;
            }

            free(ListEntryData);
            ListEntryData = NULL;
        }

        DbgOut("\n");
    }

    //
    // If children should be printed, go through their list.
    //

    if (PrintChildren != FALSE) {

        //
        // Get the first child and enumerate until no more siblings of that
        // child are found.
        //

        ChildListHeadAddress = ObjectAddress + ChildListOffset;
        ObjectAddress = FirstChild;
        while ((ObjectAddress != (INTN)NULL) &&
               (ObjectAddress != ChildListHeadAddress)) {

            Status = ExtpPrintObject(Context,
                                     IndentationLevel + 1,
                                     ObjectAddress - SiblingEntryOffset,
                                     TRUE,
                                     FALSE,
                                     FullyRecurse,
                                     FullyRecurse);

            if (Status != 0) {
                DbgOut("Failed to print child at 0x%I64x.\n", ObjectAddress);
                goto PrintObjectEnd;
            }

            assert(ListEntryData == NULL);

            Status = DbgReadType(Context,
                                 ObjectAddress,
                                 ListEntryType,
                                 &ListEntryData,
                                 &ListEntryDataSize);

            if (Status != 0) {
                DbgOut("Error: Could not read LIST_ENTRY at 0x%I64x.\n",
                       ObjectAddress);

                goto PrintObjectEnd;
            }

            Status = DbgReadIntegerMember(Context,
                                          ListEntryType,
                                          "Next",
                                          0,
                                          ListEntryData,
                                          ListEntryDataSize,
                                          &ObjectAddress);

            if (Status != 0) {
                goto PrintObjectEnd;
            }

            free(ListEntryData);
            ListEntryData = NULL;
        }
    }

    Status = 0;

PrintObjectEnd:
    if (ObjectData != NULL) {
        free(ObjectData);
    }

    if (Data != NULL) {
        free(Data);
    }

    if (ListEntryData != NULL) {
        free(ListEntryData);
    }

    if (CurrentName != NULL) {
        FREE(CurrentName);
    }

    if (FullName != NULL) {
        FREE(FullName);
    }

    return Status;
}

VOID
ExtpPrintIndentation (
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out some indentation spaces.

Arguments:

    IndentationLevel - Supplies the current indentation level.

Return Value:

    None.

--*/

{

    DbgOut("%*s", IndentationLevel, "");
    return;
}

