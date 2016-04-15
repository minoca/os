/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Libraries

Abstract:

    This directory builds common libraries that run in multiple
    environments.

Author:

    Evan Green 28-Mar-2014

Environment:

    Any

--*/

function build() {
    test_apps = [
        "//lib/crypto/testcryp:",
        "//lib/fatlib/fattest:",
        "//lib/rtl/testrtl:",
        "//lib/yy/yytest:",
        "//kernel/mm/testmm:",
    ];

    entries = group("test_apps", test_apps);
    return entries;
}

return build();
