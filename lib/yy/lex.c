/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lex.c

Abstract:

    This module implements a basic lexer. This lexer understands regular
    expressions to a certain extent, but is simplified in that it will not
    backtrack. Backtracking is normally not needed in language specifications.

Author:

    Evan Green 9-Oct-2015

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include "yyp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
YypMatchExpression (
    PLEXER Lexer,
    PSTR *Expressions,
    PULONG Position,
    PULONG ExpressionIndex
    );

BOOL
YypMatchSubexpression (
    PLEXER Lexer,
    PULONG Position,
    PSTR *ExpressionPointer
    );

BOOL
YypMatchExpressionComponent (
    PLEXER Lexer,
    PULONG Position,
    PSTR *ExpressionPointer
    );

VOID
YypSkipExpression (
    PSTR *ExpressionPointer,
    BOOL FindAlternate
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

YY_API
KSTATUS
YyLexInitialize (
    PLEXER Lexer
    )

/*++

Routine Description:

    This routine initializes a lexer.

Arguments:

    Lexer - Supplies a pointer to the lexer to initialize.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the required fields of the lexer are not filled
    in.

--*/

{

    Lexer->Line = 1;
    Lexer->Column = 0;
    Lexer->Position = 0;
    Lexer->TokenCount = 0;
    Lexer->LargestToken = 0;
    Lexer->TokenStringsSize = 0;
    return STATUS_SUCCESS;
}

YY_API
KSTATUS
YyLexGetToken (
    PLEXER Lexer,
    PLEXER_TOKEN Token
    )

/*++

Routine Description:

    This routine gets the next token from the lexer.

Arguments:

    Lexer - Supplies a pointer to the lexer.

    Token - Supplies a pointer where the next token will be filled out and
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_END_OF_FILE if the file end was reached.

    STATUS_MALFORMED_DATA_STREAM if the given input matched no rule in the
    lexer and the lexer was not configured to ignore such things.

--*/

{

    CHAR Character;
    ULONG Current;
    BOOL Ignore;
    PCSTR Input;
    PSTR Literals;
    BOOL Match;
    ULONG Position;
    ULONG TokenValue;

    Input = Lexer->Input;
    Token->String = NULL;

    //
    // Loop until an expression not to ignore comes up.
    //

    do {
        if (Lexer->Position >= Lexer->InputSize) {
            Token->Value = 0;
            Token->Position = Lexer->Position;
            Token->Size = 0;
            Token->Line = Lexer->Line;
            Token->Column = Lexer->Column;
            return STATUS_END_OF_FILE;
        }

        Ignore = FALSE;

        //
        // Try to match a literal first.
        //

        Match = FALSE;
        Literals = Lexer->Literals;
        if (Literals != NULL) {
            Character = Lexer->Input[Lexer->Position];
            while (*Literals != '\0') {
                if (*Literals == Character) {
                    Match = TRUE;
                    Position = Lexer->Position + 1;
                    TokenValue = (UCHAR)Character;
                    break;
                }

                Literals += 1;
            }
        }

        //
        // Attempt to match one of the real tokens.
        //

        if (Match == FALSE) {
            Match = YypMatchExpression(Lexer,
                                       Lexer->Expressions,
                                       &Position,
                                       &TokenValue);

            if (Match != FALSE) {
                TokenValue += Lexer->TokenBase;
            }
        }

        //
        // Attempt to match one of the ignored expressions.
        //

        if (Match == FALSE) {
            Match = YypMatchExpression(Lexer,
                                       Lexer->IgnoreExpressions,
                                       &Position,
                                       &TokenValue);

            Ignore = Match;
        }

        //
        // If there was no match but the caller wants to ignore unknown things,
        // move forward a character.
        //

        if (Match == FALSE) {
            if ((Lexer->Flags & YY_LEX_FLAG_IGNORE_UNKNOWN) != 0) {
                Position = Lexer->Position + 1;
                Ignore = TRUE;

            } else {
                return STATUS_MALFORMED_DATA_STREAM;
            }

        }

        assert(Position <= Lexer->InputSize);

        if (Position > Lexer->InputSize) {
            Position = Lexer->InputSize;
        }

        //
        // Set the return token information unless this portion is being
        // ignored.
        //

        Current = Lexer->Position;
        if (Ignore == FALSE) {
            Token->Value = TokenValue;
            Token->Position = Lexer->Position;
            Token->Size = Position - Current;
            Token->Line = Lexer->Line;
            Token->Column = Lexer->Column;
        }

        //
        // If there's a match, get to the new position, keeping track of
        // column and line.
        //

        while (Current < Position) {
            Lexer->Column += 1;
            if (Input[Current] == '\n') {
                Lexer->Column = 0;
                Lexer->Line += 1;
            }

            Current += 1;
        }

        Lexer->Position = Current;

    } while (Ignore != FALSE);

    assert(Token->Value != 0);

    //
    // Update the stats.
    //

    Lexer->TokenCount += 1;
    Lexer->TokenStringsSize += Token->Size + 1;
    if (Lexer->LargestToken < Token->Size) {
        Lexer->LargestToken = Token->Size;
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
YypMatchExpression (
    PLEXER Lexer,
    PSTR *Expressions,
    PULONG Position,
    PULONG ExpressionIndex
    )

/*++

Routine Description:

    This routine attempts to match the current input to one of the lexer
    constructs.

Arguments:

    Lexer - Supplies a pointer to the lexer. Upon matching, the lexer position
        will be updated.

    Expressions - Supplies a pointer to an array of expressions to check. This
        array must be null terminated.

    Position - Supplies a pointer where the updated position will be returned
        on success. The input position is grabbed from the lexer itself.

    ExpressionIndex - Supplies a pointer where the matching expression index
        will be returned on success.

Return Value:

    TRUE if an expression matched.

    FALSE if no expression matched.

--*/

{

    PSTR Expression;
    ULONG Index;
    BOOL Match;
    ULONG NextPosition;
    ULONG Winner;
    ULONG WinnerPosition;

    Winner = -1;
    WinnerPosition = 0;
    Index = 0;

    //
    // Loop over all the expressions to find the longest one, or the first in
    // the case of a tie.
    //

    while (Expressions[Index] != NULL) {
        NextPosition = Lexer->Position;
        Expression = Expressions[Index];
        Match = YypMatchSubexpression(Lexer, &NextPosition, &Expression);
        if (Match != FALSE) {

            assert(*Expression == '\0');

            if (NextPosition > WinnerPosition) {
                Winner = Index;
                WinnerPosition = NextPosition;
            }
        }

        Index += 1;
    }

    if (WinnerPosition == 0) {
        return FALSE;
    }

    *Position = WinnerPosition;
    *ExpressionIndex = Winner;
    return TRUE;
}

BOOL
YypMatchSubexpression (
    PLEXER Lexer,
    PULONG Position,
    PSTR *ExpressionPointer
    )

/*++

Routine Description:

    This routine attempts to match against the given expression or
    subexpression.

Arguments:

    Lexer - Supplies a pointer to the lexer.

    Position - Supplies a pointer to the current input position, which will be
        advanced.

    ExpressionPointer - Supplies a pointer to a pointer to expression to match.
        This will be advanced past the expression or subexpression.

Return Value:

    TRUE if the expression matches.

    FALSE if the expression does not match.

--*/

{

    ULONG CurrentPosition;
    PSTR Expression;
    ULONG Iterations;
    BOOL Match;
    PSTR NextExpression;
    ULONG NextPosition;
    BOOL NonGreedyMatch;
    PSTR NonGreedyNextExpression;
    ULONG NonGreedyNextPosition;
    CHAR Repeater;
    ULONG Size;

    Expression = *ExpressionPointer;
    NextPosition = *Position;
    Size = Lexer->InputSize;
    Iterations = 0;
    Match = FALSE;

    //
    // Loop processing alternate branches (OR statements).
    //

    while (TRUE) {

        //
        // Loop processing elements within this branch.
        //

        while ((*Expression != '\0') &&
               (*Expression != ')') &&
               (NextPosition < Size)) {

            //
            // Match the next little doodad from here. Then look to see if
            // there's a special qualifier after it, like a repeat or
            // something. This call may recurse back into this function to
            // parse inner () expressions.
            //

            NextExpression = Expression;
            CurrentPosition = NextPosition;
            Match = YypMatchExpressionComponent(Lexer,
                                                &NextPosition,
                                                &NextExpression);

            Repeater = *NextExpression;

            //
            // If a question mark is next (zero or one instances), then it
            // doesn't matter whether or not the expression matched, just keep
            // going.
            //

            if (Repeater == '?') {
                Expression = NextExpression + 1;
                Match = TRUE;

            //
            // Asterisk is zero or more instances, being as greedy as possible.
            // Plus is one or more instances.
            //

            } else if ((Repeater == '*') || (Repeater == '+')) {

                //
                // Support non-greedy versions by trying the rest of the match
                // as it is before advancing.
                //

                if (*(NextExpression + 1) == '?') {
                    NextExpression += 1;
                    if ((Repeater == '*') || (Iterations != 0)) {
                        NonGreedyNextPosition = CurrentPosition;
                        NonGreedyNextExpression = NextExpression + 1;
                        NonGreedyMatch = YypMatchSubexpression(
                                                     Lexer,
                                                     &NonGreedyNextPosition,
                                                     &NonGreedyNextExpression);

                        if (NonGreedyMatch != FALSE) {
                            NextPosition = NonGreedyNextPosition;
                            Expression = NonGreedyNextExpression;
                            Match = TRUE;
                            break;
                        }
                    }
                }

                //
                // If it worked, do not advance the expression, but go back and
                // try to match it again.
                //

                if (Match != FALSE) {
                    Iterations += 1;

                } else {

                    //
                    // For plus, at least one match is required.
                    //

                    if ((Repeater == '+') && (Iterations == 0)) {
                        Match = FALSE;
                        break;
                    }

                    Match = TRUE;
                    Iterations = 0;
                    Expression = NextExpression + 1;
                }

            //
            // A pipe symbol is the OR expression. If this one matched, skip all
            // the other expressions in the chain. Otherwise, try the next one.
            //

            } else if (*NextExpression == '|') {
                if (Match != FALSE) {
                    YypSkipExpression(&NextExpression, FALSE);

                //
                // The last element did not match. Simply move to the next.
                //

                } else {
                    NextExpression += 1;
                }

                Expression = NextExpression;

            //
            // If this is not a control character and it didn't match, then
            // that's a full failure.
            //

            } else {
                if (Match == FALSE) {
                    break;
                }

                Expression = NextExpression;
            }
        }

        //
        // Find the end of the subexpression, looking for alternates if there
        // was no match.
        //

        if ((*Expression != '\0') && (*Expression != ')')) {
            YypSkipExpression(&Expression, !Match);
        }

        //
        // If the end of an expression is an alternate, try again.
        //

        if (*Expression == '|') {

            assert(Match == FALSE);

            Expression += 1;
            continue;
        }

        break;
    }

    if (Match != FALSE) {
        *Position = NextPosition;
    }

    *ExpressionPointer = Expression;
    return Match;
}

BOOL
YypMatchExpressionComponent (
    PLEXER Lexer,
    PULONG Position,
    PSTR *ExpressionPointer
    )

/*++

Routine Description:

    This routine attempts to match a single element at the given expression.

Arguments:

    Lexer - Supplies a pointer to the lexer.

    Position - Supplies a pointer to the current input position, which will be
        advanced.

    ExpressionPointer - Supplies a pointer to a pointer to expression to match.
        This will be advanced past the component.

Return Value:

    TRUE if the expression matches.

    FALSE if the expression does not match.

--*/

{

    CHAR Character;
    PSTR Expression;
    CHAR Input;
    BOOL Match;
    BOOL Not;
    CHAR Previous;

    Match = FALSE;
    Expression = *ExpressionPointer;
    Input = Lexer->Input[*Position];

    //
    // Match a character set.
    //

    if (*Expression == '[') {
        Not = FALSE;
        Expression += 1;
        if (*Expression == '^') {
            Not = TRUE;
            Expression += 1;
        }

        //
        // Allow a close bracket in the character set if it's the first
        // character.
        //

        Previous = 0;
        while (((*Expression != ']') || (Previous == 0)) &&
               (*Expression != '\0')) {

            //
            // Check a range.
            //

            if ((Previous != 0) && (*Expression == '-') &&
                (*(Expression + 1) != ']')) {

                Expression += 1;
                if ((Input >= Previous) && (Input <= *Expression)) {
                    Match = TRUE;
                }

                if (*Expression == '\0') {
                    break;
                }

            //
            // Check one of these characters in the set.
            //

            } else {
                if (Input == *Expression) {
                    Match = TRUE;
                }
            }

            Previous = *Expression;
            Expression += 1;
        }

        if (Not != FALSE) {
            Match = !Match;
        }

        if (*Expression == ']') {
            Expression += 1;
        }

        if (Match != FALSE) {
            *Position += 1;
        }

    //
    // Attempt to match a subexpression.
    //

    } else if (*Expression == '(') {
        Expression += 1;
        Match = YypMatchSubexpression(Lexer, Position, &Expression);
        if (*Expression == ')') {
            Expression += 1;
        }

    //
    // Dot matches anything.
    //

    } else if (*Expression == '.') {
        Expression += 1;
        *Position += 1;
        if (Input != '\0') {
            Match = TRUE;
        }

    //
    // An ordinary character must match exactly, or an escaped character must
    // match the next in the expression.
    //

    } else {
        Character = *Expression;
        Expression += 1;
        if (Character == '\\') {
            Character = *Expression;
            Expression += 1;
        }

        if (Input == Character) {
            Match = TRUE;
            *Position += 1;
        }
    }

    *ExpressionPointer = Expression;
    return Match;
}

VOID
YypSkipExpression (
    PSTR *ExpressionPointer,
    BOOL FindAlternate
    )

/*++

Routine Description:

    This routine skips to the end of the current expression or subexpression.

Arguments:

    ExpressionPointer - Supplies a pointer to a pointer to expression to match.
        This will be advanced to the end of the expression or subexpression.

    FindAlternate - Supplies a boolean that if set stops at the next OR
        symbol '|'. If not set, then this skips all alternatives.

Return Value:

    None.

--*/

{

    PSTR Expression;
    ULONG Parentheses;

    Expression = *ExpressionPointer;

    //
    // Loop looking for a null terminator or a close parentheses, keeping
    // track of nested parentheses.
    //

    Parentheses = 0;
    while (*Expression != '\0') {

        //
        // Stop on an OR if requested.
        //

        if ((*Expression == '|') &&
            (FindAlternate != FALSE) &&
            (Parentheses == 0)) {

            break;
        }

        if (*Expression == ')') {
            if (Parentheses == 0) {
                break;

            } else {
                Parentheses -= 1;
            }

        //
        // Skip over a character class. Watch out that there's not a closing
        // square bracket literal coming up first.
        //

        } else if (*Expression == '[') {
            Expression += 1;
            if (*Expression == '^') {
                Expression += 1;
            }

            if (*Expression == ']') {
                Expression += 1;
            }

            while ((*Expression != '\0') && (*Expression != ']')) {
                Expression += 1;
            }

        //
        // Skip over a literal.
        //

        } else if (*Expression == '\\') {
            Expression += 1;

        //
        // Oh great, a new subexpression.
        //

        } else if (*Expression == '(') {
            Parentheses += 1;
            Expression += 1;
        }

        if (*Expression == '\0') {
            break;
        }

        Expression += 1;
    }

    *ExpressionPointer = Expression;
    return;
}

