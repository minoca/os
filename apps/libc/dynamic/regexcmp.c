/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    regexcmp.c

Abstract:

    This module implements support for compiling regular expressions.

Author:

    Evan Green 8-Jul-2013

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
// Define the maximum size of a string returned by regerror.
//

#define REGULAR_EXPRESSION_ERROR_STRING_MAX_SIZE 512

//
// Define regular expression tokens.
//

#define TOKEN_ESCAPED_OPEN_PARENTHESES 512
#define TOKEN_ESCAPED_CLOSE_PARENTHESES 513
#define TOKEN_ESCAPED_OPEN_BRACE 514
#define TOKEN_ESCAPED_CLOSE_BRACE 515
#define TOKEN_QUOTED_CHARACTER 516
#define TOKEN_BACK_REFERENCE 517

//
// Define bracket expression tokens.
//

#define TOKEN_OPEN_EQUAL 550
#define TOKEN_EQUAL_CLOSE 551
#define TOKEN_OPEN_DOT 552
#define TOKEN_DOT_CLOSE 553
#define TOKEN_OPEN_COLON 554
#define TOKEN_COLON_CLOSE 555

//
// Define the initial buffer size for regular expression strings.
//

#define REGULAR_EXPRESSION_INITIAL_STRING_SIZE 16

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the lexer state for the regular expression compiler.

Members:

    Input - Stores the input string.

    InputSize - Stores the size of the input string in bytes including the
        null terminator.

    NextInput - Stores the offset of the next character to grab.

    Token - Stores the current token.

    ActiveSubexpressionCount - Stores the number of subexpressions currently
        being parsed (the number of close parentheses to treat as special
        characters).

--*/

typedef struct _REGULAR_EXPRESSION_LEXER {
    PSTR Input;
    ULONG InputSize;
    ULONG NextInput;
    ULONG Token;
    ULONG ActiveSubexpressionCount;
} REGULAR_EXPRESSION_LEXER, *PREGULAR_EXPRESSION_LEXER;

//
// ----------------------------------------------- Internal Function Prototypes
//

REGULAR_EXPRESSION_STATUS
ClpCompileRegularExpression (
    PSTR Pattern,
    ULONG Flags,
    PREGULAR_EXPRESSION *Expression
    );

VOID
ClpDestroyRegularExpression (
    PREGULAR_EXPRESSION Expression
    );

REGULAR_EXPRESSION_STATUS
ClpParseCompleteBasicRegularExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression
    );

REGULAR_EXPRESSION_STATUS
ClpParseBasicRegularExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY ParentEntry
    );

REGULAR_EXPRESSION_STATUS
ClpParseSimpleExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY *Entry
    );

REGULAR_EXPRESSION_STATUS
ClpParseExtendedRegularExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY ParentEntry
    );

REGULAR_EXPRESSION_STATUS
ClpParseExtendedRegularExpressionBranch (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY ParentEntry
    );

REGULAR_EXPRESSION_STATUS
ClpParseExtendedExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY *Entry
    );

REGULAR_EXPRESSION_STATUS
ClpParseBracketExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY Entry
    );

REGULAR_EXPRESSION_STATUS
ClpParseRegularExpressionDuplicationSymbol (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY Entry
    );

REGULAR_EXPRESSION_STATUS
ClpParseRegularExpressionDuplicationCount (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY Entry
    );

REGULAR_EXPRESSION_STATUS
ClpGetRegularExpressionToken (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression
    );

REGULAR_EXPRESSION_STATUS
ClpGetBracketExpressionToken (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression
    );

PREGULAR_EXPRESSION_ENTRY
ClpCreateRegularExpressionEntry (
    REGEX_ENTRY_TYPE Type
    );

VOID
ClpDestroyRegularExpressionEntry (
    PREGULAR_EXPRESSION_ENTRY Entry
    );

BOOL
ClpAppendRegularExpressionString (
    PREGULAR_EXPRESSION_STRING String,
    PSTR Data,
    ULONG Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
regcomp (
    regex_t *RegularExpression,
    const char *Pattern,
    int Flags
    )

/*++

Routine Description:

    This routine compiles a regular expression.

Arguments:

    RegularExpression - Supplies a pointer to the regular expression structure
        where the compiled form will reside on success.

    Pattern - Supplies a pointer to the pattern input string.

    Flags - Supplies a bitfield of flags governing the behavior of the regular
        expression. See some REG_* definitions.

Return Value:

    0 on success.

    Returns a REG_* status code on failure.

--*/

{

    PREGULAR_EXPRESSION CompiledExpression;
    REGULAR_EXPRESSION_STATUS Status;

    RegularExpression->re_nsub = 0;
    RegularExpression->re_data = NULL;
    Status = ClpCompileRegularExpression((PSTR)Pattern,
                                         Flags,
                                         &CompiledExpression);

    if (Status != RegexStatusSuccess) {
        return Status;
    }

    RegularExpression->re_nsub = CompiledExpression->SubexpressionCount;
    RegularExpression->re_data = CompiledExpression;
    return 0;
}

LIBC_API
void
regfree (
    regex_t *RegularExpression
    )

/*++

Routine Description:

    This routine destroys and frees all resources associated with a compiled
    regular expression.

Arguments:

    RegularExpression - Supplies a pointer to the regular expression structure
        to destroy. The caller owns the structure itself, this routine just
        guts all the innards.

Return Value:

    None.

--*/

{

    if (RegularExpression == NULL) {
        return;
    }

    ClpDestroyRegularExpression(RegularExpression->re_data);
    RegularExpression->re_data = NULL;
    RegularExpression->re_nsub = 0;
    return;
}

LIBC_API
size_t
regerror (
    int ErrorCode,
    const regex_t *Expression,
    char *Buffer,
    size_t BufferSize
    )

/*++

Routine Description:

    This routine returns error information about what went wrong trying to
    compile the regular expression.

Arguments:

    ErrorCode - Supplies the error code returned from a regular expression
        token.

    Expression - Supplies an optional pointer to the expression.

    Buffer - Supplies a pointer to a buffer where the error string will be
        returned, always null terminated.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    Returns the number of bytes needed to hold the entire error string,
    including the null terminator. If the return value is greater than the
    supplied size, then the buffer will be truncated and null terminated.

--*/

{

    int BytesPrinted;
    PSTR ErrorString;

    switch (ErrorCode) {
    case 0:
        ErrorString = "No error";
        break;

    case REG_NOMATCH:
        ErrorString = "No match";
        break;

    case REG_BADPAT:
        ErrorString = "Bad pattern";
        break;

    case REG_ECOLLATE:
        ErrorString = "Invalid collating element";
        break;

    case REG_ECTYPE:
        ErrorString = "Invalid character class";
        break;

    case REG_EESCAPE:
        ErrorString = "Dangling escape character";
        break;

    case REG_ESUBREG:
        ErrorString = "Invalid subexpression";
        break;

    case REG_EBRACK:
        ErrorString = "Square bracket imbalance";
        break;

    case REG_EPAREN:
        ErrorString = "Parentheses imbalance";
        break;

    case REG_BADBR:
        ErrorString = "Invalid curly braces";
        break;

    case REG_ERANGE:
        ErrorString = "Invalid range expression";
        break;

    case REG_ESPACE:
        ErrorString = "Out of memory";
        break;

    case REG_BADRPT:
        ErrorString = "Bad repeat expression";
        break;

    default:
        ErrorString = "Unknown error";
        break;
    }

    BytesPrinted = snprintf(Buffer, BufferSize, "%s", ErrorString);
    if (BytesPrinted < 0) {
        BytesPrinted = REGULAR_EXPRESSION_ERROR_STRING_MAX_SIZE;
        if ((Buffer != NULL) && (BufferSize != 0)) {
            *Buffer = '\0';
        }

    } else if (BytesPrinted == BufferSize) {
        Buffer[BufferSize - 1] = '\0';
        BytesPrinted = REGULAR_EXPRESSION_ERROR_STRING_MAX_SIZE;

    } else {

        //
        // Include the null terminator.
        //

        BytesPrinted += 1;
    }

    return BytesPrinted;
}

//
// --------------------------------------------------------- Internal Functions
//

REGULAR_EXPRESSION_STATUS
ClpCompileRegularExpression (
    PSTR Pattern,
    ULONG Flags,
    PREGULAR_EXPRESSION *Expression
    )

/*++

Routine Description:

    This routine compiles a regular expression.

Arguments:

    Pattern - Supplies a pointer to the pattern input string.

    Flags - Supplies a bitfield of flags governing the behavior of the regular
        expression. See some REG_* definitions.

    Expression - Supplies a pointer where a pointer to the regular expression
        will be returned on success. The caller is responsible for calling the
        appropriate destroy routine for this structure.

Return Value:

    Regular expression status code.

--*/

{

    REGULAR_EXPRESSION_LEXER Lexer;
    PREGULAR_EXPRESSION Result;
    REGULAR_EXPRESSION_STATUS Status;

    //
    // Allocate and initialize the basic structures.
    //

    Result = malloc(sizeof(REGULAR_EXPRESSION));
    if (Result == NULL) {
        Status = RegexStatusNoMemory;
        goto CompileRegularExpressionEnd;
    }

    memset(Result, 0, sizeof(REGULAR_EXPRESSION));
    Result->BaseEntry.Type = RegexEntrySubexpression;
    INITIALIZE_LIST_HEAD(&(Result->BaseEntry.ChildList));
    Result->BaseEntry.DuplicateMin = 1;
    Result->BaseEntry.DuplicateMax = 1;
    Result->Flags = Flags;
    memset(&Lexer, 0, sizeof(REGULAR_EXPRESSION_LEXER));
    Lexer.Input = Pattern;
    Lexer.InputSize = strlen(Pattern) + 1;

    //
    // Prime the lexer.
    //

    Status = ClpGetRegularExpressionToken(&Lexer, Result);
    if (Status != RegexStatusSuccess) {
        goto CompileRegularExpressionEnd;
    }

    if ((Flags & REG_EXTENDED) != 0) {
        Status = ClpParseExtendedRegularExpression(&Lexer,
                                                   Result,
                                                   &(Result->BaseEntry));

    } else {
        Status = ClpParseCompleteBasicRegularExpression(&Lexer, Result);
    }

    if (Status != RegexStatusSuccess) {
        goto CompileRegularExpressionEnd;
    }

    //
    // Fail if this isn't the end.
    //

    if (Lexer.Token != '\0') {
        Status = RegexStatusBadPattern;
        goto CompileRegularExpressionEnd;
    }

CompileRegularExpressionEnd:
    if (Status != RegexStatusSuccess) {
        if (Result != NULL) {
            ClpDestroyRegularExpression(Result);
            Result = NULL;
        }
    }

    *Expression = Result;
    return Status;
}

VOID
ClpDestroyRegularExpression (
    PREGULAR_EXPRESSION Expression
    )

/*++

Routine Description:

    This routine destroys a regular expression structure.

Arguments:

    Expression - Supplies a pointer to the regular expression structure to
        destroy.

Return Value:

    None.

--*/

{

    PREGULAR_EXPRESSION_ENTRY Entry;

    if (Expression == NULL) {
        return;
    }

    while (LIST_EMPTY(&(Expression->BaseEntry.ChildList)) == FALSE) {
        Entry = LIST_VALUE(Expression->BaseEntry.ChildList.Next,
                           REGULAR_EXPRESSION_ENTRY,
                           ListEntry);

        LIST_REMOVE(&(Entry->ListEntry));
        ClpDestroyRegularExpressionEntry(Entry);
    }

    free(Expression);
    return;
}

REGULAR_EXPRESSION_STATUS
ClpParseCompleteBasicRegularExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression
    )

/*++

Routine Description:

    This routine compiles a basic regular expression.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer.

    Expression - Supplies the expression to compile on.

Return Value:

    Regular expression status code.

--*/

{

    ULONG EntryFlags;
    REGULAR_EXPRESSION_STATUS Status;

    EntryFlags = 0;

    //
    // Parse an optional left anchor (^).
    //

    if (Lexer->Token == '^') {
        EntryFlags |= REGULAR_EXPRESSION_ANCHORED_LEFT;
        Status = ClpGetRegularExpressionToken(Lexer, Expression);
        if (Status != RegexStatusSuccess) {
            goto ParseBasicRegularExpressionEnd;
        }
    }

    //
    // Parse an expression.
    //

    Status = ClpParseBasicRegularExpression(Lexer,
                                            Expression,
                                            &(Expression->BaseEntry));

    if (Status != RegexStatusSuccess) {
        goto ParseBasicRegularExpressionEnd;
    }

    //
    // Parse an optional right anchor.
    //

    if (Lexer->Token == '$') {
        EntryFlags |= REGULAR_EXPRESSION_ANCHORED_RIGHT;
        Status = ClpGetRegularExpressionToken(Lexer, Expression);
        if (Status != RegexStatusSuccess) {
            goto ParseBasicRegularExpressionEnd;
        }
    }

    Expression->BaseEntry.Flags = EntryFlags;

ParseBasicRegularExpressionEnd:
    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpParseBasicRegularExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY ParentEntry
    )

/*++

Routine Description:

    This routine parses a de-anchored expression for a basic regular
    expression.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer.

    Expression - Supplies the expression to compile on.

    ParentEntry - Supplies a pointer to the regular expression entry this entry
        belongs under.

Return Value:

    Regular expression status code.

--*/

{

    PREGULAR_EXPRESSION_ENTRY Entry;
    PREGULAR_EXPRESSION_ENTRY PreviousEntry;
    BOOL Result;
    REGULAR_EXPRESSION_STATUS Status;

    Entry = NULL;
    PreviousEntry = NULL;

    //
    // Loop parsing simple expressions and duplication symbols.
    //

    while (TRUE) {
        Status = ClpParseSimpleExpression(Lexer, Expression, &Entry);
        if (Status != RegexStatusSuccess) {
            goto ParseRegularExpressionEnd;
        }

        if (Entry == NULL) {

            //
            // Don't allow repeat symbols coming up if there was no entry,
            // which could happen if the first input is a repeat character.
            //

            if ((Lexer->Token == '*') ||
                (Lexer->Token == TOKEN_ESCAPED_OPEN_BRACE)) {

                Status = RegexStatusInvalidRepeat;
            }

            break;
        }

        Status = ClpParseRegularExpressionDuplicationSymbol(Lexer,
                                                            Expression,
                                                            Entry);

        if (Status != RegexStatusSuccess) {
            goto ParseRegularExpressionEnd;
        }

        //
        // Here's a little optimization. If this entry is just an ordinary
        // character and the last one is too, combine them into one entry to
        // avoid a gigantic chain of single character expression entries.
        // Watch out not to do this if either one had duplicate symbols on it
        // (ie "f*a" can't just be combined to "fa").
        //

        if ((Entry->Type == RegexEntryOrdinaryCharacters) &&
            (Entry->DuplicateMin == 1) && (Entry->DuplicateMax == 1) &&
            (PreviousEntry != NULL) &&
            (PreviousEntry->Type == RegexEntryOrdinaryCharacters) &&
            (PreviousEntry->DuplicateMin == 1) &&
            (PreviousEntry->DuplicateMax == 1)) {

            Result = ClpAppendRegularExpressionString(
                                                    &(PreviousEntry->U.String),
                                                    Entry->U.String.Data,
                                                    Entry->U.String.Size);

            if (Result == FALSE) {
                Status = RegexStatusNoMemory;
                goto ParseRegularExpressionEnd;
            }

            ClpDestroyRegularExpressionEntry(Entry);
            Entry = NULL;
            continue;
        }

        //
        // Add this expression entry to the parent.
        //

        Entry->Parent = ParentEntry;
        INSERT_BEFORE(&(Entry->ListEntry), &(ParentEntry->ChildList));
        PreviousEntry = Entry;
        Entry = NULL;
    }

ParseRegularExpressionEnd:
    if (Entry != NULL) {
        ClpDestroyRegularExpressionEntry(Entry);
    }

    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpParseSimpleExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY *Entry
    )

/*++

Routine Description:

    This routine parses a simple expression for a basic regular expression.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer.

    Expression - Supplies the expression to compile on.

    Entry - Supplies a pointer where the regular expression entry will be
        returned on success.

Return Value:

    Regular expression status code.

--*/

{

    CHAR Character;
    PREGULAR_EXPRESSION_ENTRY NewEntry;
    BOOL Result;
    REGULAR_EXPRESSION_STATUS Status;

    Status = RegexStatusSuccess;
    NewEntry = ClpCreateRegularExpressionEntry(RegexEntryInvalid);
    if (NewEntry == NULL) {
        Status = RegexStatusNoMemory;
        goto ParseSimpleExpressionEnd;
    }

    switch (Lexer->Token) {
    case '.':
        NewEntry->Type = RegexEntryAnyCharacter;
        break;

    case '[':
        Status = ClpParseBracketExpression(Lexer, Expression, NewEntry);
        if (Status != RegexStatusSuccess) {
            goto ParseSimpleExpressionEnd;
        }

        break;

    //
    // Parse a subexpression.
    //

    case TOKEN_ESCAPED_OPEN_PARENTHESES:
        NewEntry->Type = RegexEntrySubexpression;

        //
        // Zoom past the open parentheses.
        //

        Status = ClpGetRegularExpressionToken(Lexer, Expression);
        if (Status != RegexStatusSuccess) {
            goto ParseSimpleExpressionEnd;
        }

        //
        // Take a subexpression number and parse a subexpression.
        //

        Expression->SubexpressionCount += 1;
        NewEntry->U.SubexpressionNumber = Expression->SubexpressionCount;
        Status = ClpParseBasicRegularExpression(Lexer,
                                                Expression,
                                                NewEntry);

        if (Status != RegexStatusSuccess) {
            goto ParseSimpleExpressionEnd;
        }

        //
        // Get the close parentheses and swallow it.
        //

        if (Lexer->Token != TOKEN_ESCAPED_CLOSE_PARENTHESES) {
            Status = RegexStatusParenthesesImbalance;
            goto ParseSimpleExpressionEnd;
        }

        break;

    //
    // Parse a quoted character like a normal character.
    //

    case TOKEN_QUOTED_CHARACTER:
        NewEntry->Type = RegexEntryOrdinaryCharacters;

        assert(Lexer->NextInput != 0);

        Character = Lexer->Input[Lexer->NextInput - 1];
        Result = ClpAppendRegularExpressionString(&(NewEntry->U.String),
                                                  &Character,
                                                  1);

        if (Result == FALSE) {
            goto ParseSimpleExpressionEnd;
        }

        break;

    //
    // Parse a back reference.
    //

    case TOKEN_BACK_REFERENCE:
        NewEntry->Type = RegexEntryBackReference;

        assert(Lexer->NextInput != 0);

        Character = Lexer->Input[Lexer->NextInput - 1];

        //
        // The lexer shouldn't be returning the back reference token unless
        // it's valid.
        //

        assert((Character >= '1') && (Character <= '9'));

        NewEntry->U.BackReferenceNumber = Character - '0';
        if (NewEntry->U.BackReferenceNumber > Expression->SubexpressionCount) {
            Status = RegexStatusInvalidSubexpression;
            goto ParseSimpleExpressionEnd;
        }

        break;

    //
    // Some items are not simple expression entries by themselves. As an oddity,
    // one of these characters at the beginning of a basic regular expression
    // is considered a normal character.
    //

    case '*':
    case TOKEN_ESCAPED_CLOSE_PARENTHESES:
    case TOKEN_ESCAPED_OPEN_BRACE:
    case TOKEN_ESCAPED_CLOSE_BRACE:
    case '\0':
        if ((Lexer->Token == '\0') || (Lexer->NextInput != 1)) {
            ClpDestroyRegularExpressionEntry(NewEntry);
            NewEntry = NULL;
            goto ParseSimpleExpressionEnd;
        }

        //
        // Fall through.
        //

    //
    // This must be an ordinary character.
    //

    default:

        assert(Lexer->Token < MAX_UCHAR);

        NewEntry->Type = RegexEntryOrdinaryCharacters;
        Character = Lexer->Token;

        //
        // Watch out for a dollar sign at the end, which is actually an anchor.
        //

        if ((Character == '$') &&
            ((Lexer->NextInput >= Lexer->InputSize) ||
             (Lexer->Input[Lexer->NextInput] == '\0'))) {

            ClpDestroyRegularExpressionEntry(NewEntry);
            NewEntry = NULL;
            goto ParseSimpleExpressionEnd;
        }

        Result = ClpAppendRegularExpressionString(&(NewEntry->U.String),
                                                  &Character,
                                                  1);

        if (Result == FALSE) {
            goto ParseSimpleExpressionEnd;
        }

        break;
    }

    //
    // Swallow the token that has just been dealt with.
    //

    Status = ClpGetRegularExpressionToken(Lexer, Expression);
    if (Status != RegexStatusSuccess) {
        goto ParseSimpleExpressionEnd;
    }

ParseSimpleExpressionEnd:
    if (Status != RegexStatusSuccess) {
        if (NewEntry != NULL) {
            ClpDestroyRegularExpressionEntry(NewEntry);
            NewEntry = NULL;
        }
    }

    *Entry = NewEntry;
    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpParseExtendedRegularExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY ParentEntry
    )

/*++

Routine Description:

    This routine parses an extended regular expression.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer.

    Expression - Supplies the expression to compile on.

    ParentEntry - Supplies a pointer to the parent any new expression entries
        should be placed under.

Return Value:

    Regular expression status code.

--*/

{

    PREGULAR_EXPRESSION_ENTRY Branch;
    PREGULAR_EXPRESSION_ENTRY Child;
    PREGULAR_EXPRESSION_ENTRY Entry;
    REGULAR_EXPRESSION_STATUS Status;

    Entry = NULL;

    //
    // Create the branch umbrella.
    //

    Branch = ClpCreateRegularExpressionEntry(RegexEntryBranch);
    if (Branch == NULL) {
        Status = RegexStatusNoMemory;
        goto ParseExtendedRegularExpressionEnd;
    }

    //
    // Loop creating the branch options.
    //

    while (TRUE) {

        //
        // Create a branch entry to contain the upcoming expression.
        //

        Entry = ClpCreateRegularExpressionEntry(RegexEntryBranchOption);
        if (Entry == NULL) {
            Status = RegexStatusNoMemory;
            goto ParseExtendedRegularExpressionEnd;
        }

        //
        // Parse out the contents of the branch.
        //

        Status = ClpParseExtendedRegularExpressionBranch(Lexer,
                                                         Expression,
                                                         Entry);

        if (Status != RegexStatusSuccess) {
            goto ParseExtendedRegularExpressionEnd;
        }

        //
        // Add this branch to the parent.
        //

        Entry->Parent = Branch;
        INSERT_BEFORE(&(Entry->ListEntry), &(Branch->ChildList));
        Entry = NULL;

        //
        // Stop if there's not more.
        //

        if (Lexer->Token != '|') {
            break;
        }

        //
        // Get past that pipe and go around again.
        //

        Status = ClpGetRegularExpressionToken(Lexer, Expression);
        if (Status != RegexStatusSuccess) {
            goto ParseExtendedRegularExpressionEnd;
        }
    }

    //
    // If there's only one branch option, pull all the children off of the
    // branch option and stick them on the parent. Then the branch and branch
    // option entries can be destroyed.
    //

    assert(LIST_EMPTY(&(Branch->ChildList)) == FALSE);

    if (Branch->ChildList.Next->Next == &(Branch->ChildList)) {
        Entry = LIST_VALUE(Branch->ChildList.Next,
                           REGULAR_EXPRESSION_ENTRY,
                           ListEntry);

        while (LIST_EMPTY(&(Entry->ChildList)) == FALSE) {
            Child = LIST_VALUE(Entry->ChildList.Next,
                               REGULAR_EXPRESSION_ENTRY,
                               ListEntry);

            LIST_REMOVE(&(Child->ListEntry));
            INSERT_BEFORE(&(Child->ListEntry), &(ParentEntry->ChildList));
            Child->Parent = ParentEntry;
        }

        ClpDestroyRegularExpressionEntry(Branch);
        Entry = NULL;
        Branch = NULL;

    //
    // There are multiple branch options, so put the branch entry on the parent.
    //

    } else {
        Branch->Parent = ParentEntry;
        INSERT_BEFORE(&(Branch->ListEntry), &(ParentEntry->ChildList));
    }

ParseExtendedRegularExpressionEnd:
    if (Status != RegexStatusSuccess) {
        if (Branch != NULL) {
            ClpDestroyRegularExpressionEntry(Branch);
        }
    }

    if (Entry != NULL) {
        ClpDestroyRegularExpressionEntry(Entry);
    }

    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpParseExtendedRegularExpressionBranch (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY ParentEntry
    )

/*++

Routine Description:

    This routine parses a single extended regular expression branch (ie the
    expression "a|b|c" has three branches, this routine parses just one
    of them).

Arguments:

    Lexer - Supplies a pointer to the initialized lexer.

    Expression - Supplies the expression to compile on.

    ParentEntry - Supplies a pointer to the parent any new expression entries
        should be placed under.

Return Value:

    Regular expression status code.

--*/

{

    PREGULAR_EXPRESSION_ENTRY Entry;
    PREGULAR_EXPRESSION_ENTRY PreviousEntry;
    BOOL Result;
    REGULAR_EXPRESSION_STATUS Status;

    Entry = NULL;
    PreviousEntry = NULL;

    //
    // Loop parsing simple expressions and duplication symbols.
    //

    while (TRUE) {
        Status = ClpParseExtendedExpression(Lexer, Expression, &Entry);
        if (Status != RegexStatusSuccess) {
            goto ParseRegularExpressionEnd;
        }

        if (Entry == NULL) {

            //
            // Don't allow repeat symbols coming up if there was no entry,
            // which could happen if the first input is a repeat character.
            //

            if ((Lexer->Token == '*') || (Lexer->Token == '+') ||
                (Lexer->Token == '?') || (Lexer->Token == '{')) {

                Status = RegexStatusInvalidRepeat;
            }

            break;
        }

        Status = ClpParseRegularExpressionDuplicationSymbol(Lexer,
                                                            Expression,
                                                            Entry);

        if (Status != RegexStatusSuccess) {
            goto ParseRegularExpressionEnd;
        }

        //
        // Here's a little optimization. If this entry is just an ordinary
        // character and the last one is too, combine them into one entry to
        // avoid a gigantic chain of single character expression entries.
        // Watch out not to do this if either one had duplicate symbols on it
        // (ie "f*a" can't just be combined to "fa").
        //

        if ((Entry->Type == RegexEntryOrdinaryCharacters) &&
            (Entry->DuplicateMin == 1) && (Entry->DuplicateMax == 1) &&
            (PreviousEntry != NULL) &&
            (PreviousEntry->Type == RegexEntryOrdinaryCharacters) &&
            (PreviousEntry->DuplicateMin == 1) &&
            (PreviousEntry->DuplicateMax == 1)) {

            Result = ClpAppendRegularExpressionString(
                                                    &(PreviousEntry->U.String),
                                                    Entry->U.String.Data,
                                                    Entry->U.String.Size);

            if (Result == FALSE) {
                Status = RegexStatusNoMemory;
                goto ParseRegularExpressionEnd;
            }

            ClpDestroyRegularExpressionEntry(Entry);
            Entry = NULL;
            continue;
        }

        //
        // Add this expression entry to the parent.
        //

        Entry->Parent = ParentEntry;
        INSERT_BEFORE(&(Entry->ListEntry), &(ParentEntry->ChildList));
        PreviousEntry = Entry;
        Entry = NULL;
    }

ParseRegularExpressionEnd:
    if (Entry != NULL) {
        ClpDestroyRegularExpressionEntry(Entry);
    }

    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpParseExtendedExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY *Entry
    )

/*++

Routine Description:

    This routine parses a base expression ("ERE_expression" as described in the
    specs) for an extended regular expression.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer.

    Expression - Supplies the expression to compile on.

    Entry - Supplies a pointer where the regular expression entry will be
        returned on success.

Return Value:

    Regular expression status code.

--*/

{

    CHAR Character;
    PREGULAR_EXPRESSION_ENTRY NewEntry;
    BOOL Result;
    REGULAR_EXPRESSION_STATUS Status;

    Status = RegexStatusSuccess;
    NewEntry = ClpCreateRegularExpressionEntry(RegexEntryInvalid);
    if (NewEntry == NULL) {
        Status = RegexStatusNoMemory;
        goto ParseExtendedExpressionEnd;
    }

    switch (Lexer->Token) {
    case '.':
        NewEntry->Type = RegexEntryAnyCharacter;
        break;

    case '^':
        NewEntry->Type = RegexEntryStringBegin;
        break;

    case '$':
        NewEntry->Type = RegexEntryStringEnd;
        break;

    case '[':
        Status = ClpParseBracketExpression(Lexer, Expression, NewEntry);
        if (Status != RegexStatusSuccess) {
            goto ParseExtendedExpressionEnd;
        }

        break;

    //
    // Parse a subexpression.
    //

    case '(':
        NewEntry->Type = RegexEntrySubexpression;

        //
        // Zoom past the open parentheses.
        //

        Status = ClpGetRegularExpressionToken(Lexer, Expression);
        if (Status != RegexStatusSuccess) {
            goto ParseExtendedExpressionEnd;
        }

        //
        // Take a subexpression number and parse the subexpression.
        //

        Lexer->ActiveSubexpressionCount += 1;
        Expression->SubexpressionCount += 1;
        NewEntry->U.SubexpressionNumber = Expression->SubexpressionCount;
        Status = ClpParseExtendedRegularExpression(Lexer,
                                                   Expression,
                                                   NewEntry);

        if (Status != RegexStatusSuccess) {
            goto ParseExtendedExpressionEnd;
        }

        //
        // Verify the close parentheses.
        //

        if (Lexer->Token != ')') {
            Status = RegexStatusParenthesesImbalance;
            goto ParseExtendedExpressionEnd;
        }

        Lexer->ActiveSubexpressionCount -= 1;
        break;

    //
    // Parse a quoted character like a normal character.
    //

    case TOKEN_QUOTED_CHARACTER:
        NewEntry->Type = RegexEntryOrdinaryCharacters;

        assert(Lexer->NextInput != 0);

        Character = Lexer->Input[Lexer->NextInput - 1];
        Result = ClpAppendRegularExpressionString(&(NewEntry->U.String),
                                                  &Character,
                                                  1);

        if (Result == FALSE) {
            goto ParseExtendedExpressionEnd;
        }

        break;

    //
    // Parse a back reference.
    //

    case TOKEN_BACK_REFERENCE:
        NewEntry->Type = RegexEntryBackReference;

        assert(Lexer->NextInput != 0);

        Character = Lexer->Input[Lexer->NextInput - 1];

        //
        // The lexer shouldn't be returning the back reference token unless
        // it's valid.
        //

        assert((Character >= '0') && (Character <= '9'));

        NewEntry->U.BackReferenceNumber = Character - '0';
        if (NewEntry->U.BackReferenceNumber > Expression->SubexpressionCount) {
            Status = RegexStatusInvalidSubexpression;
            goto ParseExtendedExpressionEnd;
        }

        break;

    //
    // Some items are not simple expression entries by themselves.
    //

    case '*':
    case '+':
    case '?':
    case '{':
    case '|':
    case '\0':
        ClpDestroyRegularExpressionEntry(NewEntry);
        NewEntry = NULL;
        goto ParseExtendedExpressionEnd;

    //
    // This must be an ordinary character.
    //

    default:

        assert(Lexer->Token < MAX_UCHAR);

        NewEntry->Type = RegexEntryOrdinaryCharacters;
        Character = Lexer->Token;

        //
        // Watch out for a close parentheses if there are active open ones.
        //

        if ((Character == ')') && (Lexer->ActiveSubexpressionCount != 0)) {
            ClpDestroyRegularExpressionEntry(NewEntry);
            NewEntry = NULL;
            goto ParseExtendedExpressionEnd;
        }

        Result = ClpAppendRegularExpressionString(&(NewEntry->U.String),
                                                  &Character,
                                                  1);

        if (Result == FALSE) {
            goto ParseExtendedExpressionEnd;
        }

        break;
    }

    //
    // Swallow the token that has just been dealt with.
    //

    Status = ClpGetRegularExpressionToken(Lexer, Expression);
    if (Status != RegexStatusSuccess) {
        goto ParseExtendedExpressionEnd;
    }

ParseExtendedExpressionEnd:
    if (Status != RegexStatusSuccess) {
        if (NewEntry != NULL) {
            ClpDestroyRegularExpressionEntry(NewEntry);
            NewEntry = NULL;
        }
    }

    *Entry = NewEntry;
    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpParseBracketExpression (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY Entry
    )

/*++

Routine Description:

    This routine parses a bracket expression, which expresses a set of
    characters or collating elements that satisfy the expression.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer.

    Expression - Supplies the expression to compile on.

    Entry - Supplies a pointer to the regular expression bracket entry.

Return Value:

    Regular expression status code.

--*/

{

    PREGULAR_BRACKET_ENTRY BracketEntry;
    BRACKET_EXPRESSION_TYPE BracketType;
    CHAR PreviousCharacter;
    PREGULAR_EXPRESSION_STRING RegularCharacters;
    BOOL Result;
    REGULAR_EXPRESSION_STATUS Status;
    BOOL Stop;
    PSTR String;

    assert(Lexer->Token == '[');
    assert(Lexer->NextInput != 0);

    //
    // See if this is a start-of-word or end-of-word, and not actually a
    // bracket expression.
    //

    String = Lexer->Input + Lexer->NextInput - 1;
    if (strncmp(String, "[[:<:]]", 7) == 0) {
        Entry->Type = RegexEntryStartOfWord;
        Lexer->Input += 6;
        Status = RegexStatusSuccess;
        goto ParseBracketExpressionEnd;

    } else if (strncmp(String, "[[:>:]]", 7) == 0) {
        Entry->Type = RegexEntryEndOfWord;
        Lexer->Input += 6;
        Status = RegexStatusSuccess;
        goto ParseBracketExpressionEnd;
    }

    Entry->Type = RegexEntryBracketExpression;
    INITIALIZE_LIST_HEAD(&(Entry->U.BracketExpression.EntryList));

    //
    // Swallow the open bracket.
    //

    Status = ClpGetBracketExpressionToken(Lexer, Expression);
    if (Status != RegexStatusSuccess) {
        goto ParseBracketExpressionEnd;
    }

    RegularCharacters = &(Entry->U.BracketExpression.RegularCharacters);

    //
    // A circumflex negates the whole expression (ie matches on characters
    // *not* in this set.
    //

    if (Lexer->Token == '^') {
        Entry->Flags |= REGULAR_EXPRESSION_NEGATED;
        Status = ClpGetBracketExpressionToken(Lexer, Expression);
        if (Status != RegexStatusSuccess) {
            goto ParseBracketExpressionEnd;
        }
    }

    //
    // A closing bracket or minus here is treated as an ordinary character.
    //

    if ((Lexer->Token == ']') || (Lexer->Token == '-')) {
        Result = ClpAppendRegularExpressionString(RegularCharacters,
                                                  (PSTR)&(Lexer->Token),
                                                  1);

        if (Result == FALSE) {
            Status = RegexStatusNoMemory;
            goto ParseBracketExpressionEnd;
        }

        Status = ClpGetBracketExpressionToken(Lexer, Expression);
        if (Status != RegexStatusSuccess) {
            goto ParseBracketExpressionEnd;
        }
    }

    //
    // Loop adding characters to this bracket expression.
    //

    PreviousCharacter = 0;
    Stop = FALSE;
    while (TRUE) {
        switch (Lexer->Token) {
        case TOKEN_OPEN_COLON:
            String = Lexer->Input + Lexer->NextInput;
            BracketType = BracketExpressionInvalid;
            if (strncmp(String, "alnum", 5) == 0) {
                BracketType = BracketExpressionCharacterClassAlphanumeric;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "alpha", 5) == 0) {
                BracketType = BracketExpressionCharacterClassAlphabetic;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "blank", 5) == 0) {
                BracketType = BracketExpressionCharacterClassBlank;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "cntrl", 5) == 0) {
                BracketType = BracketExpressionCharacterClassControl;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "digit", 5) == 0) {
                BracketType = BracketExpressionCharacterClassDigit;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "graph", 5) == 0) {
                BracketType = BracketExpressionCharacterClassGraph;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "lower", 5) == 0) {
                BracketType = BracketExpressionCharacterClassLowercase;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "print", 5) == 0) {
                BracketType = BracketExpressionCharacterClassPrintable;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "punct", 5) == 0) {
                BracketType = BracketExpressionCharacterClassPunctuation;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "space", 5) == 0) {
                BracketType = BracketExpressionCharacterClassSpace;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "upper", 5) == 0) {
                BracketType = BracketExpressionCharacterClassUppercase;
                Lexer->NextInput += 5;

            } else if (strncmp(String, "xdigit", 6) == 0) {
                BracketType = BracketExpressionCharacterClassHexDigit;
                Lexer->NextInput += 6;

            } else if (strncmp(String, "name", 4) == 0) {
                BracketType = BracketExpressionCharacterClassName;
                Lexer->NextInput += 5;
            }

            if (BracketType == BracketExpressionInvalid) {
                Status = RegexStatusBadCharacterClass;
                goto ParseBracketExpressionEnd;
            }

            BracketEntry = malloc(sizeof(REGULAR_BRACKET_ENTRY));
            if (BracketEntry == NULL) {
                Status = RegexStatusNoMemory;
                goto ParseBracketExpressionEnd;
            }

            memset(BracketEntry, 0, sizeof(REGULAR_BRACKET_ENTRY));
            BracketEntry->Type = BracketType;
            INSERT_BEFORE(&(BracketEntry->ListEntry),
                          &(Entry->U.BracketExpression.EntryList));

            //
            // Swallow up the colon close.
            //

            Status = ClpGetBracketExpressionToken(Lexer, Expression);
            if (Status != RegexStatusSuccess) {
                goto ParseBracketExpressionEnd;
            }

            if (Lexer->Token != ':') {
                Status = RegexStatusBadPattern;
                goto ParseBracketExpressionEnd;
            }

            Status = ClpGetBracketExpressionToken(Lexer, Expression);
            if (Status != RegexStatusSuccess) {
                goto ParseBracketExpressionEnd;
            }

            if (Lexer->Token != ']') {
                Status = RegexStatusBadPattern;
                goto ParseBracketExpressionEnd;
            }

            break;

        case TOKEN_OPEN_DOT:
            printf("regex: Collating element support not implemented!\n");

            //
            // Spin until the dot close.
            //

            while ((PreviousCharacter != '.') || (Lexer->Token != ']')) {
                if (Lexer->Token == '\0') {
                    Status = RegexStatusBracketImbalance;
                    goto ParseBracketExpressionEnd;
                }

                PreviousCharacter = (UCHAR)Lexer->Token;
                Status = ClpGetBracketExpressionToken(Lexer, Expression);
                if (Status != RegexStatusSuccess) {
                    goto ParseBracketExpressionEnd;
                }
            }

            break;

        case TOKEN_OPEN_EQUAL:
            printf("regex: Equivalence class support not implemented!\n");

            //
            // Spin until the equal close.
            //

            while ((PreviousCharacter != '=') || (Lexer->Token != ']')) {
                if (Lexer->Token == '\0') {
                    Status = RegexStatusBracketImbalance;
                    goto ParseBracketExpressionEnd;
                }

                PreviousCharacter = (UCHAR)Lexer->Token;
                Status = ClpGetBracketExpressionToken(Lexer, Expression);
                if (Status != RegexStatusSuccess) {
                    goto ParseBracketExpressionEnd;
                }
            }

            break;

        case ']':
            Stop = TRUE;
            break;

        case '\0':
            Status = RegexStatusBracketImbalance;
            goto ParseBracketExpressionEnd;

        default:
            if (Lexer->Token > MAX_UCHAR) {
                Status = RegexStatusBadPattern;
                goto ParseBracketExpressionEnd;
            }

            //
            // If the previous character was a -, this is actually a range.
            // Pull the dash and first character off of the regular characters
            // list, and create a range.
            //

            if (PreviousCharacter == '-') {
                BracketEntry = malloc(sizeof(REGULAR_BRACKET_ENTRY));
                if (BracketEntry == NULL) {
                    Status = RegexStatusNoMemory;
                    goto ParseBracketExpressionEnd;
                }

                memset(BracketEntry, 0, sizeof(REGULAR_BRACKET_ENTRY));
                BracketEntry->Type = BracketExpressionRange;

                assert(RegularCharacters->Size >= 2);

                BracketEntry->U.Range.Minimum =
                          RegularCharacters->Data[RegularCharacters->Size - 2];

                RegularCharacters->Size -= 2;
                BracketEntry->U.Range.Maximum = Lexer->Token;
                INSERT_BEFORE(&(BracketEntry->ListEntry),
                              &(Entry->U.BracketExpression.EntryList));

            //
            // This is a regular character and not part of a range (or at least
            // the beginning character of the range. Add it to the regular
            // character string.
            //

            } else {
                Result = ClpAppendRegularExpressionString(RegularCharacters,
                                                          (PSTR)&(Lexer->Token),
                                                          1);

                if (Result == FALSE) {
                    Status = RegexStatusNoMemory;
                    goto ParseBracketExpressionEnd;
                }
            }

            break;
        }

        if (Stop != FALSE) {
            break;
        }

        if (Lexer->Token < MAX_UCHAR) {
            PreviousCharacter = (CHAR)Lexer->Token;
        }

        Status = ClpGetBracketExpressionToken(Lexer, Expression);
        if (Status != RegexStatusSuccess) {
            goto ParseBracketExpressionEnd;
        }
    }

ParseBracketExpressionEnd:
    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpParseRegularExpressionDuplicationSymbol (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY Entry
    )

/*++

Routine Description:

    This routine parses any optional duplication symbols for the regular
    expression.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer.

    Expression - Supplies the expression to compile on.

    Entry - Supplies a pointer where the regular expression entry will be
        returned on success.

Return Value:

    Regular expression status code.

--*/

{

    REGULAR_EXPRESSION_STATUS Status;
    BOOL SwallowToken;

    Status = RegexStatusSuccess;
    SwallowToken = FALSE;
    while (TRUE) {

        //
        // Stars are pretty easy, they're just zero or more occurrences.
        //

        if (Lexer->Token == '*') {
            Entry->DuplicateMin = 0;
            Entry->DuplicateMax = -1;
            SwallowToken = TRUE;

        //
        // Extended regular expressions are fancy in that they have a couple
        // extra duplication options.
        //

        } else if ((Expression->Flags & REG_EXTENDED) != 0) {

            //
            // Handle a +, which is one or more occurrences.
            //

            if (Lexer->Token == '+') {
                if (Entry->DuplicateMin > 1) {
                    Entry->DuplicateMin = 1;
                }

                Entry->DuplicateMax = -1;
                SwallowToken = TRUE;

            //
            // Handle a ?, which is zero or one occurrences.
            //

            } else if (Lexer->Token == '?') {
                Entry->DuplicateMin = 0;
                Entry->DuplicateMax = 1;
                SwallowToken = TRUE;

            //
            // Handle an opening curly brace, which just like the escaped curly
            // brace in basic regular expressions in that it takes the form {M},
            // {M,}, or {M,N} specifying the minimum and maximum number of
            // occurrences (inclusive).
            //

            } else if (Lexer->Token == '{') {
                Status = ClpParseRegularExpressionDuplicationCount(Lexer,
                                                                   Expression,
                                                                   Entry);

                if (Status != RegexStatusSuccess) {
                    goto ParseRegularExpressionDuplicationSymbolEnd;
                }

            } else {
                Status = RegexStatusSuccess;
                goto ParseRegularExpressionDuplicationSymbolEnd;
            }

        //
        // Basic expressions only. The only other duplication basic expressions
        // can do is backquote braces.
        //

        } else {
            if (Lexer->Token != TOKEN_ESCAPED_OPEN_BRACE) {
                Status = RegexStatusSuccess;
                goto ParseRegularExpressionDuplicationSymbolEnd;
            }

            //
            // Parse the duplication range.
            //

            Status = ClpParseRegularExpressionDuplicationCount(Lexer,
                                                               Expression,
                                                               Entry);

            if (Status != RegexStatusSuccess) {
                goto ParseRegularExpressionDuplicationSymbolEnd;
            }
        }

        if (SwallowToken != FALSE) {
            SwallowToken = FALSE;
            Status = ClpGetRegularExpressionToken(Lexer, Expression);
            if (Status != RegexStatusSuccess) {
                goto ParseRegularExpressionDuplicationSymbolEnd;
            }
        }
    }

ParseRegularExpressionDuplicationSymbolEnd:
    if (SwallowToken != FALSE) {

        assert(Status == RegexStatusSuccess);

        Status = ClpGetRegularExpressionToken(Lexer, Expression);
    }

    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpParseRegularExpressionDuplicationCount (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression,
    PREGULAR_EXPRESSION_ENTRY Entry
    )

/*++

Routine Description:

    This routine parses a duplication count, which takes the form "M", "M,", or
    "M,N", followed by either a close curly brace or close escaped curly brace
    depending on whether extended mode is on or not.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer.

    Expression - Supplies the expression to compile on.

    Entry - Supplies a pointer where the regular expression entry will be
        returned on success.

Return Value:

    Regular expression status code.

--*/

{

    PSTR AfterScan;
    PSTR BeforeScan;
    LONG Begin;
    CHAR Character;
    LONG End;
    REGULAR_EXPRESSION_STATUS Status;

    End = Entry->DuplicateMax;

    //
    // Get the first number.
    //

    BeforeScan = Lexer->Input + Lexer->NextInput;
    Begin = strtol(BeforeScan, &AfterScan, 10);
    if ((Begin < 0) || (AfterScan == BeforeScan)) {
        Status = RegexStatusInvalidBraces;
        goto ClpParseRegularExpressionDuplicationCountEnd;
    }

    End = Begin;
    Lexer->NextInput += (UINTN)AfterScan - (UINTN)BeforeScan;

    assert(Lexer->NextInput <= Lexer->InputSize);

    while ((Lexer->NextInput < Lexer->InputSize) &&
           (isspace(Lexer->Input[Lexer->NextInput]))) {

        Lexer->NextInput += 1;
    }

    if (Lexer->NextInput == Lexer->InputSize) {
        Status = RegexStatusInvalidBraces;
        goto ClpParseRegularExpressionDuplicationCountEnd;
    }

    //
    // If there's a comma, swallow that and get an optional second number.
    //

    if (Lexer->Input[Lexer->NextInput] == ',') {
        Lexer->NextInput += 1;
        while ((Lexer->NextInput < Lexer->InputSize) &&
               (isspace(Lexer->Input[Lexer->NextInput]))) {

            Lexer->NextInput += 1;
        }

        if (Lexer->NextInput == Lexer->InputSize) {
            Status = RegexStatusInvalidBraces;
            goto ClpParseRegularExpressionDuplicationCountEnd;
        }

        Character = Lexer->Input[Lexer->NextInput];
        if ((Character >= '1') && (Character <= '9')) {
            BeforeScan = Lexer->Input + Lexer->NextInput;
            End = strtol(BeforeScan, &AfterScan, 10);
            if ((End < 0) || (AfterScan == BeforeScan)) {
                Status = RegexStatusInvalidBraces;
                goto ClpParseRegularExpressionDuplicationCountEnd;
            }

            Lexer->NextInput += (UINTN)AfterScan - (UINTN)BeforeScan;

            assert(Lexer->NextInput <= Lexer->InputSize);

            while ((Lexer->NextInput < Lexer->InputSize) &&
                   (isspace(Lexer->Input[Lexer->NextInput]))) {

                Lexer->NextInput += 1;
            }

            if (Lexer->NextInput == Lexer->InputSize) {
                Status = RegexStatusInvalidBraces;
                goto ClpParseRegularExpressionDuplicationCountEnd;
            }

        //
        // In the {M,} form, the pattern matches at least M times with no
        // upper limit.
        //

        } else {
            End = -1;
        }
    }

    //
    // Now, get the next token and verify that it's a closing brace.
    //

    Status = ClpGetRegularExpressionToken(Lexer, Expression);
    if (Status != RegexStatusSuccess) {
        goto ClpParseRegularExpressionDuplicationCountEnd;
    }

    if ((Expression->Flags & REG_EXTENDED) != 0) {
        if (Lexer->Token != '}') {
            Status = RegexStatusInvalidBraces;
            goto ClpParseRegularExpressionDuplicationCountEnd;
        }

    } else {
        if (Lexer->Token != TOKEN_ESCAPED_CLOSE_BRACE) {
            Status = RegexStatusInvalidBraces;
            goto ClpParseRegularExpressionDuplicationCountEnd;
        }
    }

    //
    // Swallow that ending token.
    //

    Status = ClpGetRegularExpressionToken(Lexer, Expression);
    if (Status != RegexStatusSuccess) {
        goto ClpParseRegularExpressionDuplicationCountEnd;
    }

    //
    // Watch out for a backwards range.
    //

    if ((End != -1) && (End < Begin)) {
        Status = RegexStatusInvalidBraces;
        goto ClpParseRegularExpressionDuplicationCountEnd;
    }

    Status = RegexStatusSuccess;

ClpParseRegularExpressionDuplicationCountEnd:
    if (Status == RegexStatusSuccess) {
        if ((Begin < Entry->DuplicateMin) || (Entry->DuplicateMin == 1)) {
            Entry->DuplicateMin = Begin;
        }

        if ((Entry->DuplicateMax != -1) && (End > Entry->DuplicateMax)) {
            Entry->DuplicateMax = End;
        }
    }

    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpGetRegularExpressionToken (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression
    )

/*++

Routine Description:

    This routine gets the next token out of the regular expression input.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer. The token will be
        placed in the input lexer state.

    Expression - Supplies the expression to compile on.

Return Value:

    Regular expression status code.

--*/

{

    CHAR Character;
    REGULAR_EXPRESSION_STATUS Status;

    Status = RegexStatusSuccess;

    //
    // Watch out for the end.
    //

    if ((Lexer->NextInput == Lexer->InputSize) ||
        (Lexer->Input[Lexer->NextInput] == '\0')) {

        Lexer->Token = '\0';
        goto GetRegularExpressionTokenEnd;
    }

    Character = Lexer->Input[Lexer->NextInput];
    Lexer->NextInput += 1;

    //
    // If it's just a regular character, send it on.
    //

    if (Character != '\\') {
        Lexer->Token = (UCHAR)Character;
        goto GetRegularExpressionTokenEnd;
    }

    //
    // If this was the end, that's a dangling escape.
    //

    if ((Lexer->NextInput == Lexer->InputSize) ||
        (Lexer->Input[Lexer->NextInput] == '\0')) {

        Status = RegexStatusTrailingEscape;
        goto GetRegularExpressionTokenEnd;
    }

    Character = Lexer->Input[Lexer->NextInput];
    Lexer->NextInput += 1;

    //
    // Back references work in both regular and extended.
    //

    if ((Character >= '1') && (Character <= '9')) {
        Lexer->Token = TOKEN_BACK_REFERENCE;
        goto GetRegularExpressionTokenEnd;

    //
    // Some quoted characters are common to both basic and extended regular
    // expressions.
    //

    } else if ((Character == '^') || (Character == '.') || (Character == '*') ||
               (Character == '[') || (Character == '$') || (Character == ']') ||
               (Character == '\\')) {

        Lexer->Token = TOKEN_QUOTED_CHARACTER;
        goto GetRegularExpressionTokenEnd;
    }

    //
    // Check for tokens specific to extended regular expressions.
    //

    if ((Expression->Flags & REG_EXTENDED) != 0) {

        //
        // Some quoted characters are only quoted in extended regular
        // expressions.
        //

        if ((Character == '(') || (Character == ')') || (Character == '|') ||
            (Character == '+') || (Character == '?') || (Character == '{') ||
            (Character == '}')) {

            Lexer->Token = TOKEN_QUOTED_CHARACTER;
            goto GetRegularExpressionTokenEnd;
        }

    //
    // Basic regular expression syntax only.
    //

    } else {
        if (Character == '(') {
            Lexer->Token = TOKEN_ESCAPED_OPEN_PARENTHESES;
            goto GetRegularExpressionTokenEnd;

        } else if (Character == ')') {
            Lexer->Token = TOKEN_ESCAPED_CLOSE_PARENTHESES;
            goto GetRegularExpressionTokenEnd;

        } else if (Character == '{') {
            Lexer->Token = TOKEN_ESCAPED_OPEN_BRACE;
            goto GetRegularExpressionTokenEnd;

        } else if (Character == '}') {
            Lexer->Token = TOKEN_ESCAPED_CLOSE_BRACE;
            goto GetRegularExpressionTokenEnd;
        }
    }

    //
    // If it's quoting a special character, back up and send the backslash
    // through directly.
    //

    if ((Character == '0') || (Character == 'n') || (Character == 'r') ||
        (Character == 'f') || (Character == 't') || (Character == 'v') ||
        (Character == 'b') || (Character == 'a')) {

        Lexer->NextInput -= 1;
        Lexer->Token = '\\';
        goto GetRegularExpressionTokenEnd;
    }

    //
    // This backslash doesn't seem to be quoting anything. Just serve up the
    // next character.
    //

    Lexer->Token = (CHAR)Character;

GetRegularExpressionTokenEnd:
    return Status;
}

REGULAR_EXPRESSION_STATUS
ClpGetBracketExpressionToken (
    PREGULAR_EXPRESSION_LEXER Lexer,
    PREGULAR_EXPRESSION Expression
    )

/*++

Routine Description:

    This routine gets the next token out of the regular expression input.

Arguments:

    Lexer - Supplies a pointer to the initialized lexer. The token will be
        placed in the input lexer state.

    Expression - Supplies the expression to compile on.

Return Value:

    Regular expression status code.

--*/

{

    CHAR Character;
    REGULAR_EXPRESSION_STATUS Status;

    Status = RegexStatusSuccess;

    //
    // Watch out for the end.
    //

    if ((Lexer->NextInput == Lexer->InputSize) ||
        (Lexer->Input[Lexer->NextInput] == '\0')) {

        Lexer->Token = '\0';
        goto GetBracketExpressionTokenEnd;
    }

    Character = Lexer->Input[Lexer->NextInput];
    Lexer->NextInput += 1;

    //
    // If it's just a regular character, send it on.
    //

    if (Character != '[') {
        Lexer->Token = (UCHAR)Character;
        goto GetBracketExpressionTokenEnd;
    }

    //
    // If this was the end, that's a dangling open bracket.
    //

    if ((Lexer->NextInput == Lexer->InputSize) ||
        (Lexer->Input[Lexer->NextInput] == '\0')) {

        Status = RegexStatusBracketImbalance;
        goto GetBracketExpressionTokenEnd;
    }

    Character = Lexer->Input[Lexer->NextInput];
    if (Character == '=') {
        Lexer->Token = TOKEN_OPEN_EQUAL;
        Lexer->NextInput += 1;

    } else if (Character == '.') {
        Lexer->Token = TOKEN_OPEN_DOT;
        Lexer->NextInput += 1;

    } else if (Character == ':') {
        Lexer->Token = TOKEN_OPEN_COLON;
        Lexer->NextInput += 1;

    } else {

        //
        // This is just a plain Jane open bracket.
        //

        Lexer->Token = '[';
    }

GetBracketExpressionTokenEnd:
    return Status;
}

PREGULAR_EXPRESSION_ENTRY
ClpCreateRegularExpressionEntry (
    REGEX_ENTRY_TYPE Type
    )

/*++

Routine Description:

    This routine creates a new regular expression entry of the given type.

Arguments:

    Type - Supplies the type of entry to create.

Return Value:

    Returns a pointer to the created entry on success.

    NULL on allocation failure.

--*/

{

    PREGULAR_EXPRESSION_ENTRY Entry;

    Entry = malloc(sizeof(REGULAR_EXPRESSION_ENTRY));
    if (Entry == NULL) {
        return NULL;
    }

    memset(Entry, 0, sizeof(REGULAR_EXPRESSION_ENTRY));
    Entry->Type = Type;
    Entry->DuplicateMin = 1;
    Entry->DuplicateMax = 1;
    INITIALIZE_LIST_HEAD(&(Entry->ChildList));
    return Entry;
}

VOID
ClpDestroyRegularExpressionEntry (
    PREGULAR_EXPRESSION_ENTRY Entry
    )

/*++

Routine Description:

    This routine destroys a regular expression entry and recursively all of its
    children. It assumes the entry has already been removed from any parent
    list.

Arguments:

    Entry - Supplies a pointer to the entry to destroy.

Return Value:

    None.

--*/

{

    PREGULAR_BRACKET_ENTRY BracketEntry;
    PREGULAR_EXPRESSION_ENTRY Subentry;

    switch (Entry->Type) {
    case RegexEntryOrdinaryCharacters:
        if (Entry->U.String.Data != NULL) {
            free(Entry->U.String.Data);
        }

        break;

    case RegexEntryInvalid:
    case RegexEntryAnyCharacter:
    case RegexEntryBackReference:
    case RegexEntryStringBegin:
    case RegexEntryStringEnd:
    case RegexEntryBranch:
    case RegexEntryBranchOption:
    case RegexEntrySubexpression:
    case RegexEntryStartOfWord:
    case RegexEntryEndOfWord:
        break;

    case RegexEntryBracketExpression:
        if (Entry->U.BracketExpression.RegularCharacters.Data != NULL) {
            free(Entry->U.BracketExpression.RegularCharacters.Data);
        }

        while (LIST_EMPTY(&(Entry->U.BracketExpression.EntryList)) == FALSE) {
            BracketEntry = LIST_VALUE(Entry->U.BracketExpression.EntryList.Next,
                                      REGULAR_BRACKET_ENTRY,
                                      ListEntry);

            LIST_REMOVE(&(BracketEntry->ListEntry));
            free(BracketEntry);
        }

        break;

    default:

        assert(FALSE);

        return;
    }

    while (LIST_EMPTY(&(Entry->ChildList)) == FALSE) {
        Subentry = LIST_VALUE(Entry->ChildList.Next,
                              REGULAR_EXPRESSION_ENTRY,
                              ListEntry);

        LIST_REMOVE(&(Subentry->ListEntry));
        ClpDestroyRegularExpressionEntry(Subentry);
    }

    free(Entry);
    return;
}

BOOL
ClpAppendRegularExpressionString (
    PREGULAR_EXPRESSION_STRING String,
    PSTR Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine appends data onto a regular expression string.

Arguments:

    String - Supplies a pointer to the string to append to.

    Data - Supplies a pointer to the data to append.

    Size - Supplies the number of bytes in the data buffer.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    PVOID NewBuffer;
    ULONG NewCapacity;

    //
    // Reallocate the string if needed.
    //

    if (String->Size + Size > String->Capacity) {
        NewCapacity = String->Capacity;
        if (NewCapacity == 0) {
            NewCapacity = REGULAR_EXPRESSION_INITIAL_STRING_SIZE;
        }

        while (String->Size + Size > NewCapacity) {
            NewCapacity *= 2;
        }

        NewBuffer = realloc(String->Data, NewCapacity);
        if (NewBuffer == NULL) {
            String->Capacity = 0;
            return FALSE;
        }

        String->Data = NewBuffer;
        String->Capacity = NewCapacity;
    }

    //
    // Copy the new bytes in.
    //

    memcpy(String->Data + String->Size, Data, Size);
    String->Size += Size;
    return TRUE;
}

