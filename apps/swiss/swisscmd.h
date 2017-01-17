/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    swisscmd.h

Abstract:

    This header contains definitions for the support Swiss commands.

Author:

    Evan Green 5-Jun-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Command names.
//

#define SH_COMMAND_NAME "sh"
#define SH_COMMAND_DESCRIPTION "Runs a Bourne compatible shell."
#define CAT_COMMAND_NAME "cat"
#define CAT_COMMAND_DESCRIPTION "Print the contents of a file."
#define ECHO_COMMAND_NAME "echo"
#define ECHO_COMMAND_DESCRIPTION "Prints arguments."
#define TEST_COMMAND_NAME "test"
#define TEST_COMMAND_DESCRIPTION \
    "Performs file, string, and integer comparisons."

#define TEST_COMMAND_NAME2 "["
#define TEST_COMMAND_DESCRIPTON2 \
    "Same as test but needs a ] as the last argument."

#define MKDIR_COMMAND_NAME "mkdir"
#define MKDIR_COMMAND_DESCRIPTION "Create a directory."
#define LS_COMMAND_NAME "ls"
#define LS_COMMAND_DESCRIPTION "List the contents of a directory."
#define RM_COMMAND_NAME "rm"
#define RM_COMMAND_DESCRIPTION "Delete a file."
#define RMDIR_COMMAND_NAME "rmdir"
#define RMDIR_COMMAND_DESCRIPTION "Delete an empty directory."
#define MV_COMMAND_NAME "mv"
#define MV_COMMAND_DESCRIPTION "Move files."
#define CP_COMMAND_NAME "cp"
#define CP_COMMAND_DESCRIPTION "Copy files."
#define SED_COMMAND_NAME "sed"
#define SED_COMMAND_DESCRIPTION "Stream editor, edits files with patterns."
#define PRINTF_COMMAND_NAME "printf"
#define PRINTF_COMMAND_DESCRIPTION "Formatted echo."
#define EXPR_COMMAND_NAME "expr"
#define EXPR_COMMAND_DESCRIPTION "Evaluate basic mathematical expressions."
#define CHMOD_COMMAND_NAME "chmod"
#define CHMOD_COMMAND_DESCRIPTION "Change file permissions."
#define GREP_COMMAND_NAME "grep"
#define GREP_COMMAND_DESCRIPTION "Search through files."
#define EGREP_COMMAND_NAME "egrep"
#define EGREP_COMMAND_DESCRIPTION "Same as grep -E."
#define FGREP_COMMAND_NAME "fgrep"
#define FGREP_COMMAND_DESCRIPTION "Same as grep -f."
#define UNAME_COMMAND_NAME "uname"
#define UNAME_COMMAND_DESCRIPTION "Print system identification."
#define BASENAME_COMMAND_NAME "basename"
#define BASENAME_COMMAND_DESCRIPTION "Get the final component of a file path."
#define DIRNAME_COMMAND_NAME "dirname"
#define DIRNAME_COMMAND_DESCRIPTION \
    "Get everything but the final component of a file path."

#define SORT_COMMAND_NAME "sort"
#define SORT_COMMAND_DESCRIPTION "Sort input lines."
#define TR_COMMAND_NAME "tr"
#define TR_COMMAND_DESCRIPTION "Translate characters."
#define TOUCH_COMMAND_NAME "touch"
#define TOUCH_COMMAND_DESCRIPTION "Change file modification times."
#define TRUE_COMMAND_NAME "true"
#define TRUE_COMMAND_DESCRIPTION "Simply return zero."
#define FALSE_COMMAND_NAME "false"
#define FALSE_COMMAND_DESCRIPTION "Simply return non-zero."
#define PWD_COMMAND_NAME "pwd"
#define PWD_COMMAND_DESCRIPTION "Print the current working directory."
#define ENV_COMMAND_NAME "env"
#define ENV_COMMAND_DESCRIPTION \
    "Modify the environment and run another utility."

#define FIND_COMMAND_NAME "find"
#define FIND_COMMAND_DESCRIPTION "Locate files and directories."
#define CMP_COMMAND_NAME "cmp"
#define CMP_COMMAND_DESCRIPTION "Compare files for equality."
#define UNIQ_COMMAND_NAME "uniq"
#define UNIQ_COMMAND_DESCRIPTION "Remove duplicate lines from sorted file."
#define DIFF_COMMAND_NAME "diff"
#define DIFF_COMMAND_DESCRIPTION \
    "Describe differences between files or directories."
#define DATE_COMMAND_NAME "date"
#define DATE_COMMAND_DESCRIPTION \
    "Get or set the current date and time."

#define OD_COMMAND_NAME "od"
#define OD_COMMAND_DESCRIPTION "Octal dump: print file contents unambiguously."
#define SLEEP_COMMAND_NAME "sleep"
#define SLEEP_COMMAND_DESCRIPTION "Suspend execution for a bit."
#define MKTEMP_COMMAND_NAME "mktemp"
#define MKTEMP_COMMAND_DESCRIPTION "Create a temporary file or directory."
#define TAIL_COMMAND_NAME "tail"
#define TAIL_COMMAND_DESCRIPTION "Print the first or last part of a file."
#define WC_COMMAND_NAME "wc"
#define WC_COMMAND_DESCRIPTION "Count words, lines, and characters in a file."
#define LN_COMMAND_NAME "ln"
#define LN_COMMAND_DESCRIPTION "Create links between files."
#define TIME_COMMAND_NAME "time"
#define TIME_COMMAND_DESCRIPTION "Time a program's execution."
#define CECHO_COMMAND_NAME "cecho"
#define CECHO_COMMAND_DESCRIPTION "Print arguments in color."
#define CUT_COMMAND_NAME "cut"
#define CUT_COMMAND_DESCRIPTION \
    "Print a selected portion of each line in a file."

#define SPLIT_COMMAND_NAME "split"
#define SPLIT_COMMAND_DESCRIPTION "Split a file into multiple smaller files."
#define PS_COMMAND_NAME "ps"
#define PS_COMMAND_DESCRIPTION "Print process status."
#define KILL_COMMAND_NAME "kill"
#define KILL_COMMAND_DESCRIPTION "Terminate or signal processes."
#define COMM_COMMAND_NAME "comm"
#define COMM_COMMAND_DESCRIPTION \
    "Print lines common to two sorted files, or unique to each one."

#define REBOOT_COMMAND_NAME "reboot"
#define REBOOT_COMMAND_DESCRIPTION "Reset the system."
#define ID_COMMAND_NAME "id"
#define ID_COMMAND_DESCRIPTION "Print the current user and group IDs."
#define CHROOT_COMMAND_NAME "chroot"
#define CHROOT_COMMAND_DESCRIPTION \
    "Execute a command with a limited file system perspective."

#define CHOWN_COMMAND_NAME "chown"
#define CHOWN_COMMAND_DESCRIPTION \
    "Change the ownership of a file."

#define USERADD_COMMAND_NAME "useradd"
#define USERADD_COMMAND_DESCRIPTION "Add a new user"
#define USERDEL_COMMAND_NAME "userdel"
#define USERDEL_COMMAND_DESCRIPTION "Delete a user"
#define GROUPADD_COMMAND_NAME "groupadd"
#define GROUPADD_COMMAND_DESCRIPTION "Add a new user group"
#define GROUPDEL_COMMAND_NAME "groupdel"
#define GROUPDEL_COMMAND_DESCRIPTION "Delete a user group"
#define PASSWD_COMMAND_NAME "passwd"
#define PASSWD_COMMAND_DESCRIPTION "Change a user's password"
#define CHPASSWD_COMMAND_NAME "chpasswd"
#define CHPASSWD_COMMAND_DESCRIPTION "Change passwords in bulk"
#define VLOCK_COMMAND_NAME "vlock"
#define VLOCK_COMMAND_DESCRIPTION "Locks a console until a password is entered"
#define SU_COMMAND_NAME "su"
#define SU_COMMAND_DESCRIPTION "Run a command as another user"
#define SULOGIN_COMMAND_NAME "sulogin"
#define SULOGIN_COMMAND_DESCRIPTION "Single-user login utility"
#define LOGIN_COMMAND_NAME "login"
#define LOGIN_COMMAND_DESCRIPTION "Start a new user session"
#define GETTY_COMMAND_NAME "getty"
#define GETTY_COMMAND_DESCRIPTION "Set up a terminal and invoke login"
#define INIT_COMMAND_NAME "init"
#define INIT_COMMAND_DESCRIPTION "Initialize the machine"
#define START_STOP_DAEMON_NAME "start-stop-daemon"
#define START_STOP_DAEMON_DESCRIPTION "Start or stop a system process"
#define READLINK_COMMAND_NAME "readlink"
#define READLINK_COMMAND_DESCRIPTION "Read the destination of a symbolic link"
#define NL_COMMAND_NAME "nl"
#define NL_COMMAND_DESCRIPTION "Number lines"
#define TEE_COMMAND_NAME "tee"
#define TEE_COMMAND_DESCRIPTION "Copy stdin to stdout and files"
#define INSTALL_COMMAND_NAME "install"
#define INSTALL_COMMAND_DESCRIPTION "Copy files to their final location"
#define XARGS_COMMAND_NAME "xargs"
#define XARGS_COMMAND_DESCRIPTION \
    "Invoke a command using arguments from standard in"

#define SUM_COMMAND_NAME "sum"
#define SUM_COMMAND_DESCRIPTION "Sum the bytes in a file"
#define HEAD_COMMAND_NAME "head"
#define HEAD_COMMAND_DESCRIPTION "Print the first 10 or so lines of a file"
#define DD_COMMAND_NAME "dd"
#define DD_COMMAND_DESCRIPTION "Copy blocks from source to destination"
#define MKFIFO_COMMAND_NAME "mkfifo"
#define MKFIFO_COMMAND_DESCRIPTION "Create a named pipe"
#define DW_COMMAND_NAME "dw"
#define DW_COMMAND_DESCRIPTION "Passes idle time"
#define TELNETD_COMMAND_NAME "telnetd"
#define TELNETD_COMMAND_DESCRIPTION "Simple telnet server"
#define TELNET_COMMAND_NAME "telnet"
#define TELNET_COMMAND_DESCRIPTION "Telnet client"
#define NPROC_COMMAND_NAME "nproc"
#define NPROC_COMMAND_DESCRIPTION "Returns the number of processors"
#define SEQ_COMMAND_NAME "seq"
#define SEQ_COMMAND_DESCRIPTION "Print a sequence of numbers"
#define STTY_COMMAND_NAME "stty"
#define STTY_COMMAND_DESCRIPTION "Set terminal attributes"
#define WHICH_COMMAND_NAME "which"
#define WHICH_COMMAND_DESCRIPTION "Print the full path of an executable"
#define SOKO_COMMAND_NAME "soko"
#define SOKO_COMMAND_DESCRIPTION "Pack data bins"
#define WHOAMI_COMMAND_NAME "whoami"
#define WHOAMI_COMMAND_DESCRIPTION \
    "Print the user name associated with the current effective user ID"

#define HOSTNAME_COMMAND_NAME "hostname"
#define HOSTNAME_COMMAND_DESCRIPTION \
    "Print or set the machine's network host name"

#define DNSDOMAINNAME_COMMAND_NAME "dnsdomainname"
#define DNSDOMAINNAME_COMMAND_DESCRIPTION \
    "Print the DNS domain name of the machine"

//
// Command entry point prototypes.
//

INT
ShMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the main entry point for the shell app.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
CatMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the cat program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
EchoMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the echo program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 always.

--*/

INT
TestMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the test application entry point.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 or 1 if the evaluation succeeds.

    >1 on failure.

--*/

INT
MkdirMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the mkdir program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
LsMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the main entry point for the ls (list directory)
    utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
RmMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the rm program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
RmdirMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the rmdir program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
MvMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the mv utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
CpMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the cp utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SedMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the main entry point for the sed (stream editor)
    utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
PrintfMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the main entry point for the printf utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
ExprMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the main entry point for the expr utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
ChmodMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the main entry point for the chmod utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
EgrepMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the main entry point for the egrep utility, which
    searches for a pattern within a file. It is equivalent to grep -E.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
FgrepMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the main entry point for the fgrep utility, which
    searches for a pattern within a file. It is equivalent to grep -f.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
GrepMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the main entry point for the grep utility, which
    searches for a pattern within a file.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
UnameMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the uname utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
DirnameMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the dirname utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 on success.

    Returns a positive value if an error occurred.

--*/

INT
BasenameMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the basename utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 on success.

    Returns a positive value if an error occurred.

--*/

INT
SortMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the cp utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
TrMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the tr utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
TouchMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the touch utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
TrueMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the true utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 always.

--*/

INT
FalseMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the false utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    1 always.

--*/

INT
PwdMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the pwd (print working directory)
    utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 on success.

    Non-zero error on failure.

--*/

INT
EnvMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the env utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
FindMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the find utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
CmpMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the cmp (compare) utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
UniqMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the cp utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
DiffMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the diff utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
DateMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the date utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
OdMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the cp utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SleepMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the sleep utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    1 always.

--*/

INT
MktempMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the mktemp utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
TailMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the tail utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
WcMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the wc utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
LnMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the ln (link) utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
TimeMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the time utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
ColorEchoMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the color echo program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
CutMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the cut utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SplitMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the split utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
PsMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the ps utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
KillMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the kill utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, non-zero otherwise.

--*/

INT
CommMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the comm utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
RebootMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the reboot utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
IdMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the id utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
ChrootMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the chroot utility, which changes
    the root directory and runs a command.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
ChownMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the chown utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
UseraddMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the useradd utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
UserdelMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the userdel utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
GroupaddMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the groupadd utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
GroupdelMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the groupdel utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
PasswdMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the passwd utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
ChpasswdMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the chpasswd utility, which
    allows passwords to be changed in bulk.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
VlockMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the vlock utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SuMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the su utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SuloginMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the sulogin (single-user login)
    utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
LoginMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the login utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
GettyMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the getty utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
InitMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the init utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SsDaemonMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the start-stop-daemon utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
ReadlinkMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the readlink utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
NlMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the nl (number lines) utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
TeeMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the tee program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
InstallMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the install utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
XargsMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the xargs utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SumMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the sum utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
HeadMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the head utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
DdMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the head utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
MkfifoMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the mkfifo utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
DwMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the dw utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
TelnetdMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the telnetd daemon.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
TelnetMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the telnet utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
NprocMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the nproc utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SeqMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the seq utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SttyMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the stty utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
WhichMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the which utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SokoMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the soko application.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
WhoamiMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the whoami utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
HostnameMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the hostname utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

