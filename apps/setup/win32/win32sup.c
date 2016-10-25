/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    win32sup.c

Abstract:

    This module implements Windows support functions for the setup application.

Author:

    Evan Green 8-Oct-2014

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#define INITGUID

//
// Set to XP/2003 for FindFirstVolume.
//

#define _WIN32_WINNT 0x0500

#include <errno.h>
#include <stdio.h>

#include <windows.h>
#include <SetupApi.h>
#include <winioctl.h>

#include <minoca/devinfo/part.h>
#include "win32sup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SETUP_WIN32_DISK_LAYOUT_SIZE 4096

//
// ------------------------------------------------------ Data Type Definitions
//

//
// This should come from #include <ddk/ntddstor.h>, but for some very annoying
// reason it's not possible to include both <winioctl.h> and <ddk/ntddstor.h>,
// so the declaration is duplicated here.
//

typedef struct _STORAGE_DEVICE_NUMBER {
    DEVICE_TYPE DeviceType;
    ULONG DeviceNumber;
    ULONG PartitionNumber;
} STORAGE_DEVICE_NUMBER, *PSTORAGE_DEVICE_NUMBER;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SetupWin32GetDiskSize (
    HANDLE Handle,
    PULONGLONG SectorCount,
    PULONG SectorSize
    );

INT
SetupWin32UnmountVolumesForDisk (
    DWORD DiskNumber
    );

BOOL
SetupWin32IsVolumeInDisk (
    HANDLE VolumeHandle,
    DWORD DiskNumber
    );

INT
SetupWin32FillInEntriesForDisk (
    PSETUP_WIN32_PARTITION_DESCRIPTION *Partitions,
    PULONG PartitionCount,
    PSTR DevicePath,
    PSTORAGE_DEVICE_NUMBER DeviceNumber,
    PDRIVE_LAYOUT_INFORMATION_EX DiskLayout,
    ULONGLONG BlockCount,
    ULONG BlockSize
    );

INT
SetupWin32AddPartitionEntry (
    PSETUP_WIN32_PARTITION_DESCRIPTION *Partitions,
    PULONG PartitionCount,
    PSETUP_WIN32_PARTITION_DESCRIPTION NewEntry
    );

//
// -------------------------------------------------------------------- Globals
//

//
// This should also come from <ddk/ntddstor.h>, a header that cannot be
// included with winioctl.h.
//

DEFINE_GUID(GUID_DEVINTERFACE_DISK, 0x53f56307L, \
            0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b);

//
// ------------------------------------------------------------------ Functions
//

INT
SetupWin32EnumerateDevices (
    PSETUP_WIN32_PARTITION_DESCRIPTION *Devices,
    PULONG DeviceCount
    )

/*++

Routine Description:

    This routine enumerates all devices and partitions in the system.

Arguments:

    Devices - Supplies a pointer where an array of partition descriptions will
        be returned on success. The caller is responsible for freeing this
        array when finished (it's just a single free plus each device path).

    DeviceCount - Supplies a pointer where the number of elements in the
        array will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONGLONG BlockCount;
    ULONG BlockSize;
    DWORD BytesReturned;
    DWORD DeviceIndex;
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData;
    HANDLE Disk;
    HDEVINFO DiskClassDevices;
    PDRIVE_LAYOUT_INFORMATION_EX DiskLayout;
    STORAGE_DEVICE_NUMBER DiskNumber;
    DWORD Flags;
    ULONG PartitionCount;
    ULONG PartitionIndex;
    PSETUP_WIN32_PARTITION_DESCRIPTION Partitions;
    DWORD RequiredSize;
    INT Result;

    DeviceInterfaceDetailData = NULL;
    Disk = INVALID_HANDLE_VALUE;
    DiskLayout = NULL;
    PartitionCount = 0;
    Partitions = NULL;
    Result = -1;

    //
    // Get the handle to the device information set for installed disk class
    // devices. This returns only devices that are present and have exposed
    // and interface.
    //

    Flags = DIGCF_PRESENT | DIGCF_DEVICEINTERFACE;
    DiskClassDevices = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK,
                                           NULL,
                                           NULL,
                                           Flags);

    if (DiskClassDevices == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "SetupDiGetClassDevs failed: ");
        SetupWin32PrintLastError();
        goto Win32EnumerateDevicesEnd;
    }

    ZeroMemory(&DeviceInterfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    DeviceIndex = 0;

    //
    // Loop through all the disk interfaces returned.
    //

    while (TRUE) {
        Result = SetupDiEnumDeviceInterfaces(DiskClassDevices,
                                             NULL,
                                             &GUID_DEVINTERFACE_DISK,
                                             DeviceIndex,
                                             &DeviceInterfaceData);

        if (Result == FALSE) {
            break;
        }

        DeviceIndex += 1;
        SetupDiGetDeviceInterfaceDetail(DiskClassDevices,
                                        &DeviceInterfaceData,
                                        NULL,
                                        0,
                                        &RequiredSize,
                                        NULL);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            fprintf(stderr, "SetupDiGetDeviceInterfaceDetail failed: ");
            SetupWin32PrintLastError();
            Result = -1;
            goto Win32EnumerateDevicesEnd;
        }

        DeviceInterfaceDetailData = malloc(RequiredSize);
        if (DeviceInterfaceDetailData == NULL) {
            Result = ENOMEM;
            goto Win32EnumerateDevicesEnd;
        }

        ZeroMemory(DeviceInterfaceDetailData, RequiredSize);
        DeviceInterfaceDetailData->cbSize =
                                       sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        Result = SetupDiGetDeviceInterfaceDetail(DiskClassDevices,
                                                 &DeviceInterfaceData,
                                                 DeviceInterfaceDetailData,
                                                 RequiredSize,
                                                 NULL,
                                                 NULL);

        if (Result == FALSE) {
            fprintf(stderr, "SetupDiGetDeviceInterfaceDetail (2) failed: ");
            SetupWin32PrintLastError();
            Result = -1;
            goto Win32EnumerateDevicesEnd;
        }

        //
        // Open the disk.
        //

        Disk = CreateFile(DeviceInterfaceDetailData->DevicePath,
                          GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);

        if (Disk == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_ACCESS_DENIED) {
                fprintf(stderr,
                        "Unable to open disk. Try running as administrator.\n");

            } else {
                fprintf(stderr, "Open disk failed: ");
                SetupWin32PrintLastError();
                Result = -1;
                goto Win32EnumerateDevicesEnd;
            }

            free(DeviceInterfaceDetailData);
            DeviceInterfaceDetailData = NULL;
            continue;
        }

        Result = DeviceIoControl(Disk,
                                 IOCTL_STORAGE_GET_DEVICE_NUMBER,
                                 NULL,
                                 0,
                                 &DiskNumber,
                                 sizeof(STORAGE_DEVICE_NUMBER),
                                 &BytesReturned,
                                 NULL);

        if (Result == FALSE) {
            fprintf(stderr, "IOCTL_STORAGE_GET_DEVICE_NUMBER failed: ");
            SetupWin32PrintLastError();
            Result = -1;
            goto Win32EnumerateDevicesEnd;
        }

        Result = SetupWin32GetDiskSize(Disk, &BlockCount, &BlockSize);
        if (Result != 0) {
            BlockCount = 0;
            BlockSize = 0;
        }

        DiskLayout = malloc(SETUP_WIN32_DISK_LAYOUT_SIZE);
        if (DiskLayout == NULL) {
            Result = ENOMEM;
            goto Win32EnumerateDevicesEnd;
        }

        Result = DeviceIoControl(Disk,
                                 IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
                                 NULL,
                                 0,
                                 DiskLayout,
                                 SETUP_WIN32_DISK_LAYOUT_SIZE,
                                 &BytesReturned,
                                 NULL);

        if (Result == FALSE) {
            fprintf(stderr, "IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed: ");
            SetupWin32PrintLastError();
            Result = -1;
            goto Win32EnumerateDevicesEnd;
        }

        Result = SetupWin32FillInEntriesForDisk(
                                         &Partitions,
                                         &PartitionCount,
                                         DeviceInterfaceDetailData->DevicePath,
                                         &DiskNumber,
                                         DiskLayout,
                                         BlockCount,
                                         BlockSize);

        if (Result != 0) {
            goto Win32EnumerateDevicesEnd;
        }

        CloseHandle(Disk);
        Disk = INVALID_HANDLE_VALUE;
        free(DeviceInterfaceDetailData);
        DeviceInterfaceDetailData = NULL;
        free(DiskLayout);
        DiskLayout = NULL;
    }

Win32EnumerateDevicesEnd:
    if (Result != 0) {
        if (Partitions != NULL) {
            for (PartitionIndex = 0;
                 PartitionIndex < PartitionCount;
                 PartitionIndex += 1) {

                if (Partitions[PartitionIndex].DevicePath != NULL) {
                    free(Partitions[PartitionIndex].DevicePath);
                }
            }

            free(Partitions);
            Partitions = NULL;
        }

        PartitionCount = 0;
    }

    if (DiskLayout != NULL) {
        free(DiskLayout);
    }

    if (Disk != INVALID_HANDLE_VALUE) {
        CloseHandle(Disk);
    }

    if (DeviceInterfaceDetailData != NULL) {
        free(DeviceInterfaceDetailData);
    }

    if (DiskClassDevices != INVALID_HANDLE_VALUE) {
        SetupDiDestroyDeviceInfoList(DiskClassDevices);
    }

    *Devices = Partitions;
    *DeviceCount = PartitionCount;
    return Result;
}

VOID
SetupWin32PrintLastError (
    VOID
    )

/*++

Routine Description:

    This routine prints a description of GetLastError to standard error and
    also prints a newline.

Arguments:

    None.

Return Value:

    None.

--*/

{

    char *Message;

    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                   NULL,
                   GetLastError(),
                   0,
                   (LPSTR)&Message,
                   0,
                   NULL);

    fprintf(stderr, "%s\n", Message);
    LocalFree(Message);
    return;
}

PVOID
SetupWin32OpenDeviceId (
    ULONGLONG DeviceId
    )

/*++

Routine Description:

    This routine opens a handle to a disk or partition ID.

Arguments:

    DeviceId - Supplies the device ID to open.

Return Value:

    Returns the open handle on success.

    NULL on failure.

--*/

{

    ULONG DiskNumber;
    HANDLE Handle;
    ULONG PartitionNumber;
    CHAR Path[512];
    INT Result;

    DiskNumber = (DeviceId >> 16) & 0xFFFF;
    PartitionNumber = DeviceId & 0xFFFF;
    if (PartitionNumber == 0) {
        snprintf(Path, sizeof(Path), "\\\\.\\PhysicalDrive%d", DiskNumber);

    } else {
        fprintf(stderr,
                "Error: Installing to partitions on Windows is not yet "
                "supported.\n");

        return NULL;
    }

    Result = SetupWin32UnmountVolumesForDisk(DiskNumber);
    if (Result != 0) {
        fprintf(stderr,
                "Error: Failed to unmount volumes for disk %d.\n",
                DiskNumber);

        return NULL;
    }

    Handle = CreateFile(Path,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        0,
                        NULL);

    if (Handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open %s: ", Path);
        SetupWin32PrintLastError();
        return NULL;
    }

    return Handle;
}

VOID
SetupWin32Close (
    PVOID Handle
    )

/*++

Routine Description:

    This routine closes a handle.

Arguments:

    Handle - Supplies a pointer to the destination to open.

Return Value:

    None.

--*/

{

    CloseHandle(Handle);
    return;
}

ssize_t
SetupWin32Read (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine reads from an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read.

    -1 on failure.

--*/

{

    DWORD BytesCompleted;
    BOOL Result;
    ssize_t TotalBytesRead;

    TotalBytesRead = 0;
    while (ByteCount != 0) {
        Result = ReadFile(Handle, Buffer, ByteCount, &BytesCompleted, NULL);
        if (Result == FALSE) {
            fprintf(stderr, "Error: Failed to read: ");
            SetupWin32PrintLastError();
            break;
        }

        Buffer += BytesCompleted;
        TotalBytesRead += BytesCompleted;
        ByteCount -= BytesCompleted;
    }

    return TotalBytesRead;
}

ssize_t
SetupWin32Write (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine writes data to an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the bytes to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

    -1 on failure.

--*/

{

    DWORD BytesCompleted;
    BOOL Result;
    ssize_t TotalBytesWritten;

    TotalBytesWritten = 0;
    while (ByteCount != 0) {
        Result = WriteFile(Handle, Buffer, ByteCount, &BytesCompleted, NULL);
        if (Result == FALSE) {
            fprintf(stderr, "Error: Failed to write: ");
            SetupWin32PrintLastError();
            break;
        }

        Buffer += BytesCompleted;
        TotalBytesWritten += BytesCompleted;
        ByteCount -= BytesCompleted;
    }

    return TotalBytesWritten;
}

LONGLONG
SetupWin32Seek (
    PVOID Handle,
    LONGLONG Offset
    )

/*++

Routine Description:

    This routine seeks in the current file or device.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the new offset to set.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

{

    LARGE_INTEGER DistanceToMove;
    LARGE_INTEGER NewOffset;
    BOOL Result;

    DistanceToMove.QuadPart = Offset;
    NewOffset.QuadPart = 0;
    Result = SetFilePointerEx(Handle, DistanceToMove, &NewOffset, FILE_BEGIN);
    if (Result == FALSE) {
        fprintf(stderr, "Error: Failed to seek: ");
        SetupWin32PrintLastError();
    }

    return NewOffset.QuadPart;
}

LONGLONG
SetupWin32Tell (
    PVOID Handle
    )

/*++

Routine Description:

    This routine returns the current file pointer.

Arguments:

    Handle - Supplies the handle.

Return Value:

    Returns the file offset on success.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

{

    LARGE_INTEGER DistanceToMove;
    LARGE_INTEGER NewOffset;
    BOOL Result;

    DistanceToMove.QuadPart = 0;
    NewOffset.QuadPart = 0;
    Result = SetFilePointerEx(Handle, DistanceToMove, &NewOffset, FILE_CURRENT);
    if (Result == FALSE) {
        fprintf(stderr, "Error: Failed to tell: ");
        SetupWin32PrintLastError();
    }

    return NewOffset.QuadPart;
}

INT
SetupWin32FileStat (
    PVOID Handle,
    PULONGLONG FileSize
    )

/*++

Routine Description:

    This routine gets details for the given open file.

Arguments:

    Handle - Supplies the handle.

    FileSize - Supplies an optional pointer where the file size will be
        returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;
    ULONGLONG SectorCount;
    ULONG SectorSize;

    Result = SetupWin32GetDiskSize(Handle, &SectorCount, &SectorSize);
    if (Result != 0) {
        fprintf(stderr, "Error: Failed to get disk size: ");
        SetupWin32PrintLastError();
        return -1;
    }

    *FileSize = SectorSize * SectorCount;
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupWin32GetDiskSize (
    HANDLE Handle,
    PULONGLONG SectorCount,
    PULONG SectorSize
    )

/*++

Routine Description:

    This routine queries the disk size for the given handle.

Arguments:

    Handle - Supplies the open handle to the disk.

    SectorCount - Supplies a pointer where the number of sectors on the disk
        will be returned.

    SectorSize - Supplies a pointer where the size of a single sector in bytes
        will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    DWORD BytesRead;
    DISK_GEOMETRY_EX DiskGeometry;
    BOOL Result;

    Result = DeviceIoControl(Handle,
                             IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                             NULL,
                             0,
                             &DiskGeometry,
                             sizeof(DISK_GEOMETRY_EX),
                             &BytesRead,
                             NULL);

    if (Result == FALSE) {
        if (GetLastError() != ERROR_NOT_READY) {
            fprintf(stderr, "IOCTL_DISK_GET_DRIVE_GEOMETRY_EX failed: ");
            SetupWin32PrintLastError();
        }

        return -1;
    }

    *SectorSize = DiskGeometry.Geometry.BytesPerSector;
    *SectorCount = DiskGeometry.DiskSize.QuadPart /
                   DiskGeometry.Geometry.BytesPerSector;

    return 0;
}

INT
SetupWin32UnmountVolumesForDisk (
    DWORD DiskNumber
    )

/*++

Routine Description:

    This routine unmounts all volumes associated with the given disk number.

Arguments:

    DiskNumber - Supplies the disk number.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    DWORD BytesRead;
    HANDLE Handle;
    CHAR Path[1024];
    ULONG PathLength;
    BOOL Result;
    HANDLE SearchHandle;

    SearchHandle = FindFirstVolume(Path, sizeof(Path));
    if (SearchHandle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    while (TRUE) {
        PathLength = strlen(Path);
        if ((PathLength != 0) && (Path[PathLength - 1] == '\\')) {
            Path[PathLength - 1] = '\0';
        }

        Handle = CreateFile(Path,
                            GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            NULL);

        if (Handle != INVALID_HANDLE_VALUE) {
            if (SetupWin32IsVolumeInDisk(Handle, DiskNumber) != FALSE) {
                Result = DeviceIoControl(Handle,
                                         FSCTL_LOCK_VOLUME,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         &BytesRead,
                                         NULL);

                if (Result == FALSE) {
                    fprintf(stderr,
                            "Warning: Failed to lock volume '%s': ",
                            Path);

                    SetupWin32PrintLastError();
                }

                Result = DeviceIoControl(Handle,
                                         FSCTL_DISMOUNT_VOLUME,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         &BytesRead,
                                         NULL);

                if (Result == FALSE) {
                    fprintf(stderr,
                            "Warning: Failed to unmount '%s': ",
                            Path);

                    SetupWin32PrintLastError();
                }

                //
                // "Lose" the handle so it stays open and the volume stays
                // locked. The volume unlocks itself when the application
                // exits. This is not very good form, consider storing these
                // volume handles in a structure associated with the disk
                // handle.
                //

                Handle = INVALID_HANDLE_VALUE;
            }

            if (Handle != INVALID_HANDLE_VALUE) {
                CloseHandle(Handle);
            }
        }

        Result = FindNextVolume(SearchHandle, Path, sizeof(Path));
        if (Result == FALSE) {
            break;
        }
    }

    FindVolumeClose(SearchHandle);
    return 0;
}

BOOL
SetupWin32IsVolumeInDisk (
    HANDLE VolumeHandle,
    DWORD DiskNumber
    )

/*++

Routine Description:

    This routine determines in the given volume resides on the given disk.

Arguments:

    VolumeHandle - Supplies the open handle to the volume.

    DiskNumber - Supplies the disk number to query.

Return Value:

    TRUE if the volume is on the disk.

    FALSE if the volume is not on the disk.

--*/

{

    DWORD BytesReturned;
    PDISK_EXTENT Extent;
    PVOLUME_DISK_EXTENTS Extents;
    ULONG Index;
    BOOL Result;

    Extents = malloc(SETUP_WIN32_DISK_LAYOUT_SIZE);
    if (Extents == NULL) {
        return FALSE;
    }

    Result = DeviceIoControl(VolumeHandle,
                             IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                             NULL,
                             0,
                             Extents,
                             SETUP_WIN32_DISK_LAYOUT_SIZE,
                             &BytesReturned,
                             NULL);

    if (Result == FALSE) {
        return FALSE;
    }

    Extent = Extents->Extents;
    for (Index = 0; Index < Extents->NumberOfDiskExtents; Index += 1) {
        if (Extent[Index].DiskNumber == DiskNumber) {
            free(Extents);
            return TRUE;
        }
    }

    free(Extents);
    return FALSE;
}

INT
SetupWin32FillInEntriesForDisk (
    PSETUP_WIN32_PARTITION_DESCRIPTION *Partitions,
    PULONG PartitionCount,
    PSTR DevicePath,
    PSTORAGE_DEVICE_NUMBER DeviceNumber,
    PDRIVE_LAYOUT_INFORMATION_EX DiskLayout,
    ULONGLONG BlockCount,
    ULONG BlockSize
    )

/*++

Routine Description:

    This routine adds the appropriate entries for the disk and its partitions
    to the resulting enumeration array.

Arguments:

    Partitions - Supplies a pointer that on input contains a pointer to the
        array of partitions to return. This array may be reallocated and
        expanded to contain the new entries.

    PartitionCount - Supplies a pointer that on input contains the number of
        elements in the partition array. On output this will reflect the new
        correct element count.

    DevicePath - Supplies a pointer to the device path.

    DeviceNumber - Supplies a pointer to the disk device number information.

    DiskLayout - Supplies a pointer to the disk layout information.

    BlockCount - Supplies the total number of blocks in the disk.

    BlockSize - Supplies the disk block size in bytes.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    SETUP_WIN32_PARTITION_DESCRIPTION NewEntry;
    PARTITION_INFORMATION_EX *Partition;
    ULONG PartitionIndex;
    INT Result;

    if (BlockCount == 0) {
        return 0;
    }

    //
    // Add the entry for the disk itself.
    //

    memset(&NewEntry, 0, sizeof(SETUP_WIN32_PARTITION_DESCRIPTION));
    NewEntry.DiskNumber = DeviceNumber->DeviceNumber;
    NewEntry.PartitionNumber = DeviceNumber->PartitionNumber;
    NewEntry.Partition.BlockSize = BlockSize;
    NewEntry.Partition.Number = DeviceNumber->PartitionNumber;
    NewEntry.Partition.FirstBlock = 0;
    NewEntry.Partition.LastBlock = BlockCount - 1;
    if (DiskLayout->PartitionStyle == PARTITION_STYLE_MBR) {
        NewEntry.Partition.PartitionFormat = PartitionFormatMbr;
        memcpy(NewEntry.Partition.DiskId,
               &(DiskLayout->Mbr.Signature),
               sizeof(ULONG));

    } else if (DiskLayout->PartitionStyle == PARTITION_STYLE_GPT) {
        NewEntry.Partition.PartitionFormat = PartitionFormatGpt;
        memcpy(NewEntry.Partition.DiskId,
               &(DiskLayout->Gpt.DiskId),
               DISK_IDENTIFIER_SIZE);

    } else if (DiskLayout->PartitionStyle == PARTITION_STYLE_RAW) {
        NewEntry.Partition.PartitionFormat = PartitionFormatNone;
    }

    NewEntry.Partition.Flags = PARTITION_FLAG_RAW_DISK;
    NewEntry.DevicePath = strdup(DevicePath);
    Result = SetupWin32AddPartitionEntry(Partitions, PartitionCount, &NewEntry);
    if (Result != 0) {
        goto Win32FillInEntriesForDiskEnd;
    }

    NewEntry.DevicePath = NULL;
    Partition = DiskLayout->PartitionEntry;
    for (PartitionIndex = 0;
         PartitionIndex < DiskLayout->PartitionCount;
         PartitionIndex += 1) {

        NewEntry.PartitionNumber = PartitionIndex + 1;
        NewEntry.Partition.Number = PartitionIndex + 1;
        NewEntry.Partition.FirstBlock = Partition->StartingOffset.QuadPart /
                                        BlockSize;

        NewEntry.Partition.LastBlock =
                         NewEntry.Partition.FirstBlock +
                         (Partition->PartitionLength.QuadPart / BlockSize) - 1;

        ZeroMemory(&(NewEntry.Partition.PartitionId),
                   PARTITION_IDENTIFIER_SIZE);

        ZeroMemory(&(NewEntry.Partition.PartitionTypeId),
                   PARTITION_TYPE_SIZE);

        NewEntry.Partition.Flags = 0;
        if (Partition->PartitionStyle == PARTITION_STYLE_MBR) {
            if (Partition->Mbr.PartitionType == PARTITION_ENTRY_UNUSED) {
                Partition += 1;
                continue;
            }

            memcpy(&(NewEntry.Partition.PartitionId[0]),
                   &(DiskLayout->Mbr.Signature),
                   sizeof(ULONG));

            memcpy(&(NewEntry.Partition.PartitionId[sizeof(ULONG)]),
                   &(Partition->PartitionNumber),
                   sizeof(ULONG));

            NewEntry.Partition.PartitionTypeId[0] =
                                                  Partition->Mbr.PartitionType;

            if (Partition->Mbr.BootIndicator != FALSE) {
                NewEntry.Partition.Flags |= PARTITION_FLAG_BOOT;
            }

        } else if (Partition->PartitionStyle == PARTITION_STYLE_GPT) {
            memcpy(&(NewEntry.Partition.PartitionId),
                   &(Partition->Gpt.PartitionId),
                   PARTITION_IDENTIFIER_SIZE);

            memcpy(&(NewEntry.Partition.PartitionTypeId),
                   &(Partition->Gpt.PartitionType),
                   PARTITION_TYPE_SIZE);
        }

        Result = SetupWin32AddPartitionEntry(Partitions,
                                             PartitionCount,
                                             &NewEntry);

        if (Result != 0) {
            goto Win32FillInEntriesForDiskEnd;
        }

        Partition += 1;
    }

Win32FillInEntriesForDiskEnd:
    return Result;
}

INT
SetupWin32AddPartitionEntry (
    PSETUP_WIN32_PARTITION_DESCRIPTION *Partitions,
    PULONG PartitionCount,
    PSETUP_WIN32_PARTITION_DESCRIPTION NewEntry
    )

/*++

Routine Description:

    This routine appends an entry to the partition array.

Arguments:

    Partitions - Supplies a pointer that on input contains a pointer to the
        array of partitions to return. This array may be reallocated and
        expanded to contain the new entries.

    PartitionCount - Supplies a pointer that on input contains the number of
        elements in the partition array. On output this will reflect the new
        correct element count.

    NewEntry - Supplies a pointer to the entry to append to the array.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID NewBuffer;
    ULONG NewSize;
    PVOID NextEntry;

    NewEntry->Partition.Version = PARTITION_DEVICE_INFORMATION_VERSION;
    NewSize = (*PartitionCount + 1) *
              sizeof(SETUP_WIN32_PARTITION_DESCRIPTION);

    NewBuffer = realloc(*Partitions, NewSize);
    if (NewBuffer == NULL) {
        return ENOMEM;
    }

    *Partitions = NewBuffer;
    NextEntry = NewBuffer +
                (*PartitionCount *
                 sizeof(SETUP_WIN32_PARTITION_DESCRIPTION));

    memcpy(NextEntry, NewEntry, sizeof(SETUP_WIN32_PARTITION_DESCRIPTION));
    *PartitionCount += 1;
    return 0;
}

