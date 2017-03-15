/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testrtl.c

Abstract:

    This module implements the test cases for the kernel runtime library.

Author:

    Evan Green 24-Jul-2012

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#define RTL_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <wchar.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MAX_OUTPUT 1000

//
// Print format test values
//

#define BASIC_STRING "hello there!\n"
#define FORMATTED_STRING "%d.%+#08x.% #o %-#6o#%-#8.4i+0x%X\n" \
                         "%llX%c%s%-11c%5s%%%I64x", \
                         10203, 0x8888432a, 7, 0, -12, 0xabcd, \
                         0x12345678ABCDDCBAULL, 'h', "ello %s there!", \
                         'X', "str", 0x123456789ABCDEF0ULL

#define FORMATTED_STRING_RESULT "10203.+0x8888432a. 07 0     #-0012   " \
                                "+0xABCD\n" \
                                "12345678ABCDDCBAhello %s there!" \
                                "X            str%123456789abcdef0"

#define FORMATTED_STRING_POSITIONAL_ARGUMENTS \
    "%4$ *3$.*2$hhi; %5$x; %5$lu; %6$llx; %8$-8.*7$c; ; %1$-o", \
    6, 4, 8, -1, 0xFF, 0x1FFFFEEEEULL, 8, 'a'

#define FORMATTED_STRING_POSITIONAL_RESULT \
    "   -0001; ff; 255; 1ffffeeee; a       ; ; 6"

#define PRINT_FLOAT_FORMAT               \
    "% 1f %5F % e %+#E %+g %.7G\n"       \
    "% 030F\n"                           \
    "%f %15g % 15g %+15E\n"              \
    "%8.0G %8.0G %+#5.0G\n"              \
    "%5f % 6F %5g %5.0G %5e %5.0E\n"     \
    "%5f % 6F %5g %5.0G %5e %5.0E\n"     \
    "%10.0E %10.1E %#+.010E % 010.0E\n"  \
    "%015f %-15f %-15.3f %15f\n"         \
    "%f %f %f %f\n"                      \
    "%50.30f\n"                          \
    "%#8.1g %#8.0g %5.3g %5.3g %5.3g\n"  \
    "%5.3g %5.3g %3.2g %6.4g %10.4g\n"   \
    "%+1.1g %.30g %g %G x%-012.9gx\n"    \
    "%f %.f %.1f\n"                      \

#define PRINT_FLOAT_ARGUMENTS                           \
    INFINITY, -INFINITY, NAN, NAN, INFINITY, -INFINITY, \
    -123.0000013,                                       \
    123456000.0, 0.0001234565, 0.00000001234567,        \
    1.234, 1.999, 0.9, 0.9,                             \
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0,                       \
    -0.0, -0.0, -0.0, -0.0, -0.0, -0.0,                 \
    3.4E100, 2.66E299, -6.8E-100, -6.9E-299,            \
    1.0, 2.0, -0.9, -0.99,                              \
    9.9999999E10, 9.999999E6, 9.999999E0, 9.999999E-1,  \
    -9.9999E-20,                                        \
    100.0, 100.0, 100.0, 10.0, 10.0,                    \
    101.0, 10.1, 10.1, 0.01, 0.0123457,                 \
    0.99, 515.0, 1e34, 1234567.89, 12345.6789,          \
    1234.0, 1234.0, 1234.0

#define PRINT_FLOAT_RESULT \
    " inf  -INF  nan +NAN +inf -INF\n" \
    "-0000000000000000000123.000001\n" \
    "123456000.000000     0.000123457     1.23457e-08   +1.234000E+00\n" \
    "       2      0.9  +0.9\n" \
    "0.000000  0.000000     0     0 0.000000e+00 0E+00\n" \
    "-0.000000 -0.000000    -0    -0 -0.000000e+00 -0E+00\n" \
    "    3E+100   2.7E+299 -6.8000000000E-100 -0007E-299\n" \
    "00000001.000000 2.000000        -0.900                -0.990000\n" \
    "99999999000.000000 9999999.000000 9.999999 1.000000\n" \
    "                 -0.000000000000000000099999000000\n" \
    "  1.e+02   1.e+02   100    10    10\n" \
    "  101  10.1  10   0.01    0.01235\n" \
    "+1 515 1e+34 1.23457E+06 x12345.6789  x\n" \
    "1234.000000 1234 1234.0\n"

#define PRINT_HEX_FLOAT_FORMAT \
    "%6A %6a %6A %6a %6a\n" \
    "%10a %10A %10.3a\n" \
    "%10.1A %10.0a\n" \
    "%30a %30A %15.3a\n" \
    "%10.1A %30.0a\n" \
    "% a % a %+015.1a\n" \
    "%+01.40a %#A\n" \
    "%20a %10.1a %10.0a\n" \

#define PRINT_HEX_FLOAT_ARGUMENTS \
    0.0, -0.0, INFINITY, -INFINITY, NAN, \
    -1.1, 1.1, -1.1, \
    1.1, -1.1, \
    -0.6, 0.6E-100, 123456.789, \
    5.9, -1.1, \
    256.0, -1.03125, -0.0, \
    1.03125, 1.001953125, \
    1.999999999, 1.999999999, 1.999999999 \

#define PRINT_HEX_FLOAT_RESULT \
    "0X0P+0 -0x0p+0    INF   -inf    nan\n" \
    "-0x1.199999999999ap+0 0X1.199999999999AP+0 -0x1.19ap+0\n" \
    "  0X1.2P+0    -0x1p+0\n" \
    "         -0x1.3333333333333p-1         0X1.0CC4F55EECFEAP-333" \
    "     0x1.e24p+16\n" \
    "  0X1.8P+2                        -0x1p+0\n" \
    " 0x1p+8 -0x1.08p+0 -0x0000000.0p+0\n" \
    "+0x1.0800000000000000000000000000000000000000p+0 0X1.008P+0\n" \
    " 0x1.fffffffbb47dp+0   0x2.0p+0     0x2p+0\n" \

//
// Wide print format test values
//

#define BASIC_STRING_WIDE L"hello there!\n"
#define FORMATTED_STRING_WIDE L"%d.%+#08x.% #o %-#6o#%-#8.4i+0x%X\n" \
                              L"%llX%C%S%-11C%5S%%%I64x", \
                              10203, 0x8888432a, 7, 0, -12, 0xabcd, \
                              0x12345678ABCDDCBAULL, L'h', L"ello %s there!", \
                              L'X', L"str", 0x123456789ABCDEF0ULL

#define FORMATTED_STRING_RESULT_WIDE L"10203.+0x8888432a. 07 0     #-0012   " \
                                     L"+0xABCD\n" \
                                     L"12345678ABCDDCBAhello %s there!" \
                                     L"X            str%123456789abcdef0"

#define FORMATTED_STRING_POSITIONAL_ARGUMENTS_WIDE \
    L"%4$ *3$.*2$hhi; %5$x; %5$lu; %6$llx; %8$-8.*7$C; ; %1$-o", \
    6, 4, 8, -1, 0xFF, 0x1FFFFEEEEULL, 8, L'a'

#define FORMATTED_STRING_POSITIONAL_RESULT_WIDE \
    L"   -0001; ff; 255; 1ffffeeee; a       ; ; 6"

#define PRINT_FLOAT_FORMAT_WIDE           \
    L"% 1f %5F % e %+#E %+g %.7G\n"       \
    L"% 030F\n"                           \
    L"%f %15g % 15g %+15E\n"              \
    L"%8.0G %8.0G %+#5.0G\n"              \
    L"%5f % 6F %5g %5.0G %5e %5.0E\n"     \
    L"%5f % 6F %5g %5.0G %5e %5.0E\n"     \
    L"%10.0E %10.1E %#+.010E % 010.0E\n"  \
    L"%015f %-15f %-15.3f %15f\n"         \
    L"%f %f %f %f\n"                      \
    L"%50.30f\n"                          \
    L"%#8.1g %#8.0g %5.3g %5.3g %5.3g\n"  \
    L"%5.3g %5.3g %3.2g %6.4g %10.4g\n"   \
    L"%+1.1g %.30g %g %G x%-012.9gx\n"    \
    L"%f %.f %.1f\n"                      \

#define PRINT_FLOAT_WIDE_ARGUMENTS                      \
    INFINITY, -INFINITY, NAN, NAN, INFINITY, -INFINITY, \
    -123.0000013,                                       \
    123456000.0, 0.0001234565, 0.00000001234567,        \
    1.234, 1.999, 0.9, 0.9,                             \
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0,                       \
    -0.0, -0.0, -0.0, -0.0, -0.0, -0.0,                 \
    3.4E100, 2.66E299, -6.8E-100, -6.9E-299,            \
    1.0, 2.0, -0.9, -0.99,                              \
    9.9999999E10, 9.999999E6, 9.999999E0, 9.999999E-1,  \
    -9.9999E-20,                                        \
    100.0, 100.0, 100.0, 10.0, 10.0,                    \
    101.0, 10.1, 10.1, 0.01, 0.0123457,                 \
    0.99, 515.0, 1e34, 1234567.89, 12345.6789,          \
    1234.0, 1234.0, 1234.0

#define PRINT_FLOAT_RESULT_WIDE \
    L" inf  -INF  nan +NAN +inf -INF\n" \
    L"-0000000000000000000123.000001\n" \
    L"123456000.000000     0.000123457     1.23457e-08   +1.234000E+00\n" \
    L"       2      0.9  +0.9\n" \
    L"0.000000  0.000000     0     0 0.000000e+00 0E+00\n" \
    L"-0.000000 -0.000000    -0    -0 -0.000000e+00 -0E+00\n" \
    L"    3E+100   2.7E+299 -6.8000000000E-100 -0007E-299\n" \
    L"00000001.000000 2.000000        -0.900                -0.990000\n" \
    L"99999999000.000000 9999999.000000 9.999999 1.000000\n" \
    L"                 -0.000000000000000000099999000000\n" \
    L"  1.e+02   1.e+02   100    10    10\n" \
    L"  101  10.1  10   0.01    0.01235\n" \
    L"+1 515 1e+34 1.23457E+06 x12345.6789  x\n" \
    L"1234.000000 1234 1234.0\n"

#define PRINT_HEX_FLOAT_FORMAT_WIDE \
    L"%6A %6a %6A %6a %6a\n" \
    L"%10a %10A %10.3a\n" \
    L"%10.1A %10.0a\n" \
    L"%30a %30A %15.3a\n" \
    L"%10.1A %30.0a\n" \
    L"% a % a %+015.1a\n" \
    L"%+01.40a %#A\n" \
    L"%20a %10.1a %10.0a\n" \

#define PRINT_HEX_FLOAT_RESULT_WIDE \
    L"0X0P+0 -0x0p+0    INF   -inf    nan\n" \
    L"-0x1.199999999999ap+0 0X1.199999999999AP+0 -0x1.19ap+0\n" \
    L"  0X1.2P+0    -0x1p+0\n" \
    L"         -0x1.3333333333333p-1         0X1.0CC4F55EECFEAP-333" \
    L"     0x1.e24p+16\n" \
    L"  0X1.8P+2                        -0x1p+0\n" \
    L" 0x1p+8 -0x1.08p+0 -0x0000000.0p+0\n" \
    L"+0x1.0800000000000000000000000000000000000000p+0 0X1.008P+0\n" \
    L" 0x1.fffffffbb47dp+0   0x2.0p+0     0x2p+0\n" \

#define TEST_NODE_COUNT 5000

//
// Define test cases for scanning strings into integers.
//

#define SCAN_STRING_BLANK "      "
#define SCAN_EMPTY_STRING ""
#define SCAN_INVALID_STRING "-a"
#define SCAN_0X_STRING "0x"
#define SCAN_DECIMAL_INTEGER "  123456789123456789  "
#define SCAN_DECIMAL_INTEGER_LENGTH 20
#define SCAN_DECIMAL_INTEGER_VALUE 123456789123456789LL
#define SCAN_OCTAL_INTEGER "+076550999"
#define SCAN_OCTAL_INTEGER_LENGTH 7
#define SCAN_OCTAL_INTEGER_VALUE 32104
#define SCAN_HEX_INTEGER "\t\v\n-0xFAB90165cfG"
#define SCAN_HEX_INTEGER_LENGTH 16
#define SCAN_HEX_INTEGER_VALUE 0xFFFFFF0546FE9A31ULL
#define SCAN_BASE35_INTEGER "yCZ"
#define SCAN_BASE35_INTEGER_LENGTH 2
#define SCAN_BASE35_INTEGER_VALUE 1202
#define SCAN_0XZ "0xz"
#define SCAN_0XZ_LENGTH 1
#define SCAN_0XZ_VALUE 0
#define SCAN_ZERO "0"
#define SCAN_ZERO_LENGTH 1
#define SCAN_ZERO_VALUE 0

//
// Define test cases for the generic string format scanner.
//

#define SCAN_BASIC_INPUT "AB%  -123CD EFG H 0x12345678 ASDF]GH "
#define SCAN_BASIC_FORMAT "AB%%%d%2c%s H%6i56%*c%*c%n %200[]DSFAH] "
#define SCAN_BASIC_ITEM_COUNT 5
#define SCAN_BASIC_INTEGER1 -123
#define SCAN_BASIC_STRING1 "CD"
#define SCAN_BASIC_STRING2 "EFG"
#define SCAN_BASIC_INTEGER2 0x1234
#define SCAN_BASIC_BYTES_SO_FAR 28
#define SCAN_BASIC_STRING3 "ASDF]"

#define SCAN_INTEGERS_INPUT "65535 0x123456 40000000001\t0FFFFFFFFfffeffff\n" \
                            "0xABCDEF90ABCDEF99 0 0 257 0"

#define SCAN_INTEGERS_FORMAT "%hhd %hi %lo %llx %jx %zu %ti %1hhd57 %*2lo%n"
#define SCAN_INTEGERS_ITEM_COUNT 8
#define SCAN_INTEGERS_INTEGER1 0xFF
#define SCAN_INTEGERS_INTEGER2 0x3456
#define SCAN_INTEGERS_INTEGER3 1
#define SCAN_INTEGERS_INTEGER4 0xFFFFFFFFFFFEFFFFULL
#define SCAN_INTEGERS_INTEGER5 0xABCDEF90ABCDEF99ULL
#define SCAN_INTEGERS_INTEGER6 0
#define SCAN_INTEGERS_INTEGER7 0
#define SCAN_INTEGERS_INTEGER8 2
#define SCAN_INTEGERS_BYTES_SO_FAR 73

#define SCAN_SET_FORMAT "%1[123]21  %[^p]pA %2[]]] %[^]*]%[]* ]"
#define SCAN_SET_INPUT "321 ANDPpA  ]]] as[*] D"
#define SCAN_SET_STRING1 "3"
#define SCAN_SET_STRING2 "ANDP"
#define SCAN_SET_STRING3 "]]"
#define SCAN_SET_STRING4 "as["
#define SCAN_SET_STRING5 "*] "

#define SCAN_DUMMY_INPUT "abcd"

#define SCAN_DOUBLE_FORMAT                                  \
    "%lF %lG %la %la %lE\n"                                 \
    "%lE %lF %le %lf\n"                                     \
    "%lg %lg %lg %le\n"                                     \
    "%la %6la%4la %le\n"                                    \
    "%lf %lf\n"                                             \
    "%la %le %lf %lg %lg\n"                                 \
    "%lf %lg %lG\n"                                         \

#define SCAN_DOUBLE_INPUT                                   \
    "inf -inf INFINITY -INFINIty nan\n"                     \
    "0.0 -0.0 0.1 -0.1\n"                                   \
    "2.0 123456.7899 1230000.113 3.123543321123E-176\n"     \
    "-0.7777 123.45678.9 +00000000000001.00000000e+0003\n"  \
    "-0.00000000000000012345678988 -9999.9\n"               \
    "-0xf234.008p-23 0x0.0p0 0x0 -0x0 -0x0p-0\n"            \
    "0x1.CCCCCCCCCCCCDP-1 0x1.3BE9595FEDA67P+3 0xF\n"

#define SCAN_DOUBLE_COUNT 27

//
// Wide scanner values
//

//
// Define test cases for scanning strings into integers.
//

#define SCAN_STRING_BLANK_WIDE L"      "
#define SCAN_EMPTY_STRING_WIDE L""
#define SCAN_INVALID_STRING_WIDE L"-a"
#define SCAN_0X_STRING_WIDE L"0x"
#define SCAN_DECIMAL_INTEGER_WIDE L"  123456789123456789  "
#define SCAN_OCTAL_INTEGER_WIDE L"+076550999"
#define SCAN_HEX_INTEGER_WIDE L"\t\v\n-0xFAB90165cfG"
#define SCAN_BASE35_INTEGER_WIDE L"yCZ"
#define SCAN_0XZ_WIDE L"0xz"
#define SCAN_ZERO_WIDE L"0"

//
// Define test cases for the generic string format scanner.
//

#define SCAN_BASIC_INPUT_WIDE L"AB%  -123CD EFG H 0x12345678 ASDF]GH "
#define SCAN_BASIC_FORMAT_WIDE L"AB%%%d%2C%S H%6i56%*C%*C%n %200l[]DSFAH] "
#define SCAN_BASIC_STRING1_WIDE L"CD"
#define SCAN_BASIC_STRING2_WIDE L"EFG"
#define SCAN_BASIC_STRING3_WIDE L"ASDF]"

#define SCAN_INTEGERS_INPUT_WIDE \
    L"65535 0x123456 40000000001\t0FFFFFFFFfffeffff\n" \
    L"0xABCDEF90ABCDEF99 0 0 257 0"

#define SCAN_INTEGERS_FORMAT_WIDE \
    L"%hhd %hi %lo %llx %jx %zu %ti %1hhd57 %*2lo%n"

#define SCAN_SET_FORMAT_WIDE L"%1l[123]21  %l[^p]pA %2l[]]] %l[^]*]%l[]* ]"
#define SCAN_SET_INPUT_WIDE L"321 ANDPpA  ]]] as[*] D"
#define SCAN_SET_STRING1_WIDE L"3"
#define SCAN_SET_STRING2_WIDE L"ANDP"
#define SCAN_SET_STRING3_WIDE L"]]"
#define SCAN_SET_STRING4_WIDE L"as["
#define SCAN_SET_STRING5_WIDE L"*] "

#define SCAN_DUMMY_INPUT_WIDE L"abcd"

#define SCAN_DOUBLE_FORMAT_WIDE                              \
    L"%lF %lG %la %la %lE\n"                                 \
    L"%lE %lF %le %lf\n"                                     \
    L"%lg %lg %lg %le\n"                                     \
    L"%la %6la%4la %le\n"                                    \
    L"%lf %lf\n"                                             \
    L"%la %le %lf %lg %lg\n"                                 \
    L"%lf %lg %lG\n"                                         \

#define SCAN_DOUBLE_INPUT_WIDE                               \
    L"inf -inf INFINITY -INFINIty nan\n"                     \
    L"0.0 -0.0 0.1 -0.1\n"                                   \
    L"2.0 123456.7899 1230000.113 3.123543321123E-176\n"     \
    L"-0.7777 123.45678.9 +00000000000001.00000000e+0003\n"  \
    L"-0.00000000000000012345678988 -9999.9\n"               \
    L"-0xf234.008p-23 0x0.0p0 0x0 -0x0 -0x0p-0\n"            \
    L"0x1.CCCCCCCCCCCCDP-1 0x1.3BE9595FEDA67P+3 0xF\n"

//
// The scanner isn't perfect as it doesn't handle rounding very well.
//

#define SCAN_DOUBLE_PLAY 11

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _TEST_RED_BLACK_TREE_NODE {
    ULONG Value;
    RED_BLACK_TREE_NODE TreeNode;
} TEST_RED_BLACK_TREE_NODE, *PTEST_RED_BLACK_TREE_NODE;

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
TestHeaps (
    BOOL Quiet
    );

ULONG
TestSoftFloatSingle (
    VOID
    );

ULONG
TestSoftFloatDouble (
    VOID
    );

ULONG
TestTime (
    VOID
    );

ULONG
TestRedBlackTrees (
    BOOL Quiet
    );

COMPARISON_RESULT
TestCompareRedBlackTreeNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

VOID
TestPrintRedBlackTreeNode (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    ULONG Level,
    PVOID Context
    );

VOID
TestRedBlackTreeVerifyInOrderTraversal (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    ULONG Level,
    PVOID Context
    );

ULONG
TestFullRedBlackTreeQueries (
    PRED_BLACK_TREE Tree
    );

ULONG
TestEmptyRedBlackTreeQueries (
    PRED_BLACK_TREE Tree
    );

ULONG
TestRedBlackTreeNodesBlank (
    PTEST_RED_BLACK_TREE_NODE Node,
    ULONG StartIndex,
    ULONG EndIndex
    );

ULONG
TestScanInteger (
    BOOL Quiet
    );

ULONG
TestScanDouble (
    VOID
    );

ULONG
TestStringScanner (
    VOID
    );

ULONG
TestScanIntegerWide (
    BOOL Quiet
    );

ULONG
TestScanDoubleWide (
    VOID
    );

ULONG
TestStringScannerWide (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

CHAR PrintOutput[MAX_OUTPUT];
WCHAR WidePrintOutput[MAX_OUTPUT];
TEST_RED_BLACK_TREE_NODE TestRedBlackTreeNodes[TEST_NODE_COUNT];

ULONG NextExpectedRedBlackTreeValue = 0;

double TestScanDoubleValues[SCAN_DOUBLE_COUNT] = {
    INFINITY,
    -INFINITY,
    INFINITY,
    -INFINITY,
    NAN,
    0x0p+0,
    -0x0p+0,
    0x1.999999999999ap-4,
    -0x1.999999999999ap-4,
    0x1p+1,
    0x1.e240ca36e2eb2p+16,
    0x1.2c4b01ced9168p+20,
    0x1.fa4bea99e4f3ap-584,
    -0x1.8e2eb1c432ca5p-1,
    0x1.edccccccccccdp+6,
    0x1.53p+9,
    0x1.2p+3,
    0x1.f4p+9,
    -0x1.1cac069c90c0dp-53,
    -0x1.387f333333333p+13,
    -0x1.e46801p-8,
    0x0p+0,
    0x0p+0,
    -0x0p+0,
    -0x0p+0,
    0x1.ccccccccccccdp-1,
    0x1.3be9595feda67p+3,
};

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the entry point for the RTL test program. It executes the
        tests.

Arguments:

    ArgumentCount - Supplies the number of arguments specified on the command
        line.

    Arguments - Supplies an array of strings representing the command line
        arguments.

Return Value:

    returns 0 on success, or nonzero on failure.

--*/

{

    int BytesPrinted;
    ULONGLONG Dividend;
    ULONGLONG Divisor;
    ULONGLONG Quotient;
    ULONGLONG Remainder;
    LONGLONG SignedDividend;
    LONGLONG SignedDivisor;
    LONGLONG SignedQuotient;
    LONGLONG SignedRemainder;
    ULONG StringLength;
    ULONG TestsFailed;

    srand(time(NULL));
    TestsFailed = 0;
    TestsFailed += TestSoftFloatSingle();
    TestsFailed += TestSoftFloatDouble();
    TestsFailed += TestTime();
    TestsFailed += TestHeaps(TRUE);

    //
    // Test basic unsigned division.
    //

    Dividend = 21;
    Divisor = 5;
    Quotient = RtlDivideUnsigned64(Dividend, Divisor, &Remainder);
    if ((Quotient != 4) || (Remainder != 1)) {
        printf("Error: Unsigned divide of %lld/%lld returned %lld, "
               "remainder %lld.\n",
               Dividend,
               Divisor,
               Quotient,
               Remainder);

        TestsFailed += 1;
    }

    //
    // Test division with NULL arguments.
    //

    RtlDivideUnsigned64(Dividend, Divisor, NULL);

    //
    // Test division with the high 32-bits set.
    //

    Dividend = 0x1000000000ULL;
    Divisor = 0x100000000ULL;
    Quotient = RtlDivideUnsigned64(Dividend, Divisor, &Remainder);
    if ((Quotient != 0x10) || (Remainder != 0)) {
        printf("Error: Unsigned divide of %lld/%lld returned %lld, "
               "remainder %lld.\n",
               Dividend,
               Divisor,
               Quotient,
               Remainder);

        TestsFailed += 1;
    }

    //
    // Test high division with a remainder.
    //

    Dividend = 0x1000000000ULL;
    Divisor = 11;
    Quotient = RtlDivideUnsigned64(Dividend, Divisor, &Remainder);
    if ((Quotient != 0x1745D1745ULL) || (Remainder != 9)) {
        printf("Error: Unsigned divide of 0x%llx/0x%llx returned 0x%llx, "
               "remainder 0x%llx.\n",
               Dividend,
               Divisor,
               Quotient,
               Remainder);

        TestsFailed += 1;
    }

    //
    // Test basic signed division.
    //

    SignedDividend = -21;
    SignedDivisor = 5;
    SignedQuotient = RtlDivideModulo64(SignedDividend,
                                       SignedDivisor,
                                       &SignedRemainder);

    if ((SignedQuotient != -4) || (SignedRemainder != -1)) {
        printf("Error: Signed divide of %lld/%lld returned %lld, "
               "remainder %lld.\n",
               SignedDividend,
               SignedDivisor,
               SignedQuotient,
               SignedRemainder);

        TestsFailed += 1;
    }

    SignedDividend = 2000;
    SignedDivisor = -3;
    SignedQuotient = RtlDivideModulo64(SignedDividend,
                                       SignedDivisor,
                                       &SignedRemainder);

    if ((SignedQuotient != -666) || (SignedRemainder != 2)) {
        printf("Error: Signed divide of %lld/%lld returned %lld, "
               "remainder %lld.\n",
               SignedDividend,
               SignedDivisor,
               SignedQuotient,
               SignedRemainder);

        TestsFailed += 1;
    }

    //
    // Test basic print, no formatting, no output.
    //

    StringLength = RtlPrintToString(NULL,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    BASIC_STRING);

    if (StringLength != strlen(BASIC_STRING) + 1) {
        printf("Error: Print basic string with NULL output returned output "
               "length of %d, should have been %lu.\n",
               StringLength,
               (long)strlen(BASIC_STRING) + 1);

        TestsFailed += 1;
    }

    //
    // Test basic print, no formatting, with output.
    //

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    BASIC_STRING);

    if (StringLength != strlen(BASIC_STRING) + 1) {
        printf("Error: Print basic string with no output returned output "
               "length of %d, should have been %lu.\n",
               StringLength,
               (long)strlen(BASIC_STRING) + 1);

        TestsFailed += 1;
    }

    if (strcmp(PrintOutput, BASIC_STRING) != 0) {
        printf("Error: Print basic string failed:\nOutput : %s\nCorrect: %s\n",
               PrintOutput,
               BASIC_STRING);

        TestsFailed += 1;
    }

    //
    // Test more complicated formatting.
    //

    StringLength = RtlPrintToString(NULL,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    FORMATTED_STRING);

    if (StringLength != strlen(FORMATTED_STRING_RESULT) + 1) {
        printf("Error: Print formatted string with no output returned output "
               "length of %d, should have been %lu.\n",
               StringLength,
               (long)strlen(FORMATTED_STRING_RESULT) + 1);

        TestsFailed += 1;
    }

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    FORMATTED_STRING);

    if (StringLength != strlen(FORMATTED_STRING_RESULT) + 1) {
        printf("Error: Print formatted string with output returned output "
               "length of %d, should have been %lu.\n",
               StringLength,
               (long)strlen(FORMATTED_STRING_RESULT) + 1);

        TestsFailed += 1;
    }

    if (strcmp(PrintOutput, FORMATTED_STRING_RESULT) != 0) {
        printf("Error: Print formatted string failed:\nOutput : %s\n"
               "Correct: %s\n",
               PrintOutput,
               FORMATTED_STRING_RESULT);

        TestsFailed += 1;
    }

    //
    // Test a null character at the end.
    //

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    "%o %.*s%c",
                                    0100644,
                                    1,
                                    "a",
                                    '\0');

    if (StringLength != 10) {
        printf("Error: Failed to format with null character at end.\n");
        TestsFailed += 1;
    }

    //
    // Test one with positional arguments.
    //

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    FORMATTED_STRING_POSITIONAL_ARGUMENTS);

    if (StringLength != strlen(FORMATTED_STRING_POSITIONAL_RESULT) + 1) {
        printf("Error: Print formatted string with output returned output "
               "length of %d, should have been %lu.\n",
               StringLength,
               (long)strlen(FORMATTED_STRING_POSITIONAL_RESULT) + 1);

        TestsFailed += 1;
    }

    if (strcmp(PrintOutput, FORMATTED_STRING_POSITIONAL_RESULT) != 0) {
        printf("Error: Print formatted string failed:\nOutput : %s\n"
               "Correct: %s\n",
               PrintOutput,
               FORMATTED_STRING_POSITIONAL_RESULT);

        TestsFailed += 1;
    }

    //
    // Test one with the %n specifier.
    //

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    "%d %n",
                                    123,
                                    &BytesPrinted,
                                    456,
                                    789);

    if ((StringLength != 5) ||
        (strcmp(PrintOutput, "123 ") != 0) ||
        (BytesPrinted != 4)) {

        printf("Error: %%n specifier failed.\n");
        TestsFailed += 1;
    }

    //
    // Test floating point output.
    //

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    PRINT_FLOAT_FORMAT,
                                    PRINT_FLOAT_ARGUMENTS);

    if (StringLength != strlen(PRINT_FLOAT_RESULT) + 1) {
        printf("Error: Print float string with output returned output "
               "length of %d, should have been %lu.\n",
               StringLength,
               (long)strlen(PRINT_FLOAT_RESULT) + 1);

        TestsFailed += 1;
    }

    if (strcmp(PrintOutput, PRINT_FLOAT_RESULT) != 0) {
        printf("Error: Print float format string failed:\nOutput : %s\n"
               "Correct: %s\n",
               PrintOutput,
               PRINT_FLOAT_RESULT);

        TestsFailed += 1;
    }

    //
    // Test hex floating point output.
    //

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    PRINT_HEX_FLOAT_FORMAT,
                                    PRINT_HEX_FLOAT_ARGUMENTS);

    if (StringLength != strlen(PRINT_HEX_FLOAT_RESULT) + 1) {
        printf("Error: Print hex float string with output returned output "
               "length of %d, should have been %lu.\n",
               StringLength,
               (long)strlen(PRINT_HEX_FLOAT_RESULT) + 1);

        TestsFailed += 1;
    }

    if (strcmp(PrintOutput, PRINT_HEX_FLOAT_RESULT) != 0) {
        printf("Error: Print float format string failed:\nOutput : %s\n"
               "Correct: %s\n",
               PrintOutput,
               PRINT_HEX_FLOAT_RESULT);

        TestsFailed += 1;
    }

    //
    // Test NULL string.
    //

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    NULL);

    if ((StringLength != strlen("(null)") + 1) ||
        (strcmp(PrintOutput, "(null)") != 0)) {

        printf("Error: Calling print with NULL failed.\n");
        TestsFailed += 1;
    }

    //
    // Test printing that truncates.
    //

    PrintOutput[4] = 'A';
    RtlPrintToString(PrintOutput, 5, CharacterEncodingAscii, "123456789");
    if (PrintOutput[4] != '\0') {
        printf("Error: print output limit doesn't work.\n");
        TestsFailed += 1;
    }

    //
    // Test zero length precision.
    //

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    "s%.0ds",
                                    0);

    if ((StringLength != 3) || (strcmp(PrintOutput, "ss") != 0)) {
        printf("Error: Print zero precision failed.\n");
        TestsFailed += 1;
    }

    StringLength = RtlPrintToString(PrintOutput,
                                    MAX_OUTPUT,
                                    CharacterEncodingDefault,
                                    "s% .0ds",
                                    0);

    if ((StringLength != 4) || (strcmp(PrintOutput, "s s") != 0)) {
        printf("Error: Print zero precision failed 2.\n");
        TestsFailed += 1;
    }

    //
    // Test wide basic print, no formatting, with output.
    //

    StringLength = RtlPrintToStringWide(WidePrintOutput,
                                        MAX_OUTPUT,
                                        CharacterEncodingDefault,
                                        BASIC_STRING_WIDE);

    if (StringLength != wcslen(BASIC_STRING_WIDE) + 1) {
        wprintf(L"Error: Print wide basic string with no output returned "
                L"output length of %d, should have been %d.\n",
                StringLength,
                wcslen(BASIC_STRING_WIDE) + 1);

        TestsFailed += 1;
    }

    if (wcscmp(WidePrintOutput, BASIC_STRING_WIDE) != 0) {
        wprintf(L"Error: Print basic wide string failed:\nOutput : %ls\n"
                L"Correct: %ls\n",
                WidePrintOutput,
                BASIC_STRING_WIDE);

        TestsFailed += 1;
    }

    //
    // Test more complicated formatting.
    //

    StringLength = RtlPrintToStringWide(NULL,
                                        MAX_OUTPUT,
                                        CharacterEncodingDefault,
                                        FORMATTED_STRING_WIDE);

    if (StringLength != wcslen(FORMATTED_STRING_RESULT_WIDE) + 1) {
        wprintf(L"Error: Print formatted wide string with no output returned "
                L"output length of %d, should have been %d.\n",
                StringLength,
                wcslen(FORMATTED_STRING_RESULT_WIDE) + 1);

        TestsFailed += 1;
    }

    StringLength = RtlPrintToStringWide(WidePrintOutput,
                                        MAX_OUTPUT,
                                        CharacterEncodingDefault,
                                        FORMATTED_STRING_WIDE);

    if (StringLength != wcslen(FORMATTED_STRING_RESULT_WIDE) + 1) {
        wprintf(L"Error: Print formatted wide string with output returned "
                L"output length of %d, should have been %d.\n",
                StringLength,
                wcslen(FORMATTED_STRING_RESULT_WIDE) + 1);

        TestsFailed += 1;
    }

    if (wcscmp(WidePrintOutput, FORMATTED_STRING_RESULT_WIDE) != 0) {
        wprintf(L"Error: Print wide formatted string failed:\nOutput : %ls\n"
                L"Correct: %ls\n",
                WidePrintOutput,
                FORMATTED_STRING_RESULT_WIDE);

        TestsFailed += 1;
    }

    //
    // Test a null character at the end.
    //

    StringLength = RtlPrintToStringWide(WidePrintOutput,
                                        MAX_OUTPUT,
                                        CharacterEncodingDefault,
                                        L"%o %.*s%C",
                                        0100644,
                                        1,
                                        "a",
                                        L'\0');

    if (StringLength != 10) {
        printf("Error: Failed to format wide with null character at end.\n");
        TestsFailed += 1;
    }

    //
    // Test one with positional arguments.
    //

    StringLength = RtlPrintToStringWide(
                                   WidePrintOutput,
                                   MAX_OUTPUT,
                                   CharacterEncodingDefault,
                                   FORMATTED_STRING_POSITIONAL_ARGUMENTS_WIDE);

    if (StringLength != wcslen(FORMATTED_STRING_POSITIONAL_RESULT_WIDE) + 1) {
        wprintf(L"Error: Print formatted wide string with output returned "
                L"output length of %d, should have been %d.\n",
                StringLength,
                wcslen(FORMATTED_STRING_POSITIONAL_RESULT_WIDE) + 1);

        TestsFailed += 1;
    }

    if (wcscmp(WidePrintOutput, FORMATTED_STRING_POSITIONAL_RESULT_WIDE) != 0) {
        wprintf(L"Error: Print formatted wide positional string failed:\n"
                L"Output : %ls\n"
                L"Correct: %ls\n",
                WidePrintOutput,
                FORMATTED_STRING_POSITIONAL_RESULT_WIDE);

        TestsFailed += 1;
    }

    //
    // Test one with the %n specifier.
    //

    StringLength = RtlPrintToStringWide(WidePrintOutput,
                                        MAX_OUTPUT,
                                        CharacterEncodingDefault,
                                        L"%d %n",
                                        123,
                                        &BytesPrinted,
                                        456,
                                        789);

    if ((StringLength != 5) ||
        (wcscmp(WidePrintOutput, L"123 ") != 0) ||
        (BytesPrinted != 4)) {

        printf("Error: Wide %%n specifier failed.\n");
        TestsFailed += 1;
    }

    //
    // Test floating point output.
    //

    StringLength = RtlPrintToStringWide(WidePrintOutput,
                                        MAX_OUTPUT,
                                        CharacterEncodingDefault,
                                        PRINT_FLOAT_FORMAT_WIDE,
                                        PRINT_FLOAT_WIDE_ARGUMENTS);

    if (StringLength != wcslen(PRINT_FLOAT_RESULT_WIDE) + 1) {
        wprintf(L"Error: Print float wide string with output returned output "
                L"length of %d, should have been %d.\n",
                StringLength,
                wcslen(PRINT_FLOAT_RESULT_WIDE) + 1);

        TestsFailed += 1;
    }

    if (wcscmp(WidePrintOutput, PRINT_FLOAT_RESULT_WIDE) != 0) {
        wprintf(L"Error: Print float format wide string failed:\nOutput : %ls\n"
                L"Correct: %ls\n",
                WidePrintOutput,
                PRINT_FLOAT_RESULT_WIDE);

        TestsFailed += 1;
    }

    //
    // Test hex floating point output.
    //

    StringLength = RtlPrintToStringWide(WidePrintOutput,
                                        MAX_OUTPUT,
                                        CharacterEncodingDefault,
                                        PRINT_HEX_FLOAT_FORMAT_WIDE,
                                        PRINT_HEX_FLOAT_ARGUMENTS);

    if (StringLength != wcslen(PRINT_HEX_FLOAT_RESULT_WIDE) + 1) {
        wprintf(L"Error: Print hex float wide string with output returned "
                L"output length of %d, should have been %d.\n",
                StringLength,
                wcslen(PRINT_HEX_FLOAT_RESULT_WIDE) + 1);

        TestsFailed += 1;
    }

    if (wcscmp(WidePrintOutput, PRINT_HEX_FLOAT_RESULT_WIDE) != 0) {
        wprintf(L"Error: Print float format wide string failed:\nOutput : %ls\n"
                L"Correct: %ls\n",
                WidePrintOutput,
                PRINT_HEX_FLOAT_RESULT_WIDE);

        TestsFailed += 1;
    }

    //
    // Test NULL string.
    //

    StringLength = RtlPrintToStringWide(WidePrintOutput,
                                        MAX_OUTPUT,
                                        CharacterEncodingDefault,
                                        NULL);

    if ((StringLength != wcslen(L"(null)") + 1) ||
        (wcscmp(WidePrintOutput, L"(null)") != 0)) {

        printf("Error: Calling print with NULL failed.\n");
        TestsFailed += 1;
    }

    //
    // Test printing that truncates.
    //

    WidePrintOutput[4] = L'A';
    RtlPrintToStringWide(WidePrintOutput,
                         5,
                         CharacterEncodingAscii,
                         L"123456789");

    if (WidePrintOutput[4] != L'\0') {
        printf("Error: Wide print output limit doesn't work.\n");
        TestsFailed += 1;
    }

    TestsFailed += TestRedBlackTrees(TRUE);
    TestsFailed += TestScanInteger(TRUE);
    TestsFailed += TestScanDouble();
    TestsFailed += TestStringScanner();
    TestsFailed += TestScanIntegerWide(TRUE);
    TestsFailed += TestScanDoubleWide();
    TestsFailed += TestStringScannerWide();

    //
    // Tests are over, print results.
    //

    if (TestsFailed != 0) {
        printf("*** %d Failure(s) in RTL Test. ***\n", TestsFailed);
        return 1;
    }

    printf("All RTL tests passed.\n");
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
TestRedBlackTrees (
    BOOL Quiet
    )

/*++

Routine Description:

    This routine tests Red-Black trees.

Arguments:

    Quiet - Supplies a boolean indicating whether the test should be run
        without printouts (TRUE) or with debug output (FALSE).

Return Value:

    Returns the number of tests that failed.

--*/

{

    PTEST_RED_BLACK_TREE_NODE Node;
    ULONG NodeIndex;
    ULONG TestsFailed;
    RED_BLACK_TREE Tree;

    Node = TestRedBlackTreeNodes;
    TestsFailed = 0;
    RtlZeroMemory(Node, sizeof(TEST_RED_BLACK_TREE_NODE) * TEST_NODE_COUNT);

    //
    // Initialize the nodes.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        Node[NodeIndex].Value = NodeIndex;
    }

    RtlRedBlackTreeInitialize(&Tree,
                              RED_BLACK_TREE_FLAG_PERIODIC_VALIDATION,
                              TestCompareRedBlackTreeNodes);

    //
    // Test an empty tree.
    //

    TestsFailed += TestEmptyRedBlackTreeQueries(&Tree);

    //
    // Fire up a tree and add a bunch of stuff to it in order.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        RtlRedBlackTreeInsert(&Tree, &(Node[NodeIndex].TreeNode));
        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after inserting index %d\n", NodeIndex);
            TestsFailed += 1;
        }
    }

    TestsFailed += TestFullRedBlackTreeQueries(&Tree);

    //
    // Now remove everything from it in order.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        RtlRedBlackTreeRemove(&Tree, &(Node[NodeIndex].TreeNode));
        RtlZeroMemory(&(Node[NodeIndex]), sizeof(TEST_RED_BLACK_TREE_NODE));
        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after removing index %d\n", NodeIndex);
            TestsFailed += 1;
        }
    }

    TestsFailed += TestEmptyRedBlackTreeQueries(&Tree);
    TestsFailed += TestRedBlackTreeNodesBlank(Node, 0, TEST_NODE_COUNT);
    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        Node[NodeIndex].Value = NodeIndex;
    }

    //
    // Add stuff in reverse order.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        RtlRedBlackTreeInsert(
                            &Tree,
                            &(Node[TEST_NODE_COUNT - 1 - NodeIndex].TreeNode));

        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after inserting index %d\n",
                   TEST_NODE_COUNT - 1 - NodeIndex);

            TestsFailed += 1;
        }
    }

    TestsFailed += TestFullRedBlackTreeQueries(&Tree);

    //
    // Now remove everything from it in regular order.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        RtlRedBlackTreeRemove(&Tree, &(Node[NodeIndex].TreeNode));
        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after removing index %d\n", NodeIndex);
            TestsFailed += 1;
        }
    }

    RtlRedBlackTreeIterate(&Tree, TestPrintRedBlackTreeNode, NULL);

    //
    // Now add everything in alternating order.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        if ((NodeIndex & 0x1) != 0) {
            RtlRedBlackTreeInsert(
                                &Tree,
                                &(Node[TEST_NODE_COUNT - NodeIndex].TreeNode));

        } else {
            RtlRedBlackTreeInsert(&Tree, &(Node[NodeIndex].TreeNode));
        }

        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after inserting index %d\n", NodeIndex);
            TestsFailed += 1;
        }
    }

    TestsFailed += TestFullRedBlackTreeQueries(&Tree);

    //
    // Now remove everything from it in reverse order.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        RtlRedBlackTreeRemove(
                            &Tree,
                            &(Node[TEST_NODE_COUNT - 1 - NodeIndex].TreeNode));

        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after removing index %d\n",
                   TEST_NODE_COUNT - 1 - NodeIndex);

            TestsFailed += 1;
        }
    }

    //
    // Randomize all the keys, insert them, and remove them.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        Node[NodeIndex].Value = rand();
        RtlRedBlackTreeInsert(&Tree, &(Node[NodeIndex].TreeNode));
        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after inserting index %d\n", NodeIndex);
            TestsFailed += 1;
        }
    }

    //
    // Now remove everything from it in order.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        RtlRedBlackTreeRemove(&Tree, &(Node[NodeIndex].TreeNode));
        RtlZeroMemory(&(Node[NodeIndex]), sizeof(TEST_RED_BLACK_TREE_NODE));
        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after removing index %d\n", NodeIndex);
            TestsFailed += 1;
        }
    }

    TestsFailed += TestEmptyRedBlackTreeQueries(&Tree);
    TestsFailed += TestRedBlackTreeNodesBlank(Node, 0, TEST_NODE_COUNT);

    //
    // Randomize the keys again, but this time mod them to make many duplicates.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        Node[NodeIndex].Value = rand() % 64;
        RtlRedBlackTreeInsert(&Tree, &(Node[NodeIndex].TreeNode));
        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after inserting index %d\n", NodeIndex);
            TestsFailed += 1;
        }
    }

    //RtlRedBlackTreeIterate(&Tree, TestPrintRedBlackTreeNode);

    //
    // Now remove everything from it in order.
    //

    for (NodeIndex = 0; NodeIndex < TEST_NODE_COUNT; NodeIndex += 1) {
        RtlRedBlackTreeRemove(&Tree, &(Node[NodeIndex].TreeNode));
        RtlZeroMemory(&(Node[NodeIndex]), sizeof(TEST_RED_BLACK_TREE_NODE));
        if (RtlValidateRedBlackTree(&Tree) == FALSE) {
            printf("RBTREE: Not valid after removing index %d\n", NodeIndex);
            TestsFailed += 1;
        }
    }

    TestsFailed += TestEmptyRedBlackTreeQueries(&Tree);
    TestsFailed += TestRedBlackTreeNodesBlank(Node, 0, TEST_NODE_COUNT);
    return TestsFailed;
}

COMPARISON_RESULT
TestCompareRedBlackTreeNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes.

Arguments:

    Tree - Supplies a pointer to the tree being iterated over.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PTEST_RED_BLACK_TREE_NODE First;
    PTEST_RED_BLACK_TREE_NODE Second;

    First = RED_BLACK_TREE_VALUE(FirstNode, TEST_RED_BLACK_TREE_NODE, TreeNode);
    Second = RED_BLACK_TREE_VALUE(SecondNode,
                                  TEST_RED_BLACK_TREE_NODE,
                                  TreeNode);

    if (First->Value > Second->Value) {
        return ComparisonResultDescending;

    } else if (First->Value < Second->Value) {
        return ComparisonResultAscending;
    }

    return ComparisonResultSame;
}

VOID
TestPrintRedBlackTreeNode (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    ULONG Level,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each node in the tree (via an in order
    traversal). It assumes that the tree will not be modified during the
    traversal.

Arguments:

    Tree - Supplies a pointer to the tree being enumerated.

    Node - Supplies a pointer to the node.

    Level - Supplies the depth into the tree that this node exists at. 0 is
        the root.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested.

Return Value:

    None.

--*/

{

    UCHAR Color;
    ULONG SpaceIndex;
    PTEST_RED_BLACK_TREE_NODE TestNode;

    for (SpaceIndex = 0; SpaceIndex < Level; SpaceIndex += 1) {
        RtlDebugPrint("  ");
    }

    TestNode = RED_BLACK_TREE_VALUE(Node,
                                    TEST_RED_BLACK_TREE_NODE,
                                    TreeNode);

    Color = 'B';
    if (TestNode->TreeNode.Red != FALSE) {
        Color = 'R';
    }

    RtlDebugPrint("%d %c (0x%x)\n", TestNode->Value, Color, TestNode);

    ASSERT(Node != &(Tree->NullNode));
    ASSERT(Node != &(Tree->Root));

    return;
}

VOID
TestRedBlackTreeVerifyInOrderTraversal (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    ULONG Level,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each node in the tree (via an in order
    traversal). It assumes that the tree will not be modified during the
    traversal.

Arguments:

    Tree - Supplies a pointer to the tree being enumerated.

    Node - Supplies a pointer to the node.

    Level - Supplies the depth into the tree that this node exists at. 0 is
        the root.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested.

Return Value:

    None.

--*/

{

    PTEST_RED_BLACK_TREE_NODE TestNode;

    TestNode = RED_BLACK_TREE_VALUE(Node,
                                    TEST_RED_BLACK_TREE_NODE,
                                    TreeNode);

    ASSERT(Node != &(Tree->NullNode));
    ASSERT(Node != &(Tree->Root));
    ASSERT(TestNode->Value == NextExpectedRedBlackTreeValue);

    NextExpectedRedBlackTreeValue += 1;
    return;
}

ULONG
TestFullRedBlackTreeQueries (
    PRED_BLACK_TREE Tree
    )

/*++

Routine Description:

    This routine performs some standard queries on a Red-Black tree filled with
    nodes.

Arguments:

    Tree - Supplies a pointer to the tree.

Return Value:

    Returns the number of tests that failed.

--*/

{

    TEST_RED_BLACK_TREE_NODE DummyNode;
    PRED_BLACK_TREE_NODE FoundNode;
    PTEST_RED_BLACK_TREE_NODE FoundTestNode;
    ULONG TestsFailed;

    TestsFailed = 0;

    //
    // Attempt to find some values.
    //

    FoundNode = RtlRedBlackTreeGetLowestNode(Tree);
    if (FoundNode == NULL) {
        printf("RBTREE: Failed to find lowest node.\n");
        TestsFailed += 1;

    } else {
        FoundTestNode = RED_BLACK_TREE_VALUE(FoundNode,
                                             TEST_RED_BLACK_TREE_NODE,
                                             TreeNode);

        if (FoundTestNode->Value != 0) {
            printf("RBTREE: Found lowest value %d instead of 0.\n",
                   FoundTestNode->Value);

            TestsFailed += 1;
        }
    }

    FoundNode = RtlRedBlackTreeGetHighestNode(Tree);
    if (FoundNode == NULL) {
        printf("RBTREE: Failed to find lowest node.\n");
        TestsFailed += 1;

    } else {
        FoundTestNode = RED_BLACK_TREE_VALUE(FoundNode,
                                             TEST_RED_BLACK_TREE_NODE,
                                             TreeNode);

        if (FoundTestNode->Value != TEST_NODE_COUNT - 1) {
            printf("RBTREE: Found lowest value %d instead of %d.\n",
                   FoundTestNode->Value,
                   TEST_NODE_COUNT - 1);

            TestsFailed += 1;
        }
    }

    DummyNode.Value = 0;
    FoundNode = RtlRedBlackTreeSearch(Tree, &(DummyNode.TreeNode));
    if (FoundNode == NULL) {
        printf("RBTREE: Search for 0 failed.\n");
        TestsFailed += 1;

    } else {
        FoundTestNode = RED_BLACK_TREE_VALUE(FoundNode,
                                             TEST_RED_BLACK_TREE_NODE,
                                             TreeNode);

        if (FoundTestNode->Value != 0) {
            printf("RBTREE: Found lowest value %d instead of %d.\n",
                   FoundTestNode->Value,
                   0);

            TestsFailed += 1;
        }
    }

    DummyNode.Value = 1;
    FoundNode = RtlRedBlackTreeSearch(Tree, &(DummyNode.TreeNode));
    if (FoundNode == NULL) {
        printf("RBTREE: Search for 1 failed.\n");
        TestsFailed += 1;

    } else {
        FoundTestNode = RED_BLACK_TREE_VALUE(FoundNode,
                                             TEST_RED_BLACK_TREE_NODE,
                                             TreeNode);

        if (FoundTestNode->Value != 1) {
            printf("RBTREE: Found lowest value %d instead of %d.\n",
                   FoundTestNode->Value,
                   1);

            TestsFailed += 1;
        }
    }

    DummyNode.Value = (TEST_NODE_COUNT / 2) - 1;
    FoundNode = RtlRedBlackTreeSearch(Tree, &(DummyNode.TreeNode));
    if (FoundNode == NULL) {
        printf("RBTREE: Search for %d failed.\n", (TEST_NODE_COUNT / 2) - 1);
        TestsFailed += 1;

    } else {
        FoundTestNode = RED_BLACK_TREE_VALUE(FoundNode,
                                             TEST_RED_BLACK_TREE_NODE,
                                             TreeNode);

        if (FoundTestNode->Value != (TEST_NODE_COUNT / 2) - 1) {
            printf("RBTREE: Found lowest value %d instead of %d.\n",
                   FoundTestNode->Value,
                   (TEST_NODE_COUNT / 2) - 1);

            TestsFailed += 1;
        }
    }

    DummyNode.Value = TEST_NODE_COUNT - 1;
    FoundNode = RtlRedBlackTreeSearch(Tree, &(DummyNode.TreeNode));
    if (FoundNode == NULL) {
        printf("RBTREE: Search for %d failed.\n", TEST_NODE_COUNT - 1);
        TestsFailed += 1;

    } else {
        FoundTestNode = RED_BLACK_TREE_VALUE(FoundNode,
                                             TEST_RED_BLACK_TREE_NODE,
                                             TreeNode);

        if (FoundTestNode->Value != TEST_NODE_COUNT - 1) {
            printf("RBTREE: Found lowest value %d instead of %d.\n",
                   FoundTestNode->Value,
                   TEST_NODE_COUNT - 1);

            TestsFailed += 1;
        }
    }

    DummyNode.Value = TEST_NODE_COUNT + 1;
    FoundNode = RtlRedBlackTreeSearch(Tree, &(DummyNode.TreeNode));
    if (FoundNode != NULL) {
        printf("RBTREE: Found %p for out of bounds search %d\n",
               FoundNode,
               TEST_NODE_COUNT + 1);

        TestsFailed += 1;
    }

    //
    // Iterate through the tree and verify that it comes out in order.
    //

    NextExpectedRedBlackTreeValue = 0;
    RtlRedBlackTreeIterate(Tree, TestRedBlackTreeVerifyInOrderTraversal, NULL);
    return TestsFailed;
}

ULONG
TestEmptyRedBlackTreeQueries (
    PRED_BLACK_TREE Tree
    )

/*++

Routine Description:

    This routine performs some standard queries on a Red-Black tree that's
    empty.

Arguments:

    Tree - Supplies a pointer to the tree.

Return Value:

    Returns the number of tests that failed.

--*/

{

    TEST_RED_BLACK_TREE_NODE DummyNode;
    PRED_BLACK_TREE_NODE FoundNode;
    ULONG TestsFailed;

    TestsFailed = 0;
    NextExpectedRedBlackTreeValue = 0;
    RtlRedBlackTreeIterate(Tree, TestRedBlackTreeVerifyInOrderTraversal, NULL);
    FoundNode = RtlRedBlackTreeGetLowestNode(Tree);
    if (FoundNode != NULL) {
        printf("RBTREE: Get Lowest Node on an empty tree returned %p\n",
               FoundNode);

        TestsFailed += 1;
    }

    FoundNode = RtlRedBlackTreeGetHighestNode(Tree);
    if (FoundNode != NULL) {
        printf("RBTREE: Get Highest Node on an empty tree returned %p\n",
               FoundNode);

        TestsFailed += 1;
    }

    DummyNode.Value = 0;
    FoundNode = RtlRedBlackTreeSearch(Tree, &(DummyNode.TreeNode));
    if (FoundNode != NULL) {
        printf("RBTREE: Search on an empty tree returned %p.\n", FoundNode);
        TestsFailed += 1;
    }

    return TestsFailed;
}

ULONG
TestRedBlackTreeNodesBlank (
    PTEST_RED_BLACK_TREE_NODE Node,
    ULONG StartIndex,
    ULONG EndIndex
    )

/*++

Routine Description:

    This routine verifies that the given red black tree nodes are zeroed out.

Arguments:

    Node - Supplies a pointer to an array of nodes.

    StartIndex - Supplies the start index of the array to verify, inclusive.

    EndIndex - Supplies the end index of the array to verify, exclusive.

Return Value:

    0 if the array is completely zeroed out.

    1 if the array has nonzero contents.

--*/

{

    ULONG ByteIndex;
    PUCHAR BytePointer;
    PTEST_RED_BLACK_TREE_NODE CurrentNode;
    ULONG Index;
    ULONG TestsFailed;

    TestsFailed = 0;
    for (Index = StartIndex; Index < EndIndex; Index += 1) {
        CurrentNode = &(Node[Index]);
        BytePointer = (PUCHAR)CurrentNode;
        for (ByteIndex = 0;
             ByteIndex < sizeof(TEST_RED_BLACK_TREE_NODE);
             ByteIndex += 1) {

            if (BytePointer[ByteIndex] != 0) {
                printf("RBTREE: Node %p Index %d has non-zero contents.\n",
                       CurrentNode,
                       Index);

                TestsFailed = 1;
                break;
            }
        }
    }

    return TestsFailed;
}

ULONG
TestScanInteger (
    BOOL Quiet
    )

/*++

Routine Description:

    This routine tests the scan integer function.

Arguments:

    Quiet - Supplies a boolean indicating whether the test should be run
        without printouts (TRUE) or with debug output (FALSE).

Return Value:

    Returns the number of tests that failed.

--*/

{

    ULONG Failures;
    PSTR InputString;
    KSTATUS Status;
    PCSTR String;
    ULONG StringSize;
    LONGLONG Value;

    Failures = 0;

    //
    // Test that all whitespace doesn't advance the string.
    //

    InputString = SCAN_STRING_BLANK;
    String = InputString;
    StringSize = sizeof(SCAN_STRING_BLANK);
    Status = RtlStringScanInteger(&String, &StringSize, 0, TRUE, &Value);
    if ((Status != STATUS_END_OF_FILE) ||
        (String != InputString) ||
        (StringSize != sizeof(SCAN_STRING_BLANK))) {

        printf("ScanInteger: Failed to not scan blank string.\n");
        Failures += 1;
    }

    //
    // Test the same thing for the empty string.
    //

    InputString = SCAN_EMPTY_STRING;
    String = InputString;
    StringSize = sizeof(SCAN_EMPTY_STRING);
    Status = RtlStringScanInteger(&String, &StringSize, 0, TRUE, &Value);
    if ((Status != STATUS_END_OF_FILE) ||
        (String != InputString) ||
        (StringSize != sizeof(SCAN_EMPTY_STRING))) {

        printf("ScanInteger: Failed to not scan empty string.\n");
        Failures += 1;
    }

    //
    // Scan an invalid string.
    //

    InputString = SCAN_INVALID_STRING;
    String = InputString;
    StringSize = sizeof(SCAN_INVALID_STRING);
    Status = RtlStringScanInteger(&String, &StringSize, 0, TRUE, &Value);
    if ((Status != STATUS_INVALID_SEQUENCE) ||
        (String != InputString) ||
        (StringSize != sizeof(SCAN_INVALID_STRING))) {

        printf("ScanInteger: Failed to not scan invalid string.\n");
        Failures += 1;
    }

    //
    // Scan a nice decimal integer automatically detecting the base.
    //

    InputString = SCAN_DECIMAL_INTEGER;
    String = InputString;
    StringSize = sizeof(SCAN_DECIMAL_INTEGER);
    Status = RtlStringScanInteger(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_DECIMAL_INTEGER_LENGTH) ||
        (StringSize !=
         sizeof(SCAN_DECIMAL_INTEGER) - SCAN_DECIMAL_INTEGER_LENGTH) ||
        (Value != SCAN_DECIMAL_INTEGER_VALUE)) {

        printf("ScanInteger: Failed to scan decimal integer string.\n");
        Failures += 1;
    }

    //
    // Scan an octal integer with non-octal digits on the end.
    //

    InputString = SCAN_OCTAL_INTEGER;
    String = InputString;
    StringSize = sizeof(SCAN_OCTAL_INTEGER);
    Status = RtlStringScanInteger(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_OCTAL_INTEGER_LENGTH) ||
        (StringSize !=
         sizeof(SCAN_OCTAL_INTEGER) - SCAN_OCTAL_INTEGER_LENGTH) ||
        (Value != SCAN_OCTAL_INTEGER_VALUE)) {

        printf("ScanInteger: Failed to scan octal integer string.\n");
        Failures += 1;
    }

    //
    // Scan a hexadecimal string on auto-detect.
    //

    InputString = SCAN_HEX_INTEGER;
    String = InputString;
    StringSize = sizeof(SCAN_HEX_INTEGER);
    Status = RtlStringScanInteger(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_HEX_INTEGER_LENGTH) ||
        (StringSize !=
         sizeof(SCAN_HEX_INTEGER) - SCAN_HEX_INTEGER_LENGTH) ||
        (Value != SCAN_HEX_INTEGER_VALUE)) {

        printf("ScanInteger: Failed to scan hex integer string.\n");
        Failures += 1;
    }

    //
    // Scan something with a weird base.
    //

    InputString = SCAN_BASE35_INTEGER;
    String = InputString;
    StringSize = sizeof(SCAN_BASE35_INTEGER);
    Status = RtlStringScanInteger(&String, &StringSize, 35, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_BASE35_INTEGER_LENGTH) ||
        (StringSize !=
         sizeof(SCAN_BASE35_INTEGER) - SCAN_BASE35_INTEGER_LENGTH) ||
        (Value != SCAN_BASE35_INTEGER_VALUE)) {

        printf("ScanInteger: Failed to scan base35 integer string.\n");
        Failures += 1;
    }

    //
    // Scan an 0xz, and watch out that only the zero gets scanned.
    //

    InputString = SCAN_0XZ;
    String = InputString;
    StringSize = sizeof(SCAN_0XZ);
    Status = RtlStringScanInteger(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_0XZ_LENGTH) ||
        (StringSize !=
         sizeof(SCAN_0XZ) - SCAN_0XZ_LENGTH) ||
        (Value != SCAN_0XZ_VALUE)) {

        printf("ScanInteger: Failed to scan 0xz.\n");
        Failures += 1;
    }

    //
    // Scan just a zero.
    //

    InputString = SCAN_ZERO;
    String = InputString;
    StringSize = sizeof(SCAN_ZERO);
    Status = RtlStringScanInteger(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_ZERO_LENGTH) ||
        (StringSize !=
         sizeof(SCAN_ZERO) - SCAN_ZERO_LENGTH) ||
        (Value != SCAN_ZERO_VALUE)) {

        printf("ScanInteger: Failed to scan 0.\n");
        Failures += 1;
    }

    return Failures;
}

ULONG
TestScanDouble (
    VOID
    )

/*++

Routine Description:

    This routine tests the scan double function.

Arguments:

    None.

Return Value:

    Returns the number of tests that failed.

--*/

{

    PCSTR AfterScan;
    DOUBLE_PARTS AnswerParts;
    ULONGLONG Difference;
    ULONG Failures;
    double Result[SCAN_DOUBLE_COUNT];
    ULONG ResultCount;
    ULONG ResultIndex;
    DOUBLE_PARTS ResultParts;
    KSTATUS Status;
    CHAR String[10];
    ULONG StringSize;

    Failures = 0;
    RtlZeroMemory(Result, sizeof(Result));
    Status = RtlStringScan(SCAN_DOUBLE_INPUT,
                           MAX_ULONG,
                           SCAN_DOUBLE_FORMAT,
                           MAX_ULONG,
                           CharacterEncodingDefault,
                           &ResultCount,
                           &(Result[0]),
                           &(Result[1]),
                           &(Result[2]),
                           &(Result[3]),
                           &(Result[4]),
                           &(Result[5]),
                           &(Result[6]),
                           &(Result[7]),
                           &(Result[8]),
                           &(Result[9]),
                           &(Result[10]),
                           &(Result[11]),
                           &(Result[12]),
                           &(Result[13]),
                           &(Result[14]),
                           &(Result[15]),
                           &(Result[16]),
                           &(Result[17]),
                           &(Result[18]),
                           &(Result[19]),
                           &(Result[20]),
                           &(Result[21]),
                           &(Result[22]),
                           &(Result[23]),
                           &(Result[24]),
                           &(Result[25]),
                           &(Result[26]),
                           &(Result[27]));

    if (!KSUCCESS(Status)) {
        printf("ScanDouble: Failed to scan, status %d\n", Status);
        Failures += 1;
    }

    if (ResultCount != SCAN_DOUBLE_COUNT) {
        printf("ScanDouble: Only scanned %d of %d items.\n",
               ResultCount,
               SCAN_DOUBLE_COUNT);

        Failures += 1;
    }

    for (ResultIndex = 0; ResultIndex < ResultCount; ResultIndex += 1) {
        ResultParts.Double = Result[ResultIndex];
        AnswerParts.Double = TestScanDoubleValues[ResultIndex];
        if (ResultParts.Ulonglong >= AnswerParts.Ulonglong) {
            Difference = ResultParts.Ulonglong - AnswerParts.Ulonglong;

        } else {
            Difference = AnswerParts.Ulonglong - ResultParts.Ulonglong;
        }

        if (Difference > SCAN_DOUBLE_PLAY) {
            printf("ScanDouble: Item %d was %.13a (%.16g), should have been "
                   "%.13a (%.16g)\n",
                   ResultIndex,
                   ResultParts.Double,
                   ResultParts.Double,
                   AnswerParts.Double,
                   AnswerParts.Double);

            Failures += 1;
        }
    }

    //
    // Try nan() and nan(.
    //

    StringSize = sizeof(String);
    RtlStringCopy(String, "nan()", StringSize);
    AfterScan = String;
    Status = RtlStringScanDouble(&AfterScan, &StringSize, &(Result[0]));
    if ((!KSUCCESS(Status)) || (AfterScan != String + 5)) {
        printf("ScanDouble: Failed to scan nan()\n");
        Failures += 1;
    }

    StringSize = sizeof(String);
    RtlStringCopy(String, "nan(", StringSize);
    AfterScan = String;
    Status = RtlStringScanDouble(&AfterScan, &StringSize, &(Result[0]));
    if ((!KSUCCESS(Status)) || (AfterScan != String + 3)) {
        printf("ScanDouble: Failed to scan nan(\n");
        Failures += 1;
    }

    if (Failures != 0) {
        printf("%d ScanDouble failures.\n", Failures);
    }

    return Failures;
}

ULONG
TestStringScanner (
    VOID
    )

/*++

Routine Description:

    This routine tests the string scanning functionality.

Arguments:

    None.

Return Value:

    Returns the number of test failures.

--*/

{

    INT BytesSoFar;
    ULONG Failures;
    INT Integer1;
    INT Integer2;
    INT Integer3;
    INT Integer4;
    INT Integer5;
    INT Integer6;
    INT Integer7;
    INT Integer8;
    ULONG ItemsScanned;
    LONGLONG LongInteger4;
    LONGLONG LongInteger5;
    KSTATUS Status;
    CHAR String1[10];
    CHAR String2[10];
    CHAR String3[10];
    CHAR String4[10];
    CHAR String5[10];

    Failures = 0;

    //
    // Test basic functionality.
    //

    Integer1 = 0;
    Integer2 = 0;
    RtlZeroMemory(String1, sizeof(String1));
    RtlZeroMemory(String2, sizeof(String2));
    RtlZeroMemory(String3, sizeof(String3));
    ItemsScanned = 0;
    Status = RtlStringScan(SCAN_BASIC_INPUT,
                           sizeof(SCAN_BASIC_INPUT),
                           SCAN_BASIC_FORMAT,
                           sizeof(SCAN_BASIC_FORMAT),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           &Integer1,
                           String1,
                           String2,
                           &Integer2,
                           &BytesSoFar,
                           String3);

    if ((!KSUCCESS(Status)) ||
        (ItemsScanned != SCAN_BASIC_ITEM_COUNT) ||
        (Integer1 != SCAN_BASIC_INTEGER1) ||
        (RtlAreStringsEqual(String1, SCAN_BASIC_STRING1, 10) == FALSE) ||
        (RtlAreStringsEqual(String2, SCAN_BASIC_STRING2, 10) == FALSE) ||
        (Integer2 != SCAN_BASIC_INTEGER2) ||
        (BytesSoFar != SCAN_BASIC_BYTES_SO_FAR) ||
        (RtlAreStringsEqual(String3, SCAN_BASIC_STRING3, 10) == FALSE)) {

        printf("ScanString: Failed to scan basic string.\n");
        Failures += 1;
    }

    //
    // Test the integer varieties, size overrides, and field lengths.
    //

    Integer1 = 0;
    Integer2 = 0;
    Integer3 = 0;
    LongInteger4 = 0;
    LongInteger5 = 0;
    Integer6 = 0;
    Integer7 = 0;
    Integer8 = 0;
    Status = RtlStringScan(SCAN_INTEGERS_INPUT,
                           sizeof(SCAN_INTEGERS_INPUT),
                           SCAN_INTEGERS_FORMAT,
                           sizeof(SCAN_INTEGERS_FORMAT),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           &Integer1,
                           &Integer2,
                           &Integer3,
                           &LongInteger4,
                           &LongInteger5,
                           &Integer6,
                           &Integer7,
                           &Integer8,
                           &BytesSoFar);

    if ((!KSUCCESS(Status)) ||
        (ItemsScanned != SCAN_INTEGERS_ITEM_COUNT) ||
        (Integer1 != SCAN_INTEGERS_INTEGER1) ||
        (Integer2 != SCAN_INTEGERS_INTEGER2) ||
        (Integer3 != SCAN_INTEGERS_INTEGER3) ||
        (LongInteger4 != SCAN_INTEGERS_INTEGER4) ||
        (LongInteger5 != SCAN_INTEGERS_INTEGER5) ||
        (Integer6 != SCAN_INTEGERS_INTEGER6) ||
        (Integer7 != SCAN_INTEGERS_INTEGER7) ||
        (Integer8 != SCAN_INTEGERS_INTEGER8) ||
        (BytesSoFar != SCAN_INTEGERS_BYTES_SO_FAR)) {

        printf("ScanString: Failed to scan integers sequences.\n");
        Failures += 1;
    }

    //
    // Test some character sets.
    //

    Status = RtlStringScan(SCAN_SET_INPUT,
                           sizeof(SCAN_SET_INPUT),
                           SCAN_SET_FORMAT,
                           sizeof(SCAN_SET_FORMAT),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           String1,
                           String2,
                           String3,
                           String4,
                           String5);

    if ((!KSUCCESS(Status)) ||
        (ItemsScanned != 5) ||
        (RtlAreStringsEqual(String1, SCAN_SET_STRING1, 10) == FALSE) ||
        (RtlAreStringsEqual(String2, SCAN_SET_STRING2, 10) == FALSE) ||
        (RtlAreStringsEqual(String3, SCAN_SET_STRING3, 10) == FALSE) ||
        (RtlAreStringsEqual(String4, SCAN_SET_STRING4, 10) == FALSE) ||
        (RtlAreStringsEqual(String5, SCAN_SET_STRING5, 10) == FALSE)) {

        printf("ScanString: Failed to scan scan set input.\n");
        Failures += 1;
    }

    //
    // Try a bunch of formats that shouldn't work.
    //

    Status = RtlStringScan(SCAN_DUMMY_INPUT,
                           sizeof(SCAN_DUMMY_INPUT),
                           "%",
                           sizeof("%"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 1.\n");
        Failures += 1;
    }

    Status = RtlStringScan(SCAN_DUMMY_INPUT,
                           sizeof(SCAN_DUMMY_INPUT),
                           "%301",
                           sizeof("%301"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 2.\n");
        Failures += 1;
    }

    Status = RtlStringScan(SCAN_DUMMY_INPUT,
                           sizeof(SCAN_DUMMY_INPUT),
                           "%ll",
                           sizeof("%ll"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 3.\n");
        Failures += 1;
    }

    Status = RtlStringScan(SCAN_DUMMY_INPUT,
                           sizeof(SCAN_DUMMY_INPUT),
                           "%c",
                           1,
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 4.\n");
        Failures += 1;
    }

    Status = RtlStringScan(SCAN_DUMMY_INPUT,
                           sizeof(SCAN_DUMMY_INPUT),
                           "%30[",
                           sizeof("%30["),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 5.\n");
        Failures += 1;
    }

    Status = RtlStringScan(SCAN_DUMMY_INPUT,
                           sizeof(SCAN_DUMMY_INPUT),
                           "%[^",
                           sizeof("%[^"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 6.\n");
        Failures += 1;
    }

    Status = RtlStringScan(SCAN_DUMMY_INPUT,
                           sizeof(SCAN_DUMMY_INPUT),
                           "%[]aaa",
                           sizeof("%[]aaa"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 7.\n");
        Failures += 1;
    }

    Status = RtlStringScan(SCAN_DUMMY_INPUT,
                           sizeof(SCAN_DUMMY_INPUT),
                           "%0s",
                           sizeof("%0s"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 8.\n");
        Failures += 1;
    }

    Status = RtlStringScan(SCAN_DUMMY_INPUT,
                           sizeof(SCAN_DUMMY_INPUT),
                           "%jj",
                           sizeof("%jj"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 9.\n");
        Failures += 1;
    }

    Status = RtlStringScan(" ",
                           sizeof(" "),
                           "%s",
                           sizeof("%s"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 10.\n");
        Failures += 1;
    }

    //
    // Try scanning stuff passing in the empty string as input.
    //

    Status = RtlStringScan("",
                           sizeof(""),
                           "%c",
                           sizeof("%c"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail empty string 1.\n");
        Failures += 1;
    }

    Status = RtlStringScan("",
                           sizeof(""),
                           "%lld",
                           sizeof("%lld"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail empty string 2.\n");
        Failures += 1;
    }

    Status = RtlStringScan("",
                           sizeof(""),
                           "%[a]",
                           sizeof("%[a]"),
                           CharacterEncodingDefault,
                           &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail empty string 3.\n");
        Failures += 1;
    }

    //
    // See if the input string goes out of bounds.
    //

    Integer1 = 0;
    Status = RtlStringScan("123456",
                           sizeof("1234") - 1,
                           "%d",
                           sizeof("%d"),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           &Integer1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 1) || (Integer1 != 1234)) {
        printf("ScanString: Failed to stop integer at input boundary.\n");
        Failures += 1;
    }

    Status = RtlStringScan("  ASDFASDF",
                           sizeof("  ASDF") - 1,
                           "%s",
                           sizeof("%s"),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           String1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 1) ||
        (RtlAreStringsEqual(String1, "ASDF", 10) == FALSE)) {

        printf("ScanString: Failed to stop string at input boundary.\n");
        Failures += 1;
    }

    RtlZeroMemory(String1, sizeof(String1));
    Status = RtlStringScan(" ASDF",
                           sizeof(" A") - 1,
                           "%10c",
                           sizeof("%10c"),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           String1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 1) ||
        (RtlAreStringsEqual(String1, " A", 10) == FALSE)) {

        printf("ScanString: Failed to stop characters at input boundary.\n");
        Failures += 1;
    }

    Status = RtlStringScan("ASDF",
                           sizeof("AS") - 1,
                           "%10[SDFA]",
                           sizeof("%10[SDFA]"),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           String1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 1) ||
        (RtlAreStringsEqual(String1, "AS", 10) == FALSE)) {

        printf("ScanString: Failed to stop scanset at input boundary.\n");
        Failures += 1;
    }

    Status = RtlStringScan("123456",
                           sizeof("123456"),
                           "%3s%3s",
                           sizeof("%3s%3s"),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           String1,
                           String2);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 2) ||
        (RtlAreStringsEqual(String1, "123", 10) == FALSE) ||
        (RtlAreStringsEqual(String2, "456", 10) == FALSE)) {

        printf("ScanString: Failed to scan two consecutive strings.\n");
        Failures += 1;
    }

    Status = RtlStringScan("123",
                           sizeof("123"),
                           "%*d%d",
                           sizeof("%*d%d"),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           &Integer1);

    if ((Status != STATUS_END_OF_FILE) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail supressed then EOF scan.\n");
        Failures += 1;
    }

    Integer1 = 0;
    Status = RtlStringScan("123",
                           sizeof("123"),
                           "%*d%n",
                           sizeof("%*d%n"),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           &Integer1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 0) || (Integer1 != 3)) {
        printf("ScanString: Failed to count characters correctly.\n");
        Failures += 1;
    }

    Status = RtlStringScan("1 2 3 4",
                           sizeof("1 2 3 4"),
                           "%d %d %d %d%n",
                           sizeof("%d %d %d %d%n"),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           &Integer1,
                           &Integer2,
                           &Integer3,
                           &Integer4,
                           &Integer5);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 4) ||
        (Integer1 != 1) || (Integer2 != 2) || (Integer3 != 3) ||
        (Integer4 != 4) || (Integer5 != 7)) {

        printf("ScanString: Failed to count characters 2.\n");
        Failures += 1;
    }

    return Failures;
}

ULONG
TestScanIntegerWide (
    BOOL Quiet
    )

/*++

Routine Description:

    This routine tests the wide scan integer function.

Arguments:

    Quiet - Supplies a boolean indicating whether the test should be run
        without printouts (TRUE) or with debug output (FALSE).

Return Value:

    Returns the number of tests that failed.

--*/

{

    ULONG Failures;
    PCWSTR InputString;
    KSTATUS Status;
    PCWSTR String;
    ULONG StringSize;
    LONGLONG Value;

    Failures = 0;

    //
    // Test that all whitespace doesn't advance the string.
    //

    InputString = SCAN_STRING_BLANK_WIDE;
    String = InputString;
    StringSize = wcslen(SCAN_STRING_BLANK_WIDE) + 1;
    Status = RtlStringScanIntegerWide(&String, &StringSize, 0, TRUE, &Value);
    if ((Status != STATUS_END_OF_FILE) ||
        (String != InputString) ||
        (StringSize != wcslen(SCAN_STRING_BLANK_WIDE) + 1)) {

        printf("ScanInteger: Failed to not scan blank string.\n");
        Failures += 1;
    }

    //
    // Test the same thing for the empty string.
    //

    InputString = SCAN_EMPTY_STRING_WIDE;
    String = InputString;
    StringSize = wcslen(SCAN_EMPTY_STRING_WIDE) + 1;
    Status = RtlStringScanIntegerWide(&String, &StringSize, 0, TRUE, &Value);
    if ((Status != STATUS_END_OF_FILE) ||
        (String != InputString) ||
        (StringSize != wcslen(SCAN_EMPTY_STRING_WIDE) + 1)) {

        printf("ScanInteger: Failed to not scan empty string.\n");
        Failures += 1;
    }

    //
    // Scan an invalid string.
    //

    InputString = SCAN_INVALID_STRING_WIDE;
    String = InputString;
    StringSize = wcslen(SCAN_INVALID_STRING_WIDE) + 1;
    Status = RtlStringScanIntegerWide(&String, &StringSize, 0, TRUE, &Value);
    if ((Status != STATUS_INVALID_SEQUENCE) ||
        (String != InputString) ||
        (StringSize != wcslen(SCAN_INVALID_STRING_WIDE) + 1)) {

        printf("ScanInteger: Failed to not scan invalid string.\n");
        Failures += 1;
    }

    //
    // Scan a nice decimal integer automatically detecting the base.
    //

    InputString = SCAN_DECIMAL_INTEGER_WIDE;
    String = InputString;
    StringSize = wcslen(SCAN_DECIMAL_INTEGER_WIDE) + 1;
    Status = RtlStringScanIntegerWide(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_DECIMAL_INTEGER_LENGTH) ||
        (StringSize !=
         wcslen(SCAN_DECIMAL_INTEGER_WIDE) + 1 - SCAN_DECIMAL_INTEGER_LENGTH) ||
        (Value != SCAN_DECIMAL_INTEGER_VALUE)) {

        printf("ScanInteger: Failed to scan decimal integer string.\n");
        Failures += 1;
    }

    //
    // Scan an octal integer with non-octal digits on the end.
    //

    InputString = SCAN_OCTAL_INTEGER_WIDE;
    String = InputString;
    StringSize = wcslen(SCAN_OCTAL_INTEGER_WIDE) + 1;
    Status = RtlStringScanIntegerWide(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_OCTAL_INTEGER_LENGTH) ||
        (StringSize !=
         wcslen(SCAN_OCTAL_INTEGER_WIDE) + 1 - SCAN_OCTAL_INTEGER_LENGTH) ||
        (Value != SCAN_OCTAL_INTEGER_VALUE)) {

        printf("ScanInteger: Failed to scan octal integer string.\n");
        Failures += 1;
    }

    //
    // Scan a hexadecimal string on auto-detect.
    //

    InputString = SCAN_HEX_INTEGER_WIDE;
    String = InputString;
    StringSize = wcslen(SCAN_HEX_INTEGER_WIDE) + 1;
    Status = RtlStringScanIntegerWide(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_HEX_INTEGER_LENGTH) ||
        (StringSize !=
         wcslen(SCAN_HEX_INTEGER_WIDE) + 1 - SCAN_HEX_INTEGER_LENGTH) ||
        (Value != SCAN_HEX_INTEGER_VALUE)) {

        printf("ScanInteger: Failed to scan hex integer string.\n");
        Failures += 1;
    }

    //
    // Scan something with a weird base.
    //

    InputString = SCAN_BASE35_INTEGER_WIDE;
    String = InputString;
    StringSize = wcslen(SCAN_BASE35_INTEGER_WIDE) + 1;
    Status = RtlStringScanIntegerWide(&String, &StringSize, 35, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_BASE35_INTEGER_LENGTH) ||
        (StringSize !=
         wcslen(SCAN_BASE35_INTEGER_WIDE) + 1 - SCAN_BASE35_INTEGER_LENGTH) ||
        (Value != SCAN_BASE35_INTEGER_VALUE)) {

        printf("ScanInteger: Failed to scan base35 integer string.\n");
        Failures += 1;
    }

    //
    // Scan an 0xz, and watch out that only the zero gets scanned.
    //

    InputString = SCAN_0XZ_WIDE;
    String = InputString;
    StringSize = sizeof(SCAN_0XZ_WIDE);
    Status = RtlStringScanIntegerWide(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_0XZ_LENGTH) ||
        (StringSize !=
         sizeof(SCAN_0XZ_WIDE) - SCAN_0XZ_LENGTH) ||
        (Value != SCAN_0XZ_VALUE)) {

        printf("ScanInteger: Failed to scan 0xz.\n");
        Failures += 1;
    }

    //
    // Scan just a zero.
    //

    InputString = SCAN_ZERO_WIDE;
    String = InputString;
    StringSize = sizeof(SCAN_ZERO_WIDE);
    Status = RtlStringScanIntegerWide(&String, &StringSize, 0, TRUE, &Value);
    if ((!KSUCCESS(Status)) ||
        (String != InputString + SCAN_ZERO_LENGTH) ||
        (StringSize !=
         sizeof(SCAN_ZERO_WIDE) - SCAN_ZERO_LENGTH) ||
        (Value != SCAN_ZERO_VALUE)) {

        printf("ScanInteger: Failed to scan 0.\n");
        Failures += 1;
    }

    return Failures;
}

ULONG
TestScanDoubleWide (
    VOID
    )

/*++

Routine Description:

    This routine tests the wide scan double function.

Arguments:

    None.

Return Value:

    Returns the number of tests that failed.

--*/

{

    PCWSTR AfterScan;
    DOUBLE_PARTS AnswerParts;
    ULONGLONG Difference;
    ULONG Failures;
    double Result[SCAN_DOUBLE_COUNT];
    ULONG ResultCount;
    ULONG ResultIndex;
    DOUBLE_PARTS ResultParts;
    KSTATUS Status;
    WCHAR String[10];
    ULONG StringSize;

    Failures = 0;
    RtlZeroMemory(Result, sizeof(Result));
    Status = RtlStringScanWide(SCAN_DOUBLE_INPUT_WIDE,
                               MAX_ULONG,
                               SCAN_DOUBLE_FORMAT_WIDE,
                               MAX_ULONG,
                               CharacterEncodingDefault,
                               &ResultCount,
                               &(Result[0]),
                               &(Result[1]),
                               &(Result[2]),
                               &(Result[3]),
                               &(Result[4]),
                               &(Result[5]),
                               &(Result[6]),
                               &(Result[7]),
                               &(Result[8]),
                               &(Result[9]),
                               &(Result[10]),
                               &(Result[11]),
                               &(Result[12]),
                               &(Result[13]),
                               &(Result[14]),
                               &(Result[15]),
                               &(Result[16]),
                               &(Result[17]),
                               &(Result[18]),
                               &(Result[19]),
                               &(Result[20]),
                               &(Result[21]),
                               &(Result[22]),
                               &(Result[23]),
                               &(Result[24]),
                               &(Result[25]),
                               &(Result[26]),
                               &(Result[27]));

    if (!KSUCCESS(Status)) {
        printf("ScanDoubleWide: Failed to scan, status %d\n", Status);
        Failures += 1;
    }

    if (ResultCount != SCAN_DOUBLE_COUNT) {
        printf("ScanDoubleWide: Only scanned %d of %d items.\n",
               ResultCount,
               SCAN_DOUBLE_COUNT);

        Failures += 1;
    }

    for (ResultIndex = 0; ResultIndex < ResultCount; ResultIndex += 1) {
        ResultParts.Double = Result[ResultIndex];
        AnswerParts.Double = TestScanDoubleValues[ResultIndex];
        if (ResultParts.Ulonglong >= AnswerParts.Ulonglong) {
            Difference = ResultParts.Ulonglong - AnswerParts.Ulonglong;

        } else {
            Difference = AnswerParts.Ulonglong - ResultParts.Ulonglong;
        }

        if (Difference > SCAN_DOUBLE_PLAY) {
            printf("ScanDoubleWide: Item %d was %.13a (%.16g), should have "
                   "been %.13a (%.16g)\n",
                   ResultIndex,
                   ResultParts.Double,
                   ResultParts.Double,
                   AnswerParts.Double,
                   AnswerParts.Double);

            Failures += 1;
        }
    }

    //
    // Try nan() and nan(.
    //

    StringSize = sizeof(String);
    RtlStringCopyWide(String, L"nan()", StringSize);
    AfterScan = String;
    Status = RtlStringScanDoubleWide(&AfterScan, &StringSize, &(Result[0]));
    if ((!KSUCCESS(Status)) || (AfterScan != String + 5)) {
        printf("ScanDoubleWide: Failed to scan nan()\n");
        Failures += 1;
    }

    StringSize = sizeof(String);
    RtlStringCopyWide(String, L"nan(", StringSize);
    AfterScan = String;
    Status = RtlStringScanDoubleWide(&AfterScan, &StringSize, &(Result[0]));
    if ((!KSUCCESS(Status)) || (AfterScan != String + 3)) {
        printf("ScanDoubleWide: Failed to scan nan(\n");
        Failures += 1;
    }

    if (Failures != 0) {
        printf("%d ScanDoubleWide failures.\n", Failures);
    }

    return Failures;
}

ULONG
TestStringScannerWide (
    VOID
    )

/*++

Routine Description:

    This routine tests the wide string scanning functionality.

Arguments:

    None.

Return Value:

    Returns the number of test failures.

--*/

{

    INT BytesSoFar;
    ULONG Failures;
    INT Integer1;
    INT Integer2;
    INT Integer3;
    INT Integer6;
    INT Integer7;
    INT Integer8;
    ULONG ItemsScanned;
    LONGLONG LongInteger4;
    LONGLONG LongInteger5;
    KSTATUS Status;
    WCHAR String1[10];
    WCHAR String2[10];
    WCHAR String3[10];
    WCHAR String4[10];
    WCHAR String5[10];

    Failures = 0;

    //
    // Test basic functionality.
    //

    Integer1 = 0;
    Integer2 = 0;
    RtlZeroMemory(String1, sizeof(String1));
    RtlZeroMemory(String2, sizeof(String2));
    RtlZeroMemory(String3, sizeof(String3));
    ItemsScanned = 0;
    Status = RtlStringScanWide(SCAN_BASIC_INPUT_WIDE,
                               wcslen(SCAN_BASIC_INPUT_WIDE) + 1,
                               SCAN_BASIC_FORMAT_WIDE,
                               wcslen(SCAN_BASIC_FORMAT_WIDE) + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               &Integer1,
                               String1,
                               String2,
                               &Integer2,
                               &BytesSoFar,
                               String3);

    if ((!KSUCCESS(Status)) ||
        (ItemsScanned != SCAN_BASIC_ITEM_COUNT) ||
        (Integer1 != SCAN_BASIC_INTEGER1) ||
        (RtlAreStringsEqualWide(String1, SCAN_BASIC_STRING1_WIDE, 10) ==
         FALSE) ||
        (RtlAreStringsEqualWide(String2, SCAN_BASIC_STRING2_WIDE, 10) ==
         FALSE) ||
        (Integer2 != SCAN_BASIC_INTEGER2) ||
        (BytesSoFar != SCAN_BASIC_BYTES_SO_FAR) ||
        (RtlAreStringsEqualWide(String3, SCAN_BASIC_STRING3_WIDE, 10) ==
                                                                      FALSE)) {

        printf("ScanString: Failed to scan basic string.\n");
        Failures += 1;
    }

    //
    // Test the integer varieties, size overrides, and field lengths.
    //

    Integer1 = 0;
    Integer2 = 0;
    Integer3 = 0;
    LongInteger4 = 0;
    LongInteger5 = 0;
    Integer6 = 0;
    Integer7 = 0;
    Integer8 = 0;
    Status = RtlStringScanWide(SCAN_INTEGERS_INPUT_WIDE,
                               wcslen(SCAN_INTEGERS_INPUT_WIDE) + 1,
                               SCAN_INTEGERS_FORMAT_WIDE,
                               wcslen(SCAN_INTEGERS_FORMAT_WIDE) + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               &Integer1,
                               &Integer2,
                               &Integer3,
                               &LongInteger4,
                               &LongInteger5,
                               &Integer6,
                               &Integer7,
                               &Integer8,
                               &BytesSoFar);

    if ((!KSUCCESS(Status)) ||
        (ItemsScanned != SCAN_INTEGERS_ITEM_COUNT) ||
        (Integer1 != SCAN_INTEGERS_INTEGER1) ||
        (Integer2 != SCAN_INTEGERS_INTEGER2) ||
        (Integer3 != SCAN_INTEGERS_INTEGER3) ||
        (LongInteger4 != SCAN_INTEGERS_INTEGER4) ||
        (LongInteger5 != SCAN_INTEGERS_INTEGER5) ||
        (Integer6 != SCAN_INTEGERS_INTEGER6) ||
        (Integer7 != SCAN_INTEGERS_INTEGER7) ||
        (Integer8 != SCAN_INTEGERS_INTEGER8) ||
        (BytesSoFar != SCAN_INTEGERS_BYTES_SO_FAR)) {

        printf("ScanString: Failed to scan integers sequences.\n");
        Failures += 1;
    }

    //
    // Test some character sets.
    //

    Status = RtlStringScanWide(SCAN_SET_INPUT_WIDE,
                               wcslen(SCAN_SET_INPUT_WIDE) + 1,
                               SCAN_SET_FORMAT_WIDE,
                               wcslen(SCAN_SET_FORMAT_WIDE) + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               String1,
                               String2,
                               String3,
                               String4,
                               String5);

    if ((!KSUCCESS(Status)) ||
        (ItemsScanned != 5) ||
        (RtlAreStringsEqualWide(String1, SCAN_SET_STRING1_WIDE, 10) == FALSE) ||
        (RtlAreStringsEqualWide(String2, SCAN_SET_STRING2_WIDE, 10) == FALSE) ||
        (RtlAreStringsEqualWide(String3, SCAN_SET_STRING3_WIDE, 10) == FALSE) ||
        (RtlAreStringsEqualWide(String4, SCAN_SET_STRING4_WIDE, 10) == FALSE) ||
        (RtlAreStringsEqualWide(String5, SCAN_SET_STRING5_WIDE, 10) == FALSE)) {

        printf("ScanString: Failed to scan scan set input.\n");
        Failures += 1;
    }

    //
    // Try a bunch of formats that shouldn't work.
    //

    Status = RtlStringScanWide(SCAN_DUMMY_INPUT_WIDE,
                               wcslen(SCAN_DUMMY_INPUT_WIDE) + 1,
                               L"%",
                               wcslen(L"%") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 1.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(SCAN_DUMMY_INPUT_WIDE,
                               wcslen(SCAN_DUMMY_INPUT_WIDE) + 1,
                               L"%301",
                               wcslen(L"%301") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 2.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(SCAN_DUMMY_INPUT_WIDE,
                               wcslen(SCAN_DUMMY_INPUT_WIDE) + 1,
                               L"%ll",
                               wcslen(L"%ll") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 3.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(SCAN_DUMMY_INPUT_WIDE,
                               wcslen(SCAN_DUMMY_INPUT_WIDE) + 1,
                               L"%C",
                               1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 4.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(SCAN_DUMMY_INPUT_WIDE,
                               wcslen(SCAN_DUMMY_INPUT_WIDE) + 1,
                               L"%30l[",
                               wcslen(L"%30l[") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 5.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(SCAN_DUMMY_INPUT_WIDE,
                               wcslen(SCAN_DUMMY_INPUT_WIDE) + 1,
                               L"%l[^",
                               wcslen(L"%l[^") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 6.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(SCAN_DUMMY_INPUT_WIDE,
                               wcslen(SCAN_DUMMY_INPUT_WIDE) + 1,
                               L"%l[]aaa",
                               wcslen(L"%l[]aaa") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 7.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(SCAN_DUMMY_INPUT_WIDE,
                               wcslen(SCAN_DUMMY_INPUT_WIDE) + 1,
                               L"%0S",
                               wcslen(L"%0S") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 8.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(SCAN_DUMMY_INPUT_WIDE,
                               wcslen(SCAN_DUMMY_INPUT_WIDE) + 1,
                               L"%jj",
                               wcslen(L"%jj") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 9.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(L" ",
                               wcslen(L" ") + 1,
                               L"%S",
                               wcslen(L"%S") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail invalid string 10.\n");
        Failures += 1;
    }

    //
    // Try scanning stuff passing in the empty string as input.
    //

    Status = RtlStringScanWide(L"",
                               wcslen(L"") + 1,
                               L"%C",
                               wcslen(L"%C") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail empty string 1.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(L"",
                               wcslen(L"") + 1,
                               L"%lld",
                               wcslen(L"%lld") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail empty string 2.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(L"",
                               wcslen(L"") + 1,
                               L"%l[a]",
                               wcslen(L"%l[a]") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned);

    if ((KSUCCESS(Status)) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail empty string 3.\n");
        Failures += 1;
    }

    //
    // See if the input string goes out of bounds.
    //

    Integer1 = 0;
    Status = RtlStringScanWide(L"123456",
                               wcslen(L"1234") + 1 - 1,
                               L"%d",
                               wcslen(L"%d") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               &Integer1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 1) || (Integer1 != 1234)) {
        printf("ScanString: Failed to stop integer at input boundary.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(L"  ASDFASDF",
                               wcslen(L"  ASDF") + 1 - 1,
                               L"%ls",
                               wcslen(L"%ls") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               String1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 1) ||
        (RtlAreStringsEqualWide(String1, L"ASDF", 10) == FALSE)) {

        printf("ScanString: Failed to stop string at input boundary.\n");
        Failures += 1;
    }

    RtlZeroMemory(String1, sizeof(String1));
    Status = RtlStringScanWide(L" ASDF",
                               wcslen(L" A") + 1 - 1,
                               L"%10lc",
                               wcslen(L"%10lc") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               String1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 1) ||
        (RtlAreStringsEqualWide(String1, L" A", 10) == FALSE)) {

        printf("ScanString: Failed to stop characters at input boundary.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(L"ASDF",
                               wcslen(L"AS") + 1 - 1,
                               L"%10l[SDFA]",
                               wcslen(L"%10l[SDFA]") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               String1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 1) ||
        (RtlAreStringsEqualWide(String1, L"AS", 10) == FALSE)) {

        printf("ScanString: Failed to stop scanset at input boundary.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(L"123456",
                               wcslen(L"123456") + 1,
                               L"%3S%3S",
                               wcslen(L"%3S%3S") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               String1,
                               String2);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 2) ||
        (RtlAreStringsEqualWide(String1, L"123", 10) == FALSE) ||
        (RtlAreStringsEqualWide(String2, L"456", 10) == FALSE)) {

        printf("ScanString: Failed to scan two consecutive strings.\n");
        Failures += 1;
    }

    Status = RtlStringScanWide(L"123",
                               wcslen(L"123") + 1,
                               L"%*d%d",
                               wcslen(L"%*d%d") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               &Integer1);

    if ((Status != STATUS_END_OF_FILE) || (ItemsScanned != 0)) {
        printf("ScanString: Failed to fail supressed then EOF scan.\n");
        Failures += 1;
    }

    Integer1 = 0;
    Status = RtlStringScanWide(L"123",
                               wcslen(L"123") + 1,
                               L"%*d%n",
                               wcslen(L"%*d%n") + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               &Integer1);

    if ((!KSUCCESS(Status)) || (ItemsScanned != 0) || (Integer1 != 3)) {
        printf("ScanString: Failed to count characters correctly.\n");
        Failures += 1;
    }

    return Failures;
}

VOID
KdPrintWithArgumentList (
    PCSTR Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine prints a string to the debugger. Currently the maximum length
    string is a little less than one debug packet.

Arguments:

    Format - Supplies a pointer to the printf-like format string.

    ArgumentList - Supplies a pointer to the initialized list of arguments
        required for the format string.

Return Value:

    None.

--*/

{

    vfprintf(stderr, Format, ArgumentList);
    return;
}

