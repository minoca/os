/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    regextst.c

Abstract:

    This module implements the tests for the regular expression support within
    the C library.

Author:

    Evan Green 9-Jul-2013

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "testc.h"
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the match count array size.
//

#define REGEX_TEST_MATCH_COUNT 5

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _REGEX_EXECUTION_TEST_CASE {
    PSTR Pattern;
    INT CompileFlags;
    PSTR Input;
    INT InputFlags;
    INT ExecutionResult;
    regmatch_t ExpectedMatch[REGEX_TEST_MATCH_COUNT];
} REGEX_EXECUTION_TEST_CASE, *PREGEX_EXECUTION_TEST_CASE;

typedef struct _REGEX_COMPILE_TEST_CASE {
    PSTR Pattern;
    INT CompileFlags;
    INT SubexpressionCount;
    INT Result;
} REGEX_COMPILE_TEST_CASE, *PREGEX_COMPILE_TEST_CASE;

typedef struct _REGEX_ERROR_STRING {
    INT Code;
    PSTR String;
} REGEX_ERROR_STRING, *PREGEX_ERROR_STRING;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
TestRegularExpressionExecutionCase (
    ULONG Index,
    PREGEX_EXECUTION_TEST_CASE Case
    );

BOOL
TestRegularExpressionCompileCase (
    ULONG Index,
    PREGEX_COMPILE_TEST_CASE Case
    );

PSTR
TestRegexGetErrorCodeString (
    INT Code
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the execution test cases.
//

REGEX_EXECUTION_TEST_CASE RegexExecutionTestCases[] = {

    //
    // An empty pattern should match anything.
    //

    {
        "", 0,
        "a", 0,
        0,
        {{0, 0}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // A beginning circumflex should match anything.
    //

    {
        "^", 0,
        "", 0,
        0,
        {{0, 0}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // A beginning dollar sign should match anything.
    //

    {
        "$", 0,
        "a", 0,
        0,
        {{1, 1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // A circumflex and dollar sign should match just the empty string.
    //

    {
        "^$", 0,
        "", 0,
        0,
        {{0, 0}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
    {
        "^$", 0,
        "a", 0,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Circumflexes should anchor things to the beginning and dollar signs to
    // the end.
    //

    {
        "^abc", 0,
        "aabc", 0,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
    {
        "abc$", 0,
        "abcabc", 0,
        0,
        {{3, 6}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
    {
        "^abc$", 0,
        "abc", 0,
        0,
        {{0, 3}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try a basic but comprehensive pattern.
    //

    {
        "a.cd\\(ef\\)(g)h\\{2\\}hi\\{1,4\\}ij*k*.*mno\\*\\*"
        "{}pq\\(rs\\)\\(\\1\\2\\)*^$[tuv][]xw][^ab]*z", 0,
        "00abcdef(g)hhhiiiiijjjmno**{}pqrsefrsefrs^$twxyz123", 0,
        0,
        {{2, 48}, {6, 8}, {31, 33}, {37, 41}, {-1, -1}},
    },

    //
    // Try nested subexpressions.
    //

    {
        "^\\(abcd\\(e*fg\\(hi\\(\\)j\\)\\)kl\\)\\(\\3\\)$", 0,
        "abcdeeefghijklhij", 0,
        0,
        {{0, 17}, {0, 14}, {4, 12}, {9, 12}, {11, 11}},
    },

    //
    // Try the same nested subexpression except have an outer subexpression
    // fail after the inner ones succeed to make sure those inner ones get
    // cleared out. The difference here is the last character in the input.
    //

    {
        "^\\(abcd\\(e*fg\\(hi\\(\\)j\\)\\)kl\\)\\(\\3\\)$", 0,
        "abcdeeefghijklhi", 0,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try some subexpressions that go beyond the size of the array.
    //

    {
        "+?\\(a\\)*.\\(b*\\)\\(c\\)*\\([def*]*\\)\\(g\\)\\(h\\)\\([ij]\\)*|", 0,
        "+?abbbccced**dfghiiijj|klm", 0,
        0,
        {{0, 23}, {2, 3}, {4, 6}, {8, 9}, {9, 15}},
    },

    //
    // Try the "not EOL" and "not BOL" flags.
    //

    {
        "^abc$", 0,
        "abc", REG_NOTBOL,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
    {
        "^abc$", 0,
        "abc", REG_NOTEOL,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try out the newline flag.
    //

    {
        "^abc$", REG_NEWLINE,
        "abc\nabc\nh", REG_NOTBOL | REG_NOTEOL,
        0,
        {{4, 7}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
    {
        "abc$", REG_NEWLINE,
        "abcd\nabc\n123", REG_NOTBOL | REG_NOTEOL,
        0,
        {{5, 8}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try out the no-sub flag.
    //

    {
        "^\\(a\\)\\(b\\)\\(c\\)$", REG_NOSUB,
        "abc", 0,
        0,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try out the ignore case flag.
    //

    {
        "abcdef12!!!%$^*6*\\.*4", REG_ICASE,
        "aAaaaAbCDef12!!!%$...456", 0,
        0,
        {{5, 22}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try out some bracket patterns.
    //

    {
        "\\([ABC]\\{2,6\\}\\)ABC.*\\([[:digit:]]\\).*\\([[:alpha:]]\\).*"
        "\\([[:blank:]]\\).*[[:cntrl:]].*[[:graph:]].*[[:print:]].*[[:punct:]]"
        ".*[[:space:]].*[[:upper:]].*[[:lower:]].*[[:xdigit:]]456", 0,
        "aBCAABC  7  xzz \t7   . AAz   F456", REG_NOTBOL | REG_NOTEOL,
        0,
        {{1, 33}, {1, 4}, {9, 10}, {14, 15}, {15, 16}},
    },

    //
    // Extended mode tests.
    //

    //
    // An empty pattern should match anything.
    //

    {
        "", REG_EXTENDED,
        "a", 0,
        0,
        {{0, 0}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // A beginning circumflex should match anything.
    //

    {
        "^", REG_EXTENDED,
        "", 0,
        0,
        {{0, 0}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // A beginning dollar sign should match anything.
    //

    {
        "$", REG_EXTENDED,
        "a", 0,
        0,
        {{1, 1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // A circumflex and dollar sign should match just the empty string.
    //

    {
        "^$", REG_EXTENDED,
        "", 0,
        0,
        {{0, 0}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
    {
        "^$", REG_EXTENDED,
        "a", 0,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Circumflex and dollar sign should be usable from within the regex.
    //

    {
        "f*^abc$g*", REG_EXTENDED,
        "abc", 0,
        0,
        {{0, 3}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try out some extended features.
    //

    {
        "^(ab){1,2}cd[^ef[:digit:]]+7 ?([][:digit:]]{2})", REG_EXTENDED,
        "ababcdxx7 0]", 0,
        0,
        {{0, 12}, {2, 4}, {10, 12}, {-1, -1}, {-1, -1}},
    },

    //
    // The plus should give one or more. Question mark should be zero or one.
    //

    {
        "ba+c", REG_EXTENDED,
        "bc", 0,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
    {
        "ba+cD?", REG_EXTENDED | REG_ICASE,
        "0bAAcde", 0,
        0,
        {{1, 6}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
    {
        "da?a", REG_EXTENDED | REG_ICASE,
        "ccdAa", 0,
        0,
        {{2, 5}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try out branches.
    //

    {
        "(abc)|(de(f*)|g)|h", REG_EXTENDED | REG_ICASE,
        "000deg", 0,
        0,
        {{3, 5}, {-1, -1}, {3, 5}, {5, 5}, {-1, -1}},
    },
    {
        "(abc)|(de(f*)|g)|h", REG_EXTENDED | REG_ICASE,
        "h", 0,
        0,
        {{0, 1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try out some escape characters.
    //

    {
        "(o\\(\\)o\\{\\}s\\*d\\.b\\\\q\\?\\^p\\+s\\[\\]p\\|)|a",
        REG_EXTENDED | REG_ICASE,
        "o()o{}s*d.b\\q?^p+s[]p|", 0,
        0,
        {{0, 22}, {0, 22}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
    {
        "(o\\(\\)o\\{\\}s\\*d\\.b\\\\q\\?\\^p\\+s\\[\\]p\\|)|a",
        REG_EXTENDED | REG_ICASE,
        "A", 0,
        0,
        {{0, 1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try a repeat that didn't make the minimum count.
    //

    {
        "(ab){3,5}", REG_EXTENDED | REG_ICASE,
        "abab", 0,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Dots shouldn't swallow newlines if they're on.
    //

    {
        "a*ab.+", REG_EXTENDED | REG_NEWLINE,
        "caab \ncd", 0,
        0,
        {{1, 5}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Back references should still work even with the nosub flag.
    //

    {
        "(.)bcd\\1+", REG_EXTENDED | REG_NOSUB,
        "abcdaaab", 0,
        0,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Bracket expressions should also honor the ignore case flag.
    //

    {
        "[[:lower:]][ABC][[:upper:]]", REG_EXTENDED | REG_ICASE,
        "Xcd", 0,
        0,
        {{0, 3}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Close parentheses are normal if not opened first.
    //

    {
        "1)", REG_EXTENDED,
        "(1)", 0,
        0,
        {{1, 3}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Apparently stars can override pluses, and some other
    // overrides are valid too.
    //

    {
        "0+*", REG_EXTENDED,
        "000+++", 0,
        0,
        {{0, 3}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    {
        "AS?+", REG_EXTENDED,
        "BASSS", 0,
        0,
        {{1, 5}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    {
        "AS?+", REG_EXTENDED,
        "BA", 0,
        0,
        {{1, 2}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    {
        "A*{5}", REG_EXTENDED,
        "AAAAAAAA", 0,
        0,
        {{0, 8}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    {
        "A*{5}", REG_EXTENDED,
        "B", 0,
        0,
        {{0, 0}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    {
        "(A|AB)+C", REG_EXTENDED,
        "ABABC", 0,
        0,
        {{0, 5}, {2, 4}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    {
        "(AC|A)+C+", REG_EXTENDED,
        "ACACC", 0,
        0,
        {{0, 5}, {2, 4}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // TODO: The commented out cases are what other C libraries would see.
    // This implementation finds shorter versions due to its backtracking
    // nature. Consider implementing a NFA/DFA regex implementation, which
    // would then enable these cases.
    //

#if 0

    {
        "(A|AC)+C+", REG_EXTENDED,
        "ACACC", 0,
        0,
        {{0, 5}, {2, 3}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    {
        "(A|AB){2,5}A*", REG_EXTENDED,
        "AAAABA", 0,
        0,
        {{0, 6}, {5, 6}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

#endif

    //
    // Test that backtracking properly refills subexpressions with the old
    // choices. In this case, subexpressions 1 and 2 need to be refreshed after
    // backing out of a failed third repeat.
    //

    {
        "(((A|B)|(C|D)))+D", REG_EXTENDED,
        "ACD", 0,
        0,
        {{0, 3}, {1, 2}, {1, 2}, {0, 1}, {1, 2}},
    },

    //
    // Test that repeated emptiness won't send it into conniptions.
    //

    {
        "A()*B(())+(C||)*", REG_EXTENDED,
        "AB", 0,
        0,
        {{0, 2}, {1, 1}, {2, 2}, {2, 2}, {2, 2}},
    },

    //
    // Try an open ended repeat count.
    //

    {

        "AB\\{2,\\}", 0,
        "ABBBBC", 0,
        0,
        {{0, 5}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try a beginning of word that works.
    //

    {

        "[[:<:]](AB) [[:<:]](C)", REG_EXTENDED,
        "AB C", 0,
        0,
        {{0, 4}, {0, 2}, {3, 4}, {-1, -1}, {-1, -1}},
    },

    //
    // Try a beginning of word that doesn't work.
    //

    {

        "[[:<:]]AB[[:<:]]C", 0,
        "ABC", 0,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },

    //
    // Try a beginning of word that works.
    //

    {

        "(AB)[[:>:]] (C)[[:>:]]", REG_EXTENDED,
        "AB C", 0,
        0,
        {{0, 4}, {0, 2}, {3, 4}, {-1, -1}, {-1, -1}},
    },

    //
    // Try a beginning of word that doesn't work.
    //

    {

        "[[:>:]]AB C", REG_EXTENDED,
        "AB C", 0,
        REG_NOMATCH,
        {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},
    },
};

//
// Define the compile test cases.
//

REGEX_COMPILE_TEST_CASE RegexCompileTestCases[] = {

    //
    // Some basic but cornery cases that should all compile.
    //

    {"", 0, 0, 0},
    {"$", 0, 0, 0},
    {"^^^^^$$$$$", REG_EXTENDED, 0, 0},
    {"^^^^^$$$$$", 0, 0, 0},
    {"(1)(2)(3)(4)(5)(6)(7)(8)(9)(A)\\9\\5\\1.", REG_EXTENDED, 10, 0},
    {"a{    0,    0   }", REG_EXTENDED, 10, REG_BADBR},
    {"[[:alpha:][:alnum:][:blank:][:cntrl:][:digit:][:graph:][:lower:]"
     "[:print:][:punct:][:space:][:upper:][:xdigit:]]", 0, 0},

    {"]]]", REG_EXTENDED, 0},
    {"(((((((((((((((((((((((((((((())))))))))))))))))))))))))))))",
     REG_EXTENDED, 30, 0},

    {")", REG_EXTENDED, 0, 0},
    {"\\(abc\\(d*e\\(f*\\)g\\)dd\\)\\(\\)", 0, 4, 0},
    {"\\(abc\\(d*e\\(f*\\)g\\)dd\\)\\(\\)", REG_EXTENDED, 0, 0},

    //
    // Back references are only valid between 1 and 9, and must already have a
    // valid subexpression.
    //

    {"(asdf)\\2", 0, 1, REG_ESUBREG},
    {"(asdf)\\99", 0, 1, REG_ESUBREG},

    //
    // Invalid braces.
    //

    {"a{asdf}", REG_EXTENDED, 0, REG_BADBR},
    {"a{4,,}", REG_EXTENDED, 0, REG_BADBR},
    {"a{0,-3}", REG_EXTENDED, 0, REG_BADBR},
    {"a{-999}", REG_EXTENDED, 0, REG_BADBR},
    {"a{-1,-3}", REG_EXTENDED, 0, REG_BADBR},
    {"a{6000, ASDF}", REG_EXTENDED, 0, REG_BADBR},
    {"a{ 4 , 4 ,}", REG_EXTENDED, 0, REG_BADBR},
    {"a{5,3}", REG_EXTENDED, 0, REG_BADBR},

    //
    // Parentheses imbalance.
    //

    {"(1((3))\\)", REG_EXTENDED, 0, REG_EPAREN},
    {"(1", REG_EXTENDED, 0, REG_EPAREN},
    {"\\(2", 0, 0, REG_EPAREN},

    //
    // Bad character class.
    //

    {"[[:poopy:]]", REG_EXTENDED, 0, REG_ECTYPE},
    {"[[:ALPHA:]]", REG_EXTENDED, 0, REG_ECTYPE},

    //
    // Bad brackets.
    //

    {"[[:alpha:]", REG_EXTENDED, 0, REG_EBRACK},
    {"[]asdf", REG_EXTENDED, 0, REG_EBRACK},

    //
    // Trailing escape.
    //

    {"asdf\\", REG_EXTENDED, 0, REG_EESCAPE},

    //
    // Bad repeat.
    //

    {"*", REG_EXTENDED, 0, REG_BADRPT},
    {"*?", REG_EXTENDED, 0, REG_BADRPT},
    {"??", REG_EXTENDED, 0, REG_BADRPT},
    {"{6}", REG_EXTENDED, 0, REG_BADRPT},
    {"+", REG_EXTENDED, 0, REG_BADRPT},
};

REGEX_ERROR_STRING RegexErrorStrings[] = {
    {0, "SUCCESS"},
    {REG_NOMATCH, "REG_NOMATCH"},
    {REG_BADPAT, "REG_BADPAT"},
    {REG_ECOLLATE, "REG_ECOLLATE"},
    {REG_ECTYPE, "REG_ECTYPE"},
    {REG_EESCAPE, "REG_EESCAPE"},
    {REG_ESUBREG, "REG_ESUBREG"},
    {REG_EBRACK, "REG_EBRACK"},
    {REG_EPAREN, "REG_EPAREN"},
    {REG_BADBR, "REG_BADBR"},
    {REG_ERANGE, "REG_ERANGE"},
    {REG_ESPACE, "REG_ESPACE"},
    {REG_BADRPT, "REG_BADRPT"}
};

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestRegularExpressions (
    VOID
    )

/*++

Routine Description:

    This routine implements the entry point for the regular expression tests.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;
    BOOL Result;
    ULONG TestCount;
    ULONG TestIndex;

    Failures = 0;

    //
    // Run the compile tests.
    //

    TestCount = sizeof(RegexCompileTestCases) /
                sizeof(RegexCompileTestCases[0]);

    for (TestIndex = 0; TestIndex < TestCount; TestIndex += 1) {
        Result = TestRegularExpressionCompileCase(
                                        TestIndex,
                                        &(RegexCompileTestCases[TestIndex]));

        if (Result == FALSE) {
            Failures += 1;
        }
    }

    //
    // Run the execution tests.
    //

    TestCount = sizeof(RegexExecutionTestCases) /
                sizeof(RegexExecutionTestCases[0]);

    for (TestIndex = 0; TestIndex < TestCount; TestIndex += 1) {
        Result = TestRegularExpressionExecutionCase(
                                        TestIndex,
                                        &(RegexExecutionTestCases[TestIndex]));

        if (Result == FALSE) {
            printf("Case %d Failed\n", TestIndex);
            Failures += 1;
        }
    }

    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
TestRegularExpressionExecutionCase (
    ULONG Index,
    PREGEX_EXECUTION_TEST_CASE Case
    )

/*++

Routine Description:

    This routine performs a regular expression execution test.

Arguments:

    Index - Supplies the test case number.

    Case - Supplies a pointer to the test case.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    regex_t Expression;
    regmatch_t Match[REGEX_TEST_MATCH_COUNT];
    size_t MatchIndex;
    int Result;
    BOOL Status;

    Status = FALSE;
    for (MatchIndex = 0; MatchIndex < REGEX_TEST_MATCH_COUNT; MatchIndex += 1) {
        Match[MatchIndex].rm_so = -1;
        Match[MatchIndex].rm_eo = -1;
    }

    //
    // Compile the regular expression.
    //

    Result = regcomp(&Expression, Case->Pattern, Case->CompileFlags);
    if (Result != 0) {
        printf("Error: Failed to compile regex \"%s\".\n", Case->Pattern);
        goto TestRegularExpressionCaseEnd;
    }

    //
    // Run the test case.
    //

    Result = regexec(&Expression,
                     Case->Input,
                     REGEX_TEST_MATCH_COUNT,
                     Match,
                     Case->InputFlags);

    if (Result != Case->ExecutionResult) {
        printf("Error: regexec returned %d instead of expected result %d.\n",
               Result,
               Case->ExecutionResult);
    }

    //
    // Compare the matches.
    //

    Status = TRUE;
    for (MatchIndex = 0; MatchIndex < REGEX_TEST_MATCH_COUNT; MatchIndex += 1) {
        if ((Match[MatchIndex].rm_so !=
             Case->ExpectedMatch[MatchIndex].rm_so) ||
            (Match[MatchIndex].rm_eo !=
             Case->ExpectedMatch[MatchIndex].rm_eo)) {

            printf("Error: Regex test match %d failed.\n", MatchIndex);
            Status = FALSE;
        }
    }

TestRegularExpressionCaseEnd:
    if (Status == FALSE) {
        printf("Regex test %d failed.\n"
               "Pattern: \"%s\", Flags 0x%x.\n"
               "Input: \"%s\", len %d, Flags 0x%x.\n"
               "Ruler:  0        1         2         3        4         5\n"
               "Expected Result: %d.\n",
               Index,
               Case->Pattern,
               Case->CompileFlags,
               Case->Input,
               strlen(Case->Input),
               Case->InputFlags,
               Case->ExecutionResult);

        for (MatchIndex = 0;
             MatchIndex < REGEX_TEST_MATCH_COUNT;
             MatchIndex += 1) {

            printf("Match %d: Expected {%d, %d}, got {%d, %d}\n",
                   MatchIndex,
                   Case->ExpectedMatch[MatchIndex].rm_so,
                   Case->ExpectedMatch[MatchIndex].rm_eo,
                   Match[MatchIndex].rm_so,
                   Match[MatchIndex].rm_eo);
        }

        printf("------------------------------------\n");
    }

    regfree(&Expression);
    return Status;
}

BOOL
TestRegularExpressionCompileCase (
    ULONG Index,
    PREGEX_COMPILE_TEST_CASE Case
    )

/*++

Routine Description:

    This routine performs a regular expression compile test.

Arguments:

    Index - Supplies the test case number.

    Case - Supplies a pointer to the test case.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    regex_t Expression;
    INT Result;
    BOOL Status;

    Status = TRUE;
    Expression.re_nsub = 0;
    Result = regcomp(&Expression, Case->Pattern, Case->CompileFlags);
    if ((Result != Case->Result) ||
        ((Result == 0) && (Expression.re_nsub != Case->SubexpressionCount))) {

        printf("Regex compile test case %d failed.\n"
               "Pattern: \"%s\", Flags 0x%x.\n"
               "Expected Result %d (%s), got %d (%s).\n"
               "Expected %d subexpressions, got %d.\n",
               Index,
               Case->Pattern,
               Case->CompileFlags,
               Case->Result,
               TestRegexGetErrorCodeString(Case->Result),
               Result,
               TestRegexGetErrorCodeString(Result),
               (INT)Case->SubexpressionCount,
               (INT)Expression.re_nsub);

        Status = FALSE;
    }

    if (Result == 0) {
        regfree(&Expression);
    }

    return Status;
}

PSTR
TestRegexGetErrorCodeString (
    INT Code
    )

/*++

Routine Description:

    This routine returns the string version of the given error code.

Arguments:

    Code - Supplies the REG_* error code (or zero).

Return Value:

    Returns a string of that error code that can be printed.

--*/

{

    ULONG Count;
    ULONG Index;

    Count = sizeof(RegexErrorStrings) / sizeof(RegexErrorStrings[0]);
    for (Index = 0; Index < Count; Index += 1) {
        if (RegexErrorStrings[Index].Code == Code) {
            return RegexErrorStrings[Index].String;
        }
    }

    return "Unknown Error";
}

