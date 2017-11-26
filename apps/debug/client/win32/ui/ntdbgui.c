/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ntdbgui.c

Abstract:

    This module implements the graphical UI for the debugger running on
    Windows.

Author:

    Evan Green 14-Jul-2012

Environment:

    Debugger (Windows)

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Define WIN32 versions to enable group view for the list view control.
//

#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0600

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shlwapi.h>
#include <fcntl.h>
#include <limits.h>
#include <assert.h>
#include <stdio.h>
#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "dbgrprof.h"
#include "console.h"
#include "resource.h"
#include "missing.h"

//
// ---------------------------------------------------------------- Definitions
//

#define MAX_KEYWORD 30
#define MAX_FILENAME MAX_PATH
#define BACKGROUND_COLOR RGB(39, 40, 34)
#define BREAKPOINT_COLOR RGB(140, 0, 0)
#define EXECUTING_COLOR RGB(9, 2, 134)

#define PLAIN_TEXT_COLOR "\\red248\\green248\\blue242"
#define CONSTANT_COLOR "\\red174\\green129\\blue255"
#define KEYWORD_COLOR "\\red249\\green38\\blue114"
#define COMMENT_COLOR "\\red117\\green113\\blue94"
#define BRACE_COLOR "\\red240\\green240\\blue240"
#define QUOTE_COLOR "\\red230\\green219\\blue90"
#define DISABLED_COLOR "\\red70\\green70\\blue70"

#define RTF_HEADER "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deftab720{\\fonttbl{" \
                   "\\f0\\fmodern\\fcharset1 Courier New;}}{\\colortbl ;"    \
                   PLAIN_TEXT_COLOR ";" CONSTANT_COLOR ";" KEYWORD_COLOR ";" \
                   COMMENT_COLOR ";" BRACE_COLOR ";" QUOTE_COLOR ";"         \
                   DISABLED_COLOR ";}"                                       \
                   "\\deflang1033\\pard\\plain\\f0\\fs18 \\cf1"

#define RTF_FOOTER "}"
#define RTF_NEWLINE "\\highlight0\\par"
#define RTF_NEWLINE_SIZE 15
#define RTF_PLAIN_TEXT "\\cf1 "
#define RTF_CONSTANT "\\cf2 "
#define RTF_KEYWORD "\\cf3 "
#define RTF_COMMENT "\\cf4 "
#define RTF_BRACE "\\cf5 "
#define RTF_QUOTE "\\cf6 "
#define RTF_DISABLED "\\cf7 "
#define RTF_COLOR_SIZE 5

#define RTF_TEST "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deftab720{\\fonttbl{\\f0" \
                 "\\fmodern\\fcharset1 Courier New;}}\\deflang1033\\pard"      \
                 "\\plain\\f0\\fs22 howdy }"

//
// Define values associated with the profiler display timer.
//

#define PROFILER_TIMER_ID 0x1
#define PROFILER_TIMER_PERIOD 1000

//
// Define the name for the root of the call stack tree.
//

#define CALL_STACK_TREE_ROOT_STRING "Root"

//
// Define the number of columns in the memory statistics list view.
//

#define MEMORY_STATISTICS_COLUMN_COUNT 7

//
// Add some extra padding to make mouse click regions bigger.
//

#define UI_MOUSE_PLAY 8

//
// Define the current debugger UI preferences version number.
//

#define DEBUGGER_UI_PREFERENCES_VERSION 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the function type for the common controls initialization routine.
//

typedef
BOOL
(WINAPI *PINITCOMMONCONTROLSEX) (
    LPINITCOMMONCONTROLSEX lpInitCtrls
    );

typedef
INT
(*PNTDBGCOMPAREROUTINE) (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    );

typedef
ULONGLONG
(*PGETCOLUMNVALUE) (
    PVOID Structure,
    ULONG Offset
    );

typedef struct _STREAM_IN_DATA {
    PCHAR Buffer;
    ULONG BufferSize;
    ULONG CurrentPosition;
} STREAM_IN_DATA, *PSTREAM_IN_DATA;

/*++

Structure Description:

    This structure defines strings, string formats, and various routines
    associated with a column in the memory statistics list view.

Members:

    Header - Stores the string to use as the column header.

    Format - Stores the FormatMessage style format message for the column's
        data.

    DeltaFormat - Stores the FormatMessage style format message to use for the
        column's data when in delta mode.

    CompareRoutine - Stores a pointer to a routine that can be used to compare
        two elements in this column.

    DeltaCompareRoutine - Stores a pointer to a routine that can be used to
        compare two elements in this column when in delta mode.

    Offset - Stores the offset within the PROFILER_MEMORY_POOL_TAG_STATISTIC
        structure that holds the column's data.

--*/

typedef struct _MEMORY_COLUMN {
    LPTSTR Header;
    LPTSTR Format;
    LPTSTR DeltaFormat;
    PNTDBGCOMPAREROUTINE CompareRoutine;
    PNTDBGCOMPAREROUTINE DeltaCompareRoutine;
    PGETCOLUMNVALUE GetColumnValueRoutine;
    ULONG Offset;
} MEMORY_COLUMN, *PMEMORY_COLUMN;

/*++

Structure Description:

    This structure defines strings, string formats, and various routines
    associated with a column in the memory statistics list view.

Members:

    Version - Stores the version of the structure. Set to
        DEBUGGER_UI_PREFERENCES_VERSION.

    WindowX - Stores the X position of the debugger window in pixels.

    WindowY - Stores the Y position of the debugger window in pixels.

    WindowWidth - Stores the width of the debugger window in pixels.

    WindowHeight - Stores the height of the debugger window in pixels.

    MainPaneXPosition - Stores the X position of the divider between the two
        main panes.

    MainPainXPositionWidth - Stores the width of the main pane at the time the
        X position was stored. The X position is used to create a percentage of
        the current width and is only relevant if the old width is saved.

    ProfilerPaneYPosition - Stores the Y position of the diveder between the
        profiler and source code.

    ProfilerPaneYPositionHeight - Store the height of the left pain at the time
        the Y position for the profiler was stored. The Y position is used to
        create a percentage of the current height and is only relevant if the
        old height is stored.

--*/

typedef struct _DEBUGGER_UI_PREFERENCES {
    ULONG Version;
    DWORD WindowX;
    DWORD WindowY;
    DWORD WindowWidth;
    DWORD WindowHeight;
    DWORD MainPaneXPosition;
    DWORD MainPaneXPositionWidth;
    DWORD ProfilerPaneYPosition;
    DWORD ProfilerPaneYPositionHeight;
} DEBUGGER_UI_PREFERENCES, *PDEBUGGER_UI_PREFERENCES;

//
// ----------------------------------------------- Internal Function Prototypes
//

DWORD
WINAPI
ConsoleStandardOutThread (
    LPVOID WindowHandle
    );

DWORD
WINAPI
UiThreadMain (
    LPVOID Parameter
    );

INT_PTR
CALLBACK
MainDialogProc (
    HWND DialogHandle,
    UINT Message,
    WPARAM WParam,
    LPARAM LParam
    );

LRESULT
CALLBACK
TreeViewWindowProcedure (
    HWND Window,
    UINT Message,
    WPARAM WParam,
    LPARAM LParam
    );

DWORD
CALLBACK
RichEditLoadCallback (
    DWORD_PTR Context,
    LPBYTE Buffer,
    LONG Bytes,
    PLONG BytesComplete
    );

BOOL
LoadFileIntoRichEdit (
    HWND RichEdit,
    LPCTSTR Filename,
    PUCHAR FileBuffer,
    ULONGLONG FileSize
    );

BOOL
HighlightSyntax (
    PUCHAR TextBuffer,
    ULONG TextBufferSize,
    PCHAR *BufferOut,
    PULONG FileSizeOut
    );

BOOL
IsKeyword (
    PSTR String
    );

BOOL
IsKeywordSeparator (
    UCHAR Character
    );

VOID
HighlightLine (
    HWND RichEdit,
    LONG LineNumber,
    COLORREF Color,
    BOOL ScrollToLine
    );

VOID
HandleResize (
    HWND Dialog
    );

VOID
HandleCommandMessage (
    HWND Dialog,
    WPARAM WParam
    );

VOID
HandleCommonControlMessage (
    HWND Dialog,
    LPARAM LParam
    );

VOID
HandleCommandEnter (
    HWND CommandEdit
    );

VOID
WriteByteToInput (
    BYTE Byte
    );

VOID
InitializeProfilerControls (
    VOID
    );

VOID
UpdateProfilerWindowType (
    HWND Dialog,
    PROFILER_DATA_TYPE DataType
    );

VOID
HandleProfilerTreeViewCommand (
    HWND Dialog,
    LPARAM LParam
    );

VOID
HandleProfilerListViewCommand (
    HWND Dialog,
    LPARAM LParam
    );

PSTACK_DATA_ENTRY
FindStackDataEntryByHandle (
    PSTACK_DATA_ENTRY Root,
    HTREEITEM Handle
    );

VOID
SetProfilerTimer (
    PROFILER_DATA_TYPE DataType
    );

VOID
KillProfilerTimer (
    PROFILER_DATA_TYPE DataType
    );

VOID
PauseProfilerTimer (
    VOID
    );

VOID
ResumeProfilerTimer (
    VOID
    );

VOID
CALLBACK
ProfilerTimerCallback (
    HWND DialogHandle,
    UINT Message,
    UINT_PTR EventId,
    DWORD Time
    );

BOOL
UpdateProfilerDisplay (
    PROFILER_DATA_TYPE DataType,
    PROFILER_DISPLAY_REQUEST DisplayRequest,
    ULONG Threshold
    );

VOID
UpdateCallStackTree (
    HTREEITEM Parent,
    PSTACK_DATA_ENTRY Root,
    ULONG TotalCount
    );

INT
CALLBACK
StackProfilerTreeCompare (
    LPARAM LParamOne,
    LPARAM LParamTwo,
    LPARAM LParamSort
    );

VOID
UpdateMemoryStatisticsListView (
    PLIST_ENTRY PoolListHead
    );

BOOL
CreateMemoryPoolListViewGroup (
    PPROFILER_MEMORY_POOL MemoryPool,
    PINT GroupId
    );

BOOL
DoesMemoryPoolListViewGroupExist (
    PPROFILER_MEMORY_POOL MemoryPool,
    PINT GroupId
    );

BOOL
UpdateMemoryPoolListViewGroup (
    PPROFILER_MEMORY_POOL MemoryPool,
    INT GroupId
    );

INT
GetMemoryPoolGroupId (
    PPROFILER_MEMORY_POOL MemoryPool
    );

BOOL
CreateMemoryPoolTagListViewItem (
    ULONG Tag,
    INT GroupId,
    PINT ItemIndex
    );

VOID
DeleteMemoryPoolTagListViewItem (
    INT ListViewIndex
    );

BOOL
DoesMemoryPoolTagListViewItemExist (
    PPROFILER_MEMORY_POOL_TAG_STATISTIC Statistic,
    INT GroupId,
    PINT ListViewIndex
    );

BOOL
UpdateMemoryPoolTagListViewItem (
    INT ItemIndex,
    INT GroupId,
    PPROFILER_MEMORY_POOL_TAG_STATISTIC Statistic
    );

INT
CALLBACK
MemoryProfilerListViewCompare (
    LPARAM LParamOne,
    LPARAM LParamTwo,
    LPARAM LParamSort
    );

BOOL
TreeViewIsTreeItemVisible (
    HWND TreeViewWindow,
    HTREEITEM TreeItem
    );

BOOL
TreeViewGetItemRect (
    HWND Window,
    HTREEITEM Item,
    LPRECT Rect,
    BOOL ItemRect
    );

LPTSTR
GetFormattedMessageA (
     LPTSTR Message,
     ...
     );

LPWSTR
GetFormattedMessageW (
     LPWSTR Message,
     ...
     );

VOID
ListViewSetItemText (
    HWND Window,
    int Item,
    int SubItem,
    LPTSTR Text
    );

INT
ComparePoolTag (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    );

INT
CompareUlong (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    );

INT
CompareLong (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    );

INT
CompareLonglong (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    );

INT
CompareUlonglong (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    );

ULONGLONG
GetUlonglongValue (
    PVOID Structure,
    ULONG Offset
    );

ULONGLONG
GetUlongValue (
    PVOID Structure,
    ULONG Offset
    );

VOID
UiGetWindowPreferences (
    HWND Dialog
    );

VOID
UiLoadPreferences (
    HWND Dialog
    );

VOID
UiSavePreferences (
    HWND Dialog
    );

BOOL
UiReadPreferences (
    PDEBUGGER_UI_PREFERENCES Preferences
    );

BOOL
UiWritePreferences (
    PDEBUGGER_UI_PREFERENCES Preferences
    );

HANDLE
UiOpenPreferences (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

HANDLE StdInPipeRead;
HANDLE StdInPipeWrite;
HANDLE StdOutPipe;
BOOL ConsoleInitialized = FALSE;
HWND DialogWindow;
extern HANDLE StdInPipeWrite;

//
// Remember whether or not commands are currently enabled.
//

BOOL CommandsEnabled;

//
// Stores an enumerated type indicating which data type is currently showing in
// the profiler window. The max type means the window is hidden.
//

PROFILER_DATA_TYPE ProfilerWindowType = ProfilerDataTypeMax;

//
// Stores a pointer to the root of the profiler stack tree.
//

PSTACK_DATA_ENTRY StackTreeRoot = NULL;

//
// Stores a handle to a lock that protects access to the stack tree.
//

HANDLE StackTreeLock;

//
// Stores an array describing which profiling types are using the timer.
//

BOOL ProfilerTimerTypes[ProfilerDataTypeMax];

//
// Stores a pointer to the original Tree View Window message procedure call.
//

WNDPROC OriginalTreeViewWindowProcedure;

//
// Stores a handle to the currently selected Tree View item.
//

HTREEITEM TreeViewSelection = NULL;

//
// Stores a boolean indicating whether the currently selected Tree View item is
// visible.
//

BOOL TreeViewSelectionVisible = FALSE;

//
// Stores an array of memory statistics list view column names.
//

MEMORY_COLUMN MemoryStatisticsColumns[MEMORY_STATISTICS_COLUMN_COUNT] = {
     {"Tag",
      "%1!c!%2!c!%3!c!%4!c!",
      "%1!c!%2!c!%3!c!%4!c!",
      ComparePoolTag,
      ComparePoolTag,
      GetUlongValue,
      FIELD_OFFSET(PROFILER_MEMORY_POOL_TAG_STATISTIC, Tag)},

     {"Largest Alloc",
      "%1!#I32x!",
      "%1!#I32x!",
      CompareUlong,
      CompareUlong,
      GetUlongValue,
      FIELD_OFFSET(PROFILER_MEMORY_POOL_TAG_STATISTIC, LargestAllocation)},

     {"Active Bytes",
      "%1!#I64x!",
      "%1!I64d!",
      CompareUlonglong,
      CompareLonglong,
      GetUlonglongValue,
      FIELD_OFFSET(PROFILER_MEMORY_POOL_TAG_STATISTIC, ActiveSize)},

     {"Max Active Bytes",
      "%1!#I64x!",
      "%1!#I64x!",
      CompareUlonglong,
      CompareUlonglong,
      GetUlonglongValue,
      FIELD_OFFSET(PROFILER_MEMORY_POOL_TAG_STATISTIC, LargestActiveSize)},

     {"Active Count",
      "%1!I32u!",
      "%1!I32d!",
      CompareUlong,
      CompareLong,
      GetUlongValue,
      FIELD_OFFSET(PROFILER_MEMORY_POOL_TAG_STATISTIC, ActiveAllocationCount)},

     {"Max Count",
      "%1!I32u!",
      "%1!I32u!",
      CompareUlong,
      CompareUlong,
      GetUlongValue,
      FIELD_OFFSET(PROFILER_MEMORY_POOL_TAG_STATISTIC,
                   LargestActiveAllocationCount)},

     {"Lifetime Alloc",
      "%1!#I64x!",
      "%1!#I64x!",
      CompareUlonglong,
      CompareUlonglong,
      GetUlonglongValue,
      FIELD_OFFSET(PROFILER_MEMORY_POOL_TAG_STATISTIC, LifetimeAllocationSize)}
};

//
// Stores an array of headers for each of the profiler memory types.
//

LPWSTR MemoryStatisticsPoolHeaders[ProfilerMemoryTypeMax] = {
    L"Non-Paged Pool",
    L"Paged Pool"
};

//
// Stores a handle to a lock that protects access to the memory lists.
//

HANDLE MemoryListLock;

//
// Stores a list of memory pools.
//

PLIST_ENTRY MemoryPoolListHead = NULL;

//
// Stores a list of base line memory statistics used to display deltas.
//

PLIST_ENTRY MemoryBaseListHead = NULL;

//
// Stores a point to the memory pool deltas between the current list and the
// base line list.
//

PLIST_ENTRY MemoryDeltaListHead = NULL;

//
// Stores a boolean indicating whether or not delta memory display mode is
// enabled.
//

BOOL MemoryDeltaModeEnabled = FALSE;

//
// Stores memory list view sorting variables.
//

INT CurrentSortColumn = INT_MAX;
BOOL SortAscending = TRUE;

//
// Store whether or not various panes are currently being resized.
//

BOOL WindowSizesInitialized = FALSE;
BOOL ResizingMainPanes = FALSE;
LONG MainPaneXPosition;
LONG MainPaneXPositionWidth;
BOOL ResizingProfilerPane = FALSE;
LONG ProfilerPaneYPosition;
LONG ProfilerPaneYPositionHeight;
LONG ProfilerPaneCurrentYPosition;

//
// Stores the window rect last captured before a minimize or maximize. This is
// used to save the UI preferences.
//

RECT CurrentWindowRect;

//
// ------------------------------------------------------------------ Functions
//

int
APIENTRY
WinMain (
    HINSTANCE Instance,
    HINSTANCE PreviousInstance,
    LPSTR CommandLine,
    int CmdShow
    )

/*++

Routine Description:

    This routine is the main entry point for a Win32 application. It simply
    calls the plaform-independent main function.

Arguments:

    Instance - Supplies a handle to the current instance of the application.

    PreviousInstance - Supplies a handle to the previous instance of the
        application.

    CommandLine - Supplies a pointer to a null-terminated string specifying the
        command line for the application, excluding the program name.

    CmdShow - Supplies how the window is to be shown.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    HANDLE PipeRead;
    HANDLE PipeWrite;
    BOOL Result;
    INT ReturnValue;
    HANDLE UiThread;

    //
    // Create a pipe for the standard output.
    //

    Result = CreatePipe(&PipeRead, &PipeWrite, NULL, 0);
    if (Result == FALSE) {
        DbgOut("Error: Could not create stdout pipe.\n");
        return 1;
    }

    //
    // Set standard output to point to the pipe.
    //

    Result = SetStdHandle(STD_OUTPUT_HANDLE, PipeWrite);
    if (Result == FALSE) {
        DbgOut("Error: Could not redirect stdout.\n");
        return 2;
    }

    StdOutPipe = PipeRead;

    //
    // Create a pipe for standard input.
    //

    Result = CreatePipe(&StdInPipeRead, &StdInPipeWrite, NULL, 0);
    if (Result == FALSE) {
        DbgOut("Error: Could not create stdin pipe.\n");
        return 3;
    }

    //
    // Redirect Standard output to the pipe.
    //

    stdout->_file = _open_osfhandle((LONG)PipeWrite, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    //
    // Kick off the UI thread.
    //

    UiThread = CreateThread(NULL, 0, UiThreadMain, NULL, 0, NULL);
    if (UiThread == NULL) {
        DbgOut("Unable to create the UI thread!\n");
        return 4;
    }

    ReturnValue = DbgrMain(__argc, __argv);
    return ReturnValue;
}

BOOL
DbgrOsInitializeConsole (
    PBOOL EchoCommands
    )

/*++

Routine Description:

    This routine performs any initialization steps necessary before the console
    can be used.

Arguments:

    EchoCommands - Supplies a pointer where a boolean will be returned
        indicating if the debugger should echo commands received (TRUE) or if
        the console has already echoed the command (FALSE).

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ULONG Index;
    ULONG Retries;

    *EchoCommands = TRUE;

    //
    // Wait for the UI to initialize.
    //

    Retries = 10;
    while (Retries != 0) {
        if (ConsoleInitialized != FALSE) {
            break;
        }

        Sleep(100);
        Retries -= 1;
    }

    //
    // If the UI timed out, fail.
    //

    if (Retries == 0) {
        return FALSE;
    }

    //
    // Disable commands from being sent.
    //

    SetFocus(GetDlgItem(DialogWindow, IDE_COMMAND));
    UiEnableCommands(FALSE);

    //
    // Create a lock to protect access to the stack data tree.
    //

    StackTreeLock = CreateDebuggerLock();
    if (StackTreeLock == NULL) {
        return FALSE;
    }

    //
    // Create a lock to protect access to the memory pool lists.
    //

    MemoryListLock = CreateDebuggerLock();
    if (MemoryListLock == NULL) {
        return FALSE;
    }

    //
    // Initialize the profiler timer references.
    //

    for (Index = 0; Index < ProfilerDataTypeMax; Index += 1) {
        ProfilerTimerTypes[Index] = FALSE;
    }

    return TRUE;
}

VOID
DbgrOsDestroyConsole (
    VOID
    )

/*++

Routine Description:

    This routine cleans up anything related to console functionality as a
    debugger is exiting.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (StackTreeLock != NULL) {
        AcquireDebuggerLock(StackTreeLock);
        DbgrDestroyProfilerStackData(StackTreeRoot);
        ReleaseDebuggerLock(StackTreeLock);
        DestroyDebuggerLock(StackTreeLock);
    }

    if (MemoryListLock != NULL) {
        AcquireDebuggerLock(MemoryListLock);
        if (MemoryPoolListHead != MemoryBaseListHead) {
            DbgrDestroyProfilerMemoryData(MemoryBaseListHead);
        }

        DbgrDestroyProfilerMemoryData(MemoryPoolListHead);
        DbgrDestroyProfilerMemoryData(MemoryDeltaListHead);
        ReleaseDebuggerLock(MemoryListLock);
        DestroyDebuggerLock(MemoryListLock);
    }

    return;
}

VOID
DbgrOsPrepareToReadInput (
    VOID
    )

/*++

Routine Description:

    This routine is called before the debugger begins to read a line of input
    from the user.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

BOOL
DbgrOsGetCharacter (
    PUCHAR Key,
    PUCHAR ControlKey
    )

/*++

Routine Description:

    This routine gets one character from the standard input console.

Arguments:

    Key - Supplies a pointer that receives the printable character. If this
        parameter is NULL, printing characters will be discarded from the input
        buffer.

    ControlKey - Supplies a pointer that receives the non-printable character.
        If this parameter is NULL, non-printing characters will be discarded
        from the input buffer.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ULONG BytesRead;
    UCHAR Character;
    UCHAR ControlKeyValue;
    BOOL Result;

    ControlKeyValue = 0;
    while (TRUE) {
        Result = ReadFile(StdInPipeRead, &Character, 1, &BytesRead, NULL);
        if (Result == FALSE) {
            goto GetCharacterEnd;
        }

        if (BytesRead != 1) {
            continue;
        }

        //
        // If it's the magic escape character, look to see if it's a literal
        // escape or just a poke character since there's remote input.
        //

        if (Character == 0xFF) {
            Result = ReadFile(StdInPipeRead, &Character, 1, &BytesRead, NULL);
            if (Result == FALSE) {
                goto GetCharacterEnd;
            }

            if (BytesRead != 1) {
                DbgOut("Failed to read a second byte.\n");
                continue;
            }

            if (Character != 0xFF) {
                Character = 0;
                ControlKeyValue = KEY_REMOTE;
            }
        }

        break;
    }

    //
    // Handle non-printing characters.
    //

    if (Character == '\n') {
        Character = 0;
        ControlKeyValue = KEY_RETURN;
    }

    if ((Character == KEY_UP) || (Character == KEY_DOWN) ||
        (Character == KEY_ESCAPE)) {

        ControlKeyValue = Character;
        Character = 0;
    }

    Result = TRUE;

GetCharacterEnd:
    if (Key != NULL) {
        *Key = Character;
    }

    if (ControlKey != NULL) {
        *ControlKey = ControlKeyValue;
    }

    return Result;
}

VOID
DbgrOsRemoteInputAdded (
    VOID
    )

/*++

Routine Description:

    This routine is called after a remote command is received and placed on the
    standard input remote command list. It wakes up a thread blocked on local
    user input in an OS-dependent fashion.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD BytesWritten;
    unsigned char Message[2];

    //
    // Write the escaped "remote" sequence into the input pipe funnel.
    //

    Message[0] = 0xFF;
    Message[1] = 0x00;
    WriteFile(StdInPipeWrite,
              Message,
              sizeof(Message),
              &BytesWritten,
              NULL);

    return;
}

VOID
DbgrOsPostInputCallback (
    VOID
    )

/*++

Routine Description:

    This routine is called after a line of input is read from the user, giving
    the OS specific code a chance to restore anything it did in the prepare
    to read input function.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

BOOL
UiLoadSourceFile (
    PSTR Path,
    PVOID Contents,
    ULONGLONG Size
    )

/*++

Routine Description:

    This routine loads the contents of a file into the source window.

Arguments:

    Path - Supplies the path of the file being loaded. If this is NULL, then
        the source window should be cleared.

    Contents - Supplies the source file data. This can be NULL.

    Size - Supplies the size of the source file data in bytes.

Return Value:

    Returns TRUE if there was no error, or FALSE if there was an error.

--*/

{

    BOOL Result;
    HWND RichEdit;
    HWND SourceFileEdit;
    INT TextLength;

    if (DialogWindow == NULL) {
        return FALSE;
    }

    Result = TRUE;
    RichEdit = GetDlgItem(DialogWindow, IDE_SOURCE_RICHEDIT);
    SourceFileEdit = GetDlgItem(DialogWindow, IDE_SOURCE_FILE);

    //
    // If NULL was passed in for the file name, just pass that along. This has
    // the effect of clearing the source window.
    //

    if (Path == NULL) {
        Result = LoadFileIntoRichEdit(RichEdit, NULL, NULL, 0);
        Edit_SetText(SourceFileEdit, "");
        goto LoadSourceFileEnd;
    }

    //
    // If the file is not already loaded, then load it.
    //

    Result = LoadFileIntoRichEdit(RichEdit, Path, Contents, Size);
    Edit_SetText(SourceFileEdit, Path);

LoadSourceFileEnd:

    //
    // Point the cursor at the end of the text.
    //

    TextLength = Edit_GetTextLength(SourceFileEdit);
    Edit_SetSel(SourceFileEdit, TextLength, TextLength);
    return Result;
}

BOOL
UiHighlightExecutingLine (
    LONG LineNumber,
    BOOL Enable
    )

/*++

Routine Description:

    This routine highlights the currently executing line and scrolls the source
    window to it, or restores a previously executing source line to the
    normal background color.

Arguments:

    LineNumber - Supplies the 1-based line number to highlight (ie the first
        line in the source file is line 1).

    Enable - Supplies a flag indicating whether to highlight this line (TRUE)
        or restore the line to its original color (FALSE).

Return Value:

    Returns TRUE if there was no error, or FALSE if there was an error.

--*/

{

    HWND RichEdit;

    if (DialogWindow == NULL) {
        return FALSE;
    }

    RichEdit = GetDlgItem(DialogWindow, IDE_SOURCE_RICHEDIT);
    if (Enable != FALSE) {
        HighlightLine(RichEdit, LineNumber, EXECUTING_COLOR, TRUE);

    } else {
        HighlightLine(RichEdit, LineNumber, BACKGROUND_COLOR, FALSE);
    }

    return TRUE;
}

VOID
UiEnableCommands (
    BOOL Enable
    )

/*++

Routine Description:

    This routine enables or disables the command edit control from being
    enabled. If disabled, the edit control will be made read only.

Arguments:

    Enable - Supplies a flag indicating whether or not to enable (TRUE) or
        disable (FALSE) the command box.

Return Value:

    None.

--*/

{

    HWND CommandEdit;

    CommandEdit = GetDlgItem(DialogWindow, IDE_COMMAND);
    CommandsEnabled = Enable;
    if (Enable != FALSE) {
        SendMessage(CommandEdit, EM_SETREADONLY, (WPARAM)FALSE, 0);

    } else {
        SendMessage(CommandEdit, EM_SETREADONLY, (WPARAM)TRUE, 0);
    }

    return;
}

VOID
UiSetCommandText (
    PSTR Text
    )

/*++

Routine Description:

    This routine sets the text inside the command edit box.

Arguments:

    Text - Supplies a pointer to a null terminated string to set the command
        text to.

Return Value:

    None.

--*/

{

    HWND CommandEdit;
    INT TextLength;

    CommandEdit = GetDlgItem(DialogWindow, IDE_COMMAND);
    Edit_SetText(CommandEdit, Text);

    //
    // Point the cursor at the end of the text.
    //

    TextLength = Edit_GetTextLength(CommandEdit);
    Edit_SetSel(CommandEdit, TextLength, TextLength);
    return;
}

DWORD
CALLBACK
RichEditLoadCallback (
    DWORD_PTR Context,
    LPBYTE Buffer,
    LONG Bytes,
    PLONG BytesComplete
    )

/*++

Routine Description:

    This routine is the callback function Windows uses to get input into the
    rich edit control. When a EM_STREAMIN message is processed, the OS calls
    this function repeatedly to get little chunks of data at a time to put into
    the rich edit control.

Arguments:

    Context - Supplies a context pointer supplied by the user when the
        EM_STREAMIN message was passed.

    Buffer - Supplies a pointer to the OS created buffer to stream data into.

    Bytes - Supplies the number of bytes the OS wants this routine to put into
        the Buffer.

    BytesComplete - Supplies a pointer where the number of bytes actually
        written into the buffer by this function is returned.

Return Value:

    Returns 0 on success, and nonzero on failure.

--*/

{

    ULONG BytesToTransfer;
    PSTREAM_IN_DATA StreamData;

    StreamData = (PSTREAM_IN_DATA)Context;

    //
    // If the caller didn't pass anything, just bail out now.
    //

    if (StreamData == NULL) {
        return -1;
    }

    if (StreamData->CurrentPosition + Bytes > StreamData->BufferSize) {
        BytesToTransfer = StreamData->BufferSize - StreamData->CurrentPosition;

    } else {
        BytesToTransfer = Bytes;
    }

    *BytesComplete = BytesToTransfer;

    //
    // If no bytes can be transferred, error out.
    //

    if (BytesToTransfer == 0) {
        return -1;
    }

    //
    // Some bytes can be copied, so do it and return success.
    //

    memcpy(Buffer,
           StreamData->Buffer + StreamData->CurrentPosition,
           BytesToTransfer);

    StreamData->CurrentPosition += BytesToTransfer;
    return 0;
}

VOID
UiSetPromptText (
    PSTR Text
    )

/*++

Routine Description:

    This routine sets the text inside the prompt edit box.

Arguments:

    Text - Supplies a pointer to a null terminated string to set the prompt
        text to.

Return Value:

    None.

--*/

{

    HWND CommandEdit;

    CommandEdit = GetDlgItem(DialogWindow, IDE_PROMPT);
    Edit_SetText(CommandEdit, Text);
    return;
}

VOID
UiDisplayProfilerData (
    PROFILER_DATA_TYPE DataType,
    PROFILER_DISPLAY_REQUEST DisplayRequest,
    ULONG Threshold
    )

/*++

Routine Description:

    This routine displays the profiler data collected by the core debugging
    infrastructure.

Arguments:

    DataType - Supplies the type of profiler data that is to be displayed.

    DisplayRequest - Supplies a value requesting a display action, which can
        either be to display data once, continually, or to stop continually
        displaying data.

    Threshold - Supplies the minimum percentage a stack entry hit must be in
        order to be displayed.

Return Value:

    None.

--*/

{

    BOOL DataDisplayed;
    HWND Profiler;

    //
    // Pause the profiler timer before taking any action. If the timer goes
    // off it will try to acquire one or more of the profiler locks, which
    // could deadlock with this routine trying to output to the main dialog
    // window.
    //

    PauseProfilerTimer();
    switch (DisplayRequest) {

    //
    // If the debugger requested a one-time display of the profiler data, try
    // to display the data.
    //

    case ProfilerDisplayOneTime:
    case ProfilerDisplayOneTimeThreshold:
        DataDisplayed = UpdateProfilerDisplay(DataType,
                                              DisplayRequest,
                                              Threshold);

        if (DataDisplayed == FALSE) {
            DbgOut("There was no new profiler data to display.\n");
            goto DisplayProfilerDataEnd;
        }

        //
        // If no threshold was supplied, it will get displayed in the GUI
        // window; make sure it is visible.
        //

        if (DisplayRequest == ProfilerDisplayOneTime) {
            UpdateProfilerWindowType(DialogWindow, DataType);
        }

        break;

    //
    // If a continuous display was requested, then set the timer for the given
    // type. Additionally, immediately display the data to give the user a good
    // response time since the timer doesn't fire until after the first period.
    //

    case ProfilerDisplayStart:
        UpdateProfilerDisplay(DataType, DisplayRequest, Threshold);
        SetProfilerTimer(DataType);
        break;

    //
    // If a stop was requested, kill the timer for the provided type, hiding
    // the profiler window for that type.
    //

    case ProfilerDisplayStop:
        KillProfilerTimer(DataType);
        break;

    //
    // Handle clear requests.
    //

    case ProfilerDisplayClear:

        //
        // The clear should only be requested for the stack profiler.
        //

        assert(DataType == ProfilerDataTypeStack);

        //
        // Erase the tree control and erase the previously collected stack data.
        //

        if (StackTreeRoot != NULL) {
            AcquireDebuggerLock(StackTreeLock);
            Profiler = GetDlgItem(DialogWindow, IDC_STACK_PROFILER);
            TreeView_DeleteItem(Profiler, (HTREEITEM)StackTreeRoot->UiHandle);
            DbgrDestroyProfilerStackData(StackTreeRoot);
            StackTreeRoot = NULL;
            ReleaseDebuggerLock(StackTreeLock);
        }

        break;

    case ProfilerDisplayStartDelta:

        //
        // The delta request should only be for the memory profiler.
        //

        assert(DataType == ProfilerDataTypeMemory);

        //
        // The delta start request always moves the most recent full statistics
        // to become the base statistics, destroying the old base statistics.
        // It also destroys the delta statistics, which can be released after
        // it wipes the list view from the screen..
        //

        AcquireDebuggerLock(MemoryListLock);
        Profiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);
        ListView_DeleteAllItems(Profiler);
        DbgrDestroyProfilerMemoryData(MemoryDeltaListHead);
        MemoryDeltaListHead = NULL;
        DbgrDestroyProfilerMemoryData(MemoryBaseListHead);

        //
        // If there are no current statistics, collect them.
        //

        if (MemoryPoolListHead != NULL) {
            MemoryBaseListHead = MemoryPoolListHead;
            MemoryPoolListHead = NULL;

        } else {
            MemoryBaseListHead = NULL;
            DbgrGetProfilerMemoryData(&MemoryBaseListHead);
        }

        MemoryDeltaModeEnabled = TRUE;
        ReleaseDebuggerLock(MemoryListLock);

        //
        // Display the most recent data and make sure the timer is enabled.
        //

        UpdateProfilerDisplay(DataType, DisplayRequest, Threshold);
        SetProfilerTimer(DataType);
        break;

    case ProfilerDisplayStopDelta:

        //
        // The delta request should only be for the memory profiler.
        //

        assert(DataType == ProfilerDataTypeMemory);

        AcquireDebuggerLock(MemoryListLock);

        //
        // Do nothing if delta mode is not enabled.
        //

        if (MemoryDeltaModeEnabled == FALSE) {
            ReleaseDebuggerLock(MemoryListLock);
            break;
        }

        //
        // The delta stop request always destroys all the memory lists and sets
        // their pointers to NULL after clearing the display of all list items.
        // Note that delta mode stop does not disable the timer, it takes a
        // full stop command to stop the memory profiler.
        //

        Profiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);
        ListView_DeleteAllItems(Profiler);
        DbgrDestroyProfilerMemoryData(MemoryDeltaListHead);
        MemoryDeltaListHead = NULL;
        DbgrDestroyProfilerMemoryData(MemoryBaseListHead);
        MemoryBaseListHead = NULL;
        DbgrDestroyProfilerMemoryData(MemoryPoolListHead);
        MemoryPoolListHead = NULL;
        MemoryDeltaModeEnabled = FALSE;
        ReleaseDebuggerLock(MemoryListLock);
        break;

    default:
        DbgOut("Error: Invalid profiler display request %d.\n", DisplayRequest);
        break;
    }

DisplayProfilerDataEnd:

    //
    // Restart the profiler timer.
    //

    ResumeProfilerTimer();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

DWORD
WINAPI
ConsoleStandardOutThread (
    LPVOID WindowHandle
    )

/*++

Routine Description:

    This routine is the worker thread that simply receives data from the
    standard out pipe and puts it in the command window.

Arguments:

    WindowHandle - Supplies a pointer to the command window edit box.

Return Value:

    0 Always.

--*/

{

    CHAR Buffer[1024];
    ULONG BytesRead;
    BOOL Result;
    ULONG TextLength;
    HWND Window;

    Window = (HWND)WindowHandle;
    ConsoleInitialized = TRUE;
    while (TRUE) {

        //
        // Read data out of the stdout pipe.
        //

        Result = ReadFile(StdOutPipe,
                          Buffer,
                          sizeof(Buffer) - 1,
                          &BytesRead,
                          NULL);

        if (Result == FALSE) {
            break;
        }

        Buffer[BytesRead] = '\0';

        //
        // Send the character to the command window.
        //

        TextLength = GetWindowTextLength(Window);
        SendMessage(Window, EM_SETSEL, (WPARAM)TextLength, (LPARAM)TextLength);
        SendMessage(Window, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)Buffer);
    }

    return 0;
}

DWORD
WINAPI
UiThreadMain (
    LPVOID Parameter
    )

/*++

Routine Description:

    This routine is the startup routine for the UI thread.

Arguments:

    Parameter - Supplies an unused parameter.

Return Value:

    Returns 0 on success, or nonzero if there was an error.

--*/

{

    HACCEL Accelerators;
    HANDLE CommonControl;
    HANDLE CurrentInstance;
    PINITCOMMONCONTROLSEX InitCommonControlsEx;
    INITCOMMONCONTROLSEX InitControls;
    HICON LargeIcon;
    MSG Message;
    BOOL Result;
    HANDLE RichEditDll;
    HICON SmallIcon;
    HWND StackProfiler;

    //
    // Initialize globals.
    //

    DialogWindow = NULL;

    //
    // Load the rich edit DLL.
    //

    RichEditDll = LoadLibrary(TEXT("Riched20.dll"));
    if (RichEditDll == NULL) {
        DbgOut("Error: Unable to load riched20.dll! The source window will "
               "be unavailable.\n");

        return 0;
    }

    //
    // Load the common control DLL. This is used to create tree views.
    //

    CommonControl = LoadLibrary(TEXT("comctl32.dll"));
    if (CommonControl == NULL) {
        DbgOut("Error: Unable to load comctl32.dll! The source and profiler "
               "window will be unavailable.\n");

        return 0;
    }

    InitCommonControlsEx = (PINITCOMMONCONTROLSEX)GetProcAddress(
                                                 CommonControl,
                                                 TEXT("InitCommonControlsEx"));

    if (InitCommonControlsEx == NULL) {
        DbgOut("Error: Could not get the procedure address for "
               "InitCommonControlsEx.\n");

        return 0;
    }

    //
    // Initialize the common controls.
    //
    // N.B. Rumor has it that adding ICC_LISTVIEW_CLASSES to the initialization
    //      flags prevents group views from working. It is omitted as a result.
    //

    InitControls.dwSize = sizeof(INITCOMMONCONTROLSEX);
    InitControls.dwICC = ICC_TREEVIEW_CLASSES;
    Result = InitCommonControlsEx(&InitControls);
    if (Result == FALSE) {
        DbgOut("InitCommonControlsEx failed\n");
    }

    //
    // Create the main source window. The DialogBox function will not return
    // until the dialog box has been closed, at which point the thread can
    // clean up and exit.
    //

    CurrentInstance = GetModuleHandle(NULL);
    Accelerators = LoadAccelerators(CurrentInstance,
                                    MAKEINTRESOURCE(IDD_ACCELERATORS));

    if (Accelerators == NULL) {
        DbgOut("Error: Could not load accelerators.\n");
        return 0;
    }

    DialogWindow = CreateDialog(CurrentInstance,
                                MAKEINTRESOURCE(IDD_MAIN_WINDOW),
                                NULL,
                                MainDialogProc);

    //
    // TODO: Add support for break-at-cursor and goto-cursor
    //

    ShowWindow(GetDlgItem(DialogWindow, IDC_BREAK_CURSOR), SW_HIDE);
    ShowWindow(GetDlgItem(DialogWindow, IDC_GOTO_CURSOR), SW_HIDE);
    ShowWindow(DialogWindow, SW_SHOW);

    //
    // Load the application icons.
    //

    LargeIcon = LoadImage(CurrentInstance,
                          MAKEINTRESOURCE(IDI_DEBUG_ICON),
                          IMAGE_ICON,
                          32,
                          32,
                          LR_DEFAULTSIZE);

    if (LargeIcon != NULL) {
        SendMessage(DialogWindow, WM_SETICON, ICON_BIG, (LPARAM)LargeIcon);
    }

    SmallIcon = LoadImage(CurrentInstance,
                          MAKEINTRESOURCE(IDI_DEBUG_ICON),
                          IMAGE_ICON,
                          16,
                          16,
                          LR_DEFAULTSIZE);

    if (SmallIcon != NULL) {
        SendMessage(DialogWindow, WM_SETICON, ICON_SMALL, (LPARAM)SmallIcon);
    }

    //
    // Set focus on the input command box.
    //

    SetFocus(GetDlgItem(DialogWindow, IDE_COMMAND));

    //
    // Override the stack profiler's window message procedure call.
    //

    StackProfiler = GetDlgItem(DialogWindow, IDC_STACK_PROFILER);
    OriginalTreeViewWindowProcedure = (WNDPROC)SetWindowLong(
                                                StackProfiler,
                                                GWL_WNDPROC,
                                                (LONG)TreeViewWindowProcedure);

    //
    // Initialize the memory profiler list view control.
    //

    InitializeProfilerControls();

    //
    // Pump messages into the dialog processing function.
    //

    while (GetMessage(&Message, NULL, 0, 0) != FALSE) {
        if ((TranslateAccelerator(DialogWindow, Accelerators, &Message) == 0) &&
            (IsDialogMessage(DialogWindow, &Message) == FALSE)) {

            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }

    DialogWindow = NULL;
    FreeLibrary(CommonControl);
    FreeLibrary(RichEditDll);
    if (LargeIcon != NULL) {
        CloseHandle(LargeIcon);
    }

    if (SmallIcon != NULL) {
        CloseHandle(SmallIcon);
    }

    CloseHandle(StdInPipeWrite);
    exit(0);
    return 0;
}

INT_PTR
CALLBACK
MainDialogProc (
    HWND DialogHandle,
    UINT Message,
    WPARAM WParam,
    LPARAM LParam
    )

/*++

Routine Description:

    This routine is the main message pump for the source window. It receives
    messages pertaining to the window and handles interesting ones.

Arguments:

    DialogHandle - Supplies the handle for the overall dialog window.

    Message - Supplies the message being sent to the window.

    WParam - Supplies the "width" parameter, basically the first parameter of
        the message.

    LParam - Supplies the "length" parameter, basically the second parameter of
        the message.

Return Value:

    Returns TRUE if the message was handled, or FALSE if the message was not
    handled and the default handler should be invoked.

--*/

{

    RECT DialogRect;
    LONG NewDivider;
    CHARFORMAT2 NewFormat;
    HANDLE OutputThread;
    POINT Point;
    BOOL Result;
    HWND SourceEdit;
    RECT SourceRect;
    HWND StdOutEdit;
    RECT StdOutRect;

    Result = FALSE;
    switch (Message) {

    //
    // The WM_INITDIALOG message handles the initial dialog creation.
    //

    case WM_INITDIALOG:
        StdOutEdit = GetDlgItem(DialogHandle, IDE_STDOUT_RICHEDIT);
        SourceEdit = GetDlgItem(DialogHandle, IDE_SOURCE_RICHEDIT);

        //
        // Perform a sanity check to make sure the richedit control is there.
        //

        if ((StdOutEdit != NULL) && (SourceEdit != NULL)) {

            //
            // Set the text color, size, and font of the rich edit controls.
            // The Y height is the font's point size times twenty.
            //

            memset(&NewFormat, 0, sizeof(CHARFORMAT2));
            NewFormat.cbSize = sizeof(CHARFORMAT2);
            NewFormat.dwMask = CFM_FACE | CFM_SIZE;
            NewFormat.yHeight = 10 * 20;
            strcpy(NewFormat.szFaceName, "Courier");
            SendMessage(SourceEdit,
                        EM_SETCHARFORMAT,
                        (WPARAM)SCF_ALL,
                        (LPARAM)&NewFormat);

            SendMessage(StdOutEdit,
                        EM_SETCHARFORMAT,
                        (WPARAM)SCF_ALL,
                        (LPARAM)&NewFormat);

            //
            // Set the background color of the source area.
            //

            SendMessage(SourceEdit,
                        EM_SETBKGNDCOLOR,
                        (WPARAM)FALSE,
                        (LPARAM)BACKGROUND_COLOR);

            //
            // Kick off the stdout thread.
            //

            OutputThread = CreateThread(NULL,
                                        0,
                                        ConsoleStandardOutThread,
                                        StdOutEdit,
                                        0,
                                        NULL);

            if (OutputThread == NULL) {
                DbgOut("Unable to create the output thread!\n");
            }
        }

        //
        // Position the elements in the window.
        //

        HandleResize(DialogHandle);
        UiLoadPreferences(DialogHandle);
        Result = TRUE;
        break;

    //
    // The WM_LBUTTONDOWN message is sent when the user clicks in the main
    // window.
    //

    case WM_LBUTTONDOWN:
        StdOutEdit = GetDlgItem(DialogHandle, IDE_STDOUT_RICHEDIT);
        SourceEdit = GetDlgItem(DialogHandle, IDE_SOURCE_RICHEDIT);
        GetWindowRect(StdOutEdit, &StdOutRect);
        GetWindowRect(SourceEdit, &SourceRect);
        MapWindowPoints(NULL,
                        DialogHandle,
                        (LPPOINT)&StdOutRect,
                        sizeof(RECT) / sizeof(POINT));

        MapWindowPoints(NULL,
                        DialogHandle,
                        (LPPOINT)&SourceRect,
                        sizeof(RECT) / sizeof(POINT));

        Point.x = LOWORD(LParam);
        Point.y = HIWORD(LParam);

        //
        // Check to see if the click happened between the two edit windows.
        //

        if ((Point.x >= (SourceRect.right - UI_MOUSE_PLAY)) &&
            (Point.x <= (StdOutRect.left + UI_MOUSE_PLAY))) {

            //
            // Capture mouse events.
            //

            SetCapture(DialogHandle);
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            ResizingMainPanes = TRUE;

        //
        // Check to see if the click happened between the profiler window and
        // the source window.
        //

        } else if ((Point.y >= (SourceRect.bottom - UI_MOUSE_PLAY)) &&
                   (Point.y <=
                    (ProfilerPaneCurrentYPosition + UI_MOUSE_PLAY))) {

            SetCapture(DialogHandle);
            SetCursor(LoadCursor(NULL, IDC_SIZENS));
            ResizingProfilerPane = TRUE;
        }

        break;

    //
    // The WM_LBUTTONUP message is sent when the user releases the mouse in the
    // main window (or all mouse events are being captured).
    //

    case WM_LBUTTONUP:
        if ((ResizingMainPanes != FALSE) || (ResizingProfilerPane != FALSE)) {
            ReleaseCapture();
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            ResizingProfilerPane = FALSE;
            ResizingMainPanes = FALSE;
        }

        break;

    //
    // The WM_MOUSEMOVE message is sent when the mouse moves within the window.
    //

    case WM_MOUSEMOVE:

        //
        // Don't do anything if the left button isn't also held down.
        //

        if (WParam != MK_LBUTTON) {
            break;
        }

        GetWindowRect(DialogHandle, &DialogRect);
        MapWindowPoints(NULL,
                        DialogHandle,
                        (LPPOINT)&DialogRect,
                        sizeof(RECT) / sizeof(POINT));

        StdOutEdit = GetDlgItem(DialogHandle, IDE_STDOUT_RICHEDIT);
        SourceEdit = GetDlgItem(DialogHandle, IDE_SOURCE_RICHEDIT);
        GetWindowRect(StdOutEdit, &StdOutRect);
        MapWindowPoints(NULL,
                        DialogHandle,
                        (LPPOINT)&StdOutRect,
                        sizeof(RECT) / sizeof(POINT));

        GetWindowRect(SourceEdit, &SourceRect);
        MapWindowPoints(NULL,
                        DialogHandle,
                        (LPPOINT)&SourceRect,
                        sizeof(RECT) / sizeof(POINT));

        Point.x = LOWORD(LParam);
        Point.y = HIWORD(LParam);

        //
        // Resize the main panes if in the middle of that.
        //

        if (ResizingMainPanes != FALSE) {
            NewDivider = (SHORT)Point.x;
            MainPaneXPosition = NewDivider;
            MainPaneXPositionWidth = DialogRect.right;
            HandleResize(DialogHandle);

        } else if (ResizingProfilerPane != FALSE) {
            NewDivider = (SHORT)Point.y;
            ProfilerPaneYPosition = NewDivider;
            ProfilerPaneYPositionHeight = DialogRect.bottom;
            HandleResize(DialogHandle);
        }

        break;

    //
    // The WM_COMMAND message indicates that a button or keyboard accelerator
    // has been pressed.
    //

    case WM_COMMAND:
        HandleCommandMessage(DialogHandle, WParam);
        Result = TRUE;
        break;

    //
    // The WM_NOTIFY message indicates that a common control event has
    // occurred.
    //

    case WM_NOTIFY:
        HandleCommonControlMessage(DialogHandle, LParam);
        Result = TRUE;
        break;

    //
    // The WM_SIZE message indicates that the window was resized.
    //

    case WM_SIZE:
        HandleResize(DialogHandle);
        Result = TRUE;
        break;

    //
    // The WM_EXITSIZEMOVE message indicates that the window is done being
    // moved (dragged or resized).
    //

    case WM_EXITSIZEMOVE:
        UiGetWindowPreferences(DialogHandle);
        Result = TRUE;
        break;

    //
    // The program is exiting.
    //

    case WM_DESTROY:
        UiSavePreferences(DialogHandle);
        PostQuitMessage(0);
        Result = TRUE;
        break;
    }

    return Result;
}

LRESULT
CALLBACK
TreeViewWindowProcedure (
    HWND Window,
    UINT Message,
    WPARAM WParam,
    LPARAM LParam
    )

/*++

Routine Description:

    This routine handles window messages that are passed to the Tree View
    control.

Arguments:

    Window - Supplies the handle for the Tree View window.

    Message - Supplies the message being sent to the window.

    WParam - Supplies the "width" parameter, basically the first parameter of
        the message.

    LParam - Supplies the "length" parameter, basically the second parameter of
        the message.

Return Value:

    Returns TRUE if the message was handled, or FALSE if the message was not
    handled and the default handler should be invoked.

--*/

{

    LRESULT Result;

    switch (Message) {

    //
    // On key up or down, if the currently "selected" item is not visible, then
    // reselected it. This will snap it back into view.
    //

    case WM_KEYUP:
    case WM_KEYDOWN:
        if ((TreeViewSelection != NULL) &&
            (TreeViewIsTreeItemVisible(Window, TreeViewSelection) == FALSE)) {

            TreeView_SelectItem(Window, TreeViewSelection);
            TreeViewSelectionVisible = TRUE;
        }

        break;

    //
    // If the window is scrolled and the selected item goes out of view, then
    // deselect it. This makes updates smoother. If the scroll pulls the
    // selected item into view, then select it again.
    //

    case WM_VSCROLL:
    case WM_MOUSEWHEEL:
        if ((TreeViewSelection != NULL) &&
            (TreeViewIsTreeItemVisible(Window, TreeViewSelection) != FALSE)) {

            if (TreeViewSelectionVisible == FALSE) {
                TreeView_SelectItem(Window, TreeViewSelection);
                TreeViewSelectionVisible = TRUE;
            }

        } else {
            if (TreeViewSelectionVisible != FALSE) {
                TreeView_SelectItem(Window, NULL);
                TreeViewSelectionVisible = FALSE;
            }
        }

        break;

    default:
        break;
    }

    //
    // Always forwad the call on to the original window procedure call.
    //

    Result = CallWindowProc(OriginalTreeViewWindowProcedure,
                            Window,
                            Message,
                            WParam,
                            LParam);

    return Result;
}

BOOL
LoadFileIntoRichEdit (
    HWND RichEdit,
    LPCTSTR Filename,
    PUCHAR FileBuffer,
    ULONGLONG FileSize
    )

/*++

Routine Description:

    This routine loads the contents of a file into the rich edit box.

Arguments:

    RichEdit - Supplies the handle to the rich edit box.

    Filename - Supplies a NULL terminated string containing the name of the file
        to load.

    FileBuffer - Supplies a pointer to the file contents.

    FileSize - Supplies the size of the file contents in bytes.

Return Value:

    Returns TRUE if there was no error, or FALSE if there was an error.

--*/

{

    EDITSTREAM EditStream;
    LRESULT Result;
    STREAM_IN_DATA StreamData;
    BOOL Success;

    memset(&StreamData, 0, sizeof(STREAM_IN_DATA));

    //
    // Highlight C-Style syntax and convert the text file into a rich text
    // formatted buffer.
    //

    Success = HighlightSyntax(FileBuffer,
                              FileSize,
                              &(StreamData.Buffer),
                              &(StreamData.BufferSize));

    if (Success == FALSE) {
        goto LoadFileIntoRichEditEnd;
    }

    //
    // Set the maximum amount of rich text allowed in the control to twice the
    // buffer size. Without this message, the default is 32 kilobytes.
    //

    SendMessage(RichEdit,
                EM_EXLIMITTEXT,
                (WPARAM)0,
                (LPARAM)(StreamData.BufferSize * 2));

    //
    // Set up the EM_STREAMIN command by filling out the edit stream
    // context and callback function.
    //

    memset(&EditStream, 0, sizeof(EDITSTREAM));
    EditStream.pfnCallback = RichEditLoadCallback;
    EditStream.dwCookie = (DWORD_PTR)&StreamData;
    Result = SendMessage(RichEdit, EM_STREAMIN, SF_RTF, (LPARAM)&EditStream);
    if ((Result != 0) && (EditStream.dwError == 0)) {
        Success = TRUE;
    }

LoadFileIntoRichEditEnd:

    //
    // If a failure occurred, clear the source window.
    //

    if (Success == FALSE) {
        EditStream.pfnCallback = RichEditLoadCallback;
        EditStream.dwCookie = (DWORD_PTR)NULL;
        SendMessage(RichEdit, EM_STREAMIN, SF_RTF, (LPARAM)&EditStream);
    }

    if (StreamData.Buffer != NULL) {
        free(StreamData.Buffer);
    }

    return Success;
}

BOOL
HighlightSyntax (
    PUCHAR TextBuffer,
    ULONG TextBufferSize,
    PCHAR *BufferOut,
    PULONG FileSizeOut
    )

/*++

Routine Description:

    This routine takes in a text file and adds rich text formatting to perform
    C style syntax highlighting. The caller must remember to free memory
    allocated here.

Arguments:

    TextBuffer - Supplies a pointer to text contents to highlight.

    TextBufferSize - Supplies the size of the text, in bytes.

    BufferOut - Supplies a pointer that receives a pointer to the rich text
        buffer. The caller must remember to free this buffer.

    FileSizeOut - Supplies a pointer that receives the size of the highlighted
        file. Note that the buffer may or may not be bigger than this value.

Return Value:

    Returns TRUE if there was no error, or FALSE if there was an error.

--*/

{

    ULONG BytesOut;
    ULONG BytesReadIn;
    ULONG BytesToCopy;
    ULONG ColorChangeLength;
    PCHAR Destination;
    PCHAR FileBuffer;
    ULONG FileBufferSize;
    UCHAR FileByte;
    BOOL InDisabledCode;
    BOOL InDoubleQuotes;
    BOOL InMultiLineComment;
    BOOL InSingleLineComment;
    BOOL InSingleQuotes;
    CHAR Keyword[MAX_KEYWORD + 1];
    ULONG KeywordIndex;
    ULONG KeywordLength;
    PSTR KeywordStart;
    PSTR PoundIfStart;
    UCHAR PreviousCharacter;
    BOOL PreviousKeywordPoundIf;
    BOOL ResetColor;
    PCHAR Source;
    BOOL Success;
    BOOL WasBackslash;

    BytesOut = 0;
    BytesReadIn = 0;
    InDisabledCode = FALSE;
    InDoubleQuotes = FALSE;
    InSingleLineComment = FALSE;
    InSingleQuotes = FALSE;
    InMultiLineComment = FALSE;
    KeywordIndex = 0;
    KeywordStart = NULL;
    PoundIfStart = NULL;
    PreviousKeywordPoundIf = FALSE;
    Success = TRUE;
    WasBackslash = FALSE;
    *BufferOut = NULL;
    *FileSizeOut = 0;

    //
    // Allocate a buffer big enough to hold the original text file plus all the
    // formatting. Guess a size that should be big enough.
    //

    FileBufferSize = TextBufferSize * 5;
    if (FileBufferSize < strlen(RTF_HEADER) + strlen(RTF_FOOTER) + 8192) {
        FileBufferSize += strlen(RTF_HEADER) + strlen(RTF_FOOTER) + 8192;
    }

    FileBuffer = malloc(FileBufferSize);
    if (FileBuffer == NULL) {
        Success = FALSE;
        goto HighlightSyntaxEnd;
    }

    *BufferOut = FileBuffer;
    memset(Keyword, 0, MAX_KEYWORD + 1);

    //
    // Copy the RTF header.
    //

    memcpy(FileBuffer, RTF_HEADER, strlen(RTF_HEADER));
    FileBuffer += strlen(RTF_HEADER);
    BytesOut += strlen(RTF_HEADER);
    ResetColor = FALSE;
    PreviousCharacter = '\0';
    while (TRUE) {

        //
        // If the entire input file has been read, end the loop.
        //

        if (BytesReadIn == TextBufferSize) {
            break;
        }

        //
        // Get a byte from the input buffer.
        //

        FileByte = TextBuffer[BytesReadIn];
        BytesReadIn += 1;

        //
        // If this is a single quote, it's not preceded by a backslash, and
        // it's not in any other sort of comment or quote, toggle the single
        // line quote.
        //

        if ((FileByte == '\'') &&
            (WasBackslash == FALSE) &&
            (InSingleLineComment == FALSE) &&
            (InMultiLineComment == FALSE) &&
            (InDoubleQuotes == FALSE) &&
            (InDisabledCode == FALSE)) {

            if (InSingleQuotes == FALSE) {
                InSingleQuotes = TRUE;
                memcpy(FileBuffer, RTF_QUOTE, RTF_COLOR_SIZE);
                FileBuffer += RTF_COLOR_SIZE;
                BytesOut += RTF_COLOR_SIZE;

            } else {
                InSingleQuotes = FALSE;
                ResetColor = TRUE;
            }
        }

        //
        // If this is a double quote, it's not preceded by a backslash, and it's
        // not in any other sort of comment or quote, toggle the double line
        // quote.
        //

        if ((FileByte == '\"') &&
            (WasBackslash == FALSE) &&
            (InSingleLineComment == FALSE) &&
            (InMultiLineComment == FALSE) &&
            (InSingleQuotes == FALSE) &&
            (InDisabledCode == FALSE)) {

            if (InDoubleQuotes == FALSE) {
                InDoubleQuotes = TRUE;
                memcpy(FileBuffer, RTF_QUOTE, RTF_COLOR_SIZE);
                FileBuffer += RTF_COLOR_SIZE;
                BytesOut += RTF_COLOR_SIZE;

            } else {
                InDoubleQuotes = FALSE;
                ResetColor = TRUE;
            }
        }

        //
        // If this is a newline, end a single line comment now.
        //

        if ((InSingleLineComment != FALSE) &&
            (InMultiLineComment == FALSE) &&
            (InSingleQuotes == FALSE) &&
            (InDoubleQuotes == FALSE) &&
            (InDisabledCode == FALSE) &&
            ((FileByte == '\n') || (FileByte == '\r'))) {

            InSingleLineComment = FALSE;
            ResetColor = TRUE;
        }

        //
        // If this character is a / and so was the last one, this begins a
        // single line comment. Back up a character to apply the formatting, but
        // remember that the / got formatted as a divide sign, so there's a
        // plain text directive after the slash already which needs to be
        // backed out.
        //

        if ((InSingleLineComment == FALSE) &&
            (InMultiLineComment == FALSE) &&
            (InSingleQuotes == FALSE) &&
            (InDoubleQuotes == FALSE) &&
            (InDisabledCode == FALSE) &&
            (FileByte == '/') &&
            (PreviousCharacter == '/')) {

            FileBuffer -= RTF_COLOR_SIZE + 1;
            memcpy(FileBuffer, RTF_COMMENT, RTF_COLOR_SIZE);
            FileBuffer += RTF_COLOR_SIZE;
            BytesOut += RTF_COLOR_SIZE;
            *FileBuffer = '/';
            FileBuffer += 1;
            BytesOut -= RTF_COLOR_SIZE;
            InSingleLineComment = TRUE;
        }

        //
        // If another comment or quote is not in progress, check for the
        // beginning of a multiline comment. Back up to format the / as well,
        // but watch out for that plain text directive.
        //

        if ((FileByte == '*') &&
            (PreviousCharacter == '/') &&
            (InSingleLineComment == FALSE) &&
            (InSingleQuotes == FALSE) &&
            (InDoubleQuotes == FALSE) &&
            (InDisabledCode == FALSE)) {

            FileBuffer -= RTF_COLOR_SIZE + 1;
            memcpy(FileBuffer, RTF_COMMENT, RTF_COLOR_SIZE);
            FileBuffer += RTF_COLOR_SIZE;
            BytesOut += RTF_COLOR_SIZE;
            *FileBuffer = '/';
            FileBuffer += 1;
            BytesOut -= RTF_COLOR_SIZE;
            InMultiLineComment = TRUE;
        }

        //
        // Don't do syntax highlighting while inside a comment or quote.
        // *Do* go into this loop for disabled code to know when to get out of
        // disabled code.
        //

        if ((InSingleLineComment == FALSE) &&
            (InMultiLineComment == FALSE) &&
            (InSingleQuotes == FALSE) &&
            (InDoubleQuotes == FALSE)) {

            //
            // If this character marks the end of a keyword, evaluate the
            // keyword.
            //

            if (IsKeywordSeparator(FileByte) != FALSE) {

                //
                // End the keyword and compare against known keywords (or a
                // number).
                //

                Keyword[KeywordIndex] = '\0';

                //
                // If the current code is marked as disabled code, an #endif
                // ends that.
                //

                if ((InDisabledCode != FALSE) &&
                    ((strcmp(Keyword, "#endif") == 0) ||
                     (strcmp(Keyword, "#else") == 0))) {

                    InDisabledCode = FALSE;
                    ResetColor = TRUE;
                }

                //
                // If the keyword is a zero and the previous keyword was #if,
                // then its a "#if 0" disabling the code.
                //

                if ((PreviousKeywordPoundIf != FALSE) &&
                    (strcmp(Keyword, "0") == 0)) {

                    //
                    // Copy the "#if 0" forward to make room for the color
                    // change. Standard memory copy routines are not appropriate
                    // here because the regions may be overlapping (which is
                    // also the reason for copying backwards).
                    //

                    ColorChangeLength = RTF_COLOR_SIZE;
                    KeywordLength = (ULONG)FileBuffer - (ULONG)PoundIfStart;
                    Source = PoundIfStart + KeywordLength - 1;
                    Destination = FileBuffer + ColorChangeLength - 1;
                    BytesToCopy = KeywordLength;
                    while (BytesToCopy != 0) {
                        *Destination = *Source;
                        Destination -= 1;
                        Source -= 1;
                        BytesToCopy -= 1;
                    }

                    InDisabledCode = TRUE;

                    //
                    // Copy the disabled code color into the text stream. Use
                    // memcpy to avoid copying a null terminator over the data.
                    //

                    memcpy(PoundIfStart, RTF_DISABLED, ColorChangeLength);
                    FileBuffer += ColorChangeLength;
                    BytesOut += ColorChangeLength;
                }

                //
                // If it's #if, set that flag in preparation for a possible
                // 0 coming next.
                //

                if (strcmp(Keyword, "#if") == 0) {
                    PreviousKeywordPoundIf = TRUE;
                    PoundIfStart = KeywordStart;

                } else {
                    PreviousKeywordPoundIf = FALSE;
                    PoundIfStart = NULL;
                }

                //
                // Highlight the keyword if it's a number or C reserved keyword.
                // Don't highlight if in disabled code.
                //

                if ((InDisabledCode == FALSE) &&
                    (((Keyword[0] >= '0') && (Keyword[0] <= '9')) ||
                     (IsKeyword(Keyword) != FALSE))) {

                    //
                    // Copy the part of the buffer after the start of the
                    // keyword forward to make room for the color change text.
                    // Don't use standard routines because the regions may
                    // overlap. It's okay to copy overlapping regions manually
                    // because it's known that the destination comes after the
                    // source, so as long as copying is done right-to-left, the
                    // operation is safe.
                    //

                    ColorChangeLength = RTF_COLOR_SIZE;
                    KeywordLength = strlen(Keyword);
                    Destination = FileBuffer + ColorChangeLength - 1;
                    Source = KeywordStart + KeywordLength - 1;
                    BytesToCopy = KeywordLength;
                    while (BytesToCopy != 0) {
                        *Destination = *Source;
                        Destination -= 1;
                        Source -= 1;
                        BytesToCopy -= 1;
                    }

                    //
                    // Use memcpy instead of strcpy because the null terminator
                    // that was that strcpy would write on the end would clobber
                    // the data just shifted over.
                    //

                    memcpy(KeywordStart, RTF_KEYWORD, ColorChangeLength);
                    FileBuffer += ColorChangeLength;
                    BytesOut += ColorChangeLength;
                    ResetColor = TRUE;
                }

                //
                // This was a keyword and it was just dealt with. Reset the
                // keyword contents and pointer.
                //

                KeywordIndex = 0;
                KeywordStart = NULL;

            //
            // This character is not a keyword separator. Save the new byte into
            // the current keyword, but only if there's room.
            //

            } else if (KeywordIndex < MAX_KEYWORD) {

                //
                // If this is the first character in the token, save the
                // position in the file buffer.
                //

                if (KeywordIndex == 0) {
                    KeywordStart = FileBuffer;
                }

                Keyword[KeywordIndex] = FileByte;
                KeywordIndex += 1;
            }

            //
            // Handle single character highlights. Don't highlight disabled
            // code.
            //

            if (InDisabledCode == FALSE) {
                switch (FileByte) {

                //
                // Operators +, -, *, /, >, <, =, ., !, ^, &, |, :, ;, ~, %
                // should be highlighted with the constant color.
                //

                case '+':
                case '-':
                case '*':
                case '/':
                case '?':
                case '>':
                case '<':
                case '=':
                case '.':
                case '!':
                case '^':
                case '&':
                case '|':
                case ':':
                case ';':
                case '~':
                case '%':
                    memcpy(FileBuffer, RTF_CONSTANT, RTF_COLOR_SIZE);
                    FileBuffer += RTF_COLOR_SIZE;
                    BytesOut += RTF_COLOR_SIZE;
                    ResetColor = TRUE;
                    break;

                //
                // Braces {}, [], and () should be highlighted with the brace
                // color.
                //

                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                    memcpy(FileBuffer, RTF_BRACE, RTF_COLOR_SIZE);
                    FileBuffer += RTF_COLOR_SIZE;
                    BytesOut += RTF_COLOR_SIZE;
                    ResetColor = TRUE;
                    break;

                default:
                    break;
                }
            }
        }

        //
        // New lines must be replaced by /par. The actual new line characters
        // seem to be ignored, so leave them in. Also reset the comment coloring
        // if inside a multi-line comment.
        //

        if (FileByte == '\n') {
            memcpy(FileBuffer, RTF_NEWLINE, RTF_NEWLINE_SIZE);
            FileBuffer += RTF_NEWLINE_SIZE;
            BytesOut += RTF_NEWLINE_SIZE;
        }

        //
        // The characters }, {, and \ have to be preceded by a \.
        //

        if ((FileByte == '{') || (FileByte == '}') || (FileByte == '\\')) {
            *FileBuffer = '\\';
            FileBuffer += 1;
            BytesOut += 1;
        }

        //
        // Copy the character from the file into the buffer.
        //

        if (FileByte != '\r') {
            *FileBuffer = FileByte;
            FileBuffer += 1;
            BytesOut += 1;
        }

        //
        // If this is a */, end a multiline comment now. This couldn't be
        // handled earlier because the / shouldn't be highlighted like a divide.
        //

        if ((InMultiLineComment != FALSE) &&
            (FileByte == '/') &&
            (PreviousCharacter == '*') &&
            (InSingleQuotes == FALSE) &&
            (InDoubleQuotes == FALSE) &&
            (InSingleLineComment == FALSE) &&
            (InDisabledCode == FALSE)) {

            InMultiLineComment = FALSE;
            ResetColor = TRUE;
        }

        PreviousCharacter = FileByte;

        //
        // Reset the color if something was highlighted but is finished now.
        //

        if (ResetColor != FALSE) {
            ResetColor = FALSE;
            memcpy(FileBuffer, RTF_PLAIN_TEXT, RTF_COLOR_SIZE);
            FileBuffer += RTF_COLOR_SIZE;
            BytesOut += RTF_COLOR_SIZE;
        }

        //
        // Remember if the previous character was a backslash.
        //

        if (FileByte == '\\') {
            if (WasBackslash == FALSE) {
                WasBackslash = TRUE;

            } else {
                WasBackslash = FALSE;
            }

        } else {
            WasBackslash = FALSE;
        }
    }

    //
    // Copy the footer, including the NULL terminator.
    //

    memcpy(FileBuffer, RTF_FOOTER, strlen(RTF_FOOTER) + 1);
    FileBuffer += strlen(RTF_FOOTER) + 1;
    BytesOut += strlen(RTF_FOOTER) + 1;

    //
    // Set the output size, and return.
    //

    *FileSizeOut = BytesOut;
    Success = TRUE;
    if (strlen(*BufferOut) + 1 != BytesOut) {
        DbgOut("ERROR: Not all bytes were accounted for. The rich text buffer "
               "is %d bytes, but only %d bytes were reported!\n",
               strlen(*BufferOut) + 1,
               BytesOut);
    }

    if (BytesOut >= FileBufferSize) {
        DbgOut("ERROR: The rich text buffer was %d bytes, but %d were used. "
               "The buffer was overrun!\n",
               FileBufferSize,
               BytesOut);

        assert(BytesOut < FileBufferSize);
    }

#if 0

    DbgOut("File size: %d, File buffer size: %d, output file size: %d\n",
           TextBufferSize,
           FileBufferSize,
           BytesOut);

#endif

HighlightSyntaxEnd:
    if (Success == FALSE) {
        if (*BufferOut != NULL) {
            free(*BufferOut);
        }
    }

    return Success;
}

BOOL
IsKeyword (
    PSTR String
    )

/*++

Routine Description:

    This routine determines whether or not the given character is a C reserved
    keyword.

Arguments:

    String - Supplies the string containing the suspected keyword.

Return Value:

    Returns TRUE if the keyword is a C reserved keyword. Returns FALSE if it is
    not a C reserved keyword.

--*/

{

    if ((strcmp("auto", String) == 0) ||
        (strcmp("do", String) == 0) ||
        (strcmp("for", String) == 0) ||
        (strcmp("return", String) == 0) ||
        (strcmp("typedef", String) == 0) ||
        (strcmp("break", String) == 0) ||
        (strcmp("double", String) == 0) ||
        (strcmp("goto", String) == 0) ||
        (strcmp("short", String) == 0) ||
        (strcmp("union", String) == 0) ||
        (strcmp("case", String) == 0) ||
        (strcmp("else", String) == 0) ||
        (strcmp("if", String) == 0) ||
        (strcmp("sizeof", String) == 0) ||
        (strcmp("unsigned", String) == 0) ||
        (strcmp("char", String) == 0) ||
        (strcmp("enum", String) == 0) ||
        (strcmp("int", String) == 0) ||
        (strcmp("static", String) == 0) ||
        (strcmp("void", String) == 0) ||
        (strcmp("continue", String) == 0) ||
        (strcmp("extern", String) == 0) ||
        (strcmp("long", String) == 0) ||
        (strcmp("struct", String) == 0) ||
        (strcmp("while", String) == 0) ||
        (strcmp("default", String) == 0) ||
        (strcmp("float", String) == 0) ||
        (strcmp("register", String) == 0) ||
        (strcmp("switch", String) == 0) ||
        (strcmp("const", String) == 0) ||
        (strcmp("signed", String) == 0) ||
        (strcmp("volatile", String) == 0)) {

        return TRUE;
    }

    return FALSE;
}

BOOL
IsKeywordSeparator (
    UCHAR Character
    )

/*++

Routine Description:

    This routine determines whether or not the given character splits two
    keywords.

Arguments:

    Character - Supplies the character to evaluate.

Return Value:

    Returns TRUE if the character could not exist in a keyword, and marks the
    transition between two keywords. Returns FALSE if the character could be
    part of a normal token/keyword.

--*/

{

    switch (Character) {
    case ' ':
    case '\r':
    case '\n':
    case '\\':
    case ',':
    case '+':
    case '-':
    case '*':
    case '?':
    case '/':
    case '>':
    case '<':
    case '=':
    case '.':
    case '!':
    case '^':
    case '&':
    case '|':
    case ':':
    case ';':
    case '~':
    case '%':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
        return TRUE;

    default:
        break;
    }

    return FALSE;
}

VOID
HighlightLine (
    HWND RichEdit,
    LONG LineNumber,
    COLORREF Color,
    BOOL ScrollToLine
    )

/*++

Routine Description:

    This routine highlights or unhighlights a line in the currently loaded
    source file.

Arguments:

    RichEdit - Supplies a handle to the rich edit control.

    LineNumber - Supplies the line number to change the background of. The first
        line in the file is line 1 (ie line numbers are 1 based).

    Color - Supplies the color to paint the background.

    ScrollToLine - Supplies a flag indicating whether or not the window should
        scroll to that line selection.

Return Value:

    None.

--*/

{

    CHARFORMAT2 Format;
    LONG LineBegin;
    LONG LineEnd;
    ULONG OldSelectionBegin;
    ULONG OldSelectionEnd;

    //
    // Get the character index of the line to highlight. Subtract 1 because the
    // Rich Edit line numbers are zero based. Failure here indicates that the
    // line number is greater than the number of lines in the currently loaded
    // file.
    //

    LineBegin = SendMessage(RichEdit,
                            EM_LINEINDEX,
                            (WPARAM)(LineNumber - 1),
                            (LPARAM)0);

    if (LineBegin == -1) {
        return;
    }

    //
    // Get the character index of the first character of the next line, to find
    // out where highlighting should end. Failure here is okay because -1
    // indicates the end of the file, which is correct for highlighting the
    // last line of the file.
    //

    LineEnd = SendMessage(RichEdit,
                          EM_LINEINDEX,
                          (WPARAM)LineNumber,
                          (LPARAM)0);

    //
    // Get the current selection so it can be restored later.
    //

    if (ScrollToLine == FALSE) {
        SendMessage(RichEdit,
                    EM_GETSEL,
                    (WPARAM)(&OldSelectionBegin),
                    (LPARAM)(&OldSelectionEnd));
    }

    //
    // Set the selection to the line about to be highlighted.
    //

    SendMessage(RichEdit,
                EM_SETSEL,
                (WPARAM)LineBegin,
                (LPARAM)LineEnd);

    //
    // Fill out a format structure indicating that the only valid field in the
    // structure is the background color, which is about to be changed.
    //

    memset(&Format, 0, sizeof(CHARFORMAT2));
    Format.cbSize = sizeof(CHARFORMAT2);
    Format.dwMask = CFM_BACKCOLOR;
    Format.crBackColor = Color;

    //
    // Send the message that actually sets the character formatting. Only format
    // the current selection.
    //

    SendMessage(RichEdit,
                EM_SETCHARFORMAT,
                (WPARAM)(SCF_SELECTION),
                (LPARAM)&Format);

    //
    // Restore the current selection if not scrolling to the line.
    //

    if (ScrollToLine == FALSE) {
        SendMessage(RichEdit,
                    EM_SETSEL,
                    (WPARAM)OldSelectionBegin,
                    (LPARAM)OldSelectionEnd);

    //
    // Set the selection to the beginning of the line (but not highlighting the
    // line anymore), and scroll to the caret.
    //

    } else {
        SendMessage(RichEdit, EM_SETSEL, (WPARAM)LineBegin, (LPARAM)LineBegin);
        SendMessage(RichEdit, EM_SCROLLCARET, 0, 0);
    }

    return;
}

VOID
HandleResize (
    HWND Dialog
    )

/*++

Routine Description:

    This routine handles scaling the UI elements when the dialog window is
    resized.

Arguments:

    Dialog - Supplies the handle to the main dialog window.

Return Value:

    None.

--*/

{

    LONG AdjustedMainPaneXPosition;
    LONG AdjustedProfilerPaneYPosition;
    HWND BreakAtCursorButton;
    HWND CommandEdit;
    RECT Control;
    ULONG DialogHeight;
    RECT DialogSize;
    ULONG DialogWidth;
    HWND GotoCursorButton;
    HWND MemoryToggle;
    HWND MemoryView;
    HWND OutputEdit;
    LONG PaneXPosition;
    HWND ProfilerView;
    LONG ProfilerYPosition;
    HWND PromptEdit;
    BOOL Result;
    HWND SourceEdit;
    HWND SourceFileEdit;
    HWND StackToggle;
    HWND StackView;

    //
    // Get handles to all UI elements that need to be adjusted.
    //

    BreakAtCursorButton = GetDlgItem(Dialog, IDC_BREAK_CURSOR);
    CommandEdit = GetDlgItem(Dialog, IDE_COMMAND);
    GotoCursorButton = GetDlgItem(Dialog, IDC_GOTO_CURSOR);
    MemoryToggle = GetDlgItem(Dialog, IDC_MEMORY_PROFILER_TOGGLE);
    MemoryView = GetDlgItem(Dialog, IDC_MEMORY_PROFILER);
    OutputEdit = GetDlgItem(Dialog, IDE_STDOUT_RICHEDIT);
    PromptEdit = GetDlgItem(Dialog, IDE_PROMPT);
    SourceEdit = GetDlgItem(Dialog, IDE_SOURCE_RICHEDIT);
    SourceFileEdit = GetDlgItem(Dialog, IDE_SOURCE_FILE);
    StackToggle = GetDlgItem(Dialog, IDC_STACK_PROFILER_TOGGLE);
    StackView = GetDlgItem(Dialog, IDC_STACK_PROFILER);

    //
    // Get the size of the dialog window (in dialog units) and conversion
    // factors.
    //

    Result = GetWindowRect(Dialog, &DialogSize);
    if (Result == FALSE) {
        DbgOut("Error: Unable to get dialog size.\n");
    }

    DialogWidth = DialogSize.right - DialogSize.left - 15;
    DialogHeight = DialogSize.bottom - DialogSize.top - 37;

    //
    // Initialize the window sizes to a default value if not done.
    //

    if (WindowSizesInitialized == FALSE) {
        MainPaneXPosition = (DialogWidth / 2) - (UI_BORDER / 2);
        MainPaneXPositionWidth = DialogWidth;
        ProfilerPaneYPosition = (DialogHeight / 2) + (UI_BORDER / 2);
        ProfilerPaneYPositionHeight = DialogHeight;
        WindowSizesInitialized = TRUE;
    }

    //
    // Scale the pane positions.
    //

    AdjustedMainPaneXPosition = MainPaneXPosition;
    if (AdjustedMainPaneXPosition < UI_BORDER - 1) {
        AdjustedMainPaneXPosition = UI_BORDER - 1;
    }

    if (AdjustedMainPaneXPosition > DialogWidth - UI_BORDER) {
        AdjustedMainPaneXPosition = DialogWidth - UI_BORDER;
    }

    AdjustedProfilerPaneYPosition = ProfilerPaneYPosition;
    if (AdjustedProfilerPaneYPosition < UI_BUTTON_HEIGHT + (2 * UI_BORDER)) {
        AdjustedProfilerPaneYPosition = UI_BUTTON_HEIGHT + (2 * UI_BORDER);
    }

    if (AdjustedProfilerPaneYPosition > DialogHeight - UI_BORDER) {
        AdjustedProfilerPaneYPosition = DialogHeight - UI_BORDER;
    }

    PaneXPosition = (AdjustedMainPaneXPosition * DialogWidth) /
                    MainPaneXPositionWidth;

    ProfilerYPosition = (AdjustedProfilerPaneYPosition * DialogHeight) /
                         ProfilerPaneYPositionHeight;

    ProfilerPaneCurrentYPosition = ProfilerYPosition;

    //
    // Resize the source and output edit controls to split the screen.
    //

    Control.left = UI_BORDER;
    Control.top = UI_BUTTON_HEIGHT + (2 * UI_BORDER);
    Control.right = PaneXPosition;
    if (ProfilerWindowType != ProfilerDataTypeMax) {
        Control.bottom = ProfilerYPosition - UI_BORDER;

    } else {
        Control.bottom = DialogHeight - UI_BORDER;
    }

    MoveWindow(SourceEdit,
               Control.left,
               Control.top,
               Control.right - Control.left,
               Control.bottom - Control.top,
               FALSE);

    Control.left = PaneXPosition + UI_BORDER;
    Control.top = UI_BUTTON_HEIGHT + (2 * UI_BORDER);
    Control.right = DialogWidth - UI_BORDER;
    Control.bottom = DialogHeight - (2 * UI_BORDER) - UI_BUTTON_HEIGHT;
    MoveWindow(OutputEdit,
               Control.left,
               Control.top,
               Control.right - Control.left,
               Control.bottom - Control.top,
               FALSE);

    //
    // Show or hide the correct profiler view depending on window state.
    //

    if (ProfilerWindowType != ProfilerDataTypeMax) {
        if (ProfilerWindowType == ProfilerDataTypeStack) {
            ProfilerView = StackView;
            ShowWindow(MemoryView, SW_HIDE);

        } else {

            assert(ProfilerWindowType == ProfilerDataTypeMemory);

            ProfilerView = MemoryView;
            ShowWindow(StackView, SW_HIDE);
        }

        Control.left = UI_BORDER;
        Control.top = ProfilerYPosition;
        Control.right = PaneXPosition;
        Control.bottom = DialogHeight - UI_BORDER;
        MoveWindow(ProfilerView,
                   Control.left,
                   Control.top,
                   Control.right - Control.left,
                   Control.bottom - Control.top,
                   FALSE);

        ShowWindow(ProfilerView, SW_SHOW);

    } else {
        ShowWindow(StackView, SW_HIDE);
        ShowWindow(MemoryView, SW_HIDE);
    }

    //
    // Move the prompt and command controls.
    //

    Control.left = PaneXPosition + UI_BORDER;
    Control.top = DialogHeight - UI_BUTTON_HEIGHT - UI_BORDER;
    Control.right = Control.left + UI_PROMPT_WIDTH;
    Control.bottom = Control.top + UI_BUTTON_HEIGHT;
    MoveWindow(PromptEdit,
               Control.left,
               Control.top,
               Control.right - Control.left,
               Control.bottom - Control.top,
               FALSE);

    Control.left = PaneXPosition + (UI_BORDER * 2) + UI_PROMPT_WIDTH;
    Control.top = DialogHeight - UI_BUTTON_HEIGHT - UI_BORDER;
    Control.right = DialogWidth - UI_BORDER;
    Control.bottom = DialogHeight - UI_BORDER;
    MoveWindow(CommandEdit,
               Control.left,
               Control.top,
               Control.right - Control.left,
               Control.bottom - Control.top,
               FALSE);

    //
    // Move the source file edit and right buttons.
    //

    Control.left = UI_BORDER;
    Control.top = UI_BORDER;
    Control.right = PaneXPosition;
    Control.bottom = UI_BORDER + UI_BUTTON_HEIGHT;
    MoveWindow(SourceFileEdit,
               Control.left,
               Control.top,
               Control.right - Control.left,
               Control.bottom - Control.top,
               FALSE);

    Control.left = PaneXPosition + (3 * UI_LARGE_BUTTON_WIDTH) +
                   (UI_BORDER * 4);

    Control.top = UI_BORDER;
    Control.right = Control.left + UI_LARGE_BUTTON_WIDTH;
    Control.bottom = Control.top + UI_BUTTON_HEIGHT;
    MoveWindow(GotoCursorButton,
               Control.left,
               Control.top,
               Control.right - Control.left,
               Control.bottom - Control.top,
               FALSE);

    Control.left = PaneXPosition + (2 * UI_LARGE_BUTTON_WIDTH) +
                   (UI_BORDER * 3);

    Control.top = UI_BORDER;
    Control.right = Control.left + UI_LARGE_BUTTON_WIDTH;
    Control.bottom = Control.top + UI_BUTTON_HEIGHT;
    MoveWindow(BreakAtCursorButton,
               Control.left,
               Control.top,
               Control.right - Control.left,
               Control.bottom - Control.top,
               FALSE);

    //
    // Move the profiler elements.
    //

    Control.left = PaneXPosition + UI_LARGE_BUTTON_WIDTH + (UI_BORDER * 2);
    Control.top = UI_BORDER;
    Control.right = Control.left + UI_LARGE_BUTTON_WIDTH;
    Control.bottom = Control.top + UI_BUTTON_HEIGHT;
    MoveWindow(MemoryToggle,
               Control.left,
               Control.top,
               Control.right - Control.left,
               Control.bottom - Control.top,
               FALSE);

    Control.left = PaneXPosition + UI_BORDER;
    Control.top = UI_BORDER;
    Control.right = Control.left + UI_LARGE_BUTTON_WIDTH;
    Control.bottom = Control.top + UI_BUTTON_HEIGHT;
    MoveWindow(StackToggle,
               Control.left,
               Control.top,
               Control.right - Control.left,
               Control.bottom - Control.top,
               FALSE);

    //
    // Repaint the entire window.
    //

    RedrawWindow(Dialog, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
    return;
}

VOID
HandleCommandMessage (
    HWND Dialog,
    WPARAM WParam
    )

/*++

Routine Description:

    This routine handles WM_COMMAND messages coming into the dialog box.

Arguments:

    Dialog - Supplies the handle to the main dialog window.

    WParam - Supplies the W Parameter passed in with the message.

Return Value:

    None.

--*/

{

    HWND CommandEdit;
    HWND Focus;

    CommandEdit = GetDlgItem(Dialog, IDE_COMMAND);
    switch (WParam & 0xFFFF) {

    //
    // Destroy the window if it was closed.
    //

    case IDCANCEL:
        DestroyWindow(Dialog);
        break;

    //
    // Control-B was pressed.
    //

    case IDA_CONTROL_B:
        DbgrRequestBreakIn();
        SetFocus(GetDlgItem(DialogWindow, IDE_COMMAND));
        break;

    //
    // Control-K was pressed.
    //

    case IDA_CONTROL_K:
        MessageBox(NULL, "Control K!", "Yippee!", MB_OK);
        break;

    //
    // Up was pressed.
    //

    case IDA_UP:
        Focus = GetFocus();
        if (Focus == CommandEdit) {
            WriteByteToInput(KEY_UP);

        } else {
            SendMessage(Focus, WM_KEYDOWN, VK_UP, 0);
        }

        break;

    //
    // Down was pressed.
    //

    case IDA_DOWN:
        Focus = GetFocus();
        if (Focus == CommandEdit) {
            WriteByteToInput(KEY_DOWN);

        } else {
            SendMessage(Focus, WM_KEYDOWN, VK_DOWN, 0);
        }

        break;

    //
    // Escape was pressed.
    //

    case IDA_ESCAPE:
        WriteByteToInput(KEY_ESCAPE);
        break;

    //
    // The toggle stack profiler view button was pressed.
    //

    case IDC_STACK_PROFILER_TOGGLE:
        if (ProfilerWindowType != ProfilerDataTypeStack) {
            UpdateProfilerWindowType(Dialog, ProfilerDataTypeStack);

        } else {
            UpdateProfilerWindowType(Dialog, ProfilerDataTypeMax);
        }

        break;

    //
    // The toggle memory profiler view button was pressed.
    //

    case IDC_MEMORY_PROFILER_TOGGLE:
        if (ProfilerWindowType != ProfilerDataTypeMemory) {
            UpdateProfilerWindowType(Dialog, ProfilerDataTypeMemory);

        } else {
            UpdateProfilerWindowType(Dialog, ProfilerDataTypeMax);
        }

        break;

    //
    // The OK button means the enter key was pressed on an edit box.
    //

    case IDOK:
        Focus = GetFocus();
        if (Focus == CommandEdit) {
            HandleCommandEnter(CommandEdit);
        }

        break;
    }

    return;
}

VOID
HandleCommonControlMessage (
    HWND Dialog,
    LPARAM LParam
    )

/*++

Routine Description:

    This routine handles WM_NOTIFY messages coming into the dialog box.

Arguments:

    Dialog - Supplies the handle to the main dialog window.

    LParam - Supplies the L Parameter passed in with the message.

Return Value:

    None.

--*/

{

    LPNMHDR MessageHeader;

    MessageHeader = (LPNMHDR)LParam;
    switch (MessageHeader->idFrom) {
    case IDC_STACK_PROFILER:
        HandleProfilerTreeViewCommand(Dialog, LParam);
        break;

    case IDC_MEMORY_PROFILER:
        HandleProfilerListViewCommand(Dialog, LParam);
        break;

    default:
        break;
    }

    return;
}

VOID
HandleCommandEnter (
    HWND CommandEdit
    )

/*++

Routine Description:

    This routine handles a command entered into the command edit box.

Arguments:

    CommandEdit - Supplies a handle to the command edit box containing the
        command.

Return Value:

    None.

--*/

{

    PCHAR Buffer;
    ULONG BytesWritten;
    PCHAR CurrentBuffer;
    BOOL Result;
    INT TextLength;

    Buffer = NULL;

    //
    // Do nothing if commands are not enabled.
    //

    if (CommandsEnabled == FALSE) {
        return;
    }

    //
    // Get the length of the text in the command control, allocate space,
    // and read in the control.
    //

    TextLength = Edit_GetTextLength(CommandEdit);
    if (TextLength < 0) {
        goto HandleCommandEnterEnd;
    }

    Buffer = malloc(TextLength + 1);
    if (Buffer == NULL) {
        goto HandleCommandEnterEnd;
    }

    Edit_GetText(CommandEdit, Buffer, TextLength + 1);

    //
    // Write the data into the pipe.
    //

    CurrentBuffer = Buffer;
    while (TextLength != 0) {
        Result = WriteFile(StdInPipeWrite,
                           CurrentBuffer,
                           TextLength,
                           &BytesWritten,
                           NULL);

        if (Result == FALSE) {
            goto HandleCommandEnterEnd;
        }

        CurrentBuffer += BytesWritten;
        TextLength -= BytesWritten;
    }

    //
    // Write the final newline into the pipe.
    //

    Buffer[0] = '\n';
    Result = WriteFile(StdInPipeWrite,
                       Buffer,
                       1,
                       &BytesWritten,
                       NULL);

    if ((Result == FALSE) || (BytesWritten != 1)) {
        DbgOut("Error: final newline could not be sent.\n");
        goto HandleCommandEnterEnd;
    }

HandleCommandEnterEnd:
    Edit_SetText(CommandEdit, "");
    if (Buffer != NULL) {
        free(Buffer);
    }

    return;
}

VOID
WriteByteToInput (
    BYTE Byte
    )

/*++

Routine Description:

    This routine puts a byte of data into the standard input buffer.

Arguments:

    Byte - Supplies the byte to insert into stdin.

Return Value:

    None.

--*/

{

    ULONG BytesWritten;
    BOOL Result;

    Result = WriteFile(StdInPipeWrite,
                       &Byte,
                       1,
                       &BytesWritten,
                       NULL);

    if ((Result == FALSE) || (BytesWritten != 1)) {
        DbgOut("Error: could not send byte to stdin.\n");
    }

    return;
}

VOID
InitializeProfilerControls (
    VOID
    )

/*++

Routine Description:

    This routine initializes the controls used by the profiler.

Arguments:

    None.

Return Value:

    None.

--*/

{

    LPTSTR ColumnHeader;
    ULONG Index;
    LVCOLUMN ListViewColumn;
    HWND MemoryProfiler;

    MemoryProfiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);

    //
    // Set full row select.
    //

    ListView_SetExtendedListViewStyle(MemoryProfiler, LVS_EX_FULLROWSELECT);

    //
    // Add the columns.
    //

    RtlZeroMemory(&ListViewColumn, sizeof(LVCOLUMN));
    ListViewColumn.mask = LVCF_TEXT | LVCF_FMT;
    ListViewColumn.fmt = LVCFMT_RIGHT;
    for (Index = 0; Index < MEMORY_STATISTICS_COLUMN_COUNT; Index += 1) {
        ColumnHeader = MemoryStatisticsColumns[Index].Header;
        ListViewColumn.pszText = ColumnHeader;
        ListViewColumn.cchTextMax = strlen(ColumnHeader) + 1;
        ListView_InsertColumn(MemoryProfiler, Index, &ListViewColumn);
        ListView_SetColumnWidth(MemoryProfiler,
                                Index,
                                LVSCW_AUTOSIZE_USEHEADER);
    }

    //
    // With all the columns width's now appropriately sized, reset the width of
    // the first column. Since it was the only column present when inserted, it
    // greedily consumed the whole control width before others were added.
    //

    ListView_SetColumnWidth(MemoryProfiler, 0, LVSCW_AUTOSIZE_USEHEADER);

    //
    // Enable group mode.
    //

    ListView_EnableGroupView(MemoryProfiler, TRUE);
    return;
}

VOID
UpdateProfilerWindowType (
    HWND Dialog,
    PROFILER_DATA_TYPE DataType
    )

/*++

Routine Description:

    This routine updates the profiler window to show the data of the supplied
    type.

Arguments:

    Dialog - Supplies the handle to the main dialog window.

    DataType - Supplies the type of profiler data whose profiler window should
        be shown.

Return Value:

    None.

--*/

{

    ProfilerWindowType = DataType;
    HandleResize(Dialog);
    return;
}

VOID
HandleProfilerTreeViewCommand (
    HWND Dialog,
    LPARAM LParam
    )

/*++

Routine Description:

    This routine handles tree view commands.

Arguments:

    Dialog - Supplies a handle to the main dialog window.

    LParam - Supplies the L Parameter passed in with the message.

Return Value:

    None.

--*/

{

    UINT Code;
    PSTACK_DATA_ENTRY StackData;
    LPNMTREEVIEW TreeView;

    Code = ((LPNMHDR)LParam)->code;
    switch (Code) {

    //
    // A tree item was selected.
    //

    case TVN_SELCHANGED:
        TreeView = (LPNMTREEVIEW)LParam;
        if (TreeView->itemNew.hItem == NULL) {
            break;
        }

        AcquireDebuggerLock(StackTreeLock);
        StackData = FindStackDataEntryByHandle(StackTreeRoot,
                                               TreeView->itemNew.hItem);

        ReleaseDebuggerLock(StackTreeLock);

        //
        // Save the selection.
        //

        TreeViewSelection = TreeView->itemNew.hItem;
        TreeViewSelectionVisible = TRUE;
        DbgrProfilerStackEntrySelected(StackData);
        break;

    default:
        break;
    }

    return;
}

PSTACK_DATA_ENTRY
FindStackDataEntryByHandle (
    PSTACK_DATA_ENTRY Root,
    HTREEITEM Handle
    )

/*++

Routine Description:

    This routine searches the provided call stack tree to find the entry
    belonging to the given tree item handle.

Arguments:

    Root - Supplies a pointer to the root entry of the call stack tree.

    Handle - Supplies the handle to be matched.

Return Value:

    Returns a pointer to the call stack entry belonging to the given handle on
    success, or NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSTACK_DATA_ENTRY StackData;

    //
    // If the handle matches, return immediately.
    //

    if (Root->UiHandle == Handle) {
        return Root;
    }

    //
    // Recursively search all the children of the tree. Exit if any tree finds
    // a result.
    //

    CurrentEntry = Root->Children.Flink;
    while (CurrentEntry != &(Root->Children)) {
        StackData = CONTAINING_RECORD(CurrentEntry,
                                      STACK_DATA_ENTRY,
                                      SiblingEntry);

        StackData = FindStackDataEntryByHandle(StackData, Handle);
        if (StackData != NULL) {
            return StackData;
        }

        CurrentEntry = CurrentEntry->Flink;
    }

    return NULL;
}

VOID
HandleProfilerListViewCommand (
    HWND Dialog,
    LPARAM LParam
    )

/*++

Routine Description:

    This routine handles list view commands.

Arguments:

    Dialog - Supplies a handle to the main dialog window.

    LParam - Supplies the L Parameter passed in with the message.

Return Value:

    None.

--*/

{

    UINT Code;
    LPNMLISTVIEW ListView;
    HWND MemoryProfiler;

    Code = ((LPNMHDR)LParam)->code;
    switch (Code) {

    //
    // A list view column was clicked.
    //

    case LVN_COLUMNCLICK:

        //
        // Prevent the list from updating during the sort operation as that can
        // result in incorrectly sorted columns.
        //

        AcquireDebuggerLock(MemoryListLock);
        ListView = (LPNMLISTVIEW)LParam;
        if (ListView->iSubItem == CurrentSortColumn) {
            if (SortAscending == FALSE) {
                SortAscending = TRUE;

            } else {
                SortAscending = FALSE;
            }

        } else {
            CurrentSortColumn = ListView->iSubItem;
            SortAscending = TRUE;
        }

        MemoryProfiler = GetDlgItem(Dialog, IDC_MEMORY_PROFILER);
        ListView_SortItems(MemoryProfiler, MemoryProfilerListViewCompare, 0);
        ReleaseDebuggerLock(MemoryListLock);
        break;

    default:
        break;
    }

    return;
}

VOID
SetProfilerTimer (
    PROFILER_DATA_TYPE DataType
    )

/*++

Routine Description:

    This routine sets the profiler timer for the given profiler type and
    prepares the profiler window to display the data.

Arguments:

    DataType - Supplies the profiler data type for which the timer should be
        set.

Return Value:

    None.

--*/

{

    UINT_PTR Result;

    //
    // Set this profiler type's window to come to the front.
    //

    UpdateProfilerWindowType(DialogWindow, DataType);

    //
    // Make this data type update when the timer expires.
    //

    ProfilerTimerTypes[DataType] = TRUE;

    //
    // Set the timer. It's OK if the timer is already set.
    //

    Result = SetTimer(DialogWindow,
                      PROFILER_TIMER_ID,
                      PROFILER_TIMER_PERIOD,
                      ProfilerTimerCallback);

    if (Result == 0) {
        DbgOut("Error: failed to set the profiler update timer.\n");
    }

    return;
}

VOID
KillProfilerTimer (
    PROFILER_DATA_TYPE DataType
    )

/*++

Routine Description:

    This routine kills the profiler timer for the given profiler type and
    hides the data from the profiler window.

Arguments:

    DataType - Supplies the profiler data type for which the timer should be
        set.

Return Value:

    None.

--*/

{

    ULONG Index;
    BOOL TimerInUse;

    //
    // Disable this type for the timer callback.
    //

    ProfilerTimerTypes[DataType] = FALSE;

    //
    // Since this data type is no longer using the timer, determine if another
    // type is using the timer.
    //

    TimerInUse = FALSE;
    for (Index = 0; Index < ProfilerDataTypeMax; Index += 1) {
        if (ProfilerTimerTypes[Index] != 0) {
            TimerInUse = TRUE;
            break;
        }
    }

    //
    // If the timer is still in use, toggle the profiler window to show the
    // still running profiler data.
    //

    if (TimerInUse != FALSE) {
        UpdateProfilerWindowType(DialogWindow, Index);

    //
    // If the timer is not in use, kill it.
    //

    } else {
        KillTimer(DialogWindow, PROFILER_TIMER_ID);
        UpdateProfilerWindowType(DialogWindow, ProfilerDataTypeMax);
    }

    return;
}

VOID
PauseProfilerTimer (
    VOID
    )

/*++

Routine Description:

    This routine pauses the profile timer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    RECT DialogRect;
    ULONG Index;
    MSG Message;
    BOOL Result;
    BOOL TimerInUse;

    //
    // Determine if there is any work to be done.
    //

    TimerInUse = FALSE;
    for (Index = 0; Index < ProfilerDataTypeMax; Index += 1) {
        if (ProfilerTimerTypes[Index] != 0) {
            TimerInUse = TRUE;
            break;
        }
    }

    //
    // If the timer is enabled, then kill it and flush it.
    //

    if (TimerInUse != FALSE) {
        KillTimer(DialogWindow, PROFILER_TIMER_ID);
        RtlZeroMemory(&Message, sizeof(MSG));
        while (Message.message != WM_QUIT) {
            Result = PeekMessage(&Message,
                                 DialogWindow,
                                 WM_TIMER,
                                 WM_TIMER,
                                 PM_REMOVE);

            if (Result == FALSE) {
                break;
            }
        }

        //
        // Flush out any timer message that was in the middle of running by
        // calling a routine that generates a window message.
        //

        GetWindowRect(DialogWindow, &DialogRect);
    }

    return;
}

VOID
ResumeProfilerTimer (
    VOID
    )

/*++

Routine Description:

    This routine resumes the profiler timer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Index;
    UINT_PTR Result;
    BOOL TimerInUse;

    //
    // Determine if any of the profile timers are in use.
    //

    TimerInUse = FALSE;
    for (Index = 0; Index < ProfilerDataTypeMax; Index += 1) {
        if (ProfilerTimerTypes[Index] != 0) {
            TimerInUse = TRUE;
            break;
        }
    }

    //
    // Set the timer. It's OK if the timer is already set.
    //

    if (TimerInUse != FALSE) {
        Result = SetTimer(DialogWindow,
                          PROFILER_TIMER_ID,
                          PROFILER_TIMER_PERIOD,
                          ProfilerTimerCallback);

        if (Result == 0) {
            DbgOut("Error: failed to set the profiler update timer.\n");
        }
    }

    return;
}

VOID
CALLBACK
ProfilerTimerCallback (
    HWND DialogHandle,
    UINT Message,
    UINT_PTR EventId,
    DWORD Time
    )

/*++

Routine Description:

    This routine handles profiler timer callbacks.

Arguments:

    DialogHandle - Supplies a handle to the window dialog associated with the
        timer.

    Message - Supplies the message associated with this callback. It should be
        the timer message.

    EventId - Supplies the ID of the callback event. It should be the profiler
        timer event ID.

    Time - Supplies the number of milliseconds that have elapsed since the
        system was started.

Return Value:

    None.

--*/

{

    ULONG Index;

    assert(Message == WM_TIMER);
    assert(EventId == PROFILER_TIMER_ID);

    //
    // Update the display for every profiler type that is registered with the
    // timer.
    //

    for (Index = 0; Index < ProfilerDataTypeMax; Index += 1) {
        if (ProfilerTimerTypes[Index] != FALSE) {
            UpdateProfilerDisplay(Index, ProfilerDisplayOneTime, 0);
        }
    }

    return;
}

BOOL
UpdateProfilerDisplay (
    PROFILER_DATA_TYPE DataType,
    PROFILER_DISPLAY_REQUEST DisplayRequest,
    ULONG Threshold
    )

/*++

Routine Description:

    This routine updates the profiler display. It collects the updated data
    from the common debugger code and then displays it.

Arguments:

    DataType - Supplies the type of profiler data that is to be displayed.

    DisplayRequest - Supplies a value requesting a display action, which can
        either be to display data once, continually, or to stop continually
        displaying data.

    Threshold - Supplies the minimum percentage a stack entry hit must be in
        order to be displayed.

Return Value:

    Returns TRUE if new data was display, or FALSE otherwise.

--*/

{

    PLIST_ENTRY PoolListHead;
    BOOL Result;
    PLIST_ENTRY ResultPoolListHead;
    BOOL StackTreeLockHeld;

    StackTreeLockHeld = FALSE;
    switch (DataType) {
    case ProfilerDataTypeStack:

        //
        // Acquire the stack tree lock to protect accesses between the profiler
        // timer, run on the UI thread, and console requests from the main
        // debugger thread.
        //

        AcquireDebuggerLock(StackTreeLock);
        StackTreeLockHeld = TRUE;

        //
        // Attempt to get the most up-to-date profiler data.
        //

        Result = DbgrGetProfilerStackData(&StackTreeRoot);
        if (Result == FALSE) {
            goto UpdateProfilerDisplayEnd;
        }

        //
        // If a threshold was specified, then print the stack contents to the
        // display console.
        //

        if (DisplayRequest == ProfilerDisplayOneTimeThreshold) {
            DbgrPrintProfilerStackData(StackTreeRoot, Threshold);

        //
        // Otherwise update the GUI stack tree display.
        //

        } else {
            UpdateCallStackTree(NULL, StackTreeRoot, StackTreeRoot->Count);
        }

        ReleaseDebuggerLock(StackTreeLock);
        StackTreeLockHeld = FALSE;
        break;

    case ProfilerDataTypeMemory:
        Result = DbgrGetProfilerMemoryData(&PoolListHead);
        if ((Result == FALSE) &&
            (DisplayRequest != ProfilerDisplayOneTimeThreshold)) {

            goto UpdateProfilerDisplayEnd;
        }

        //
        // If a threshold was specified, then print the memory contents to the
        // display console, using the saved data if nothing new was returned.
        //
        // N.B. This cannot delete any of the global lists because the UI is
        //      still using them for sorting.
        //

        AcquireDebuggerLock(MemoryListLock);
        if (DisplayRequest == ProfilerDisplayOneTimeThreshold) {
            if (Result == FALSE) {
                PoolListHead = MemoryPoolListHead;
            }

            ResultPoolListHead = DbgrSubtractMemoryStatistics(
                                                           PoolListHead,
                                                           MemoryBaseListHead);

            DbgrPrintProfilerMemoryData(ResultPoolListHead,
                                        MemoryDeltaModeEnabled,
                                        Threshold);

            if (ResultPoolListHead != PoolListHead) {
                DbgrDestroyProfilerMemoryData(ResultPoolListHead);
            }

            if (PoolListHead != MemoryPoolListHead) {
                DbgrDestroyProfilerMemoryData(PoolListHead);
            }

        //
        // Otherwise update the GUI memory list view.
        //

        } else {
            UpdateMemoryStatisticsListView(PoolListHead);
        }

        ReleaseDebuggerLock(MemoryListLock);
        break;

    default:
        DbgOut("Error: invalid profiler data type %d.\n", DataType);
        break;
    }

    Result = TRUE;

UpdateProfilerDisplayEnd:
    if (StackTreeLockHeld != FALSE) {
        ReleaseDebuggerLock(StackTreeLock);
    }

    return Result;
}

VOID
UpdateCallStackTree (
    HTREEITEM Parent,
    PSTACK_DATA_ENTRY Root,
    ULONG TotalCount
    )

/*++

Routine Description:

    This routine updates the tree view for the provided call stack tree entry.
    It will either create a new element for this entry or update the count and
    text associated with the entry. It then operates on the entry's children.
    Once it completes the update of the children, it sorts the children based
    on their count and address.

Arguments:

    Parent - Supplies a handle to the stack entry's parent tree view item.

    Root - Supplies a pointer to the root stack entry of this tree.

    TotalCount - Supplies the total number of stack traces observed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSTR FunctionString;
    LPTSTR ItemString;
    ULONG Percent;
    HWND Profiler;
    BOOL Result;
    LPTSTR ScratchString;
    TVSORTCB Sort;
    PSTACK_DATA_ENTRY StackData;
    HTREEITEM TreeItem;
    TV_INSERTSTRUCT TreeView;
    TVITEM UpdateItem;

    //
    // Return if the total count is zero. There is nothing to do.
    //

    if (TotalCount == 0) {
        return;
    }

    //
    // Calculate the percentage of stack traces in which this entry has been
    // observed.
    //

    Percent = (Root->Count * 100) / TotalCount;

    //
    // Get the symbol string associated with this stack entry. If there is no
    // parent, then it is the root.
    //

    if (Parent == NULL) {
        FunctionString = CALL_STACK_TREE_ROOT_STRING;

    } else {
        FunctionString = Root->AddressSymbol;
    }

    //
    // Get the string for this tree view item.
    //

    ItemString = GetFormattedMessageA("%1: %2!lu!%%, %3!lu!",
                                      FunctionString,
                                      Percent,
                                      Root->Count);

    if (ItemString == NULL) {
        DbgOut("Formatted message failed with status 0x%x\n", GetLastError());
        goto UpdateCallStackTreeEnd;
    }

    //
    // If the treeview item has never been created for this entry, then create
    // a treeview item, supplying display text and a pointer to the stack entry.
    //

    Profiler = GetDlgItem(DialogWindow, IDC_STACK_PROFILER);
    if (Root->UiHandle == NULL) {
        TreeView.hParent = Parent;
        TreeView.item.mask = TVIF_TEXT | TVIF_PARAM;
        TreeView.item.pszText = ItemString;
        TreeView.item.cchTextMax = strlen(ItemString) + 1;
        TreeView.item.lParam = (LONG_PTR)Root;
        TreeItem = TreeView_InsertItem(Profiler, &TreeView);
        if (TreeItem == NULL) {
            DbgOut("Failed to insert item: %s\n", ItemString);
            goto UpdateCallStackTreeEnd;
        }

        //
        // Save the tree item handle for future updates.
        //

        Root->UiHandle = TreeItem;

    //
    // If a treeview item exists, then update its text if necessary. The stack
    // entry should be the same.
    //

    } else {
        ScratchString = LocalAlloc(0, strlen(ItemString) + 1);
        if (ScratchString == NULL) {
            DbgOut("Failed to update item text: %s\n", ItemString);
            goto UpdateCallStackTreeEnd;
        }

        UpdateItem.mask = TVIF_TEXT;
        UpdateItem.pszText = ScratchString;
        UpdateItem.cchTextMax = strlen(ItemString) + 1;
        UpdateItem.hItem = Root->UiHandle;
        Result = TreeView_GetItem(Profiler, &UpdateItem);

        //
        // If the current text could not be retrieved or it does not match the
        // update text, then update the item.
        //

        if ((Result == FALSE) || (strcmp(ScratchString, ItemString) != 0)) {
            LocalFree(ScratchString);
            UpdateItem.mask = TVIF_TEXT;
            UpdateItem.pszText = ItemString;
            UpdateItem.cchTextMax = strlen(ItemString) + 1;
            UpdateItem.hItem = Root->UiHandle;
            Result = TreeView_SetItem(Profiler, &UpdateItem);
            if (Result == FALSE) {
                DbgOut("Failed to update item text %s\n", ItemString);
                goto UpdateCallStackTreeEnd;
            }

        } else {
            LocalFree(ScratchString);
        }

        TreeItem = Root->UiHandle;
    }

    //
    // Release the formatted message string. The insert and set calls above
    // cause the tree view to copy the string.
    //

    LocalFree(ItemString);
    ItemString = NULL;

    //
    // Update the child tree entries.
    //

    CurrentEntry = Root->Children.Flink;
    while (CurrentEntry != &(Root->Children)) {
        StackData = CONTAINING_RECORD(CurrentEntry,
                                      STACK_DATA_ENTRY,
                                      SiblingEntry);

        UpdateCallStackTree(TreeItem, StackData, TotalCount);
        CurrentEntry = CurrentEntry->Flink;
    }

    //
    // Since the children have been updated, sort them by hit count.
    //

    Sort.hParent = TreeItem;
    Sort.lpfnCompare = StackProfilerTreeCompare;
    Sort.lParam = 0;
    TreeView_SortChildrenCB(Profiler, &Sort, FALSE);

UpdateCallStackTreeEnd:
    if (ItemString != NULL) {
        LocalFree(ItemString);
    }

    return;
}

INT
CALLBACK
StackProfilerTreeCompare (
    LPARAM LParamOne,
    LPARAM LParamTwo,
    LPARAM LParamSort
    )

/*++

Routine Description:

    This routine compares two profiler stack entries and determines the order
    in which they should be listed in the tree. This is used to sort a tree
    item's children.

Arguments:

    LParamOne - Supplies a pointer to the stack data entry for the first tree
        item.

    LParamTwo - Supplies a pointer to the stack data entry for the second tree
        item.

    LParamSort - Supplies an unused parameter that is supplied by the parent
        whose children are being sorted.

Return Value:

    Returns a negative value if the first stack entry should precede the second.

    Returns a positive value if the second stack entry should preced the first.

--*/

{

    PSTACK_DATA_ENTRY DataOne;
    PSTACK_DATA_ENTRY DataTwo;

    DataOne = (PSTACK_DATA_ENTRY)LParamOne;
    DataTwo = (PSTACK_DATA_ENTRY)LParamTwo;

    //
    // If the first entry's count is greater, return a negative number to
    // indicate that it should come first.
    //

    if (DataOne->Count > DataTwo->Count) {
        return -1;

    //
    // If the first entry's count is less, return a positive number to indicate
    // that it should come second.
    //

    } else if (DataOne->Count < DataTwo->Count) {
        return 1;

    //
    // If the counts are equal, then compare the entries' addresses. The lower
    // address comes first.
    //

    } else {
        if (DataOne->Address < DataTwo->Address) {
            return -1;
        }
    }

    return 1;
}

VOID
UpdateMemoryStatisticsListView (
    PLIST_ENTRY PoolListHead
    )

/*++

Routine Description:

    This routine updates the memory statistics list view control with the
    newest data returned by the profiling target.

Arguments:

    PoolListHead - Supplies a pointer to the head of the list of new memory
        pool data.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLIST_ENTRY CurrentListHead;
    INT GroupId;
    ULONG Index;
    INT ListViewIndex;
    PPROFILER_MEMORY_POOL MemoryPool;
    PMEMORY_POOL_ENTRY MemoryPoolEntry;
    BOOL ReenableDeltaMode;
    BOOL Result;
    PPROFILER_MEMORY_POOL_TAG_STATISTIC Statistic;
    ULONG TagCount;

    //
    // Subtract the baseline memory statsitics from the current statistics.
    //

    CurrentListHead = DbgrSubtractMemoryStatistics(PoolListHead,
                                                   MemoryBaseListHead);

    //
    // If the subtraction didn't go succeed, temporarily disable delta mode.
    //

    if ((CurrentListHead == PoolListHead) &&
        (MemoryDeltaModeEnabled != FALSE)) {

        MemoryDeltaModeEnabled = TRUE;
        ReenableDeltaMode = TRUE;

    } else {
        ReenableDeltaMode = FALSE;
    }

    //
    // Display the memory statistics for each memory pool.
    //

    CurrentEntry = CurrentListHead->Flink;
    while (CurrentEntry != CurrentListHead) {
        MemoryPoolEntry = CONTAINING_RECORD(CurrentEntry,
                                            MEMORY_POOL_ENTRY,
                                            ListEntry);

        CurrentEntry = CurrentEntry->Flink;

        //
        // Make sure the group exists for this memory pool. If a group does not
        // exist, create one.
        //

        MemoryPool = &(MemoryPoolEntry->MemoryPool);
        Result = DoesMemoryPoolListViewGroupExist(MemoryPool, &GroupId);
        if (Result == FALSE) {
            Result = CreateMemoryPoolListViewGroup(MemoryPool, &GroupId);
            if (Result == FALSE) {
                continue;
            }
        }

        //
        // Update the list view group based on the current memory pool data.
        //

        Result = UpdateMemoryPoolListViewGroup(MemoryPool, GroupId);
        if (Result == FALSE) {
            continue;
        }

        //
        // Create and update list view items for each tag in this memory pool.
        //

        TagCount = MemoryPoolEntry->MemoryPool.TagCount;
        for (Index = 0; Index < TagCount; Index += 1) {
            Statistic = &(MemoryPoolEntry->TagStatistics[Index]);

            //
            // If the subtraction above resulted in a tag with no deltas, then
            // do not display it.
            //

            if ((Statistic->ActiveSize == 0) &&
                (Statistic->ActiveAllocationCount == 0) &&
                (Statistic->LifetimeAllocationSize == 0) &&
                (Statistic->LargestAllocation == 0) &&
                (Statistic->LargestActiveAllocationCount == 0) &&
                (Statistic->LargestActiveSize == 0)) {

                //
                // If, however, the tag already exists, remove it!
                //

                Result = DoesMemoryPoolTagListViewItemExist(Statistic,
                                                            GroupId,
                                                            &ListViewIndex);

                if (Result != FALSE) {
                    DeleteMemoryPoolTagListViewItem(ListViewIndex);
                }

                continue;
            }

            //
            // If there is not already a list view item for these tag
            // statistics, then create one.
            //

            Result = DoesMemoryPoolTagListViewItemExist(Statistic,
                                                        GroupId,
                                                        &ListViewIndex);

            if (Result == FALSE) {
                Result = CreateMemoryPoolTagListViewItem(Statistic->Tag,
                                                         GroupId,
                                                         &ListViewIndex);

                if (Result == FALSE) {
                    continue;
                }
            }

            //
            // Update the list view item for the current tag statistics.
            //

            Result = UpdateMemoryPoolTagListViewItem(ListViewIndex,
                                                     GroupId,
                                                     Statistic);

            if (Result == FALSE) {
                continue;
            }
        }
    }

    //
    // Re-enable delta mode if necessary.
    //

    if (ReenableDeltaMode != FALSE) {
        MemoryDeltaModeEnabled = TRUE;
    }

    //
    // If delta mode is enabled, but no baseline has been established, use the
    // most recent data.
    //

    if ((MemoryDeltaModeEnabled != FALSE) && (MemoryBaseListHead == NULL)) {
        MemoryBaseListHead = PoolListHead;
    }

    //
    // Destroy the saved memory list unless it is acting as the base line list.
    //

    if (MemoryPoolListHead != MemoryBaseListHead) {
        DbgrDestroyProfilerMemoryData(MemoryPoolListHead);
    }

    //
    // Always save the newest pool list.
    //

    MemoryPoolListHead = PoolListHead;

    //
    // If the base list was subtracted from the pool list, then delete the old
    // delta list, saving the current list as the new delta list.
    //

    if (CurrentListHead != PoolListHead) {
        DbgrDestroyProfilerMemoryData(MemoryDeltaListHead);
        MemoryDeltaListHead = CurrentListHead;
    }

    return;
}

BOOL
CreateMemoryPoolListViewGroup (
    PPROFILER_MEMORY_POOL MemoryPool,
    PINT GroupId
    )

/*++

Routine Description:

    This routine creates a new list view group for the given memory pool.

Arguments:

    MemoryPool - Supplies a pointer to the memory pool for which the group will
        be created.

    GroupId - Supplies a pointer that receives the ID of the new group.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    LVGROUP Group;
    INT GroupIndex;
    LPWSTR HeaderString;
    HWND MemoryProfiler;
    BOOL Result;

    *GroupId = GetMemoryPoolGroupId(MemoryPool);

    //
    // Get the header string for this memory pool.
    //

    HeaderString = MemoryStatisticsPoolHeaders[MemoryPool->ProfilerMemoryType];

    //
    // Initialize the list view group, providing a group ID, state, and a
    // header. The header is based on the pool type.
    //

    RtlZeroMemory(&Group, sizeof(LVGROUP));
    Group.cbSize = sizeof(LVGROUP);
    Group.mask = LVGF_HEADER | LVGF_STATE | LVGF_GROUPID;
    Group.iGroupId = MemoryPool->ProfilerMemoryType;
    Group.pszHeader = HeaderString;
    Group.cchHeader = wcslen(HeaderString) + sizeof(WCHAR);
    Group.stateMask = LVGS_COLLAPSIBLE | LVGS_NORMAL;
    Group.state = LVGS_COLLAPSIBLE | LVGS_NORMAL;

    //
    // Insert the group into the memory profiler's list view.
    //

    Result = TRUE;
    MemoryProfiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);
    GroupIndex = ListView_InsertGroup(MemoryProfiler, -1, &Group);
    if (GroupIndex == -1) {
        DbgOut("Error: failed to create memory group for pool type "
               "%d.\n",
               MemoryPool->ProfilerMemoryType);

        Result = FALSE;
        goto CreateMemoryPoolListViewGroupEnd;
    }

    *GroupId = GetMemoryPoolGroupId(MemoryPool);

CreateMemoryPoolListViewGroupEnd:
    return Result;
}

BOOL
DoesMemoryPoolListViewGroupExist (
    PPROFILER_MEMORY_POOL MemoryPool,
    PINT GroupId
    )

/*++

Routine Description:

    This routine returns whether or not a list view group already exists for
    the given memory pool. If it exists, then it returns the ID of the group.

Arguments:

    MemoryPool - Supplies a pointer to the memory pool whose list view group
        status is to be tested.

    GroupId - Supplies a pointer that receives the ID of the memory pool group,
        if it exists.

Return Value:

    Returns TRUE if a list view group exists for the memory pool, or FALSE if
    if does not.

--*/

{

    INT LocalGroupId;
    HWND MemoryProfiler;
    BOOL Result;

    //
    // Determine if there is already a group for this memory pool. The pool
    // memory type is used as the group ID.
    //

    LocalGroupId = GetMemoryPoolGroupId(MemoryPool);
    MemoryProfiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);
    Result = ListView_HasGroup(MemoryProfiler, LocalGroupId);
    if (Result == FALSE) {
        return FALSE;
    }

    *GroupId = LocalGroupId;
    return TRUE;
}

BOOL
UpdateMemoryPoolListViewGroup (
    PPROFILER_MEMORY_POOL MemoryPool,
    INT GroupId
    )

/*++

Routine Description:

    This routine updates the memory pool list view group for the given group ID
    with the given memory pool data.

Arguments:

    MemoryPool - Supplies a pointer to the memory pool data to be used to
        update the list view group.

    GroupId - Supplies the ID of the list view group that is to be updated.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    ULONGLONG FreePercentage;
    LVGROUP Group;
    INT GroupIndex;
    LPWSTR GroupSubtitle;
    HWND MemoryProfiler;
    BOOL Result;

    //
    // Create the wide character string for the group's subtitle.
    //

    if (MemoryPool->TotalPoolSize != 0) {
        FreePercentage = MemoryPool->FreeListSize * 100;
        FreePercentage /= MemoryPool->TotalPoolSize;
        GroupSubtitle = GetFormattedMessageW(L"Size: %1!#I64x!, "
                                             L"Allocs: %2!I64u!, "
                                             L"Frees: %3!I64u!, "
                                             L"Failed: %4!I64u!, "
                                             L"Percent Free: %5!I64u!%%, "
                                             L"Free: %6!#I64x!",
                                             MemoryPool->TotalPoolSize,
                                             MemoryPool->TotalAllocationCalls,
                                             MemoryPool->TotalFreeCalls,
                                             MemoryPool->FailedAllocations,
                                             FreePercentage,
                                             MemoryPool->FreeListSize);

    } else {

        assert(MemoryPool->FreeListSize == 0);

        GroupSubtitle = GetFormattedMessageW(L"Size: -, "
                                             L"Allocs: %1!I64u!, "
                                             L"Frees: %2!I64u!, "
                                             L"Failed: %3!I64u!, "
                                             L"Percent Free: -, "
                                             L"Free: -",
                                             MemoryPool->TotalAllocationCalls,
                                             MemoryPool->TotalFreeCalls,
                                             MemoryPool->FailedAllocations);
    }

    if (GroupSubtitle == NULL) {
        DbgOut("Error: failed to create subtitle for group %d\n", GroupId);
        Result = FALSE;
        goto UpdateMemoryPoolListViewGroupEnd;
    }

    //
    // Initialize the group with the new subtitle.
    //

    Group.mask = LVGF_SUBTITLE;
    Group.cbSize = sizeof(LVGROUP);
    Group.pszSubtitle = GroupSubtitle;
    Group.cchSubtitle = wcslen(GroupSubtitle) + sizeof(WCHAR);

    //
    // Set the group information for the group with the given ID.
    //

    MemoryProfiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);
    GroupIndex = ListView_SetGroupInfo(MemoryProfiler, GroupId, &Group);
    if (GroupIndex == -1) {
        DbgOut("Error: failed to update the subtitle for group %d.\n", GroupId);
        Result = FALSE;
        goto UpdateMemoryPoolListViewGroupEnd;
    }

    Result = TRUE;

UpdateMemoryPoolListViewGroupEnd:
    if (GroupSubtitle == NULL) {
        LocalFree(GroupSubtitle);
    }

    return Result;
}

INT
GetMemoryPoolGroupId (
    PPROFILER_MEMORY_POOL MemoryPool
    )

/*++

Routine Description:

    This routine gets the group ID for the given memory pool. This should
    return a unique value for each pool type.

Arguments:

    MemoryPool - Supplies a pointer to the memory pool for which the group ID
        is to be returned.

Return Value:

    Returns the group ID.

--*/

{

    //
    // The group ID is simply the memory pool type.
    //

    return MemoryPool->ProfilerMemoryType;
}

BOOL
CreateMemoryPoolTagListViewItem (
    ULONG Tag,
    INT GroupId,
    PINT ItemIndex
    )

/*++

Routine Description:

    This routine creates a new item in the memory profiler's list view. The new
    item is added to the given group with the provided tag. The index of the
    item is return.

Arguments:

    Tag - Supplies the pool tag of the new memory list item.

    GroupId - Supplies the ID of the group to which this item will belong.

    ItemIndex - Supplies a pointer that receives the list view index of the new
        item.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    INT Index;
    LPTSTR ItemString;
    LVITEM ListItem;
    HWND MemoryProfiler;
    BOOL Result;

    ItemString = GetFormattedMessageA("%1!c!%2!c!%3!c!%4!c!",
                                      (UCHAR)Tag,
                                      (UCHAR)(Tag >> 8),
                                      (UCHAR)(Tag >> 16),
                                      (UCHAR)(Tag >> 24));

    if (ItemString == NULL) {
        Result = FALSE;
        goto CreateNewListItemEnd;
    }

    //
    // Initialize the new list item to set the first column text and group ID.
    //

    RtlZeroMemory(&ListItem, sizeof(LVITEM));
    ListItem.mask = LVIF_TEXT | LVIF_GROUPID;
    ListItem.iItem = INT_MAX;
    ListItem.iSubItem = 0;
    ListItem.iGroupId = GroupId;
    ListItem.pszText = ItemString;
    ListItem.cchTextMax = strlen(ItemString) + 1;

    //
    // Insert the item in the list view.
    //

    MemoryProfiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);
    Index = ListView_InsertItem(MemoryProfiler, &ListItem);
    if (Index == -1) {
        DbgOut("Error: failed to insert memory item: %s\n", ItemString);
        Result = FALSE;
        goto CreateNewListItemEnd;
    }

    //
    // Adjust the column width to make sure the new text fits.
    //

    ListView_SetColumnWidth(MemoryProfiler, 0, LVSCW_AUTOSIZE);
    *ItemIndex = Index;
    Result = TRUE;

CreateNewListItemEnd:
    if (ItemString != NULL) {
        LocalFree(ItemString);
    }

    return Result;
}

VOID
DeleteMemoryPoolTagListViewItem (
    INT ListViewIndex
    )

/*++

Routine Description:

    This routine deletes a single memory list view item at the given index.

Arguments:

    ListViewIndex - Supplies the index of the list view item that is to be
        deleted.

Return Value:

    None.

--*/

{

    HWND MemoryProfiler;

    MemoryProfiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);
    ListView_DeleteItem(MemoryProfiler, ListViewIndex);
    return;
}

BOOL
DoesMemoryPoolTagListViewItemExist (
    PPROFILER_MEMORY_POOL_TAG_STATISTIC Statistic,
    INT GroupId,
    PINT ListViewIndex
    )

/*++

Routine Description:

    This routine determines whether or not a list view item exists for the
    given tag statistic within the given group. If the list view item is found,
    then the routine returns the item's index.

Arguments:

    Statistic - Supplies a pointer to the pool tag's statistics.

    GroupId - Supplies the ID of the group to which the list item belongs.

    ListViewIndex - Supplies a pointer that receives the index of the list view
        item if it is found.

Return Value:

    Returns TRUE if a list view item does exist for the provided tag and group.
    Returns FALSE if no such list view item can be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    LVFINDINFO FindInformation;
    ULONG Index;
    LPARAM LParam;
    PMEMORY_POOL_ENTRY MemoryPoolEntry;
    HWND MemoryProfiler;
    INT PoolGroupId;
    PLIST_ENTRY PoolListHead;
    ULONG Tag;
    ULONG TagCount;
    INT ViewIndex;

    //
    // Determine which list to use. If no lists are available, then there is
    // nothing on the screen and no items exist, return FALSE.
    //

    if (MemoryDeltaListHead != NULL) {
        PoolListHead = MemoryDeltaListHead;

    } else if (MemoryPoolListHead != NULL) {
        PoolListHead = MemoryPoolListHead;

    } else {
        return FALSE;
    }

    //
    // Search through the previously displayed pool statistics for an entry
    // that has the same tag as the given statistics and the same group ID.
    //

    Tag = Statistic->Tag;
    CurrentEntry = PoolListHead->Flink;
    while (CurrentEntry != PoolListHead) {
        MemoryPoolEntry = CONTAINING_RECORD(CurrentEntry,
                                            MEMORY_POOL_ENTRY,
                                            ListEntry);

        //
        // Skip to the next memory pool if the group IDs do not match.
        //

        CurrentEntry = CurrentEntry->Flink;
        PoolGroupId = GetMemoryPoolGroupId(&(MemoryPoolEntry->MemoryPool));
        if (PoolGroupId != GroupId) {
            continue;
        }

        //
        // Search through the memory pool for the correct tag.
        //

        TagCount = MemoryPoolEntry->MemoryPool.TagCount;
        for (Index = 0; Index < TagCount; Index += 1) {

            //
            // If the tags are equal, try to find a list item with the LParam
            // for the current tag. The LParam is the pointer to the tag
            // statistic.
            //

            if (MemoryPoolEntry->TagStatistics[Index].Tag == Tag) {
                MemoryProfiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);
                LParam = (LPARAM)&(MemoryPoolEntry->TagStatistics[Index]);
                RtlZeroMemory(&FindInformation, sizeof(LVFINDINFO));
                FindInformation.flags = LVFI_PARAM;
                FindInformation.lParam = LParam;
                ViewIndex = ListView_FindItem(MemoryProfiler,
                                              -1,
                                              &FindInformation);

                if (ViewIndex == -1) {
                    return FALSE;

                } else {
                    *ListViewIndex = ViewIndex;
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

BOOL
UpdateMemoryPoolTagListViewItem (
    INT ItemIndex,
    INT GroupId,
    PPROFILER_MEMORY_POOL_TAG_STATISTIC Statistic
    )

/*++

Routine Description:

    This routine updates a memory list view item and the given index for the
    given group. The given statistics are used to update the columns for the
    item.

Arguments:

    ItemIndex - Supplies the list view index of the item.

    GroupId - Supplies the ID of the group to which the item belongs.

    Statistic - Supplies the profiler memory statistics to use for updating the
        item.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    LPTSTR Format;
    PGETCOLUMNVALUE GetColumnValueRoutine;
    ULONG Index;
    LPTSTR ItemString;
    LVITEM ListItem;
    PMEMORY_COLUMN MemoryColumn;
    HWND MemoryProfiler;
    BOOL Result;
    ULONGLONG Value;

    //
    // Zero the list item and set any fields that will not change throughout
    // the duration of this routine.
    //

    ItemString = NULL;
    RtlZeroMemory(&ListItem, sizeof(LVITEM));
    ListItem.iItem = ItemIndex;
    ListItem.iGroupId = GroupId;

    //
    // Get the profiler memory window.
    //

    MemoryProfiler = GetDlgItem(DialogWindow, IDC_MEMORY_PROFILER);

    //
    // Update the LParam for the item to point to the latest statistics. This
    // is used when sorting on column clicks.
    //

    ListItem.mask = LVIF_PARAM;
    ListItem.iSubItem = 0;
    ListItem.lParam = (LPARAM)Statistic;
    Result = ListView_SetItem(MemoryProfiler, &ListItem);
    if (Result == FALSE) {
        DbgOut("Error: failed to set LParam for pool tag %c%c%c%c.\n",
               (UCHAR)Statistic->Tag,
               (UCHAR)(Statistic->Tag >> 8),
               (UCHAR)(Statistic->Tag >> 16),
               (UCHAR)(Statistic->Tag >> 24));

        goto UpdateListViewItemEnd;
    }

    //
    // Update the value of each subitem.
    //

    ListItem.mask = LVIF_TEXT;
    for (Index = 1; Index < MEMORY_STATISTICS_COLUMN_COUNT; Index += 1) {
        MemoryColumn = &(MemoryStatisticsColumns[Index]);

        //
        // Get the new string based on the size of the column and the format.
        // Skip the column is there is no data to display.
        //

        if (MemoryDeltaModeEnabled != FALSE) {
            Format = MemoryColumn->DeltaFormat;

        } else {
            Format = MemoryColumn->Format;
        }

        GetColumnValueRoutine = MemoryColumn->GetColumnValueRoutine;
        Value = GetColumnValueRoutine(Statistic, MemoryColumn->Offset);
        if ((MemoryBaseListHead != NULL) && (Value == 0)) {
            ListViewSetItemText(MemoryProfiler, ListItem.iItem, Index, "");
            continue;
        }

        ItemString = GetFormattedMessageA(Format, Value);
        if (ItemString == NULL) {
            Result = FALSE;
            DbgOut("Error: failed to allocate string for pool tag "
                   "statistics.\n");

            goto UpdateListViewItemEnd;
        }

        //
        // Set the new string at the correct subitem index.
        //

        ListItem.iSubItem = Index;
        ListItem.pszText = ItemString;
        ListItem.cchTextMax = strlen(ItemString) + 1;
        Result = ListView_SetItem(MemoryProfiler, &ListItem);
        if (Result == FALSE) {
            DbgOut("Error: failed to insert memory subitem (%d, %d): %s\n",
                   ItemIndex,
                   Index,
                   ItemString);

            goto UpdateListViewItemEnd;
        }

        LocalFree(ItemString);
    }

UpdateListViewItemEnd:
    if (ItemString != NULL) {
        LocalFree(ItemString);
    }

    return Result;
}

INT
CALLBACK
MemoryProfilerListViewCompare (
    LPARAM LParamOne,
    LPARAM LParamTwo,
    LPARAM LParamSort
    )

/*++

Routine Description:

    This routine compares two memory profiler list view rows by the values in
    the column by which they are being sorted. It returns a negative value if
    the first parameter should be before the second, zero if they are equal,
    and a positive value if the first parameter should be after the second. It
    accounts for whether or not the sort is ascending or descending.

Arguments:

    LParamOne - Supplies the LParam value for the first list item. This is a
        pointer to the item's memory statistics.

    LParamTwo - Supplies the LParam value for the second list item. This is a
       pointer to the item's memory statistics.

    LParamSort - Supplies an LParam value for the entire sort operation. This
        is not used.

Return Value:

    Returns -1 if LParamOne should be before LParamTwo, 0 if they are equal,
    and 1 if LParamOne should be after LParamTwo.

--*/

{

    PNTDBGCOMPAREROUTINE CompareRoutine;
    PGETCOLUMNVALUE GetColumnValueRoutine;
    PMEMORY_COLUMN MemoryColumn;
    INT Result;
    PPROFILER_MEMORY_POOL_TAG_STATISTIC StatisticOne;
    PPROFILER_MEMORY_POOL_TAG_STATISTIC StatisticTwo;
    ULONGLONG ValueOne;
    ULONGLONG ValueTwo;

    assert(CurrentSortColumn < MEMORY_STATISTICS_COLUMN_COUNT);

    //
    // Compare the list view items based on the compare routine and field value
    // for the current sort column.
    //

    StatisticOne = (PPROFILER_MEMORY_POOL_TAG_STATISTIC)LParamOne;
    StatisticTwo = (PPROFILER_MEMORY_POOL_TAG_STATISTIC)LParamTwo;
    MemoryColumn = &(MemoryStatisticsColumns[CurrentSortColumn]);
    if (MemoryBaseListHead != NULL) {
        CompareRoutine = MemoryColumn->DeltaCompareRoutine;

    } else {
        CompareRoutine = MemoryColumn->CompareRoutine;
    }

    GetColumnValueRoutine = MemoryColumn->GetColumnValueRoutine;
    ValueOne = GetColumnValueRoutine(StatisticOne, MemoryColumn->Offset);
    ValueTwo = GetColumnValueRoutine(StatisticTwo, MemoryColumn->Offset);
    Result = CompareRoutine(ValueOne, ValueTwo);
    if (SortAscending == FALSE) {
        Result = 0 - Result;
    }

    return Result;
}

BOOL
TreeViewIsTreeItemVisible (
    HWND TreeViewWindow,
    HTREEITEM TreeItem
    )

/*++

Routine Description:

    This routine determines whether or not the given tree item is currently
    visible in the provided Tree View window.

Arguments:

    TreeViewWindow - Supplies a handle to a Tree View window.

    TreeItem - Supplies a handle to a tree item.

Return Value:

    Returns TRUE if the tree item is visible in the window, or FALSE otherwise.

--*/

{

    HTREEITEM FirstVisible;
    RECT FirstVisibleRect;
    INT ItemHeight;
    BOOL Result;
    RECT TreeItemRect;
    LONG VisibleBottom;
    UINT VisibleCount;

    if (TreeItem == NULL) {
        return FALSE;
    }

    //
    // If the given tree item is the first visible, then the job is simple. If
    // not, then the position the the item needs to be analyzed.
    //

    FirstVisible = TreeView_GetFirstVisible(TreeViewWindow);
    if (FirstVisible == NULL) {
        return FALSE;
    }

    if (FirstVisible == TreeItem) {
        return TRUE;
    }

    //
    // Get the current position of the first visible item, the give tree item,
    // and calculate the bottom of the visible items. Oddly,
    // TreeView_GetLastVisible does not return the last visible item. It just
    // returns the last expanded item.
    //

    ItemHeight = TreeView_GetItemHeight(TreeViewWindow);
    VisibleCount = TreeView_GetVisibleCount(TreeViewWindow);
    Result = TreeViewGetItemRect(TreeViewWindow,
                                 TreeItem,
                                 &TreeItemRect,
                                 FALSE);

    if (Result == FALSE) {
        return FALSE;
    }

    Result = TreeViewGetItemRect(TreeViewWindow,
                                 FirstVisible,
                                 &FirstVisibleRect,
                                 FALSE);

    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Compare the values to see if the given tree item is in or out of view.
    //

    VisibleBottom = (ItemHeight * VisibleCount) + FirstVisibleRect.top;
    if ((TreeItemRect.top < FirstVisibleRect.top) ||
        (TreeItemRect.bottom > VisibleBottom)) {

        return FALSE;
    }

    return TRUE;
}

BOOL
TreeViewGetItemRect (
    HWND Window,
    HTREEITEM Item,
    LPRECT Rect,
    BOOL ItemRect
    )

/*++

Routine Description:

    This routine retrieves the bounding rectangle for a tree-view item and
    indicates whether the item is visible.

Arguments:

    Window - Supplies the window handle to the tree-view control.

    Item - Supplies the handle to the tree-view item.

    Rect - Supplies a pointer to the rect structure that receives the bounding
        rectangle. The coordinates are relative to the upper-left corner of
        the tree-view control.

    ItemRect - Supplies a boolean indicating whether the bounding rectangle
        includes only the text of the item (TRUE) or the entire line the item
        occupies in the tree view (FALSE).

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    //
    // The input to the message is in the same parameter as the output
    // rectangle. Don't use the commctrl.h macro as it violates strict-aliasing
    // rules.
    //

    memcpy(Rect, Item, sizeof(HTREEITEM));
    return SendMessage(Window, TVM_GETITEMRECT, ItemRect, (LPARAM)Rect);
}

LPTSTR
GetFormattedMessageA (
     LPTSTR Message,
     ...
     )

/*++

Routine Description:

    This routine takes a formatted message string with an argument list and
    returns the expanded message string. This routine operates on ASCII
    strings.

Arguments:

    Message - Supplies a pointer to a formatted message string.

    ... - Supplies any arguments needed to conver the formatted string.

Return Value:

    Returns a pointer to the expanded message string.

--*/

{

    va_list ArgumentList;
    LPTSTR Buffer;

    Buffer = NULL;
    ArgumentList = NULL;
    va_start(ArgumentList, Message);
    FormatMessage(FORMAT_MESSAGE_FROM_STRING |
                  FORMAT_MESSAGE_ALLOCATE_BUFFER,
                  Message,
                  0,
                  0,
                  (LPTSTR)&Buffer,
                  0,
                  &ArgumentList);

    va_end(ArgumentList);
    return Buffer;
}

LPWSTR
GetFormattedMessageW (
     LPWSTR Message,
     ...
     )

/*++

Routine Description:

    This routine takes a formatted message string with an argument list and
    returns the expanded message string. This routine operates on UNICODE wide
    character strings.

Arguments:

    Message - Supplies a pointer to a formatted message string.

    ... - Supplies any arguments needed to conver the formatted string.

Return Value:

    Returns a pointer to the expanded message string.

--*/

{

    va_list ArgumentList;
    LPWSTR Buffer;

    Buffer = NULL;
    ArgumentList = NULL;
    va_start(ArgumentList, Message);
    FormatMessageW(FORMAT_MESSAGE_FROM_STRING |
                   FORMAT_MESSAGE_ALLOCATE_BUFFER,
                   Message,
                   0,
                   0,
                   (LPWSTR)&Buffer,
                   0,
                   &ArgumentList);

    va_end(ArgumentList);
    return Buffer;
}

VOID
ListViewSetItemText (
    HWND Window,
    int Item,
    int SubItem,
    LPTSTR Text
    )

/*++

Routine Description:

    This routine changes the text of a list-view item or subitem.

Arguments:

    Window - Supplies the window handle to the list-view control.

    Item - Supplies the zero-based index of the list-view item.

    SubItem - Supplies the one-based index of the subitem. To set the item
        label, supply zero here.

    Text - Supplies a pointer to a null-terminated string containing the new
        text. This parameter can be LPSTR_TEXTCALLBACK to indicate a callback
        item for which the parent window stores the text. This parameter can
        be NULL.

Return Value:

    None.

--*/

{

    ListView_SetItemText(Window, Item, SubItem, Text);
    return;
}

INT
ComparePoolTag (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    )

/*++

Routine Description:

    This routine compares two pool tag values. It is given the numeric value of
    the pool tags, converts them to a string of four characters and the
    compares them alphabeticall, ignoring case.

Arguments:

    ValueOne - Supplies the numeric value of the first pool tag to compare.

    ValueTwo - Supplies the numeric value of the second pool tag to compare.

Return Value:

    Returns -1 if ValueOne is less than ValueTwo, 0 if they are equal, and 1
    if ValueOne is greater than ValueTwo.

--*/

{

    INT Result;
    CHAR TagOne[5];
    CHAR TagTwo[5];

    sprintf(TagOne,
            "%c%c%c%c",
            (UCHAR)ValueOne,
            (UCHAR)(ValueOne >> 8),
            (UCHAR)(ValueOne >> 16),
            (UCHAR)(ValueOne >> 24));

    sprintf(TagTwo,
            "%c%c%c%c",
            (UCHAR)ValueTwo,
            (UCHAR)(ValueTwo >> 8),
            (UCHAR)(ValueTwo >> 16),
            (UCHAR)(ValueTwo >> 24));

    Result = strcasecmp(TagOne, TagTwo);
    if (Result < 0) {
        Result = -1;

    } else if (Result > 0) {
        Result = 1;

    } else {
        Result = 0;
    }

    return Result;
}

INT
CompareUlong (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    )

/*++

Routine Description:

    This routine compares two ULONG values, casting the input parameters to
    ULONGs.

Arguments:

    ValueOne - Supplies the first ULONG to compare.

    ValueTwo - Supplies the second ULONG to compare.

Return Value:

    Returns -1 if ValueOne is less than ValueTwo, 0 if they are equal, and 1
    if ValueOne is greater than ValueTwo.

--*/

{

    INT Result;

    if ((ULONG)ValueOne < (ULONG)ValueTwo) {
        Result = -1;

    } else if ((ULONG)ValueOne == (ULONG)ValueTwo) {
        Result = 0;

    } else {
        Result = 1;
    }

    return Result;
}

INT
CompareLong (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    )

/*++

Routine Description:

    This routine compares two LONG values, casting the input parameters to
    LONGs.

Arguments:

    ValueOne - Supplies the first LONG to compare.

    ValueTwo - Supplies the second LONG to compare.

Return Value:

    Returns -1 if ValueOne is less than ValueTwo, 0 if they are equal, and 1
    if ValueOne is greater than ValueTwo.

--*/

{

    INT Result;

    if ((LONG)ValueOne < (LONG)ValueTwo) {
        Result = -1;

    } else if ((LONG)ValueOne == (LONG)ValueTwo) {
        Result = 0;

    } else {
        Result = 1;
    }

    return Result;
}

INT
CompareUlonglong (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    )

/*++

Routine Description:

    This routine compares two ULONGLONG values.

Arguments:

    ValueOne - Supplies the first ULONGLONG to compare.

    ValueTwo - Supplies the second ULONGLONG to compare.

Return Value:

    Returns -1 if ValueOne is less than ValueTwo, 0 if they are equal, and 1
    if ValueOne is greater than ValueTwo.

--*/

{

    INT Result;

    if (ValueOne < ValueTwo) {
        Result = -1;

    } else if (ValueOne == ValueTwo) {
        Result = 0;

    } else {
        Result = 1;
    }

    return Result;
}

INT
CompareLonglong (
    ULONGLONG ValueOne,
    ULONGLONG ValueTwo
    )

/*++

Routine Description:

    This routine compares two LONGLONG values, casting the input parameters to
    LONGLONGs.

Arguments:

    ValueOne - Supplies the first LONGLONG to compare.

    ValueTwo - Supplies the second LONGLONG to compare.

Return Value:

    Returns -1 if ValueOne is less than ValueTwo, 0 if they are equal, and 1
    if ValueOne is greater than ValueTwo.

--*/

{

    INT Result;

    if ((LONGLONG)ValueOne < (LONGLONG)ValueTwo) {
        Result = -1;

    } else if ((LONGLONG)ValueOne == (LONGLONG)ValueTwo) {
        Result = 0;

    } else {
        Result = 1;
    }

    return Result;
}

ULONGLONG
GetUlonglongValue (
    PVOID Structure,
    ULONG Offset
    )

/*++

Routine Description:

    This routine returns a ULONGLONG value at the given offset from within the
    given structure.

Arguments:

    Structure - Supplies a pointer to the structure that contains the ULONGLONG
        value.

    Offset - Supplies the offset within the structure where the value is stored.

Return Value:

    Returns a ULONGLONG value.

--*/

{

    return *(PULONGLONG)((PBYTE)Structure + Offset);
}

ULONGLONG
GetUlongValue (
    PVOID Structure,
    ULONG Offset
    )

/*++

Routine Description:

    This routine returns a ULONG value at the given offset from within the
    given structure.

Arguments:

    Structure - Supplies a pointer to the structure that contains the ULONGLONG
        value.

    Offset - Supplies the offset within the structure where the value is stored.

Return Value:

    Returns a ULONG value.

--*/

{

    return *(PULONG)((PBYTE)Structure + Offset);
}

VOID
UiGetWindowPreferences (
    HWND Dialog
    )

/*++

Routine Description:

    This routine saves the given window's current rect information so that it
    can be written to the preferences file on exit.

Arguments:

    Dialog - Supplies the dialog window handle.

Return Value:

    None.

--*/

{

    RECT WindowRect;

    //
    // Only save the window rect if it has a non-zero height and width.
    //

    GetWindowRect(Dialog, &WindowRect);
    if ((WindowRect.left != WindowRect.right) &&
        (WindowRect.top != WindowRect.bottom)) {

        memcpy(&CurrentWindowRect, &WindowRect, sizeof(RECT));
    }

    return;
}

VOID
UiLoadPreferences (
    HWND Dialog
    )

/*++

Routine Description:

    This routine attempts to load up the previously saved debugger preferences.

Arguments:

    Dialog - Supplies the dialog window handle.

Return Value:

    None.

--*/

{

    DEBUGGER_UI_PREFERENCES Preferences;
    BOOL Result;

    Result = UiReadPreferences(&Preferences);
    if (Result == FALSE) {
        return;
    }

    if (Preferences.Version < DEBUGGER_UI_PREFERENCES_VERSION) {
        return;
    }

    if ((Preferences.WindowWidth != 0) && (Preferences.WindowHeight != 0)) {
        MainPaneXPosition = Preferences.MainPaneXPosition;
        MainPaneXPositionWidth = Preferences.MainPaneXPositionWidth;
        ProfilerPaneYPosition = Preferences.ProfilerPaneYPosition;
        ProfilerPaneYPositionHeight = Preferences.ProfilerPaneYPositionHeight;
        SetWindowPos(Dialog,
                     HWND_TOP,
                     Preferences.WindowX,
                     Preferences.WindowY,
                     Preferences.WindowWidth,
                     Preferences.WindowHeight,
                     0);

        WindowSizesInitialized = TRUE;
    }

    //
    // Save the initial UI preferences. In case the window is never moved.
    //

    UiGetWindowPreferences(Dialog);
    return;
}

VOID
UiSavePreferences (
    HWND Dialog
    )

/*++

Routine Description:

    This routine attempts to save the current UI features into the preferences
    file.

Arguments:

    Dialog - Supplies the dialog window handle.

Return Value:

    None.

--*/

{

    DEBUGGER_UI_PREFERENCES Preferences;

    memset(&Preferences, 0, sizeof(DEBUGGER_UI_PREFERENCES));
    Preferences.Version = DEBUGGER_UI_PREFERENCES_VERSION;
    Preferences.WindowX = CurrentWindowRect.left;
    Preferences.WindowY = CurrentWindowRect.top;
    Preferences.WindowWidth = CurrentWindowRect.right - CurrentWindowRect.left;
    Preferences.WindowHeight = CurrentWindowRect.bottom - CurrentWindowRect.top;
    Preferences.MainPaneXPosition = MainPaneXPosition;
    Preferences.MainPaneXPositionWidth = MainPaneXPositionWidth;
    Preferences.ProfilerPaneYPosition = ProfilerPaneYPosition;
    Preferences.ProfilerPaneYPositionHeight = ProfilerPaneYPositionHeight;
    UiWritePreferences(&Preferences);
    return;
}

BOOL
UiReadPreferences (
    PDEBUGGER_UI_PREFERENCES Preferences
    )

/*++

Routine Description:

    This routine attempts to open and read the preferences file.

Arguments:

    Preferences - Supplies a pointer where the preferences will be returned on
        success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    DWORD BytesRead;
    HANDLE File;
    BOOL Result;

    memset(Preferences, 0, sizeof(DEBUGGER_UI_PREFERENCES));
    Result = FALSE;
    File = UiOpenPreferences();
    if (File == INVALID_HANDLE_VALUE) {
        goto ReadPreferencesEnd;
    }

    Result = ReadFile(File,
                      Preferences,
                      sizeof(DEBUGGER_UI_PREFERENCES),
                      &BytesRead,
                      NULL);

    if (Result == FALSE) {
        goto ReadPreferencesEnd;
    }

ReadPreferencesEnd:
    if (File != INVALID_HANDLE_VALUE) {
        CloseHandle(File);
    }

    return Result;
}

BOOL
UiWritePreferences (
    PDEBUGGER_UI_PREFERENCES Preferences
    )

/*++

Routine Description:

    This routine attempts to open and write the preferences file.

Arguments:

    Preferences - Supplies a pointer to the preferences to write.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    DWORD BytesWritten;
    HANDLE File;
    BOOL Result;

    Result = FALSE;
    File = UiOpenPreferences();
    if (File == INVALID_HANDLE_VALUE) {
        goto WritePreferencesEnd;
    }

    Result = WriteFile(File,
                       Preferences,
                       sizeof(DEBUGGER_UI_PREFERENCES),
                       &BytesWritten,
                       NULL);

    if (Result == FALSE) {
        goto WritePreferencesEnd;
    }

WritePreferencesEnd:
    if (File != INVALID_HANDLE_VALUE) {
        CloseHandle(File);
    }

    return Result;
}

HANDLE
UiOpenPreferences (
    VOID
    )

/*++

Routine Description:

    This routine attempts to open the preferences file.

Arguments:

    None.

Return Value:

    Returns an open handle to the preferences file on success.

    INVALID_HANDLE_VALUE on failure.

--*/

{

    HANDLE File;
    TCHAR Path[MAX_PATH];
    HRESULT Result;

    Result = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, Path);
    if (!SUCCEEDED(Result)) {
        return INVALID_HANDLE_VALUE;
    }

    PathAppend(Path, TEXT("\\Minoca"));
    CreateDirectory(Path, NULL);
    PathAppend(Path, TEXT("\\DebugUI"));
    CreateDirectory(Path, NULL);
    PathAppend(Path, TEXT("prefs"));
    File = CreateFile(Path,
                      GENERIC_WRITE | GENERIC_READ,
                      0,
                      NULL,
                      OPEN_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL,
                      NULL);

    return File;
}

