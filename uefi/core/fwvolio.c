/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fwvolio.c

Abstract:

    This module implements support for firmware volume I/O.

Author:

    Evan Green 11-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "fwvolp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_FV_FILE_ATTRIBUTES
EfipFvConvertFfsAttributesToFileAttributes (
    EFI_FFS_FILE_ATTRIBUTES FfsAttributes
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the conversion between FFS alignments and FW volume alignments.
//

UINT8 EfiFvFfsAlignments[] = {0, 4, 7, 9, 10, 12, 15, 16};

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiFvGetVolumeAttributes (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    EFI_FV_ATTRIBUTES *Attributes
    )

/*++

Routine Description:

    This routine returns the attributes and current settings of the firmware
    volume. Because of constraints imposed by the underlying firmware storage,
    an instance of the Firmware Volume Protocol may not be to able to support
    all possible variations of this architecture. These constraints and the
    current state of the firmware volume are exposed to the caller using the
    get volume attributes function. The get volume attributes function is
    callable only from TPL_NOTIFY and below. Behavior of this routine at any
    EFI_TPL above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    Attributes - Supplies a pointer where the volume attributes and current
        settings are returned.

Return Value:

    EFI_SUCCESS on success.

--*/

{

    EFI_FVB_ATTRIBUTES BlockAttributes;
    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *BlockIo;
    PEFI_FIRMWARE_VOLUME Device;
    EFI_STATUS Status;

    Device = EFI_FIRMWARE_VOLUME_FROM_THIS(This);
    BlockIo = Device->BlockIo;

    //
    // Get the firmware volume block attributes, then mask out anything that's
    // irrelevant.
    //

    Status = BlockIo->GetAttributes(BlockIo, &BlockAttributes);
    BlockAttributes &= 0xFFFFF0FF;
    *Attributes = (EFI_FV_ATTRIBUTES)BlockAttributes;
    return Status;
}

EFIAPI
EFI_STATUS
EfiFvSetVolumeAttributes (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    EFI_FV_ATTRIBUTES *Attributes
    )

/*++

Routine Description:

    This routine modifies current settings of the firmware volume according to
    the input parameter. This function is used to set configurable firmware
    volume attributes. Only EFI_FV_READ_STATUS, EFI_FV_WRITE_STATUS, and
    EFI_FV_LOCK_STATUS may be modified, and then only in accordance with the
    declared capabilities. All other bits of FvAttributes are ignored on input.
    On successful return, all bits of *FvAttributes are valid and it contains
    the completed EFI_FV_ATTRIBUTES for the volume. To modify an attribute, the
    corresponding status bit in the EFI_FV_ATTRIBUTES is set to the desired
    value on input. The EFI_FV_LOCK_STATUS bit does not affect the ability to
    read or write the firmware volume. Rather, once the EFI_FV_LOCK_STATUS bit
    is set, it prevents further modification to all the attribute bits. This
    routine is callable only from TPL_NOTIFY and below. Behavior of this
    routine at any EFI_TPL above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    Attributes - Supplies a that on input contains the attributes to set. On
        output, the new current settings of the firmware volume are returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the device does not support the status bit
    settings.

    EFI_ACCESS_DENIED if the device is locked and does not allow modification.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiFvGetVolumeInfo (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    CONST EFI_GUID *InformationType,
    UINTN *BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine returns information about a firmware volume. this routine
    returns information of the requested type for the requested firmware
    volume. If the volume does not support the requested information type, then
    EFI_UNSUPPORTED is returned. If the buffer is not large enough to hold the
    requested structure, EFI_BUFFER_TOO_SMALL is returned and the buffer size
    is set to the size of buffer that is required to make the request. The
    information types defined by this specification are required information
    types that all file systems must support.

Arguments:

    This - Supplies the protocol instance.

    InformationType - Supplies a pointer to the GUID defining the type of
        information being requested.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer. On output, contains the size of the data, even if the buffer
        was too small.

    Buffer - Supplies the buffer where the information value will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the information type is not known.

    EFI_NO_MEDIA if the device has no media inserted.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_VOLUME_CORRUPTED if the file system structures are gonezo.

    EFI_BUFFER_TOO_SMALL if the supplied buffer was not large enough to hold
    the requested value. In this case the buffer size will have been updated
    to contain the needed value.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiFvSetVolumeInfo (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    CONST EFI_GUID *InformationType,
    UINTN BufferSize,
    CONST VOID *Buffer
    )

/*++

Routine Description:

    This routine sets information about a firmware volume.

Arguments:

    This - Supplies the protocol instance.

    InformationType - Supplies a pointer to the GUID defining the type of
        information being set.

    BufferSize - Supplies the size of the information value in bytes.

    Buffer - Supplies the information value.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the information type is not known.

    EFI_NO_MEDIA if the device has no media inserted.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_VOLUME_CORRUPTED if the file system structures are gonezo.

    EFI_WRITE_PROTECTED if the volume is read-only.

    EFI_BAD_BUFFER_SIZE if the buffer size was smaller than the size of the
    type indicated in the information type GUID.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiFvReadFileSection (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    CONST EFI_GUID *NameGuid,
    EFI_SECTION_TYPE SectionType,
    UINTN SectionInstance,
    VOID **Buffer,
    UINTN *BufferSize,
    UINT32 *AuthenticationStatus
    )

/*++

Routine Description:

    This routine locates the requested section within a file and returns it in
    a buffer. This routine is used to retrieve a specific section from a file
    within a firmware volume. The section returned is determined using a
    depth-first, left-to-right search algorithm through all sections found in
    the specified file. The output buffer is specified by a double indirection
    of the buffer parameter. The input value of Buffer is used to determine if
    the output buffer is caller allocated or is dynamically allocated by this
    routine. If the input value of the buffer is not NULL, it indicates that
    the output buffer is caller allocated. In this case, the input value of
    *BufferSize indicates the size of the caller-allocated output buffer. If
    the output buffer is not large enough to contain the entire requested
    output, it is filled up to the point that the output buffer is exhausted
    and EFI_WARN_BUFFER_TOO_SMALL is returned, and then BufferSize is returned
    with the size that is required to successfully complete the read. All other
    output parameters are returned with valid values. If the input value of the
    buffer is NULL, it indicates the output buffer is to be allocated by this
    routine. In this case, this routine will allocate an appropriately sized
    buffer from boot services pool memory, which will be returned in the buffer
    value. The size of the new buffer is returned in *BufferSize and all other
    output parameters are returned with valid values. This routine is callable
    only from TPL_NOTIFY and below. Behavior of this routine at any EFI_TPL
    above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    NameGuid - Supplies a pointer to a GUID containing the name of the file.
        All firmware file names are EFI_GUIDs. Each firmware volume file name
        must be unique.

    SectionType - Supplies the type of section to return.

    SectionInstance - Supplies the occurrence of the given section type to
        return.

    Buffer - Supplies a pointer to a buffer in which the file contents are
        returned, not including the file header.

    BufferSize - Supplies a pointer that on input indicates the size of the
        supplied buffer in bytes.

    AuthenticationStatus - Supplies a pointer where the file authentication
        status will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_WARN_TOO_SMALL if the buffer is too small to contain the requested
    output. The buffer is filled and truncated.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_NOT_FOUND if the requested file or section was not found in the
    firmware volume or file.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_ACCESS_DENIED if the firmware volume is configured to disallow reads.

    EFI_PROTOCOL_ERROR if the requested section was not found, but the file
    could not be fully parsed because a required
    GUIDED_SECTION_EXTRACTION_PROTOCOL was not found. The requested section
    might have been found if only it could be extracted.

--*/

{

    PEFI_FIRMWARE_VOLUME Device;
    PEFI_FFS_FILE_LIST_ENTRY FfsEntry;
    EFI_FV_FILE_ATTRIBUTES FileAttributes;
    UINT8 *FileBuffer;
    UINTN FileSize;
    EFI_FV_FILETYPE FileType;
    EFI_STATUS Status;

    if ((NameGuid == NULL) || (Buffer == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    Device = EFI_FIRMWARE_VOLUME_FROM_THIS(This);

    //
    // Read the whole file into a buffer.
    //

    FileBuffer = NULL;
    FileSize = 0;
    Status = EfiFvReadFile(This,
                           NameGuid,
                           (VOID **)&FileBuffer,
                           &FileSize,
                           &FileType,
                           &FileAttributes,
                           AuthenticationStatus);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Get the last key used by the call to read file as it is the FFS entry
    // for this file.
    //

    FfsEntry = (PEFI_FFS_FILE_LIST_ENTRY)(Device->LastKey);
    if (FileType == EFI_FV_FILETYPE_RAW) {
        Status = EFI_NOT_FOUND;
        goto FvReadSectionEnd;
    }

    if (FfsEntry->StreamHandle == 0) {
        Status = EfiFvOpenSectionStream(FileSize,
                                        FileBuffer,
                                        &(FfsEntry->StreamHandle));

        if (EFI_ERROR(Status)) {
            goto FvReadSectionEnd;
        }
    }

    //
    // If the section type is zero then the whole stream is needed.
    //

    if (SectionType == 0) {
        Status = EfiFvGetSection(FfsEntry->StreamHandle,
                                 NULL,
                                 NULL,
                                 0,
                                 Buffer,
                                 BufferSize,
                                 AuthenticationStatus,
                                 Device->IsFfs3);

    } else {
        Status = EfiFvGetSection(FfsEntry->StreamHandle,
                                 &SectionType,
                                 NULL,
                                 SectionInstance,
                                 Buffer,
                                 BufferSize,
                                 AuthenticationStatus,
                                 Device->IsFfs3);
    }

    if (!EFI_ERROR(Status)) {
        Status = Device->AuthenticationStatus;
    }

FvReadSectionEnd:
    EfiCoreFreePool(FileBuffer);
    return Status;
}

EFIAPI
EFI_STATUS
EfiFvReadFile (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    CONST EFI_GUID *NameGuid,
    VOID **Buffer,
    UINTN *BufferSize,
    EFI_FV_FILETYPE *FoundType,
    EFI_FV_FILE_ATTRIBUTES *FileAttributes,
    UINT32 *AuthenticationStatus
    )

/*++

Routine Description:

    This routine retrieves a file and/or file information from the firmware
    volume. This routine is used to retrieve any file from a firmware volume
    during the DXE phase. The actual binary encoding of the file in the
    firmware volume media may be in any arbitrary format as long as it does the
    following: It is accessed using the Firmware Volume Protocol. The image
    that is returned follows the image format defined in Code Definitions:
    PI Firmware File Format. If the input buffer is NULL, it indicates the
    caller is requesting only that the type, attributes, and size of the file
    be returned and that there is no output buffer. In this case, the following
    occurs:
    - BufferSize is returned with the size that is required to
      successfully complete the read.
    - The output parameters FoundType and *FileAttributes are
      returned with valid values.
    - The returned value of *AuthenticationStatus is undefined.

    If the input buffer is not NULL, the output buffer is specified by a double
    indirection of the Buffer parameter. The input value of *Buffer is used to
    determine if the output buffer is caller allocated or is dynamically
    allocated by this routine. If the input value of *Buffer is not NULL, it
    indicates the output buffer is caller allocated. In this case, the input
    value of *BufferSize indicates the size of the caller-allocated output
    buffer. If the output buffer is not large enough to contain the entire
    requested output, it is filled up to the point that the output buffer is
    exhausted and EFI_WARN_BUFFER_TOO_SMALL is returned, and then BufferSize is
    returned with the size required to successfully complete the read. All
    other output parameters are returned with valid values. If the input buffer
    is NULL, it indicates the output buffer is to be allocated by this routine.
    In this case, this routine will allocate an appropriately sized buffer from
    boot services pool memory, which will be returned in Buffer. The size of
    the new buffer is returned in the buffer size parameter and all other
    output parameters are returned with valid values. This routine is callable
    only from TPL_NOTIFY and below. Behavior of this routine at any EFI_TPL
    above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    NameGuid - Supplies a pointer to a GUID containing the name of the file.
        All firmware file names are EFI_GUIDs. Each firmware volume file name
        must be unique.

    Buffer - Supplies a pointer to a buffer in which the file contents are
        returned, not including the file header.

    BufferSize - Supplies a pointer that on input indicates the size of the
        supplied buffer in bytes.

    FoundType - Supplies a pointer where the file type will be returned.

    FileAttributes - Supplies a pointer where the file attributes will be
        returned.

    AuthenticationStatus - Supplies a pointer where the file authentication
        status will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_WARN_TOO_SMALL if the buffer is too small to contain the requested
    output. The buffer is filled and truncated.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_NOT_FOUND if the given name was not in the firmware volume.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_ACCESS_DENIED if the firmware volume is configured to disallow reads.

--*/

{

    PEFI_FIRMWARE_VOLUME Device;
    EFI_FFS_FILE_HEADER *FfsHeader;
    UINTN FileSize;
    UINTN InputBufferSize;
    EFI_FV_FILE_ATTRIBUTES LocalAttributes;
    EFI_FV_FILETYPE LocalFoundType;
    EFI_GUID SearchNameGuid;
    UINT8 *SourcePointer;
    EFI_STATUS Status;

    if (NameGuid == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Device = EFI_FIRMWARE_VOLUME_FROM_THIS(This);

    //
    // Keep working until the matching name GUID is found. The key is really an
    // FFS file list entry.
    //

    Device->LastKey = NULL;
    do {
        LocalFoundType = 0;
        Status = EfiFvGetNextFile(This,
                                  &(Device->LastKey),
                                  &LocalFoundType,
                                  &SearchNameGuid,
                                  &LocalAttributes,
                                  &FileSize);

        if (EFI_ERROR(Status)) {
            return EFI_NOT_FOUND;
        }

    } while (EfiCoreCompareGuids(&SearchNameGuid, (EFI_GUID *)NameGuid) ==
             FALSE);

    FfsHeader = Device->LastKey->FileHeader;
    InputBufferSize = *BufferSize;
    *FoundType = FfsHeader->Type;
    *FileAttributes = EfipFvConvertFfsAttributesToFileAttributes(
                                                        FfsHeader->Attributes);

    if ((Device->VolumeHeader->Attributes & EFI_FVB_MEMORY_MAPPED) != 0) {
        *FileAttributes |= EFI_FV_FILE_ATTRIB_MEMORY_MAPPED;
    }

    *AuthenticationStatus = 0;
    *BufferSize = FileSize;

    //
    // If the buffer is NULL, then just the information is needed.
    //

    if (Buffer == NULL) {
        return EFI_SUCCESS;
    }

    //
    // Skip over the file header.
    //

    if (EFI_IS_FFS_FILE2(FfsHeader)) {
        SourcePointer = ((UINT8 *)FfsHeader) + sizeof(EFI_FFS_FILE_HEADER2);

    } else {
        SourcePointer = ((UINT8 *)FfsHeader) + sizeof(EFI_FFS_FILE_HEADER);
    }

    Status = EFI_SUCCESS;
    if (*Buffer == NULL) {
        *Buffer = EfiCoreAllocateBootPool(FileSize);
        if (*Buffer == NULL) {
            return EFI_OUT_OF_RESOURCES;
        }

    } else if (FileSize > InputBufferSize) {
        Status = EFI_WARN_BUFFER_TOO_SMALL;
        FileSize = InputBufferSize;
    }

    EfiCoreCopyMemory(*Buffer, SourcePointer, FileSize);
    return Status;
}

EFIAPI
EFI_STATUS
EfiFvGetNextFile (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    VOID *Key,
    EFI_FV_FILETYPE *FileType,
    EFI_GUID *NameGuid,
    EFI_FV_FILE_ATTRIBUTES *Attributes,
    UINTN *Size
    )

/*++

Routine Description:

    This routine retrieves information about the next file in the firmware
    volume store that matches the search criteria. This routine is the
    interface that is used to search a firmware volume for a particular file.
    It is called successively until the desired file is located or the function
    returns EFI_NOT_FOUND. To filter uninteresting files from the output, the
    type of file to search for may be specified in FileType. For example, if the
    file type is EFI_FV_FILETYPE_DRIVER, only files of this type will be
    returned in the output. If the file type is EFI_FV_FILETYPE_ALL, no
    filtering of file types is done. The key parameter is used to indicate a
    starting point of the search. If the value of the key parameter is
    completely initialized to zero, the search re-initialized and starts at the
    beginning. Subsequent calls to this routine must maintain the value of *Key
    returned by the immediately previous call. The actual contents of *Key are
    implementation specific and no semantic content is implied. This routine is
    callable only from TPL_NOTIFY and below. Behavior of this routine at any
    EFI_TPL above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    Key - Supplies a pointer to a caller-allocated buffer containing the
        implementation-specific data used to track the current position of the
        search. The size of the buffer must be at least This->KeySize bytes
        long. To re-initialize the search and begin from the beginning of the
        firmware volume, the entire buffer must be cleared to zero. Other than
        clearing the buffer to initiate a new search, the caller must not
        modify the data in the buffer between calls to this routine.

    FileType - Supplies a pointer that on input contains the filter of file
        types to search for. If a file is found, its type will be returned
        here.

    NameGuid - Supplies a pointer where the name GUID of the file will be
        returned on success.

    Attributes - Supplies a pointer where the file attributes will be returned
        on success.

    Size - Supplies a pointer where the size of the file is returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no files of the given file type were found.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_ACCESS_DENIED if the firmware volume does not allow reads.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_FIRMWARE_VOLUME Device;
    PEFI_FFS_FILE_LIST_ENTRY FfsEntry;
    EFI_FFS_FILE_HEADER *FfsHeader;
    UINTN *KeyValue;
    EFI_STATUS Status;
    EFI_FV_ATTRIBUTES VolumeAttributes;

    Device = EFI_FIRMWARE_VOLUME_FROM_THIS(This);
    Status = EfiFvGetVolumeAttributes(This, &VolumeAttributes);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Fail if the read operation is not enabled.
    //

    if ((VolumeAttributes & EFI_FV2_READ_STATUS) == 0) {
        return EFI_ACCESS_DENIED;
    }

    if (*FileType > EFI_FV_FILETYPE_SMM_CORE) {
        return EFI_NOT_FOUND;
    }

    KeyValue = (UINTN *)Key;
    while (TRUE) {
        if (*KeyValue == 0) {
            CurrentEntry = &(Device->FfsFileList);

        } else {
            CurrentEntry = (PLIST_ENTRY)(*KeyValue);
        }

        //
        // If the next entry is the end of the list then there are no more
        // files.
        //

        if (CurrentEntry->Next == &(Device->FfsFileList)) {
            return EFI_NOT_FOUND;
        }

        FfsEntry = LIST_VALUE(CurrentEntry->Next,
                              EFI_FFS_FILE_LIST_ENTRY,
                              ListEntry);

        FfsHeader = FfsEntry->FileHeader;

        //
        // Save the key.
        //

        *KeyValue = (UINTN)&(FfsEntry->ListEntry);

        //
        // Stop if there's a match. Ignore pad files.
        //

        if (FfsHeader->Type == EFI_FV_FILETYPE_FFS_PAD) {
            continue;
        }

        if ((*FileType == EFI_FV_FILETYPE_ALL) ||
            (*FileType == FfsHeader->Type)) {

            break;
        }
    }

    //
    // Populate the return values.
    //

    *FileType = FfsHeader->Type;
    EfiCoreCopyMemory(NameGuid, &(FfsHeader->Name), sizeof(EFI_GUID));
    *Attributes = EfipFvConvertFfsAttributesToFileAttributes(
                                                        FfsHeader->Attributes);

    if ((Device->VolumeHeader->Attributes & EFI_FVB_MEMORY_MAPPED) != 0) {
        *Attributes |= EFI_FV_FILE_ATTRIB_MEMORY_MAPPED;
    }

    //
    // Subtract the size of the header.
    //

    if (EFI_IS_FFS_FILE2(FfsHeader)) {
        *Size = EFI_FFS_FILE2_SIZE(FfsHeader) - sizeof(EFI_FFS_FILE_HEADER2);

    } else {
        *Size = EFI_FFS_FILE_SIZE(FfsHeader) - sizeof(EFI_FFS_FILE_HEADER);
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiFvWriteFile (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    UINT32 NumberOfFiles,
    EFI_FV_WRITE_POLICY WritePolicy,
    EFI_FV_WRITE_FILE_DATA *FileData
    )

/*++

Routine Description:

    This routine is used to write one or more files to a firmware volume. Each
    file to be written is described by an EFI_FV_WRITE_FILE_DATA structure.
    The caller must ensure that any required alignment for all files listed in
    the file data array is compatible with the firmware volume. Firmware volume
    capabilities can be determined using the get volume attributes function.
    Similarly, if the write policy is set to EFI_FV_RELIABLE_WRITE, the caller
    must check the firmware volume capabilities to ensure EFI_FV_RELIABLE_WRITE
    is supported by the firmware volume. EFI_FV_UNRELIABLE_WRITE must always be
    supported. Writing a file with a size of zero (FileData[n].BufferSize == 0)
    deletes the file from the firmware volume if it exists. Deleting a file
    must be done one at a time. Deleting a file as part of a multiple file
    write is not allowed. This routine is callable only from TPL_NOTIFY and
    below. Behavior of this routine at any EFI_TPL above TPL_NOTIFY is
    undefined.

Arguments:

    This - Supplies the protocol instance.

    NumberOfFiles - Supplies the number of elements in the write file data
        array.

    WritePolicy - Supplies the level of reliability for the write in the event
        of a power failure or other system failure during the write operation.

    FileData - Supplies a pointer to an array of file data structures, where
        each element represents a file to be written.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_NOT_FOUND if a delete was requested but the specified file was not
    found.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_WRITE_PROTECTED if the firmware volume is configured to disallow writes.

    EFI_INVALID_PARAMETER if a delete was requested with multiple file writes,
    the write policy was unsupported, an unknown file type was specified, or a
    file system specific error occurred.

--*/

{

    return EFI_UNSUPPORTED;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_FV_FILE_ATTRIBUTES
EfipFvConvertFfsAttributesToFileAttributes (
    EFI_FFS_FILE_ATTRIBUTES FfsAttributes
    )

/*++

Routine Description:

    This routine converts FFS file attributes into Firmware Volume file
    attributes.

Arguments:

    FfsAttributes - Supplies the FFS file attributes.

Return Value:

    Returns the file attributes.

--*/

{

    UINT8 DataAlignment;
    EFI_FV_FILE_ATTRIBUTES FileAttributes;

    DataAlignment = (UINT8)((FfsAttributes & FFS_ATTRIB_DATA_ALIGNMENT) >> 3);

    ASSERT(DataAlignment < 8);

    FileAttributes = EfiFvFfsAlignments[DataAlignment];
    if ((FfsAttributes & FFS_ATTRIB_FIXED) != 0) {
        FileAttributes |= EFI_FV_FILE_ATTRIB_FIXED;
    }

    return FileAttributes;
}

