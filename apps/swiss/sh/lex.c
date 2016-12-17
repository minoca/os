/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lex.c

Abstract:

    This module implements the lexical tokenizer for the shell.

Author:

    Evan Green 5-Jun-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sh.h"
#include "shparse.h"
#include "../swlib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro puts a character back into the input stream.
//

#define SHELL_LEXER_UNPUT(_Shell, _Character)                 \
    if ((_Character) != EOF) {                                \
                                                              \
        assert((_Shell)->Lexer.UnputCharacterValid == FALSE); \
                                                              \
        (_Shell)->Lexer.UnputCharacter = (_Character);        \
        (_Shell)->Lexer.UnputCharacterValid = TRUE;           \
        if ((_Character) == '\n') {                           \
            (_Shell)->Lexer.LineNumber -= 1;                  \
        }                                                     \
    }

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _EXPANSION_SYNTAX {
    ExpansionSyntaxInvalid,
    ExpansionSyntaxName,
    ExpansionSyntaxBackquote,
    ExpansionSyntaxCurlyBrace,
    ExpansionSyntaxParentheses,
    ExpansionSyntaxDoubleParentheses
} EXPANSION_SYNTAX, *PEXPANSION_SYNTAX;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ShGetInputCharacter (
    PSHELL Shell,
    PINT Character
    );

BOOL
ShGetAnyInputCharacter (
    PSHELL Shell,
    PINT Character
    );

BOOL
ShAddCharacterToTokenBuffer (
    PSHELL Shell,
    CHAR Character
    );

BOOL
ShScanExpansion (
    PSHELL Shell,
    INT Character
    );

VOID
ShCheckForReservedWord (
    PSHELL Shell
    );

BOOL
ShScanPendingHereDocuments (
    PSHELL Shell
    );

BOOL
ShScanHereDocument (
    PSHELL Shell,
    PSHELL_HERE_DOCUMENT HereDocument
    );

VOID
ShLexerError (
    PSHELL Shell,
    PSTR Format,
    ...
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to have the lexer print out each token it grabs.
//

BOOL ShDebugLexer = FALSE;

//
// Define the characters that must be explictly escaped when inside double
// quotes. This also applies for single quotes, but with the addition of a
// backslash. This is null terminated so it is a legitimate string.
//

CHAR ShQuoteEscapeCharacters[] = {
    '!',
    '*',
    '?',
    '[',
    '=',
    '~',
    ':',
    '/',
    '-',
    ']',
    SHELL_CONTROL_QUOTE,
    SHELL_CONTROL_ESCAPE,
    '\0',
};

//
// Define the names of all the tokens.
//

PSTR ShTokenStrings[] = {
    "WORD",
    "ASSIGNMENT_WORD",
    "NAME",
    "IO_NUMBER",
    "DOUBLE_AND",
    "DOUBLE_OR",
    "DOUBLE_SEMICOLON",
    "DOUBLE_LESS_THAN",
    "DOUBLE_GREATER_THAN",
    "LESS_THAN_AND",
    "GREATER_THAN_AND",
    "LESS_THAN_GREATER_THAN",
    "DOUBLE_LESS_THAN_DASH",
    "CLOBBER",
    "IF",
    "THEN",
    "ELSE",
    "ELIF",
    "FI",
    "DO",
    "DONE",
    "CASE",
    "ESAC",
    "WHILE",
    "UNTIL",
    "FOR",
    "TOKEN_IN",
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
ShInitializeLexer (
    PSHELL_LEXER_STATE Lexer,
    FILE *InputFile,
    PSTR InputBuffer,
    UINTN InputBufferSize
    )

/*++

Routine Description:

    This routine initializes the shell lexer state.

Arguments:

    Lexer - Supplies a pointer to the lexer state.

    InputFile - Supplies an optional pointer to the input file.

    InputBuffer - Supplies an optional pointer to the input buffer to use. If
        no buffer is provided one will be created, otherwise the provided one
        will be copied.

    InputBufferSize - Supplies the size of the provided input buffer in bytes
        including the null terminator.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;

    Result = FALSE;
    memset(Lexer, 0, sizeof(SHELL_LEXER_STATE));
    Lexer->TokenType = -1;
    Lexer->InputFile = InputFile;
    Lexer->LineNumber = 1;
    INITIALIZE_LIST_HEAD(&(Lexer->HereDocumentList));
    if (InputBuffer != NULL) {
        Lexer->InputBuffer = malloc(InputBufferSize);
        if (Lexer->InputBuffer == NULL) {
            goto InitializeLexerEnd;
        }

        memcpy(Lexer->InputBuffer, InputBuffer, InputBufferSize);
        Lexer->InputBufferSize = InputBufferSize;
        Lexer->InputBufferCapacity = InputBufferSize;

    } else {
        Lexer->InputBuffer = malloc(DEFAULT_INPUT_BUFFER_SIZE);
        if (Lexer->InputBuffer == NULL) {
            goto InitializeLexerEnd;
        }

        Lexer->InputBufferCapacity = DEFAULT_INPUT_BUFFER_SIZE;
    }

    Lexer->TokenBuffer = malloc(DEFAULT_TOKEN_BUFFER_SIZE);
    if (Lexer->TokenBuffer == NULL) {
        goto InitializeLexerEnd;
    }

    Lexer->TokenBufferCapacity = DEFAULT_TOKEN_BUFFER_SIZE;
    Result = TRUE;

InitializeLexerEnd:
    if (Result == FALSE) {
        if (Lexer->InputBuffer != NULL) {
            free(Lexer->InputBuffer);
        }

        if (Lexer->TokenBuffer != NULL) {
            free(Lexer->TokenBuffer);
        }
    }

    return Result;
}

VOID
ShDestroyLexer (
    PSHELL_LEXER_STATE Lexer
    )

/*++

Routine Description:

    This routine tears down the shell lexer state.

Arguments:

    Lexer - Supplies a pointer to the lexer state.

Return Value:

    None.

--*/

{

    if (Lexer->InputBuffer != NULL) {
        free(Lexer->InputBuffer);
        Lexer->InputBuffer = NULL;
    }

    if (Lexer->TokenBuffer != NULL) {
        free(Lexer->TokenBuffer);
        Lexer->TokenBuffer = NULL;
    }

    if (Lexer->InputFile != NULL) {
        if (Lexer->InputFile != stdin) {
            fclose(Lexer->InputFile);
        }

        Lexer->InputFile = NULL;
    }

    return;
}

BOOL
ShGetToken (
    PSHELL Shell,
    BOOL FirstCommandToken
    )

/*++

Routine Description:

    This routine fetches the next token out of the shell input.

Arguments:

    Shell - Supplies a pointer to the shell to read from.

    FirstCommandToken - Supplies a boolean indicating if this token could be
        the first word in a command, in which case alias substitution will be
        enabled.

Return Value:

    TRUE on success. The next token will be written into the shell structure.

    FALSE on failure.

--*/

{

    BOOL AddCharacter;
    INT Character;
    UINTN CharacterIndex;
    BOOL Delimit;
    BOOL InComment;
    BOOL IsOperator;
    PSHELL_LEXER_STATE Lexer;
    CHAR Quote;
    ULONG QuoteLineNumber;
    BOOL Result;
    BOOL UnputCharacter;

    Delimit = FALSE;
    InComment = FALSE;
    IsOperator = FALSE;
    Lexer = &(Shell->Lexer);
    Lexer->TokenType = -1;
    Lexer->TokenBufferSize = 0;
    Lexer->LastAlias = NULL;
    Quote = 0;
    QuoteLineNumber = 0;
    while (TRUE) {
        AddCharacter = TRUE;
        UnputCharacter = FALSE;
        Result = ShGetInputCharacter(Shell, &Character);
        if (Result == FALSE) {
            goto GetTokenEnd;
        }

        //
        // If inside a quote of some kind, scan according to those rules.
        // Single quotes are only ended by another single quote. Double quotes
        // are ended by an unescaped double quote.
        //

        if ((Quote != 0) && (Character != '\\')) {

            assert((Quote == '"') || (Quote == '\''));

            //
            // Watch out for unterminated quotes.
            //

            if (Character == EOF) {
                ShLexerError(Shell,
                             "Unterminated string starting at line %d.\n",
                             QuoteLineNumber);

                Result = FALSE;
                goto GetTokenEnd;
            }

            //
            // Escape the magic characters when in quotes to make them unmagic.
            //

            if ((strchr(ShQuoteEscapeCharacters, Character) != NULL) ||
                ((Quote == '\'') &&
                 ((Character == '$') || (Character == '`')))) {

                Result = ShAddCharacterToTokenBuffer(Shell,
                                                     SHELL_CONTROL_ESCAPE);

                if (Result == FALSE) {
                    goto GetTokenEnd;
                }
            }

            if (Quote == '\'') {
                if (Character == '\'') {
                    Quote = 0;
                    Character = SHELL_CONTROL_QUOTE;
                }

            } else if (Quote == '"') {
                if (Character == '"') {
                    Quote = 0;
                    Character = SHELL_CONTROL_QUOTE;

                } else if ((Character == '`') || (Character == '$')) {
                    Result = ShScanExpansion(Shell, Character);
                    if (Result == FALSE) {
                        goto GetTokenEnd;
                    }

                    AddCharacter = FALSE;
                }
            }

        //
        // If inside a comment, wait for a newline. When the newline comes,
        // put it back so it gets the full newline treatment on the next pass.
        //

        } else if (InComment != FALSE) {
            AddCharacter = FALSE;
            if (Character == '\n') {
                UnputCharacter = TRUE;
                InComment = FALSE;

            } else if (Character == EOF) {
                InComment = FALSE;
            }

        //
        // If the end of the input is found, delimit the current token, or
        // return it by itself.
        //

        } else if (Character == EOF) {
            Delimit = TRUE;
            if (Lexer->TokenBufferSize != 0) {
                AddCharacter = FALSE;
                if (Character != EOF) {
                    UnputCharacter = TRUE;
                }

            } else {
                Lexer->TokenType = TOKEN_END_OF_FILE;
            }

        //
        // If the previous character was an operator and this one can glom on,
        // then do it.
        //

        } else if (IsOperator != FALSE) {

            assert(Lexer->TokenBufferSize != 0);

            IsOperator = FALSE;
            Delimit = TRUE;

            //
            // This is the second byte, so look at the first.
            //

            if (Lexer->TokenBufferSize == 1) {
                switch (Lexer->TokenBuffer[0]) {

                //
                // Allow <<, <&, <>, and <<-.
                //

                case '<':
                    if (Character == '&') {
                        Lexer->TokenType = TOKEN_LESS_THAN_AND;

                    } else if (Character == '<') {
                        Delimit = FALSE;
                        IsOperator = TRUE;

                    } else if (Character == '>') {
                        Lexer->TokenType = TOKEN_LESS_THAN_GREATER_THAN;

                    } else {
                        AddCharacter = FALSE;
                        UnputCharacter = TRUE;
                    }

                    break;

                //
                // Allow >>, >&, and >|.
                //

                case '>':
                    if (Character == '&') {
                        Lexer->TokenType = TOKEN_GREATER_THAN_AND;

                    } else if (Character == '|') {
                        Lexer->TokenType = TOKEN_CLOBBER;

                    } else if (Character == '>') {
                        Lexer->TokenType = TOKEN_DOUBLE_GREATER_THAN;

                    } else {
                        AddCharacter = FALSE;
                        UnputCharacter = TRUE;
                    }

                    break;

                //
                // Allow for ;;.
                //

                case ';':
                    if (Character == ';') {
                        Lexer->TokenType = TOKEN_DOUBLE_SEMICOLON;

                    } else {
                        AddCharacter = FALSE;
                        UnputCharacter = TRUE;
                    }

                    break;

                //
                // Allow for &&.
                //

                case '&':
                    if (Character == '&') {
                        Lexer->TokenType = TOKEN_DOUBLE_AND;

                    } else {
                        AddCharacter = FALSE;
                        UnputCharacter = TRUE;
                    }

                    break;

                //
                // Allow for ||.
                //

                case '|':
                    if (Character == '|') {
                        Lexer->TokenType = TOKEN_DOUBLE_OR;

                    } else {
                        AddCharacter = FALSE;
                        UnputCharacter = TRUE;
                    }

                    break;

                default:

                    assert(FALSE);

                    Result = FALSE;
                    goto GetTokenEnd;
                }

            //
            // The only three character operator is <<-.
            //

            } else {

                assert(Lexer->TokenBufferSize == 2);
                assert((Lexer->TokenBuffer[0] == '<') &&
                       (Lexer->TokenBuffer[1] == '<'));

                if (Character == '-') {
                    Lexer->TokenType = TOKEN_DOUBLE_LESS_THAN_DASH;

                } else {
                    Lexer->TokenType = TOKEN_DOUBLE_LESS_THAN;
                    AddCharacter = FALSE;
                    UnputCharacter = TRUE;
                }
            }

        //
        // Watch out for the beginning of a quoted section.
        //

        } else if ((Character == '\'') || (Character == '"')) {
            Quote = Character;
            Character = SHELL_CONTROL_QUOTE;
            QuoteLineNumber = Lexer->LineNumber;
            Lexer->TokenType = TOKEN_WORD;

        //
        // If it's a backslash, escape the next character, or prepare a line
        // continuation. This logic is entered even if inside a quoted region.
        //

        } else if (Character == '\\') {
            if (Quote == '\'') {

                //
                // In single quotes, the backslash is escaped and literal.
                //

                Result = ShAddCharacterToTokenBuffer(Shell,
                                                     SHELL_CONTROL_ESCAPE);

                if (Result == FALSE) {
                    goto GetTokenEnd;
                }

            //
            // Not in single quotes, so look at the next character.
            //

            } else {
                Result = ShGetInputCharacter(Shell, &Character);
                if (Result == FALSE) {
                    goto GetTokenEnd;
                }

                if (Character == EOF) {
                    Character = '\\';

                //
                // If it's a newline, then it's a line continuation, so just
                // swallow the backslash and add the newline as a normal
                // character (ie do nothing).
                //

                } else if (Character == '\n') {
                    if (Quote == 0) {
                        ShPrintPrompt(Shell, 2);
                    }

                    AddCharacter = FALSE;

                } else {

                    //
                    // If inside double quotes and the backslash isn't quoting
                    // anything, then add it as a literal.
                    //

                    if ((Quote == '"') && (Character != '\\') &&
                        (Character != '`') && (Character != '$') &&
                        (Character != '"')) {

                        Result = ShAddCharacterToTokenBuffer(Shell, '\\');
                        if (Result == FALSE) {
                            goto GetTokenEnd;
                        }
                    }

                    //
                    // Escape the next character, whatever it may be.
                    //

                    Result = ShAddCharacterToTokenBuffer(Shell,
                                                         SHELL_CONTROL_ESCAPE);

                    if (Result == FALSE) {
                        goto GetTokenEnd;
                    }
                }

                if ((Character != '\n') && (Lexer->TokenType == -1)) {
                    Lexer->TokenType = TOKEN_WORD;
                }
            }

        //
        // If it's an unquoted dollar sign or backquote, scan past the
        // following expansion. The expansion does not delimit the token.
        //

        } else if ((Character == '$') || (Character == '`')) {
            Lexer->TokenType = TOKEN_WORD;
            Result = ShScanExpansion(Shell, Character);
            if (Result == FALSE) {
                goto GetTokenEnd;
            }

            AddCharacter = FALSE;

        //
        // Check for a new operator. Lump newlines in here too since their
        // processing is about the same. Notice that bang and the braces aren't
        // in here, as they're recognized at the token level rather than the
        // lexical level.
        //

        } else if ((Character == '&') || (Character == '|') ||
                   (Character == ';') || (Character == '<') ||
                   (Character == '>') || (Character == ')') ||
                   (Character == '(') || (Character == '\n')) {

            //
            // If there was a previous token, delimit it now.
            //

            if (Lexer->TokenBufferSize != 0) {
                Delimit = TRUE;
                AddCharacter = FALSE;
                UnputCharacter = TRUE;

                //
                // If this is a redirection symbol and everything in the token
                // is a digit, then this is an I/O number token.
                //

                if ((Lexer->TokenType == TOKEN_WORD) &&
                    ((Character == '>') || (Character == '<'))) {

                    for (CharacterIndex = 0;
                         CharacterIndex < Lexer->TokenBufferSize;
                         CharacterIndex += 1) {

                        if ((Lexer->TokenBuffer[CharacterIndex] < '0') ||
                            (Lexer->TokenBuffer[CharacterIndex] > '9')) {

                            break;
                        }
                    }

                    if (CharacterIndex == Lexer->TokenBufferSize) {
                        Lexer->TokenType = TOKEN_IO_NUMBER;
                    }
                }

            //
            // The token buffer is empty, this operator is up. If there's a
            // possibility that it's a multi-character operator, then don't
            // delimit right away.
            //

            } else {
                Lexer->TokenType = Character;
                if ((Character == '>') || (Character == '<') ||
                    (Character == '&') || (Character == '|') ||
                    (Character == ';')) {

                    IsOperator = TRUE;

                } else {
                    Delimit = TRUE;

                    //
                    // If this is a newline, parse out any pending here
                    // documents.
                    //

                    if (Character == '\n') {
                        Result = ShScanPendingHereDocuments(Shell);
                        if (Result == FALSE) {
                            goto GetTokenEnd;
                        }
                    }
                }
            }

        //
        // If it's an unquoted space, any token containing the previous
        // character is delimited, and the blank is discarded.
        //

        } else if (isspace(Character)) {
            AddCharacter = FALSE;
            if (Lexer->TokenBufferSize != 0) {
                Delimit = TRUE;
            }

        //
        // Look out for a comment. Comments can only start if there's not
        // already a word in progress.
        //

        } else if ((Lexer->TokenBufferSize == 0) && (Character == '#')) {
            AddCharacter = FALSE;
            InComment = TRUE;

        //
        // It doesn't fit any other interesting case, so it's just a word.
        //

        } else {
            if (Lexer->TokenType == -1) {
                Lexer->TokenType = TOKEN_WORD;
            }

            //
            // If it's a control character, escape it.
            //

            if ((Character == SHELL_CONTROL_QUOTE) ||
                (Character == SHELL_CONTROL_ESCAPE)) {

                Result = ShAddCharacterToTokenBuffer(Shell,
                                                     SHELL_CONTROL_ESCAPE);

                if (Result == FALSE) {
                    goto GetTokenEnd;
                }
            }
        }

        if ((Quote != 0) && (Character == '\n')) {
            ShPrintPrompt(Shell, 2);
        }

        //
        // Add the character if desired.
        //

        if (AddCharacter != FALSE) {
            Result = ShAddCharacterToTokenBuffer(Shell, Character);
            if (Result == FALSE) {
                goto GetTokenEnd;
            }
        }

        if (UnputCharacter != FALSE) {

            assert(AddCharacter == FALSE);

            SHELL_LEXER_UNPUT(Shell, Character);
        }

        //
        // If the token is over, null terminate it, put back this character,
        // and break out.
        //

        if (Delimit != FALSE) {
            Delimit = FALSE;
            Result = ShAddCharacterToTokenBuffer(Shell, '\0');
            if (Result == FALSE) {
                goto GetTokenEnd;
            }

            if (Lexer->TokenType == TOKEN_WORD) {
                ShCheckForReservedWord(Shell);
            }

            //
            // If it's still just a word but has an equals in it, it's an
            // assignment word. It could also be a ! } or { if it's just that
            // character.
            //

            if (Lexer->TokenType == TOKEN_WORD) {
                if (strchr(Lexer->TokenBuffer, '=') != NULL) {
                    Lexer->TokenType = TOKEN_ASSIGNMENT_WORD;

                } else if (Lexer->TokenBufferSize == 2) {
                    if (Lexer->TokenBuffer[0] == '!') {
                        Lexer->TokenType = '!';

                    } else if (Lexer->TokenBuffer[0] == '{') {
                        Lexer->TokenType = '{';

                    } else if (Lexer->TokenBuffer[0] == '}') {
                        Lexer->TokenType = '}';
                    }
                }
            }

            //
            // If even after all that it's still a word and it's the first
            // word of the command, perform alias substitution.
            //

            if ((FirstCommandToken != FALSE) &&
                (Lexer->TokenType == TOKEN_WORD)) {

                Result = ShPerformAliasSubstitution(Shell);
                if (Result == FALSE) {
                    goto GetTokenEnd;
                }

            } else {

                assert(Shell->Lexer.TokenType != -1);

            }

            //
            // If alias substitution didn't kill this token, then break out
            // and return it.
            //

            if (Shell->Lexer.TokenType != -1) {
                break;

            } else {
                Character = 0;
            }
        }
    }

GetTokenEnd:

    assert((Result == FALSE) || (Lexer->TokenType != -1));

    if (ShDebugLexer != FALSE) {
        if (Result != FALSE) {
            if (Lexer->TokenType == TOKEN_END_OF_FILE) {
                ShPrintTrace(Shell, "Reached end of file.\n");

            } else if (Lexer->TokenType < 0xFF) {
                if (Lexer->TokenType < ' ') {
                    if (Lexer->TokenType == '\n') {
                        ShPrintTrace(Shell,
                                     "%20s: Line %d\n",
                                     "<newline>",
                                     Lexer->LineNumber);

                    } else {
                        ShPrintTrace(Shell, "%20d: \n", Lexer->TokenType);
                    }

                } else {
                    ShPrintTrace(Shell,
                                 "%20c: %s\n",
                                 Lexer->TokenType,
                                 Lexer->TokenBuffer);
                }

            } else {

                assert(Lexer->TokenType >= TOKEN_WORD);

                ShPrintTrace(Shell,
                             "%20s: %s\n",
                             ShTokenStrings[Lexer->TokenType - TOKEN_WORD],
                             Lexer->TokenBuffer);
            }

        } else {
            ShPrintTrace(Shell,
                         "Error: Failed to parse token at line %d.\n",
                         Lexer->LineNumber);
        }
    }

    return Result;
}

BOOL
ShScanPastExpansion (
    PSTR String,
    UINTN StringSize,
    PUINTN ExpansionSize
    )

/*++

Routine Description:

    This routine is called to find the end of an expansion.

Arguments:

    String - Supplies a pointer to the string at an expansion to scan.

    StringSize - Supplies the number of bytes in the string.

    ExpansionSize - Supplies a pointer where the size of the expansion in
        bytes will be returned.

Return Value:

    TRUE on success. The extent of the expansion will be added to the token
        buffer.

    FALSE on failure.

--*/

{

    CHAR Character;
    UINTN Index;
    UINTN InnerExpansionSize;
    UINTN OpenCount;
    CHAR Quote;
    BOOL RecognizeComments;
    BOOL RecognizeQuotes;
    BOOL Result;
    EXPANSION_SYNTAX Syntax;
    BOOL WasBackslash;
    BOOL WasName;
    BOOL WasParentheses;

    Index = 1;
    OpenCount = 0;

    //
    // Figure out what type of expansion this is.
    //

    assert(StringSize != 0);
    assert((*String == '$') || (*String == '`') || (*String == '~'));

    RecognizeComments = TRUE;
    RecognizeQuotes = TRUE;
    if (*String == '`') {
        Syntax = ExpansionSyntaxBackquote;
        RecognizeComments = FALSE;
        RecognizeQuotes = FALSE;

    } else if (*String == '~') {
        Syntax = ExpansionSyntaxName;

    } else {

        assert(*String == '$');

        Character = String[Index];

        //
        // If it was a digit or a special parameter, then that's all there is
        // to it.
        //

        if (((Character >= '0') && (Character <= '9')) ||
            (Character == '@') || (Character == '*') || (Character == '#') ||
            (Character == '?') || (Character == '-') || (Character == '$') ||
            (Character == '!')) {

            *ExpansionSize = Index + 1;
            return TRUE;
        }

        //
        // It shouldn't be the end of file.
        //

        if (Character == '\0') {
            *ExpansionSize = Index;
            return TRUE;

        //
        // Note if it's a single curly.
        //

        } else if (Character == '{') {
            Syntax = ExpansionSyntaxCurlyBrace;
            RecognizeComments = FALSE;

        //
        // Note if it's a single parentheses. It could also be a double
        // parentheses.
        //

        } else if (Character == '(') {
            Syntax = ExpansionSyntaxParentheses;
            Index += 1;
            if (Index == StringSize) {
                return FALSE;
            }

            Character = String[Index];
            if (Character == '\0') {
                return FALSE;

            } else if (Character == '(') {
                Syntax = ExpansionSyntaxDoubleParentheses;
                Index += 1;
            }

        //
        // The only other option is it's a raw name.
        //

        } else if (SHELL_NAME_FIRST_CHARACTER(Character) != FALSE) {
            Syntax = ExpansionSyntaxName;

        //
        // Something funky is following the dollar sign.
        //

        } else {
            *ExpansionSize = 0;
            return TRUE;
        }
    }

    if (Syntax == ExpansionSyntaxName) {
        RecognizeComments = FALSE;
        RecognizeQuotes = FALSE;
    }

    //
    // Loop looking at characters until the parameter is finished.
    //

    Quote = 0;
    WasBackslash = FALSE;
    WasParentheses = FALSE;
    WasName = FALSE;
    while (TRUE) {
        Character = String[Index];

        //
        // If quoting is in progress, look for the end.
        //

        if (Quote != 0) {
            if ((Quote == '\'') || (Quote == SHELL_CONTROL_QUOTE)) {
                if (Character == Quote) {
                    Quote = 0;
                }

            } else if (Quote == '"') {
                if ((WasBackslash == FALSE) && (Character == '"')) {
                    Quote = 0;
                }

            } else if (Quote == '#') {
                if (Character == '\n') {
                    Quote = 0;
                }

            } else {

                assert((Quote == '\\') || (Quote == SHELL_CONTROL_ESCAPE));

                Quote = 0;
            }

        //
        // If eligible for quotes, look for quotes starting.
        //

        } else if ((RecognizeQuotes != FALSE) &&
                   ((Character == '\'') || (Character == '"') ||
                    (Character == '\\') ||
                    (Character == SHELL_CONTROL_QUOTE) ||
                    (Character == SHELL_CONTROL_ESCAPE))) {

            Quote = Character;

        //
        // If eligible for comments, look for comments starting.
        //

        } else if ((RecognizeComments != FALSE) && (Character == '#') &&
                   (WasName == FALSE)) {

            Quote = Character;

        //
        // No quotes or comments, look for the end expansion character.
        //

        } else {
            switch (Syntax) {
            case ExpansionSyntaxName:
                if (!SHELL_NAME_CHARACTER(Character)) {
                    *ExpansionSize = Index;
                    return TRUE;
                }

                break;

            case ExpansionSyntaxBackquote:
                if ((Character == '`') && (WasBackslash == FALSE)) {
                    *ExpansionSize = Index + 1;
                    return TRUE;
                }

                break;

            case ExpansionSyntaxCurlyBrace:
                if (Character == '}') {
                    *ExpansionSize = Index + 1;
                    return TRUE;
                }

                break;

            case ExpansionSyntaxParentheses:
                if (Character == '(') {
                    OpenCount += 1;

                } else if (Character == ')') {
                    if (OpenCount == 0) {
                        *ExpansionSize = Index + 1;
                        return TRUE;

                    } else {
                        OpenCount -= 1;
                    }
                }

                break;

            case ExpansionSyntaxDoubleParentheses:
                if (Character == ')') {
                    if (OpenCount != 0) {
                        OpenCount -= 1;

                    } else {
                        if (WasParentheses != FALSE) {
                            *ExpansionSize = Index + 1;
                            return TRUE;

                        } else {
                            WasParentheses = TRUE;
                        }
                    }

                } else {
                    WasParentheses = FALSE;
                    if (Character == '(') {
                        OpenCount += 1;
                    }
                }

                break;

            default:

                assert(FALSE);

                return FALSE;
            }

            //
            // Look for a new expansion beginning.
            //

            if (((Character == '$') || (Character == '`')) &&
                (Syntax != ExpansionSyntaxBackquote)) {

                Result = ShScanPastExpansion(String + Index,
                                             StringSize - Index,
                                             &InnerExpansionSize);

                if (Result == FALSE) {
                    return FALSE;
                }

                if (Index == StringSize) {
                    return FALSE;
                }

                if (InnerExpansionSize == 0) {
                    InnerExpansionSize = 1;
                }

                Index += InnerExpansionSize;
                WasBackslash = FALSE;
                WasParentheses = FALSE;
                WasName = FALSE;
                continue;
            }
        }

        if (Character == '\\') {
            WasBackslash = !WasBackslash;

        } else if (Character == SHELL_CONTROL_ESCAPE) {
            WasBackslash = TRUE;

        } else {
            WasBackslash = FALSE;
        }

        if (SHELL_NAME_CHARACTER(Character)) {
            WasName = TRUE;

        } else {
            WasName = FALSE;
        }

        Index += 1;
        if (Index == StringSize) {
            return FALSE;
        }
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ShGetInputCharacter (
    PSHELL Shell,
    PINT Character
    )

/*++

Routine Description:

    This routine gets a character from the input stream.

Arguments:

    Shell - Supplies a pointer to the shell to read from.

    Character - Supplies a pointer where the character will be returned on
        success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;

    do {
        Result = ShGetAnyInputCharacter(Shell, Character);

    } while ((Result != FALSE) &&
             ((*Character == '\r') || (*Character == '\0')));

    return Result;
}

BOOL
ShGetAnyInputCharacter (
    PSHELL Shell,
    PINT Character
    )

/*++

Routine Description:

    This routine gets a character from the input stream.

Arguments:

    Shell - Supplies a pointer to the shell to read from.

    Character - Supplies a pointer where the character will be returned on
        success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ssize_t BytesRead;
    size_t BytesToRead;
    PSHELL_LEXER_STATE Lexer;
    PSTR NewInputBuffer;
    ULONG NewInputBufferSize;
    BOOL Result;

    Lexer = &(Shell->Lexer);
    if (Lexer->UnputCharacterValid != FALSE) {
        *Character = Lexer->UnputCharacter;
        if (*Character == '\n') {
            Lexer->LineNumber += 1;
        }

        Lexer->UnputCharacterValid = FALSE;
        return TRUE;
    }

    //
    // If there's more in the buffer, return that.
    //

    if (Lexer->InputBufferNextIndex < Lexer->InputBufferSize) {
        *Character = Lexer->InputBuffer[Lexer->InputBufferNextIndex];
        Lexer->InputBufferNextIndex += 1;
        goto GetInputCharacterEnd;
    }

    //
    // If there is no file, donezo.
    //

    if (((Shell->Options & SHELL_OPTION_READ_FROM_STDIN) == 0) &&
        (Lexer->InputFile == NULL)) {

        *Character = EOF;
        goto GetInputCharacterEnd;
    }

    if ((Shell->Options & SHELL_OPTION_INPUT_BUFFER_ONLY) != 0) {
        *Character = EOF;
        goto GetInputCharacterEnd;
    }

    //
    // Read from the file, or do fancy line-based input for interactive shells.
    //

    if ((Shell->Options & SHELL_OPTION_RAW_INPUT) != 0) {
        Result = ShReadLine(Shell, &NewInputBuffer, &NewInputBufferSize);
        if (Result == FALSE) {
            return FALSE;
        }

        //
        // Change the null terminator into a newline.
        //

        if ((NewInputBufferSize != 0) &&
            (NewInputBuffer[NewInputBufferSize - 1] == '\0')) {

            NewInputBuffer[NewInputBufferSize - 1] = '\n';
        }

        if (Lexer->InputBuffer != NULL) {
            free(Lexer->InputBuffer);
        }

        Lexer->InputBuffer = NewInputBuffer;
        Lexer->InputBufferSize = NewInputBufferSize;
        Lexer->InputBufferCapacity = Lexer->InputBufferSize;
        BytesRead = NewInputBufferSize;

    } else {
        if ((Shell->Options & SHELL_OPTION_INTERACTIVE) != 0) {
            BytesToRead = 1;

        } else {
            BytesToRead = Lexer->InputBufferCapacity;
        }

        //
        // Read using a file stream.
        //

        if (Lexer->InputFile != NULL) {
            do {
                BytesRead = fread(Lexer->InputBuffer,
                                  1,
                                  BytesToRead,
                                  Lexer->InputFile);

            } while ((BytesRead == 0) && (errno == EINTR));

            if (BytesRead <= 0) {
                if (feof(Lexer->InputFile) != 0) {
                    *Character = EOF;
                    goto GetInputCharacterEnd;
                }

                return FALSE;
            }

        //
        // If reading from standard in, read directly from the descriptor.
        //

        } else {

            assert((Shell->Options & SHELL_OPTION_READ_FROM_STDIN) != 0);

            do {
                BytesRead = read(STDIN_FILENO, Lexer->InputBuffer, BytesToRead);

            } while ((BytesRead < 0) && (errno == EINTR));

            if (BytesRead <= 0) {
                if (BytesRead == 0) {
                    *Character = EOF;
                    goto GetInputCharacterEnd;
                }

                return FALSE;
            }
        }
    }

    Lexer->InputBufferSize = BytesRead;
    *Character = Lexer->InputBuffer[0];
    Lexer->InputBufferNextIndex = 1;

GetInputCharacterEnd:
    if ((*Character != 0) &&
        ((Shell->Options & SHELL_OPTION_DISPLAY_INPUT) != 0)) {

        if (*Character == EOF) {
            ShPrintTrace(Shell, "<EOF>");

        } else {
            ShPrintTrace(Shell, "%c", *Character);
        }
    }

    if (*Character == '\n') {
        Lexer->LineNumber += 1;
    }

    return TRUE;
}

BOOL
ShAddCharacterToTokenBuffer (
    PSHELL Shell,
    CHAR Character
    )

/*++

Routine Description:

    This routine adds the given character to the token buffer, expanding it if
    necessary.

Arguments:

    Shell - Supplies a pointer to the shell to operate on.

    Character - Supplies the character to add.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_LEXER_STATE Lexer;
    PSTR NewBuffer;
    UINTN NewCapacity;

    Lexer = &(Shell->Lexer);
    if (Lexer->TokenBufferSize < Lexer->TokenBufferCapacity) {
        Lexer->TokenBuffer[Lexer->TokenBufferSize] = Character;
        Lexer->TokenBufferSize += 1;
        return TRUE;
    }

    //
    // Bummer, the buffer needs to be reallocated.
    //

    NewCapacity = Lexer->TokenBufferCapacity * 2;
    NewBuffer = realloc(Lexer->TokenBuffer, NewCapacity);
    if (NewBuffer == NULL) {
        printf("Error: Failed to allocate %ld bytes for expanded token "
               "buffer.\n",
               NewCapacity);

        return FALSE;
    }

    //
    // Now add the byte.
    //

    Lexer->TokenBuffer = NewBuffer;
    Lexer->TokenBufferCapacity = NewCapacity;
    Lexer->TokenBuffer[Lexer->TokenBufferSize] = Character;
    Lexer->TokenBufferSize += 1;
    return TRUE;
}

BOOL
ShScanExpansion (
    PSHELL Shell,
    INT Character
    )

/*++

Routine Description:

    This routine is called when the lexer finds a dollar sign. It recursively
    scans the inside of an expansion such as `...`, $param, ${...}, $(...), and
    $((...)).

Arguments:

    Shell - Supplies a pointer to the shell to read from.

    Character - Supplies the initial character that caused entry into this
        function. It is assumed that this character has not yet been added to
        the token buffer.

Return Value:

    TRUE on success. The extent of the expansion will be added to the token
        buffer.

    FALSE on failure.

--*/

{

    BOOL AddCharacter;
    BOOL InComment;
    BOOL InWord;
    CHAR LastCharacter;
    PSHELL_LEXER_STATE Lexer;
    ULONG OpenCount;
    CHAR Quote;
    BOOL Result;
    BOOL Stop;
    EXPANSION_SYNTAX Syntax;
    BOOL WasParentheses;

    InComment = FALSE;
    Lexer = &(Shell->Lexer);
    OpenCount = 0;
    Quote = 0;
    WasParentheses = FALSE;

    //
    // First add the dollar sign or backquote to the token buffer.
    //

    Result = ShAddCharacterToTokenBuffer(Shell, Character);
    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Figure out what type of expansion this is.
    //

    if (Character == '`') {
        Syntax = ExpansionSyntaxBackquote;

    } else {

        assert(Character == '$');

        //
        // Get the next character to learn more.
        //

        Result = ShGetInputCharacter(Shell, &Character);
        if ((Result == FALSE) || (Character == EOF)) {
            return TRUE;
        }

        Result = ShAddCharacterToTokenBuffer(Shell, Character);
        if (Result == FALSE) {
            return FALSE;
        }

        //
        // If it was a digit or a special parameter, then that's all there is
        // to it.
        //

        if (((Character >= '0') && (Character <= '9')) ||
            (Character == '@') || (Character == '*') || (Character == '#') ||
            (Character == '?') || (Character == '-') || (Character == '$') ||
            (Character == '!')) {

            return TRUE;
        }

        //
        // Note if it's a single curly.
        //

        if (Character == '{') {
            Syntax = ExpansionSyntaxCurlyBrace;

        //
        // Note if it's a single parentheses. It could also be a double
        // parentheses.
        //

        } else if (Character == '(') {
            Syntax = ExpansionSyntaxParentheses;
            Result = ShGetInputCharacter(Shell, &Character);
            if ((Result == FALSE) || (Character == EOF)) {
                return FALSE;
            }

            Result = ShAddCharacterToTokenBuffer(Shell, Character);
            if (Result == FALSE) {
                return FALSE;
            }

            if (Character == '\0') {
                return FALSE;

            } else if (Character == '(') {
                Syntax = ExpansionSyntaxDoubleParentheses;

            } else {
                SHELL_LEXER_UNPUT(Shell, Character);

                assert(Lexer->TokenBufferSize != 0);

                Lexer->TokenBufferSize -= 1;
            }

        //
        // The only other option is it's a raw name.
        //

        } else if (SHELL_NAME_CHARACTER(Character) != FALSE) {
            Syntax = ExpansionSyntaxName;

        //
        // Something funky is following the dollar sign, this isn't really
        // an expansion.
        //

        } else {
            SHELL_LEXER_UNPUT(Shell, Character);

            assert(Lexer->TokenBufferSize != 0);

            Lexer->TokenBufferSize -= 1;
            return TRUE;
        }
    }

    //
    // Loop getting input until this expansion is over.
    //

    Stop = FALSE;
    while (Stop == FALSE) {
        AddCharacter = TRUE;
        Result = ShGetInputCharacter(Shell, &Character);
        if (Result == FALSE) {
            return FALSE;
        }

        //
        // If inside a quote of some kind, scan according to those rules.
        // Single quotes are only ended by another single quote. Double quotes
        // are ended by an unescaped double quote.
        //

        if ((Quote != 0) && (Character != '\\')) {

            assert((Quote == '"') || (Quote == '\''));

            //
            // Escape the magic characters when in quotes to make them unmagic.
            //

            if (Syntax == ExpansionSyntaxCurlyBrace) {
                if ((strchr(ShQuoteEscapeCharacters, Character) != NULL) ||
                    ((Quote == '\'') &&
                     ((Character == '$') || (Character == '`')))) {

                    Result = ShAddCharacterToTokenBuffer(Shell,
                                                         SHELL_CONTROL_ESCAPE);

                    if (Result == FALSE) {
                        return FALSE;
                    }
                }
            }

            if (Quote == '\'') {
                if (Character == '\'') {
                    Quote = 0;
                    if (Syntax == ExpansionSyntaxCurlyBrace) {
                        Character = SHELL_CONTROL_QUOTE;
                    }
                }

            } else if (Quote == '"') {
                if (Character == '"') {
                    Quote = 0;
                    if (Syntax == ExpansionSyntaxCurlyBrace) {
                        Character = SHELL_CONTROL_QUOTE;
                    }
                }
            }

        //
        // If inside a comment, wait for a newline. When the newline comes,
        // it's not really handled any differently other than it would break up
        // two parentheses in a row.
        //

        } else if (InComment != FALSE) {
            AddCharacter = FALSE;
            if (Character == '\n') {
                AddCharacter = TRUE;
                WasParentheses = FALSE;
                InComment = FALSE;
            }

        //
        // If it's a backslash, escape the next character, or prepare a line
        // continuation. This logic is entered even if inside a quoted region.
        //

        } else if ((Character == '\\') && (Syntax != ExpansionSyntaxName)) {
            if (Quote == '\'') {

                //
                // In single quotes, the backslash is escaped and literal.
                //

                if (Syntax == ExpansionSyntaxCurlyBrace) {
                    Result = ShAddCharacterToTokenBuffer(Shell,
                                                         SHELL_CONTROL_ESCAPE);

                    if (Result == FALSE) {
                        return FALSE;
                    }
                }

            //
            // Not in single quotes, so look at the next character.
            //

            } else {
                Result = ShGetInputCharacter(Shell, &Character);
                if (Result == FALSE) {
                    return FALSE;
                }

                //
                // If it's a newline, then it's a line continuation, so just
                // swallow the backslash and newline.
                //

                if (Character == '\n') {
                    AddCharacter = FALSE;

                } else {
                    if (Syntax == ExpansionSyntaxCurlyBrace) {

                        //
                        // If inside double quotes and the backslash isn't
                        // quoting anything, then add it as a literal.
                        //

                        if ((Quote == '"') && (Character != '\\') &&
                            (Character != '`') && (Character != '$') &&
                            (Character != '"')) {

                            Result = ShAddCharacterToTokenBuffer(Shell, '\\');
                            if (Result == FALSE) {
                                return FALSE;
                            }
                        }

                        //
                        // Escape the next character, whatever it may be.
                        //

                        Result = ShAddCharacterToTokenBuffer(
                                                         Shell,
                                                         SHELL_CONTROL_ESCAPE);

                        if (Result == FALSE) {
                            return FALSE;
                        }

                    //
                    // Pass everything through for non-curly expansion, as it
                    // gets reinterpreted inside the subshell.
                    //

                    } else {
                        Result = ShAddCharacterToTokenBuffer(Shell, '\\');
                        if (Result == FALSE) {
                            return FALSE;
                        }
                    }
                }
            }

        //
        // Look for the elusive closing sequence.
        //

        } else {
            switch (Syntax) {
            case ExpansionSyntaxName:
                if (SHELL_NAME_CHARACTER(Character) == FALSE) {
                    Stop = TRUE;
                    AddCharacter = FALSE;
                    SHELL_LEXER_UNPUT(Shell, Character);
                }

                break;

            case ExpansionSyntaxBackquote:
                if (Character == '`') {
                    Stop = TRUE;
                }

                break;

            case ExpansionSyntaxCurlyBrace:
            case ExpansionSyntaxParentheses:
                if ((Syntax == ExpansionSyntaxParentheses) &&
                    (Character == '(')) {

                    OpenCount += 1;

                } else if ((Syntax == ExpansionSyntaxParentheses) &&
                           (Character == ')')) {

                    if (OpenCount != 0) {
                        OpenCount -= 1;

                    } else {
                        Stop = TRUE;
                        break;
                    }

                //
                // Note that curly braces don't allow recursion or quotes
                // inside the variable name, but they can be in the
                // post-variable-name part (ie ${myvar+"other$var"}).
                //

                } else if ((Syntax == ExpansionSyntaxCurlyBrace) &&
                           (Character == '}')) {

                    Stop = TRUE;
                    break;
                }

                //
                // Watch out for quotes starting.
                //

                if ((Character == '"') || (Character == '\'')) {
                    Quote = Character;
                    if (Syntax == ExpansionSyntaxCurlyBrace) {
                        Character = SHELL_CONTROL_QUOTE;
                    }

                //
                // If it's a dollar sign or backquote, recurse into another
                // expansion.
                //

                } else if ((Character == '$') || (Character == '`')) {
                    AddCharacter = FALSE;
                    Result = ShScanExpansion(Shell, Character);
                    if (Result == FALSE) {
                        return FALSE;
                    }

                //
                // Watch out for a comment beginning, but only if it's not
                // already in the middle of a word. Don't do this inside
                // curly brace expansions, as something like ${#a} means
                // "length of a".
                //

                } else if ((Character == '#') &&
                           (Syntax != ExpansionSyntaxCurlyBrace)) {

                    InWord = FALSE;

                    assert(Lexer->TokenBufferSize != 0);

                    LastCharacter =
                                Lexer->TokenBuffer[Lexer->TokenBufferSize - 1];

                    if (SHELL_NAME_CHARACTER(LastCharacter) != FALSE) {
                        InWord = TRUE;
                    }

                    if (InWord == FALSE) {
                        InComment = TRUE;
                        AddCharacter = FALSE;
                    }
                }

                break;

            case ExpansionSyntaxDoubleParentheses:
                if (Character == ')') {
                    if (OpenCount != 0) {
                        OpenCount -= 1;

                    } else {
                        if (WasParentheses != FALSE) {
                            Stop = TRUE;

                        } else {
                            WasParentheses = TRUE;
                        }
                    }

                } else {
                    WasParentheses = FALSE;
                    if (Character == '(') {
                        OpenCount += 1;
                    }
                }

                break;

            default:

                assert(FALSE);

                return FALSE;
            }
        }

        if ((Character == '\0') || (Character == EOF)) {
            AddCharacter = FALSE;
        }

        if (AddCharacter != FALSE) {
            Result = ShAddCharacterToTokenBuffer(Shell, Character);
            if (Result == FALSE) {
                return FALSE;
            }
        }

        if (Stop != FALSE) {
            break;
        }

        if (Character == '\n') {
            ShPrintPrompt(Shell, 2);
        }

        if ((Character == '\0') || (Character == EOF)) {
            return TRUE;
        }
    }

    return TRUE;
}

VOID
ShCheckForReservedWord (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine is called immediately before returning what would otherwise
    be a WORD token from the lexer. It checks against the reserved words of
    the shell language and fixes up the token if it matches.

Arguments:

    Shell - Supplies a pointer to the shell about to return a WORD.

Return Value:

    None.

--*/

{

    PSTR Word;

    assert(Shell->Lexer.TokenType == TOKEN_WORD);

    Word = Shell->Lexer.TokenBuffer;
    switch (Word[0]) {
    case 'c':
        if (strcmp(Word + 1, "ase") == 0) {
            Shell->Lexer.TokenType = TOKEN_CASE;
        }

        break;

    case 'd':
        if (strcmp(Word + 1, "o") == 0) {
            Shell->Lexer.TokenType = TOKEN_DO;

        } else if (strcmp(Word + 1, "one") == 0) {
            Shell->Lexer.TokenType = TOKEN_DONE;
        }

        break;

    case 'e':
        if (strcmp(Word + 1, "sac") == 0) {
            Shell->Lexer.TokenType = TOKEN_ESAC;

        } else if (strcmp(Word + 1, "lse") == 0) {
            Shell->Lexer.TokenType = TOKEN_ELSE;

        } else if (strcmp(Word + 1, "lif") == 0) {
            Shell->Lexer.TokenType = TOKEN_ELIF;
        }

        break;

    case 'f':
        if (strcmp(Word + 1, "i") == 0) {
            Shell->Lexer.TokenType = TOKEN_FI;

        } else if (strcmp(Word + 1, "or") == 0) {
            Shell->Lexer.TokenType = TOKEN_FOR;
        }

        break;

    case 'i':
        if (strcmp(Word + 1, "f") == 0) {
            Shell->Lexer.TokenType = TOKEN_IF;

        } else if (strcmp(Word + 1, "n") == 0) {
            Shell->Lexer.TokenType = TOKEN_IN;
        }

        break;

    case 't':
        if (strcmp(Word + 1, "hen") == 0) {
            Shell->Lexer.TokenType = TOKEN_THEN;
        }

        break;

    case 'u':
        if (strcmp(Word + 1, "ntil") == 0) {
            Shell->Lexer.TokenType = TOKEN_UNTIL;
        }

        break;

    case 'w':
        if (strcmp(Word + 1, "hile") == 0) {
            Shell->Lexer.TokenType = TOKEN_WHILE;
        }

        break;
    }

    return;
}

BOOL
ShScanPendingHereDocuments (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine scans any pending here documents that are starting now.

Arguments:

    Shell - Supplies a pointer to the shell.

Return Value:

    TRUE on success or if there are no here documents to scan.

    FALSE on failure.

--*/

{

    PSHELL_HERE_DOCUMENT HereDocument;
    BOOL Result;

    while (LIST_EMPTY(&(Shell->Lexer.HereDocumentList)) == FALSE) {
        HereDocument = LIST_VALUE(Shell->Lexer.HereDocumentList.Next,
                                  SHELL_HERE_DOCUMENT,
                                  ListEntry);

        Result = ShScanHereDocument(Shell, HereDocument);
        if (Result == FALSE) {
            return FALSE;
        }

        LIST_REMOVE(&(HereDocument->ListEntry));
        HereDocument->ListEntry.Next = NULL;
    }

    return TRUE;
}

BOOL
ShScanHereDocument (
    PSHELL Shell,
    PSHELL_HERE_DOCUMENT HereDocument
    )

/*++

Routine Description:

    This routine scans out the contents of a here document from the shell
    input.

Arguments:

    Shell - Supplies a pointer to the shell.

    HereDocument - Supplies a pointer to the here document to fill out.

Return Value:

    TRUE on success or if there are no here documents to scan.

    FALSE on failure.

--*/

{

    ULONG BeginLineNumber;
    BOOL BeginningOfLine;
    INT Character;
    UINTN EndWordSize;
    PSHELL_LEXER_STATE Lexer;
    PSTR Line;
    UINTN LineBegin;
    UINTN LineSize;
    BOOL Result;
    INT StringDifference;
    BOOL WasBackslash;

    EndWordSize = HereDocument->EndWordSize;
    Lexer = &(Shell->Lexer);
    BeginLineNumber = Lexer->LineNumber;
    LineBegin = 0;

    //
    // This routine borrows the token buffer, so there had better be nothing
    // in it.
    //

    assert(Lexer->TokenBufferSize == 0);

    //
    // If it's going to be expanded, simulate the whole thing being in double
    // quotes so that control characters inside variable expansions get
    // escaped during expansion.
    //

    if (HereDocument->EndWordWasQuoted == FALSE) {
        Result = ShAddCharacterToTokenBuffer(Shell, SHELL_CONTROL_QUOTE);
        if (Result == FALSE) {
            return FALSE;
        }

        LineBegin = 1;
    }

    ShPrintPrompt(Shell, 2);
    WasBackslash = FALSE;
    BeginningOfLine = TRUE;
    while (TRUE) {
        Result = ShGetInputCharacter(Shell, &Character);
        if (Result == FALSE) {
            ShLexerError(Shell,
                         "Unterminated here document at line %d.\n",
                         BeginLineNumber);

            return FALSE;
        }

        if ((Character == '\n') || (Character == EOF) ||
            (Character == '\0')) {

            //
            // If there was a backslash, remove both the newline and the
            // backslash. Don't do this if the original end word was
            // quoted in any way.
            //

            if ((HereDocument->EndWordWasQuoted == FALSE) &&
                (WasBackslash != FALSE)) {

                assert(Lexer->TokenBufferSize != 0);

                Lexer->TokenBufferSize -= 1;
                WasBackslash = FALSE;
                ShPrintPrompt(Shell, 2);
                if (Character == EOF) {
                    break;
                }

                continue;
            }

            //
            // It's not a backslash, this is a complete line. It needs to
            // be checked against the ending line. Null terminate it and
            // compare strings.
            //

            Result = ShAddCharacterToTokenBuffer(Shell, '\0');
            if (Result == FALSE) {
                return FALSE;
            }

            assert(Lexer->TokenBufferSize > LineBegin);

            Line = Lexer->TokenBuffer + LineBegin;
            LineSize = Lexer->TokenBufferSize - LineBegin - 1;
            while ((LineSize != 0) && (Line[LineSize - 1] == '\r')) {
                LineSize -= 1;
            }

            if ((LineSize == 0) || (LineSize != EndWordSize - 1)) {
                StringDifference = 1;

            } else {
                StringDifference = strncmp(Line,
                                           HereDocument->EndWord,
                                           LineSize);
            }

            //
            // If the line matched, then throw out this line, as it was the
            // terminating word.
            //

            if (StringDifference == 0) {
                Lexer->TokenBufferSize = LineBegin;
                Result = ShAddCharacterToTokenBuffer(Shell, '\0');
                if (Result == FALSE) {
                    return FALSE;
                }

                HereDocument->Document =
                                 SwStringDuplicate(Lexer->TokenBuffer,
                                                   Lexer->TokenBufferSize);

                if (HereDocument->Document == NULL) {
                    return FALSE;
                }

                HereDocument->DocumentSize = Lexer->TokenBufferSize;
                Lexer->TokenBufferSize = 0;
                break;

            //
            // If it didn't match, then remove null terminator and reset the
            // line beginning to be right after the newline.
            //

            } else {

                assert(Lexer->TokenBufferSize != 0);

                LineBegin = Lexer->TokenBufferSize;
                Lexer->TokenBufferSize -= 1;
            }

            ShPrintPrompt(Shell, 2);
            BeginningOfLine = TRUE;

        //
        // If this was not an EOF, null, newline, or tab, then this is
        // not the beginning of the line.
        //

        } else if (Character != '\t') {
            BeginningOfLine = FALSE;

            //
            // Watch out for expansions.
            //

            if (HereDocument->EndWordWasQuoted == FALSE) {

                //
                // Just like in double quotes, some characters need to be
                // escaped if preceded by a backslash.
                //

                if ((Character == '$') || (Character == '`') ||
                    (Character == '\\')) {

                    if (WasBackslash != FALSE) {

                        assert(Lexer->TokenBufferSize != 0);

                        Lexer->TokenBuffer[Lexer->TokenBufferSize - 1] =
                                                      SHELL_CONTROL_ESCAPE;

                    //
                    // For unescaped $ and `, scan through an expansion.
                    //

                    } else if (Character != '\\') {
                        Result = ShScanExpansion(Shell, Character);
                        if (Result == FALSE) {
                            return FALSE;
                        }

                        continue;
                    }

                //
                // Quote the magic characters.
                //

                } else if ((Character == SHELL_CONTROL_QUOTE) ||
                           (Character == SHELL_CONTROL_ESCAPE)) {

                    Result = ShAddCharacterToTokenBuffer(Shell,
                                                         SHELL_CONTROL_ESCAPE);

                    if (Result == FALSE) {
                        return FALSE;
                    }
                }
            }
        }

        if (Character == '\\') {
            WasBackslash = !WasBackslash;

        } else {
            WasBackslash = FALSE;
        }

        if (Character == EOF) {
            return FALSE;
        }

        //
        // Potentially strip leading tabs from the beginning of every line
        // including the one with the ending word.
        //

        if ((BeginningOfLine != FALSE) &&
            (Character == '\t') && (HereDocument->StripLeadingTabs != FALSE)) {

            continue;
        }

        Result = ShAddCharacterToTokenBuffer(Shell, Character);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

VOID
ShLexerError (
    PSHELL Shell,
    PSTR Format,
    ...
    )

/*++

Routine Description:

    This routine prints a shell lexer error to standard error.

Arguments:

    Shell - Supplies a pointer to the shell.

    Format - Supplies the printf style format string.

    ... - Supplies the remaining arguments to the printf string.

Return Value:

    TRUE if the string has a quoting character in it.

    FALSE if the string is clean.

--*/

{

    va_list ArgumentList;
    PSHELL_LEXER_STATE Lexer;

    Lexer = &(Shell->Lexer);
    fprintf(stderr, "sh: %d: ", Lexer->LineNumber);
    va_start(ArgumentList, Format);
    vfprintf(stderr, Format, ArgumentList);
    va_end(ArgumentList);
    if (Lexer->TokenBufferSize != 0) {
        if (Lexer->TokenBuffer[Lexer->TokenBufferSize - 1] != '\0') {
            if (Lexer->TokenBufferCapacity > Lexer->TokenBufferSize) {
                Lexer->TokenBuffer[Lexer->TokenBufferSize] = '\0';

            } else {
                Lexer->TokenBuffer[Lexer->TokenBufferSize - 1] = '\0';
            }
        }

        fprintf(stderr, ".\nToken: %s.", Shell->Lexer.TokenBuffer);
    }

    return;
}

