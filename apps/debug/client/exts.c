/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    exts.c

Abstract:

    This module contains support for loading and running debugger extensions.

Author:

    Evan Green 10-Sep-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/lib/im.h>
#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "symbols.h"
#include "dbgapi.h"
#include "dbgrprof.h"
#include "dbgrcomm.h"
#include "extsp.h"
#include "../dbgext/extimp.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DEBUGGER_EXTENSION {
    LIST_ENTRY ListEntry;
    PSTR BinaryName;
    ULONG Handle;
    LIST_ENTRY ExtensionsHead;
} DEBUGGER_EXTENSION, *PDEBUGGER_EXTENSION;

typedef struct _DEBUGGER_EXTENSION_ENTRY {
    LIST_ENTRY ListEntry;
    PSTR Command;
    PEXTENSION_PROTOTYPE Handler;
    PSTR OneLineDescription;
} DEBUGGER_EXTENSION_ENTRY, *PDEBUGGER_EXTENSION_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

PDEBUGGER_EXTENSION
DbgpFindExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR BinaryName
    );

PDEBUGGER_EXTENSION_ENTRY
DbgpFindExtensionEntry (
    PDEBUGGER_CONTEXT Context,
    PSTR ExtensionCommand
    );

//
// -------------------------------------------------------------------- Globals
//

DEBUG_EXTENSION_IMPORT_INTERFACE DbgExports = {
    DEBUG_EXTENSION_INTERFACE_VERSION,
    DbgRegisterExtension,
    DbgOutVaList,
    DbgEvaluate,
    DbgPrintAddressSymbol,
    DbgReadMemory,
    DbgWriteMemory,
    DbgReboot,
    DbgGetCallStack,
    DbgPrintCallStack,
    DbgGetTargetInformation,
    DbgGetTargetPointerSize,
    DbgGetMemberOffset,
    DbgGetTypeByName,
    DbgReadIntegerMember,
    DbgReadTypeByName,
    DbgReadType,
    DbgPrintTypeMember,
};

//
// ------------------------------------------------------------------ Functions
//

INT
DbgInitializeExtensions (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes support for debugger extensions and loads any
    extensions supplied on the command line.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INITIALIZE_LIST_HEAD(&(Context->LoadedExtensions));
    return 0;
}

INT
DbgLoadExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR BinaryName
    )

/*++

Routine Description:

    This routine loads a debugger extension library.

Arguments:

    Context - Supplies a pointer to the application context.

    BinaryName - Supplies the path to the binary to load.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDEBUGGER_EXTENSION ExistingExtension;
    PEXTENSION_ENTRY_INTERNAL ExtensionEntry;
    BOOL ExtensionLinked;
    ULONG Handle;
    PDEBUGGER_EXTENSION NewExtension;
    INT Result;

    ExtensionLinked = FALSE;
    NewExtension = NULL;

    //
    // Ensure a library of the same name is not already loaded.
    //

    ExistingExtension = DbgpFindExtension(Context, BinaryName);
    if (ExistingExtension != NULL) {
        Result = EEXIST;
        goto LoadExtensionEnd;
    }

    //
    // Attempt to load the library. If this fails, cleanup and exit.
    //

    Handle = DbgLoadLibrary(BinaryName);
    if (Handle == 0) {
        Result = EINVAL;
        goto LoadExtensionEnd;
    }

    //
    // Attempt to find the entrypoint. If this fails, cleanup and exit.
    //

    ExtensionEntry = DbgGetProcedureAddress(Handle, EXTENSION_ENTRY_NAME);
    if (ExtensionEntry == NULL) {
        Result = EINVAL;
        DbgOut("Error: Extension entry function %s could not be found.\n",
               EXTENSION_ENTRY_NAME);

        goto LoadExtensionEnd;
    }

    //
    // Allocate space to store the extension information and binary name.
    //

    NewExtension = MALLOC(sizeof(DEBUGGER_EXTENSION));
    if (NewExtension == NULL) {
        Result = ENOMEM;
        goto LoadExtensionEnd;
    }

    RtlZeroMemory(NewExtension, sizeof(DEBUGGER_EXTENSION));
    INITIALIZE_LIST_HEAD(&(NewExtension->ExtensionsHead));
    NewExtension->BinaryName = MALLOC(RtlStringLength(BinaryName) + 1);
    if (NewExtension->BinaryName == NULL) {
        Result = ENOMEM;
        goto LoadExtensionEnd;
    }

    RtlStringCopy(NewExtension->BinaryName,
                  BinaryName,
                  RtlStringLength(BinaryName) + 1);

    NewExtension->Handle = Handle;
    INSERT_BEFORE(&(NewExtension->ListEntry), &(Context->LoadedExtensions));
    ExtensionLinked = TRUE;

    //
    // Call the entry point and allow the extension to initialize.
    //

    Result = ExtensionEntry(EXTENSION_API_VERSION,
                            Context,
                            NewExtension,
                            &DbgExports);

    if (Result != 0) {
        goto LoadExtensionEnd;
    }

LoadExtensionEnd:
    if (Result != 0) {
        if (NewExtension != NULL) {
            if (ExtensionLinked != FALSE) {
                LIST_REMOVE(&(NewExtension->ListEntry));
            }

            if (NewExtension->BinaryName != NULL) {
                FREE(NewExtension->BinaryName);
            }

            FREE(NewExtension);
        }
    }

    return Result;
}

VOID
DbgUnloadExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR BinaryName
    )

/*++

Routine Description:

    This routine unloads and frees a debugger extension library.

Arguments:

    Context - Supplies a pointer to the application context.

    BinaryName - Supplies the path to the binary to unload.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDEBUGGER_EXTENSION Extension;
    PDEBUGGER_EXTENSION_ENTRY ExtensionEntry;

    //
    // Attempt to find the extension.
    //

    Extension = DbgpFindExtension(Context, BinaryName);
    if (Extension == NULL) {
        return;
    }

    //
    // Free all extension entries.
    //

    CurrentEntry = Extension->ExtensionsHead.Next;
    while (CurrentEntry != &(Extension->ExtensionsHead)) {
        ExtensionEntry = LIST_VALUE(CurrentEntry,
                                    DEBUGGER_EXTENSION_ENTRY,
                                    ListEntry);

        CurrentEntry = CurrentEntry->Next;
        FREE(ExtensionEntry);
    }

    //
    // Unlink the extension, unload the library, and free the memory.
    //

    LIST_REMOVE(&(Extension->ListEntry));
    DbgFreeLibrary(Extension->Handle);
    FREE(Extension->BinaryName);
    FREE(Extension);
    return;
}

VOID
DbgUnloadAllExtensions (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine unloads all debugger extensions.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    PDEBUGGER_EXTENSION Extension;

    while (LIST_EMPTY(&(Context->LoadedExtensions)) == FALSE) {
        Extension = LIST_VALUE(Context->LoadedExtensions.Next,
                               DEBUGGER_EXTENSION,
                               ListEntry);

        DbgUnloadExtension(Context, Extension->BinaryName);
    }

    return;
}

INT
DbgDispatchExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine dispatches a debugger extension command.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR Command;
    PSTR CommandCopy;
    PLIST_ENTRY CurrentEntry;
    PLIST_ENTRY CurrentExtension;
    PDEBUGGER_EXTENSION_ENTRY CurrentExtensionEntry;
    PDEBUGGER_EXTENSION Extension;
    INT Status;
    PSTR SubCommand;

    Command = Arguments[0] + 1;

    //
    // If the command is just a !, print out all extensions with a description.
    //

    if (*Command == '\0') {

        //
        // Loop through all registered extension binaries.
        //

        CurrentExtension = Context->LoadedExtensions.Next;
        while (CurrentExtension != &(Context->LoadedExtensions)) {
            Extension = LIST_VALUE(CurrentExtension,
                                   DEBUGGER_EXTENSION,
                                   ListEntry);

            CurrentExtension = CurrentExtension->Next;
            DbgOut("%s:\n", Extension->BinaryName);

            //
            // Loop through all extensions of the current binary.
            //

            CurrentEntry = Extension->ExtensionsHead.Next;
            while (CurrentEntry != &(Extension->ExtensionsHead)) {
                CurrentExtensionEntry = LIST_VALUE(CurrentEntry,
                                                   DEBUGGER_EXTENSION_ENTRY,
                                                   ListEntry);

                CurrentEntry = CurrentEntry->Next;
                DbgOut("  !%s - %s\n",
                       CurrentExtensionEntry->Command,
                       CurrentExtensionEntry->OneLineDescription);
            }
        }

        Status = 0;

    //
    // Find the extension and dispatch it.
    //

    } else {

        //
        // Find the first period, which splits the extension to its subcommand.
        //

        CommandCopy = strdup(Command);
        if (CommandCopy == NULL) {
            return ENOMEM;
        }

        SubCommand = strchr(CommandCopy, '.');
        if (SubCommand != NULL) {
            *SubCommand = '\0';
            SubCommand += 1;
        }

        //
        // Find the extension and dispatch it.
        //

        CurrentExtensionEntry = DbgpFindExtensionEntry(Context, CommandCopy);
        if (CurrentExtensionEntry != NULL) {
            Status = CurrentExtensionEntry->Handler(Context,
                                                    SubCommand,
                                                    ArgumentCount,
                                                    Arguments);

        } else {
            DbgOut("Error: Extension !%s not found.\n", Command);
            Status = ENOENT;
        }

        free(CommandCopy);
    }

    return Status;
}

INT
DbgRegisterExtension (
    PDEBUGGER_CONTEXT Context,
    PVOID Token,
    PSTR ExtensionName,
    PSTR OneLineDescription,
    PEXTENSION_PROTOTYPE Routine
    )

/*++

Routine Description:

    This routine registers a debugger extension with the client.

Arguments:

    Context - Supplies a pointer to the application context.

    Token - Supplies the unique token provided to the extension library upon
        initialization.

    ExtensionName - Supplies the name of the extension to register. This name
        must not already be registered by the current extension or any other.

    OneLineDescription - Supplies a quick description of the extension, no
        longer than 60 characters. This parameter is not optional.

    Routine - Supplies the routine to call when the given extension is
        invoked.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDEBUGGER_EXTENSION_ENTRY ExistingEntry;
    PDEBUGGER_EXTENSION Extension;
    PDEBUGGER_EXTENSION_ENTRY NewEntry;
    INT Result;

    NewEntry = NULL;

    //
    // The token is actually just a pointer to the extension structure. Though
    // this is susceptible to tampering, this DLL is loaded in our address
    // space and has already been allowed to run arbitrary code. If it wanted
    // to take the process down, it could have already.
    //

    if (Token == NULL) {
        return EINVAL;
    }

    Extension = (PDEBUGGER_EXTENSION)Token;

    //
    // Descriptions are *not* optional.
    //

    if (OneLineDescription == NULL) {
        return EINVAL;
    }

    //
    // Refuse to register extensions that are already registered.
    //

    ExistingEntry = DbgpFindExtensionEntry(Context, ExtensionName);
    if (ExistingEntry != NULL) {
        Result = EEXIST;
        goto RegisterExtensionEnd;
    }

    NewEntry = MALLOC(sizeof(DEBUGGER_EXTENSION_ENTRY));
    if (NewEntry == NULL) {
        Result = ENOMEM;
        goto RegisterExtensionEnd;
    }

    RtlZeroMemory(NewEntry, sizeof(DEBUGGER_EXTENSION_ENTRY));
    NewEntry->Command = ExtensionName;
    NewEntry->Handler = Routine;
    NewEntry->OneLineDescription = OneLineDescription;
    INSERT_BEFORE(&(NewEntry->ListEntry), &(Extension->ExtensionsHead));
    Result = 0;

RegisterExtensionEnd:
    if (Result != 0) {
        if (NewEntry != NULL) {
            FREE(NewEntry);
        }
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

PDEBUGGER_EXTENSION
DbgpFindExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR BinaryName
    )

/*++

Routine Description:

    This routine finds a loaded debugger extension matching the given binary
    name.

Arguments:

    Context - Supplies a pointer to the application context.

    BinaryName - Supplies the extension binary name.

Return Value:

    Returns a pointer to the loaded extension, or NULL if the extension could
    not be found.

--*/

{

    PLIST_ENTRY Entry;
    PDEBUGGER_EXTENSION Extension;

    Entry = Context->LoadedExtensions.Next;
    while (Entry != &(Context->LoadedExtensions)) {
        Extension = LIST_VALUE(Entry, DEBUGGER_EXTENSION, ListEntry);
        if (RtlAreStringsEqual(BinaryName, Extension->BinaryName, 1024) !=
            FALSE) {

            return Extension;
        }
    }

    return NULL;
}

PDEBUGGER_EXTENSION_ENTRY
DbgpFindExtensionEntry (
    PDEBUGGER_CONTEXT Context,
    PSTR ExtensionCommand
    )

/*++

Routine Description:

    This routine finds the extension entry corresponding to the given extension
    command.

Arguments:

    Context - Supplies a pointer to the application context.

    ExtensionCommand - Supplies the extension command, not including the
        leading !.

Return Value:

    Returns a pointer to the extension entry, or NULL if none was registered.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLIST_ENTRY CurrentExtension;
    PDEBUGGER_EXTENSION_ENTRY CurrentExtensionEntry;
    PDEBUGGER_EXTENSION Extension;
    BOOL StringsEqual;

    //
    // Loop through all registered extension binaries.
    //

    CurrentExtension = Context->LoadedExtensions.Next;
    while (CurrentExtension != &(Context->LoadedExtensions)) {
        Extension = LIST_VALUE(CurrentExtension, DEBUGGER_EXTENSION, ListEntry);
        CurrentExtension = CurrentExtension->Next;

        //
        // Loop through all extensions of the current binary.
        //

        CurrentEntry = Extension->ExtensionsHead.Next;
        while (CurrentEntry != &(Extension->ExtensionsHead)) {
            CurrentExtensionEntry = LIST_VALUE(CurrentEntry,
                                               DEBUGGER_EXTENSION_ENTRY,
                                               ListEntry);

            CurrentEntry = CurrentEntry->Next;
            StringsEqual = RtlAreStringsEqual(CurrentExtensionEntry->Command,
                                              ExtensionCommand,
                                              MAX_EXTENSION_COMMAND);

            if (StringsEqual != FALSE) {
                return CurrentExtensionEntry;
            }
        }
    }

    return NULL;
}

