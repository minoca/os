/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fsvars.c

Abstract:

    This module implements loading EFI variables from the file system.

Author:

    Evan Green 27-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/uefi/protocol/sfilesys.h>
#include "fileinfo.h"
#include "varback.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipCoreLoadVariablesFromFileSystem (
    EFI_HANDLE Handle
    );

EFI_STATUS
EfipCoreSaveVariablesToFileSystem (
    EFI_HANDLE Handle
    );

EFI_STATUS
EfipCoreGetFileInformation (
    EFI_FILE_PROTOCOL *File,
    EFI_FILE_INFO **FileInformation,
    UINTN *FileInformationSize
    );

EFI_STATUS
EfipCoreGetVariablesFile (
    EFI_HANDLE Handle,
    BOOLEAN OpenForRead,
    EFI_FILE_PROTOCOL **File
    );

VOID
EfipSetVariablesFileVariable (
    BOOLEAN Delete
    );

//
// -------------------------------------------------------------------- Globals
//

BOOLEAN EfiFileSystemVariablesLoaded = FALSE;

EFI_GUID EfiVariableBackendProtocolGuid = EFI_VARIABLE_BACKEND_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

VOID
EfiCoreLoadVariablesFromFileSystem (
    VOID
    )

/*++

Routine Description:

    This routine loads variable data from the EFI system partition(s).

Arguments:

    None.

Return Value:

    None. Failure here is not fatal.

--*/

{

    UINTN DataSize;
    VOID *DummyValue;
    UINTN HandleCount;
    EFI_HANDLE *Handles;
    UINTN Index;
    EFI_STATUS Status;

    if (EfiFileSystemVariablesLoaded != FALSE) {
        return;
    }

    //
    // Check a variable to see if the file system variables have already been
    // loaded. This is important for allowing variables to survive a reboot.
    //

    DataSize = sizeof(DummyValue);
    Status = EfiGetVariable(L"NvVars",
                            &EfiSimpleFileSystemProtocolGuid,
                            NULL,
                            &DataSize,
                            &DummyValue);

    ASSERT(Status != EFI_BUFFER_TOO_SMALL);

    //
    // If this volatile variable is already present, then the volatile
    // variables probably survived a reboot.
    //

    if (!EFI_ERROR(Status)) {
        EfiFileSystemVariablesLoaded = TRUE;
        return;
    }

    HandleCount = 0;
    Handles = NULL;
    Status = EfiLocateHandleBuffer(ByProtocol,
                                   &EfiSimpleFileSystemProtocolGuid,
                                   NULL,
                                   &HandleCount,
                                   &Handles);

    if (EFI_ERROR(Status)) {
        return;
    }

    //
    // Loop through every handle that supports the simple file system protocol.
    //

    for (Index = 0; Index < HandleCount; Index += 1) {

        //
        // Skip any handles that are not also an EFI system partition.
        //

        Status = EfiHandleProtocol(Handles[Index],
                                   &EfiPartitionTypeSystemPartitionGuid,
                                   &DummyValue);

        if (EFI_ERROR(Status)) {
            continue;
        }

        EfipCoreLoadVariablesFromFileSystem(Handles[Index]);
    }

    if (HandleCount != 0) {
        EfiFreePool(Handles);
    }

    EfiFileSystemVariablesLoaded = TRUE;
    return;
}

VOID
EfiCoreSaveVariablesToFileSystem (
    VOID
    )

/*++

Routine Description:

    This routine saves variable data to the EFI system partition(s).

Arguments:

    None.

Return Value:

    None. Failure here is not fatal.

--*/

{

    VOID *DummyValue;
    UINTN HandleCount;
    EFI_HANDLE *Handles;
    UINTN Index;
    EFI_STATUS Status;

    HandleCount = 0;
    Handles = NULL;
    Status = EfiLocateHandleBuffer(ByProtocol,
                                   &EfiSimpleFileSystemProtocolGuid,
                                   NULL,
                                   &HandleCount,
                                   &Handles);

    if (EFI_ERROR(Status)) {
        return;
    }

    //
    // Loop through every handle that supports the simple file system protocol.
    //

    for (Index = 0; Index < HandleCount; Index += 1) {

        //
        // Skip any handles that are not also an EFI system partition.
        //

        Status = EfiHandleProtocol(Handles[Index],
                                   &EfiPartitionTypeSystemPartitionGuid,
                                   &DummyValue);

        if (EFI_ERROR(Status)) {
            continue;
        }

        EfipCoreSaveVariablesToFileSystem(Handles[Index]);
    }

    if (HandleCount != 0) {
        EfiFreePool(Handles);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipCoreLoadVariablesFromFileSystem (
    EFI_HANDLE Handle
    )

/*++

Routine Description:

    This routine loads variable data from the given file system interface
    handle.

Arguments:

    Handle - Supplies the handle that contains the simple file system interface.

Return Value:

    EFI status code.

--*/

{

    EFI_FILE_PROTOCOL *File;
    VOID *FileData;
    EFI_FILE_INFO *FileInformation;
    UINTN FileInformationSize;
    UINTN FileSize;
    UINTN HandleCount;
    EFI_HANDLE *Handles;
    EFI_STATUS Status;
    EFI_VARIABLE_BACKEND_PROTOCOL *VariableBackend;

    File = NULL;
    FileData = NULL;
    FileInformation = NULL;
    Handles = NULL;
    Status = EfipCoreGetVariablesFile(Handle, TRUE, &File);
    if (EFI_ERROR(Status)) {
        goto CoreLoadVariablesFromFileSystemEnd;
    }

    //
    // Get the file information.
    //

    Status = EfipCoreGetFileInformation(File,
                                        &FileInformation,
                                        &FileInformationSize);

    if (EFI_ERROR(Status)) {
        goto CoreLoadVariablesFromFileSystemEnd;
    }

    //
    // Skip it if it's a directory, that's not right.
    //

    if ((FileInformation->Attribute & EFI_FILE_DIRECTORY) != 0) {
        Status = EFI_NOT_FOUND;
        goto CoreLoadVariablesFromFileSystemEnd;
    }

    //
    // Allocate data for the file contents, and read the contents in.
    //

    FileSize = FileInformation->FileSize;
    FileData = EfiCoreAllocateBootPool(FileSize);
    if (FileData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CoreLoadVariablesFromFileSystemEnd;
    }

    Status = File->Read(File, &FileSize, FileData);
    if (EFI_ERROR(Status)) {
        goto CoreLoadVariablesFromFileSystemEnd;
    }

    //
    // Open up the variable backend protocol.
    //

    HandleCount = 0;
    Status = EfiLocateHandleBuffer(ByProtocol,
                                   &EfiVariableBackendProtocolGuid,
                                   NULL,
                                   &HandleCount,
                                   &Handles);

    if (EFI_ERROR(Status)) {
        goto CoreLoadVariablesFromFileSystemEnd;
    }

    if (HandleCount != 0) {
        Status = EfiHandleProtocol(Handles[0],
                                   &EfiVariableBackendProtocolGuid,
                                   (VOID **)&VariableBackend);

        if (EFI_ERROR(Status)) {
            goto CoreLoadVariablesFromFileSystemEnd;
        }
    }

    //
    // Add these variables to the current EFI variables via the backend
    // protocol.
    //

    Status = VariableBackend->SetData(VariableBackend,
                                      FileData,
                                      FileSize,
                                      FALSE);

    if (EFI_ERROR(Status)) {
        goto CoreLoadVariablesFromFileSystemEnd;
    }

    EfipSetVariablesFileVariable(FALSE);

CoreLoadVariablesFromFileSystemEnd:
    if (FileInformation != NULL) {
        EfiFreePool(FileInformation);
    }

    if (File != NULL) {
        File->Close(File);
    }

    if (Handles != NULL) {
        EfiFreePool(Handles);
    }

    if (FileData != NULL) {
        EfiFreePool(FileData);
    }

    return Status;
}

EFI_STATUS
EfipCoreSaveVariablesToFileSystem (
    EFI_HANDLE Handle
    )

/*++

Routine Description:

    This routine saves variable data to the given file system interface
    handle.

Arguments:

    Handle - Supplies the handle that contains the simple file system interface.

Return Value:

    EFI status code.

--*/

{

    EFI_FILE_PROTOCOL *File;
    VOID *FileData;
    UINTN FileSize;
    UINTN HandleCount;
    EFI_HANDLE *Handles;
    EFI_STATUS Status;
    EFI_VARIABLE_BACKEND_PROTOCOL *VariableBackend;

    //
    // Open up the file.
    //

    File = NULL;
    FileData = NULL;
    Handles = NULL;
    Status = EfipCoreGetVariablesFile(Handle, FALSE, &File);
    if (EFI_ERROR(Status)) {
        goto CoreWriteVariablesToFileSystemEnd;
    }

    //
    // Open up the variable backend protocol.
    //

    HandleCount = 0;
    Status = EfiLocateHandleBuffer(ByProtocol,
                                   &EfiVariableBackendProtocolGuid,
                                   NULL,
                                   &HandleCount,
                                   &Handles);

    if (EFI_ERROR(Status)) {
        goto CoreWriteVariablesToFileSystemEnd;
    }

    if (HandleCount != 0) {
        Status = EfiHandleProtocol(Handles[0],
                                   &EfiVariableBackendProtocolGuid,
                                   (VOID **)&VariableBackend);

        if (EFI_ERROR(Status)) {
            goto CoreWriteVariablesToFileSystemEnd;
        }
    }

    //
    // Get the current variable data
    //

    FileData = NULL;
    FileSize = 0;
    Status = VariableBackend->GetData(VariableBackend,
                                      &FileData,
                                      &FileSize);

    if (EFI_ERROR(Status)) {
        goto CoreWriteVariablesToFileSystemEnd;
    }

    //
    // Try to write it out.
    //

    Status = File->Write(File, &FileSize, FileData);
    if (EFI_ERROR(Status)) {
        goto CoreWriteVariablesToFileSystemEnd;
    }

CoreWriteVariablesToFileSystemEnd:
    if (Handles != NULL) {
        EfiFreePool(Handles);
    }

    if (File != NULL) {
        File->Close(File);
    }

    return Status;
}

EFI_STATUS
EfipCoreGetFileInformation (
    EFI_FILE_PROTOCOL *File,
    EFI_FILE_INFO **FileInformation,
    UINTN *FileInformationSize
    )

/*++

Routine Description:

    This routine returns the file information, allocated from pool.

Arguments:

    File - Supplies the open file protocol instance.

    FileInformation - Supplies a pointer where a pointer to the file
        information will be returned on success. The caller is responsible
        for freeing this buffer.

    FileInformationSize - Supplies a pointer where the size of the file
        information will be returned on success.

Return Value:

    EFI status code.

--*/

{

    EFI_FILE_INFO *Information;
    UINTN InformationSize;
    EFI_STATUS Status;

    Information = NULL;
    InformationSize = 0;
    Status = File->GetInfo(File,
                           &EfiFileInformationGuid,
                           &InformationSize,
                           NULL);

    if (Status == EFI_BUFFER_TOO_SMALL) {
        Information = EfiCoreAllocateBootPool(InformationSize);
        if (Information == NULL) {
            return EFI_OUT_OF_RESOURCES;
        }

        EfiSetMem(Information, InformationSize, 0);
        Status = File->GetInfo(File,
                               &EfiFileInformationGuid,
                               &InformationSize,
                               Information);

        if (EFI_ERROR(Status)) {
            EfiFreePool(Information);
            InformationSize = 0;
        }
    }

    *FileInformation = Information;
    *FileInformationSize = InformationSize;
    return Status;
}

EFI_STATUS
EfipCoreGetVariablesFile (
    EFI_HANDLE Handle,
    BOOLEAN OpenForRead,
    EFI_FILE_PROTOCOL **File
    )

/*++

Routine Description:

    This routine opens the variables file for reading or writing.

Arguments:

    Handle - Supplies the handle that supports the simple file system protocol.

    OpenForRead - Supplies a boolean indicating whether to open the file for
        reading (TRUE) or writing (FALSE).

    File - Supplies a pointer where the opened file protocol will be returned.

Return Value:

    EFI status code.

--*/

{

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    UINT64 OpenMode;
    EFI_FILE_PROTOCOL *Root;
    EFI_STATUS Status;

    Status = EfiHandleProtocol(Handle,
                               &EfiSimpleFileSystemProtocolGuid,
                               (VOID **)&FileSystem);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (OpenForRead != FALSE) {
        OpenMode = EFI_FILE_MODE_READ;

    //
    // If opening the file to write, first open and delete it.
    //

    } else {
        OpenMode = EFI_FILE_MODE_WRITE;
    }

    Status = Root->Open(Root,
                        File,
                        L"EFI\\NvVars",
                        OpenMode,
                        0);

    //
    // If opening for write, delete the file if it opened successfully, and
    // then reopen with create. Deleting closes the file handle too.
    //

    if (OpenForRead == FALSE) {
        if (!EFI_ERROR(Status)) {
            (*File)->Delete(*File);
        }

        OpenMode = EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ |
                   EFI_FILE_MODE_WRITE;

        Status = Root->Open(Root,
                            File,
                            L"EFI\\NvVars",
                            OpenMode,
                            0);
    }

    Root->Close(Root);
    return Status;
}

VOID
EfipSetVariablesFileVariable (
    BOOLEAN Delete
    )

/*++

Routine Description:

    This routine sets a volatile variable to indicate that variables have been
    loaded from a file. This way if variables survive a reboot, they won't be
    smashed by older data from the file sysetm later.

Arguments:

    Delete - Supplies a boolean indicating whether to delete or set the
        variable.

Return Value:

    None.

--*/

{

    UINT32 Attributes;
    VOID *DataPointer;
    VOID *DummyData;
    UINTN Size;

    Attributes = EFI_VARIABLE_NON_VOLATILE |
                 EFI_VARIABLE_BOOTSERVICE_ACCESS |
                 EFI_VARIABLE_RUNTIME_ACCESS;

    DummyData = NULL;
    if (Delete != FALSE) {
        DataPointer = NULL;
        Size = 0;

    } else {
        DataPointer = &DummyData;
        Size = sizeof(DummyData);
    }

    EfiSetVariable(L"NvVars",
                   &EfiSimpleFileSystemProtocolGuid,
                   Attributes,
                   Size,
                   DataPointer);

    return;
}

