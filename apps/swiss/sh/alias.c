/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    alias.c

Abstract:

    This module implements support for alias substitution in the shell.

Author:

    Evan Green 12-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sh.h"
#include "shparse.h"
#include "../swlib.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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
ShDestroyAlias (
    PSHELL_ALIAS Alias
    );

BOOL
ShPrintAlias (
    PSHELL_ALIAS Alias
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL ShDebugAlias = FALSE;

//
// ------------------------------------------------------------------ Functions
//

BOOL
ShPerformAliasSubstitution (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine destroys all the aliases in a shell. It is usually called
    during cleanup.

Arguments:

    Shell - Supplies a pointer to the shell whose aliases will be cleaned up.

Return Value:

    TRUE on success (either looking it up and substituting or looking it up and
    finding nothing).

    FALSE on failure.

--*/

{

    PSHELL_ALIAS Alias;
    PSHELL_LEXER_STATE Lexer;
    BOOL Result;

    Lexer = &(Shell->Lexer);
    Alias = ShLookupAlias(Shell, Lexer->TokenBuffer, Lexer->TokenBufferSize);
    if (Alias == NULL) {
        Result = TRUE;
        goto PerformAliasSubstitutionEnd;
    }

    //
    // If this alias is recursive (ie ls='ls -la') then don't take the bait.
    //

    if (Alias == Lexer->LastAlias) {
        if (ShDebugAlias != FALSE) {
            ShPrintTrace(Shell, "AliasSkipped: %s\n", Lexer->LastAlias->Name);
        }

        Result = TRUE;
        goto PerformAliasSubstitutionEnd;
    }

    assert((Alias->Value != NULL) && (Alias->ValueSize >= 1));

    if (ShDebugAlias != FALSE) {
        ShPrintTrace(Shell,
                     "Aliasing '%s', replacing with '%s'\n",
                     Lexer->TokenBuffer,
                     Alias->Value);
    }

    //
    // If the unput character is valid, it needs to be rolled back into the
    // input too before this replacement text is spliced in. Most of the time
    // this is easy, it can just be put in some earlier space in the buffer.
    //

    if (Lexer->UnputCharacterValid != FALSE) {
        if (Lexer->InputBufferNextIndex != 0) {
            Lexer->InputBufferNextIndex -= 1;
            Lexer->InputBuffer[Lexer->InputBufferNextIndex] =
                                                         Lexer->UnputCharacter;

        //
        // There's no space for the unput character, so bring out the heavy
        // artillery.
        //

        } else {
            Result = SwStringReplaceRegion(&(Lexer->InputBuffer),
                                           &(Lexer->InputBufferSize),
                                           &(Lexer->InputBufferCapacity),
                                           0,
                                           0,
                                           &(Lexer->UnputCharacter),
                                           2);

            if (Result == FALSE) {
                goto PerformAliasSubstitutionEnd;
            }
        }

        Lexer->UnputCharacterValid = FALSE;
    }

    //
    // The substitution needs to be performed. Put the value onto the input
    // buffer. Start by expanding the buffer if needed.
    //

    Result = SwStringReplaceRegion(&(Lexer->InputBuffer),
                                   &(Lexer->InputBufferSize),
                                   &(Lexer->InputBufferCapacity),
                                   Lexer->InputBufferNextIndex,
                                   Lexer->InputBufferNextIndex,
                                   Alias->Value,
                                   Alias->ValueSize);

    if (Result == FALSE) {
        goto PerformAliasSubstitutionEnd;
    }

    //
    // Clear out this input token, as it was replaced. Also mark this alias as
    // the previous one so that if the value of this alias is recursive this
    // doesn't result in an infinite loop.
    //

    Lexer->TokenBufferSize = 0;
    Lexer->TokenType = -1;
    Lexer->LastAlias = Alias;
    Result = TRUE;

PerformAliasSubstitutionEnd:
    return Result;
}

VOID
ShDestroyAliasList (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine destroys all the aliases in a shell. It is usually called
    during cleanup.

Arguments:

    Shell - Supplies a pointer to the shell whose aliases will be cleaned up.

Return Value:

    None.

--*/

{

    PSHELL_ALIAS Alias;

    while (LIST_EMPTY(&(Shell->AliasList)) == FALSE) {
        Alias = LIST_VALUE(Shell->AliasList.Next, SHELL_ALIAS, ListEntry);
        LIST_REMOVE(&(Alias->ListEntry));
        ShDestroyAlias(Alias);
    }

    return;
}

INT
ShBuiltinAlias (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin alias statement.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments are in the form name=value, where name will get
        substituted for value when it is found as the first word in a command.

Return Value:

    0 on success.

    1 if an alias was not found.

--*/

{

    PSHELL_ALIAS Alias;
    PSTR Argument;
    INT ArgumentIndex;
    UINTN ArgumentSize;
    PLIST_ENTRY CurrentEntry;
    PSTR Equals;
    PSTR Name;
    UINTN NameSize;
    BOOL Result;
    ULONG ReturnValue;
    PSTR Value;
    UINTN ValueSize;

    Alias = NULL;
    Name = NULL;
    Value = NULL;

    //
    // If there are no arguments, then print all the aliases.
    //

    if (ArgumentCount == 1) {
        CurrentEntry = Shell->AliasList.Next;
        while (CurrentEntry != &(Shell->AliasList)) {
            Alias = LIST_VALUE(CurrentEntry, SHELL_ALIAS, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            Result = ShPrintAlias(Alias);
            if (Result == FALSE) {
                return 1;
            }
        }

        return 0;
    }

    //
    // Loop through each argument and create or print the alias.
    //

    ReturnValue = 0;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        ArgumentSize = strlen(Argument) + 1;
        Equals = strchr(Argument, '=');
        if ((Equals == NULL) || (Equals == Argument)) {
            Alias = ShLookupAlias(Shell, Argument, ArgumentSize);
            if (Alias == NULL) {
                PRINT_ERROR("Alias %s not found.\n", Argument);
                ReturnValue = 1;

            } else {
                ShPrintAlias(Alias);
            }

            Alias = NULL;
            continue;
        }

        //
        // Create or replace the alias.
        //

        NameSize = (UINTN)Equals - (UINTN)Argument + 1;
        Alias = ShLookupAlias(Shell, Argument, NameSize);
        if (Alias == NULL) {
            Alias = malloc(sizeof(SHELL_ALIAS));
            if (Alias == NULL) {
                ReturnValue = 1;
                goto BuiltinAliasEnd;
            }

            memset(Alias, 0, sizeof(SHELL_ALIAS));
            Name = SwStringDuplicate(Argument, NameSize);
            if (Name == NULL) {
                ReturnValue = 1;
                goto BuiltinAliasEnd;
            }

            Alias->Name = Name;
            Alias->NameSize = NameSize;
        }

        //
        // Create a copy of the value, and add a space onto the end of it.
        //

        ValueSize = ArgumentSize - NameSize + 1;

        assert(ValueSize >= 2);

        Value = malloc(ValueSize);
        if (Value == NULL) {
            ReturnValue = 1;
            goto BuiltinAliasEnd;
        }

        memcpy(Value, Equals + 1, ValueSize - 1);
        Value[ValueSize - 2] = ' ';
        Value[ValueSize - 1] = '\0';
        if (Alias->Value != NULL) {
            free(Alias->Value);
        }

        Alias->Value = Value;
        Alias->ValueSize = ValueSize;
        if (Alias->ListEntry.Next == NULL) {
            INSERT_BEFORE(&(Alias->ListEntry), &(Shell->AliasList));
        }

        Alias = NULL;
        Name = NULL;
        Value = NULL;
    }

BuiltinAliasEnd:
    if (Alias != NULL) {
        free(Alias);
    }

    if (Name != NULL) {
        free(Name);
    }

    if (Value != NULL) {
        free(Value);
    }

    return ReturnValue;
}

INT
ShBuiltinUnalias (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin unalias statement.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments are in the form name=value, where name will get
        substituted for value when it is found as the first word in a command.

Return Value:

    0 on success.

    1 if an alias was not found.

--*/

{

    PSHELL_ALIAS Alias;
    PSTR Argument;
    ULONG ArgumentIndex;
    INT ReturnValue;

    ReturnValue = 0;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];

        //
        // The -a flag destroys all aliases.
        //

        if (strcmp(Argument, "-a") == 0) {
            ShDestroyAliasList(Shell);
            return 0;
        }

        Alias = ShLookupAlias(Shell, Argument, strlen(Argument) + 1);
        if (Alias == NULL) {
            PRINT_ERROR("Alias %s not found.\n", Argument);
            ReturnValue = 1;

        } else {
            LIST_REMOVE(&(Alias->ListEntry));
            ShDestroyAlias(Alias);
        }
    }

    return ReturnValue;
}

PSHELL_ALIAS
ShLookupAlias (
    PSHELL Shell,
    PSTR Name,
    ULONG NameSize
    )

/*++

Routine Description:

    This routine looks up the given name and tries to find an alias for it.

Arguments:

    Shell - Supplies a pointer to the shell to search.

    Name - Supplies a pointer to the name to search for.

    NameSize - Supplies the size of the name buffer in bytes including the
        null terminator.

Return Value:

    Returns a pointer to the alias matching the given name on success.

    NULL if no alias could be found matching the given name.

--*/

{

    PSHELL_ALIAS Alias;
    PLIST_ENTRY CurrentEntry;

    assert((Name != NULL) && (NameSize != 0));

    CurrentEntry = Shell->AliasList.Next;
    while (CurrentEntry != &(Shell->AliasList)) {
        Alias = LIST_VALUE(CurrentEntry, SHELL_ALIAS, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (strncmp(Alias->Name, Name, NameSize - 1) == 0) {
            return Alias;
        }
    }

    return NULL;
}

BOOL
ShCopyAliases (
    PSHELL Source,
    PSHELL Destination
    )

/*++

Routine Description:

    This routine copies the list of declared aliases from one shell to
    another.

Arguments:

    Source - Supplies a pointer to the shell containing the aliases to copy.

    Destination - Supplies a pointer to the shell where the aliases should be
        copied to.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_ALIAS Alias;
    PLIST_ENTRY CurrentEntry;
    PSHELL_ALIAS NewAlias;

    CurrentEntry = Source->AliasList.Next;
    while (CurrentEntry != &(Source->AliasList)) {
        Alias = LIST_VALUE(CurrentEntry, SHELL_ALIAS, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        NewAlias = malloc(sizeof(SHELL_ALIAS));
        if (NewAlias == NULL) {
            return FALSE;
        }

        memcpy(NewAlias, Alias, sizeof(SHELL_ALIAS));
        NewAlias->Name = SwStringDuplicate(Alias->Name, Alias->NameSize);
        NewAlias->Value = SwStringDuplicate(Alias->Value, Alias->ValueSize);
        if ((NewAlias->Name == NULL) || (NewAlias->Value == NULL)) {
            if (NewAlias->Name != NULL) {
                free(NewAlias->Name);
            }

            if (NewAlias->Value != NULL) {
                free(NewAlias->Value);
            }

            free(NewAlias);
            return FALSE;
        }

        INSERT_BEFORE(&(NewAlias->ListEntry), &(Destination->AliasList));
    }

    return TRUE;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ShDestroyAlias (
    PSHELL_ALIAS Alias
    )

/*++

Routine Description:

    This routine frees a shell alias. It assumes the alias has already been
    removed from the shell's alias list.

Arguments:

    Alias - Supplies a pointer to the alias to destroy.

Return Value:

    None.

--*/

{

    if (Alias->Name != NULL) {
        free(Alias->Name);
    }

    if (Alias->Value != NULL) {
        free(Alias->Value);
    }

    free(Alias);
    return;
}

BOOL
ShPrintAlias (
    PSHELL_ALIAS Alias
    )

/*++

Routine Description:

    This routine prints a shell alias.

Arguments:

    Alias - Supplies a pointer to the alias to destroy.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;
    PSTR Value;
    UINTN ValueSize;

    printf("%s=", Alias->Name);
    Result = ShStringFormatForReentry(Alias->Value,
                                      Alias->ValueSize,
                                      &Value,
                                      &ValueSize);

    if (Result == FALSE) {
        return FALSE;
    }

    printf("%s\n", Value);
    free(Value);
    return TRUE;
}

