/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    glob.c

Abstract:

    This module implements the glob function, which expands a pattern out to
    valid path names.

Author:

    Evan Green 10-Feb-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <glob.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

//
// --------------------------------------------------------------------- Macros
//

#define GLOB_CHARACTER(_Value) ((_Value) & GLOB_META_CHARACTER_MASK)
#define GLOB_MAKE_META(_Character) (CHAR)((_Character) | GLOB_META_QUOTE)

#define GLOB_IS_META(_Character) (((_Character) & GLOB_META_QUOTE) != 0)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define some limits.
//

#define GLOB_MAX_BRACE 128
#define GLOB_MAX_PATH 65536
#define GLOB_MAX_READDIR 16384
#define GLOB_MAX_STAT 1024
#define GLOB_MAX_STRING 65536

//
// Encode the meta qualities in the character.
//

#define GLOB_META_QUOTE 0x80
#define GLOB_META_PROTECT 0x40
#define GLOB_META_CHARACTER_MASK 0x7F

#define GLOB_META_ALL GLOB_MAKE_META('*')
#define GLOB_META_END GLOB_MAKE_META(']')
#define GLOB_META_NOT GLOB_MAKE_META('!')
#define GLOB_META_ONE GLOB_MAKE_META('?')
#define GLOB_META_RANGE GLOB_MAKE_META('-')
#define GLOB_META_SET GLOB_MAKE_META('[')

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the current counts towards the limit of a glob
    operation.

Members:

    BraceCount - Stores the number of braces expanded.

    PathLimit - Stores the number of path elements.

    ReadCount - Stores the number of calls to readdir.

    StatCount - Stores the number of calls to stat.

    StringCount - Stores the number of strings.

--*/

typedef struct _GLOB_COUNT {
    UINTN BraceCount;
    UINTN PathLimit;
    UINTN ReadCount;
    UINTN StatCount;
    UINTN StringCount;
} GLOB_COUNT, *PGLOB_COUNT;

//
// ----------------------------------------------- Internal Function Prototypes
//

int
ClpGlobExpandBraces (
    const char *Pattern,
    glob_t *Glob,
    PGLOB_COUNT Limit
    );

int
ClpGlobExpandBrace (
    const char *Brace,
    const char *Pattern,
    glob_t *Glob,
    int *ReturnValue,
    PGLOB_COUNT Limit
    );

int
ClpGlob (
    const char *Pattern,
    glob_t *Glob,
    PGLOB_COUNT Limit
    );

int
ClpGlobExecute (
    char *Pattern,
    glob_t *Glob,
    PGLOB_COUNT Limit
    );

int
ClpGlobExecuteRecursive (
    char *PathBuffer,
    char *PathEnd,
    char *PathBufferEnd,
    char *Pattern,
    glob_t *Glob,
    PGLOB_COUNT Limit
    );

int
ClpGlobSearch (
    char *PathBuffer,
    char *PathEnd,
    char *PathBufferEnd,
    char *Pattern,
    char *PatternRemainder,
    glob_t *Glob,
    PGLOB_COUNT Limit
    );

BOOL
ClpGlobMatch (
    char *Name,
    char *Pattern,
    char *PatternEnd
    );

const char *
ClpGlobTilde (
    const char *Pattern,
    char *PathBuffer,
    size_t PathBufferSize,
    glob_t *Glob
    );

int
ClpGlobExtend (
    const char *Path,
    glob_t *Glob,
    PGLOB_COUNT Limit
    );

int
ClpGlobCompareEntries (
    const void *FirstEntry,
    const void *SecondEntry
    );

int
ClpGlobConvertString (
    const char *String,
    char *Buffer,
    size_t BufferSize
    );

DIR *
ClpGlobOpenDirectory (
    char *Path,
    glob_t *Glob
    );

int
ClpGlobLstat (
    char *Path,
    struct stat *Stat,
    glob_t *Glob
    );

int
ClpGlobStat (
    char *Path,
    struct stat *Stat,
    glob_t *Glob
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
glob (
    const char *Pattern,
    int Flags,
    int (*ErrorFunction) (const char *, int),
    glob_t *Glob
    )

/*++

Routine Description:

    This routine is a pathname generator that will expand a pattern out to all
    matching path names.

Arguments:

    Pattern - Supplies a null terminated string containing the pattern to
        match.

    Flags - Supplies a bitfield of flags governing the operation. See GLOB_*
        definitions.

    ErrorFunction - Supplies an optional pointer to an error function that is
        called if a directory cannot be read. It receives the path that failed,
        and the error number set by the operation. If this routine returns
        non-zero, the GLOB_ERR flag is set in the flags, and this routine stops
        and returns GLOB_ABORTED after setting gl_pathc and gl_pathv to
        reflect the paths already scanned. If the routine returns 0, the error
        is ignored.

    Glob - Supplies a pointer to the state where paths are returned.

Return Value:

    0 on success. The gl_pathc and gl_pathv members will be filled out with the
    number of matches.

    Returns one of the GLOB_* return values on failure.

--*/

{

    char *BufferEnd;
    char *CurrentBuffer;
    const char *CurrentPattern;
    GLOB_COUNT Limit;
    char PatternBuffer[MAXPATHLEN];
    char Protection;

    memset(&Limit, 0, sizeof(GLOB_COUNT));

    //
    // Initialize the state unless appending.
    //

    if ((Flags & GLOB_APPEND) == 0) {
        Glob->gl_pathc = 0;
        Glob->gl_pathv = NULL;
        if ((Flags & GLOB_DOOFFS) == 0) {
            Glob->gl_offs = 0;
        }
    }

    if ((Flags & GLOB_LIMIT) != 0) {
        Limit.PathLimit = Glob->gl_matchc;
        if (Limit.PathLimit == 0) {
            Limit.PathLimit = GLOB_MAX_PATH;
        }
    }

    Glob->gl_flags = Flags & (~GLOB_MAGCHAR);
    Glob->gl_errfunc = ErrorFunction;
    Glob->gl_matchc = 0;

    //
    // Unescape the buffer.
    //

    CurrentPattern = Pattern;
    CurrentBuffer = PatternBuffer;
    BufferEnd = PatternBuffer + sizeof(PatternBuffer) - 1;
    if ((Flags & GLOB_NOESCAPE) != 0) {
        while (BufferEnd - CurrentBuffer >= 1) {
            *CurrentBuffer = *CurrentPattern;
            if (*CurrentPattern == '\0') {
                break;
            }

            CurrentPattern += 1;
            CurrentBuffer += 1;
        }

    } else {
        while (BufferEnd - CurrentBuffer >= 1) {
            if (*CurrentPattern == '\\') {
                CurrentPattern += 1;
                if (*CurrentPattern == '\0') {
                    *CurrentBuffer = '\\' | GLOB_META_PROTECT;
                    CurrentBuffer += 1;
                    continue;
                }

                Protection = GLOB_META_PROTECT;

            } else {
                Protection = 0;
            }

            *CurrentBuffer = *CurrentPattern | Protection;
            CurrentBuffer += 1;
            if (*CurrentPattern == '\0') {
                break;
            }

            CurrentPattern += 1;
        }
    }

    if ((Flags & GLOB_BRACE) != 0) {
        return ClpGlobExpandBraces(PatternBuffer, Glob, &Limit);
    }

    return ClpGlob(PatternBuffer, Glob, &Limit);
}

LIBC_API
void
globfree (
    glob_t *Glob
    )

/*++

Routine Description:

    This routine frees allocated data inside of a glob state structure.

Arguments:

    Glob - Supplies a pointer to the state to free.

Return Value:

    None.

--*/

{

    size_t Index;

    if (Glob->gl_pathv != NULL) {
        for (Index = Glob->gl_offs; Index < Glob->gl_pathc; Index += 1) {
            if (Glob->gl_pathv[Index] != NULL) {
                free(Glob->gl_pathv[Index]);
            }
        }

        free(Glob->gl_pathv);
        Glob->gl_pathv = NULL;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

int
ClpGlobExpandBraces (
    const char *Pattern,
    glob_t *Glob,
    PGLOB_COUNT Limit
    )

/*++

Routine Description:

    This routine recursively expands a glob pattern with many curly braces.

Arguments:

    Pattern - Supplies a null terminated string containing the pattern to
        match.

    Glob - Supplies a pointer to the state where paths are returned.

    Limit - Supplies the current counts.

Return Value:

    0 on success. The gl_pathc and gl_pathv members will be filled out with the
    number of matches.

    Returns one of the GLOB_* return values on failure.

--*/

{

    const char *CurrentPattern;
    int Result;
    int ReturnValue;

    CurrentPattern = Pattern;
    if ((Glob->gl_flags & GLOB_LIMIT) != 0) {
        Limit->BraceCount += 1;
        if (Limit->BraceCount >= GLOB_MAX_BRACE) {
            errno = 0;
            return GLOB_NOSPACE;
        }
    }

    //
    // If it's just {} and that's it, treat it normally.
    //

    if ((Pattern[0] == '{') && (Pattern[1] == '}') && (Pattern[2] == '\0')) {
        return ClpGlob(Pattern, Glob, Limit);
    }

    //
    // Loop expanding braces.
    //

    while (TRUE) {
        CurrentPattern = strchr(CurrentPattern, '{');
        if (CurrentPattern == NULL) {
            break;
        }

        Result = ClpGlobExpandBrace(CurrentPattern,
                                    Pattern,
                                    Glob,
                                    &ReturnValue,
                                    Limit);

        if (Result == 0) {
            return ReturnValue;
        }
    }

    return ClpGlob(Pattern, Glob, Limit);
}

int
ClpGlobExpandBrace (
    const char *Brace,
    const char *Pattern,
    glob_t *Glob,
    int *ReturnValue,
    PGLOB_COUNT Limit
    )

/*++

Routine Description:

    This routine recursively expands a glob pattern with a single curly brace
    expression.

Arguments:

    Brace - Supplies a pointer where the brace begins.

    Pattern - Supplies a null terminated string containing the pattern to
        match.

    Glob - Supplies a pointer to the state where paths are returned.

    ReturnValue - Supplies a pointer where the return value of the operation
        will be returned on success.

    Limit - Supplies the current counts.

Return Value:

    0 on success. The gl_pathc and gl_pathv members will be filled out with the
    number of matches.

    Returns one of the GLOB_* return values on failure.

--*/

{

    size_t BraceLevel;
    char *CurrentBuffer;
    const char *CurrentPattern;
    const char *End;
    char PatternBuffer[MAXPATHLEN];
    const char *PatternCopy;
    const char *Search;
    char *Start;

    //
    // Copy everything up to the first brace.
    //

    CurrentBuffer = PatternBuffer;
    CurrentPattern = Pattern;
    while (CurrentPattern != Brace) {
        *CurrentBuffer = *CurrentPattern;
        CurrentBuffer += 1;
        CurrentPattern += 1;
    }

    *CurrentBuffer = '\0';
    Start = CurrentBuffer;

    //
    // Find the corresponding brace.
    //

    BraceLevel = 0;
    Brace += 1;
    End = Brace;
    while (*End != '\0') {

        //
        // Ignore characters between square brackets.
        //

        if (*End == '[') {
            End += 1;
            Search = End;
            while ((*Search != ']') && (*Search != '\0')) {
                Search += 1;
            }

            if (*Search != '\0') {
                End = Search;
            }

        } else if (*End == '{') {
            BraceLevel += 1;

        } else if (*End == '}') {
            if (BraceLevel == 0) {
                break;
            }

            BraceLevel -= 1;
        }

        End += 1;
    }

    //
    // If the braces didn't match, just glob the pattern regularly.
    //

    if ((BraceLevel != 0) || (*End == '\0')) {
        *ReturnValue = ClpGlob(PatternBuffer, Glob, Limit);
        return 0;
    }

    PatternCopy = Brace;
    CurrentPattern = Brace;
    while (CurrentPattern <= End) {
        switch (*CurrentPattern) {

        //
        // Ignore characters between square brackets.
        //

        case '[':
            CurrentPattern += 1;
            Search = CurrentPattern;
            while ((*Search != ']') && (*Search != '\0')) {
                Search += 1;
            }

            if (*Search != '\0') {
                CurrentPattern = Search;
            }

            break;

        case '{':
            BraceLevel += 1;
            break;

        case '}':
            if (BraceLevel != 0) {
                BraceLevel -= 1;
                break;
            }

            //
            // Fall through.
            //

        case ',':
            if ((BraceLevel != 0) && (*CurrentPattern == ',')) {
                break;
            }

            //
            // Append the current string option.
            //

            CurrentBuffer = Start;
            while (PatternCopy < CurrentPattern) {
                *CurrentBuffer = *PatternCopy;
                CurrentBuffer += 1;
                PatternCopy += 1;
            }

            //
            // Append the remainder of the pattern after the closing curly.
            //

            PatternCopy = End + 1;
            while (TRUE) {
                *CurrentBuffer = *PatternCopy;
                if (*PatternCopy == '\0') {
                    break;
                }

                CurrentBuffer += 1;
                PatternCopy += 1;
            }

            *ReturnValue = ClpGlobExpandBraces(PatternBuffer, Glob, Limit);

            //
            // Advance beyond the comma.
            //

            PatternCopy = CurrentPattern + 1;
            break;

        default:
            break;
        }

        CurrentPattern += 1;
    }

    *ReturnValue = 0;
    return 0;
}

int
ClpGlob (
    const char *Pattern,
    glob_t *Glob,
    PGLOB_COUNT Limit
    )

/*++

Routine Description:

    This routine expands a glob pattern.

Arguments:

    Pattern - Supplies a null terminated string containing the pattern to
        match.

    Glob - Supplies a pointer to the state where paths are returned.

    Limit - Supplies the current counts.

Return Value:

    0 on success. The gl_pathc and gl_pathv members will be filled out with the
    number of matches.

    Returns one of the GLOB_* return values on failure.

--*/

{

    char Character;
    char *CurrentBuffer;
    const char *CurrentPattern;
    int Error;
    size_t OriginalPathCount;
    char PatternBuffer[MAXPATHLEN];

    CurrentPattern = ClpGlobTilde(Pattern,
                                  PatternBuffer,
                                  sizeof(PatternBuffer),
                                  Glob);

    OriginalPathCount = Glob->gl_pathc;
    CurrentBuffer = PatternBuffer;
    while (TRUE) {
        Character = *CurrentPattern;
        CurrentPattern += 1;
        if (Character == '\0') {
            break;
        }

        switch (Character) {
        case '[':
            Character = *CurrentPattern;
            if (Character == '!') {
                CurrentPattern += 1;
            }

            if ((*CurrentPattern == '\0') ||
                (strchr(CurrentPattern + 1, ']') == NULL)) {

                *CurrentBuffer = '[';
                CurrentBuffer += 1;
                if (Character == '!') {
                    CurrentPattern -= 1;
                }

                break;
            }

            *CurrentBuffer = GLOB_META_SET;
            CurrentBuffer += 1;
            if (Character == '!') {
                *CurrentBuffer = GLOB_META_NOT;
                CurrentBuffer += 1;
            }

            Character = *CurrentPattern;
            CurrentPattern += 1;
            do {
                *CurrentBuffer = GLOB_CHARACTER(Character);
                CurrentBuffer += 1;
                if (*CurrentPattern == '-') {
                    Character = CurrentPattern[1];
                    if (Character != ']') {
                        *CurrentBuffer = GLOB_META_RANGE;
                        CurrentBuffer += 1;
                        *CurrentBuffer = GLOB_CHARACTER(Character);
                        CurrentBuffer += 1;
                        CurrentPattern += 2;
                    }
                }

                Character = *CurrentPattern;
                CurrentPattern += 1;

            } while (Character != ']');

            Glob->gl_flags |= GLOB_MAGCHAR;
            *CurrentBuffer = GLOB_META_END;
            CurrentBuffer += 1;
            break;

        case '?':
            Glob->gl_flags |= GLOB_MAGCHAR;
            *CurrentBuffer = GLOB_META_ONE;
            CurrentBuffer += 1;
            break;

        case '*':
            Glob->gl_flags |= GLOB_MAGCHAR;

            //
            // Collapse multiple asterisks into a single one.
            //

            if ((CurrentBuffer == PatternBuffer) ||
                (*(CurrentBuffer - 1) != GLOB_META_ALL)) {

                *CurrentBuffer = GLOB_META_ALL;
                CurrentBuffer += 1;
            }

            break;

        default:
            *CurrentBuffer = GLOB_CHARACTER(Character);
            CurrentBuffer += 1;
            break;
        }
    }

    *CurrentBuffer = '\0';
    Error = ClpGlobExecute(PatternBuffer, Glob, Limit);
    if (Error != 0) {
        return Error;
    }

    //
    // If there was no match, potentially append the pattern.
    //

    if (Glob->gl_pathc == OriginalPathCount) {
        if (((Glob->gl_flags & GLOB_NOCHECK) != 0) ||
            (((Glob->gl_flags & GLOB_NOMAGIC) != 0) &&
             ((Glob->gl_flags & GLOB_MAGCHAR) == 0))) {

            return ClpGlobExtend(Pattern, Glob, Limit);

        } else {
            return GLOB_NOMATCH;
        }
    }

    //
    // Sort the results if desired.
    //

    if ((Glob->gl_flags & GLOB_NOSORT) == 0) {
        qsort(Glob->gl_pathv + Glob->gl_offs + OriginalPathCount,
              Glob->gl_pathc - OriginalPathCount,
              sizeof(char *),
              ClpGlobCompareEntries);
    }

    return 0;
}

int
ClpGlobExecute (
    char *Pattern,
    glob_t *Glob,
    PGLOB_COUNT Limit
    )

/*++

Routine Description:

    This routine executes a glob search on an encoded pattern.

Arguments:

    Pattern - Supplies a null terminated string containing the pattern to
        match.

    Glob - Supplies a pointer to the state where paths are returned.

    Limit - Supplies the current counts.

Return Value:

    0 on success. The gl_pathc and gl_pathv members will be filled out with the
    number of matches.

    Returns one of the GLOB_* return values on failure.

--*/

{

    char PathBuffer[MAXPATHLEN];
    int Result;

    if (*Pattern == '\0') {
        return 0;
    }

    Result = ClpGlobExecuteRecursive(PathBuffer,
                                     PathBuffer,
                                     PathBuffer + MAXPATHLEN - 1,
                                     Pattern,
                                     Glob,
                                     Limit);

    return Result;
}

int
ClpGlobExecuteRecursive (
    char *PathBuffer,
    char *PathEnd,
    char *PathBufferEnd,
    char *Pattern,
    glob_t *Glob,
    PGLOB_COUNT Limit
    )

/*++

Routine Description:

    This routine executes the recursive innner function that executes a glob
    search on an encoded pattern.

Arguments:

    PathBuffer - Supplies a pointer to the encoded path.

    PathEnd - Supplies the current end of the encoded path string.

    PathBufferEnd - Supplies a pointer to the end of the whole path buffer.

    Pattern - Supplies a null terminated string containing the pattern to
        match.

    Glob - Supplies a pointer to the state where paths are returned.

    Limit - Supplies the current counts.

Return Value:

    0 on success. The gl_pathc and gl_pathv members will be filled out with the
    number of matches.

    Returns one of the GLOB_* return values on failure.

--*/

{

    BOOL AnyMetaPresent;
    char *CurrentPath;
    char *CurrentPattern;
    int Result;
    struct stat Stat;

    AnyMetaPresent = 0;
    while (TRUE) {
        if (*Pattern == '\0') {
            *PathEnd = '\0';
            if (ClpGlobLstat(PathBuffer, &Stat, Glob) != 0) {
                return 0;
            }

            if ((Glob->gl_flags & GLOB_LIMIT) != 0) {
                Limit->StatCount += 1;
                if (Limit->StatCount >= GLOB_MAX_STAT) {
                    errno = 0;
                    if (PathEnd + 1 > PathBufferEnd) {
                        return GLOB_ABORTED;
                    }

                    *PathEnd = '/';
                    PathEnd += 1;
                    *PathEnd = '\0';
                    return GLOB_NOSPACE;
                }
            }

            if (((Glob->gl_flags & GLOB_MARK) != 0) &&
                (*(PathEnd - 1) != '/') &&
                ((S_ISDIR(Stat.st_mode) != 0) ||
                 ((S_ISLNK(Stat.st_mode) != 0) &&
                  (ClpGlobStat(PathBuffer, &Stat, Glob) == 0) &&
                  (S_ISDIR(Stat.st_mode) != 0)))) {

                if (PathEnd + 1 > PathBufferEnd) {
                    return GLOB_ABORTED;
                }

                *PathEnd = '/';
                PathEnd += 1;
                *PathEnd = '\0';
            }

            Glob->gl_matchc += 1;
            return ClpGlobExtend(PathBuffer, Glob, Limit);
        }

        //
        // Copy the pattern to the path, watching for meta characters.
        //

        CurrentPath = PathEnd;
        CurrentPattern = Pattern;
        while ((*CurrentPattern != '\0') && (*CurrentPattern != '/')) {
            if (GLOB_IS_META(*CurrentPattern)) {
                AnyMetaPresent = TRUE;
            }

            if (CurrentPath + 1 > PathBufferEnd) {
                return GLOB_ABORTED;
            }

            *CurrentPath = *CurrentPattern;
            CurrentPath += 1;
            CurrentPattern += 1;
        }

        //
        // If no meta characters were found, loop around and do the next
        // segment.
        //

        if (AnyMetaPresent == FALSE) {
            PathEnd = CurrentPath;
            Pattern = CurrentPattern;
            while (*Pattern == '/') {
                if (PathEnd + 1 > PathBufferEnd) {
                    return GLOB_ABORTED;
                }

                *PathEnd = *Pattern;
                PathEnd += 1;
                Pattern += 1;
            }

        //
        // Search the directory for something matching the pattern.
        //

        } else {
            Result = ClpGlobSearch(PathBuffer,
                                   PathEnd,
                                   PathBufferEnd,
                                   Pattern,
                                   CurrentPattern,
                                   Glob,
                                   Limit);

            return Result;
        }
    }

    //
    // This code is never run.
    //

    assert(FALSE);

    return GLOB_ABORTED;
}

int
ClpGlobSearch (
    char *PathBuffer,
    char *PathEnd,
    char *PathBufferEnd,
    char *Pattern,
    char *PatternRemainder,
    glob_t *Glob,
    PGLOB_COUNT Limit
    )

/*++

Routine Description:

    This routine searches a directory for a path that matches the given
    pattern containing meta characters. This routine is recursive, as it
    calls the execute recursive function, which calls this.

Arguments:

    PathBuffer - Supplies a pointer to the encoded path.

    PathEnd - Supplies the current end of the encoded path string.

    PathBufferEnd - Supplies a pointer to the end of the whole path buffer.

    Pattern - Supplies a null terminated string containing the pattern to
        match.

    PatternRemainder - Supplies a pointer to the end of the pattern portion to
        match now.

    Glob - Supplies a pointer to the state where paths are returned.

    Limit - Supplies the current counts.

Return Value:

    0 on success. The gl_pathc and gl_pathv members will be filled out with the
    number of matches.

    Returns one of the GLOB_* return values on failure.

--*/

{

    char Buffer[MAXPATHLEN];
    char *CurrentEntry;
    char *CurrentPath;
    DIR *Directory;
    struct dirent *Entry;
    int Error;

    if (PathEnd > PathBufferEnd) {
        return GLOB_ABORTED;
    }

    *PathEnd = '\0';
    errno = 0;
    Directory = ClpGlobOpenDirectory(PathBuffer, Glob);
    if (Directory == NULL) {
        if (Glob->gl_errfunc != NULL) {
            if (ClpGlobConvertString(PathBuffer, Buffer, sizeof(Buffer)) != 0) {
                return GLOB_ABORTED;
            }

            if ((Glob->gl_errfunc(Buffer, errno) != 0) ||
                ((Glob->gl_flags & GLOB_ERR) != 0)) {

                return GLOB_ABORTED;
            }
        }

        return 0;
    }

    Error = 0;
    while (TRUE) {
        if ((Glob->gl_flags & GLOB_ALTDIRFUNC) != 0) {
            Entry = Glob->gl_readdir(Directory);

        } else {
            Entry = readdir(Directory);
        }

        if (Entry == NULL) {
            break;
        }

        if ((Glob->gl_flags & GLOB_LIMIT) != 0) {
            Limit->ReadCount += 1;
            if (Limit->ReadCount >= GLOB_MAX_READDIR) {
                errno = 0;
                if (PathEnd + 1 > PathBufferEnd) {
                    Error = GLOB_ABORTED;

                } else {
                    *PathEnd = '/';
                    PathEnd += 1;
                    *PathEnd = '\0';
                    Error = GLOB_NOSPACE;
                }

                break;
            }
        }

        //
        // An initial dot must be handled literally.
        //

        if ((Entry->d_name[0] == '.') && (*Pattern != '.')) {
            continue;
        }

        CurrentPath = PathEnd;
        CurrentEntry = Entry->d_name;
        while (CurrentPath < PathBufferEnd) {
            *CurrentPath = *CurrentEntry;
            CurrentPath += 1;
            if (*CurrentEntry == '\0') {
                break;
            }

            CurrentEntry += 1;
        }

        if (ClpGlobMatch(PathEnd, Pattern, PatternRemainder) == FALSE) {
            *PathEnd = '\0';
            continue;
        }

        Error = ClpGlobExecuteRecursive(PathBuffer,
                                        CurrentPath - 1,
                                        PathBufferEnd,
                                        PatternRemainder,
                                        Glob,
                                        Limit);

        if (Error != 0) {
            break;
        }
    }

    if ((Glob->gl_flags & GLOB_ALTDIRFUNC) != 0) {
        Glob->gl_closedir(Directory);

    } else {
        closedir(Directory);
    }

    return Error;
}

BOOL
ClpGlobMatch (
    char *Name,
    char *Pattern,
    char *PatternEnd
    )

/*++

Routine Description:

    This routine determines if the given name matches the given pattern. This
    function is recursive.

Arguments:

    Name - Supplies a pointer to the name to match.

    Pattern - Supplies a pointer to the pattern to match against.

    PatternEnd - Supplies a pointer to one beyond the last valid character in
        the pattern.

Return Value:

    FALSE if the pattern does not match.

    TRUE if the pattern matches.

--*/

{

    BOOL Found;
    char NameCharacter;
    BOOL Negated;
    char PatternCharacter;

    while (Pattern < PatternEnd) {
        PatternCharacter = *Pattern;
        Pattern += 1;
        switch (PatternCharacter) {
        case GLOB_META_ALL:
            if (Pattern == PatternEnd) {
                return TRUE;
            }

            while (TRUE) {
                if (ClpGlobMatch(Name, Pattern, PatternEnd) != FALSE) {
                    return TRUE;
                }

                if (*Name == '\0') {
                    break;
                }

                Name += 1;
            }

            return FALSE;

        case GLOB_META_ONE:
            if (*Name == '\0') {
                return FALSE;
            }

            Name += 1;
            break;

        case GLOB_META_SET:
            Found = FALSE;
            NameCharacter = *Name;
            Name += 1;
            if (NameCharacter == '\0') {
                return FALSE;
            }

            Negated = FALSE;
            if (*Pattern == GLOB_META_NOT) {
                Negated = TRUE;
                Pattern += 1;
            }

            while (TRUE) {
                PatternCharacter = *Pattern;
                Pattern += 1;
                if (PatternCharacter == GLOB_META_END) {
                    break;
                }

                if (*Pattern == GLOB_META_RANGE) {
                    if ((GLOB_CHARACTER(NameCharacter) >=
                         GLOB_CHARACTER(PatternCharacter)) &&
                        (GLOB_CHARACTER(NameCharacter) <=
                         GLOB_CHARACTER(*(Pattern + 1)))) {

                        Found = TRUE;
                    }

                    Pattern += 1;

                } else if (NameCharacter == PatternCharacter) {
                    Found = TRUE;
                }
            }

            if (Found == Negated) {
                return FALSE;
            }

            break;

        default:
            if (*Name != PatternCharacter) {
                return FALSE;
            }

            Name += 1;
            break;
        }
    }

    if (*Name == '\0') {
        return TRUE;
    }

    return FALSE;
}

const char *
ClpGlobTilde (
    const char *Pattern,
    char *PathBuffer,
    size_t PathBufferSize,
    glob_t *Glob
    )

/*++

Routine Description:

    This routine expands a tilde into a user's home directory.

Arguments:

    Pattern - Supplies a pointer to the tilde pattern to expand.

    PathBuffer - Supplies the buffer where the expanded home directory will be
        returned.

    PathBufferSize - Supplies the size of the path buffer in bytes.

    Glob - Supplies the glob state.

Return Value:

    Returns a pointer to the advanced pattern.

--*/

{

    char *BufferEnd;
    char *CurrentPath;
    const char *CurrentPattern;
    char *Home;
    struct passwd *Information;
    char *User;

    if ((*Pattern != '~') || ((Glob->gl_flags & GLOB_TILDE) == 0)) {
        return Pattern;
    }

    //
    // Copy to the end of the string or the first slash.
    //

    BufferEnd = &(PathBuffer[PathBufferSize - 1]);
    CurrentPattern = Pattern + 1;
    CurrentPath = PathBuffer;
    while ((CurrentPath < BufferEnd) && (*CurrentPattern != '\0') &&
           (*CurrentPattern != '/')) {

        *CurrentPath = *CurrentPattern;
        CurrentPath += 1;
        CurrentPattern += 1;
    }

    *CurrentPath = '\0';

    //
    // If it's ~ or ~/, then first try expanding HOME, but only if not setuid
    // or setgid. Then try the password file.
    //

    if (*PathBuffer == '\0') {
        Home = getenv("HOME");
        if ((Home == NULL) ||
            (getuid() != geteuid()) ||
            (getgid() != getegid())) {

            User = getlogin();
            if (User != NULL) {
                Information = getpwnam(User);
                if (Information != NULL) {
                    Home = Information->pw_dir;
                }
            }

            if (Home == NULL) {
                return Pattern;
            }
        }

    //
    // Lookup ~user to get their home directory.
    //

    } else {
        Information = getpwnam(PathBuffer);
        if (Information == NULL) {
            return Pattern;
        }

        Home = Information->pw_dir;
    }

    //
    // Add the home directory.
    //

    CurrentPath = PathBuffer;
    while ((CurrentPath < BufferEnd) && (*Home != '\0')) {
        *CurrentPath = *Home;
        CurrentPath += 1;
        Home += 1;
    }

    //
    // Add the rest of the pattern.
    //

    while (CurrentPath < BufferEnd) {
        *CurrentPath = *CurrentPattern;
        if (*CurrentPattern == '\0') {
            break;
        }

        CurrentPath += 1;
        CurrentPattern += 1;
    }

    *CurrentPath = '\0';
    return CurrentPath;
}

int
ClpGlobExtend (
    const char *Path,
    glob_t *Glob,
    PGLOB_COUNT Limit
    )

/*++

Routine Description:

    This routine adds an element to the glob path array.

Arguments:

    Path - Supplies a pointer to the path to add. A copy of this buffer will be
        made.

    Glob - Supplies a pointer to the glob state where the new array member
        will be added.

    Limit - Supplies a pointer to the glob count information.

Return Value:

    0 on success.

    GLOB_NOSPACE on error.

--*/

{

    char *Copy;
    const char *CurrentPath;
    size_t Index;
    size_t Length;
    char **NewPathArray;
    size_t NewSize;

    if (((Glob->gl_flags & GLOB_LIMIT) != 0) &&
        (Glob->gl_matchc > Limit->PathLimit)) {

        errno = 0;
        return GLOB_NOSPACE;
    }

    NewSize = sizeof(*NewPathArray) * (2 + Glob->gl_pathc + Glob->gl_offs);
    NewPathArray = realloc(Glob->gl_pathv, NewSize);
    if (NewPathArray == NULL) {
        return GLOB_NOSPACE;
    }

    //
    // For the first allocation, clear out the initial empty elements.
    //

    if ((Glob->gl_pathv == NULL) && (Glob->gl_offs > 0)) {
        for (Index = 0; Index < Glob->gl_offs; Index += 1) {
            NewPathArray[Index] = NULL;
        }
    }

    Glob->gl_pathv = NewPathArray;
    CurrentPath = Path;
    while (*CurrentPath != '\0') {
        CurrentPath += 1;
    }

    Length = (size_t)(CurrentPath - Path) + 1;
    Limit->StringCount += Length;
    if (((Glob->gl_flags & GLOB_LIMIT) != 0) &&
        (Limit->StringCount >= GLOB_MAX_STRING)) {

        errno = 0;
        return GLOB_NOSPACE;
    }

    Copy = malloc(Length);
    if (Copy != NULL) {
        if (ClpGlobConvertString(Path, Copy, Length) != 0) {
            free(Copy);
            return GLOB_NOSPACE;
        }

        Glob->gl_pathv[Glob->gl_offs + Glob->gl_pathc] = Copy;
        Glob->gl_pathc += 1;
    }

    Glob->gl_pathv[Glob->gl_offs + Glob->gl_pathc] = NULL;
    if (Copy == NULL) {
        return GLOB_NOSPACE;
    }

    return 0;
}

int
ClpGlobCompareEntries (
    const void *FirstEntry,
    const void *SecondEntry
    )

/*++

Routine Description:

    This routine compares two path array elements, with a prototype suitable
    for passing to the qsort routine.

Arguments:

    FirstEntry - Supplies a pointer to the first element, in this case a
        pointer to a pointer to a string.

    SecondEntry - Supplies a pointer to the second element, in this case a
        pointer to a pointer to a string.

Return Value:

    Less than 0 if the first entry is less than the second.

    0 if the two strings are equal.

    Greater than 0 if the first entry is greater than the second.

--*/

{

    return strcmp(*((char **)FirstEntry), *((char **)SecondEntry));
}

int
ClpGlobConvertString (
    const char *String,
    char *Buffer,
    size_t BufferSize
    )

/*++

Routine Description:

    This routine converts a glob string buffer into a suitable form to be
    returned.

Arguments:

    String - Supplies the internal glob string.

    Buffer - Supplies a pointer where the output will be returned.

    BufferSize - Supplies the size of the output buffer in bytes.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    while (BufferSize >= 1) {
        *Buffer = *String;
        if (*String == '\0') {
            return 0;
        }

        String += 1;
        Buffer += 1;
        BufferSize -= 1;
    }

    return 1;
}

DIR *
ClpGlobOpenDirectory (
    char *Path,
    glob_t *Glob
    )

/*++

Routine Description:

    This routine opens a directory for the glob functions.

Arguments:

    Path - Supplies a pointer to the path to open.

    Glob - Supplies a pointer to the glob state.

Return Value:

    Returns a pointer to an open directory on success.

    NULL on failure.

--*/

{

    char Buffer[MAXPATHLEN];

    if (*Path == '\0') {
        strcpy(Buffer, ".");

    } else {
        if (ClpGlobConvertString(Path, Buffer, sizeof(Buffer)) != 0) {
            return NULL;
        }
    }

    if ((Glob->gl_flags & GLOB_ALTDIRFUNC) != 0) {
        return Glob->gl_opendir(Buffer);
    }

    return opendir(Buffer);
}

int
ClpGlobLstat (
    char *Path,
    struct stat *Stat,
    glob_t *Glob
    )

/*++

Routine Description:

    This routine gets the file information for a given path for the glob
    functions, not following a final symbolic link.

Arguments:

    Path - Supplies a pointer to the path to get information for.

    Stat - Supplies a pointer where the file information will be returned on
        success.

    Glob - Supplies a pointer to the glob state.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    char Buffer[MAXPATHLEN];

    if (ClpGlobConvertString(Path, Buffer, sizeof(Buffer)) != 0) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if ((Glob->gl_flags & GLOB_ALTDIRFUNC) != 0) {
        return Glob->gl_lstat(Buffer, Stat);
    }

    return lstat(Buffer, Stat);
}

int
ClpGlobStat (
    char *Path,
    struct stat *Stat,
    glob_t *Glob
    )

/*++

Routine Description:

    This routine gets the file information for a given path for the glob
    functions, following a final symbolic link.

Arguments:

    Path - Supplies a pointer to the path to get information for.

    Stat - Supplies a pointer where the file information will be returned on
        success.

    Glob - Supplies a pointer to the glob state.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    char Buffer[MAXPATHLEN];

    if (ClpGlobConvertString(Path, Buffer, sizeof(Buffer)) != 0) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if ((Glob->gl_flags & GLOB_ALTDIRFUNC) != 0) {
        return Glob->gl_stat(Buffer, Stat);
    }

    return stat(Buffer, Stat);
}

