/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uefifw.h

Abstract:

    This header contains base definitions for the UEFI firmware implementations.
    This header is internal to the firmware implementation and is not exposed
    to UEFI applications or drivers.

Author:

    Evan Green 26-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/uefi/uefi.h>
#include "shortcut.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros convert to and from Binary Coded Decimal values.
//

#define EFI_BCD_TO_BINARY(_BcdValue) \
    ((((_BcdValue) >> 4) * 10) + ((_BcdValue) & 0x0F))

#define EFI_BINARY_TO_BCD(_BinaryValue) \
    ((((_BinaryValue) / 10) << 4) | ((_BinaryValue) % 10))

//
// ---------------------------------------------------------------- Definitions
//

#ifndef NOTHING

#define NOTHING

#endif

#define EFI_IDLE_LOOP_EVENT_GUID                            \
    {                                                       \
        0x3C8D294C, 0x5FC3, 0x4451,                         \
        {0xBB, 0x31, 0xC4, 0xC0, 0x32, 0x29, 0x5E, 0x6C}    \
    }

#define EFI_DEFAULT_SHELL_FILE_GUID                             \
    {                                                           \
        0x7C04A583, 0x9E3E, 0x4F1C,                             \
        {0xAD, 0x65, 0xE0, 0x52, 0x68, 0xD0, 0xB4, 0xD1}        \
    }

//
// Define the default watchdog timer duration that gets set when handing
// control to drivers and boot entries.
//

#define EFI_DEFAULT_WATCHDOG_DURATION (5 * 60)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
VOID
(*EFI_PLATFORM_BEGIN_INTERRUPT) (
    UINT32 *InterruptNumber,
    VOID **InterruptContext
    );

/*++

Routine Description:

    This routine is called when an interrupts comes in. The platform code is
    responsible for reporting the interrupt number. Interrupts are disabled at
    the processor core at this point.

Arguments:

    InterruptNumber - Supplies a pointer where interrupt line number will be
        returned.

    InterruptContext - Supplies a pointer where the platform can store a
        pointer's worth of context that will be passed back when ending the
        interrupt.

Return Value:

    None.

--*/

typedef
VOID
(*EFI_PLATFORM_HANDLE_INTERRUPT) (
    UINT32 InterruptNumber,
    VOID *InterruptContext
    );

/*++

Routine Description:

    This routine is called to handle a platform interrupt.

Arguments:

    InterruptNumber - Supplies the interrupt number that occurred.

    InterruptContext - Supplies the context returned by the interrupt
        controller when the interrupt began.

Return Value:

    None.

--*/

typedef
VOID
(*EFI_PLATFORM_END_INTERRUPT) (
    UINT32 InterruptNumber,
    VOID *InterruptContext
    );

/*++

Routine Description:

    This routine is called to finish handling of a platform interrupt. This is
    where the End-Of-Interrupt would get sent to the interrupt controller.

Arguments:

    InterruptNumber - Supplies the interrupt number that occurred.

    InterruptContext - Supplies the context returned by the interrupt
        controller when the interrupt began.

Return Value:

    None.

--*/

typedef
VOID
(*EFI_PLATFORM_SERVICE_TIMER_INTERRUPT) (
    UINT32 InterruptNumber
    );

/*++

Routine Description:

    This routine is called to acknowledge a platform timer interrupt. This
    routine is responsible for quiescing the interrupt.

Arguments:

    InterruptNumber - Supplies the interrupt number that occurred.

Return Value:

    None.

--*/

typedef
UINT64
(*EFI_PLATFORM_READ_TIMER) (
    VOID
    );

/*++

Routine Description:

    This routine is called to read the current platform time value. The timer
    is assumed to be free running at a constant frequency, and should have a
    bit width as reported in the initialize function. The UEFI core will
    manage software bit extension out to 64 bits, this routine should just
    reporte the hardware timer value.

Arguments:

    None.

Return Value:

    Returns the hardware timer value.

--*/

//
// -------------------------------------------------------------------- Globals
//

//
// Store the firmware version information.
//

extern UINT16 EfiVersionMajor;
extern UINT16 EfiVersionMinor;
extern UINT16 EfiVersionRevision;
extern UINT8 EfiVersionRelease;
extern UINT32 EfiEncodedVersion;
extern UINT32 EfiVersionSerial;
extern UINT32 EfiBuildTime;
extern CHAR8 *EfiBuildString;
extern CHAR8 *EfiProductName;
extern CHAR8 *EfiBuildTimeString;

//
// Define some well known GUIDs.
//

extern EFI_GUID EfiAcpiTableGuid;
extern EFI_GUID EfiAcpiTable1Guid;
extern EFI_GUID EfiBlockIoProtocolGuid;
extern EFI_GUID EfiDevicePathProtocolGuid;
extern EFI_GUID EfiDiskIoProtocolGuid;
extern EFI_GUID EfiDriverBindingProtocolGuid;
extern EFI_GUID EfiEventExitBootServicesGuid;
extern EFI_GUID EfiEventVirtualAddressChangeGuid;
extern EFI_GUID EfiEventMemoryMapChangeGuid;
extern EFI_GUID EfiEventReadyToBootGuid;
extern EFI_GUID EfiFileInformationGuid;
extern EFI_GUID EfiFirmwareVolume2ProtocolGuid;
extern EFI_GUID EfiGlobalVariableGuid;
extern EFI_GUID EfiGraphicsOutputProtocolGuid;
extern EFI_GUID EfiLoadedImageProtocolGuid;
extern EFI_GUID EfiLoadFileProtocolGuid;
extern EFI_GUID EfiLoadFile2ProtocolGuid;
extern EFI_GUID EfiPartitionTypeSystemPartitionGuid;
extern EFI_GUID EfiSimpleFileSystemProtocolGuid;
extern EFI_GUID EfiSimpleTextInputProtocolGuid;
extern EFI_GUID EfiSimpleTextOutputProtocolGuid;

//
// -------------------------------------------------------- Function Prototypes
//

//
// Define functions implemented by the platform-specific firmware, called by
// the firmware core.
//

//
// Debug transport routines
//

EFI_STATUS
EfiPlatformDebugDeviceReset (
    UINT32 BaudRate
    );

/*++

Routine Description:

    This routine attempts to initialize the serial UART used for debugging.

Arguments:

    BaudRate - Supplies the desired baud rate.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred while resetting the device.

    EFI_UNSUPPORTED if the given baud rate cannot be achieved.

--*/

EFI_STATUS
EfiPlatformDebugDeviceTransmit (
    VOID *Data,
    UINTN Size
    );

/*++

Routine Description:

    This routine transmits data from the host out through the debug device.

Arguments:

    Data - Supplies a pointer to the data to write.

    Size - Supplies the size to write, in bytes.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

EFI_STATUS
EfiPlatformDebugDeviceReceive (
    VOID *Data,
    UINTN *Size
    );

/*++

Routine Description:

    This routine receives incoming data from the debug device.

Arguments:

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_READY if there was no data to be read at the current time.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

EFI_STATUS
EfiPlatformDebugDeviceGetStatus (
    BOOLEAN *ReceiveDataAvailable
    );

/*++

Routine Description:

    This routine returns the current device status.

Arguments:

    ReceiveDataAvailable - Supplies a pointer where a boolean will be returned
        indicating whether or not receive data is available.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

VOID
EfiPlatformDebugDeviceDisconnect (
    VOID
    );

/*++

Routine Description:

    This routine disconnects a device, taking it offline.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Initialization routines
//

EFI_STATUS
EfiPlatformInitialize (
    UINT32 Phase
    );

/*++

Routine Description:

    This routine performs platform-specific firmware initialization.

Arguments:

    Phase - Supplies the iteration number this routine is being called on.
        Phase zero occurs very early, just after the debugger comes up.
        Phase one occurs a bit later, after timer and interrupt services are
        initialized. Phase two happens right before boot, after all platform
        devices have been enumerated.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfiPlatformGetInitialMemoryMap (
    EFI_MEMORY_DESCRIPTOR **Map,
    UINTN *MapSize
    );

/*++

Routine Description:

    This routine returns the initial platform memory map to the EFI core. The
    core maintains this memory map. The memory map returned does not need to
    take into account the firmware image itself or stack, the EFI core will
    reserve those regions automatically.

Arguments:

    Map - Supplies a pointer where the array of memory descriptors constituting
        the initial memory map is returned on success. The EFI core will make
        a copy of these descriptors, so they can be in read-only or
        temporary memory.

    MapSize - Supplies a pointer where the number of elements in the initial
        memory map will be returned on success.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfiPlatformInitializeInterrupts (
    EFI_PLATFORM_BEGIN_INTERRUPT *BeginInterruptFunction,
    EFI_PLATFORM_HANDLE_INTERRUPT *HandleInterruptFunction,
    EFI_PLATFORM_END_INTERRUPT *EndInterruptFunction
    );

/*++

Routine Description:

    This routine initializes support for platform interrupts. Interrupts are
    assumed to be disabled at the processor now. This routine should enable
    interrupts at the procesor core.

Arguments:

    BeginInterruptFunction - Supplies a pointer where a pointer to a function
        will be returned that is called when an interrupt occurs.

    HandleInterruptFunction - Supplies a pointer where a pointer to a function
        will be returned that is called to handle a platform-specific interurpt.
        NULL may be returned here.

    EndInterruptFunction - Supplies a pointer where a pointer to a function
        will be returned that is called to complete an interrupt.

Return Value:

    EFI Status code.

--*/

VOID
EfiPlatformTerminateInterrupts (
    VOID
    );

/*++

Routine Description:

    This routine terminates interrupt services in preparation for transitioning
    out of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

EFIAPI
EFI_STATUS
EfiPlatformSetWatchdogTimer (
    UINTN Timeout,
    UINT64 WatchdogCode,
    UINTN DataSize,
    CHAR16 *WatchdogData
    );

/*++

Routine Description:

    This routine sets the system's watchdog timer.

Arguments:

    Timeout - Supplies the number of seconds to set the timer for.

    WatchdogCode - Supplies a numeric code to log on a watchdog timeout event.

    DataSize - Supplies the size of the watchdog data.

    WatchdogData - Supplies an optional buffer that includes a null-terminated
        string, optionally followed by additional binary data.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the supplied watchdog code is invalid.

    EFI_UNSUPPORTED if there is no watchdog timer.

    EFI_DEVICE_ERROR if an error occurred accessing the device hardware.

--*/

EFI_STATUS
EfiPlatformInitializeTimers (
    UINT32 *ClockTimerInterruptNumber,
    EFI_PLATFORM_SERVICE_TIMER_INTERRUPT *ClockTimerServiceRoutine,
    EFI_PLATFORM_READ_TIMER *ReadTimerRoutine,
    UINT64 *ReadTimerFrequency,
    UINT32 *ReadTimerWidth
    );

/*++

Routine Description:

    This routine initializes platform timer services. There are actually two
    different timer services returned in this routine. The periodic timer tick
    provides a periodic interrupt. The read timer provides a free running
    counter value. These are likely serviced by different timers. For the
    periodic timer tick, this routine should start the periodic interrupts
    coming in. The periodic rate of the timer can be anything reasonable, as
    the time counter will be used to count actual duration. The rate should be
    greater than twice the rollover rate of the time counter to ensure proper
    time accounting. Interrupts are disabled at the processor core for the
    duration of this routine.

Arguments:

    ClockTimerInterruptNumber - Supplies a pointer where the interrupt line
        number of the periodic timer tick will be returned.

    ClockTimerServiceRoutine - Supplies a pointer where a pointer to a routine
        called when the periodic timer tick interrupt occurs will be returned.

    ReadTimerRoutine - Supplies a pointer where a pointer to a routine
        called to read the current timer value will be returned.

    ReadTimerFrequency - Supplies the frequency of the counter.

    ReadTimerWidth - Supplies a pointer where the read timer bit width will be
        returned.

Return Value:

    EFI Status code.

--*/

VOID
EfiPlatformTerminateTimers (
    VOID
    );

/*++

Routine Description:

    This routine terminates timer services in preparation for the termination
    of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

EFI_STATUS
EfiPlatformEnumerateFirmwareVolumes (
    VOID
    );

/*++

Routine Description:

    This routine enumerates any firmware volumes the platform may have
    tucked away. The platform should load them into memory and call
    EfiCreateFirmwareVolume for each one.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfiPlatformEnumerateDevices (
    VOID
    );

/*++

Routine Description:

    This routine enumerates and connects any builtin devices the platform
    contains.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfiPlatformRuntimeInitialize (
    VOID
    );

/*++

Routine Description:

    This routine performs platform-specific firmware initialization in the
    runtime core driver. The runtime routines are in a separate binary from the
    firmware core routines as they need to be relocated for runtime. This
    routine should perform platform-specific initialization needed to provide
    the core runtime services.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

//
// Variable services, implemented in the runtime core.
//

EFI_STATUS
EfiPlatformReadNonVolatileData (
    VOID *Data,
    UINTN DataSize
    );

/*++

Routine Description:

    This routine reads the EFI variable data from non-volatile storage.

Arguments:

    Data - Supplies a pointer where the platform returns the non-volatile
        data.

    DataSize - Supplies the size of the data to return.

Return Value:

    EFI_SUCCESS if some data was successfully loaded.

    EFI_UNSUPPORTED if the platform does not have non-volatile storage. In this
    case the firmware core saves the non-volatile variables to a file on the
    EFI system partition, and the variable library hopes to catch the same
    variable buffer on reboots to see variable writes that happened at
    runtime.

    EFI_DEVICE_IO_ERROR if a device error occurred during the operation.

    Other error codes on other failures.

--*/

EFI_STATUS
EfiPlatformWriteNonVolatileData (
    VOID *Data,
    UINTN DataSize
    );

/*++

Routine Description:

    This routine writes the EFI variable data to non-volatile storage.

Arguments:

    Data - Supplies a pointer to the data to write.

    DataSize - Supplies the size of the data to write, in bytes.

Return Value:

    EFI_SUCCESS if some data was successfully loaded.

    EFI_UNSUPPORTED if the platform does not have non-volatile storage. In this
    case the firmware core saves the non-volatile variables to a file on the
    EFI system partition, and the variable library hopes to catch the same
    variable buffer on reboots to see variable writes that happened at
    runtime.

    EFI_DEVICE_IO_ERROR if a device error occurred during the operation.

    Other error codes on other failures.

--*/

VOID
EfiPlatformRuntimeExitBootServices (
    VOID
    );

/*++

Routine Description:

    This routine is called in the runtime core driver when the firmware is in
    the process of terminating boot services. The platform can do any work it
    needs to prepare for the imminent termination of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfiPlatformRuntimeVirtualAddressChange (
    VOID
    );

/*++

Routine Description:

    This routine is called in the runtime core driver when the firmware is
    converting to virtual address mode. It should convert any pointers it's
    got. This routine is called after ExitBootServices, so no EFI boot services
    are available.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Define functions implemented by the UEFI core that platform functions can
// call.
//

VOID
EfiCoreMain (
    VOID *FirmwareBaseAddress,
    VOID *FirmwareLowestAddress,
    UINTN FirmwareSize,
    CHAR8 *FirmwareBinaryName,
    VOID *StackBase,
    UINTN StackSize
    );

/*++

Routine Description:

    This routine implements the entry point into the UEFI firmware. This
    routine is called by the platform firmware, and should be called as early
    as possible. It will perform callouts to allow the platform to initialize
    further.

Arguments:

    FirmwareBaseAddress - Supplies the base address where the firmware was
        loaded into memory. Supply -1 to indicate that the image is loaded at
        its preferred base address and was not relocated.

    FirmwareLowestAddress - Supplies the lowest address where the firmware was
        loaded into memory.

    FirmwareSize - Supplies the size of the firmware image in memory, in bytes.

    FirmwareBinaryName - Supplies the name of the binary that's loaded, which
        is reported to the debugger for symbol loading.

    StackBase - Supplies the base (lowest) address of the stack.

    StackSize - Supplies the size in bytes of the stack. This should be at
        least 0x4000 bytes (16kB).

Return Value:

    This routine does not return.

--*/

EFI_STATUS
EfiCreateFirmwareVolume (
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64 Length,
    EFI_HANDLE ParentHandle,
    UINT32 AuthenticationStatus,
    EFI_HANDLE *BlockIoProtocol
    );

/*++

Routine Description:

    This routine creates a firmware volume out of the given memory buffer.
    Specifically, this function creates a handle and adds the Firmware Block
    I/O protocol and the Device Path protocol to it. The firmware volume
    protocol will then attach after noticing the block I/O protocol instance.

Arguments:

    BaseAddress - Supplies the physical address of the firmware volume buffer.

    Length - Supplies the length of the firmware volume buffer in bytes.

    ParentHandle - Supplies an optional handle to a parent firmware volume this
        volume is being enumerated from.

    AuthenticationStatus - Supplies the authentication status of this volume if
        this volume came from another file and volume.

    BlockIoProtocol - Supplies an optional pointer where the created handle
        will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_VOLUME_CORRUPTED if the volume was not valid.

    EFI_OUT_OF_RESOURCES on allocation failure.

--*/

EFI_STATUS
EfiCoreEnumerateRamDisk (
    EFI_PHYSICAL_ADDRESS Base,
    UINT64 Size
    );

/*++

Routine Description:

    This routine enumerates a RAM disk at the given address.

Arguments:

    Base - Supplies the base physical address of the RAM disk.

    Size - Supplies the size of the RAM disk.

Return Value:

    EFI Status code.

--*/

EFIAPI
EFI_STATUS
EfiCoreFlushVariableData (
    VOID
    );

/*++

Routine Description:

    This routine attempts to write variable data out to non-volatile storage.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiAcpiInstallTable (
    VOID *AcpiTableBuffer,
    UINTN AcpiTableBufferSize,
    UINTN *TableKey
    );

/*++

Routine Description:

    This routine installs an ACPI table into the RSDT/XSDT.

Arguments:

    AcpiTableBuffer - Supplies a pointer to the buffer containing the ACPI
        table to insert.

    AcpiTableBufferSize - Supplies the size in bytes of the ACPI table buffer.

    TableKey - Supplies a pointer where a key will be returned that refers
        to the table.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiAcpiUninstallTable (
    UINTN TableKey
    );

/*++

Routine Description:

    This routine uninstalls a previously install ACPI table.

Arguments:

    TableKey - Supplies the key returned when the table was installed.

Return Value:

    EFI status code.

--*/

EFIAPI
VOID
EfiAcpiChecksumTable (
    VOID *Buffer,
    UINTN Size,
    UINTN ChecksumOffset
    );

/*++

Routine Description:

    This routine checksums an ACPI table.

Arguments:

    Buffer - Supplies a pointer to the table to checksum.

    Size - Supplies the size of the table in bytes.

    ChecksumOffset - Supplies the offset of the 8 bit checksum field.

Return Value:

    None.

--*/

EFIAPI
VOID *
EfiGetAcpiTable (
    UINT32 Signature,
    VOID *PreviousTable
    );

/*++

Routine Description:

    This routine attempts to find an ACPI description table with the given
    signature. This routine does not validate the checksum of the table.

Arguments:

    Signature - Supplies the signature of the desired table.

    PreviousTable - Supplies an optional pointer to the table to start the
        search from.

Return Value:

    Returns a pointer to the beginning of the header to the table if the table
    was found, or NULL if the table could not be located.

--*/

EFIAPI
EFI_STATUS
EfiSmbiosAddStructure (
    VOID *Table,
    ...
    );

/*++

Routine Description:

    This routine adds an SMBIOS structure to the SMBIOS table.

Arguments:

    Table - Supplies a pointer to the table to add. A copy of this data will be
        made. The length of the table must be correctly filled in.

    ... - Supplies an array of pointers to strings to add to the end of the
        table. This list must be terminated with a NULL.

Return Value:

    EFI_SUCCESS on success.

    EFI_INSUFFICIENT_RESOURCES if a memory allocation failed.

--*/

EFI_STATUS
EfiConvertCounterToEfiTime (
    INT64 Counter,
    EFI_TIME *EfiTime
    );

/*++

Routine Description:

    This routine converts from a second-based counter value to an EFI time
    structure.

Arguments:

    Counter - Supplies the count of seconds since January 1, 2001 GMT.

    EfiTime - Supplies a pointer where the EFI time fields will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the counter could not be converted.

--*/

EFI_STATUS
EfiConvertEfiTimeToCounter (
    EFI_TIME *EfiTime,
    INT64 *Counter
    );

/*++

Routine Description:

    This routine converts from an EFI time structure into the number of seconds
    since January 1, 2001 GMT.

Arguments:

    EfiTime - Supplies a pointer to the EFI time to convert.

    Counter - Supplies a pointer where the count of seconds will be returned
        on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the counter could not be converted.

--*/

BOOLEAN
EfiDivideUnsigned64 (
    UINT64 Dividend,
    UINT64 Divisor,
    UINT64 *QuotientOut,
    UINT64 *RemainderOut
    );

/*++

Routine Description:

    This routine performs a 64-bit divide of two unsigned numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    QuotientOut - Supplies a pointer that receives the result of the divide.
        This parameter may be NULL.

    RemainderOut - Supplies a pointer that receives the remainder of the
        divide. This parameter may be NULL.

Return Value:

    Returns TRUE if the operation was successful, or FALSE if there was an
    error (like divide by 0).

--*/

BOOLEAN
EfiDivide64 (
    INT64 Dividend,
    INT64 Divisor,
    INT64 *QuotientOut,
    INT64 *RemainderOut
    );

/*++

Routine Description:

    This routine performs a 64-bit divide of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    QuotientOut - Supplies a pointer that receives the result of the divide.
        This parameter may be NULL.

    RemainderOut - Supplies a pointer that receives the remainder of the
        divide. This parameter may be NULL.

Return Value:

    Returns TRUE if the operation was successful, or FALSE if there was an
    error (like divide by 0).

--*/

//
// Interrupt functions
//

BOOLEAN
EfiDisableInterrupts (
    VOID
    );

/*++

Routine Description:

    This routine disables all interrupts on the current processor.

Arguments:

    None.

Return Value:

    TRUE if interrupts were previously enabled on the processor.

    FALSE if interrupts were not previously enabled on the processor.

--*/

VOID
EfiEnableInterrupts (
    VOID
    );

/*++

Routine Description:

    This routine enables interrupts on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/

BOOLEAN
EfiAreInterruptsEnabled (
    VOID
    );

/*++

Routine Description:

    This routine determines whether or not interrupts are currently enabled
    on the processor.

Arguments:

    None.

Return Value:

    TRUE if interrupts are enabled in the processor.

    FALSE if interrupts are globally disabled.

--*/

VOID
EfiCoreInvalidateInstructionCacheRange (
    VOID *Address,
    UINTN Length
    );

/*++

Routine Description:

    This routine invalidates a region of memory in the instruction cache.

Arguments:

    Address - Supplies the address to invalidate. If translation is enabled,
        this is a virtual address.

    Length - Supplies the number of bytes in the region to invalidate.

Return Value:

    None.

--*/

BOOLEAN
EfiIsAtRuntime (
    VOID
    );

/*++

Routine Description:

    This routine determines whether or not the system has gone through
    ExitBootServices.

Arguments:

    None.

Return Value:

    TRUE if the system has gone past ExitBootServices and is now in the
    Runtime phase.

    FALSE if the system is currently in the Boot phase.

--*/

//
// I/O port functions (only applicable to PC platforms).
//

UINT8
EfiIoPortIn8 (
    UINT16 Address
    );

/*++

Routine Description:

    This routine performs an 8-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

UINT16
EfiIoPortIn16 (
    UINT16 Address
    );

/*++

Routine Description:

    This routine performs a 16-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

UINT32
EfiIoPortIn32 (
    UINT16 Address
    );

/*++

Routine Description:

    This routine performs a 32-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

VOID
EfiIoPortOut8 (
    UINT16 Address,
    UINT8 Value
    );

/*++

Routine Description:

    This routine performs an 8-bit write to the given I/O port.

Arguments:

    Address - Supplies the address to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

VOID
EfiIoPortOut16 (
    UINT16 Address,
    UINT16 Value
    );

/*++

Routine Description:

    This routine performs a 16-bit write to the given I/O port.

Arguments:

    Address - Supplies the address to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

VOID
EfiIoPortOut32 (
    UINT16 Address,
    UINT32 Value
    );

/*++

Routine Description:

    This routine performs a 32-bit write to the given I/O port.

Arguments:

    Address - Supplies the address to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

//
// Safe memory-mapped hardware register access functions
//

UINT32
EfiReadRegister32 (
    VOID *RegisterAddress
    );

/*++

Routine Description:

    This routine performs a 32-bit memory register read. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to read.

Return Value:

    Returns the value at the given register.

--*/

VOID
EfiWriteRegister32 (
    VOID *RegisterAddress,
    UINT32 Value
    );

/*++

Routine Description:

    This routine performs a 32-bit memory register write. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

UINT16
EfiReadRegister16 (
    VOID *RegisterAddress
    );

/*++

Routine Description:

    This routine performs a 16-bit memory register read. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to read.

Return Value:

    Returns the value at the given register.

--*/

VOID
EfiWriteRegister16 (
    VOID *RegisterAddress,
    UINT16 Value
    );

/*++

Routine Description:

    This routine performs a 16-bit memory register write. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

UINT8
EfiReadRegister8 (
    VOID *RegisterAddress
    );

/*++

Routine Description:

    This routine performs an 8-bit memory register read. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to read.

Return Value:

    Returns the value at the given register.

--*/

VOID
EfiWriteRegister8 (
    VOID *RegisterAddress,
    UINT8 Value
    );

/*++

Routine Description:

    This routine performs an 8-bit memory register write. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

VOID
EfiMemoryBarrier (
    VOID
    );

/*++

Routine Description:

    This routine provides a full memory barrier, ensuring that all memory
    accesses occurring before this function complete before any memory accesses
    after this function start.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfiDebugPrint (
    CHAR8 *Format,
    ...
    );

/*++

Routine Description:

    This routine prints to the debugger and console.

Arguments:

    Format - Supplies a pointer to the format string.

    ... - Supplies the remaining arguments to the format string.

Return Value:

    None.

--*/
