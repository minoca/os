/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sedutil.c

Abstract:

    This module implements utility functions for the sed utility.

Author:

    Evan Green 11-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sed.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SED_READ_BLOCK_SIZE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PSED_STRING
SedReadFileIn (
    PSTR Path,
    BOOL MustSucceed
    )

/*++

Routine Description:

    This routine reads a file into a null terminated sed string.

Arguments:

    Path - Supplies a pointer to a string containing the path of the file to
        read in.

    MustSucceed - Supplies a boolean indicating whether or not the open and
        read calls must succeed, or should return a partial or empty string
        on failure.

Return Value:

    Returns a pointer to the string containing the contents of the file on
    success.

    NULL on failure.

--*/

{

    PCHAR Buffer;
    ssize_t BytesRead;
    FILE *File;
    BOOL Result;
    PSED_STRING String;

    Buffer = NULL;
    File = NULL;
    Result = FALSE;
    String = SedCreateString(NULL, 0, TRUE);
    if (String == NULL) {
        goto ReadFileInEnd;
    }

    Buffer = malloc(SED_READ_BLOCK_SIZE);
    if (Buffer == NULL) {
        goto ReadFileInEnd;
    }

    File = fopen(Path, "r");
    if (File == NULL) {
        if (MustSucceed == FALSE) {
            Result = TRUE;
        }

        goto ReadFileInEnd;
    }

    while (TRUE) {
        do {
            BytesRead = fread(Buffer, 1, SED_READ_BLOCK_SIZE, File);

        } while ((BytesRead == 0) && (errno == EINTR));

        if (BytesRead == 0) {
            if (ferror(File) != 0) {
                Result = FALSE;

            } else if (feof(File) == 0) {
                Result = FALSE;

            } else {
                Result = TRUE;
            }

            break;
        }

        Result = SedAppendString(String, Buffer, BytesRead);
        if (Result == FALSE) {
            goto ReadFileInEnd;
        }
    }

    Result = TRUE;

ReadFileInEnd:
    if (File != NULL) {
        fclose(File);
    }

    if (Buffer != NULL) {
        free(Buffer);
    }

    if (Result == FALSE) {
        if (String == NULL) {
            SedDestroyString(String);
            String = NULL;
        }
    }

    return String;
}

PSED_STRING
SedCreateString (
    PSTR Data,
    UINTN Size,
    BOOL NullTerminate
    )

/*++

Routine Description:

    This routine creates a sed string.

Arguments:

    Data - Supplies an optional pointer to the initial data. This data will be
        copied.

    Size - Supplies the size of the initial data. Supply 0 if no data was
        supplied.

    NullTerminate - Supplies a boolean indicating if the string should be
        null terminated if it is not already.

Return Value:

    Returns a pointer to the allocated string structure on success. The caller
    is responsible for destroying this string structure.

    NULL on allocation failure.

--*/

{

    UINTN Capacity;
    UINTN NeededSize;
    BOOL Result;
    PSED_STRING String;

    Result = FALSE;
    String = malloc(sizeof(SED_STRING));
    if (String == NULL) {
        goto CreateStringEnd;
    }

    memset(String, 0, sizeof(SED_STRING));
    if ((NullTerminate != FALSE) && (Size == 0)) {
        String->Data = malloc(SED_INITIAL_STRING_SIZE);
        if (String->Data == NULL) {
            goto CreateStringEnd;
        }

        String->Capacity = SED_INITIAL_STRING_SIZE;
        String->Size = 1;
        String->Data[0] = '\0';

    } else if ((Data != NULL) && (Size != 0)) {
        NeededSize = Size;
        if ((NullTerminate != FALSE) && (Data[Size - 1] != '\0')) {
            NeededSize += 1;
        }

        Capacity = SED_INITIAL_STRING_SIZE;
        while (Capacity < NeededSize) {
            Capacity *= 2;
        }

        String->Data = malloc(Capacity);
        if (String->Data == NULL) {
            goto CreateStringEnd;
        }

        memcpy(String->Data, Data, Size);
        if ((NullTerminate != FALSE) && (Data[Size - 1] != '\0')) {
            String->Data[Size] = '\0';
        }

        String->Size = NeededSize;
        String->Capacity = Capacity;
    }

    Result = TRUE;

CreateStringEnd:
    if (Result == FALSE) {
        if (String != NULL) {
            if (String->Data != NULL) {
                free(String->Data);
            }

            free(String);
            String = NULL;
        }
    }

    return String;
}

BOOL
SedAppendString (
    PSED_STRING String,
    PSTR Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine appends a string of characters to the given string. If the
    original string was null terminated, the resulting string will also be
    null terminated on success.

Arguments:

    String - Supplies a pointer to the string to append to.

    Data - Supplies a pointer to the bytes to append.

    Size - Supplies the number of bytes to append.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINTN Index;
    PSTR NewBuffer;
    BOOL NullTerminated;
    BOOL Result;

    NullTerminated = FALSE;
    if ((String->Size != 0) && (String->Data[String->Size - 1] == '\0')) {
        NullTerminated = TRUE;
        String->Size -= 1;
    }

    //
    // Reallocate the buffer if needed.
    //

    if (String->Size + Size + 1 >= String->Capacity) {
        if (String->Capacity == 0) {
            String->Capacity = SED_INITIAL_STRING_SIZE;
        }

        while (String->Size + Size >= String->Capacity) {
            String->Capacity *= 2;
        }

        NewBuffer = realloc(String->Data, String->Capacity);
        if (NewBuffer == NULL) {
            Result = FALSE;
            goto AppendStringEnd;
        }

        String->Data = NewBuffer;
    }

    for (Index = 0; Index < Size; Index += 1) {
        String->Data[String->Size] = Data[Index];
        String->Size += 1;
    }

    if (NullTerminated != FALSE) {
        if (String->Data[String->Size - 1] != '\0') {
            String->Data[String->Size] = '\0';
            String->Size += 1;
        }
    }

    Result = TRUE;

AppendStringEnd:
    return Result;
}

VOID
SedDestroyString (
    PSED_STRING String
    )

/*++

Routine Description:

    This routine destroys a sed string structure.

Arguments:

    String - Supplies a pointer to the string to destroy.

Return Value:

    None.

--*/

{

    if (String->Data != NULL) {
        free(String->Data);
    }

    String->Data = NULL;
    String->Capacity = 0;
    String->Size = 0;
    free(String);
    return;
}

INT
SedOpenWriteFile (
    PSED_CONTEXT Context,
    PSED_STRING Path,
    PSED_WRITE_FILE *WriteFile
    )

/*++

Routine Description:

    This routine opens up a write file, sharing descriptors between
    duplicate write file names.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to the string containing the path of the write
        file.

    WriteFile - Supplies a pointer where the pointer to the write file will
        be returned on success.

Return Value:

    0 on success.

    Error code on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSED_WRITE_FILE CurrentFile;
    PSED_WRITE_FILE NewFile;
    INT Result;

    *WriteFile = NULL;

    //
    // Look to see if this file is already opened, and return it if so.
    //

    CurrentEntry = Context->WriteFileList.Next;
    while (CurrentEntry != &(Context->WriteFileList)) {
        CurrentFile = LIST_VALUE(CurrentEntry, SED_WRITE_FILE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (strcmp(CurrentFile->Name->Data, Path->Data) == 0) {
            *WriteFile = CurrentFile;
            return 0;
        }
    }

    //
    // Allocate a new structure.
    //

    NewFile = malloc(sizeof(SED_WRITE_FILE));
    if (NewFile == NULL) {
        Result = ENOMEM;
        goto OpenWriteFileEnd;
    }

    memset(NewFile, 0, sizeof(SED_WRITE_FILE));
    NewFile->LineTerminated = TRUE;
    NewFile->Name = SedCreateString(Path->Data, Path->Size, TRUE);
    if (NewFile->Name == NULL) {
        Result = ENOMEM;
        goto OpenWriteFileEnd;
    }

    //
    // Open up the write file.
    //

    NewFile->File = fopen(NewFile->Name->Data, "w");
    if (NewFile->File == NULL) {
        Result = errno;
        SwPrintError(Result, NewFile->Name->Data, "Unable to open write file");
        goto OpenWriteFileEnd;
    }

    //
    // Add it to the global list.
    //

    INSERT_BEFORE(&(NewFile->ListEntry), &(Context->WriteFileList));
    Result = 0;

OpenWriteFileEnd:
    if (Result != 0) {
        if (NewFile != NULL) {
            if (NewFile->File != NULL) {
                fclose(NewFile->File);
            }

            if (NewFile->Name != NULL) {
                SedDestroyString(NewFile->Name);
            }

            free(NewFile);
            NewFile = NULL;
        }
    }

    *WriteFile = NewFile;
    return Result;
}

INT
SedPrint (
    PSED_CONTEXT Context,
    PSTR String,
    INT LineTerminator
    )

/*++

Routine Description:

    This routine prints a null terminated string to standard out.

Arguments:

    Context - Supplies a pointer to the application context.

    String - Supplies the null terminated string to print.

    LineTerminator - Supplies the character that terminates this line. If this
        is EOF, then that tells this routine the line is not terminated.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    size_t Size;

    Size = strlen(String);
    return SedWrite(&(Context->StandardOut), String, Size, LineTerminator);
}

INT
SedWrite (
    PSED_WRITE_FILE WriteFile,
    PVOID Buffer,
    UINTN Size,
    INT LineTerminator
    )

/*++

Routine Description:

    This routine write the given buffer out to the given file descriptor.

Arguments:

    WriteFile - Supplies a pointer to the file to write to.

    Buffer - Supplies the buffer to write.

    Size - Supplies the number of characters in the buffer.

    LineTerminator - Supplies the character that terminates this line. If this
        is EOF, then that tells this routine the line is not terminated.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    size_t BytesThisRound;
    ssize_t BytesWritten;
    INT Result;
    UINTN TotalBytesWritten;

    assert(WriteFile->File != NULL);

    //
    // If the previous line written wasn't terminated, terminate it now.
    //

    if (WriteFile->LineTerminated == FALSE) {
        fputc('\n', WriteFile->File);
        WriteFile->LineTerminated = TRUE;
    }

    //
    // Writing anything resets the terminator status.
    //

    if (Size != 0) {
        WriteFile->LineTerminated = FALSE;
    }

    //
    // Write the stuff.
    //

    Result = 0;
    TotalBytesWritten = 0;
    while (TotalBytesWritten < Size) {
        if (Size - TotalBytesWritten > MAX_LONG) {
            BytesThisRound = MAX_LONG;

        } else {
            BytesThisRound = Size - TotalBytesWritten;
        }

        do {
            BytesWritten = fwrite(Buffer + TotalBytesWritten,
                                  1,
                                  BytesThisRound,
                                  WriteFile->File);

        } while ((BytesWritten <= 0) && (errno == EINTR));

        if (BytesWritten <= 0) {
            Result = errno;
            SwPrintError(Result, NULL, "Could not write to file");
            return Result;
        }

        TotalBytesWritten += BytesWritten;
    }

    //
    // If there is a terminating character, write it out. But only mark the
    // line as terminated if it's a newline so if anything else comes in a
    // newline will get written.
    //

    if (LineTerminator != EOF) {
        fputc(LineTerminator, WriteFile->File);
        if (LineTerminator == '\n') {
            WriteFile->LineTerminated = TRUE;
        }
    }

    return 0;
}

