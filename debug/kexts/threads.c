/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    threads.c

Abstract:

    This module implements thread related debugger extensions.

Author:

    Evan Green 4-Oct-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/kernel/arm.h>
#include <minoca/debug/dbgext.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MAX_THREAD_NAME 100

#define MALLOC(_x) malloc(_x)
#define FREE(_x) free(_x)

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

INT
ExtThread (
    PDEBUGGER_CONTEXT Context,
    PSTR Command,
    ULONG ArgumentCount,
    PSTR *ArgumentValues
    )

/*++

Routine Description:

    This routine prints out the contents of a Memory Descriptor List. Arguments
    to the extension are:

        Address - Supplies the address of the MDL.

Arguments:

    Context - Supplies a pointer to the debugger applicaton context, which is
        an argument to most of the API functions.

    Command - Supplies the subcommand entered. This parameter is unused.

    ArgumentCount - Supplies the number of arguments in the ArgumentValues
        array.

    ArgumentValues - Supplies the values of each argument. This memory will be
        reused when the function returns, so extensions must not touch this
        memory after returning from this call.

Return Value:

    0 if the debugger extension command was successful.

    Returns an error code if a failure occurred along the way.

--*/

{

    ULONG AddressSize;
    ULONGLONG BasePointer;
    ULONGLONG BasePointerAddress;
    ULONG BytesRead;
    PVOID Data;
    ULONG DataSize;
    ULONGLONG InstructionPointer;
    REGISTERS_UNION LocalRegisters;
    ULONGLONG Preemptions;
    ULONGLONG StackPointer;
    ULONGLONG State;
    INT Status;
    DEBUG_TARGET_INFORMATION TargetInformation;
    ULONGLONG ThreadAddress;
    PSTR ThreadName;
    PTYPE_SYMBOL ThreadType;
    ULONGLONG Value;
    ULONGLONG Yields;

    Data = NULL;
    if ((Command != NULL) || (ArgumentCount != 2)) {
        DbgOut("Usage: !thread <ThreadAddress>.\n"
               "       The thread extension prints out the contents of a "
               " thread object.\n"
               "       ThreadAddress - Supplies the address of the thread to "
               "dump.\n");

        return EINVAL;
    }

    AddressSize = DbgGetTargetPointerSize(Context);
    memset(&LocalRegisters, 0, sizeof(REGISTERS_UNION));

    //
    // Get the address of the thread and read in the structure.
    //

    Status = DbgEvaluate(Context, ArgumentValues[1], &ThreadAddress);
    if (Status != 0) {
        DbgOut("Error: Unable to evaluate Address parameter.\n");
        goto ExtThreadEnd;
    }

    DbgOut("Dumping Thread at 0x%08I64x ", ThreadAddress);
    Status = DbgReadTypeByName(Context,
                               ThreadAddress,
                               "KTHREAD",
                               &ThreadType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read KTHREAD at 0x%I64x.\n", ThreadAddress);
        goto ExtThreadEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "Header.Type",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    if (Value != ObjectThread) {
        DbgOut("Probably not a thread, has an object type %I64d instead of "
               "%d.\n",
               Value,
               ObjectThread);

        Status = EINVAL;
        goto ExtThreadEnd;
    }

    //
    // If the thread has a name, attempt to read that in and print it.
    //

    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "Header.Name",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    if (Value != 0) {
        DbgOut("Name: ");
        ThreadName = MALLOC(MAX_THREAD_NAME + 1);
        if (ThreadName == NULL) {
            DbgOut("Error: Could not allocate memory\n");
            return ENOMEM;;
        }

        memset(ThreadName, 0, MAX_THREAD_NAME + 1);
        Status = DbgReadMemory(Context,
                               TRUE,
                               Value,
                               MAX_THREAD_NAME,
                               ThreadName,
                               &BytesRead);

        if ((Status != 0) || (BytesRead == 0)) {
            DbgOut("Error: Could not read thread name.\n");

        } else {
            DbgOut("%s\n", ThreadName);
        }

        FREE(ThreadName);
    }

    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "OwningProcess",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    DbgOut("Process 0x%08I64x ID ", Value);
    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "ThreadId",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    DbgOut("%I64d, Flags: ", Value);
    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "Flags",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    DbgOut("0x%I64x", Value);
    if ((Value & THREAD_FLAG_USER_MODE) != 0) {
        DbgOut(" UserMode ");

    } else {
        DbgOut(" KernelMode ");
    }

    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "ThreadRoutine",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    DbgPrintAddressSymbol(Context, Value);
    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "ThreadParameter",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    DbgOut(" (Param 0x%I64x)", Value);
    DbgOut("\nState: ");
    DbgPrintTypeMember(Context,
                       ThreadAddress,
                       Data,
                       DataSize,
                       ThreadType,
                       "State",
                       0,
                       0);

    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "State",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &State);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    if (State == ThreadStateBlocked) {
        Status = DbgReadIntegerMember(Context,
                                      ThreadType,
                                      "WaitBlock",
                                      ThreadAddress,
                                      Data,
                                      DataSize,
                                      &Value);

        if (Status != 0) {
            goto ExtThreadEnd;
        }

        DbgOut(" on 0x%08I64x", Value);
    }

    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "ResourceUsage.Preemptions",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &Preemptions);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "ResourceUsage.Yields",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &Yields);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    DbgOut(" Runs: %I64d, Preemptions %I64d Yields %I64d",
           Preemptions + Yields,
           Preemptions,
           Yields);

    DbgOut("\n\n");

    //
    // To avoid bad memory accesses, avoid printing call stacks for non-living
    // or currently running threads.
    //

    if ((State == ThreadStateRunning) ||
        (State == ThreadStateExited) ||
        (State == ThreadStateFirstTime)) {

        Status = 0;
        goto ExtThreadEnd;
    }

    //
    // Get the target information, including the architecture being debugged.
    //

    Status = DbgGetTargetInformation(Context,
                                     &TargetInformation,
                                     sizeof(DEBUG_TARGET_INFORMATION));

    if (Status != 0) {
        DbgOut("Error getting debug target information.\n");
        return 0;
    }

    //
    // Determine the instruction pointer, stack pointer, and base pointer,
    // which are all needed for printing the call stack.
    //

    InstructionPointer = (UINTN)NULL;
    Status = DbgReadIntegerMember(Context,
                                  ThreadType,
                                  "KernelStackPointer",
                                  ThreadAddress,
                                  Data,
                                  DataSize,
                                  &StackPointer);

    if (Status != 0) {
        goto ExtThreadEnd;
    }

    BasePointer = (UINTN)NULL;
    switch (TargetInformation.MachineType) {
    case MACHINE_TYPE_X86:
        Status = DbgReadMemory(Context,
                               TRUE,
                               StackPointer + 24,
                               AddressSize,
                               &BasePointer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != AddressSize)) {
            DbgOut("Error: Could not get base pointer at 0x%08I64x.\n",
                   StackPointer + 24);

            if (Status == 0) {
                Status = EINVAL;
            }

            return Status;
        }

        Status = DbgReadMemory(Context,
                               TRUE,
                               StackPointer + 28,
                               AddressSize,
                               &InstructionPointer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != AddressSize)) {
            DbgOut("Error: Could not get return address at 0x%08I64x.\n",
                   StackPointer + 28);

            if (Status == 0) {
                Status = EINVAL;
            }

            return Status;
        }

        LocalRegisters.X86.Eip = InstructionPointer;
        LocalRegisters.X86.Ebp = BasePointer;
        LocalRegisters.X86.Esp = BasePointer;
        break;

    case MACHINE_TYPE_ARM:
        Status = DbgReadMemory(Context,
                               TRUE,
                               StackPointer + 40,
                               AddressSize,
                               &InstructionPointer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != AddressSize)) {
            DbgOut("Error: Could not get return address at 0x%08I64x.\n",
                   StackPointer + 36);

            if (Status == 0) {
                Status = EINVAL;
            }

            return Status;
        }

        BasePointerAddress = StackPointer + 32;
        if ((InstructionPointer & ARM_THUMB_BIT) != 0) {
            BasePointerAddress = StackPointer + 16;
        }

        Status = DbgReadMemory(Context,
                               TRUE,
                               BasePointerAddress,
                               AddressSize,
                               &BasePointer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != AddressSize)) {
            DbgOut("Error: Could not get base pointer at 0x%08I64x.\n",
                   BasePointerAddress);

            if (Status == 0) {
                Status = EINVAL;
            }

            return Status;
        }

        LocalRegisters.Arm.R15Pc = InstructionPointer;
        LocalRegisters.Arm.R11Fp = BasePointer;
        LocalRegisters.Arm.R7 = BasePointer;
        LocalRegisters.Arm.R13Sp = BasePointer;
        break;

    default:
        DbgOut("Error: Unknown machine type %d.\n",
               TargetInformation.MachineType);

        return EINVAL;
    }

    //
    // Print the call stack for the given thread.
    //

    DbgPrintCallStack(Context, &LocalRegisters, FALSE);
    Status = 0;

ExtThreadEnd:
    if (Data != NULL) {
        free(Data);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

