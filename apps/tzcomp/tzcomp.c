/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tzcomp.c

Abstract:

    This module implements the time zone compiler program, which translates
    time zone data into a binary format.

Author:

    Evan Green 2-Aug-2013

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>
#include <minoca/lib/tzfmt.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define TIME_ZONE_COMPILER_VERSION_MAJOR 1
#define TIME_ZONE_COMPILER_VERSION_MINOR 1

#define TIME_ZONE_COMPILER_USAGE                                               \
    "Usage: tzcomp [-p] [-f <zone>] [-o <outputfile>] [files...]\n"            \
    "The tzcomp utility compiles standard time zone data files into a binary " \
    "format. Options are:\n\n"                                                 \
    "  -o, --output=<file> -- Write the output to the given file rather \n"    \
    "      than the default file name \"" TIME_ZONE_DEFAULT_OUTPUT_FILE        \
    "\".\n\n"                                                                  \
    "  -v, --verbose -- Print the parsed results coming from the input files." \
    "\n"                                                                       \
    "  -y, --year=<year> -- Write only zone information newer than the \n"     \
    "given year.\n"                                                            \
    "  -z, --zone=<zone> -- Produce output only for the time zone of the \n"   \
    "      given name.\n"                                                      \

#define INITIAL_MALLOC_SIZE 32

#define TIME_ZONE_DEFAULT_OUTPUT_FILE "tzdata"

#define TZCOMP_OPTIONS_STRING "ho:vy:z:V"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TZC_RULE_FIELD {
    RuleFieldMagic,
    RuleFieldName,
    RuleFieldFrom,
    RuleFieldTo,
    RuleFieldType,
    RuleFieldIn,
    RuleFieldOn,
    RuleFieldAt,
    RuleFieldSave,
    RuleFieldLetters,
    RuleFieldCount
} TZC_RULE_FIELD, *PTZC_RULE_FIELD;

typedef enum _TZC_ZONE_FIELD {
    ZoneFieldMagic,
    ZoneFieldName,
    ZoneFieldGmtOffset,
    ZoneFieldRules,
    ZoneFieldFormat,
    ZoneFieldUntilYear,
    ZoneFieldUntilMonth,
    ZoneFieldUntilDay,
    ZoneFieldUntilTime,
    ZoneFieldCount
} TZC_ZONE_FIELD, *PTZC_ZONE_FIELD;

typedef enum _TZC_LINK_FIELD {
    LinkFieldMagic,
    LinkFieldFrom,
    LinkFieldTo,
    LinkFieldCount
} TZC_LINK_FIELD, *PTZC_LINK_FIELD;

typedef enum _TZC_LEAP_FIELD {
    LeapFieldMagic,
    LeapFieldYear,
    LeapFieldMonth,
    LeapFieldDay,
    LeapFieldTime,
    LeapFieldCorrection,
    LeapFieldRollingOrStationary,
    LeapFieldCount
} TZC_LEAP_FIELD, *PTZC_LEAP_FIELD;

typedef struct _TZC_RULE {
    LIST_ENTRY ListEntry;
    ULONG NameIndex;
    SHORT From;
    SHORT To;
    TIME_ZONE_MONTH Month;
    TIME_ZONE_OCCASION On;
    LONG At;
    TIME_ZONE_LENS AtLens;
    LONG Save;
    ULONG LettersOffset;
} TZC_RULE, *PTZC_RULE;

typedef struct _TZC_ZONE {
    LIST_ENTRY ListEntry;
    ULONG NameOffset;
    ULONG ZoneEntryIndex;
    ULONG ZoneEntryCount;
} TZC_ZONE, *PTZC_ZONE;

typedef struct _TZC_LINK {
    LIST_ENTRY ListEntry;
    PSTR From;
    PSTR To;
} TZC_LINK, *PTZC_LINK;

typedef struct _TZC_ZONE_ENTRY {
    LIST_ENTRY ListEntry;
    ULONG Index;
    LONG GmtOffset;
    ULONG RulesNameIndex;
    LONG Save;
    ULONG FormatOffset;
    LONGLONG Until;
} TZC_ZONE_ENTRY, *PTZC_ZONE_ENTRY;

typedef struct _TZC_LEAP {
    LIST_ENTRY ListEntry;
    LONGLONG Date;
    CHAR Positive;
    CHAR LocalTime;
} TZC_LEAP, *PTZC_LEAP;

typedef struct _TZC_STRING {
    LIST_ENTRY ListEntry;
    ULONG Offset;
    PSTR String;
} TZC_STRING, *PTZC_STRING;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ReadTimeZoneFile (
    PSTR FilePath
    );

INT
ProcessTimeZoneRule (
    PSTR *Fields,
    ULONG FieldCount
    );

INT
ProcessTimeZone (
    PSTR *Fields,
    ULONG FieldCount,
    PBOOL Continuation
    );

INT
ProcessTimeZoneLink (
    PSTR *Fields,
    ULONG FieldCount
    );

INT
ProcessTimeZoneLeap (
    PSTR *Fields,
    ULONG FieldCount
    );

INT
ReadTimeZoneFields (
    FILE *File,
    PVOID *LineBuffer,
    PULONG LineBufferSize,
    PSTR **Fields,
    PULONG FieldsSize,
    PULONG FieldCount,
    PBOOL EndOfFile
    );

INT
ReadTimeZoneLine (
    FILE *File,
    PVOID *LineBuffer,
    PULONG LineBufferSize,
    PBOOL EndOfFile
    );

INT
TranslateLinksToZones (
    );

INT
TimeZoneFilter (
    PSTR Name,
    INT Year
    );

INT
WriteTimeZoneData (
    PSTR FileName
    );

VOID
DestroyTimeZoneRule (
    PTZC_RULE Rule
    );

VOID
DestroyTimeZone (
    PTZC_ZONE Zone
    );

VOID
DestroyTimeZoneEntry (
    PTZC_ZONE_ENTRY ZoneEntry
    );

VOID
DestroyTimeZoneLink (
    PTZC_LINK Link
    );

VOID
DestroyTimeZoneLeap (
    PTZC_LEAP Leap
    );

VOID
DestroyTimeZoneStringList (
    PLIST_ENTRY ListHead
    );

INT
ParseTimeZoneRuleLimit (
    PSTR Field,
    SHORT OnlyValue,
    PSHORT Value
    );

INT
ParseTimeZoneMonth (
    PSTR Field,
    PTIME_ZONE_MONTH Value
    );

INT
ParseTimeZoneRuleWeekday (
    PSTR Field,
    PTIME_ZONE_WEEKDAY Value
    );

INT
ParseTimeZoneOccasion (
    PSTR Field,
    PTIME_ZONE_OCCASION Occasion
    );

INT
ParseTimeZoneTime (
    PSTR Field,
    PLONG Time,
    PTIME_ZONE_LENS Lens
    );

VOID
PrintTimeZoneRule (
    PTZC_RULE Rule
    );

VOID
PrintTimeZone (
    PTZC_ZONE Zone
    );

VOID
PrintTimeZoneLink (
    PTZC_LINK Link
    );

VOID
PrintTimeZoneLeap (
    PTZC_LEAP Leap
    );

VOID
PrintTimeZoneEntry (
    PTZC_ZONE_ENTRY ZoneEntry
    );

VOID
PrintTimeZoneTime (
    LONG Time,
    TIME_ZONE_LENS Lens
    );

VOID
PrintTimeZoneDate (
    LONGLONG Date
    );

INT
CalculateOccasionForDate (
    PTIME_ZONE_OCCASION Occasion,
    INT Year,
    TIME_ZONE_MONTH Month,
    PINT Date
    );

INT
CalculateWeekdayForMonth (
    INT Year,
    TIME_ZONE_MONTH Month,
    PTIME_ZONE_WEEKDAY Weekday
    );

LONG
ComputeDaysForYear (
    INT Year
    );

INT
ComputeYearForDays (
    PLONG Days
    );

PSTR
TimeZoneGetString (
    PLIST_ENTRY ListHead,
    ULONG Offset
    );

INT
TimeZoneAddString (
    PSTR String,
    PULONG Offset
    );

INT
TimeZoneAddRuleString (
    PSTR String,
    PULONG Index
    );

INT
TimeZoneAddStringToList (
    PSTR String,
    PLIST_ENTRY ListHead,
    PULONG ListSize,
    BOOL TrackSize,
    PULONG Offset
    );

VOID
TimeZoneCompressEntries (
    PLIST_ENTRY ZoneEntryList,
    PULONG ZoneEntryCount,
    PTZC_ZONE Zone
    );

//
// -------------------------------------------------------------------- Globals
//

struct option TzcompLongOptions[] = {
    {"output", required_argument, 0, 'o'},
    {"verbose", no_argument, 0, 'v'},
    {"year", required_argument, 0, 'y'},
    {"zone", required_argument, 0, 'z'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

LIST_ENTRY TimeZoneRuleList;
LIST_ENTRY TimeZoneList;
LIST_ENTRY TimeZoneEntryList;
LIST_ENTRY TimeZoneLinkList;
LIST_ENTRY TimeZoneLeapList;

//
// Store the string table and next free offset into it.
//

LIST_ENTRY TimeZoneStringList;
ULONG TimeZoneNextStringOffset;

//
// Store the string table for the rule list (which will eventually be
// discarded) and the next valid rule number.
//

LIST_ENTRY TimeZoneRuleStringList;
ULONG TimeZoneNextRuleNumber;

ULONG TimeZoneNextZoneEntryIndex;
ULONG TimeZoneRuleCount;
ULONG TimeZoneCount;
ULONG TimeZoneLeapCount;

PSTR TimeZoneMonthStrings[TimeZoneMonthCount] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

PSTR TimeZoneAbbreviatedMonthStrings[TimeZoneMonthCount] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

PSTR TimeZoneWeekdayStrings[TimeZoneWeekdayCount] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

PSTR TimeZoneAbbreviatedWeekdayStrings[TimeZoneWeekdayCount] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

CHAR TimeZoneDaysPerMonth[2][TimeZoneMonthCount] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

SHORT TimeZoneMonthDays[2][TimeZoneMonthCount] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
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

    This routine is the main entry point for the time zone compiler program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    PLIST_ENTRY CurrentEntry;
    PSTR FilterZone;
    PTZC_LEAP Leap;
    PTZC_LINK Link;
    INT Option;
    PSTR OutputName;
    BOOL PrintParsedEntries;
    INT Result;
    PTZC_RULE Rule;
    ULONG UnusedIndex;
    INT YearFilter;
    PTZC_ZONE Zone;
    PTZC_ZONE_ENTRY ZoneEntry;

    INITIALIZE_LIST_HEAD(&TimeZoneRuleList);
    INITIALIZE_LIST_HEAD(&TimeZoneList);
    INITIALIZE_LIST_HEAD(&TimeZoneEntryList);
    INITIALIZE_LIST_HEAD(&TimeZoneLinkList);
    INITIALIZE_LIST_HEAD(&TimeZoneLeapList);
    INITIALIZE_LIST_HEAD(&TimeZoneStringList);
    INITIALIZE_LIST_HEAD(&TimeZoneRuleStringList);
    FilterZone = NULL;
    OutputName = TIME_ZONE_DEFAULT_OUTPUT_FILE;
    PrintParsedEntries = FALSE;
    Result = 0;
    YearFilter = 0;
    if (ArgumentCount <= 1) {
        fprintf(stderr, TIME_ZONE_COMPILER_USAGE);
        return 1;
    }

    //
    // Add the null strings to the first positions.
    //

    TimeZoneAddString("", &UnusedIndex);
    TimeZoneAddRuleString("", &UnusedIndex);

    //
    // Loop through the arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             TZCOMP_OPTIONS_STRING,
                             TzcompLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Result = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'o':
            OutputName = optarg;
            break;

        case 'v':
            PrintParsedEntries = TRUE;
            break;

        case 'y':
            YearFilter = strtoul(optarg, NULL, 10);
            if ((YearFilter <= 0) || (YearFilter > 9999)) {
                fprintf(stderr, "Invalid year %s\n", optarg);
                Result = 1;
                goto MainEnd;
            }

            break;

        case 'z':
            FilterZone = optarg;
            break;

        case 'h':
            printf(TIME_ZONE_COMPILER_USAGE);
            return 1;

        case 'V':
            printf("Tzcomp version %d.%d\n",
                   TIME_ZONE_COMPILER_VERSION_MAJOR,
                   TIME_ZONE_COMPILER_VERSION_MINOR);

            return 1;

        default:

            assert(FALSE);

            Result = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        Result = ReadTimeZoneFile(Argument);
        if (Result != 0) {
            fprintf(stderr,
                    "tzcomp: Failed to process time zone data file %s.\n",
                    Argument);

            goto MainEnd;
        }

        ArgumentIndex += 1;
    }

    Result = TranslateLinksToZones();
    if (Result != 0) {
        goto MainEnd;
    }

    //
    // Filter for the requested name and/or years if requested.
    //

    if ((FilterZone != NULL) || (YearFilter != 0)) {
        Result = TimeZoneFilter(FilterZone, YearFilter);
        if (Result != 0) {
            fprintf(stderr,
                    "tzcomp: Error: Failed to filter time zone: %s.\n",
                    strerror(errno));

            goto MainEnd;
        }
    }

    //
    // If requested, print all the parsed entries.
    //

    if (PrintParsedEntries != FALSE) {
        CurrentEntry = TimeZoneRuleList.Next;
        while (CurrentEntry != &TimeZoneRuleList) {
            Rule = LIST_VALUE(CurrentEntry, TZC_RULE, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            PrintTimeZoneRule(Rule);
        }

        CurrentEntry = TimeZoneList.Next;
        while (CurrentEntry != &TimeZoneList) {
            Zone = LIST_VALUE(CurrentEntry, TZC_ZONE, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            PrintTimeZone(Zone);
        }

        CurrentEntry = TimeZoneLinkList.Next;
        while (CurrentEntry != &TimeZoneLinkList) {
            Link = LIST_VALUE(CurrentEntry, TZC_LINK, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            PrintTimeZoneLink(Link);
        }

        CurrentEntry = TimeZoneLeapList.Next;
        while (CurrentEntry != &TimeZoneLeapList) {
            Leap = LIST_VALUE(CurrentEntry, TZC_LEAP, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            PrintTimeZoneLeap(Leap);
        }
    }

    Result = WriteTimeZoneData(OutputName);
    if (Result != 0) {
        fprintf(stderr,
                "tzcomp: Error: Failed to write time zone data: %s.\n",
                strerror(errno));

        goto MainEnd;
    }

MainEnd:
    while (LIST_EMPTY(&TimeZoneRuleList) == FALSE) {
        Rule = LIST_VALUE(TimeZoneRuleList.Next, TZC_RULE, ListEntry);
        LIST_REMOVE(&(Rule->ListEntry));
        DestroyTimeZoneRule(Rule);
    }

    while (LIST_EMPTY(&TimeZoneList) == FALSE) {
        Zone = LIST_VALUE(TimeZoneList.Next, TZC_ZONE, ListEntry);
        LIST_REMOVE(&(Zone->ListEntry));
        DestroyTimeZone(Zone);
    }

    while (LIST_EMPTY(&TimeZoneLinkList) == FALSE) {
        Link = LIST_VALUE(TimeZoneLinkList.Next, TZC_LINK, ListEntry);
        LIST_REMOVE(&(Link->ListEntry));
        DestroyTimeZoneLink(Link);
    }

    while (LIST_EMPTY(&TimeZoneLeapList) == FALSE) {
        Leap = LIST_VALUE(TimeZoneLeapList.Next, TZC_LEAP, ListEntry);
        LIST_REMOVE(&(Leap->ListEntry));
        DestroyTimeZoneLeap(Leap);
    }

    while (LIST_EMPTY(&TimeZoneEntryList) == FALSE) {
        ZoneEntry = LIST_VALUE(TimeZoneEntryList.Next,
                               TZC_ZONE_ENTRY,
                               ListEntry);

        LIST_REMOVE(&(ZoneEntry->ListEntry));
        DestroyTimeZoneEntry(ZoneEntry);
    }

    DestroyTimeZoneStringList(&TimeZoneStringList);
    DestroyTimeZoneStringList(&TimeZoneRuleStringList);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ReadTimeZoneFile (
    PSTR FilePath
    )

/*++

Routine Description:

    This routine reads in time zone data from a file.

Arguments:

    FilePath - Supplies a pointer to the file path of the data to read in.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    BOOL EndOfFile;
    ULONG FieldCount;
    PSTR *Fields;
    ULONG FieldsSize;
    FILE *File;
    PVOID LineBuffer;
    ULONG LineBufferSize;
    ULONG LineNumber;
    INT Result;
    BOOL ZoneContinuation;

    EndOfFile = FALSE;
    FieldsSize = 0;
    Fields = NULL;
    LineBuffer = NULL;
    LineBufferSize = 0;
    ZoneContinuation = FALSE;
    File = fopen(FilePath, "rb");
    if (File == NULL) {
        fprintf(stderr,
                "tzcomp: Failed to open %s: %s\n",
                FilePath,
                strerror(errno));

        Result = errno;
        goto ReadTimeZoneFileEnd;
    }

    //
    // Loop reading and processing lines.
    //

    LineNumber = 1;
    while (TRUE) {
        Result = ReadTimeZoneFields(File,
                                    &LineBuffer,
                                    &LineBufferSize,
                                    &Fields,
                                    &FieldsSize,
                                    &FieldCount,
                                    &EndOfFile);

        if (Result != 0) {
            fprintf(stderr,
                    "tzcomp: Failed to read line %s:%d: %s\n",
                    FilePath,
                    LineNumber,
                    strerror(Result));

            goto ReadTimeZoneFileEnd;
        }

        //
        // Process the line according to its type.
        //

        if (FieldCount != 0) {
            Result = 0;
            if ((ZoneContinuation != FALSE) ||
                (strcasecmp(Fields[0], "Zone") == 0)) {

                Result = ProcessTimeZone(Fields, FieldCount, &ZoneContinuation);

            } else if (strcasecmp(Fields[0], "Rule") == 0) {
                Result = ProcessTimeZoneRule(Fields, FieldCount);

            } else if (strcasecmp(Fields[0], "Link") == 0) {
                Result = ProcessTimeZoneLink(Fields, FieldCount);

            } else if (strcasecmp(Fields[0], "Leap") == 0) {
                Result = ProcessTimeZoneLeap(Fields, FieldCount);
            }

            if (Result != 0) {
                fprintf(stderr,
                        "tzcomp: Failed to process line %s:%d: %s.\n",
                        FilePath,
                        LineNumber,
                        strerror(Result));

                goto ReadTimeZoneFileEnd;
            }
        }

        if (EndOfFile != FALSE) {
            break;
        }

        LineNumber += 1;
    }

    Result = 0;

ReadTimeZoneFileEnd:
    if (File != NULL) {
        fclose(File);
    }

    if (LineBuffer != NULL) {
        free(LineBuffer);
    }

    if (Fields != NULL) {
        free(Fields);
    }

    return Result;
}

INT
ProcessTimeZoneRule (
    PSTR *Fields,
    ULONG FieldCount
    )

/*++

Routine Description:

    This routine processes a time zone rule line.

Arguments:

    Fields - Supplies a pointer to the fields in the line.

    FieldCount - Supplies the number of elements in the fields array.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;
    PTZC_RULE Rule;

    //
    // Validate the number of fields and the first field.
    //

    assert(strcasecmp(Fields[RuleFieldMagic], "Rule") == 0);

    Rule = NULL;
    if (FieldCount != RuleFieldCount) {
        fprintf(stderr,
                "tzcomp: Expected %d fields in a Rule, got %d.\n",
                RuleFieldCount,
                FieldCount);

        Result = EILSEQ;
        goto ProcessTimeZoneRuleEnd;
    }

    //
    // Allocate the new rule structure.
    //

    Rule = malloc(sizeof(TZC_RULE));
    if (Rule == NULL) {
        Result = ENOMEM;
        goto ProcessTimeZoneRuleEnd;
    }

    memset(Rule, 0, sizeof(TZC_RULE));

    //
    // Copy the name.
    //

    Result = TimeZoneAddRuleString(Fields[RuleFieldName], &(Rule->NameIndex));
    if (Result != 0) {
        goto ProcessTimeZoneRuleEnd;
    }

    //
    // Parse the FROM and TO years.
    //

    Result = ParseTimeZoneRuleLimit(Fields[RuleFieldFrom],
                                    MIN_TIME_ZONE_YEAR,
                                    &(Rule->From));

    if (Result != 0) {
        fprintf(stderr,
                "Failed to process Rule FROM: %s\n",
                Fields[RuleFieldFrom]);

        goto ProcessTimeZoneRuleEnd;
    }

    Result = ParseTimeZoneRuleLimit(Fields[RuleFieldTo],
                                    Rule->From,
                                    &(Rule->To));

    if (Result != 0) {
        fprintf(stderr,
                "Failed to process Rule TO: %s\n",
                Fields[RuleFieldTo]);

        goto ProcessTimeZoneRuleEnd;
    }

    //
    // Skip over the type field.
    //

    if (strcmp(Fields[RuleFieldType], "-") != 0) {
        fprintf(stderr,
                "Warning: Ignoring rule type %s.\n",
                Fields[RuleFieldType]);
    }

    //
    // Parse the IN month.
    //

    Result = ParseTimeZoneMonth(Fields[RuleFieldIn], &(Rule->Month));
    if (Result != 0) {
        fprintf(stderr,
                "Failed to process Rule IN: %s\n",
                Fields[RuleFieldIn]);

        goto ProcessTimeZoneRuleEnd;
    }

    //
    // Parse the ON occasion.
    //

    Result = ParseTimeZoneOccasion(Fields[RuleFieldOn], &(Rule->On));
    if (Result != 0) {
        fprintf(stderr,
                "Failed to process Rule ON: %s\n",
                Fields[RuleFieldOn]);

        goto ProcessTimeZoneRuleEnd;
    }

    //
    // Parse the AT time.
    //

    Result = ParseTimeZoneTime(Fields[RuleFieldAt],
                               &(Rule->At),
                               &(Rule->AtLens));

    if (Result != 0) {
        fprintf(stderr,
                "Failed to process Rule AT: %s\n",
                Fields[RuleFieldAt]);

        goto ProcessTimeZoneRuleEnd;
    }

    //
    // Parse the SAVE time.
    //

    Result = ParseTimeZoneTime(Fields[RuleFieldSave], &(Rule->Save), NULL);
    if (Result != 0) {
        fprintf(stderr,
                "Failed to process Rule SAVE: %s\n",
                Fields[RuleFieldSave]);

        goto ProcessTimeZoneRuleEnd;
    }

    //
    // Copy the letters.
    //

    Result = TimeZoneAddString(Fields[RuleFieldLetters],
                               &(Rule->LettersOffset));

    if (Result != 0) {
        goto ProcessTimeZoneRuleEnd;
    }

    INSERT_BEFORE(&(Rule->ListEntry), &TimeZoneRuleList);
    TimeZoneRuleCount += 1;
    Result = 0;

ProcessTimeZoneRuleEnd:
    if (Result != 0) {
        if (Rule != NULL) {
            DestroyTimeZoneRule(Rule);
        }
    }

    return Result;
}

INT
ProcessTimeZone (
    PSTR *Fields,
    ULONG FieldCount,
    PBOOL Continuation
    )

/*++

Routine Description:

    This routine processes a time zone line.

Arguments:

    Fields - Supplies a pointer to the fields in the line.

    FieldCount - Supplies the number of elements in the fields array.

    Continuation - Supplies a pointer that on input contains whether or not
        this zone line is a continuation line. On output, this variable will
        be set to indicate whether another continuation line is expected.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    INT Day;
    PSTR Field;
    ULONG FieldOffset;
    INT Leap;
    TIME_ZONE_MONTH Month;
    TIME_ZONE_OCCASION Occasion;
    INT Result;
    LONG ScannedValue;
    TIME_ZONE_LENS UntilLens;
    LONG UntilTime;
    BOOL UntilValid;
    INT Year;
    PTZC_ZONE Zone;
    PTZC_ZONE_ENTRY ZoneEntry;

    UntilValid = FALSE;
    Zone = NULL;
    ZoneEntry = NULL;

    //
    // If this is a continuation, then get the most recent zone and set the
    // field offset since the "Zone" and Name fields will be missing.
    //

    if (*Continuation != FALSE) {
        FieldOffset = ZoneFieldGmtOffset;
        if (FieldCount < ZoneFieldFormat - FieldOffset) {
            fprintf(stderr,
                    "Error: Not enough fields for zone continuation.\n");

            Result = EILSEQ;
            goto ProcessTimeZoneEnd;
        }

        Zone = LIST_VALUE(TimeZoneList.Previous, TZC_ZONE, ListEntry);

    //
    // This is not a continuation.
    //

    } else {

        assert(strcasecmp(Fields[ZoneFieldMagic], "Zone") == 0);

        FieldOffset = 0;
        if (FieldCount < ZoneFieldRules) {
            fprintf(stderr,
                    "Error: Not enough fields for zone line.\n");

            Result = EILSEQ;
            goto ProcessTimeZoneEnd;
        }

        Zone = malloc(sizeof(TZC_ZONE));
        if (Zone == NULL) {
            Result = ENOMEM;
            goto ProcessTimeZoneEnd;
        }

        memset(Zone, 0, sizeof(TZC_ZONE));

        //
        // Copy the name.
        //

        Result = TimeZoneAddString(Fields[ZoneFieldName], &(Zone->NameOffset));
        if (Result != 0) {
            free(Zone);
            goto ProcessTimeZoneEnd;
        }

        Zone->ZoneEntryIndex = TimeZoneNextZoneEntryIndex;
        INSERT_BEFORE(&(Zone->ListEntry), &TimeZoneList);
        TimeZoneCount += 1;
    }

    //
    // Create the zone entry.
    //

    ZoneEntry = malloc(sizeof(TZC_ZONE_ENTRY));
    if (ZoneEntry == NULL) {
        Result = ENOMEM;
        goto ProcessTimeZoneEnd;
    }

    memset(ZoneEntry, 0, sizeof(TZC_ZONE_ENTRY));
    ZoneEntry->Until = MAX_TIME_ZONE_DATE;
    ZoneEntry->Index = TimeZoneNextZoneEntryIndex;
    TimeZoneNextZoneEntryIndex += 1;

    //
    // Get the GMT offset time.
    //

    Field = Fields[ZoneFieldGmtOffset - FieldOffset];
    Result = ParseTimeZoneTime(Field, &(ZoneEntry->GmtOffset), NULL);
    if (Result != 0) {
        fprintf(stderr,
                "Error: Failed to parse Zone GMTOFFSET %s.\n",
                Field);

        goto ProcessTimeZoneEnd;
    }

    //
    // Get the rules string. If it's a -, then this zone is always in
    // standard time. Otherwise, it could be a save value (like 1:00) or
    // the name of a set of rules.
    //

    Field = Fields[ZoneFieldRules - FieldOffset];
    ZoneEntry->RulesNameIndex = -1;
    if (strcmp(Field, "-") != 0) {
        if ((*Field == '-') || (isdigit(*Field))) {
            Result = ParseTimeZoneTime(Field, &(ZoneEntry->Save), NULL);
            if (Result != 0) {
                fprintf(stderr,
                        "Error: Failed to parse Zone SAVE %s.\n",
                        Field);

                goto ProcessTimeZoneEnd;
            }

        } else {
            Result = TimeZoneAddRuleString(Field, &(ZoneEntry->RulesNameIndex));
            if (Result != 0) {
                goto ProcessTimeZoneEnd;
            }
        }
    }

    //
    // Copy the format string.
    //

    Field = Fields[ZoneFieldFormat - FieldOffset];
    Result = TimeZoneAddString(Field, &(ZoneEntry->FormatOffset));
    if (Result != 0) {
        goto ProcessTimeZoneEnd;
    }

    //
    // If there's no until year, then this is done.
    //

    if (FieldCount <= ZoneFieldUntilYear - FieldOffset) {
        Result = 0;
        goto ProcessTimeZoneEnd;
    }

    Field = Fields[ZoneFieldUntilYear - FieldOffset];
    ScannedValue = strtol(Field, &AfterScan, 10);
    if ((ScannedValue <= MIN_TIME_ZONE_YEAR) ||
        (ScannedValue >= MAX_TIME_ZONE_YEAR) ||
        (AfterScan == Field)) {

        fprintf(stderr,
                "Error: Failed to parse Zone UNTIL YEAR %s.\n",
                Field);

        Result = EILSEQ;
        goto ProcessTimeZoneEnd;
    }

    Year = ScannedValue;
    UntilValid = TRUE;
    ZoneEntry->Until = (LONGLONG)ComputeDaysForYear(Year) * SECONDS_PER_DAY;

    //
    // If there's no until month, this is done.
    //

    if (FieldCount <= ZoneFieldUntilMonth - FieldOffset) {
        Result = 0;
        goto ProcessTimeZoneEnd;
    }

    Leap = 0;
    if (IS_LEAP_YEAR(ScannedValue)) {
        Leap = 1;
    }

    Field = Fields[ZoneFieldUntilMonth - FieldOffset];
    Result = ParseTimeZoneMonth(Field, &Month);
    if (Result != 0) {
        fprintf(stderr,
                "Error: Failed to parse Zone UNTIL MONTH %s.\n",
                Field);

        goto ProcessTimeZoneEnd;
    }

    ZoneEntry->Until += TimeZoneMonthDays[Leap][Month] * SECONDS_PER_DAY;

    //
    // If there is no until day, this is done.
    //

    if (FieldCount <= ZoneFieldUntilDay - FieldOffset) {
        Result = 0;
        goto ProcessTimeZoneEnd;
    }

    Field = Fields[ZoneFieldUntilDay - FieldOffset];

    //
    // The day portion of the until field can apparently either be a number or
    // an occasion.
    //

    if (isdigit(*Field)) {
        ScannedValue = strtol(Field, &AfterScan, 10);
        if ((ScannedValue <= 0) || (ScannedValue > 31) ||
            (AfterScan == Field)) {

            fprintf(stderr,
                    "Error: Failed to parse Zone UNTIL DAY %s.\n",
                    Field);

            Result = EILSEQ;
            goto ProcessTimeZoneEnd;
        }

        Day = ScannedValue;

    } else {
        memset(&Occasion, 0, sizeof(TIME_ZONE_OCCASION));
        Result = ParseTimeZoneOccasion(Field, &Occasion);
        if (Result != 0) {
            fprintf(stderr,
                    "Error: Failed to parse Zone UNTIL DAY (occasion) %s.\n",
                    Field);

            goto ProcessTimeZoneEnd;
        }

        Result = CalculateOccasionForDate(&Occasion, Year, Month, &Day);
        if (Result != 0) {
            fprintf(stderr, "Error: Zone UNTIL DAY occasion does not exist.\n");
            goto ProcessTimeZoneEnd;
        }
    }

    ZoneEntry->Until += (Day - 1) * SECONDS_PER_DAY;

    //
    // Finally, if there is no time, this is done.
    //

    if (FieldCount <= ZoneFieldUntilTime - FieldOffset) {
        Result = 0;
        goto ProcessTimeZoneEnd;
    }

    Field = Fields[ZoneFieldUntilTime - FieldOffset];
    Result = ParseTimeZoneTime(Field, &UntilTime, &UntilLens);
    if (Result != 0) {
        fprintf(stderr,
                "Error: Failed to parse Zone UNTIL TIME %s.\n",
                Field);

        goto ProcessTimeZoneEnd;
    }

    ZoneEntry->Until += UntilTime;
    if ((UntilLens == TimeZoneLensLocalTime) ||
        (UntilLens == TimeZoneLensLocalStandardTime)) {

        ZoneEntry->Until += ZoneEntry->GmtOffset;
        if (UntilLens == TimeZoneLensLocalTime) {
            ZoneEntry->Until += ZoneEntry->Save;
        }
    }

ProcessTimeZoneEnd:
    if (Result != 0) {
        if (ZoneEntry != NULL) {
            DestroyTimeZoneEntry(ZoneEntry);
        }

    } else {

        assert((ZoneEntry != NULL) && (Zone != NULL));

        Zone->ZoneEntryCount += 1;
        INSERT_BEFORE(&(ZoneEntry->ListEntry), &TimeZoneEntryList);

        //
        // If this is the last zone entry, attempt to compress by finding the
        // same zone entries elsewhere.
        //

        if (UntilValid == FALSE) {
            TimeZoneCompressEntries(&TimeZoneEntryList,
                                    &TimeZoneNextZoneEntryIndex,
                                    Zone);
        }
    }

    *Continuation = UntilValid;
    return Result;
}

INT
ProcessTimeZoneLink (
    PSTR *Fields,
    ULONG FieldCount
    )

/*++

Routine Description:

    This routine processes a time zone link line.

Arguments:

    Fields - Supplies a pointer to the fields in the line.

    FieldCount - Supplies the number of elements in the fields array.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PTZC_LINK Link;
    INT Result;

    Link = NULL;
    Result = ENOMEM;
    if (FieldCount != LinkFieldCount) {
        fprintf(stderr,
                "Error: Link should have had %d fields, had %d.\n",
                LinkFieldCount,
                FieldCount);

        return EILSEQ;
    }

    assert(strcasecmp(Fields[LinkFieldMagic], "Link") == 0);

    Link = malloc(sizeof(TZC_LINK));
    if (Link == NULL) {
        goto ProcessTimeZoneLinkEnd;
    }

    memset(Link, 0, sizeof(TZC_LINK));
    Link->From = strdup(Fields[LinkFieldFrom]);
    if (Link->From == NULL) {
        goto ProcessTimeZoneLinkEnd;
    }

    Link->To = strdup(Fields[LinkFieldTo]);
    if (Link->To == NULL) {
        goto ProcessTimeZoneLinkEnd;
    }

    INSERT_BEFORE(&(Link->ListEntry), &TimeZoneLinkList);
    Result = 0;

ProcessTimeZoneLinkEnd:
    if (Result != 0) {
        if (Link != NULL) {
            DestroyTimeZoneLink(Link);
        }
    }

    return Result;
}

INT
ProcessTimeZoneLeap (
    PSTR *Fields,
    ULONG FieldCount
    )

/*++

Routine Description:

    This routine processes a time zone leap second line.

Arguments:

    Fields - Supplies a pointer to the fields in the line.

    FieldCount - Supplies the number of elements in the fields array.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    INT Day;
    PSTR Field;
    PTZC_LEAP Leap;
    INT LeapYear;
    TIME_ZONE_MONTH Month;
    INT Result;
    LONG ScannedValue;
    LONG Time;
    INT Year;

    Leap = NULL;
    Result = ENOMEM;
    if (FieldCount != LeapFieldCount) {
        fprintf(stderr,
                "Error: Link should have had %d fields, had %d.\n",
                LinkFieldCount,
                FieldCount);

        return EILSEQ;
    }

    assert(strcasecmp(Fields[LeapFieldMagic], "Leap") == 0);

    Leap = malloc(sizeof(TZC_LEAP));
    if (Leap == NULL) {
        goto ProcessTimeZoneLeapEnd;
    }

    memset(Leap, 0, sizeof(TZC_LEAP));

    //
    // Process the year.
    //

    Field = Fields[LeapFieldYear];
    ScannedValue = strtol(Field, &AfterScan, 10);
    if ((ScannedValue <= MIN_TIME_ZONE_YEAR) ||
        (ScannedValue >= MAX_TIME_ZONE_YEAR) ||
        (AfterScan == Field)) {

        fprintf(stderr,
                "Error: Failed to parse Leap YEAR %s.\n",
                Field);

        Result = EILSEQ;
        goto ProcessTimeZoneLeapEnd;
    }

    Year = ScannedValue;
    Leap->Date = (LONGLONG)ComputeDaysForYear(Year) * SECONDS_PER_DAY;

    //
    // Process the month.
    //

    LeapYear = 0;
    if (IS_LEAP_YEAR(Year)) {
        LeapYear = 1;
    }

    Field = Fields[LeapFieldMonth];
    Result = ParseTimeZoneMonth(Field, &Month);
    if (Result != 0) {
        fprintf(stderr,
                "Error: Failed to parse Leap MONTH %s.\n",
                Field);

        goto ProcessTimeZoneLeapEnd;
    }

    Leap->Date += TimeZoneMonthDays[LeapYear][Month] * SECONDS_PER_DAY;

    //
    // Process the month day.
    //

    Field = Fields[LeapFieldDay];
    ScannedValue = strtol(Field, &AfterScan, 10);
    if ((ScannedValue <= 0) || (ScannedValue > 31) ||
        (AfterScan == Field)) {

        fprintf(stderr,
                "Error: Failed to parse Leap DAY %s.\n",
                Field);

        Result = EILSEQ;
        goto ProcessTimeZoneLeapEnd;
    }

    Day = ScannedValue;
    Leap->Date += (Day - 1) * SECONDS_PER_DAY;

    //
    // Process the time.
    //

    Field = Fields[LeapFieldTime];
    Result = ParseTimeZoneTime(Field, &Time, NULL);
    if (Result != 0) {
        fprintf(stderr,
                "Error: Failed to parse Leap TIME %s.\n",
                Field);

        goto ProcessTimeZoneLeapEnd;
    }

    Leap->Date += Time;

    //
    // Process the correction, which should be + or -.
    //

    Field = Fields[LeapFieldCorrection];
    if (strcmp(Field, "+") == 0) {
        Leap->Positive = TRUE;

    } else if (strcmp(Field, "-") == 0) {
        Leap->Positive = FALSE;

    } else {
        fprintf(stderr,
                "Error: Failed to parse Leap CORRECTION %s.\n",
                Field);

        Result = EILSEQ;
        goto ProcessTimeZoneLeapEnd;
    }

    //
    // Process the Rolling/Stationary bit, which should be R or S.
    //

    Field = Fields[LeapFieldRollingOrStationary];
    if (strcasecmp(Field, "R") == 0) {
        Leap->LocalTime = TRUE;

    } else if (strcasecmp(Field, "S") == 0) {
        Leap->LocalTime = FALSE;

    } else {
        fprintf(stderr,
                "Error: Failed to parse Leap R/S %s.\n",
                Field);

        Result = EILSEQ;
        goto ProcessTimeZoneLeapEnd;
    }

    INSERT_BEFORE(&(Leap->ListEntry), &TimeZoneLeapList);
    TimeZoneLeapCount += 1;
    Result = 0;

ProcessTimeZoneLeapEnd:
    if (Result != 0) {
        if (Leap != NULL) {
            DestroyTimeZoneLeap(Leap);
        }
    }

    return Result;
}

INT
ReadTimeZoneFields (
    FILE *File,
    PVOID *LineBuffer,
    PULONG LineBufferSize,
    PSTR **Fields,
    PULONG FieldsSize,
    PULONG FieldCount,
    PBOOL EndOfFile
    )

/*++

Routine Description:

    This routine reads a line in from the time zone file.

Arguments:

    File - Supplies the file stream to read from.

    LineBuffer - Supplies a pointer that on input points to an allocated buffer
        (which can be null). On output, this buffer will be used for the
        field values, and potentially realloced.

    LineBufferSize - Supplies a pointer that on input contains the size of the
        line buffer in bytes. On output this value will be updated to reflect
        the new buffer allocation size.

    Fields - Supplies a pointer that on input contains a pointer to an array
        of pointers to strings. On output, the pointers to the various fields
        of the line will be returned here. The entire buffer may be realloced
        for large field counts.

    FieldsSize - Supplies a pointer that on input contains the size of the
        fields buffer in bytes. This value will be updated on output.

    FieldCount - Supplies a pointer where the number of fields in this line
        will be returned.

    EndOfFile - Supplies a pointer where a boolean will be returned indicating
        if the end of the input file was reached.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG ElementCount;
    ULONG FieldsCapacity;
    BOOL InQuote;
    PSTR Line;
    PSTR *LineFields;
    PVOID NewBuffer;
    ULONG NewCapacity;
    INT Result;

    ElementCount = 0;
    FieldsCapacity = *FieldsSize;
    LineFields = *Fields;
    Result = ReadTimeZoneLine(File, LineBuffer, LineBufferSize, EndOfFile);
    if (Result != 0) {
        goto ReadTimeZoneFieldsEnd;
    }

    //
    // Loop delimiting fields.
    //

    Line = *LineBuffer;
    while (*Line != '\0') {

        //
        // Swoop past leading spaces.
        //

        while (isspace(*Line)) {
            Line += 1;
        }

        //
        // If the next character is a terminator or # (comment), then this line
        // is toast.
        //

        if ((*Line == '\0') || (*Line == '#')) {
            break;
        }

        //
        // Reallocate the fields buffer if needed.
        //

        if ((ElementCount * sizeof(PSTR)) >= FieldsCapacity) {
            NewCapacity = FieldsCapacity;
            if (NewCapacity == 0) {
                NewCapacity = INITIAL_MALLOC_SIZE;

            } else {
                NewCapacity *= 2;
            }

            assert(NewCapacity > (ElementCount * sizeof(PSTR)));

            NewBuffer = realloc(LineFields, NewCapacity);
            if (NewBuffer == NULL) {
                Result = ENOMEM;
                goto ReadTimeZoneFieldsEnd;
            }

            FieldsCapacity = NewCapacity;
            LineFields = NewBuffer;
        }

        //
        // Set the entry in the fields array.
        //

        LineFields[ElementCount] = Line;
        ElementCount += 1;

        //
        // Find the end of the field.
        //

        InQuote = FALSE;
        while ((*Line != '\0') && ((!isspace(*Line)) || (InQuote != FALSE))) {
            if (InQuote != FALSE) {
                if (*Line == '"') {
                    InQuote = FALSE;
                }

            } else {
                if (*Line == '"') {
                    InQuote = TRUE;
                }
            }

            Line += 1;
        }

        //
        // Terminate the field.
        //

        if (*Line == '\0') {
            break;
        }

        *Line = '\0';
        Line += 1;
    }

    Result = 0;

ReadTimeZoneFieldsEnd:
    *FieldCount = ElementCount;
    *FieldsSize = FieldsCapacity;
    *Fields = LineFields;
    return Result;
}

INT
ReadTimeZoneLine (
    FILE *File,
    PVOID *LineBuffer,
    PULONG LineBufferSize,
    PBOOL EndOfFile
    )

/*++

Routine Description:

    This routine reads a line in from the time zone file.

Arguments:

    File - Supplies the file stream to read from.

    LineBuffer - Supplies a pointer that on input points to an allocated buffer
        (which can be null). On output, this buffer will contain the null
        terminated line.

    LineBufferSize - Supplies a pointer that on input contains the size of the
        line buffer in bytes. On output this value will be updated to reflect
        the new buffer allocation size.

    EndOfFile - Supplies a pointer where a boolean will be returned indicating
        if the end of file was hit.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Character;
    BOOL EndOfLine;
    ULONG Length;
    PSTR Line;
    ULONG LineCapacity;
    PVOID NewBuffer;
    ULONG NewLineCapacity;
    INT Result;

    *EndOfFile = FALSE;
    EndOfLine = FALSE;
    Line = *LineBuffer;
    LineCapacity = *LineBufferSize;
    Length = 0;
    while (TRUE) {

        //
        // Read a character from the file.
        //

        Character = fgetc(File);
        if (Character == EOF) {
            if (feof(File) != 0) {
                *EndOfFile = TRUE;
                EndOfLine = TRUE;
                Character = '\0';

            } else {
                fprintf(stderr, "Error reading file: %s.\n", strerror(errno));
                Result = errno;
                goto ReadTimeZoneLineEnd;
            }
        }

        //
        // Reallocate the buffer if it's too small to hold this character.
        //

        if (Length >= LineCapacity) {
            NewLineCapacity = LineCapacity;
            if (NewLineCapacity == 0) {
                NewLineCapacity = INITIAL_MALLOC_SIZE;

            } else {
                NewLineCapacity *= 2;
            }

            assert(NewLineCapacity > Length);

            NewBuffer = realloc(Line, NewLineCapacity);
            if (NewBuffer == NULL) {
                Result = ENOMEM;
                goto ReadTimeZoneLineEnd;
            }

            LineCapacity = NewLineCapacity;
            Line = NewBuffer;
        }

        //
        // Terminate if this is a newline.
        //

        if (Character == '\n') {
            Character = '\0';
            EndOfLine = TRUE;
        }

        //
        // Add the line to the buffer.
        //

        Line[Length] = Character;
        Length += 1;
        if (EndOfLine != FALSE) {
            break;
        }
    }

    Result = 0;

ReadTimeZoneLineEnd:
    *LineBuffer = Line;
    *LineBufferSize = LineCapacity;
    return Result;
}

INT
TranslateLinksToZones (
    )

/*++

Routine Description:

    This routine converts time zone links into time zone structures.

Arguments:

    None.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLIST_ENTRY CurrentZoneEntry;
    PTZC_ZONE DestinationZone;
    PTZC_LINK Link;
    INT Result;
    PTZC_ZONE Zone;
    PSTR ZoneName;

    CurrentEntry = TimeZoneLinkList.Next;
    while (CurrentEntry != &TimeZoneLinkList) {
        Link = LIST_VALUE(CurrentEntry, TZC_LINK, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Loop through all the zones looking for the destination.
        //

        DestinationZone = NULL;
        CurrentZoneEntry = TimeZoneList.Next;
        while (CurrentZoneEntry != &TimeZoneList) {
            DestinationZone = LIST_VALUE(CurrentZoneEntry, TZC_ZONE, ListEntry);
            CurrentZoneEntry = CurrentZoneEntry->Next;
            ZoneName = TimeZoneGetString(&TimeZoneStringList,
                                         DestinationZone->NameOffset);

            if (strcmp(ZoneName, Link->From) == 0) {
                break;
            }

            DestinationZone = NULL;
        }

        if (DestinationZone == NULL) {
            fprintf(stderr,
                    "tzcomp: Warning: Link destination time zone %s not "
                    "found. Source (%s).\n",
                    Link->From,
                    Link->To);

            continue;
        }

        //
        // Create a time zone structure and initialize it based on the
        // destination.
        //

        Zone = malloc(sizeof(TZC_ZONE));
        if (Zone == NULL) {
            return ENOMEM;
        }

        memset(Zone, 0, sizeof(TZC_ZONE));
        Result = TimeZoneAddString(Link->To, &(Zone->NameOffset));
        if (Result != 0) {
            free(Zone);
            return Result;
        }

        Zone->ZoneEntryIndex = DestinationZone->ZoneEntryIndex;
        Zone->ZoneEntryCount = DestinationZone->ZoneEntryCount;
        INSERT_BEFORE(&(Zone->ListEntry), &TimeZoneList);
        TimeZoneCount += 1;
    }

    return 0;
}

INT
TimeZoneFilter (
    PSTR Name,
    INT Year
    )

/*++

Routine Description:

    This routine removes all time zone data from the global list except for
    that that matches the given time zone name and/or starts after the given
    year.

Arguments:

    Name - Supplies an optional pointer to a string containing the name of the
        time zone to keep.

    Year - Supplies an optional year before which to exclude any time zone
        entries.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLIST_ENTRY CurrentZoneEntry;
    ULONG CurrentZoneEntryCount;
    ULONG EntryIndex;
    PSTR FormatString;
    PSTR LettersString;
    LIST_ENTRY NewEntryList;
    LIST_ENTRY NewRuleList;
    LIST_ENTRY NewStringList;
    ULONG NewStringListSize;
    PTZC_ZONE_ENTRY NewZoneEntry;
    LIST_ENTRY NewZoneList;
    PTZC_ZONE RemoveZone;
    INT Result;
    PTZC_RULE Rule;
    ULONG RuleCount;
    PLIST_ENTRY RuleEntry;
    LONGLONG Until;
    ULONG UnusedOffset;
    PTZC_ZONE Zone;
    PTZC_ZONE_ENTRY ZoneEntry;
    ULONG ZoneEntryCount;
    PSTR ZoneName;
    ULONG ZonesAdded;

    INITIALIZE_LIST_HEAD(&NewEntryList);
    INITIALIZE_LIST_HEAD(&NewRuleList);
    INITIALIZE_LIST_HEAD(&NewStringList);
    INITIALIZE_LIST_HEAD(&NewZoneList);
    NewStringListSize = 0;
    RuleCount = 0;
    Zone = NULL;
    ZoneEntryCount = 0;
    ZoneName = NULL;
    Until = (LONGLONG)ComputeDaysForYear(Year) * SECONDS_PER_DAY;
    TimeZoneAddStringToList("",
                            &NewStringList,
                            &NewStringListSize,
                            TRUE,
                            &UnusedOffset);

    //
    // Loop looking for time zones that match the name and the year.
    //

    ZonesAdded = 0;
    CurrentZoneEntry = TimeZoneList.Next;
    while (CurrentZoneEntry != &TimeZoneList) {
        Zone = LIST_VALUE(CurrentZoneEntry, TZC_ZONE, ListEntry);
        CurrentZoneEntry = CurrentZoneEntry->Next;
        ZoneName = TimeZoneGetString(&TimeZoneStringList, Zone->NameOffset);
        if (Name != NULL) {
            if (strcasecmp(Name, ZoneName) != 0) {
                continue;
            }
        }

        //
        // Find the starting zone entry.
        //

        CurrentEntry = TimeZoneEntryList.Next;
        for (EntryIndex = 0;
             EntryIndex < Zone->ZoneEntryIndex;
             EntryIndex += 1) {

            assert(CurrentEntry != &TimeZoneEntryList);

            CurrentEntry = CurrentEntry->Next;
        }

        //
        // Pull the zone entries onto the new list.
        //

        CurrentZoneEntryCount = 0;
        Zone->ZoneEntryIndex = ZoneEntryCount;
        for (EntryIndex = 0;
             EntryIndex < Zone->ZoneEntryCount;
             EntryIndex += 1) {

            ZoneEntry = LIST_VALUE(CurrentEntry, TZC_ZONE_ENTRY, ListEntry);

            assert(CurrentEntry != &TimeZoneEntryList);

            CurrentEntry = CurrentEntry->Next;

            //
            // Skip an entry that is too old.
            //

            if (ZoneEntry->Until <= Until) {
                continue;
            }

            NewZoneEntry = malloc(sizeof(TZC_ZONE_ENTRY));
            if (NewZoneEntry == NULL) {
                Result = ENOMEM;
                goto TimeZoneFilterByNameEnd;
            }

            memcpy(NewZoneEntry, ZoneEntry, sizeof(TZC_ZONE_ENTRY));
            INSERT_BEFORE(&(NewZoneEntry->ListEntry), &NewEntryList);
            NewZoneEntry->Index = ZoneEntryCount;
            ZoneEntryCount += 1;
            CurrentZoneEntryCount += 1;
            FormatString = TimeZoneGetString(&TimeZoneStringList,
                                             ZoneEntry->FormatOffset);

            Result = TimeZoneAddStringToList(FormatString,
                                             &NewStringList,
                                             &NewStringListSize,
                                             TRUE,
                                             &(NewZoneEntry->FormatOffset));

            if (Result != 0) {
                goto TimeZoneFilterByNameEnd;
            }

            //
            // Loop through and pull off any rules that apply to this zone
            // entry.
            //

            if (NewZoneEntry->RulesNameIndex != -1) {
                RuleEntry = TimeZoneRuleList.Next;
                while (RuleEntry != &TimeZoneRuleList) {
                    Rule = LIST_VALUE(RuleEntry, TZC_RULE, ListEntry);
                    RuleEntry = RuleEntry->Next;
                    if (Rule->NameIndex == NewZoneEntry->RulesNameIndex) {
                        if (Rule->To <= Year) {
                            continue;
                        }

                        LettersString = TimeZoneGetString(&TimeZoneStringList,
                                                          Rule->LettersOffset);

                        Result = TimeZoneAddStringToList(
                                                       LettersString,
                                                       &NewStringList,
                                                       &NewStringListSize,
                                                       TRUE,
                                                       &(Rule->LettersOffset));

                        if (Result != 0) {
                            goto TimeZoneFilterByNameEnd;
                        }

                        LIST_REMOVE(&(Rule->ListEntry));
                        INSERT_BEFORE(&(Rule->ListEntry), &NewRuleList);
                        RuleCount += 1;
                    }
                }
            }
        }

        if (CurrentZoneEntryCount != 0) {
            Zone->ZoneEntryCount = CurrentZoneEntryCount;
            LIST_REMOVE(&(Zone->ListEntry));
            INSERT_BEFORE(&(Zone->ListEntry), &NewZoneList);
            ZonesAdded += 1;

            //
            // Add the zone name to the string table.
            //

            Result = TimeZoneAddStringToList(ZoneName,
                                             &NewStringList,
                                             &NewStringListSize,
                                             TRUE,
                                             &(Zone->NameOffset));

            if (Result != 0) {
                goto TimeZoneFilterByNameEnd;
            }

            TimeZoneCompressEntries(&NewEntryList,
                                    &ZoneEntryCount,
                                    Zone);
        }
    }

    //
    // Fail if the time zone was not found.
    //

    if (ZonesAdded == 0) {
        Result = EINVAL;
        if (Name != NULL) {
            fprintf(stderr,
                    "Error: Could not find time zone \"%s\" after year %d.\n",
                    Name,
                    Year);

        } else {
            fprintf(stderr,
                    "Error: No time zones after year %d.\n",
                    Year);
        }

        goto TimeZoneFilterByNameEnd;
    }

    //
    // Destroy all the other zones, rules, and zone entries.
    //

    while (LIST_EMPTY(&TimeZoneRuleList) == FALSE) {
        Rule = LIST_VALUE(TimeZoneRuleList.Next, TZC_RULE, ListEntry);
        LIST_REMOVE(&(Rule->ListEntry));
        DestroyTimeZoneRule(Rule);
    }

    while (LIST_EMPTY(&TimeZoneList) == FALSE) {
        RemoveZone = LIST_VALUE(TimeZoneList.Next, TZC_ZONE, ListEntry);
        LIST_REMOVE(&(RemoveZone->ListEntry));
        DestroyTimeZone(RemoveZone);
    }

    while (LIST_EMPTY(&TimeZoneEntryList) == FALSE) {
        ZoneEntry = LIST_VALUE(TimeZoneEntryList.Next,
                               TZC_ZONE_ENTRY,
                               ListEntry);

        LIST_REMOVE(&(ZoneEntry->ListEntry));
        DestroyTimeZoneEntry(ZoneEntry);
    }

    DestroyTimeZoneStringList(&TimeZoneStringList);

    //
    // Now insert all entries from the local lists onto the global lists.
    //

    if (!LIST_EMPTY(&NewRuleList)) {
        MOVE_LIST(&NewRuleList, &TimeZoneRuleList);
        INITIALIZE_LIST_HEAD(&NewRuleList);
    }

    TimeZoneRuleCount = RuleCount;
    if (!LIST_EMPTY(&NewEntryList)) {
        MOVE_LIST(&NewEntryList, &TimeZoneEntryList);
        INITIALIZE_LIST_HEAD(&NewEntryList);
    }

    TimeZoneNextZoneEntryIndex = ZoneEntryCount;
    if (!LIST_EMPTY(&NewStringList)) {
        MOVE_LIST(&NewStringList, &TimeZoneStringList);
        INITIALIZE_LIST_HEAD(&NewStringList);
    }

    TimeZoneNextStringOffset = NewStringListSize;
    if (!LIST_EMPTY(&NewZoneList)) {
        MOVE_LIST(&NewZoneList, &TimeZoneList);
        INITIALIZE_LIST_HEAD(&NewZoneList);
    }

    TimeZoneCount = ZonesAdded;
    Result = 0;

TimeZoneFilterByNameEnd:
    while (LIST_EMPTY(&NewRuleList) == FALSE) {
        Rule = LIST_VALUE(NewRuleList.Next, TZC_RULE, ListEntry);
        LIST_REMOVE(&(Rule->ListEntry));
        DestroyTimeZoneRule(Rule);
    }

    while (LIST_EMPTY(&NewEntryList) == FALSE) {
        ZoneEntry = LIST_VALUE(NewEntryList.Next,
                               TZC_ZONE_ENTRY,
                               ListEntry);

        LIST_REMOVE(&(ZoneEntry->ListEntry));
        DestroyTimeZoneEntry(ZoneEntry);
    }

    while (LIST_EMPTY(&NewZoneList) == FALSE) {
        Zone = LIST_VALUE(NewZoneList.Next, TZC_ZONE, ListEntry);
        LIST_REMOVE(&(Zone->ListEntry));
        DestroyTimeZone(Zone);
    }

    DestroyTimeZoneStringList(&NewStringList);
    return Result;
}

INT
WriteTimeZoneData (
    PSTR FileName
    )

/*++

Routine Description:

    This routine writes the time zone data out to a file in binary format.

Arguments:

    FileName - Supplies a pointer to a string containing the name of the file
        to write.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    size_t BytesWritten;
    PLIST_ENTRY CurrentEntry;
    ULONG ElementsWritten;
    FILE *File;
    TIME_ZONE_LEAP_SECOND FileLeap;
    TIME_ZONE_RULE FileRule;
    TIME_ZONE FileZone;
    TIME_ZONE_ENTRY FileZoneEntry;
    TIME_ZONE_HEADER Header;
    PTZC_LEAP Leap;
    INT Result;
    PTZC_RULE Rule;
    PTZC_STRING String;
    size_t StringLength;
    PTZC_ZONE Zone;
    PTZC_ZONE_ENTRY ZoneEntry;

    memset(&Header, 0, sizeof(TIME_ZONE_HEADER));
    File = fopen(FileName, "wb");
    if (File == NULL) {
        Result = errno;
        fprintf(stderr,
                "tzcomp: Failed to open output file \"%s\": %s.\n",
                FileName,
                strerror(Result));

        goto WriteTimeZoneDataEnd;
    }

    //
    // Write out the header.
    //

    Header.Magic = TIME_ZONE_HEADER_MAGIC;
    Header.RuleOffset = sizeof(TIME_ZONE_HEADER);
    Header.RuleCount = TimeZoneRuleCount;
    Header.ZoneOffset = Header.RuleOffset +
                        (Header.RuleCount * sizeof(TIME_ZONE_RULE));

    Header.ZoneCount = TimeZoneCount;
    Header.ZoneEntryOffset = Header.ZoneOffset +
                             (Header.ZoneCount * sizeof(TIME_ZONE));

    Header.ZoneEntryCount = TimeZoneNextZoneEntryIndex;
    Header.LeapOffset = Header.ZoneEntryOffset +
                        (Header.ZoneEntryCount * sizeof(TIME_ZONE_ENTRY));

    Header.LeapCount = TimeZoneLeapCount;
    Header.StringsOffset = Header.LeapOffset +
                           (Header.LeapCount * sizeof(TIME_ZONE_LEAP_SECOND));

    Header.StringsSize = TimeZoneNextStringOffset;
    BytesWritten = fwrite(&Header, 1, sizeof(TIME_ZONE_HEADER), File);
    if (BytesWritten <= 0) {
        Result = errno;
        fprintf(stderr, "tzcomp: Write error: %s.\n", strerror(Result));
        goto WriteTimeZoneDataEnd;
    }

    assert(ftell(File) == Header.RuleOffset);

    //
    // Write out the rules.
    //

    memset(&FileRule, 0, sizeof(TIME_ZONE_RULE));
    ElementsWritten = 0;
    CurrentEntry = TimeZoneRuleList.Next;
    while (CurrentEntry != &TimeZoneRuleList) {
        Rule = LIST_VALUE(CurrentEntry, TZC_RULE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        FileRule.Number = Rule->NameIndex;
        FileRule.From = Rule->From;
        FileRule.To = Rule->To;
        FileRule.Month = Rule->Month;
        FileRule.On = Rule->On;
        FileRule.At = Rule->At;
        FileRule.AtLens = Rule->AtLens;
        FileRule.Save = Rule->Save;
        FileRule.Letters = Rule->LettersOffset;
        BytesWritten = fwrite(&FileRule, 1, sizeof(TIME_ZONE_RULE), File);
        if (BytesWritten <= 0) {
            Result = errno;
            fprintf(stderr, "tzcomp: Write error: %s.\n", strerror(Result));
            goto WriteTimeZoneDataEnd;
        }

        ElementsWritten += 1;
    }

    assert(ElementsWritten == Header.RuleCount);
    assert(ftell(File) == Header.ZoneOffset);

    //
    // Write out the zones.
    //

    memset(&FileZone, 0, sizeof(TIME_ZONE));
    ElementsWritten = 0;
    CurrentEntry = TimeZoneList.Next;
    while (CurrentEntry != &TimeZoneList) {
        Zone = LIST_VALUE(CurrentEntry, TZC_ZONE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        FileZone.Name = Zone->NameOffset;
        FileZone.EntryIndex = Zone->ZoneEntryIndex;
        FileZone.EntryCount = Zone->ZoneEntryCount;
        BytesWritten = fwrite(&FileZone, 1, sizeof(TIME_ZONE), File);
        if (BytesWritten <= 0) {
            Result = errno;
            fprintf(stderr, "tzcomp: Write error: %s.\n", strerror(Result));
            goto WriteTimeZoneDataEnd;
        }

        ElementsWritten += 1;
    }

    assert(ElementsWritten == Header.ZoneCount);
    assert(ftell(File) == Header.ZoneEntryOffset);

    //
    // Write out the zone entries.
    //

    memset(&FileZoneEntry, 0, sizeof(TIME_ZONE_ENTRY));
    ElementsWritten = 0;
    CurrentEntry = TimeZoneEntryList.Next;
    while (CurrentEntry != &TimeZoneEntryList) {
        ZoneEntry = LIST_VALUE(CurrentEntry, TZC_ZONE_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        FileZoneEntry.GmtOffset = ZoneEntry->GmtOffset;
        FileZoneEntry.Rules = ZoneEntry->RulesNameIndex;
        FileZoneEntry.Save = ZoneEntry->Save;
        FileZoneEntry.Format = ZoneEntry->FormatOffset;
        FileZoneEntry.Until = ZoneEntry->Until;
        BytesWritten = fwrite(&FileZoneEntry, 1, sizeof(TIME_ZONE_ENTRY), File);
        if (BytesWritten <= 0) {
            Result = errno;
            fprintf(stderr, "tzcomp: Write error: %s.\n", strerror(Result));
            goto WriteTimeZoneDataEnd;
        }

        ElementsWritten += 1;
    }

    assert(ElementsWritten == Header.ZoneEntryCount);
    assert(ftell(File) == Header.LeapOffset);

    //
    // Write out the leap seconds.
    //

    memset(&FileLeap, 0, sizeof(TIME_ZONE_LEAP_SECOND));
    ElementsWritten = 0;
    CurrentEntry = TimeZoneLeapList.Next;
    while (CurrentEntry != &TimeZoneLeapList) {
        Leap = LIST_VALUE(CurrentEntry, TZC_LEAP, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        FileLeap.Date = Leap->Date;
        FileLeap.Positive = Leap->Positive;
        FileLeap.LocalTime = Leap->LocalTime;
        BytesWritten = fwrite(&FileLeap,
                              1,
                              sizeof(TIME_ZONE_LEAP_SECOND),
                              File);

        if (BytesWritten <= 0) {
            Result = errno;
            fprintf(stderr, "tzcomp: Write error: %s.\n", strerror(Result));
            goto WriteTimeZoneDataEnd;
        }

        ElementsWritten += 1;
    }

    assert(ElementsWritten == Header.LeapCount);
    assert(ftell(File) == Header.StringsOffset);

    //
    // Write out the string table.
    //

    ElementsWritten = 0;
    CurrentEntry = TimeZoneStringList.Next;
    while (CurrentEntry != &TimeZoneStringList) {
        String = LIST_VALUE(CurrentEntry, TZC_STRING, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        StringLength = strlen(String->String) + 1;
        BytesWritten = fwrite(String->String, 1, StringLength, File);
        if (BytesWritten <= 0) {
            Result = errno;
            fprintf(stderr, "tzcomp: Write error: %s.\n", strerror(Result));
            goto WriteTimeZoneDataEnd;
        }

        ElementsWritten += StringLength;
    }

    assert(ElementsWritten == Header.StringsSize);

    Result = 0;

WriteTimeZoneDataEnd:
    if (File != NULL) {
        fclose(File);
    }

    return Result;
}

VOID
DestroyTimeZoneRule (
    PTZC_RULE Rule
    )

/*++

Routine Description:

    This routine frees all memory associated with a time zone rule. This
    routine assumes the rule has already been pulled off of any list it was on.

Arguments:

    Rule - Supplies a pointer to the rule to destroy.

Return Value:

    None.

--*/

{

    free(Rule);
    return;
}

VOID
DestroyTimeZone (
    PTZC_ZONE Zone
    )

/*++

Routine Description:

    This routine destroys a time zone (the structure, of course). This routine
    assumes the zone has already been pulled off of any list it was on.

Arguments:

    Zone - Supplies a pointer to the zone to destroy.

Return Value:

    None.

--*/

{

    free(Zone);
    return;
}

VOID
DestroyTimeZoneEntry (
    PTZC_ZONE_ENTRY ZoneEntry
    )

/*++

Routine Description:

    This routine destroys a time zone entry. This routine assumes the entry has
    already been pulled off of any list it was on.

Arguments:

    ZoneEntry - Supplies a pointer to the zone entry to destroy.

Return Value:

    None.

--*/

{

    free(ZoneEntry);
    return;
}

VOID
DestroyTimeZoneLink (
    PTZC_LINK Link
    )

/*++

Routine Description:

    This routine destroys a time zone link. This routine assumes the structure
    has already been pulled off of any list it was on.

Arguments:

    Link - Supplies a pointer to the zone link to destroy.

Return Value:

    None.

--*/

{

    if (Link->From != NULL) {
        free(Link->From);
    }

    if (Link->To != NULL) {
        free(Link->To);
    }

    free(Link);
    return;
}

VOID
DestroyTimeZoneLeap (
    PTZC_LEAP Leap
    )

/*++

Routine Description:

    This routine destroys a time zone leap second structure. This routine
    assumes the structure has already been pulled off of any list it was on.

Arguments:

    Leap - Supplies a pointer to the leap second to destroy.

Return Value:

    None.

--*/

{

    free(Leap);
    return;
}

VOID
DestroyTimeZoneStringList (
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine destroys a time zone string list.

Arguments:

    ListHead - Supplies a pointer to the head of the list.

Return Value:

    None.

--*/

{

    PTZC_STRING StringEntry;

    //
    // First search for the string.
    //

    while (LIST_EMPTY(ListHead) == FALSE) {
        StringEntry = LIST_VALUE(ListHead->Next, TZC_STRING, ListEntry);
        LIST_REMOVE(&(StringEntry->ListEntry));
        free(StringEntry->String);
        free(StringEntry);
    }

    return;
}

INT
ParseTimeZoneRuleLimit (
    PSTR Field,
    SHORT OnlyValue,
    PSHORT Value
    )

/*++

Routine Description:

    This routine parses a rule limit (the FROM and TO fields of the rule).
    Valid values are a year, Minimum, Maximum, and Only (with abbreviations).

Arguments:

    Field - Supplies a pointer to the field.

    OnlyValue - Supplies the value to return if the field has "only" in it.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    INT Result;
    LONG ScannedValue;

    Result = 0;
    if ((strcasecmp(Field, "Minimum") == 0) ||
        (strcasecmp(Field, "Min") == 0)) {

        *Value = MIN_TIME_ZONE_YEAR;

    } else if ((strcasecmp(Field, "Maximum") == 0) ||
               (strcasecmp(Field, "Max") == 0)) {

        *Value = MAX_TIME_ZONE_YEAR;

    } else if (strcasecmp(Field, "Only") == 0) {
        *Value = OnlyValue;

    } else {
        ScannedValue = strtol(Field, &AfterScan, 10);
        if ((ScannedValue < MIN_TIME_ZONE_YEAR) ||
            (ScannedValue > MAX_TIME_ZONE_YEAR) ||
            (AfterScan == Field)) {

            fprintf(stderr, "Error: Cannot parse rule limit %s.\n", Field);
            Result = EILSEQ;
        }

        *Value = ScannedValue;
    }

    return Result;
}

INT
ParseTimeZoneMonth (
    PSTR Field,
    PTIME_ZONE_MONTH Value
    )

/*++

Routine Description:

    This routine parses a month (such as in the Rule IN column).

Arguments:

    Field - Supplies a pointer to the field.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    TIME_ZONE_MONTH Month;

    for (Month = TimeZoneMonthJanuary;
         Month <= TimeZoneMonthDecember;
         Month += 1) {

        if ((strcasecmp(Field, TimeZoneMonthStrings[Month]) == 0) ||
            (strcasecmp(Field, TimeZoneAbbreviatedMonthStrings[Month]) == 0)) {

            *Value = Month;
            return 0;
        }
    }

    *Value = TimeZoneMonthCount;
    return EILSEQ;
}

INT
ParseTimeZoneRuleWeekday (
    PSTR Field,
    PTIME_ZONE_WEEKDAY Value
    )

/*++

Routine Description:

    This routine parses a rule weekday (buried in the ON column).

Arguments:

    Field - Supplies a pointer to the field.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    TIME_ZONE_WEEKDAY Weekday;

    for (Weekday = TimeZoneWeekdaySunday;
         Weekday <= TimeZoneWeekdaySaturday;
         Weekday += 1) {

        if ((strcasecmp(Field, TimeZoneWeekdayStrings[Weekday]) == 0) ||
            (strcasecmp(Field, TimeZoneAbbreviatedWeekdayStrings[Weekday]) ==
                                                                          0)) {

            *Value = Weekday;
            return 0;
        }
    }

    *Value = TimeZoneWeekdayCount;
    return EILSEQ;
}

INT
ParseTimeZoneOccasion (
    PSTR Field,
    PTIME_ZONE_OCCASION Occasion
    )

/*++

Routine Description:

    This routine parses an occasion (such as in the Rule ON column).

Arguments:

    Field - Supplies a pointer to the field.

    Occasion - Supplies a pointer where the result will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    CHAR Comparator;
    PSTR Equals;
    CHAR LastString[5];
    INT Result;
    LONG ScanResult;
    TIME_ZONE_WEEKDAY Weekday;
    PSTR WeekdayString;

    Result = EILSEQ;
    memset(Occasion, 0, sizeof(TIME_ZONE_OCCASION));
    memcpy(LastString, Field, sizeof(LastString) - 1);
    LastString[sizeof(LastString) - 1] = '\0';

    //
    // If the field starts with a digit, it's just a straight up month date.
    //

    if ((*Field >= '0') && (*Field <= '9')) {
        ScanResult = strtol(Field, &AfterScan, 10);
        if ((AfterScan == Field) || (ScanResult < 0) || (ScanResult > 31)) {
            fprintf(stderr,
                    "Error: Unable to scan occasion month date %s.\n",
                    Field);

            return Result;
        }

        Occasion->Type = TimeZoneOccasionMonthDate;
        Occasion->MonthDay = (CHAR)ScanResult;

    //
    // If the field starts with "last", then it's the last weekday in a given
    // month.
    //

    } else if (strcasecmp(LastString, "Last") == 0) {
        WeekdayString = Field + 4;
        if (*WeekdayString == '-') {
            WeekdayString += 1;
        }

        Result = ParseTimeZoneRuleWeekday(WeekdayString, &Weekday);
        if (Result != 0) {
            return Result;
        }

        Occasion->Type = TimeZoneOccasionLastWeekday;
        Occasion->Weekday = Weekday;

    //
    // If the field has a = in it, then it's the last weekday >= a month date or
    // the first weekday <= a month date.
    //

    } else if (strchr(Field, '=') != NULL) {
        Equals = strchr(Field, '=');
        if (Equals == Field) {
            fprintf(stderr, "Error: Unable to scan occasion %s.\n", Field);
            return Result;
        }

        Comparator = *(Equals - 1);
        if (Comparator == '>') {
            Occasion->Type = TimeZoneOccasionGreaterOrEqualWeekday;

        } else if (Comparator == '<') {
            Occasion->Type = TimeZoneOccasionLessOrEqualWeekday;

        } else {
            fprintf(stderr, "Error: Unable to scan occasion %s.\n", Field);
            return Result;
        }

        //
        // Scan the month date.
        //

        ScanResult = strtol(Equals + 1, &AfterScan, 10);
        if ((AfterScan == Field) || (ScanResult < 0) || (ScanResult > 31)) {
            fprintf(stderr,
                    "Error: Unable to scan occasion month date %s.\n",
                    Field);

            return Result;
        }

        Occasion->MonthDay = (CHAR)ScanResult;

        //
        // Terminate and scan the weekday.
        //

        *(Equals - 1) = '\0';
        Result = ParseTimeZoneRuleWeekday(Field, &Weekday);
        if (Result != 0) {
            return Result;
        }

        Occasion->Weekday = Weekday;

    //
    // This is unrecognized.
    //

    } else {
        fprintf(stderr, "Error: Unable to scan occasion %s.\n", Field);
        return Result;
    }

    Result = 0;
    return Result;
}

INT
ParseTimeZoneTime (
    PSTR Field,
    PLONG Time,
    PTIME_ZONE_LENS Lens
    )

/*++

Routine Description:

    This routine parses a rule time (in the Rule ON column).

Arguments:

    Field - Supplies a pointer to the field.

    Time - Supplies a pointer where the time in seconds will be returned.

    Lens - Supplies an optional pointer where the lens under which to view this
        time.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    BOOL Negative;
    PSTR OriginalField;
    INT Result;
    LONG ScannedValue;
    TIME_ZONE_LENS TimeLens;

    TimeLens = TimeZoneLensLocalTime;
    *Time = 0;
    OriginalField = Field;
    Result = EILSEQ;
    Negative = FALSE;
    if (*Field == '-') {
        Negative = TRUE;
        Field += 1;
    }

    //
    // Parse some hours.
    //

    ScannedValue = strtol(Field, &AfterScan, 10);
    if ((ScannedValue < 0) || ((AfterScan == Field) && (Negative == FALSE))) {
        goto ParseTimeZoneTimeEnd;
    }

    *Time = ScannedValue * SECONDS_PER_HOUR;
    Field = AfterScan;

    //
    // Parse some optional minutes.
    //

    if (*Field == ':') {
        Field += 1;
        ScannedValue = strtol(Field, &AfterScan, 10);
        if ((ScannedValue < 0) || (AfterScan == Field)) {
            goto ParseTimeZoneTimeEnd;
        }

        *Time += ScannedValue * SECONDS_PER_MINUTE;
        Field = AfterScan;

        //
        // Parse some optonal seconds.
        //

        if (*Field == ':') {
            Field += 1;
            ScannedValue = strtol(Field, &AfterScan, 10);
            if ((ScannedValue < 0) || (AfterScan == Field)) {
                goto ParseTimeZoneTimeEnd;
            }

            *Time += ScannedValue;
            Field = AfterScan;
        }
    }

    //
    // Parse an optional lens with which to understand this time.
    //

    if (*Field == 'w') {
        TimeLens = TimeZoneLensLocalTime;

    } else if (*Field == 's') {
        TimeLens = TimeZoneLensLocalStandardTime;

    } else if ((*Field == 'u') || (*Field == 'g') || (*Field == 'z')) {
        TimeLens = TimeZoneLensUtc;

    } else if (*Field != '\0') {
        goto ParseTimeZoneTimeEnd;
    }

    if (Negative != FALSE) {
        *Time = -*Time;
    }

    Result = 0;

ParseTimeZoneTimeEnd:
    if (Lens != NULL) {
        *Lens = TimeLens;
    }

    if (Result != 0) {
        fprintf(stderr,
                "Error: Failed to scan time field %s.\n",
                OriginalField);
    }

    return Result;
}

VOID
PrintTimeZoneRule (
    PTZC_RULE Rule
    )

/*++

Routine Description:

    This routine prints a time zone rule.

Arguments:

    Rule - Supplies a pointer to the rule to print.

Return Value:

    None.

--*/

{

    INT Weekday;

    printf("Rule %3d: %-13s %04d-%04d %-9s ",
           Rule->NameIndex,
           TimeZoneGetString(&TimeZoneRuleStringList, Rule->NameIndex),
           Rule->From,
           Rule->To,
           TimeZoneMonthStrings[Rule->Month]);

    Weekday = Rule->On.Weekday;
    switch (Rule->On.Type) {
    case TimeZoneOccasionMonthDate:
        printf("%-7d ", Rule->On.MonthDay);
        break;

    case TimeZoneOccasionLastWeekday:
        printf("Last%s ", TimeZoneAbbreviatedWeekdayStrings[Weekday]);
        break;

    case TimeZoneOccasionGreaterOrEqualWeekday:
        printf("%s>=%-2d ",
               TimeZoneAbbreviatedWeekdayStrings[Weekday],
               Rule->On.MonthDay);

        break;

    case TimeZoneOccasionLessOrEqualWeekday:
        printf("%s<=%-2d ",
               TimeZoneAbbreviatedWeekdayStrings[Weekday],
               Rule->On.MonthDay);

        break;

    default:

        assert(FALSE);

        break;
    }

    PrintTimeZoneTime(Rule->At, Rule->AtLens);
    printf(" ");
    PrintTimeZoneTime(Rule->Save, TimeZoneLensLocalTime);
    printf(" %s\n",
           TimeZoneGetString(&TimeZoneStringList, Rule->LettersOffset));

    return;
}

VOID
PrintTimeZone (
    PTZC_ZONE Zone
    )

/*++

Routine Description:

    This routine prints a time zone.

Arguments:

    Zone - Supplies a pointer to the zone to print.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONG EntryIndex;
    PTZC_ZONE_ENTRY ZoneEntry;

    printf("Zone: %s (Entry index %d, count %d)\n",
           TimeZoneGetString(&TimeZoneStringList, Zone->NameOffset),
           Zone->ZoneEntryIndex,
           Zone->ZoneEntryCount);

    //
    // Skip to the proper index in the list.
    //

    CurrentEntry = TimeZoneEntryList.Next;
    for (EntryIndex = 0; EntryIndex < Zone->ZoneEntryIndex; EntryIndex += 1) {

        assert(CurrentEntry != &TimeZoneEntryList);

        CurrentEntry = CurrentEntry->Next;
    }

    for (EntryIndex = 0; EntryIndex < Zone->ZoneEntryCount; EntryIndex += 1) {

        assert(CurrentEntry != &TimeZoneEntryList);

        ZoneEntry = LIST_VALUE(CurrentEntry, TZC_ZONE_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        printf("      ");
        PrintTimeZoneEntry(ZoneEntry);
    }

    printf("\n");
    return;
}

VOID
PrintTimeZoneEntry (
    PTZC_ZONE_ENTRY ZoneEntry
    )

/*++

Routine Description:

    This routine prints a time zone entry.

Arguments:

    ZoneEntry - Supplies a pointer to the zone entry to print.

Return Value:

    None.

--*/

{

    PSTR RuleName;

    PrintTimeZoneTime(ZoneEntry->GmtOffset, TimeZoneLensLocalTime);
    printf(" ");
    if (ZoneEntry->RulesNameIndex != -1) {
        RuleName = TimeZoneGetString(&TimeZoneRuleStringList,
                                     ZoneEntry->RulesNameIndex);

        printf("%-12s ", RuleName);

    } else {
        PrintTimeZoneTime(ZoneEntry->Save, TimeZoneLensLocalTime);
        printf("   ");
    }

    printf("%-7s ",
           TimeZoneGetString(&TimeZoneStringList, ZoneEntry->FormatOffset));

    if (ZoneEntry->Until < MAX_TIME_ZONE_DATE) {
        PrintTimeZoneDate(ZoneEntry->Until);
    }

    printf("\n");
    return;
}

VOID
PrintTimeZoneLink (
    PTZC_LINK Link
    )

/*++

Routine Description:

    This routine prints a time zone link.

Arguments:

    Link - Supplies a pointer to the link to print.

Return Value:

    None.

--*/

{

    printf("Link: %s TO %s\n", Link->From, Link->To);
    return;
}

VOID
PrintTimeZoneLeap (
    PTZC_LEAP Leap
    )

/*++

Routine Description:

    This routine prints a time zone leap second.

Arguments:

    Leap - Supplies a pointer to the leap second to print.

Return Value:

    None.

--*/

{

    CHAR Correction;
    CHAR RollingOrStationary;

    printf("Leap: ");
    PrintTimeZoneDate(Leap->Date);
    Correction = '-';
    if (Leap->Positive != FALSE) {
        Correction = '+';
    }

    RollingOrStationary = 'S';
    if (Leap->LocalTime != FALSE) {
        RollingOrStationary = 'R';
    }

    printf(" %c %c\n", Correction, RollingOrStationary);
    return;
}

VOID
PrintTimeZoneTime (
    LONG Time,
    TIME_ZONE_LENS Lens
    )

/*++

Routine Description:

    This routine prints a time zone time.

Arguments:

    Time - Supplies the time to print (in seconds).

    Lens - Supplies a lens to print as well.

Return Value:

    None.

--*/

{

    LONG Hours;
    INT Length;
    CHAR LensCharacter;
    LONG Minutes;
    BOOL Negative;
    LONG Seconds;

    Length = 0;
    Negative = FALSE;
    if (Time < 0) {
        Negative = TRUE;
        Time = -Time;
    }

    Hours = Time / SECONDS_PER_HOUR;
    Time -= Hours * SECONDS_PER_HOUR;
    Minutes = Time / SECONDS_PER_MINUTE;
    Time -= Minutes * SECONDS_PER_MINUTE;
    Seconds = Time;
    if (Negative != FALSE) {
        printf("-");
        Length += 1;
    }

    printf("%d:%02d", Hours, Minutes);
    Length += 4;
    if (Hours >= 10) {
        Length += 1;
    }

    if (Seconds != 0) {
        printf(":%02d", Seconds);
        Length += 3;
    }

    switch (Lens) {
    case TimeZoneLensLocalTime:
        LensCharacter = ' ';
        break;

    case TimeZoneLensLocalStandardTime:
        LensCharacter = 's';
        break;

    case TimeZoneLensUtc:
        LensCharacter = 'u';
        break;

    default:

        assert(FALSE);

        LensCharacter = 'X';
        break;
    }

    printf("%-*c", 10 - Length, LensCharacter);
    return;
}

VOID
PrintTimeZoneDate (
    LONGLONG Date
    )

/*++

Routine Description:

    This routine prints a time zone date.

Arguments:

    Date - Supplies the date in seconds since the epoch.

Return Value:

    None.

--*/

{

    INT Day;
    LONG Days;
    INT Leap;
    INT Month;
    INT Year;

    //
    // Figure out and subtract off the year. Make the remainder positive (so
    // that something like -1 becomes December 31, 2000.
    //

    Days = Date / SECONDS_PER_DAY;
    Date -= (LONGLONG)Days * SECONDS_PER_DAY;
    if (Date < 0) {
        Date += SECONDS_PER_DAY;
        Days -= 1;
    }

    Year = ComputeYearForDays(&Days);
    Leap = 0;
    if (IS_LEAP_YEAR(Year)) {
        Leap = 1;
    }

    //
    // Subtract off the months.
    //

    Month = 0;
    Day = Days;
    while (Day >= TimeZoneDaysPerMonth[Leap][Month]) {
        Day -= TimeZoneDaysPerMonth[Leap][Month];
        Month += 1;

        assert(Month < TimeZoneMonthCount);

    }

    //
    // Days of the month start with 1.
    //

    Day += 1;

    assert(Date < SECONDS_PER_DAY);

    printf("%04d", Year);
    if ((Month != TimeZoneMonthJanuary) || (Day != 1) || (Date != 0)) {
        printf(" %s %2d ",
               TimeZoneAbbreviatedMonthStrings[Month],
               Day);

        PrintTimeZoneTime((LONG)Date, TimeZoneLensLocalTime);

    } else {
        printf("%8s", "");
    }

    return;
}

INT
CalculateOccasionForDate (
    PTIME_ZONE_OCCASION Occasion,
    INT Year,
    TIME_ZONE_MONTH Month,
    PINT Date
    )

/*++

Routine Description:

    This routine determines the day of the month for the given occasion.

Arguments:

    Occasion - Supplies a pointer to the occasion.

    Year - Supplies the year to calculate the occasion for.

    Month - Supplies the month to calculate the occasion for.

    Date - Supplies a pointer where the date (of the month) when the occasion
        occurs will be returned.

Return Value:

    0 on success.

    1 if the occasion does not occur in the given year and month.

--*/

{

    INT DaysInMonth;
    INT Leap;
    INT MonthDate;
    INT Result;
    TIME_ZONE_WEEKDAY Weekday;

    Leap = 0;
    if (IS_LEAP_YEAR(Year)) {
        Leap = 1;
    }

    DaysInMonth = TimeZoneDaysPerMonth[Leap][Month];
    if (Occasion->Type == TimeZoneOccasionMonthDate) {
        if (Occasion->MonthDay < DaysInMonth) {
            *Date = Occasion->MonthDay;
            return 0;
        }

        return EINVAL;
    }

    //
    // Calculate the weekday for the first of the month.
    //

    Result = CalculateWeekdayForMonth(Year, Month, &Weekday);
    if (Result != 0) {
        return Result;
    }

    MonthDate = 1;

    //
    // Calculate the first instance of the desired weekday.
    //

    if (Occasion->Weekday >= Weekday) {
        MonthDate += Occasion->Weekday - Weekday;

    } else {
        MonthDate += DAYS_PER_WEEK - (Weekday - Occasion->Weekday);
    }

    switch (Occasion->Type) {

    //
    // Add a week as many times as possible.
    //

    case TimeZoneOccasionLastWeekday:
        while (MonthDate + DAYS_PER_WEEK <= DaysInMonth) {
            MonthDate += DAYS_PER_WEEK;
        }

        break;

    //
    // Add a week as long as it's less than the required minimum month day. If
    // that pushes it over the month, then the occasion doesn't exist.
    //

    case TimeZoneOccasionGreaterOrEqualWeekday:
        while (MonthDate < Occasion->MonthDay) {
            MonthDate += DAYS_PER_WEEK;
        }

        if (MonthDate > DaysInMonth) {
            return EINVAL;
        }

        break;

    //
    // If the first instance of that weekday is already too far, then the
    // occasion doesn't exist. Otherwise, keep adding weeks as long as it's
    // still under the limit.
    //

    case TimeZoneOccasionLessOrEqualWeekday:
        if (MonthDate > Occasion->MonthDay) {
            return EINVAL;
        }

        while (MonthDate + DAYS_PER_WEEK < Occasion->MonthDay) {
            MonthDate += DAYS_PER_WEEK;
        }

        break;

    default:

        assert(FALSE);

        return 1;
    }

    *Date = MonthDate;
    return 0;
}

INT
CalculateWeekdayForMonth (
    INT Year,
    TIME_ZONE_MONTH Month,
    PTIME_ZONE_WEEKDAY Weekday
    )

/*++

Routine Description:

    This routine calculates the weekday for the first of the month on the
    given month and year.

Arguments:

    Year - Supplies the year to calculate the weekday for.

    Month - Supplies the month to calculate the weekday for.

    Weekday - Supplies a pointer where the weekday will be returned on success.

Return Value:

    0 on success.

    ERANGE if the result was out of range.

--*/

{

    LONG Days;
    INT Leap;
    INT Modulo;

    if ((Year > MAX_TIME_ZONE_YEAR) || (Year < MIN_TIME_ZONE_YEAR)) {
        return ERANGE;
    }

    Days = ComputeDaysForYear(Year);
    Leap = 0;
    if (IS_LEAP_YEAR(Year)) {
        Leap = 1;
    }

    Days += TimeZoneMonthDays[Leap][Month];
    Modulo = ((TIME_ZONE_EPOCH_WEEKDAY + Days) % DAYS_PER_WEEK);
    if (Modulo < 0) {
        Modulo = DAYS_PER_WEEK + Modulo;
    }

    *Weekday = Modulo;
    return 0;
}

LONG
ComputeDaysForYear (
    INT Year
    )

/*++

Routine Description:

    This routine calculates the number of days for the given year, relative to
    the epoch.

Arguments:

    Year - Supplies the target year.

Return Value:

    Returns the number of days since the epoch that January 1st of the given
    year occurred.

--*/

{

    LONG Days;

    Days = 0;
    if (Year >= TIME_ZONE_EPOCH_YEAR) {
        while (Year > TIME_ZONE_EPOCH_YEAR) {
            if (IS_LEAP_YEAR(Year)) {
                Days += DAYS_PER_LEAP_YEAR;

            } else {
                Days += DAYS_PER_YEAR;
            }

            Year -= 1;
        }

    } else {
        while (Year < TIME_ZONE_EPOCH_YEAR) {
            if (IS_LEAP_YEAR(Year)) {
                Days -= DAYS_PER_LEAP_YEAR;

            } else {
                Days -= DAYS_PER_YEAR;
            }

            Year += 1;
        }
    }

    return Days;
}

INT
ComputeYearForDays (
    PLONG Days
    )

/*++

Routine Description:

    This routine calculates the year given a number of days from the epoch.

Arguments:

    Days - Supplies a pointer to the number of days since the epoch. On
        completion, this will contain the number of remaining days after the
        years have been subtracted.

Return Value:

    Returns the year that the day resides in.

--*/

{

    LONG RemainingDays;
    INT Year;

    Year = TIME_ZONE_EPOCH_YEAR;
    RemainingDays = *Days;

    //
    // Subtract off any years after the epoch.
    //

    while (RemainingDays > 0) {
        if (IS_LEAP_YEAR(Year)) {
            RemainingDays -= DAYS_PER_LEAP_YEAR;

        } else {
            RemainingDays -= DAYS_PER_YEAR;
        }

        Year += 1;
    }

    //
    // The subtraction may have gone one too far, or the days may have
    // started negative. Either way, get the days up to a non-negative value.
    //

    while (RemainingDays < 0) {
        Year -= 1;
        if (IS_LEAP_YEAR(Year)) {
            RemainingDays += DAYS_PER_LEAP_YEAR;

        } else {
            RemainingDays += DAYS_PER_YEAR;
        }
    }

    *Days = RemainingDays;
    return Year;
}

PSTR
TimeZoneGetString (
    PLIST_ENTRY ListHead,
    ULONG Offset
    )

/*++

Routine Description:

    This routine returns the string at a given string table offset.

Arguments:

    ListHead - Supplies a pointer to the head of the list to search.

    Offset - Supplies the string table offset value to get.

Return Value:

    Returns a pointer to the string on success.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PTZC_STRING String;

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        String = LIST_VALUE(CurrentEntry, TZC_STRING, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (String->Offset == Offset) {
            return String->String;
        }
    }

    return NULL;
}

INT
TimeZoneAddString (
    PSTR String,
    PULONG Offset
    )

/*++

Routine Description:

    This routine adds a string to the string table, reusing strings if possible.

Arguments:

    String - Supplies a pointer to the string to add. A copy of this string
        will be made.

    Offset - Supplies a pointer where the string offset will be returned on
        success.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    INT Result;

    Result = TimeZoneAddStringToList(String,
                                     &TimeZoneStringList,
                                     &TimeZoneNextStringOffset,
                                     TRUE,
                                     Offset);

    return Result;
}

INT
TimeZoneAddRuleString (
    PSTR String,
    PULONG Index
    )

/*++

Routine Description:

    This routine adds a string to the rule string table, reusing strings if
    possible.

Arguments:

    String - Supplies a pointer to the string to add. A copy of this string
        will be made.

    Index - Supplies a pointer where the rule index will be returned on success.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    INT Result;

    Result = TimeZoneAddStringToList(String,
                                     &TimeZoneRuleStringList,
                                     &TimeZoneNextRuleNumber,
                                     FALSE,
                                     Index);

    return Result;
}

INT
TimeZoneAddStringToList (
    PSTR String,
    PLIST_ENTRY ListHead,
    PULONG ListSize,
    BOOL TrackSize,
    PULONG Offset
    )

/*++

Routine Description:

    This routine adds a string to the the given string table.

Arguments:

    String - Supplies a pointer to the string to add. A copy of this string
        will be made.

    ListHead - Supplies a pointer to the head of the list to add it to.

    ListSize - Supplies a pointer to the size of the list, which will be
        updated.

    TrackSize - Supplies a boolean indicating whether the list size tracks
        the total string size (TRUE) or the element count (FALSE).

    Offset - Supplies a pointer where the offset will be returned on success.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PTZC_STRING StringEntry;

    //
    // First search for the string.
    //

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        StringEntry = LIST_VALUE(CurrentEntry, TZC_STRING, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (strcmp(StringEntry->String, String) == 0) {
            *Offset = StringEntry->Offset;
            return 0;
        }
    }

    //
    // No string entry was found, create a new one.
    //

    StringEntry = malloc(sizeof(TZC_STRING));
    if (StringEntry == NULL) {
        return ENOMEM;
    }

    StringEntry->String = strdup(String);
    if (StringEntry->String == NULL) {
        free(StringEntry);
        return ENOMEM;
    }

    StringEntry->Offset = *ListSize;
    if (TrackSize != FALSE) {
        *ListSize += strlen(String) + 1;

    } else {
        *ListSize += 1;
    }

    INSERT_BEFORE(&(StringEntry->ListEntry), ListHead);
    *Offset = StringEntry->Offset;
    return 0;
}

VOID
TimeZoneCompressEntries (
    PLIST_ENTRY ZoneEntryList,
    PULONG ZoneEntryCount,
    PTZC_ZONE Zone
    )

/*++

Routine Description:

    This routine attempts to compress the zone entries by finding a repeat
    earlier. This routine requires that the zone entries be at the end of the
    list.

Arguments:

    ZoneEntryList - Supplies a pointer to the head of the list of zone entries.

    ZoneEntryCount - Supplies a pointer to the count of entries on the list,
        which may be updated.

    Zone - Supplies a pointer to the zone owning the entries.

Return Value:

    None.

--*/

{

    ULONG Count;
    PLIST_ENTRY CurrentEntry;
    PTZC_ZONE_ENTRY Entry;
    PTZC_ZONE_ENTRY Potential;
    ULONG SearchCount;
    PTZC_ZONE_ENTRY SearchEntry;
    PTZC_ZONE_ENTRY Start;

    Count = Zone->ZoneEntryCount;
    CurrentEntry = ZoneEntryList->Previous;
    for (SearchCount = 0; SearchCount < Count - 1; SearchCount += 1) {
        CurrentEntry = CurrentEntry->Previous;
    }

    Entry = LIST_VALUE(CurrentEntry, TZC_ZONE_ENTRY, ListEntry);

    assert(Zone->ZoneEntryIndex == Entry->Index);

    SearchEntry = Entry;
    SearchCount = 0;
    Start = NULL;
    CurrentEntry = ZoneEntryList->Next;
    while (CurrentEntry != ZoneEntryList) {
        Potential = LIST_VALUE(CurrentEntry, TZC_ZONE_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Stop if the element itself was found.
        //

        if (Potential == Entry) {
            break;
        }

        //
        // If this entry matches, then advance the search.
        //

        if ((Potential->RulesNameIndex == SearchEntry->RulesNameIndex) &&
            (Potential->Save == SearchEntry->Save) &&
            (Potential->FormatOffset == SearchEntry->FormatOffset) &&
            (Potential->Until == SearchEntry->Until)) {

            if (SearchCount == 0) {
                Start = Potential;
            }

            SearchCount += 1;
            if (SearchCount == Count) {
                break;
            }

            //
            // Move to the next one, and continue scanning.
            //

            SearchEntry = LIST_VALUE(SearchEntry->ListEntry.Next,
                                     TZC_ZONE_ENTRY,
                                     ListEntry);

        //
        // No match, reset.
        //

        } else {

            //
            // Redo this one against the first entry.
            //

            if (SearchCount != 0) {
                CurrentEntry = CurrentEntry->Previous;
            }

            SearchEntry = Entry;
            SearchCount = 0;
        }
    }

    if (SearchCount != Count) {
        return;
    }

    Zone->ZoneEntryIndex = Start->Index;

    //
    // Destroy the redundant elements.
    //

    CurrentEntry = &(Entry->ListEntry);
    while (CurrentEntry != ZoneEntryList) {
        CurrentEntry = CurrentEntry->Next;
        LIST_REMOVE(&(Entry->ListEntry));
        DestroyTimeZoneEntry(Entry);
        *ZoneEntryCount -= 1;
        Entry = LIST_VALUE(CurrentEntry, TZC_ZONE_ENTRY, ListEntry);
    }

    return;
}

