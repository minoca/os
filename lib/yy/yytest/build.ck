/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    YyTest

Abstract:

    This program compiles the Lexer/Parser Library into an application and
    tests it.

Author:

    Evan Green 9-Oct-2015

Environment:

    Test

--*/

function build() {
    sources = [
        "yytest.c"
    ];

    build_libs = [
        "//lib/yy:build_yy",
        "//lib/rtl/rtlc:build_rtlc",
        "//lib/rtl/base:build_basertl"
    ];

    build_app = {
        "label": "build_yytest",
        "output": "yytest",
        "inputs": sources + build_libs,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(build_app);
    return entries;
}

return build();

