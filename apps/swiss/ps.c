/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ps.c

Abstract:

    This module implements the ps (process status) utility.

Author:

    Chris Stevens 12-Aug-2014

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PS_VERSION_MAJOR 1
#define PS_VERSION_MINOR 0

#define PS_USAGE                                                               \
    "usage: ps [-aA] [-defl] [-G grouplist] [-p proclist] [-t termlist] \n"    \
    "          [-U userlist] [-g grouplist] [-u userlist] [-o format]\n\n"     \
    "The ps utility writes process status to standard out. Options are:\n"     \
    "  -a --all-terminals -- Write status for all processes associated with\n" \
    "        terminals.\n"                                                     \
    "  -A --all -- Write status for all processes.\n"                          \
    "  -d --all-no-leaders -- Write status for all processes except session\n" \
    "        leaders.\n"                                                       \
    "  -e --all -- Write status for all processes. (Equivalent to -A).\n"      \
    "  -f --full -- Write the full status format.\n"                           \
    "  -g --group=grouplist -- Write status for all processes whose session\n" \
    "        leaders are in the given group list.\n"                           \
    "  -G --Group=grouplist -- Write status for all processes whose real\n"    \
    "        group ID's are in the group list.\n"                              \
    "  -l --long -- Write the long status format.\n"                           \
    "  -o --format=format -- Override the default format with a "              \
    "comma-separated\n"                                                        \
    "        list of process status data types.\n"                             \
    "  -p --pid=pidlist -- Write status for the processes whose process IDs\n" \
    "        are in the given process list.\n"                                 \
    "  -t --tty=termlist -- Write status for the processes whose terminals\n"  \
    "        are in the given terminal list.\n"                                \
    "  -u --user=userlist -- Write status for the processes whose user ID\n"   \
    "        number or login name are in the given user list.\n"               \
    "  -U --User=userlist -- Write status for the processes whose real user\n" \
    "        ID or login name are in the given user list.\n"                   \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define PS_OPTIONS_STRING "aAdefg:G:lo:p:t:u:U:hV"

//
// Define ps options.
//

//
// Set this option to report information for all processes associated with
// terminals.
//

#define PS_OPTION_REPORT_ALL_TERMINAL_PROCESSES 0x00000001

//
// Set this option to report information for all processes.
//

#define PS_OPTION_REPORT_ALL_PROCESSES 0x00000002

//
// Set this option to report information for all processes, except session
// leaders.
//

#define PS_OPTION_REPORT_ALL_PROCESSES_NO_LEADERS 0x00000004

//
// Set this option to generate a full report.
//

#define PS_OPTION_FULL_REPORT 0x00000008

//
// Set this option to report information for all processes whose session
// leaders are in the session leader list.
//

#define PS_OPTION_SESSION_LEADERS_LIST 0x00000010

//
// Set this option to report information for all processes whose real group IDs
// are in the real group ID list.
//

#define PS_OPTION_REAL_GROUP_ID_LIST 0x00000020

//
// Set this option to generate a long repot.
//

#define PS_OPTION_LONG_REPORT 0x00000040

//
// Set this option to generate a long repot.
//

#define PS_OPTION_CUSTOM_FORMAT 0x00000080

//
// Set this option to report information for all the processes whose process
// IDs are in the process ID list.
//

#define PS_OPTION_PROCESS_ID_LIST 0x00000100

//
// Set this option to report information for all processes whose terminals are
// in the terminal list.
//

#define PS_OPTION_TERMINAL_LIST 0x00000200

//
// Set this option to report information for all processes whose user ID or
// login name appear in the user list.
//

#define PS_OPTION_USER_LIST 0x00000400

//
// Set this option to report information for all processes whose real user ID
// or login name appear in the real user list.
//

#define PS_OPTION_REAL_USER_LIST 0x00000800

//
// Define a mask of all report format types.
//

#define PS_OPTION_REPORT_MASK (PS_OPTION_FULL_REPORT | PS_OPTION_LONG_REPORT)

//
// Define a mask of all process filter types.
//

#define PS_OPTION_FILTER_MASK                    \
    (PS_OPTION_REPORT_ALL_TERMINAL_PROCESSES |   \
     PS_OPTION_REPORT_ALL_PROCESSES |            \
     PS_OPTION_REPORT_ALL_PROCESSES_NO_LEADERS | \
     PS_OPTION_SESSION_LEADERS_LIST |              \
     PS_OPTION_REAL_GROUP_ID_LIST |              \
     PS_OPTION_PROCESS_ID_LIST |                 \
     PS_OPTION_TERMINAL_LIST |                   \
     PS_OPTION_USER_LIST |                       \
     PS_OPTION_REAL_USER_LIST)

//
// Define the number of format options in the default set.
//

#define PS_DEFAULT_REPORT_COUNT 15

//
// Define the flags for the default report.
//

#define PS_DEFAULT_REPORT_FLAG_BASIC   0x01
#define PS_DEFAULT_REPORT_FLAG_LONG    0x02
#define PS_DEFAULT_REPORT_FLAG_FULL    0x04
#define PS_DEFAULT_REPORT_FLAG_ALL     0x07

//
// Define the fudge factor to give the process list ID query a better chance of
// success.
//

#define PS_PROCESS_LIST_FUDGE_FACTOR 2

//
// Define the initial number of processes to
//

#define PS_PROCESS_LIST_INITIAL_COUNT 10

//
// Define the number or retries allowed to gather the list of process IDs.
//

#define PS_GET_PROCESS_LIST_RETRY_COUNT 10

//
// Define the character that represents data that could not be retrieved for
// the process.
//

#define PS_MISSING_DATA_CHARACTER '-'

//
// Define time formats for the CPU time data.
//

#define PS_CPU_TIME_DAYS_FORMAT "%d-%02d:%02d:%02d"
#define PS_CPU_TIME_DEFAULT_FORMAT "%02d:%02d:%02d"

//
// Define time formats for the elapsed time data.
//

#define PS_ELAPSED_TIME_DAYS_FORMAT "%d-%02d:%02d:%02d"
#define PS_ELAPSED_TIME_HOURS_FORMAT "%02d:%02d:%02d"
#define PS_ELAPSED_TIME_DEFAULT_FORMAT "%02d:%02d"

//
// Define basic date values.
//

#define PS_MONTHS_PER_YEAR 12
#define PS_SECONDS_PER_DAY (PS_SECONDS_PER_HOUR * PS_HOURS_PER_DAY)
#define PS_SECONDS_PER_HOUR (PS_SECONDS_PER_MINUTE * PS_MINUTES_PER_HOUR)
#define PS_SECONDS_PER_MINUTE 60
#define PS_MINUTES_PER_HOUR 60
#define PS_HOURS_PER_DAY 24

//
// Define the invalid terminal ID.
//

#define INVALID_TERMINAL_ID (int)-1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PS_DATA_TYPE {
    PsDataFlags,
    PsDataState,
    PsDataUserIdentifier,
    PsDataUserIdentifierText,
    PsDataRealUserIdentifier,
    PsDataEffectiveUserIdentifier,
    PsDataProcessIdentifier,
    PsDataParentProcessIdentifier,
    PsDataProcessGroupIdentifier,
    PsDataRealGroupIdentifier,
    PsDataEffectiveGroupIdentifier,
    PsDataSchedulingTime,
    PsDataPriority,
    PsDataNiceValue,
    PsDataAddress,
    PsDataBlockSize,
    PsDataVirtualSize,
    PsDataWaitEvent,
    PsDataStartTime,
    PsDataTerminal,
    PsDataElapsedTime,
    PsDataCpuTime,
    PsDataCmdName,
    PsDataCmdArguments,
    PsDataCommandName,
    PsDataCommandArguments,
    PsDataCpuPercentage,
    PsDataTypeMax,
    PsDataTypeInvalid
} PS_DATA_TYPE, *PPS_DATA_TYPE;

/*++

Structure Description:

    This structure defines a data column that can be included in one of the
    three default reports (basic, full, and long).

Members:

    Type - Stores the type of data to display in the column.

    FullType - Stores the type of data to display in the column if a full
        report is requested.

    Flags - Stores a bitmask of report flags. See PS_DEFAULT_REPORT_FLAG_* for
        definitions.

--*/

typedef struct _PS_DEFAULT_REPORT {
    PS_DATA_TYPE Type;
    PS_DATA_TYPE FullType;
    UCHAR Flags;
} PS_DEFAULT_REPORT, *PPS_DEFAULT_REPORT;

/*++

Structure Description:

    This structure defines a mapping between a custom format command line
    string and the data type to display in the column.

Members:

    Format - Stores the string name of the data type to display in the column.

    Type - Stores the data type represented by the string.

--*/

typedef struct _PS_CUSTOM_FORMAT_MAP_ENTRY {
    PSTR Format;
    PS_DATA_TYPE Type;
} PS_CUSTOM_FORMAT_MAP_ENTRY, *PPS_CUSTOM_FORMAT_MAP_ENTRY;

/*++

Structure Description:

    This structure defines column display information.

Members:

    Header - Stores a string containing the header of the column.

    Width - Stores the width of the column, in characters.

    RightJustified - Stores whether or not the header and data value should
        be right justified in the column.

--*/

typedef struct _PS_COLUMN {
    PSTR Header;
    ULONG Width;
    BOOL RightJustified;
} PS_COLUMN, *PPS_COLUMN;

/*++

Structure Description:

    This structure defines a custom display format entry.

Members:

    ListEntry - Stores pointers to the next and previous custom format entries.

    Type - Stores the data type to display for the custom format column.

    HeaderOverrideValid - Stores a boolean indicating whether or not the header
        override string is valid.

    HeaderOverrideWidth - Stores the width of the column, in characters, when
        the header override is valid.

    HeaderOverride - Stores the header override string.

--*/

typedef struct _PS_CUSTOM_FORMAT_ENTRY {
    LIST_ENTRY ListEntry;
    PS_DATA_TYPE Type;
    BOOL HeaderOverrideValid;
    ULONG HeaderOverrideWidth;
    PSTR HeaderOverride;
} PS_CUSTOM_FORMAT_ENTRY, *PPS_CUSTOM_FORMAT_ENTRY;

/*++

Structure Description:

    This structure defines an entry into a process filter list. This is used to
    determine which processes should be included in the status report.

Members:

    ListEntry - Stores pointers to the next and previous filter entries.

    NumericId - Stores the numeric ID to filter on (e.g. process ID, session ID,
        group ID, etc.).

    TextId - Stores the string ID to filter on (e.g. terminal name, group name,
        etc.).

--*/

typedef struct _PS_FILTER_ENTRY {
    LIST_ENTRY ListEntry;
    ULONG NumericId;
    PSTR TextId;
} PS_FILTER_ENTRY, *PPS_FILTER_ENTRY;

/*++

Structure Description:

    This structure defines the application context for the PS utility.

Members:

    CustomFormatList - Stores the head of the list of custom column formats.

    SessionLeaderList - Stores the head of the list of session leader IDs to
        filter on.

    RealGroupIdList - Stores the head of the list of real group IDs to filter
        on.

    TerminalList - Stores the head of the list of terminal IDs to filter on.

    UserIdList - Stores the head of the list of user IDs to filter on.

    RealUserIdList - Stores the head of the list of real user IDs to filter on.

    ProcessIdList - Stores an array of process IDs to filter on.

    ProcessIdCount - Stores the number of process IDs in the array.

    DisplayHeaderLine - Stores a boolean indicating whether or not to display
        the line of headers.

--*/

typedef struct _PS_CONTEXT {
    LIST_ENTRY CustomFormatList;
    LIST_ENTRY SessionLeaderList;
    LIST_ENTRY RealGroupIdList;
    LIST_ENTRY TerminalList;
    LIST_ENTRY UserIdList;
    LIST_ENTRY RealUserIdList;
    pid_t *ProcessIdList;
    UINTN ProcessIdCount;
    BOOL DisplayHeaderLine;
} PS_CONTEXT, *PPS_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
PspInitializeContext (
    PPS_CONTEXT Context
    );

VOID
PspDestroyContext (
    PPS_CONTEXT Context
    );

VOID
PspPrintHeaders (
    PPS_CONTEXT Context,
    ULONG Options
    );

VOID
PspPrintProcessInformation (
    PPS_CONTEXT Context,
    ULONG Options,
    PSWISS_PROCESS_INFORMATION Information
    );

VOID
PspPrintDataType (
    PS_DATA_TYPE Type,
    PSWISS_PROCESS_INFORMATION Information
    );

VOID
PspFilterProcessInformationList (
    PPS_CONTEXT Context,
    ULONG Options,
    PSWISS_PROCESS_INFORMATION *ProcessInformationList,
    UINTN ProcessCount
    );

INT
PspParseFormatList (
    PPS_CONTEXT Context,
    PSTR String
    );

INT
PspParseFilterList (
    PLIST_ENTRY ListHead,
    PSTR String
    );

INT
PspParseProcessList (
    PPS_CONTEXT Context,
    PSTR String
    );

PSWISS_PROCESS_INFORMATION *
PspGetProcessInformationList (
    pid_t *ProcessIdList,
    UINTN ProcessCount
    );

VOID
PspDestroyProcessInformationList (
    PSWISS_PROCESS_INFORMATION *ProcessInformationList,
    UINTN ProcessCount
    );

VOID
PspRemoveDuplicateProcessIds (
    pid_t *ProcessIdList,
    PUINTN ProcessIdCount
    );

INT
PspCompareProcessIds (
    const VOID *First,
    const VOID *Second
    );

//
// -------------------------------------------------------------------- Globals
//

struct option PsLongOptions[] = {
    {"all-terminals", no_argument, 0, 'a'},
    {"all", no_argument, 0, 'A'},
    {"all-no-leaders", no_argument, 0, 'd'},
    {"full", no_argument, 0, 'f'},
    {"group", required_argument, 0, 'g'},
    {"Group", required_argument, 0, 'G'},
    {"long", no_argument, 0, 'l'},
    {"format", required_argument, 0, 'o'},
    {"pid", required_argument, 0, 'p'},
    {"tty", required_argument, 0, 't'},
    {"user", required_argument, 0, 'u'},
    {"User", required_argument, 0, 'U'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// Store the set of column information for each data type.
//

PS_COLUMN PsColumnInformation[PsDataTypeMax] = {
    {"F", 1, TRUE},
    {"S", 1, FALSE},
    {"UID", 5, TRUE},
    {"UID", 7, FALSE},
    {"RUSER", 7, FALSE},
    {"USER", 7, FALSE},
    {"PID", 5, TRUE},
    {"PPID", 5, TRUE},
    {"PGID", 5, TRUE},
    {"RGROUP", 7, FALSE},
    {"GROUP", 7, FALSE},
    {"C", 2, TRUE},
    {"PRI", 3, TRUE},
    {"NI", 3, TRUE},
    {"ADDR", 4, TRUE},
    {"SZ", 5, TRUE},
    {"VSZ", 6, TRUE},
    {"WCHAN", 6, FALSE},
    {"STIME", 5, TRUE},
    {"TTY", 8, FALSE},
    {"ELAPSED", 12, TRUE},
    {"TIME", 12, TRUE},
    {"CMD", 15, FALSE},
    {"CMD", 27, FALSE},
    {"COMMAND", 15, FALSE},
    {"COMMAND", 27, FALSE},
    {"CPU%", 4, TRUE},
};

//
// Store the set of supported custom column formats associated the command line
// option with a data type.
//

PS_CUSTOM_FORMAT_MAP_ENTRY PsCustomFormatMap[] = {
    {"addr", PsDataAddress},
    {"args", PsDataCommandArguments},
    {"c", PsDataSchedulingTime},
    {"cmd", PsDataCmdArguments},
    {"comm", PsDataCommandName},
    {"etime", PsDataElapsedTime},
    {"f", PsDataFlags},
    {"flag", PsDataFlags},
    {"flags", PsDataFlags},
    {"group", PsDataEffectiveGroupIdentifier},
    {"nice", PsDataNiceValue},
    {"pcpu", PsDataCpuPercentage},
    {"pgid", PsDataProcessGroupIdentifier},
    {"pid", PsDataProcessIdentifier},
    {"ppid", PsDataParentProcessIdentifier},
    {"pri", PsDataPriority},
    {"rgroup", PsDataRealGroupIdentifier},
    {"ruser", PsDataRealUserIdentifier},
    {"s", PsDataState},
    {"state", PsDataState},
    {"stime", PsDataStartTime},
    {"sz", PsDataBlockSize},
    {"time", PsDataCpuTime},
    {"tty", PsDataTerminal},
    {"uid", PsDataUserIdentifier},
    {"user", PsDataEffectiveUserIdentifier},
    {"vsz", PsDataVirtualSize},
    {"wchan", PsDataWaitEvent},
    {NULL, PsDataTypeMax},
};

//
// Store the default reports information.
//

PS_DEFAULT_REPORT PsDefaultReports[PS_DEFAULT_REPORT_COUNT] = {
    {PsDataFlags, PsDataTypeInvalid, PS_DEFAULT_REPORT_FLAG_LONG},
    {PsDataState, PsDataTypeInvalid, PS_DEFAULT_REPORT_FLAG_LONG},
    {PsDataUserIdentifier,
     PsDataUserIdentifierText,
     (PS_DEFAULT_REPORT_FLAG_LONG | PS_DEFAULT_REPORT_FLAG_FULL)},
    {PsDataProcessIdentifier,
     PsDataProcessIdentifier,
     PS_DEFAULT_REPORT_FLAG_ALL},
    {PsDataParentProcessIdentifier,
     PsDataParentProcessIdentifier,
     (PS_DEFAULT_REPORT_FLAG_LONG | PS_DEFAULT_REPORT_FLAG_FULL)},
    {PsDataSchedulingTime,
     PsDataSchedulingTime,
     (PS_DEFAULT_REPORT_FLAG_LONG | PS_DEFAULT_REPORT_FLAG_FULL)},
    {PsDataPriority, PsDataTypeInvalid, PS_DEFAULT_REPORT_FLAG_LONG},
    {PsDataNiceValue, PsDataTypeInvalid, PS_DEFAULT_REPORT_FLAG_LONG},
    {PsDataAddress, PsDataTypeInvalid, PS_DEFAULT_REPORT_FLAG_LONG},
    {PsDataBlockSize, PsDataTypeInvalid, PS_DEFAULT_REPORT_FLAG_LONG},
    {PsDataWaitEvent, PsDataTypeInvalid, PS_DEFAULT_REPORT_FLAG_LONG},
    {PsDataStartTime, PsDataStartTime, PS_DEFAULT_REPORT_FLAG_FULL},
    {PsDataTerminal, PsDataTerminal, PS_DEFAULT_REPORT_FLAG_ALL},
    {PsDataCpuTime, PsDataCpuTime, PS_DEFAULT_REPORT_FLAG_ALL},
    {PsDataCmdName, PsDataCmdArguments, PS_DEFAULT_REPORT_FLAG_ALL},
};

//
// Store the month abbreviations.
//

PSTR PsMonths[PS_MONTHS_PER_YEAR] = {
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

//
// Store the display output for different process states.
//

PSTR PsProcessStateStrings[SwissProcessStateMax] = {
    "R",
    "D",
    "S",
    "T",
    "X",
    "Z",
    "?"
};

//
// ------------------------------------------------------------------ Functions
//

INT
PsMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the ps utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, non-zero otherwise.

--*/

{

    PSTR Argument;
    PS_CONTEXT Context;
    UINTN Index;
    INT Option;
    ULONG Options;
    UINTN ProcessCount;
    pid_t *ProcessIdList;
    size_t ProcessIdListSize;
    PSWISS_PROCESS_INFORMATION *ProcessInformationList;
    UINTN Retry;
    int Status;

    PspInitializeContext(&Context);
    Options = 0;
    ProcessIdList = NULL;
    ProcessIdListSize = 0;
    ProcessInformationList = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             PS_OPTIONS_STRING,
                             PsLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'a':
            Options |= PS_OPTION_REPORT_ALL_TERMINAL_PROCESSES;
            break;

        case 'A':
        case 'e':
            Options |= PS_OPTION_REPORT_ALL_PROCESSES;
            break;

        case 'd':
            Options |= PS_OPTION_REPORT_ALL_PROCESSES_NO_LEADERS;
            break;

        case 'f':
            Options |= PS_OPTION_FULL_REPORT;
            break;

        case 'g':
            Options |= PS_OPTION_SESSION_LEADERS_LIST;
            Argument = optarg;

            assert(Arguments != NULL);

            Status = PspParseFilterList(&(Context.SessionLeaderList), Argument);
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'G':
            Options |= PS_OPTION_REAL_GROUP_ID_LIST;
            Argument = optarg;

            assert(Arguments != NULL);

            Status = PspParseFilterList(&(Context.RealGroupIdList), Argument);
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'l':
            Options |= PS_OPTION_LONG_REPORT;
            break;

        case 'o':
            Options |= PS_OPTION_CUSTOM_FORMAT;
            Argument = optarg;

            assert(Argument != NULL);

            Status = PspParseFormatList(&Context, Argument);
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'p':
            Options |= PS_OPTION_PROCESS_ID_LIST;
            Argument = optarg;

            assert(Argument != NULL);

            Status = PspParseProcessList(&Context, Argument);
            if (Status != 0) {
                goto MainEnd;
            }

            assert(Context.ProcessIdList != NULL);

            break;

        case 't':
            Options |= PS_OPTION_TERMINAL_LIST;
            Argument = optarg;

            assert(Arguments != NULL);

            Status = PspParseFilterList(&(Context.TerminalList), Argument);
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'u':
            Options |= PS_OPTION_USER_LIST;
            Argument = optarg;

            assert(Arguments != NULL);

            Status = PspParseFilterList(&(Context.UserIdList), Argument);
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'U':
            Options |= PS_OPTION_REAL_USER_LIST;
            Argument = optarg;

            assert(Arguments != NULL);

            Status = PspParseFilterList(&(Context.RealUserIdList), Argument);
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'V':
            SwPrintVersion(PS_VERSION_MAJOR, PS_VERSION_MINOR);
            return 1;

        case 'h':
            printf(PS_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // If a custom format is supplied, then neither the 'long' or 'full'
    // arguments should be supplied.
    //

    if (((Options & PS_OPTION_CUSTOM_FORMAT) != 0) &&
        ((Options & (PS_OPTION_LONG_REPORT | PS_OPTION_FULL_REPORT)) != 0)) {

        SwPrintError(EINVAL, NULL, "Conflicting format options");
        printf(PS_USAGE);
        Status = 1;
        goto MainEnd;
    }

    //
    // In most cases, the entire list of process IDs is needs to be collected
    // and then filtered. The exception is if only a process list was specified
    // on the command line.
    //

    if ((Options & PS_OPTION_FILTER_MASK) != PS_OPTION_PROCESS_ID_LIST) {
        Retry = 0;
        ProcessIdListSize = PS_PROCESS_LIST_INITIAL_COUNT * sizeof(pid_t);
        do {
            Retry += 1;
            if (ProcessIdList != NULL) {
                free(ProcessIdList);
            }

            ProcessIdList = malloc(ProcessIdListSize);
            if (ProcessIdList == NULL) {
                Status = 1;
                goto MainEnd;
            }

            Status = SwGetProcessIdList(ProcessIdList, &ProcessIdListSize);
            if (Status == 0) {
                break;
            }

            ProcessIdListSize *= PS_PROCESS_LIST_FUDGE_FACTOR;

        } while (Retry <= PS_GET_PROCESS_LIST_RETRY_COUNT);

        if (Status != 0) {
            goto MainEnd;
        }

        ProcessCount = ProcessIdListSize / sizeof(pid_t);

        //
        // Sort the process ID list. The system shouldn't be returning
        // duplicates, so skip that step.
        //

        qsort(ProcessIdList,
              ProcessCount,
              sizeof(pid_t),
              PspCompareProcessIds);

        //
        // Get the list of process information for the process IDs.
        //

        ProcessInformationList = PspGetProcessInformationList(ProcessIdList,
                                                              ProcessCount);

        if (ProcessInformationList == NULL) {
            Status = -1;
            goto MainEnd;
        }

        //
        // With a list of all the process information, filter it based on the
        // command line options.
        //

        PspFilterProcessInformationList(&Context,
                                        Options,
                                        ProcessInformationList,
                                        ProcessCount);

    } else {
        ProcessIdList = Context.ProcessIdList;
        ProcessCount = Context.ProcessIdCount;

        //
        // Remove duplicates from the list. This will sort the list as well.
        //

        PspRemoveDuplicateProcessIds(ProcessIdList, &ProcessCount);

        //
        // Get the list of process information for the process IDs.
        //

        ProcessInformationList = PspGetProcessInformationList(ProcessIdList,
                                                              ProcessCount);

        if (ProcessInformationList == NULL) {
            Status = -1;
            goto MainEnd;
        }
    }

    assert(ProcessInformationList != NULL);

    //
    // Display the column headers and print the data for each process.
    //

    PspPrintHeaders(&Context, Options);
    for (Index = 0; Index < ProcessCount; Index += 1) {
        if (ProcessInformationList[Index] != NULL) {
            PspPrintProcessInformation(&Context,
                                       Options,
                                       ProcessInformationList[Index]);
        }
    }

    Status = 0;

MainEnd:
    if ((ProcessIdList != NULL) && (ProcessIdList != Context.ProcessIdList)) {
        free(ProcessIdList);
    }

    if (ProcessInformationList != NULL) {
        PspDestroyProcessInformationList(ProcessInformationList, ProcessCount);
    }

    PspDestroyContext(&Context);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
PspInitializeContext (
    PPS_CONTEXT Context
    )

/*++

Routine Description:

    This routine intitializes the given process status context.

Arguments:

    Context - Supplies a pointer to the process status context to initialize.

Return Value:

    None.

--*/

{

    memset(Context, 0, sizeof(PS_CONTEXT));
    INITIALIZE_LIST_HEAD(&(Context->CustomFormatList));
    INITIALIZE_LIST_HEAD(&(Context->SessionLeaderList));
    INITIALIZE_LIST_HEAD(&(Context->RealGroupIdList));
    INITIALIZE_LIST_HEAD(&(Context->TerminalList));
    INITIALIZE_LIST_HEAD(&(Context->UserIdList));
    INITIALIZE_LIST_HEAD(&(Context->RealUserIdList));
    return;
}

VOID
PspDestroyContext (
    PPS_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the given process status context, without releasing
    the pointer itself.

Arguments:

    Context - Supplies a pointer to the process status context to destroy.

Return Value:

    None.

--*/

{

    PPS_FILTER_ENTRY FilterEntry;
    PPS_CUSTOM_FORMAT_ENTRY FormatEntry;

    if (Context->ProcessIdList != NULL) {
        free(Context->ProcessIdList);
    }

    while (LIST_EMPTY(&(Context->CustomFormatList)) == FALSE) {
        FormatEntry = LIST_VALUE(Context->CustomFormatList.Next,
                                 PS_CUSTOM_FORMAT_ENTRY,
                                 ListEntry);

        LIST_REMOVE(&(FormatEntry->ListEntry));
        free(FormatEntry);
    }

    while (LIST_EMPTY(&(Context->SessionLeaderList)) == FALSE) {
        FilterEntry = LIST_VALUE(Context->SessionLeaderList.Next,
                                 PS_FILTER_ENTRY,
                                 ListEntry);

        LIST_REMOVE(&(FilterEntry->ListEntry));
        free(FilterEntry);
    }

    while (LIST_EMPTY(&(Context->RealGroupIdList)) == FALSE) {
        FilterEntry = LIST_VALUE(Context->RealGroupIdList.Next,
                                 PS_FILTER_ENTRY,
                                 ListEntry);

        LIST_REMOVE(&(FilterEntry->ListEntry));
        free(FilterEntry);
    }

    while (LIST_EMPTY(&(Context->TerminalList)) == FALSE) {
        FilterEntry = LIST_VALUE(Context->TerminalList.Next,
                                 PS_FILTER_ENTRY,
                                 ListEntry);

        LIST_REMOVE(&(FilterEntry->ListEntry));
        free(FilterEntry);
    }

    while (LIST_EMPTY(&(Context->UserIdList)) == FALSE) {
        FilterEntry = LIST_VALUE(Context->UserIdList.Next,
                                 PS_FILTER_ENTRY,
                                 ListEntry);

        LIST_REMOVE(&(FilterEntry->ListEntry));
        free(FilterEntry);
    }

    while (LIST_EMPTY(&(Context->RealUserIdList)) == FALSE) {
        FilterEntry = LIST_VALUE(Context->RealUserIdList.Next,
                                 PS_FILTER_ENTRY,
                                 ListEntry);

        LIST_REMOVE(&(FilterEntry->ListEntry));
        free(FilterEntry);
    }

    return;
}

VOID
PspPrintHeaders (
    PPS_CONTEXT Context,
    ULONG Options
    )

/*++

Routine Description:

    This routine displays the gathered process information to standard out.

Arguments:

    Context - Supplies a pointer to the application context.

    Options - Supplies a bitmask of application options.

Return Value:

    None.

--*/

{

    PPS_COLUMN Column;
    ULONG ColumnCount;
    PPS_CUSTOM_FORMAT_ENTRY ColumnEntry;
    ULONG ColumnWidth;
    PLIST_ENTRY CurrentEntry;
    PSTR Header;
    UINTN Index;
    PPS_DEFAULT_REPORT Report;
    PS_DATA_TYPE Type;

    //
    // Print out the column headers.
    //

    ColumnCount = 0;
    if ((Options & PS_OPTION_CUSTOM_FORMAT) != 0) {
        if (Context->DisplayHeaderLine != FALSE) {
            CurrentEntry = Context->CustomFormatList.Next;
            while (CurrentEntry != &(Context->CustomFormatList)) {
                ColumnEntry = LIST_VALUE(CurrentEntry,
                                         PS_CUSTOM_FORMAT_ENTRY,
                                         ListEntry);

                CurrentEntry = CurrentEntry->Next;
                Column = &(PsColumnInformation[ColumnEntry->Type]);
                if (ColumnEntry->HeaderOverrideValid != FALSE) {
                    Header = ColumnEntry->HeaderOverride;
                    ColumnWidth = ColumnEntry->HeaderOverrideWidth;

                } else {
                    Header = Column->Header;
                    ColumnWidth = Column->Width;
                }

                if (ColumnCount != 0) {
                    printf(" ");
                }

                if (Column->RightJustified != FALSE) {
                    printf("%*s", ColumnWidth, Header);

                } else {
                    printf("%-*s", ColumnWidth, Header);
                }

                ColumnCount += 1;
            }

            printf("\r\n");
        }

    } else {
        for (Index = 0; Index < PS_DEFAULT_REPORT_COUNT; Index += 1) {
            Report = &(PsDefaultReports[Index]);
            Type = PsDataTypeInvalid;
            if ((Options & PS_OPTION_REPORT_MASK) == 0) {
                if ((Report->Flags & PS_DEFAULT_REPORT_FLAG_BASIC) != 0) {
                    Type = Report->Type;
                }

            } else {
                if (((Options & PS_OPTION_LONG_REPORT) != 0) &&
                    ((Report->Flags & PS_DEFAULT_REPORT_FLAG_LONG) != 0)) {

                    Type = Report->Type;
                }

                //
                // The full report type trumps the long report type.
                //

                if (((Options & PS_OPTION_FULL_REPORT) != 0) &&
                    ((Report->Flags & PS_DEFAULT_REPORT_FLAG_FULL) != 0)) {

                    assert(Report->FullType != PsDataTypeInvalid);

                    Type = Report->FullType;
                }
            }

            //
            // If the report doesn't get included based on the options, skip
            // it.
            //

            if (Type == PsDataTypeInvalid) {
                continue;
            }

            if (ColumnCount != 0) {
                printf(" ");
            }

            Column = &(PsColumnInformation[Type]);
            if (Column->RightJustified != FALSE) {
                printf("%*s", Column->Width, Column->Header);

            } else {
                printf("%-*s", Column->Width, Column->Header);
            }

            ColumnCount += 1;
        }

        printf("\r\n");
    }

    return;
}

VOID
PspPrintProcessInformation (
    PPS_CONTEXT Context,
    ULONG Options,
    PSWISS_PROCESS_INFORMATION Information
    )

/*++

Routine Description:

    This routine displays the gathered process information to standard out.

Arguments:

    Context - Supplies a pointer to the application context.

    Options - Supplies a bitmask of application options.

    Information - Supplies a pointer to the swiss process information to print.

Return Value:

    None.

--*/

{

    ULONG ColumnCount;
    PPS_CUSTOM_FORMAT_ENTRY ColumnEntry;
    PLIST_ENTRY CurrentEntry;
    UINTN Index;
    PPS_DEFAULT_REPORT Report;
    PS_DATA_TYPE Type;

    //
    // Print out the data for each column.
    //

    ColumnCount = 0;
    if ((Options & PS_OPTION_CUSTOM_FORMAT) != 0) {
        CurrentEntry = Context->CustomFormatList.Next;
        while (CurrentEntry != &(Context->CustomFormatList)) {
            ColumnEntry = LIST_VALUE(CurrentEntry,
                                     PS_CUSTOM_FORMAT_ENTRY,
                                     ListEntry);

            CurrentEntry = CurrentEntry->Next;
            if (ColumnCount != 0) {
                printf(" ");
            }

            PspPrintDataType(ColumnEntry->Type, Information);
            ColumnCount += 1;
        }

    } else {
        for (Index = 0; Index < PS_DEFAULT_REPORT_COUNT; Index += 1) {
            Report = &(PsDefaultReports[Index]);
            Type = PsDataTypeInvalid;
            if ((Options & PS_OPTION_REPORT_MASK) == 0) {
                if ((Report->Flags & PS_DEFAULT_REPORT_FLAG_BASIC) != 0) {
                    Type = Report->Type;
                }

            } else {
                if (((Options & PS_OPTION_LONG_REPORT) != 0) &&
                    ((Report->Flags & PS_DEFAULT_REPORT_FLAG_LONG) != 0)) {

                    Type = Report->Type;
                }

                //
                // The full report type trumps the long report type.
                //

                if (((Options & PS_OPTION_FULL_REPORT) != 0) &&
                    ((Report->Flags & PS_DEFAULT_REPORT_FLAG_FULL) != 0)) {

                    assert(Report->FullType != PsDataTypeInvalid);

                    Type = Report->FullType;
                }
            }

            //
            // If the report doesn't get included based on the options, skip
            // it.
            //

            if (Type == PsDataTypeInvalid) {
                continue;
            }

            if (ColumnCount != 0) {
                printf(" ");
            }

            PspPrintDataType(Type, Information);
            ColumnCount += 1;
        }
    }

    printf("\r\n");
    return;
}

VOID
PspPrintDataType (
    PS_DATA_TYPE Type,
    PSWISS_PROCESS_INFORMATION Information
    )

/*++

Routine Description:

    This routine prints out the data for the given type based on the supplied
    process information.

Arguments:

    Type - Supplies the type of data to print out.

    Information - Supplies the process information to print.

Return Value:

    None.

--*/

{

    BOOL AllocatedString;
    PSTR Arguments;
    int BytesConverted;
    size_t CharactersPrinted;
    PPS_COLUMN Column;
    time_t CpuTime;
    struct tm CurrentDate;
    time_t CurrentTime;
    BOOL DataAvailable;
    struct tm DateData;
    ULONG Days;
    time_t ElapsedTime;
    double FloatData;
    ULONG Hours;
    INT IntegerData;
    size_t LengthRemaining;
    ULONG Minutes;
    size_t PageSize;
    INT Result;
    ULONG Seconds;
    size_t SizeData;
    PSTR StringData;
    size_t StringDataSize;
    time_t TimeData;
    size_t WidthRemaining;

    AllocatedString = FALSE;
    Column = &(PsColumnInformation[Type]);
    DataAvailable = TRUE;
    FloatData = 0;
    IntegerData = 0;
    SizeData = 0;
    StringData = NULL;

    //
    // Collect the data for the given type.
    //

    switch (Type) {
    case PsDataAddress:

        //
        // The address is never displayed.
        //

        DataAvailable = FALSE;
        break;

    case PsDataWaitEvent:

        //
        // TODO: Display the name of the routine the process is waiting on.
        //

        DataAvailable = FALSE;
        break;

    case PsDataFlags:
        IntegerData = Information->Flags;
        break;

    case PsDataState:
        StringData = PsProcessStateStrings[Information->State];
        break;

    case PsDataBlockSize:
        PageSize = SwGetPageSize();
        if (PageSize <= 0) {
            DataAvailable = FALSE;
            break;
        }

        SizeData = Information->ImageSize / PageSize;
        break;

    case PsDataVirtualSize:
        SizeData = Information->ImageSize / _1KB;
        break;

    case PsDataCpuPercentage:
        CpuTime = Information->KernelTime + Information->UserTime;
        CurrentTime = time(NULL);
        ElapsedTime = CurrentTime - Information->StartTime;
        if (ElapsedTime != 0) {
            FloatData = (double)CpuTime / (double)ElapsedTime;
        }

        break;

    case PsDataSchedulingTime:
        CpuTime = Information->KernelTime + Information->UserTime;
        CurrentTime = time(NULL);
        ElapsedTime = CurrentTime - Information->StartTime;
        if (ElapsedTime != 0) {
            IntegerData = CpuTime / ElapsedTime;
        }

        break;

    case PsDataPriority:
        IntegerData = Information->Priority;
        break;

    case PsDataNiceValue:
        IntegerData = Information->NiceValue;
        break;

    case PsDataUserIdentifier:
        IntegerData = Information->EffectiveUserId;
        break;

    case PsDataUserIdentifierText:
    case PsDataEffectiveUserIdentifier:
        Result = SwGetUserNameFromId(Information->EffectiveUserId, &StringData);
        if (Result != 0) {

            assert(StringData == NULL);

            DataAvailable = FALSE;
            break;
        }

        AllocatedString = TRUE;
        break;

    case PsDataRealUserIdentifier:
        Result = SwGetUserNameFromId(Information->RealUserId, &StringData);
        if (Result != 0) {

            assert(StringData == NULL);

            DataAvailable = FALSE;
            break;
        }

        AllocatedString = TRUE;
        break;

    case PsDataRealGroupIdentifier:
        Result = SwGetGroupNameFromId(Information->RealGroupId, &StringData);
        if (Result != 0) {

            assert(StringData == NULL);

            DataAvailable = FALSE;
            break;
        }

        AllocatedString = TRUE;
        break;

    case PsDataEffectiveGroupIdentifier:
        Result = SwGetGroupNameFromId(Information->EffectiveGroupId,
                                      &StringData);

        if (Result != 0) {

            assert(StringData == NULL);

            DataAvailable = FALSE;
            break;
        }

        AllocatedString = TRUE;
        break;

    case PsDataTerminal:
        Result = SwGetTerminalNameFromId(Information->TerminalId, &StringData);
        if (Result != 0) {
            DataAvailable = FALSE;
            break;
        }

        AllocatedString = TRUE;
        break;

    case PsDataProcessIdentifier:
        IntegerData = Information->ProcessId;
        break;

    case PsDataParentProcessIdentifier:
        IntegerData = Information->ParentProcessId;
        break;

    case PsDataProcessGroupIdentifier:
        IntegerData = Information->ProcessGroupId;
        break;

    case PsDataStartTime:
        TimeData = Information->StartTime;
        Result = SwBreakDownTime(1, &TimeData, &DateData);
        if (Result != 0) {
            DataAvailable = FALSE;
            break;
        }

        CurrentTime = time(NULL);
        Result = SwBreakDownTime(1, &CurrentTime, &CurrentDate);
        if (Result != 0) {
            DataAvailable = FALSE;
            break;
        }

        break;

    case PsDataElapsedTime:
        CurrentTime = time(NULL);
        TimeData = CurrentTime - Information->StartTime;
        Days = TimeData / PS_SECONDS_PER_DAY;
        TimeData -= Days * PS_SECONDS_PER_DAY;
        Hours = TimeData / PS_SECONDS_PER_HOUR;
        TimeData -= Hours * PS_SECONDS_PER_HOUR;
        Minutes = TimeData / PS_SECONDS_PER_MINUTE;
        Seconds = TimeData - (Minutes * PS_SECONDS_PER_MINUTE);

        assert(Seconds < PS_SECONDS_PER_MINUTE);

        if (Days != 0) {
            StringDataSize = strlen(PS_ELAPSED_TIME_DAYS_FORMAT) + 1;
            StringData = malloc(StringDataSize);

        } else if (Hours != 0) {
            StringDataSize = strlen(PS_ELAPSED_TIME_HOURS_FORMAT) + 1;
            StringData = malloc(StringDataSize);

        } else {
            StringDataSize = strlen(PS_ELAPSED_TIME_DEFAULT_FORMAT) + 1;
            StringData = malloc(StringDataSize);
        }

        if (StringData == NULL) {
            DataAvailable = FALSE;
            break;
        }

        AllocatedString = TRUE;
        if (Days != 0) {
            Result = snprintf(StringData,
                              StringDataSize,
                              PS_ELAPSED_TIME_DAYS_FORMAT,
                              Days,
                              Hours,
                              Minutes,
                              Seconds);

        } else if (Hours != 0) {
            Result = snprintf(StringData,
                              StringDataSize,
                              PS_ELAPSED_TIME_HOURS_FORMAT,
                              Hours,
                              Minutes,
                              Seconds);

        } else {
            Result = snprintf(StringData,
                              StringDataSize,
                              PS_ELAPSED_TIME_DEFAULT_FORMAT,
                              Minutes,
                              Seconds);
        }

        if (Result < 0) {
            free(StringData);
            DataAvailable = FALSE;
            AllocatedString = FALSE;
        }

        break;

    case PsDataCpuTime:
        TimeData = Information->KernelTime + Information->UserTime;
        Days = TimeData / PS_SECONDS_PER_DAY;
        TimeData -= Days * PS_SECONDS_PER_DAY;
        Hours = TimeData / PS_SECONDS_PER_HOUR;
        TimeData -= Hours * PS_SECONDS_PER_HOUR;
        Minutes = TimeData / PS_SECONDS_PER_MINUTE;
        Seconds = TimeData - (Minutes * PS_SECONDS_PER_MINUTE);

        assert(Seconds < PS_SECONDS_PER_MINUTE);

        if (Days != 0) {
            StringDataSize = strlen(PS_CPU_TIME_DAYS_FORMAT) + 1;
            StringData = malloc(StringDataSize);

        } else {
            StringDataSize = strlen(PS_CPU_TIME_DEFAULT_FORMAT) + 1;
            StringData = malloc(StringDataSize);
        }

        if (StringData == NULL) {
            DataAvailable = FALSE;
            break;
        }

        AllocatedString = TRUE;
        if (Days != 0) {
            Result = snprintf(StringData,
                              StringDataSize,
                              PS_CPU_TIME_DAYS_FORMAT,
                              Days,
                              Hours,
                              Minutes,
                              Seconds);

        } else {
            Result = snprintf(StringData,
                              StringDataSize,
                              PS_CPU_TIME_DEFAULT_FORMAT,
                              Hours,
                              Minutes,
                              Seconds);
        }

        if (Result < 0) {
            free(StringData);
            DataAvailable = FALSE;
            AllocatedString = FALSE;
        }

        break;

    case PsDataCmdName:
    case PsDataCommandName:
        if (Information->NameLength == 0) {
            DataAvailable = FALSE;
            break;
        }

        StringData = Information->Name;
        break;

    case PsDataCmdArguments:
    case PsDataCommandArguments:
        if (Information->ArgumentsSize != 0) {
            StringData = malloc(Column->Width * sizeof(char));
            if (StringData == NULL) {
                DataAvailable = FALSE;
                break;
            }

            AllocatedString = TRUE;
            Arguments = Information->Arguments;
            LengthRemaining = Information->ArgumentsSize;
            WidthRemaining = Column->Width;
            CharactersPrinted = 0;
            while ((LengthRemaining != 0) && (WidthRemaining != 0)) {
                BytesConverted = snprintf(&(StringData[CharactersPrinted]),
                                          WidthRemaining,
                                          "%s ",
                                          Arguments);

                if (BytesConverted < 0) {
                    break;
                }

                CharactersPrinted += BytesConverted;
                if (BytesConverted > LengthRemaining) {
                    LengthRemaining = 0;

                } else {
                    LengthRemaining -= BytesConverted;
                }

                if (BytesConverted > WidthRemaining) {
                    WidthRemaining = 0;

                } else {
                    WidthRemaining -= BytesConverted;
                }

                Arguments += BytesConverted;
            }

        //
        // Add square brackets to the name to signify that the arguments are
        // not available.
        //

        } else if (Information->NameLength != 0) {
            StringDataSize = Information->NameLength + 2;
            StringData = malloc(StringDataSize);
            if (StringData == NULL) {
                DataAvailable = FALSE;
                break;
            }

            AllocatedString = TRUE;
            Result = snprintf(StringData,
                              StringDataSize,
                              "[%s]",
                              Information->Name);

            if (Result < 0) {
                free(StringData);
                DataAvailable = FALSE;
                AllocatedString = FALSE;
            }

        } else {
            DataAvailable = FALSE;
        }

        break;

    default:

        assert(FALSE);

        break;
    }

    //
    // If no data is available, then just print the missing data character.
    //

    if (DataAvailable == FALSE) {
        if (Column->RightJustified != FALSE) {
            printf("%*c", Column->Width, PS_MISSING_DATA_CHARACTER);

        } else {
            printf("%-*c", Column->Width, PS_MISSING_DATA_CHARACTER);
        }

        return;
    }

    //
    // Print the data based on the type.
    //

    switch (Type) {
    case PsDataFlags:
    case PsDataUserIdentifier:
    case PsDataProcessIdentifier:
    case PsDataParentProcessIdentifier:
    case PsDataProcessGroupIdentifier:
    case PsDataSchedulingTime:
    case PsDataPriority:
    case PsDataNiceValue:
        if (Column->RightJustified != FALSE) {
            printf("%*d", Column->Width, IntegerData);

        } else {
            printf("%-*d", Column->Width, IntegerData);
        }

        break;

    case PsDataAddress:
    case PsDataBlockSize:
    case PsDataVirtualSize:
        if (Column->RightJustified != FALSE) {
            printf("%*lu", Column->Width, (unsigned long)SizeData);

        } else {
            printf("%-*lu", Column->Width, (unsigned long)SizeData);
        }

        break;

    case PsDataState:
    case PsDataUserIdentifierText:
    case PsDataRealUserIdentifier:
    case PsDataEffectiveUserIdentifier:
    case PsDataRealGroupIdentifier:
    case PsDataEffectiveGroupIdentifier:
    case PsDataWaitEvent:
    case PsDataTerminal:
    case PsDataCmdName:
    case PsDataCmdArguments:
    case PsDataCommandName:
    case PsDataCommandArguments:
    case PsDataElapsedTime:
    case PsDataCpuTime:

        assert(StringData != NULL);

        if (Column->RightJustified != FALSE) {
            printf("%*s", Column->Width, StringData);

        } else {
            printf("%-*s", Column->Width, StringData);
        }

        break;

    case PsDataStartTime:

        assert(Column->RightJustified != FALSE);

        //
        // If the process started today, print the current hour and minute at
        // which it started.
        //

        if ((DateData.tm_year == CurrentDate.tm_year) &&
            (DateData.tm_yday == CurrentDate.tm_yday)) {

            printf("%02d:%02d", DateData.tm_hour, DateData.tm_min);

        //
        // If it started this year, print the month and day it started.
        //

        } else if (DateData.tm_year == CurrentDate.tm_year) {
            printf("%s%02d", PsMonths[DateData.tm_mon], DateData.tm_mday);

        //
        // Otherwise display a full date.
        //

        } else {
            printf("%s %02d, %04d",
                   PsMonths[DateData.tm_mon],
                   DateData.tm_mday,
                   DateData.tm_year + 1900);
        }

        break;

    case PsDataCpuPercentage:
        if (Column->RightJustified != FALSE) {
            printf("%*.2f", Column->Width, FloatData);

        } else {
            printf("%-*.2f", Column->Width, FloatData);
        }

        break;

    default:

        assert(FALSE);

        break;
    }

    if (AllocatedString != FALSE) {
        free(StringData);
    }

    return;
}

VOID
PspFilterProcessInformationList (
    PPS_CONTEXT Context,
    ULONG Options,
    PSWISS_PROCESS_INFORMATION *ProcessInformationList,
    UINTN ProcessCount
    )

/*++

Routine Description:

    This routine filters the given process list based on filter options.

Arguments:

    Context - Supplies a pointer to the application context.

    Options - Supplies a pointer to the filter options.

    ProcessInformationList - Supplies an array of pointers to process
        information structures.

    ProcessCount - Supplies the number of processes in the list.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    INT EffectiveUserId;
    PPS_FILTER_ENTRY FilterEntry;
    PSTR GroupName;
    BOOL IncludeProcess;
    UINTN Index;
    PSWISS_PROCESS_INFORMATION Information;
    UINTN ProcessIdIndex;
    INT Result;
    pid_t SessionId;
    PSTR SessionName;
    INT TerminalId;
    PSTR TerminalName;
    PSTR UserName;

    //
    // If the filter option to use all processes is set, it trumps the rest.
    // Just exit.
    //

    if ((Options & PS_OPTION_REPORT_ALL_PROCESSES) != 0) {
        return;
    }

    TerminalId = SwGetTerminalId();
    EffectiveUserId = SwGetEffectiveUserId();

    //
    // Iterate over each process in the list of process information.
    //

    for (Index = 0; Index < ProcessCount; Index += 1) {
        IncludeProcess = FALSE;
        Information = ProcessInformationList[Index];
        if (Information == NULL) {
            continue;
        }

        //
        // If there are no filter options, then the default behavior is in
        // effect. Gather the processes with the same effective user ID as the
        // current user and the same controlling terminal as the invoker.
        //

        if ((Options & PS_OPTION_FILTER_MASK) == 0) {
            if ((Information->EffectiveUserId == EffectiveUserId) &&
                (Information->TerminalId == TerminalId)) {

                IncludeProcess = TRUE;
            }

        //
        // Otherwise go through all filter options, gathering the inclusive OR
        // of all processes that get caught by the filters.
        //

        } else {
            if ((Options & PS_OPTION_REPORT_ALL_TERMINAL_PROCESSES) != 0) {
                if (Information->TerminalId != INVALID_TERMINAL_ID) {
                    IncludeProcess = TRUE;
                }
            }

            if ((IncludeProcess == FALSE) &&
                ((Options & PS_OPTION_REPORT_ALL_PROCESSES_NO_LEADERS) != 0)) {

                SessionId = SwGetSessionId(Information->ProcessId);
                if (Information->ProcessId != SessionId) {
                    IncludeProcess = TRUE;
                }
            }

            if ((IncludeProcess == FALSE) &&
                ((Options & PS_OPTION_SESSION_LEADERS_LIST) != 0)) {

                assert(LIST_EMPTY(&(Context->SessionLeaderList)) == FALSE);

                SessionName = NULL;
                SessionId = SwGetSessionId(Information->ProcessId);
                CurrentEntry = Context->SessionLeaderList.Next;
                while (CurrentEntry != &(Context->SessionLeaderList)) {
                    FilterEntry = LIST_VALUE(CurrentEntry,
                                             PS_FILTER_ENTRY,
                                             ListEntry);

                    CurrentEntry = CurrentEntry->Next;
                    if (FilterEntry->TextId != NULL) {
                        if (SessionName == NULL) {
                            Result = SwGetSessionNameFromId(SessionId,
                                                            &SessionName);

                            if (Result != 0) {
                                continue;
                            }
                        }

                        if (strcmp(FilterEntry->TextId, SessionName) == 0) {
                            IncludeProcess = TRUE;
                            break;
                        }

                        continue;
                    }

                    if (FilterEntry->NumericId == SessionId) {
                        IncludeProcess = TRUE;
                        break;
                    }
                }

                if (SessionName != NULL) {
                    free(SessionName);
                    SessionName = NULL;
                }
            }

            if ((IncludeProcess == FALSE) &&
                ((Options & PS_OPTION_REAL_GROUP_ID_LIST) != 0)) {

                assert(LIST_EMPTY(&(Context->RealGroupIdList)) == FALSE);

                GroupName = NULL;
                CurrentEntry = Context->RealGroupIdList.Next;
                while (CurrentEntry != &(Context->RealGroupIdList)) {
                    FilterEntry = LIST_VALUE(CurrentEntry,
                                             PS_FILTER_ENTRY,
                                             ListEntry);

                    CurrentEntry = CurrentEntry->Next;
                    if (FilterEntry->TextId != NULL) {
                        if (GroupName == NULL) {
                            Result = SwGetGroupNameFromId(
                                                      Information->RealGroupId,
                                                      &GroupName);

                            if (Result != 0) {

                                assert(GroupName == NULL);

                                continue;
                            }
                        }

                        if (strcmp(FilterEntry->TextId, GroupName) == 0) {
                            IncludeProcess = TRUE;
                            break;
                        }

                        continue;
                    }

                    if (FilterEntry->NumericId == Information->RealGroupId) {
                        IncludeProcess = TRUE;
                        break;
                    }
                }

                if (GroupName != NULL) {
                    free(GroupName);
                    GroupName = NULL;
                }
            }

            if ((IncludeProcess == FALSE) &&
                ((Options & PS_OPTION_PROCESS_ID_LIST) != 0)) {

                assert(Context->ProcessIdList != NULL);

                for (ProcessIdIndex = 0;
                     ProcessIdIndex < Context->ProcessIdCount;
                     ProcessIdIndex += 1) {

                    if (Context->ProcessIdList[ProcessIdIndex] ==
                        Information->ProcessId) {

                        IncludeProcess = TRUE;
                        break;
                    }
                }
            }

            if ((IncludeProcess == FALSE) &&
                ((Options & PS_OPTION_TERMINAL_LIST) != 0)) {

                assert(LIST_EMPTY(&(Context->TerminalList)) == FALSE);

                TerminalName = NULL;
                CurrentEntry = Context->TerminalList.Next;
                while (CurrentEntry != &(Context->TerminalList)) {
                    FilterEntry = LIST_VALUE(CurrentEntry,
                                             PS_FILTER_ENTRY,
                                             ListEntry);

                    CurrentEntry = CurrentEntry->Next;
                    if (FilterEntry->TextId != NULL) {
                        if (TerminalName == NULL) {
                            Result = SwGetTerminalNameFromId(
                                                       Information->TerminalId,
                                                       &TerminalName);

                            if (Result != 0) {
                                continue;
                            }
                        }

                        if (strcmp(FilterEntry->TextId, TerminalName) == 0) {
                            IncludeProcess = TRUE;
                            break;
                        }

                        continue;
                    }

                    if (FilterEntry->NumericId == Information->TerminalId) {
                        IncludeProcess = TRUE;
                        break;
                    }
                }

                if (TerminalName != NULL) {
                    free(TerminalName);
                    TerminalName = NULL;
                }
            }

            if ((IncludeProcess == FALSE) &&
                ((Options & PS_OPTION_USER_LIST) != 0)) {

                assert(LIST_EMPTY(&(Context->UserIdList)) == FALSE);

                UserName = NULL;
                CurrentEntry = Context->UserIdList.Next;
                while (CurrentEntry != &(Context->UserIdList)) {
                    FilterEntry = LIST_VALUE(CurrentEntry,
                                             PS_FILTER_ENTRY,
                                             ListEntry);

                    CurrentEntry = CurrentEntry->Next;
                    if (FilterEntry->TextId != NULL) {
                        if (UserName == NULL) {
                            Result = SwGetUserNameFromId(
                                                  Information->EffectiveUserId,
                                                  &UserName);

                            if (Result != 0) {

                                assert(UserName == NULL);

                                continue;
                            }
                        }

                        if (strcmp(FilterEntry->TextId, UserName) == 0) {
                            IncludeProcess = TRUE;
                            break;
                        }

                        continue;
                    }

                    if (FilterEntry->NumericId ==
                        Information->EffectiveUserId) {

                        IncludeProcess = TRUE;
                        break;
                    }
                }

                if (UserName != NULL) {
                    free(UserName);
                    UserName = NULL;
                }
            }

            if ((IncludeProcess == FALSE) &&
                ((Options & PS_OPTION_REAL_USER_LIST) != 0)) {

                assert(LIST_EMPTY(&(Context->RealUserIdList)) == FALSE);

                UserName = NULL;
                CurrentEntry = Context->RealUserIdList.Next;
                while (CurrentEntry != &(Context->RealUserIdList)) {
                    FilterEntry = LIST_VALUE(CurrentEntry,
                                             PS_FILTER_ENTRY,
                                             ListEntry);

                    CurrentEntry = CurrentEntry->Next;
                    if (FilterEntry->TextId != NULL) {
                        if (UserName == NULL) {
                            Result = SwGetUserNameFromId(
                                                       Information->RealUserId,
                                                       &UserName);

                            if (Result != 0) {

                                assert(UserName == NULL);

                                continue;
                            }
                        }

                        if (strcmp(FilterEntry->TextId, UserName) == 0) {
                            IncludeProcess = TRUE;
                            break;
                        }

                        continue;
                    }

                    if (FilterEntry->NumericId == Information->RealUserId) {
                        IncludeProcess = TRUE;
                        break;
                    }
                }

                if (UserName != NULL) {
                    free(UserName);
                    UserName = NULL;
                }
            }
        }

        //
        // If none of the options wanted to include the process, then it's
        // lights out for this one.
        //

        if (IncludeProcess == FALSE) {
            SwDestroyProcessInformation(ProcessInformationList[Index]);
            ProcessInformationList[Index] = NULL;
        }
    }

    return;
}

INT
PspParseFormatList (
    PPS_CONTEXT Context,
    PSTR String
    )

/*++

Routine Description:

    This routine reads in a string, splits it on commas, and creates custom
    format entries for it. This routine is destructive on the string arguments.

Arguments:

    Context - Supplies a pointer to the application context.

    String - Supplies the string to split.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    size_t AllocationSize;
    PPS_CUSTOM_FORMAT_ENTRY Column;
    PPS_COLUMN ColumnInformation;
    PSTR CurrentFormat;
    PPS_CUSTOM_FORMAT_MAP_ENTRY FormatMap;
    PSTR HeaderOverride;
    BOOL Match;
    PSTR NextFormat;
    size_t OverrideLength;
    INT Status;

    Column = NULL;
    Context->DisplayHeaderLine = FALSE;
    OverrideLength = 0;

    //
    // Loop splitting on commas.
    //

    CurrentFormat = String;
    while (TRUE) {
        NextFormat = CurrentFormat;
        while ((*NextFormat != ',') &&
               (*NextFormat != ' ') &&
               (*NextFormat != '\0')) {

            NextFormat += 1;
        }

        //
        // Terminate the current string at this filter.
        //

        if (*NextFormat != '\0') {
            NextFormat[0] = '\0';
            NextFormat += 1;

            //
            // End immediately if the next character is a space, comma, or the
            // null terminator.
            //

            if ((*NextFormat == ',') ||
                (*NextFormat == ' ') ||
                (*NextFormat == '\0')) {

                SwPrintError(0, NULL, "Invalid format list");
                Status = EINVAL;
                goto ParseFormatListEnd;
            }

        //
        // Or prepare to terminate the loop if this is the end of the argument.
        //

        } else {
            NextFormat = NULL;
        }

        HeaderOverride = strchr(CurrentFormat, '=');
        if (HeaderOverride != NULL) {

            assert((NextFormat == NULL) || (NextFormat > HeaderOverride));

            HeaderOverride[0] = '\0';
            HeaderOverride += 1;
            if (NextFormat != NULL) {
                OverrideLength = (UINTN)NextFormat - (UINTN)HeaderOverride;

            } else {
                OverrideLength = strlen(HeaderOverride);
            }
        }

        //
        // Search for the column entry that matches the supplied column.
        //

        Match = FALSE;
        FormatMap = &(PsCustomFormatMap[0]);
        while (FormatMap->Format != NULL) {
            if (strcmp(FormatMap->Format, CurrentFormat) == 0) {
                Match = TRUE;
                break;
            }

            FormatMap += 1;
        }

        if (Match == FALSE) {
            SwPrintError(EINVAL, NULL, "Unknown format '%s'", CurrentFormat);
            Status = EINVAL;
            goto ParseFormatListEnd;
        }

        //
        // Allocate the column list entry and insert it into the list. Make a
        // copy of the header override if it exists.
        //

        AllocationSize = sizeof(PS_CUSTOM_FORMAT_ENTRY);
        if (HeaderOverride != NULL) {
            AllocationSize += OverrideLength + 1;
        }

        Column = malloc(AllocationSize);
        if (Column == NULL) {
            Status = ENOMEM;
            goto ParseFormatListEnd;
        }

        memset(Column, 0, sizeof(PS_CUSTOM_FORMAT_ENTRY));
        Column->Type = FormatMap->Type;
        if (HeaderOverride != NULL) {
            Column->HeaderOverrideValid = TRUE;
            Column->HeaderOverride = (PSTR)(Column + 1);
            strncpy(Column->HeaderOverride,
                    HeaderOverride,
                    OverrideLength + 1);

            assert(Column->HeaderOverride[OverrideLength] == '\0');

            if (OverrideLength != 0) {
                Context->DisplayHeaderLine = TRUE;
            }

            ColumnInformation = &(PsColumnInformation[Column->Type]);
            if (ColumnInformation->Width < OverrideLength) {
                Column->HeaderOverrideWidth = OverrideLength;

            } else {
                Column->HeaderOverrideWidth = ColumnInformation->Width;
            }

        } else {
            Context->DisplayHeaderLine = TRUE;
        }

        INSERT_BEFORE(&(Column->ListEntry), &(Context->CustomFormatList));
        Column = NULL;
        if (NextFormat == NULL) {
            break;
        }

        CurrentFormat = NextFormat;
    }

    Status = 0;

ParseFormatListEnd:
    if (Column != NULL) {
        free(Column);
    }

    return Status;
}

INT
PspParseFilterList (
    PLIST_ENTRY ListHead,
    PSTR String
    )

/*++

Routine Description:

    This routine reads in a string, splits it on commas, and creates a filter
    entry for each element. This routine is destructive on the string arguments.

Arguments:

    ListHead - Supplies a pointer to the head of the filter list.

    String - Supplies the string to split.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    size_t AllocationSize;
    PSTR CurrentFilter;
    PPS_FILTER_ENTRY FilterEntry;
    PSTR NextFilter;
    LONG NumericId;
    INT Status;

    //
    // Iterate over the string to find the filter entries.
    //

    CurrentFilter = String;
    while (TRUE) {
        NextFilter = CurrentFilter;
        while ((*NextFilter != ',') &&
               (*NextFilter != ' ') &&
               (*NextFilter != '\0')) {

            NextFilter += 1;
        }

        //
        // Terminate the current string at this filter.
        //

        if (*NextFilter != '\0') {
            NextFilter[0] = '\0';
            NextFilter += 1;

            //
            // End immediately if the next character is a space, comma, or the
            // null terminator.
            //

            if ((*NextFilter == ',') ||
                (*NextFilter == ' ') ||
                (*NextFilter == '\0')) {

                SwPrintError(0, NULL, "Invalid filter list");
                Status = EINVAL;
                goto ParseFilterListEnd;
            }

        //
        // Or prepare to terminate the loop if this is the end of the argument.
        //

        } else {
            NextFilter = NULL;
        }

        //
        // Attempt to convert the string value to a numeric filter ID.
        //

        NumericId = strtol(CurrentFilter, &AfterScan, 10);
        if (NumericId < 0) {
            SwPrintError(EINVAL,
                         NULL,
                         "Invalid process filter '%s'",
                         CurrentFilter);

            Status = EINVAL;
            goto ParseFilterListEnd;
        }

        //
        // If it wasn't a numer, assume it was a string and get on with
        // allocating a filter entry.
        //

        AllocationSize = sizeof(PS_FILTER_ENTRY);
        if ((AfterScan == CurrentFilter) || (*AfterScan != '\0')) {
            AllocationSize += strlen(CurrentFilter) + 1;
        }

        FilterEntry = malloc(AllocationSize);
        if (FilterEntry == NULL) {
            Status = ENOMEM;
            goto ParseFilterListEnd;
        }

        memset(FilterEntry, 0, AllocationSize);
        if ((AfterScan == CurrentFilter) || (*AfterScan != '\0')) {
            FilterEntry->TextId = (PSTR)(FilterEntry + 1);
            strcpy(FilterEntry->TextId, CurrentFilter);

        } else {
            FilterEntry->NumericId = NumericId;
        }

        INSERT_BEFORE(&(FilterEntry->ListEntry), ListHead);
        FilterEntry = NULL;
        if (NextFilter == NULL) {
            break;
        }

        CurrentFilter = NextFilter;
    }

    Status = 0;

ParseFilterListEnd:
    return Status;
}

INT
PspParseProcessList (
    PPS_CONTEXT Context,
    PSTR String
    )

/*++

Routine Description:

    This routine reads in a string, splits it on commas, and creates a process
    ID array based on it. This routine is destructive on the string arguments.

Arguments:

    Context - Supplies a pointer to the application context.

    String - Supplies the string to split.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    UINTN AllocationSize;
    UINTN Index;
    PSTR NextProcessIdString;
    pid_t ProcessId;
    UINTN ProcessIdCount;
    pid_t *ProcessIdList;
    PSTR ProcessIdString;
    INT Status;

    //
    // Run through the string once to determine how many process IDs are there.
    //

    ProcessIdString = String;
    ProcessIdCount = 0;
    do {
        NextProcessIdString = ProcessIdString;
        while ((*NextProcessIdString != ',') &&
               (*NextProcessIdString != ' ') &&
               (*NextProcessIdString != '\0')) {

            NextProcessIdString += 1;
        }

        //
        // If the null terminator was reached, prepare to exit the loop.
        //

        if (*NextProcessIdString == '\0') {
            ProcessIdString = NULL;

        //
        // Otherwise check to make sure the list is valid.
        //

        } else {
            NextProcessIdString += 1;

            //
            // End immediately if the next character is a space, comma, or the
            // null terminator.
            //

            if ((*NextProcessIdString == ',') ||
                (*NextProcessIdString == ' ') ||
                (*NextProcessIdString == '\0')) {

                SwPrintError(0, NULL, "Invalid process ID list");
                Status = EINVAL;
                goto ParseProcessListEnd;
            }

            ProcessIdString = NextProcessIdString;
        }

        ProcessIdCount += 1;

    } while (ProcessIdString != NULL);

    //
    // Allocate an array to store all the process IDs. Add it to the existing
    // list if there is one.
    //

    AllocationSize = ProcessIdCount * sizeof(pid_t);
    AllocationSize += Context->ProcessIdCount * sizeof(pid_t);
    ProcessIdList = realloc(Context->ProcessIdList, AllocationSize);
    if (ProcessIdList == NULL) {
        Status = ENOMEM;
        goto ParseProcessListEnd;
    }

    Context->ProcessIdList = ProcessIdList;

    //
    // Loop over the input again, converting the strings to integers.
    //

    Index = Context->ProcessIdCount;
    ProcessIdString = String;
    while (TRUE) {
        NextProcessIdString = ProcessIdString;
        while ((*NextProcessIdString != ',') &&
               (*NextProcessIdString != ' ') &&
               (*NextProcessIdString != '\0')) {

            NextProcessIdString += 1;
        }

        //
        // Terminate the current string at this filter.
        //

        if (*NextProcessIdString != '\0') {
            NextProcessIdString[0] = '\0';
            NextProcessIdString += 1;

            assert((*NextProcessIdString != ',') &&
                   (*NextProcessIdString != ' ') &&
                   (*NextProcessIdString != '\0'));

        //
        // Or prepare to terminate the loop if this is the end of the argument.
        //

        } else {
            NextProcessIdString = NULL;
        }

        ProcessId = strtol(ProcessIdString, &AfterScan, 10);
        if ((AfterScan == ProcessIdString) || (*AfterScan != '\0')) {
            SwPrintError(0,
                         NULL,
                         "Invalid process ID '%s'",
                         ProcessIdString);

            Status = EINVAL;
            goto ParseProcessListEnd;

        } else if (ProcessId < 0) {
            SwPrintError(ERANGE, NULL, "Process %d not in range", ProcessId);
            Status = ERANGE;
            goto ParseProcessListEnd;
        }

        Context->ProcessIdList[Index] = ProcessId;
        Index += 1;
        if (NextProcessIdString == NULL) {
            break;
        }

        ProcessIdString = NextProcessIdString;
    }

    assert((Context->ProcessIdCount + ProcessIdCount) == Index);

    Context->ProcessIdCount = Index;
    Status = 0;

ParseProcessListEnd:
    return Status;
}

PSWISS_PROCESS_INFORMATION *
PspGetProcessInformationList (
    pid_t *ProcessIdList,
    UINTN ProcessCount
    )

/*++

Routine Description:

    This routine creates and fills in an array of pointers to process
    information structures. The system will be queried for the process
    information of each of the processes supplied in the process ID list.

Arguments:

    ProcessIdList - Supplies an array of process IDs.

    ProcessCount - Supplies the length of the array.

Return Value:

    None.

--*/

{

    size_t AllocationSize;
    UINTN Index;
    PSWISS_PROCESS_INFORMATION *ProcessInformationList;
    INT Status;

    //
    // Create an array to store pointers to each process's data.
    //

    AllocationSize = sizeof(PSWISS_PROCESS_INFORMATION) * ProcessCount;
    ProcessInformationList = malloc(AllocationSize);
    if (ProcessInformationList == NULL) {
        Status = -1;
        SwPrintError(ENOMEM, NULL, "Failed to get process status");
        goto GetProcessInformationListEnd;
    }

    memset(ProcessInformationList, 0, AllocationSize);

    //
    // For the remaining process IDs, collect the information needed to display
    // to standard out.
    //

    for (Index = 0; Index < ProcessCount; Index += 1) {
        Status = SwGetProcessInformation(ProcessIdList[Index],
                                         &(ProcessInformationList[Index]));

        if (Status != 0) {
            continue;
        }
    }

    Status = 0;

GetProcessInformationListEnd:
    if (Status != 0) {
        if (ProcessInformationList != NULL) {
            PspDestroyProcessInformationList(ProcessInformationList,
                                             ProcessCount);

            ProcessInformationList = NULL;
        }
    }

    return ProcessInformationList;
}

VOID
PspDestroyProcessInformationList (
    PSWISS_PROCESS_INFORMATION *ProcessInformationList,
    UINTN ProcessCount
    )

/*++

Routine Description:

    This routine destroys a list of process information.

Arguments:

    ProcessInformationList - Supplies an array of pointers to process
        information structures.

    ProcessCount - Supplies the length of the array.

Return Value:

    None.

--*/

{

    UINTN Index;

    for (Index = 0; Index < ProcessCount; Index += 1) {
        if (ProcessInformationList[Index] != NULL) {
            SwDestroyProcessInformation(ProcessInformationList[Index]);
        }
    }

    free(ProcessInformationList);
    return;
}

VOID
PspRemoveDuplicateProcessIds (
    pid_t *ProcessIdList,
    PUINTN ProcessIdCount
    )

/*++

Routine Description:

    This routine removes the duplicates from a list of process IDs. As a side
    effect, the list is also sorted.

Arguments:

    ProcessIdList - Supplies an array of product ID from which duplicates will
        be removed.

    ProcessIdCount - Supplies a pointer that on input contains the number of
        process IDs in the list. And on output, receives the updated number of
        process IDs in the list.

Return Value:

    None.

--*/

{

    UINTN Count;
    UINTN Index;

    assert(ProcessIdCount != 0);

    //
    // First sort the list to make removing duplicates easier.
    //

    qsort(ProcessIdList, *ProcessIdCount, sizeof(pid_t), PspCompareProcessIds);

    //
    // Go through list removing the duplicates.
    //

    Count = 1;
    for (Index = 1; Index < *ProcessIdCount; Index += 1) {
        if (ProcessIdList[Count - 1] != ProcessIdList[Index]) {
            ProcessIdList[Count] = ProcessIdList[Index];
            Count += 1;
        }
    }

    *ProcessIdCount = Count;
    return;
}

INT
PspCompareProcessIds (
    const VOID *First,
    const VOID *Second
    )

/*++

Routine Description:

    This routine compares two process IDs.

Arguments:

    First - Supplies a pointer to the first process ID.

    Second - Supplies a pointer to the second process ID.

Return Value:

    1 if First > Second.

    0 if First == Second.

    -1 if First < Second.

--*/

{

    pid_t *FirstProcessId;
    INT Result;
    pid_t *SecondProcessId;

    FirstProcessId = (pid_t *)First;
    SecondProcessId = (pid_t *)Second;
    if (*FirstProcessId > *SecondProcessId) {
        Result = 1;

    } else if (*FirstProcessId == *SecondProcessId) {
        Result = 0;

    } else {
        Result = -1;
    }

    return Result;
}

