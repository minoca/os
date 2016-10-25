/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fwvol.c

Abstract:

    This module implements UEFI core protocol support for firmware volumes.

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

EFIAPI
VOID
EfipFvBlockNotify (
    EFI_EVENT Event,
    VOID *Context
    );

EFI_STATUS
EfipFvCheck (
    PEFI_FIRMWARE_VOLUME Device
    );

VOID
EfipFvFreeDeviceResource (
    PEFI_FIRMWARE_VOLUME Volume
    );

BOOLEAN
EfipFvIsBufferErased (
    UINT8 ErasePolarity,
    VOID *Buffer,
    UINTN BufferSize
    );

BOOLEAN
EfipFvIsValidFfsHeader (
    UINT8 ErasePolarity,
    EFI_FFS_FILE_HEADER *FfsHeader,
    EFI_FFS_FILE_STATE *FileState
    );

BOOLEAN
EfipFvIsValidFfsFile (
    UINT8 ErasePolarity,
    EFI_FFS_FILE_HEADER *FfsHeader
    );

EFI_FFS_FILE_STATE
EfipFvGetFileState (
    UINT8 ErasePolarity,
    EFI_FFS_FILE_HEADER *FfsHeader
    );

BOOLEAN
EfipFvVerifyFileHeaderChecksum (
    EFI_FFS_FILE_HEADER *FfsHeader
    );

UINT16
EfipFvCalculateSum16 (
    UINT16 *Buffer,
    UINTN Size
    );

UINT8
EfipFvCalculateChecksum8 (
    UINT8 *Buffer,
    UINTN Size
    );

UINT8
EfipFvCalculateSum8 (
    UINT8 *Buffer,
    UINTN Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Keep protocol notify related globals.
//

VOID *EfiFvBlockNotifyRegistration;
EFI_EVENT EfiFvBlockEvent;

EFI_GUID EfiFirmwareFileSystem2Guid = EFI_FIRMWARE_FILE_SYSTEM2_GUID;
EFI_GUID EfiFirmwareFileSystem3Guid = EFI_FIRMWARE_FILE_SYSTEM3_GUID;
EFI_GUID EfiFirmwareVolume2ProtocolGuid = EFI_FIRMWARE_VOLUME2_PROTOCOL_GUID;

//
// Store the template for firmware volumes.
//

EFI_FIRMWARE_VOLUME EfiFirmwareVolumeTemplate = {
    EFI_FIRMWARE_VOLUME_MAGIC,
    NULL,
    NULL,
    {
        EfiFvGetVolumeAttributes,
        EfiFvSetVolumeAttributes,
        EfiFvReadFile,
        EfiFvReadFileSection,
        EfiFvWriteFile,
        EfiFvGetNextFile,
        sizeof(UINTN),
        NULL,
        EfiFvGetVolumeInfo,
        EfiFvSetVolumeInfo,
    },

    NULL,
    NULL,
    NULL,
    NULL,
    {NULL, NULL},
    0,
    FALSE,
    0
};

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiFvDriverInit (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine initializes support for UEFI firmware volumes.

Arguments:

    ImageHandle - Supplies the image handle for this driver. This is probably
        the firmware core image handle.

    SystemTable - Supplies a pointer to the system table.

Return Value:

    EFI status code.

--*/

{

    //
    // Sign up to be notified whenever a new firmware volume block device
    // protocol crops up.
    //

    EfiFvBlockEvent = EfiCoreCreateProtocolNotifyEvent(
                                           &EfiFirmwareVolumeBlockProtocolGuid,
                                           TPL_CALLBACK,
                                           EfipFvBlockNotify,
                                           NULL,
                                           &EfiFvBlockNotifyRegistration);

    ASSERT(EfiFvBlockEvent != NULL);

    return EFI_SUCCESS;
}

EFI_STATUS
EfiFvGetVolumeHeader (
    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *BlockProtocol,
    EFI_FIRMWARE_VOLUME_HEADER **Header
    )

/*++

Routine Description:

    This routine returns the firmware volume header of the volume represented
    by the given block I/O interface.

Arguments:

    BlockProtocol - Supplies an instance of the block I/O protocol.

    Header - Supplies a pointer where a pointer to the volume header allocated
        from pool will be returned on success.

Return Value:

    EFI status code.

--*/

{

    UINT8 *Buffer;
    UINTN HeaderLength;
    EFI_FIRMWARE_VOLUME_HEADER LocalHeader;
    UINTN Offset;
    EFI_LBA StartLba;
    EFI_STATUS Status;

    //
    // Read the standard firmware volume header.
    //

    *Header = NULL;
    StartLba = 0;
    Offset = 0;
    HeaderLength = sizeof(EFI_FIRMWARE_VOLUME_HEADER);
    Status = EfiFvReadData(BlockProtocol,
                           &StartLba,
                           &Offset,
                           HeaderLength,
                           (UINT8 *)&LocalHeader);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (LocalHeader.HeaderLength < sizeof(EFI_FIRMWARE_VOLUME_HEADER)) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Allocate a buffer for the header.
    //

    *Header = EfiCoreAllocateBootPool(LocalHeader.HeaderLength);
    if (*Header == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    EfiCopyMem(*Header, &LocalHeader, sizeof(EFI_FIRMWARE_VOLUME_HEADER));

    //
    // Read the rest of the header.
    //

    HeaderLength = LocalHeader.HeaderLength -
                   sizeof(EFI_FIRMWARE_VOLUME_HEADER);

    Buffer = (UINT8 *)*Header + sizeof(EFI_FIRMWARE_VOLUME_HEADER);
    Status = EfiFvReadData(BlockProtocol,
                           &StartLba,
                           &Offset,
                           HeaderLength,
                           Buffer);

    if (EFI_ERROR(Status)) {
        EfiCoreFreePool(*Header);
        *Header = NULL;
    }

    return Status;
}

EFI_STATUS
EfiFvReadData (
    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *BlockProtocol,
    EFI_LBA *StartLba,
    UINTN *Offset,
    UINTN DataSize,
    UINT8 *Data
    )

/*++

Routine Description:

    This routine reads data from the firmware volume represented by the given
    block I/O interface. This data may span multiple block ranges.

Arguments:

    BlockProtocol - Supplies an instance of the block I/O protocol.

    StartLba - Supplies a pointer that on input contains the logical block
        address to read from. On output, this will contain the logical block
        address after reading.

    Offset - Supplies a pointer that on input contains the offset within the
        block to start reading. On output, the offset into the block after
        reading will be returned.

    DataSize - Supplies the size of the data to read in bytes.

    Data - Supplies a pointer where the read data will be returned.

Return Value:

    EFI status code.

--*/

{

    UINTN BlockIndex;
    UINTN BlockSize;
    UINTN NumberOfBlocks;
    UINTN ReadDataSize;
    EFI_STATUS Status;

    //
    // Try to read data in the current block.
    //

    BlockIndex = 0;
    ReadDataSize = DataSize;
    Status = BlockProtocol->Read(BlockProtocol,
                                 *StartLba,
                                 *Offset,
                                 &ReadDataSize,
                                 Data);

    if (Status == EFI_SUCCESS) {
        *Offset += DataSize;
        return EFI_SUCCESS;

    } else if (Status != EFI_BAD_BUFFER_SIZE) {
        return Status;
    }

    //
    // The read cross block boundaries, so read data from the next block.
    //

    DataSize -= ReadDataSize;
    Data += ReadDataSize;
    *StartLba += 1;
    while (DataSize > 0) {
        Status = BlockProtocol->GetBlockSize(BlockProtocol,
                                             *StartLba,
                                             &BlockSize,
                                             &NumberOfBlocks);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        //
        // Read data now that a block boundary was just crossed.
        //

        BlockIndex = 0;
        while ((BlockIndex < NumberOfBlocks) && (DataSize >= BlockSize)) {
            Status = BlockProtocol->Read(BlockProtocol,
                                         *StartLba + BlockIndex,
                                         0,
                                         &BlockSize,
                                         Data);

            if (EFI_ERROR(Status)) {
                return Status;
            }

            Data += BlockSize;
            DataSize -= BlockSize;
            BlockIndex += 1;
        }

        //
        // If data doesn't exceed the block range, there's no need to loop
        // back around.
        //

        if (DataSize < BlockSize) {
            break;
        }

        //
        // Request block size information from the next range.
        //

        *StartLba += NumberOfBlocks;
    }

    //
    // Read the last partial block.
    //

    if (DataSize > 0) {
        Status = BlockProtocol->Read(BlockProtocol,
                                     *StartLba + BlockIndex,
                                     0,
                                     &DataSize,
                                     Data);

        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    //
    // Update the LBA and offset used by the following read.
    //

    *StartLba += BlockIndex;
    *Offset = DataSize;
    return EFI_SUCCESS;
}

BOOLEAN
EfiFvVerifyHeaderChecksum (
    EFI_FIRMWARE_VOLUME_HEADER *VolumeHeader
    )

/*++

Routine Description:

    This routine verifies the checksum of a firmware volume header.

Arguments:

    VolumeHeader - Supplies a pointer to the volume header to verify.

Return Value:

    TRUE if the checksum verification passed.

    FALSE if the checksum verification failed.

--*/

{

    UINT16 Checksum;

    Checksum = EfipFvCalculateSum16((UINT16 *)VolumeHeader,
                                    VolumeHeader->HeaderLength);

    if (Checksum == 0) {
        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
VOID
EfipFvBlockNotify (
    EFI_EVENT Event,
    VOID *Context
    )

/*++

Routine Description:

    This routine is called when a new firmware volume block protocol appears
    in the system.

Arguments:

    Event - Supplies a pointer to the event that fired.

    Context - Supplies an unused context pointer.

Return Value:

    None.

--*/

{

    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *BlockProtocol;
    UINTN BufferSize;
    PEFI_FIRMWARE_VOLUME Device;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *FirmwareVolume;
    EFI_HANDLE Handle;
    BOOLEAN Match;
    EFI_STATUS Status;
    EFI_FIRMWARE_VOLUME_HEADER *VolumeHeader;

    //
    // Examine all new handles.
    //

    VolumeHeader = NULL;
    while (TRUE) {
        Status = EfiCoreLocateHandle(ByRegisterNotify,
                                     NULL,
                                     EfiFvBlockNotifyRegistration,
                                     &BufferSize,
                                     &Handle);

        if (Status == EFI_NOT_FOUND) {
            break;
        }

        if (EFI_ERROR(Status)) {
            continue;
        }

        //
        // Get the block protocol on the handle.
        //

        Status = EfiCoreHandleProtocol(Handle,
                                       &EfiFirmwareVolumeBlockProtocolGuid,
                                       (VOID **)&BlockProtocol);

        if (EFI_ERROR(Status)) {

            ASSERT(FALSE);

            continue;
        }

        ASSERT(BlockProtocol != NULL);

        Status = EfiFvGetVolumeHeader(BlockProtocol, &VolumeHeader);
        if (EFI_ERROR(Status)) {
            return;
        }

        ASSERT(VolumeHeader != NULL);

        Status = EfiFvVerifyHeaderChecksum(VolumeHeader);
        if (EFI_ERROR(Status)) {
            EfiCoreFreePool(VolumeHeader);
            return;
        }

        //
        // Skip file systems that aren't understood.
        //

        Match = EfiCoreCompareGuids(&(VolumeHeader->FileSystemGuid),
                                    &EfiFirmwareFileSystem2Guid);

        if (Match == FALSE) {
            Match = EfiCoreCompareGuids(&(VolumeHeader->FileSystemGuid),
                                        &EfiFirmwareFileSystem3Guid);

        }

        if (Match == FALSE) {
            continue;
        }

        //
        // Check to see if there is a firmware volume protocol already
        // installed on this handle.
        //

        Status = EfiCoreHandleProtocol(Handle,
                                       &EfiFirmwareVolume2ProtocolGuid,
                                       (VOID **)&FirmwareVolume);

        //
        // If there's a previously existing firmware volume protocol, then
        // update the block device if it was created by this driver.
        //

        if (!EFI_ERROR(Status)) {
            Device = PARENT_STRUCTURE(FirmwareVolume,
                                      EFI_FIRMWARE_VOLUME,
                                      VolumeProtocol);

            if (Device->Magic == EFI_FIRMWARE_VOLUME_MAGIC) {
                Device->BlockIo = BlockProtocol;
            }

        //
        // No firmware volume is present, create a new one.
        //

        } else {
            Device = EfiCoreAllocateBootPool(sizeof(EFI_FIRMWARE_VOLUME));
            if (Device == NULL) {
                return;
            }

            EfiCoreCopyMemory(Device,
                              &EfiFirmwareVolumeTemplate,
                              sizeof(EFI_FIRMWARE_VOLUME));

            Device->BlockIo = BlockProtocol;
            Device->Handle = Handle;
            Device->VolumeHeader = VolumeHeader;
            Match = EfiCoreCompareGuids(&(VolumeHeader->FileSystemGuid),
                                        &EfiFirmwareFileSystem3Guid);

            if (Match != FALSE) {
                Device->IsFfs3 = TRUE;
            }

            Device->VolumeProtocol.ParentHandle = BlockProtocol->ParentHandle;
            Status = EfipFvCheck(Device);
            if (!EFI_ERROR(Status)) {
                Status = EfiCoreInstallProtocolInterface(
                                               &Handle,
                                               &EfiFirmwareVolume2ProtocolGuid,
                                               EFI_NATIVE_INTERFACE,
                                               &(Device->VolumeProtocol));

                ASSERT(!EFI_ERROR(Status));

            }

            if (EFI_ERROR(Status)) {
                EfiCoreFreePool(Device);
            }
        }
    }

    return;
}

EFI_STATUS
EfipFvCheck (
    PEFI_FIRMWARE_VOLUME Device
    )

/*++

Routine Description:

    This routine checks the given firmware volume for consistency and allocates
    a cache for it.

Arguments:

    Device - Supplies a pointer to the device to check.

Return Value:

    EFI status code.

--*/

{

    EFI_FVB_ATTRIBUTES Attributes;
    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *BlockIo;
    EFI_FV_BLOCK_MAP_ENTRY *BlockMap;
    UINT8 *CacheLocation;
    BOOLEAN Erased;
    EFI_FFS_FILE_LIST_ENTRY *FfsFileEntry;
    EFI_FFS_FILE_HEADER *FfsHeader;
    EFI_FFS_FILE_STATE FileState;
    UINTN HeaderSize;
    UINTN Index;
    EFI_LBA LbaIndex;
    UINTN LbaOffset;
    UINTN Size;
    EFI_STATUS Status;
    UINTN TestLength;
    UINT8 *TopAddress;
    BOOLEAN Valid;
    EFI_FIRMWARE_VOLUME_HEADER *VolumeHeader;
    EFI_FIRMWARE_VOLUME_EXT_HEADER *VolumeHeaderExt;

    BlockIo = Device->BlockIo;
    VolumeHeader = Device->VolumeHeader;
    Status = BlockIo->GetAttributes(BlockIo, &Attributes);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Size = (UINTN)(VolumeHeader->Length - VolumeHeader->HeaderLength);
    Device->CachedVolume = EfiCoreAllocateBootPool(Size);
    if (Device->CachedVolume == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Device->EndOfCachedVolume = Device->CachedVolume + Size;

    //
    // Copy the firmware volume minus the header into memory using the block
    // map in the header.
    //

    BlockMap = VolumeHeader->BlockMap;
    CacheLocation = Device->CachedVolume;
    LbaIndex = 0;
    LbaOffset = 0;
    HeaderSize = VolumeHeader->HeaderLength;
    while ((BlockMap->BlockCount != 0) || (BlockMap->BlockLength != 0)) {
        Index = 0;
        Size = BlockMap->BlockLength;

        //
        // Skip the header.
        //

        if (HeaderSize > 0) {
            while ((Index < BlockMap->BlockCount) &&
                   (HeaderSize >= BlockMap->BlockLength)) {

                HeaderSize -= BlockMap->BlockLength;
                LbaIndex += 1;
                Index += 1;
            }

            //
            // Check whether or not the header crosses a block boundary.
            //

            if (Index >= BlockMap->BlockCount) {
                BlockMap += 1;
                continue;

            } else if (HeaderSize > 0) {
                LbaOffset = HeaderSize;
                Size = BlockMap->BlockLength - HeaderSize;
                HeaderSize = 0;
            }
        }

        //
        // Read the firmware volume data.
        //

        while (Index < BlockMap->BlockCount) {
            Status = BlockIo->Read(BlockIo,
                                   LbaIndex,
                                   LbaOffset,
                                   &Size,
                                   CacheLocation);

            if (EFI_ERROR(Status)) {
                goto FvCheckEnd;
            }

            LbaIndex += 1;
            CacheLocation += Size;
            LbaOffset = 0;
            Size = BlockMap->BlockLength;
            Index += 1;
        }

        BlockMap += 1;
    }

    //
    // Check the free space and file list.
    //

    Device->ErasePolarity = 0;
    if ((Attributes & EFI_FVB_ERASE_POLARITY) != 0) {
        Device->ErasePolarity = 1;
    }

    //
    // Go through the entire firmware volume cache and check the consistency of
    // the firmware volume. Make a linked list of all the FFS file headers.
    //

    Status = EFI_SUCCESS;
    INITIALIZE_LIST_HEAD(&(Device->FfsFileList));
    if (VolumeHeader->ExtHeaderOffset != 0) {
        VolumeHeaderExt =
            (EFI_FIRMWARE_VOLUME_EXT_HEADER *)(Device->CachedVolume +
                                               (VolumeHeader->ExtHeaderOffset -
                                                VolumeHeader->HeaderLength));

        FfsHeader = (EFI_FFS_FILE_HEADER *)((UINT8 *)VolumeHeaderExt +
                                            VolumeHeaderExt->ExtHeaderSize);

        FfsHeader = (EFI_FFS_FILE_HEADER *)ALIGN_POINTER(FfsHeader, 8);

    } else {
        FfsHeader = (EFI_FFS_FILE_HEADER *)(Device->CachedVolume);
    }

    TopAddress = Device->EndOfCachedVolume;
    while ((UINT8 *)FfsHeader < TopAddress) {
        TestLength = TopAddress - ((UINT8 *)FfsHeader);
        if (TestLength > sizeof(EFI_FFS_FILE_HEADER)) {
            TestLength = sizeof(EFI_FFS_FILE_HEADER);
        }

        //
        // If this is all free space then that's it.
        //

        Erased = EfipFvIsBufferErased(Device->ErasePolarity,
                                      FfsHeader,
                                      TestLength);

        if (Erased != FALSE) {
            goto FvCheckEnd;
        }

        Valid = EfipFvIsValidFfsHeader(Device->ErasePolarity,
                                       FfsHeader,
                                       &FileState);

        if (Valid == FALSE) {
            if ((FileState == EFI_FILE_HEADER_INVALID) ||
                (FileState == EFI_FILE_HEADER_CONSTRUCTION)) {

                if (EFI_IS_FFS_FILE2(FfsHeader)) {
                    if (Device->IsFfs3 == FALSE) {
                        RtlDebugPrint("Warning: Found an FFS3 file in an FFS2 "
                                      "volume!\n");
                    }

                    FfsHeader =
                             (EFI_FFS_FILE_HEADER *)((UINT8 *)FfsHeader +
                                                 sizeof(EFI_FFS_FILE_HEADER2));

                } else {
                    FfsHeader =
                              (EFI_FFS_FILE_HEADER *)((UINT8 *)FfsHeader +
                                                  sizeof(EFI_FFS_FILE_HEADER));
                }

                continue;

            } else {
                Status = EFI_VOLUME_CORRUPTED;
                goto FvCheckEnd;
            }
        }

        Valid = EfipFvIsValidFfsFile(Device->ErasePolarity, FfsHeader);
        if (Valid == FALSE) {
            Status = EFI_VOLUME_CORRUPTED;
            goto FvCheckEnd;
        }

        if (EFI_IS_FFS_FILE2(FfsHeader)) {

            ASSERT(EFI_FFS_FILE2_SIZE(FfsHeader) > MAX_FFS_SIZE);

            if (Device->IsFfs3 == FALSE) {
                RtlDebugPrint("Warning: Found an FFS3 file in an FFS2 "
                              "volume!\n");

                //
                // Skip the file and align up the the next 8-byte boundary.
                //

                FfsHeader =
                        (EFI_FFS_FILE_HEADER *)((UINT8 *)FfsHeader +
                                                EFI_FFS_FILE2_SIZE(FfsHeader));

                FfsHeader = (EFI_FFS_FILE_HEADER *)ALIGN_POINTER(FfsHeader, 8);
                continue;
            }
        }

        FileState = EfipFvGetFileState(Device->ErasePolarity, FfsHeader);
        if (FileState != EFI_FILE_DELETED) {
            FfsFileEntry = EfiCoreAllocateBootPool(
                                              sizeof(EFI_FFS_FILE_LIST_ENTRY));

            if (FfsFileEntry == NULL) {
                Status = EFI_OUT_OF_RESOURCES;
                goto FvCheckEnd;
            }

            EfiCoreSetMemory(FfsFileEntry, sizeof(EFI_FFS_FILE_LIST_ENTRY), 0);
            FfsFileEntry->FileHeader = FfsHeader;
            INSERT_BEFORE(&(FfsFileEntry->ListEntry), &(Device->FfsFileList));
        }

        //
        // Move to the next file header (aligned to an 8-byte boundary).
        //

        if (EFI_IS_FFS_FILE2(FfsHeader)) {
          FfsHeader = (EFI_FFS_FILE_HEADER *)((UINT8 *)FfsHeader +
                                              EFI_FFS_FILE2_SIZE(FfsHeader));

        } else {
          FfsHeader = (EFI_FFS_FILE_HEADER *)((UINT8 *)FfsHeader +
                                              EFI_FFS_FILE_SIZE(FfsHeader));
        }

        FfsHeader = (EFI_FFS_FILE_HEADER *)ALIGN_POINTER(FfsHeader, 8);
    }

FvCheckEnd:
    if (EFI_ERROR(Status)) {
        EfipFvFreeDeviceResource(Device);
    }

    return Status;
}

VOID
EfipFvFreeDeviceResource (
    PEFI_FIRMWARE_VOLUME Volume
    )

/*++

Routine Description:

    This routine destroys a firmware device volume.

Arguments:

    Volume - Supplies a pointer to the volume to destroy.

Return Value:

    None.

--*/

{

    PEFI_FFS_FILE_LIST_ENTRY FfsFileEntry;

    //
    // Free all the FFS file list entries.
    //

    while (LIST_EMPTY(&(Volume->FfsFileList)) == FALSE) {
        FfsFileEntry = LIST_VALUE(Volume->FfsFileList.Next,
                                  EFI_FFS_FILE_LIST_ENTRY,
                                  ListEntry);

        if (FfsFileEntry->StreamHandle != 0) {
            EfiFvCloseSectionStream(FfsFileEntry->StreamHandle);
        }

        LIST_REMOVE(&(FfsFileEntry->ListEntry));
        EfiCoreFreePool(FfsFileEntry);
    }

    if (Volume->CachedVolume != NULL) {
        EfiCoreFreePool(Volume->CachedVolume);
    }

    if (Volume->VolumeHeader != NULL) {
        EfiCoreFreePool(Volume->VolumeHeader);
    }

    return;
}

BOOLEAN
EfipFvIsBufferErased (
    UINT8 ErasePolarity,
    VOID *Buffer,
    UINTN BufferSize
    )

/*++

Routine Description:

    This routine determines if the given buffer is all erased data.

Arguments:

    ErasePolarity - Supplies the erase polarity of the volume.

    Buffer - Supplies the buffer to be checked.

    BufferSize - Supplies the size of the buffer.

Return Value:

    TRUE if the block of buffer is erased.

    FALSE if the block of buffer is not erased.

--*/

{

    UINT8 *CheckBuffer;
    UINTN Count;
    UINT8 EraseByte;

    EraseByte = 0;
    if (ErasePolarity != 0) {
        EraseByte = 0xFF;
    }

    CheckBuffer = Buffer;
    for (Count = 0; Count < BufferSize; Count += 1) {
        if (CheckBuffer[Count] != EraseByte) {
            return FALSE;
        }
    }

    return TRUE;
}

BOOLEAN
EfipFvIsValidFfsHeader (
    UINT8 ErasePolarity,
    EFI_FFS_FILE_HEADER *FfsHeader,
    EFI_FFS_FILE_STATE *FileState
    )

/*++

Routine Description:

    This routine determines if the given supposed FFS file header is valid.

Arguments:

    ErasePolarity - Supplies the erase polarity of the volume.

    FfsHeader - Supplies a pointer to the FFS file header to be checked.

    FileState - Supplies a pointer where the file state will be returned if the
        header is valid.

Return Value:

    TRUE if the FFS file header is valid.

    FALSE if the header is not valid.

--*/

{

    *FileState = EfipFvGetFileState(ErasePolarity, FfsHeader);
    switch (*FileState) {

    //
    // If the file state looks good verify the header checksum.
    //

    case EFI_FILE_HEADER_VALID:
    case EFI_FILE_DATA_VALID:
    case EFI_FILE_MARKED_FOR_UPDATE:
    case EFI_FILE_DELETED:
        return EfipFvVerifyFileHeaderChecksum(FfsHeader);

    case EFI_FILE_HEADER_CONSTRUCTION:
    case EFI_FILE_HEADER_INVALID:
    default:
        break;
    }

    return FALSE;
}

BOOLEAN
EfipFvIsValidFfsFile (
    UINT8 ErasePolarity,
    EFI_FFS_FILE_HEADER *FfsHeader
    )

/*++

Routine Description:

    This routine determines if the given supposed FFS file is valid.

Arguments:

    ErasePolarity - Supplies the erase polarity of the volume.

    FfsHeader - Supplies a pointer to the FFS file header to be checked.

Return Value:

    TRUE if the FFS file is valid.

    FALSE if the header is not valid.

--*/

{

    UINT8 DataChecksum;
    UINT8 *FileData;
    UINTN FileDataSize;
    EFI_FFS_FILE_STATE FileState;

    FileState = EfipFvGetFileState(ErasePolarity, FfsHeader);
    switch (FileState) {
    case EFI_FILE_DELETED:
    case EFI_FILE_DATA_VALID:
    case EFI_FILE_MARKED_FOR_UPDATE:
        DataChecksum = FFS_FIXED_CHECKSUM;
        if ((FfsHeader->Attributes & FFS_ATTRIB_CHECKSUM) != 0) {
            if (EFI_IS_FFS_FILE2(FfsHeader)) {
                FileData = (UINT8 *)FfsHeader + sizeof(EFI_FFS_FILE_HEADER2);
                FileDataSize = EFI_FFS_FILE2_SIZE(FfsHeader) -
                               sizeof(EFI_FFS_FILE_HEADER2);

            } else {
                FileData = (UINT8 *)FfsHeader + sizeof(EFI_FFS_FILE_HEADER);
                FileDataSize = EFI_FFS_FILE_SIZE(FfsHeader) -
                               sizeof(EFI_FFS_FILE_HEADER);
            }

            DataChecksum = EfipFvCalculateChecksum8(FileData, FileDataSize);
        }

        if (FfsHeader->IntegrityCheck.Checksum.File == DataChecksum) {
            return TRUE;
        }

        break;

    default:
        break;
    }

    return FALSE;
}

EFI_FFS_FILE_STATE
EfipFvGetFileState (
    UINT8 ErasePolarity,
    EFI_FFS_FILE_HEADER *FfsHeader
    )

/*++

Routine Description:

    This routine returns the FFS file state.

Arguments:

    ErasePolarity - Supplies the erase polarity of the volume.

    FfsHeader - Supplies a pointer to the FFS file header.

Return Value:

    Returns the FFS file state.

--*/

{

    EFI_FFS_FILE_STATE FileState;
    UINT8 HighestBit;

    FileState = FfsHeader->State;
    if (ErasePolarity != 0) {
        FileState = (EFI_FFS_FILE_STATE)~FileState;
    }

    HighestBit = 0x80;
    while ((HighestBit != 0) && ((HighestBit & FileState) == 0)) {
        HighestBit >>= 1;
    }

    return (EFI_FFS_FILE_STATE)HighestBit;
}

BOOLEAN
EfipFvVerifyFileHeaderChecksum (
    EFI_FFS_FILE_HEADER *FfsHeader
    )

/*++

Routine Description:

    This routine verifies the checksum of an FFS file header.

Arguments:

    FfsHeader - Supplies a pointer to the volume header to verify.

Return Value:

    TRUE if the checksum verification passed.

    FALSE if the checksum verification failed.

--*/

{

    UINT8 HeaderChecksum;

    if (EFI_IS_FFS_FILE2(FfsHeader)) {
        HeaderChecksum = EfipFvCalculateSum8((UINT8 *)FfsHeader,
                                             sizeof(EFI_FFS_FILE_HEADER2));

    } else {
        HeaderChecksum = EfipFvCalculateSum8((UINT8 *)FfsHeader,
                                             sizeof(EFI_FFS_FILE_HEADER));
    }

    HeaderChecksum = (UINT8)(HeaderChecksum - FfsHeader->State -
                             FfsHeader->IntegrityCheck.Checksum.File);

    if (HeaderChecksum == 0) {
        return TRUE;
    }

    return FALSE;
}

UINT16
EfipFvCalculateSum16 (
    UINT16 *Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine calculates the 16-bit checksum of the bytes in the given
    buffer.

Arguments:

    Buffer - Supplies a pointer to a buffer containing byte data.

    Size - Supplies the size of the buffer in bytes.

Return Value:

    Returns the 16-bit sum of the buffer words.

--*/

{

    UINTN Index;
    UINT16 Sum;

    Size = Size / sizeof(UINT16);
    Sum = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Sum = (UINT16)(Sum + Buffer[Index]);
    }

    return Sum;
}

UINT8
EfipFvCalculateChecksum8 (
    UINT8 *Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine calculates the 8-bit checksum of the bytes in the given
    buffer.

Arguments:

    Buffer - Supplies a pointer to a buffer containing byte data.

    Size - Supplies the size of the buffer.

Return Value:

    Returns the 8-bit checksum of each byte in the buffer.

--*/

{

    return 0x100 - EfipFvCalculateSum8(Buffer, Size);
}

UINT8
EfipFvCalculateSum8 (
    UINT8 *Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine calculates the 8-bit sum of the bytes in the given buffer.

Arguments:

    Buffer - Supplies a pointer to a buffer containing byte data.

    Size - Supplies the size of the buffer.

Return Value:

    Returns the 8-bit checksum of each byte in the buffer.

--*/

{

    UINTN Index;
    UINT8 Sum;

    Sum = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Sum = (UINT8)(Sum + Buffer[Index]);
    }

    return Sum;
}

