/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    TzComp

Abstract:

    This tool compiles textual time zone data into a binary format.

Author:

    Evan Green 2-Aug-2013

Environment:

    Test

--*/

function build() {
    tz_default = "America/Los_Angeles";
    sources = [
        "tzcomp.c",
    ];

    tz_data_dir = "//tzcomp/data/";
    tz_source_files = [
        "africa",
        "antarctica",
        "asia",
        "australasia",
        "etcetera",
        "europe",
        "leapseconds",
        "northamerica",
        "southamerica"
    ];

    tz_files = [];
    for (file in tz_source_files) {
        tz_files += [tz_data_dir + file];
    }

    build_app = {
        "label": "build_tzcomp",
        "output": "tzcomp",
        "inputs": sources,
        "build": TRUE
    };

    entries = application(build_app);

    //
    // Add the tzcomp tool.
    //

    tzcomp_tool = {
        "type": "tool",
        "name": "tzcomp",
        "command": "$^//tzcomp/tzcomp $TZCOMP_FLAGS -o $OUT $IN",
        "description": "Compiling Time Zone Data - $OUT"
    };

    entries += [tzcomp_tool];

    //
    // Add entries for the time zone almanac and time zone default.
    //

    almanac = {
        "type": "target",
        "label": "tzdata",
        "inputs": tz_files,
        "implicit": [":build_tzcomp"],
        "tool": "tzcomp",
        "nostrip": TRUE
    };

    tz_default_config = {
        "TZCOMP_FLAGS": ["-f " + tz_default]
    };

    tz_default_data = {
        "type": "target",
        "label": "tzdflt",
        "inputs": tz_files,
        "implicit": [":build_tzcomp"],
        "tool": "tzcomp",
        "config": tz_default_config,
        "nostrip": TRUE
    };

    entries += binplace(almanac);
    entries += binplace(tz_default_data);

    //
    // Create a group for the data files.
    //

    tz_data_files = [":tzdata", ":tzdflt"];
    entries += group("tz_files", tz_data_files);
    return entries;
}

return build();

