/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    regexexe.c

Abstract:

    This module implements support for execution of compiled regular
    expressions.

Author:

    Evan Green 9-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#define LIBC_API __DLLEXPORT

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "regexp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of internal matches that are stored. Ten are needed to
// support back references.
//

#define REGEX_INTERNAL_MATCH_COUNT 11

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _REGULAR_EXPRESSION_CHOICE
    REGULAR_EXPRESSION_CHOICE, *PREGULAR_EXPRESSION_CHOICE;

/*++

Structure Description:

    This structure stores the state for a choice made during execution of a
    regular expression.

Members:

    ListEntry - Stores pointers to the next and previous choices in the parent
        list.

    ChildList - Stores the list of child choices.

    Parent - Stores a pointer to the parent choice.

    Node - Stores a pointer to the node in the compiled tree this choice
        refers to. This is either a repeated node or a branch.

    SavedNextIndex - Stores the "next index" value from the context pointer
        right before this choice was made.

    Iteration - Stores the iteration number for repeat choices.

    SavedMatchStart - Stores the match start value to use if this choice is
        reactivated from an old path.

    SavedMatchEnd - Stores the match end value to use if this choice is
        reactivated from a revised choice.

    Choice - Stores a pointer to the branch option for branch choices.

    Value - Stores the value of the choice.

--*/

struct _REGULAR_EXPRESSION_CHOICE {
    LIST_ENTRY ListEntry;
    LIST_ENTRY ChildList;
    PREGULAR_EXPRESSION_CHOICE Parent;
    PREGULAR_EXPRESSION_ENTRY Node;
    ULONG SavedNextIndex;
    union {
        struct {
            ULONG Iteration;
            ULONG SavedMatchStart;
            ULONG SavedMatchEnd;
        };

        PREGULAR_EXPRESSION_ENTRY Choice;
    } U;

};

/*++

Structure Description:

    This structure defines the internal state used during execution of a
    regular expression.

Members:

    Input - Stores a pointer to the input string to match against.

    InputSize - Stores the size of the input string in bytes including the
        null terminator.

    NextInput - Stores the next input character to check in the pattern.

    Flags - Stores flags governing this execution.

    Match - Stores the array where the matches will be returned.

    MatchSize - Stores the element count of the match array.

    Choices - Stores the head of the choice tree.

    FreeChoices - Stores the head of the list of choice structures that have
        been destroyed. This is an optimization to speed up future choice
        allocations.

    InternalMatch - Stores the internal match array, used to support back
        references (\1, \2, etc).

--*/

typedef struct _REGULAR_EXPRESSION_EXECUTION {
    PREGULAR_EXPRESSION Expression;
    PSTR Input;
    ULONG InputSize;
    ULONG NextInput;
    ULONG Flags;
    regmatch_t *Match;
    size_t MatchSize;
    LIST_ENTRY Choices;
    LIST_ENTRY FreeChoices;
    regmatch_t InternalMatch[REGEX_INTERNAL_MATCH_COUNT];
} REGULAR_EXPRESSION_EXECUTION, *PREGULAR_EXPRESSION_EXECUTION;

//
// ----------------------------------------------- Internal Function Prototypes
//

REGULAR_EXPRESSION_STATUS
ClpExecuteRegularExpression (
    PREGULAR_EXPRESSION RegularExpression,
    PSTR String,
    regmatch_t Match[],
    size_t MatchArraySize,
    int Flags
    );

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatch (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    );

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatchEntry (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    );

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatchOrdinaryCharacters (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    );

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatchString (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PSTR CompareString,
    ULONG CompareStringSize
    );

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatchBracketExpression (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    );

VOID
ClpRegularExpressionMarkEnd (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    );

PREGULAR_EXPRESSION_CHOICE
ClpRegularExpressionCreateChoice (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_CHOICE Parent,
    PREGULAR_EXPRESSION_ENTRY Entry,
    ULONG Iteration
    );

VOID
ClpRegularExpressionDestroyChoice (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_CHOICE Choice
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
regexec (
    const regex_t *RegularExpression,
    const char *String,
    size_t MatchArraySize,
    regmatch_t Match[],
    int Flags
    )

/*++

Routine Description:

    This routine executes a regular expression, performing a search of the
    given string to see if it matches the regular expression.

Arguments:

    RegularExpression - Supplies a pointer to the compiled regular expression.

    String - Supplies a pointer to the string to check for a match.

    MatchArraySize - Supplies the number of elements in the match array
        parameter. Supply zero and the match array parameter will be ignored.

    Match - Supplies an optional pointer to an array where the string indices of
        the match and its subexpressions will be returned.

    Flags - Supplies a bitfield of flags governing the search. See some REG_*
        definitions (specifically REG_NOTBOL and REG_NOTEOL).

Return Value:

    0 on successful completion (there was a match).

    REG_NOMATCH if there was no match.

--*/

{

    REGULAR_EXPRESSION_STATUS Status;

    Status = ClpExecuteRegularExpression(RegularExpression->re_data,
                                         (PSTR)String,
                                         Match,
                                         MatchArraySize,
                                         Flags);

    if (Status == RegexStatusSuccess) {
        return 0;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

REGULAR_EXPRESSION_STATUS
ClpExecuteRegularExpression (
    PREGULAR_EXPRESSION RegularExpression,
    PSTR String,
    regmatch_t Match[],
    size_t MatchArraySize,
    int Flags
    )

/*++

Routine Description:

    This routine executes a regular expression, performing a search of the
    given string to see if it matches the regular expression.

Arguments:

    RegularExpression - Supplies a pointer to the compiled regular expression.

    String - Supplies a pointer to the string to check for a match.

    MatchArraySize - Supplies the number of elements in the match array
        parameter. Supply zero and the match array parameter will be ignored.

    Match - Supplies an optional pointer to an array where the string indices of
        the match and its subexpressions will be returned.

    Flags - Supplies a bitfield of flags governing the search. See some REG_*
        definitions (specifically REG_NOTBOL and REG_NOTEOL).

Return Value:

    Success if there was a match.

    No match if there was no match.

--*/

{

    REGULAR_EXPRESSION_EXECUTION Context;
    PLIST_ENTRY FreeEntry;
    size_t MatchIndex;
    ULONG StartIndex;
    REGULAR_EXPRESSION_STATUS Status;

    Status = RegexStatusNoMatch;
    INITIALIZE_LIST_HEAD(&(Context.Choices));
    INITIALIZE_LIST_HEAD(&(Context.FreeChoices));
    Context.Expression = RegularExpression;
    Context.Input = String;
    Context.InputSize = strlen(String) + 1;
    Context.Flags = Flags;
    Context.Match = Match;
    Context.MatchSize = MatchArraySize;
    if ((RegularExpression->Flags & REG_NOSUB) == 0) {
        for (MatchIndex = 0; MatchIndex < MatchArraySize; MatchIndex += 1) {
            Match[MatchIndex].rm_so = -1;
            Match[MatchIndex].rm_eo = -1;
        }
    }

    for (MatchIndex = 0;
         MatchIndex < REGEX_INTERNAL_MATCH_COUNT;
         MatchIndex += 1) {

        Context.InternalMatch[MatchIndex].rm_so = -1;
        Context.InternalMatch[MatchIndex].rm_eo = -1;
    }

    //
    // Try to match the expression starting at each index.
    //

    for (StartIndex = 0; StartIndex < Context.InputSize; StartIndex += 1) {

        //
        // If the expression is anchored to the left, then this had better be:
        // 1) Index zero and REG_NOTBOL is clear or
        // 2) Right after a newline and REG_NEWLINE is set.
        // If it's not one of these things then it definitely does not match.
        //

        if ((RegularExpression->BaseEntry.Flags &
             REGULAR_EXPRESSION_ANCHORED_LEFT) != 0) {

            if (!(((StartIndex == 0) && ((Flags & REG_NOTBOL) == 0)) ||
                  (((RegularExpression->Flags & REG_NEWLINE) != 0) &&
                   (StartIndex != 0) && (String[StartIndex - 1] == '\n')))) {

                Status = RegexStatusNoMatch;
                continue;
            }
        }

        Context.NextInput = StartIndex;
        Status = ClpRegularExpressionMatch(&Context,
                                           &(RegularExpression->BaseEntry));

        if (Status == RegexStatusSuccess) {

            //
            // If the expression is anchored to the right then either:
            // 1) The index had better be the end and REG_NOTEOL is clear or
            // 2) REG_NEWLINE is set and it's right before a newline.
            // If this condition isn't met then it's not a real match.
            //

            if ((RegularExpression->BaseEntry.Flags &
                 REGULAR_EXPRESSION_ANCHORED_RIGHT) != 0) {

                if (!(((Context.NextInput == Context.InputSize - 1) &&
                      ((Flags & REG_NOTEOL) == 0)) ||
                      (((RegularExpression->Flags & REG_NEWLINE) != 0) &&
                       (String[Context.NextInput] == '\n')))) {

                    Status = RegexStatusNoMatch;
                    continue;
                }
            }

            break;
        }
    }

    //
    // Save the overall match if found.
    //

    if (Status == RegexStatusSuccess) {
        if (((RegularExpression->Flags & REG_NOSUB) == 0) &&
            (MatchArraySize > 0)) {

            Match[0].rm_so = StartIndex;
            Match[0].rm_eo = Context.NextInput;
        }

    //
    // On failure, blank out the matches again.
    //

    } else if ((RegularExpression->Flags & REG_NOSUB) == 0) {
        for (MatchIndex = 0; MatchIndex < MatchArraySize; MatchIndex += 1) {
            Match[MatchIndex].rm_so = -1;
            Match[MatchIndex].rm_eo = -1;
        }
    }

    //
    // Destroy any remaining choices.
    //

    assert(LIST_EMPTY(&(Context.Choices)) != FALSE);

    while (!LIST_EMPTY(&(Context.FreeChoices))) {
        FreeEntry = Context.FreeChoices.Next;
        LIST_REMOVE(FreeEntry);
        free(FreeEntry);
    }

    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatch (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    )

/*++

Routine Description:

    This routine determines if the given regular expression entry (and all
    those after it in the list) matches the string contained in the context.

Arguments:

    Context - Supplies a pointer to the execution context.

    Entry - Supplies the regular expression entry to match.

Return Value:

    Success if there was a match.

    No match if there was no match.

--*/

{

    PREGULAR_EXPRESSION_ENTRY BranchOption;
    PREGULAR_EXPRESSION_CHOICE CurrentChoice;
    ULONG DuplicateMax;
    ULONG DuplicateMin;
    ULONG Iteration;
    regmatch_t *Match;
    PREGULAR_EXPRESSION_CHOICE NextChoice;
    PREGULAR_EXPRESSION_ENTRY Parent;
    REGULAR_EXPRESSION_STATUS Status;
    ULONG SubexpressionNumber;
    BOOL UseThisEntry;

    CurrentChoice = NULL;
    Iteration = 0;
    Status = RegexStatusSuccess;
    UseThisEntry = FALSE;

    //
    // Loop through every entry until none are left (success) or there are no
    // more possible choices (failure).
    //

    while (Entry != NULL) {
        Parent = Entry->Parent;
        DuplicateMin = Entry->DuplicateMin;
        DuplicateMax = Entry->DuplicateMax;

        //
        // Try to match this entry if it needs more iterations.
        //

        if ((DuplicateMax == -1) || (Iteration < DuplicateMax)) {

            //
            // If this is a subexpression, go inside.
            //

            if (Entry->Type == RegexEntrySubexpression) {

                //
                // Add a choice entry, even for empty subexpressions, so the
                // matches can be rebuilt if this choice is jumped to later.
                //

                if (UseThisEntry == FALSE) {
                    CurrentChoice = ClpRegularExpressionCreateChoice(
                                                                 Context,
                                                                 CurrentChoice,
                                                                 Entry,
                                                                 Iteration);

                    if (CurrentChoice == NULL) {
                        Status = RegexStatusNoMemory;
                        goto RegularExpressionMatchEnd;
                    }
                }

                UseThisEntry = FALSE;

                //
                // Save the original values of the match in the choice in case
                // this has to be undone, and mark the beginning of the match
                // for this subexpression.
                //

                assert(CurrentChoice->Node == Entry);

                SubexpressionNumber = Entry->U.SubexpressionNumber;
                if ((SubexpressionNumber < Context->MatchSize) &&
                    ((Context->Expression->Flags & REG_NOSUB) == 0)) {

                    Match = &(Context->Match[SubexpressionNumber]);
                    CurrentChoice->U.SavedMatchStart = Match->rm_so;
                    CurrentChoice->U.SavedMatchEnd = Match->rm_eo;
                    Match->rm_so = Context->NextInput;
                    Match->rm_eo = Context->NextInput;
                }

                if (SubexpressionNumber < REGEX_INTERNAL_MATCH_COUNT) {
                    Match = &(Context->InternalMatch[SubexpressionNumber]);
                    CurrentChoice->U.SavedMatchStart = Match->rm_so;
                    CurrentChoice->U.SavedMatchEnd = Match->rm_eo;
                    Match->rm_so = Context->NextInput;
                    Match->rm_eo = Context->NextInput;
                }

                if (LIST_EMPTY(&(Entry->ChildList)) != FALSE) {
                    Status = RegexStatusSuccess;

                } else {
                    Entry = LIST_VALUE(Entry->ChildList.Next,
                                       REGULAR_EXPRESSION_ENTRY,
                                       ListEntry);

                    Iteration = 0;
                    continue;
                }

            //
            // If this is a branch, take the first choice, push it on the stack,
            // and loop.
            //

            } else if (Entry->Type == RegexEntryBranch) {

                assert(Iteration == 0);
                assert((Entry->DuplicateMin == 1) &&
                       (Entry->DuplicateMax == 1));

                assert(LIST_EMPTY(&(Entry->ChildList)) == FALSE);

                Entry = LIST_VALUE(Entry->ChildList.Next,
                                   REGULAR_EXPRESSION_ENTRY,
                                   ListEntry);

                assert(Entry->Type == RegexEntryBranchOption);
                assert(UseThisEntry == FALSE);

                CurrentChoice = ClpRegularExpressionCreateChoice(Context,
                                                                 CurrentChoice,
                                                                 Entry->Parent,
                                                                 Iteration);

                if (CurrentChoice == NULL) {
                    Status = RegexStatusNoMemory;
                    goto RegularExpressionMatchEnd;
                }

                CurrentChoice->U.Choice = Entry;
                continue;

            //
            // If this is a branch option, just move on to the first child, or
            // success if there are no children.
            //

            } else if (Entry->Type == RegexEntryBranchOption) {

                assert(UseThisEntry == FALSE);

                if (LIST_EMPTY(&(Entry->ChildList)) != FALSE) {
                    Status = RegexStatusSuccess;

                } else {
                    Entry = LIST_VALUE(Entry->ChildList.Next,
                                       REGULAR_EXPRESSION_ENTRY,
                                       ListEntry);

                    continue;
                }

            //
            // If this is neither a subexpression nor a branch, just try to
            // match it.
            //

            } else {

                //
                // If there is a choice up in here, create a new entry for it.
                //

                if ((DuplicateMax == -1) || (DuplicateMin != DuplicateMax)) {
                    if (UseThisEntry == FALSE) {
                        CurrentChoice = ClpRegularExpressionCreateChoice(
                                                                 Context,
                                                                 CurrentChoice,
                                                                 Entry,
                                                                 Iteration);

                        if (CurrentChoice == NULL) {
                            Status = RegexStatusNoMemory;
                            goto RegularExpressionMatchEnd;
                        }
                    }
                }

                UseThisEntry = FALSE;
                Status = ClpRegularExpressionMatchEntry(Context, Entry);
            }

        //
        // The entry has already got enough iterations.
        //

        } else {
            Status = RegexStatusSuccess;
        }

        UseThisEntry = FALSE;

        //
        // Down here, something must have matched or not. Either an empty
        // subexpression, empty branch, or something substantive. Now's the time
        // to deal with that success or failure.
        //

        if (Status == RegexStatusSuccess) {

            //
            // Move on to the next node, which may involve popping up several
            // levels.
            //

            while (TRUE) {
                Iteration += 1;

                //
                // If the input didn't move anywhere, this just matched an
                // empty expression. Prevent that from happening infinitely.
                //

                if ((Context->NextInput == CurrentChoice->SavedNextIndex) &&
                    (DuplicateMax != 1)) {

                    assert(CurrentChoice->Node == Entry);

                    DuplicateMax = Iteration;
                }

                //
                // If there are more duplicates of this entry to find, then
                // gosh darnit go find them.
                //

                if ((DuplicateMax == -1) || (Iteration < DuplicateMax)) {
                    break;
                }

                //
                // Splendid, it's time to move forward. If this was a
                // subexpression, mark its ending.
                //

                ClpRegularExpressionMarkEnd(Context, Entry);

                //
                // Whether the next entry is the sibling or the parent, move the
                // current choice up if it corresponds to this entry.
                //

                if (CurrentChoice->Node == Entry) {
                    CurrentChoice = CurrentChoice->Parent;
                }

                //
                // If there's another expression right next to this one,
                // just move to it.
                //

                if ((Parent != NULL) &&
                    (Entry->ListEntry.Next != &(Parent->ChildList))) {

                    Entry = LIST_VALUE(Entry->ListEntry.Next,
                                       REGULAR_EXPRESSION_ENTRY,
                                       ListEntry);

                    assert(Entry->Parent == Parent);

                    Iteration = 0;
                    break;
                }

                Entry = Parent;

                //
                // If there are no more entries, then this entire regular
                // expression matches.
                //

                if (Entry == NULL) {
                    goto RegularExpressionMatchEnd;
                }

                if (Entry->Type == RegexEntryBranchOption) {
                    Entry = Entry->Parent;
                }

                Parent = Entry->Parent;

                assert(CurrentChoice->Node == Entry);

                if (Entry->Type == RegexEntryBranch) {
                    Iteration = 0;
                    DuplicateMax = 1;

                } else {
                    Iteration = CurrentChoice->U.Iteration;
                    DuplicateMax = Entry->DuplicateMax;
                }
            }

            continue;

        //
        // This didn't match, it's time to re-evaluate one of the previous
        // decisions.
        //

        } else if (Status == RegexStatusNoMatch) {
            while (CurrentChoice != NULL) {

                //
                // Find the last decision made, which may not be the current
                // decision if the current entry is working on a top level
                // subexpression when the last decision was way down inside the
                // previous subexpression.
                //

                while (LIST_EMPTY(&(CurrentChoice->ChildList)) == FALSE) {
                    CurrentChoice = LIST_VALUE(
                                             CurrentChoice->ChildList.Previous,
                                             REGULAR_EXPRESSION_CHOICE,
                                             ListEntry);
                }

                //
                // Restore the subexpression match values to what they were
                // before the choice.
                //

                Entry = CurrentChoice->Node;
                if (Entry->Type == RegexEntrySubexpression) {
                    SubexpressionNumber = Entry->U.SubexpressionNumber;
                    if ((SubexpressionNumber < Context->MatchSize) &&
                        ((Context->Expression->Flags & REG_NOSUB) == 0)) {

                        Match = &(Context->Match[SubexpressionNumber]);
                        Match->rm_so = CurrentChoice->U.SavedMatchStart;
                        Match->rm_eo = CurrentChoice->U.SavedMatchEnd;
                    }

                    if (SubexpressionNumber < REGEX_INTERNAL_MATCH_COUNT) {
                        Match =
                            &(Context->InternalMatch[SubexpressionNumber]);

                        Match->rm_so = CurrentChoice->U.SavedMatchStart;
                        Match->rm_eo = CurrentChoice->U.SavedMatchEnd;
                    }
                }

                assert((Entry->Type == RegexEntryBranch) ||
                       (Entry->Type == RegexEntrySubexpression) ||
                       (Entry->DuplicateMax != Entry->DuplicateMin));

                //
                // If the entry was a branch, try to move on to the next branch
                // option.
                //

                if (Entry->Type == RegexEntryBranch) {
                    BranchOption = CurrentChoice->U.Choice;
                    if (BranchOption->ListEntry.Next != &(Entry->ChildList)) {
                        Entry = LIST_VALUE(BranchOption->ListEntry.Next,
                                           REGULAR_EXPRESSION_ENTRY,
                                           ListEntry);

                        CurrentChoice->U.Choice = Entry;

                        assert(Entry->Type == RegexEntryBranchOption);

                        Context->NextInput = CurrentChoice->SavedNextIndex;
                        break;
                    }

                //
                // Try to pop the last repeat off and keep going.
                //

                } else {
                    if (CurrentChoice->U.Iteration + 1 > Entry->DuplicateMin) {
                        Context->NextInput = CurrentChoice->SavedNextIndex;
                        NextChoice = CurrentChoice;

                        //
                        // Move to the next entry.
                        //

                        while ((Entry->Parent != NULL) &&
                               ((Entry->Type == RegexEntryBranchOption) ||
                                (Entry->ListEntry.Next ==
                                 &(Entry->Parent->ChildList)))) {

                            if (Entry->Type == RegexEntryBranchOption) {
                                Entry = Entry->Parent;
                                continue;
                            }

                            ClpRegularExpressionMarkEnd(Context, Entry);
                            Entry = Entry->Parent;
                            NextChoice = NextChoice->Parent;
                        }

                        ClpRegularExpressionMarkEnd(Context, Entry);

                        //
                        // If this was the last element, then popping this
                        // failing iteration causes the expression to pass.
                        //

                        if (Entry->Parent == NULL) {
                            Entry = NULL;
                            Status = RegexStatusSuccess;

                        } else {
                            Entry = LIST_VALUE(Entry->ListEntry.Next,
                                               REGULAR_EXPRESSION_ENTRY,
                                               ListEntry);
                        }

                        NextChoice = NextChoice->Parent;
                        LIST_REMOVE(&(CurrentChoice->ListEntry));
                        ClpRegularExpressionDestroyChoice(Context,
                                                          CurrentChoice);

                        CurrentChoice = NextChoice;
                        Iteration = 0;
                        break;
                    }
                }

                //
                // Figure out what the previous choice is, which is basically
                // one back and then all the way deep. If that's not available,
                // then go to the parent.
                //

                if ((CurrentChoice->Parent != NULL) &&
                    (CurrentChoice->ListEntry.Previous !=
                     &(CurrentChoice->Parent->ChildList))) {

                    NextChoice = LIST_VALUE(CurrentChoice->ListEntry.Previous,
                                            REGULAR_EXPRESSION_CHOICE,
                                            ListEntry);

                    while (LIST_EMPTY(&(NextChoice->ChildList)) == FALSE) {
                        NextChoice = LIST_VALUE(NextChoice->ChildList.Previous,
                                                REGULAR_EXPRESSION_CHOICE,
                                                ListEntry);
                    }

                } else {
                    NextChoice = CurrentChoice->Parent;
                }

                assert(CurrentChoice != NextChoice);

                //
                // Pop and destroy this choice, moving back to the previous
                // choice.
                //

                LIST_REMOVE(&(CurrentChoice->ListEntry));
                ClpRegularExpressionDestroyChoice(Context, CurrentChoice);
                CurrentChoice = NextChoice;
                if (CurrentChoice != NULL) {
                    Entry = CurrentChoice->Node;

                } else {
                    goto RegularExpressionMatchEnd;
                }
            }

            continue;

        //
        // Something bizarre happened, return that failure.
        //

        } else {
            goto RegularExpressionMatchEnd;
        }
    }

RegularExpressionMatchEnd:

    //
    // Destroy the choice tree.
    //

    if (LIST_EMPTY(&(Context->Choices)) == FALSE) {
        CurrentChoice = LIST_VALUE(Context->Choices.Next,
                                   REGULAR_EXPRESSION_CHOICE,
                                   ListEntry);

        LIST_REMOVE(&(CurrentChoice->ListEntry));
        ClpRegularExpressionDestroyChoice(Context, CurrentChoice);
    }

    assert(LIST_EMPTY(&(Context->Choices)) != FALSE);

    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatchEntry (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    )

/*++

Routine Description:

    This routine determines if a single occurrence of the given entry matches
    the string in the context.

Arguments:

    Context - Supplies a pointer to the execution context.

    Entry - Supplies the regular expression entry to match.

Return Value:

    Success if there was a match.

    No match if there was no match.

--*/

{

    CHAR Character;
    regmatch_t *Match;
    REGULAR_EXPRESSION_STATUS Status;

    Status = RegexStatusNoMatch;
    switch (Entry->Type) {
    case RegexEntryOrdinaryCharacters:
        Status = ClpRegularExpressionMatchOrdinaryCharacters(Context, Entry);
        break;

    case RegexEntryAnyCharacter:

        //
        // As long as it's not the end of the string or a null terminator then
        // this matches. If the newline flag is set, then newlines don't match
        // either.
        //

        if ((Context->NextInput < Context->InputSize) &&
            (Context->Input[Context->NextInput] != '\0') &&
            ((Context->Input[Context->NextInput] != '\n') ||
             ((Context->Expression->Flags & REG_NEWLINE) == 0))) {

            Status = RegexStatusSuccess;
            Context->NextInput += 1;
        }

        break;

    //
    // Back references match against the value matched in a previous subgroup.
    //

    case RegexEntryBackReference:

        assert(Entry->U.BackReferenceNumber < REGEX_INTERNAL_MATCH_COUNT);

        Match = &(Context->InternalMatch[Entry->U.BackReferenceNumber]);
        if ((Match->rm_so != -1) && (Match->rm_eo != -1)) {
            Status = ClpRegularExpressionMatchString(
                                                 Context,
                                                 Context->Input + Match->rm_so,
                                                 Match->rm_eo - Match->rm_so);
        }

        break;

    case RegexEntryBracketExpression:
        Status = ClpRegularExpressionMatchBracketExpression(Context, Entry);
        break;

    case RegexEntryStringBegin:

        //
        // The input is said to be at the beginning if either:
        // 1) It's at the beginning of the input and "not beginning of line" is
        //    clear. OR
        // 2) The newline flag is set and the current input is right after a
        //    newline.
        //

        if ((((Context->Flags & REG_NOTBOL) == 0) &&
             (Context->NextInput == 0)) ||
            (((Context->Expression->Flags & REG_NEWLINE) != 0) &&
             (Context->NextInput != 0) &&
             (Context->Input[Context->NextInput - 1] == '\n'))) {

            Status = RegexStatusSuccess;
        }

        break;

    case RegexEntryStringEnd:

        //
        // The input is said to be at the end if either:
        // 1) It's at the end of the input and "not end of line" is clear. OR
        // 2) The newline flag is set and the current input is right before a
        //    newline.
        //

        if ((((Context->Flags & REG_NOTEOL) == 0) &&
             ((Context->NextInput >= Context->InputSize) ||
              (Context->Input[Context->NextInput] == '\0'))) ||
            (((Context->Expression->Flags & REG_NEWLINE) != 0) &&
             (Context->NextInput < Context->InputSize) &&
             (Context->Input[Context->NextInput] == '\n'))) {

            Status = RegexStatusSuccess;
        }

        break;

    case RegexEntryStartOfWord:

        //
        // Match at a position that is followed by a word character but not
        // preceded by a word character (the beginning counts as not a word
        // character). The match is zero in length.
        //

        if ((Context->NextInput < Context->InputSize) &&
            (REGULAR_EXPRESSION_IS_NAME(Context->Input[Context->NextInput]))) {

            if (Context->NextInput == 0) {
                Status = RegexStatusSuccess;

            } else {
                Character = Context->Input[Context->NextInput - 1];
                if (!REGULAR_EXPRESSION_IS_NAME(Character)) {
                    Status = RegexStatusSuccess;
                }
            }
        }

        break;

    case RegexEntryEndOfWord:

        //
        // Match at a position that is preceded by a word character but is not
        // followed by a word character. The end counts as not a word
        // character. The match is zero in length.
        //

        if (Context->NextInput == 0) {
            break;
        }

        Character = Context->Input[Context->NextInput - 1];
        if (REGULAR_EXPRESSION_IS_NAME(Character)) {
            if (Context->NextInput >= Context->InputSize) {
                Status = RegexStatusSuccess;

            } else {
                Character = Context->Input[Context->NextInput];
                if (!REGULAR_EXPRESSION_IS_NAME(Character)) {
                    Status = RegexStatusSuccess;
                }
            }
        }

        break;

    default:

        assert(FALSE);

        break;
    }

    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatchOrdinaryCharacters (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    )

/*++

Routine Description:

    This routine determines if the given regular expression entry matches the
    string contained in the context.

Arguments:

    Context - Supplies a pointer to the execution context.

    Entry - Supplies the regular expression entry to match.

Return Value:

    Success if there was a match.

    No match if there was no match.

--*/

{

    PSTR CompareString;
    ULONG CompareStringSize;
    REGULAR_EXPRESSION_STATUS Status;

    assert(Entry->Type == RegexEntryOrdinaryCharacters);

    CompareString = Entry->U.String.Data;

    assert(Entry->U.String.Size != 0);

    CompareStringSize = Entry->U.String.Size;
    Status = ClpRegularExpressionMatchString(Context,
                                             CompareString,
                                             CompareStringSize);

    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatchString (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PSTR CompareString,
    ULONG CompareStringSize
    )

/*++

Routine Description:

    This routine determines if the given regular expression entry matches the
    string contained in the context.

Arguments:

    Context - Supplies a pointer to the execution context.

    CompareString - Supplies a pointer to the string to compare the input to.

    CompareStringSize - Supplies the size of the compare string in bytes
        not including the null terminator (unless that's part of what should be
        compared).

Return Value:

    Success if there was a match.

    No match if there was no match.

--*/

{

    CHAR Character;
    BOOL IgnoreCase;
    ULONG Index;
    PSTR String;
    ULONG StringSize;

    Index = 0;
    String = Context->Input + Context->NextInput;
    StringSize = Context->InputSize - Context->NextInput;

    //
    // Shortcut if the input isn't even as large as the ordinary characters.
    //

    if (StringSize < CompareStringSize) {
        return RegexStatusNoMatch;
    }

    IgnoreCase = FALSE;
    if ((Context->Expression->Flags & REG_ICASE) != 0) {
        IgnoreCase = TRUE;
    }

    //
    // Loop comparing characters.
    //

    while (Index < CompareStringSize) {
        Character = String[Index];

        //
        // Things are on ice if the string doesn't exactly match.
        //

        if (Character != CompareString[Index]) {

            //
            // If the ignore case flag is not set or the strings don't match
            // even after converting to lowercase, then the string really
            // does not match.
            //

            if ((IgnoreCase == FALSE) ||
                (tolower(Character) != tolower(CompareString[Index]))) {

                return RegexStatusNoMatch;
            }
        }

        Index += 1;
    }

    //
    // The loop got all the way through without failing, so this matches.
    //

    Context->NextInput += Index;
    return RegexStatusSuccess;
}

REGULAR_EXPRESSION_STATUS
ClpRegularExpressionMatchBracketExpression (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    )

/*++

Routine Description:

    This routine determines if the given regular expression entry matches the
    given bracket expression.

Arguments:

    Context - Supplies a pointer to the execution context.

    Entry - Supplies the regular expression entry to match.

Return Value:

    Success if there was a match.

    No match if there was no match.

--*/

{

    PREGULAR_BRACKET_ENTRY BracketEntry;
    PREGULAR_BRACKET_EXPRESSION BracketExpression;
    CHAR Character;
    ULONG CharacterCount;
    ULONG CharacterIndex;
    PLIST_ENTRY CurrentEntry;
    PSTR RegularCharacters;
    REGULAR_EXPRESSION_STATUS Status;

    assert(Entry->Type == RegexEntryBracketExpression);

    if (Context->NextInput >= Context->InputSize) {
        return RegexStatusNoMatch;
    }

    Character = Context->Input[Context->NextInput];
    if (Character == '\0') {
        return RegexStatusNoMatch;
    }

    Status = RegexStatusNoMatch;
    BracketExpression = &(Entry->U.BracketExpression);
    CharacterCount = BracketExpression->RegularCharacters.Size;
    RegularCharacters = BracketExpression->RegularCharacters.Data;

    //
    // First match against any of the regular characters.
    //

    for (CharacterIndex = 0;
         CharacterIndex < CharacterCount;
         CharacterIndex += 1) {

        if ((Character == RegularCharacters[CharacterIndex]) ||
            (((Context->Expression->Flags & REG_ICASE) != 0) &&
              (tolower(Character) ==
               tolower(RegularCharacters[CharacterIndex])))) {

            Status = RegexStatusSuccess;
            goto RegularExpressionMatchBracketExpressionEnd;
        }
    }

    //
    // Go through the list of other stuff and see if any of that matches.
    //

    CurrentEntry = BracketExpression->EntryList.Next;
    while (CurrentEntry != &(BracketExpression->EntryList)) {
        BracketEntry = LIST_VALUE(CurrentEntry,
                                  REGULAR_BRACKET_ENTRY,
                                  ListEntry);

        CurrentEntry = CurrentEntry->Next;
        switch (BracketEntry->Type) {
        case BracketExpressionRange:
            if ((Character >= BracketEntry->U.Range.Minimum) &&
                (Character <= BracketEntry->U.Range.Maximum)) {

                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassAlphanumeric:
            if (isalnum(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassAlphabetic:
            if (isalpha(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassBlank:
            if (isblank(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassControl:
            if (iscntrl(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassDigit:
            if (isdigit(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassGraph:
            if (isgraph(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassLowercase:
            if ((islower(Character)) ||
                (((Context->Expression->Flags & REG_ICASE) != 0) &&
                 (isupper(Character)))) {

                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassPrintable:
            if (isprint(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassPunctuation:
            if (ispunct(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassSpace:
            if (isspace(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassUppercase:
            if ((isupper(Character)) ||
                (((Context->Expression->Flags & REG_ICASE) != 0) &&
                 (islower(Character)))) {

                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassHexDigit:
            if (isxdigit(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        case BracketExpressionCharacterClassName:
            if (REGULAR_EXPRESSION_IS_NAME(Character)) {
                Status = RegexStatusSuccess;
            }

            break;

        default:

            assert(FALSE);

            goto RegularExpressionMatchBracketExpressionEnd;
        }

        if (Status == RegexStatusSuccess) {
            break;
        }
    }

RegularExpressionMatchBracketExpressionEnd:
    if ((Entry->Flags & REGULAR_EXPRESSION_NEGATED) != 0) {
        if (Status == RegexStatusNoMatch) {
            Status = RegexStatusSuccess;

        } else if (Status == RegexStatusSuccess) {
            Status = RegexStatusNoMatch;
        }
    }

    if (Status == RegexStatusSuccess) {
        Context->NextInput += 1;
    }

    return Status;
}

VOID
ClpRegularExpressionMarkEnd (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_ENTRY Entry
    )

/*++

Routine Description:

    This routine marks the end of a subexpression match for a subexpression
    that just finished matching.

Arguments:

    Context - Supplies a pointer to the execution context.

    Entry - Supplies a pointer to the regular expression entry that just
        finished matching.

Return Value:

    None.

--*/

{

    regmatch_t *Match;
    ULONG SubexpressionNumber;

    if (Entry->Type != RegexEntrySubexpression) {
        return;
    }

    SubexpressionNumber = Entry->U.SubexpressionNumber;
    if ((SubexpressionNumber < Context->MatchSize) &&
        ((Context->Expression->Flags & REG_NOSUB) == 0)) {

        Match = &(Context->Match[SubexpressionNumber]);
        Match->rm_eo = Context->NextInput;
    }

    if (SubexpressionNumber < REGEX_INTERNAL_MATCH_COUNT) {
        Match = &(Context->InternalMatch[SubexpressionNumber]);
        Match->rm_eo = Context->NextInput;
    }

    return;
}

PREGULAR_EXPRESSION_CHOICE
ClpRegularExpressionCreateChoice (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_CHOICE Parent,
    PREGULAR_EXPRESSION_ENTRY Entry,
    ULONG Iteration
    )

/*++

Routine Description:

    This routine creates a new regular expression choice structure,
    initializes it, and adds it to the proper place in the choice tree.

Arguments:

    Context - Supplies a pointer to the execution context.

    Parent - Supplies an optional pointer to the parent to add the choice
        under. If this is null, the choice will be added directly into the
        context.

    Entry - Supplies a pointer to the expression corresponding to this choice.

    Iteration - Supplies the iteration (duplicate) count of this entry. If this
        is not the first iteration, then the choice will be added to the
        parent of the given parent choice so that dupilcates appear as siblings
        (presumably the previous duplicate is the previous entry in the list,
        which is an assumption taken when backtracking).

Return Value:

    Returns a pointer to the choice structure on success.

    NULL on allocation failure.

--*/

{

    PREGULAR_EXPRESSION_CHOICE NewChoice;

    //
    // Attempt to use one from the free list if there is any, or allocate one
    // otherwise.
    //

    if (LIST_EMPTY(&(Context->FreeChoices)) == FALSE) {
        NewChoice = LIST_VALUE(Context->FreeChoices.Next,
                               REGULAR_EXPRESSION_CHOICE,
                               ListEntry);

        LIST_REMOVE(&(NewChoice->ListEntry));

    } else {
        NewChoice = malloc(sizeof(REGULAR_EXPRESSION_CHOICE));
        if (NewChoice == NULL) {
            return NULL;
        }
    }

    memset(NewChoice, 0, sizeof(REGULAR_EXPRESSION_CHOICE));
    INITIALIZE_LIST_HEAD(&(NewChoice->ChildList));
    NewChoice->Parent = Parent;
    NewChoice->Node = Entry;
    NewChoice->SavedNextIndex = Context->NextInput;
    NewChoice->U.Iteration = Iteration;
    if (Iteration != 0) {

        //
        // Make repeats siblings of each other.
        //

        assert((Parent != NULL) && (Parent->Parent != NULL));

        NewChoice->Parent = Parent->Parent;
        INSERT_BEFORE(&(NewChoice->ListEntry), &(Parent->Parent->ChildList));

    } else {
        if (Parent != NULL) {
            INSERT_BEFORE(&(NewChoice->ListEntry), &(Parent->ChildList));

        } else {
            INSERT_BEFORE(&(NewChoice->ListEntry), &(Context->Choices));
        }
    }

    return NewChoice;
}

VOID
ClpRegularExpressionDestroyChoice (
    PREGULAR_EXPRESSION_EXECUTION Context,
    PREGULAR_EXPRESSION_CHOICE Choice
    )

/*++

Routine Description:

    This routine destroys a regular expression choice and recursively all of
    its child choices. It is assumed that this choice is already removed from
    its parent list.

Arguments:

    Context - Supplies a pointer to the execution context.

    Choice - Supplies a pointer to the choice to destroy.

Return Value:

    None.

--*/

{

    PREGULAR_EXPRESSION_CHOICE CurrentChoice;
    PREGULAR_EXPRESSION_CHOICE NextChoice;

    //
    // Destroy the choice tree.
    //

    if (LIST_EMPTY(&(Choice->ChildList)) == FALSE) {
        CurrentChoice = LIST_VALUE(Choice->ChildList.Next,
                                   REGULAR_EXPRESSION_CHOICE,
                                   ListEntry);

        while (CurrentChoice != NULL) {

            assert(CurrentChoice != Choice);

            //
            // Find the first leaf node.
            //

            while (LIST_EMPTY(&(CurrentChoice->ChildList)) == FALSE) {
                CurrentChoice = LIST_VALUE(CurrentChoice->ChildList.Next,
                                           REGULAR_EXPRESSION_CHOICE,
                                           ListEntry);
            }

            //
            // Get the next entry (the parent of this leaf).
            //

            if (CurrentChoice->Parent != Choice) {
                NextChoice = CurrentChoice->Parent;

            //
            // If the parent is the main, this is a top level choice.
            //

            } else if (CurrentChoice->ListEntry.Next != &(Choice->ChildList)) {
                NextChoice = LIST_VALUE(CurrentChoice->ListEntry.Next,
                                        REGULAR_EXPRESSION_CHOICE,
                                        ListEntry);

            //
            // If this was the last top level choice, then be done.
            //

            } else {
                NextChoice = NULL;
            }

            assert(LIST_EMPTY(&(CurrentChoice->ChildList)) != FALSE);

            LIST_REMOVE(&(CurrentChoice->ListEntry));

            //
            // Stick this on the head of the free list, as it's hottest in the
            // cache.
            //

            INSERT_AFTER(&(CurrentChoice->ListEntry), &(Context->FreeChoices));
            CurrentChoice = NextChoice;
        }
    }

    free(Choice);
    return;
}

