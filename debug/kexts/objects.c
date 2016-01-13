/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

#include <minoca/driver.h>
#include "dbgext.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MAX_OBJECT_NAME 100
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

    ULONG ArgumentIndex;
    ULONG BytesRead;
    ULONGLONG ObjectAddress;
    ULONGLONG RootObjectAddress;
    INT Status;

    //
    // At least one parameter is required.
    //

    if (ArgumentCount < 2) {

        //
        // Attempt to find the root object.
        //

        Status = DbgEvaluate(Context, ROOT_OBJECT_NAME, &RootObjectAddress);
        if (Status == 0) {

            //
            // TODO: 64-bit capable.
            //

            Status = DbgReadMemory(Context,
                                   TRUE,
                                   RootObjectAddress,
                                   sizeof(PVOID),
                                   &RootObjectAddress,
                                   &BytesRead);

            if ((Status != 0) || (BytesRead != sizeof(PVOID))) {
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

    ULONG BytesRead;
    ULONGLONG ChildAddress;
    ULONGLONG ChildListHeadAddress;
    LIST_ENTRY CurrentListEntry;
    ULONGLONG CurrentListEntryAddress;
    PSTR CurrentName;
    ULONG CurrentNameSize;
    OBJECT_HEADER CurrentObject;
    BOOL FirstWaiter;
    PSTR FullName;
    ULONG FullNameSize;
    ULONGLONG ListHeadAddress;
    PSTR NewFullName;
    ULONGLONG NextObjectAddress;
    OBJECT_HEADER Object;
    ULONGLONG RootObjectAddress;
    INT Status;
    ULONGLONG WaitBlockEntryAddress;

    CurrentName = NULL;
    FullName = NULL;
    FullNameSize = 0;
    RootObjectAddress = 0;
    ExtpPrintIndentation(IndentationLevel);

    //
    // Attempt to read the object header.
    //

    Status = DbgReadMemory(Context,
                           TRUE,
                           ObjectAddress,
                           sizeof(OBJECT_HEADER),
                           &Object,
                           &BytesRead);

    if ((Status != 0) || (BytesRead != sizeof(OBJECT_HEADER))) {
        DbgOut("Error: Could not read object.\n");
        if (Status == 0) {
            Status = EINVAL;
        }

        goto PrintObjectEnd;
    }

    if ((Object.Type == ObjectInvalid) || (Object.Type >= ObjectMaxTypes)) {
        DbgOut("%08I64x probably not an object, has type %x.\n",
               ObjectAddress,
               Object.Type);

        Status = EINVAL;
        goto PrintObjectEnd;
    }

    memcpy(&CurrentObject, &Object, sizeof(OBJECT_HEADER));

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

            //
            // TODO: 64-bit capable.
            //

            Status = DbgReadMemory(Context,
                                   TRUE,
                                   RootObjectAddress,
                                   sizeof(PVOID),
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

            if (CurrentObject.Name == NULL) {
                strncpy(CurrentName, "<noname>", MAX_OBJECT_NAME);

            } else {
                Status = DbgReadMemory(Context,
                                       TRUE,
                                       (ULONG)CurrentObject.Name,
                                       MAX_OBJECT_NAME,
                                       CurrentName,
                                       &BytesRead);

                if (Status != 0) {
                    DbgOut("Error: Unable to read object name at 0x%08x.\n",
                           CurrentObject.Name);

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
            // Create a new full path big enough to hold everything, and copy '
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

            if ((CurrentObject.Parent == NULL) ||
                ((ULONG)CurrentObject.Parent == RootObjectAddress)) {

                break;
            }

            Status = DbgReadMemory(Context,
                                   TRUE,
                                   (ULONG)CurrentObject.Parent,
                                   sizeof(OBJECT_HEADER),
                                   &CurrentObject,
                                   &BytesRead);

            if ((Status != 0) || (BytesRead != sizeof(OBJECT_HEADER))) {
                DbgOut("Error reading object at 0x%08x.\n",
                       CurrentObject.Parent);

                if (Status == 0) {
                    Status = EINVAL;
                }

                goto PrintObjectEnd;
            }
        }

    } else {

        //
        // Just read in this object's name.
        //

        if (Object.Name == NULL) {
            strncpy(CurrentName, "<noname>", MAX_OBJECT_NAME);

        } else {
            Status = DbgReadMemory(Context,
                                   TRUE,
                                   (ULONG)Object.Name,
                                   MAX_OBJECT_NAME,
                                   CurrentName,
                                   &BytesRead);

            if (Status != 0) {
                DbgOut("Error: Unable to read object name at 0x%08x.\n",
                       Object.Name);

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
    // Print out the one line version or the detailed version.
    //

    if (OneLiner != FALSE) {
        DbgOut("0x%08I64x ", ObjectAddress);
        Status = DbgPrintType(Context,
                              "OBJECT_TYPE",
                              &(Object.Type),
                              sizeof(OBJECT_TYPE));

        if (Status != 0) {
            DbgOut("BADOBJECTTYPE");
        }

        DbgOut(" %s\n", FullName);

    } else {
        DbgOut("%20s : 0x%08I64x\n", "Object", ObjectAddress);
        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : ", "Type");
        Status = DbgPrintType(Context,
                              "OBJECT_TYPE",
                              &(Object.Type),
                              sizeof(OBJECT_TYPE));

        if (Status != 0) {
            DbgOut("BADOBJECTTYPE");
        }

        DbgOut("\n");
        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : %s\n", "Name", FullName);
        ExtpPrintIndentation(IndentationLevel);
        if (Object.WaitQueue.Lock.LockHeld != 0) {
            DbgOut("%20s : 0x%08x.\n",
                   "Locked",
                   Object.WaitQueue.Lock.OwningThread);

            ExtpPrintIndentation(IndentationLevel);
        }

        //
        // Print various attributes of the object.
        //

        DbgOut("%20s : Parent 0x%08x Sibling ",
               "Relatives",
               Object.Parent);

        NextObjectAddress = (ULONG)Object.SiblingEntry.Next -
                            FIELD_OFFSET(OBJECT_HEADER, SiblingEntry);

        if (Object.SiblingEntry.Next == NULL) {
            DbgOut("NULL");
            NextObjectAddress = 0;

        } else if ((ULONG)Object.SiblingEntry.Next ==
                   ObjectAddress + FIELD_OFFSET(OBJECT_HEADER, SiblingEntry)) {

            DbgOut("NONE");
            NextObjectAddress = 0;

        } else {
            DbgOut("0x%08I64x", NextObjectAddress);
        }

        DbgOut(" Child ");
        if (Object.ChildListHead.Next == NULL) {
            DbgOut("NULL\n");

        } else if ((ULONG)Object.ChildListHead.Next ==
                   ObjectAddress + FIELD_OFFSET(OBJECT_HEADER, ChildListHead)) {

            DbgOut("NONE\n");

        } else {
            ChildAddress = (ULONG)Object.ChildListHead.Next -
                           FIELD_OFFSET(OBJECT_HEADER, ChildListHead);

            DbgOut("0x%08I64x\n", ChildAddress);
        }

        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : ", "State");
        Status = DbgPrintType(Context,
                              "SIGNAL_STATE",
                              (PVOID)&(Object.WaitQueue.State),
                              sizeof(SIGNAL_STATE));

        if (Status != 0) {
            DbgOut("BADSIGNALSTATE");
        }

        DbgOut("\n");
        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : %d\n", "Ref Count", Object.ReferenceCount);
        ExtpPrintIndentation(IndentationLevel);
        DbgOut("%20s : ", "Flags");
        DbgOut("\n");
        ExtpPrintIndentation(IndentationLevel);

        //
        // Print a list of all threads waiting on this object.
        //

        DbgOut("%20s : ", "Waiters");
        FirstWaiter = TRUE;
        ListHeadAddress = ObjectAddress +
                          FIELD_OFFSET(OBJECT_HEADER, WaitQueue.Waiters);

        CurrentListEntryAddress = (ULONG)Object.WaitQueue.Waiters.Next;
        while (CurrentListEntryAddress != ListHeadAddress) {
            if (FirstWaiter == FALSE) {
                DbgOut("                     : ");

            } else {
                FirstWaiter = FALSE;
            }

            //
            // TODO: This should subtract the offset of WaitListEntry in
            // WAIT_BLOCK_ENTRY, rather than a hardcoded pointer.
            //

            WaitBlockEntryAddress = CurrentListEntryAddress - sizeof(PVOID);
            DbgOut("0x%08I64x\n", WaitBlockEntryAddress);
            ExtpPrintIndentation(IndentationLevel);
            Status = DbgReadMemory(Context,
                                   TRUE,
                                   CurrentListEntryAddress,
                                   sizeof(LIST_ENTRY),
                                   &CurrentListEntry,
                                   &BytesRead);

            if ((Status != 0) || (BytesRead != sizeof(LIST_ENTRY))) {
                DbgOut("Error: Could not read list entry at 0x%08I64x.\n",
                       CurrentListEntryAddress);

                if (Status == 0) {
                    Status = EINVAL;
                }

                goto PrintObjectEnd;
            }

            CurrentListEntryAddress = (ULONG)CurrentListEntry.Next;
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

        ChildListHeadAddress = ObjectAddress +
                               FIELD_OFFSET(OBJECT_HEADER, ChildListHead);

        ObjectAddress = (ULONG)Object.ChildListHead.Next;
        while ((ObjectAddress != (INTN)NULL) &&
               (ObjectAddress != ChildListHeadAddress)) {

            ObjectAddress -= FIELD_OFFSET(OBJECT_HEADER, SiblingEntry);
            Status = DbgReadMemory(Context,
                                   TRUE,
                                   ObjectAddress,
                                   sizeof(OBJECT_HEADER),
                                   &Object,
                                   &BytesRead);

            if ((Status != 0) || (BytesRead != sizeof(OBJECT_HEADER))) {
                DbgOut("Error: Could not read object at 0x%08I64x.\n",
                       ObjectAddress);

                if (Status == 0) {
                    Status = EINVAL;
                }

                goto PrintObjectEnd;
            }

            Status = ExtpPrintObject(Context,
                                     IndentationLevel + 1,
                                     ObjectAddress,
                                     TRUE,
                                     FALSE,
                                     FullyRecurse,
                                     FullyRecurse);

            if (Status != 0) {
                DbgOut("Failed to print child at 0x%I64x.\n", ObjectAddress);
                goto PrintObjectEnd;
            }

            ObjectAddress = (ULONG)Object.SiblingEntry.Next;
        }
    }

    Status = 0;

PrintObjectEnd:
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

    ULONG IndentationIndex;

    for (IndentationIndex = 0;
         IndentationIndex < IndentationLevel;
         IndentationIndex += 1) {

        DbgOut("  ");
    }
}

