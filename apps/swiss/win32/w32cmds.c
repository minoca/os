/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    w32cmds.c

Abstract:

    This module implements the command table for the Swiss utility. These are
    the implemented commands for Windows.

Author:

    Evan Green 10-Mar-2015

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include "../swiss.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

SWISS_COMMAND_ENTRY SwissCommands[] = {
    {SH_COMMAND_NAME, SH_COMMAND_DESCRIPTION, ShMain, 0},
    {CAT_COMMAND_NAME, CAT_COMMAND_DESCRIPTION, CatMain, 0},
    {ECHO_COMMAND_NAME, ECHO_COMMAND_DESCRIPTION, EchoMain, 0},
    {TEST_COMMAND_NAME, TEST_COMMAND_DESCRIPTION, TestMain, 0},
    {TEST_COMMAND_NAME2, TEST_COMMAND_DESCRIPTON2, TestMain, 0},
    {MKDIR_COMMAND_NAME, MKDIR_COMMAND_DESCRIPTION, MkdirMain, 0},
    {LS_COMMAND_NAME, LS_COMMAND_DESCRIPTION, LsMain, 0},
    {RM_COMMAND_NAME, RM_COMMAND_DESCRIPTION, RmMain, 0},
    {RMDIR_COMMAND_NAME, RMDIR_COMMAND_DESCRIPTION, RmdirMain, 0},
    {MV_COMMAND_NAME, MV_COMMAND_DESCRIPTION, MvMain, 0},
    {CP_COMMAND_NAME, CP_COMMAND_DESCRIPTION, CpMain, 0},
    {SED_COMMAND_NAME, SED_COMMAND_DESCRIPTION, SedMain, 0},
    {PRINTF_COMMAND_NAME, PRINTF_COMMAND_DESCRIPTION, PrintfMain, 0},
    {EXPR_COMMAND_NAME, EXPR_COMMAND_DESCRIPTION, ExprMain, 0},
    {CHMOD_COMMAND_NAME, CHMOD_COMMAND_DESCRIPTION, ChmodMain, 0},
    {GREP_COMMAND_NAME, GREP_COMMAND_DESCRIPTION, GrepMain, 0},
    {EGREP_COMMAND_NAME, EGREP_COMMAND_DESCRIPTION, EgrepMain, 0},
    {FGREP_COMMAND_NAME, FGREP_COMMAND_DESCRIPTION, FgrepMain, 0},
    {UNAME_COMMAND_NAME, UNAME_COMMAND_DESCRIPTION, UnameMain, 0},
    {BASENAME_COMMAND_NAME, BASENAME_COMMAND_DESCRIPTION, BasenameMain, 0},
    {DIRNAME_COMMAND_NAME, DIRNAME_COMMAND_DESCRIPTION, DirnameMain, 0},
    {SORT_COMMAND_NAME, SORT_COMMAND_DESCRIPTION, SortMain, 0},
    {TR_COMMAND_NAME, TR_COMMAND_DESCRIPTION, TrMain, 0},
    {TOUCH_COMMAND_NAME, TOUCH_COMMAND_DESCRIPTION, TouchMain, 0},
    {TRUE_COMMAND_NAME, TRUE_COMMAND_DESCRIPTION, TrueMain, 0},
    {FALSE_COMMAND_NAME, FALSE_COMMAND_DESCRIPTION, FalseMain, 0},
    {PWD_COMMAND_NAME, PWD_COMMAND_DESCRIPTION, PwdMain, 0},
    {ENV_COMMAND_NAME, ENV_COMMAND_DESCRIPTION, EnvMain, 0},
    {FIND_COMMAND_NAME, FIND_COMMAND_DESCRIPTION, FindMain, 0},
    {CMP_COMMAND_NAME, CMP_COMMAND_DESCRIPTION, CmpMain, 0},
    {UNIQ_COMMAND_NAME, UNIQ_COMMAND_DESCRIPTION, UniqMain, 0},
    {DIFF_COMMAND_NAME, DIFF_COMMAND_DESCRIPTION, DiffMain, 0},
    {DATE_COMMAND_NAME, DATE_COMMAND_DESCRIPTION, DateMain, 0},
    {OD_COMMAND_NAME, OD_COMMAND_DESCRIPTION, OdMain, 0},
    {SLEEP_COMMAND_NAME, SLEEP_COMMAND_DESCRIPTION, SleepMain, 0},
    {MKTEMP_COMMAND_NAME, MKTEMP_COMMAND_DESCRIPTION, MktempMain, 0},
    {TAIL_COMMAND_NAME, TAIL_COMMAND_DESCRIPTION, TailMain, 0},
    {WC_COMMAND_NAME, WC_COMMAND_DESCRIPTION, WcMain, 0},
    {LN_COMMAND_NAME, LN_COMMAND_DESCRIPTION, LnMain, 0},
    {TIME_COMMAND_NAME, TIME_COMMAND_DESCRIPTION, TimeMain, 0},
    {CECHO_COMMAND_NAME, CECHO_COMMAND_DESCRIPTION, ColorEchoMain, 0},
    {CUT_COMMAND_NAME, CUT_COMMAND_DESCRIPTION, CutMain, 0},
    {SPLIT_COMMAND_NAME, SPLIT_COMMAND_DESCRIPTION, SplitMain, 0},
    {PS_COMMAND_NAME, PS_COMMAND_DESCRIPTION, PsMain, 0},
    {KILL_COMMAND_NAME, KILL_COMMAND_DESCRIPTION, KillMain, 0},
    {COMM_COMMAND_NAME, COMM_COMMAND_DESCRIPTION, CommMain, 0},
    {REBOOT_COMMAND_NAME, REBOOT_COMMAND_DESCRIPTION, RebootMain, 0},
    {ID_COMMAND_NAME, ID_COMMAND_DESCRIPTION, IdMain, 0},
    {NL_COMMAND_NAME, NL_COMMAND_DESCRIPTION, NlMain, 0},
    {TEE_COMMAND_NAME, TEE_COMMAND_DESCRIPTION, TeeMain, 0},
    {INSTALL_COMMAND_NAME, INSTALL_COMMAND_DESCRIPTION, InstallMain, 0},
    {XARGS_COMMAND_NAME, XARGS_COMMAND_DESCRIPTION, XargsMain, 0},
    {SUM_COMMAND_NAME, SUM_COMMAND_DESCRIPTION, SumMain, 0},
    {HEAD_COMMAND_NAME, HEAD_COMMAND_DESCRIPTION, HeadMain, 0},
    {DD_COMMAND_NAME, DD_COMMAND_DESCRIPTION, DdMain, 0},
    {DW_COMMAND_NAME, DW_COMMAND_DESCRIPTION, DwMain, SWISS_APP_HIDDEN},
    {NPROC_COMMAND_NAME, NPROC_COMMAND_DESCRIPTION, NprocMain, 0},
    {SEQ_COMMAND_NAME, SEQ_COMMAND_DESCRIPTION, SeqMain, 0},
    {WHICH_COMMAND_NAME, WHICH_COMMAND_DESCRIPTION, WhichMain, 0},
    {SOKO_COMMAND_NAME, SOKO_COMMAND_DESCRIPTION, SokoMain, SWISS_APP_HIDDEN},
    {WHOAMI_COMMAND_NAME, WHOAMI_COMMAND_DESCRIPTION, WhoamiMain, 0},
    {NULL, NULL, NULL, 0},
};

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

