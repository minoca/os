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

#include <minoca/driver.h>
#include "dbgext.h"

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

    ULONGLONG BasePointer;
    ULONG BytesRead;
    ULONGLONG InstructionPointer;
    REGISTERS_UNION LocalRegisters;
    ULONGLONG StackPointer;
    INT Status;
    DEBUG_TARGET_INFORMATION TargetInformation;
    KTHREAD Thread;
    ULONGLONG ThreadAddress;
    PSTR ThreadName;

    if ((Command != NULL) || (ArgumentCount != 2)) {
        DbgOut("Usage: !thread <ThreadAddress>.\n"
               "       The thread extension prints out the contents of a "
               " thread object.\n"
               "       ThreadAddress - Supplies the address of the thread to "
               "dump.\n");

        return EINVAL;
    }

    memset(&LocalRegisters, 0, sizeof(REGISTERS_UNION));

    //
    // Get the address of the thread and read in the structure.
    //

    Status = DbgEvaluate(Context, ArgumentValues[1], &ThreadAddress);
    if (Status != 0) {
        DbgOut("Error: Unable to evaluate Address parameter.\n");
        return Status;
    }

    DbgOut("Dumping Thread at 0x%08I64x ", ThreadAddress);
    Status = DbgReadMemory(Context,
                           TRUE,
                           ThreadAddress,
                           sizeof(KTHREAD),
                           &Thread,
                           &BytesRead);

    if ((Status != 0) || (BytesRead != sizeof(KTHREAD))) {
        DbgOut("Error: Could not read thread.\n");
        if (Status == 0) {
            Status = EINVAL;
        }

        return Status;
    }

    if (Thread.Header.Type != ObjectThread) {
        DbgOut("Probably not a thread, has an object type %d instead of %d.\n",
               Thread.Header.Type,
               ObjectThread);

        return EINVAL;
    }

    //
    // If the thread has a name, attempt to read that in and print it.
    //

    if (Thread.Header.Name != NULL) {
        DbgOut("Name: ");
        ThreadName = MALLOC(MAX_THREAD_NAME + 1);
        if (ThreadName == NULL) {
            DbgOut("Error: Could not allocate memory\n");
            return ENOMEM;;
        }

        memset(ThreadName, 0, MAX_THREAD_NAME + 1);
        Status = DbgReadMemory(Context,
                               TRUE,
                               (UINTN)Thread.Header.Name,
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

    DbgOut("Process %08x ID 0x%x ", Thread.OwningProcess, Thread.ThreadId);
    if ((Thread.Flags & THREAD_FLAG_USER_MODE) != 0) {
        DbgOut("UserMode ");

    } else {
        DbgOut("KernelMode ");
    }

    DbgPrintAddressSymbol(Context, (UINTN)Thread.ThreadRoutine);
    DbgOut("\nState: ");
    DbgPrintType(Context,
                 "kernel!THREAD_STATE",
                 (PVOID)&(Thread.State),
                 sizeof(THREAD_STATE));

    if (Thread.State == ThreadStateBlocked) {
        DbgOut(" on %08x", Thread.WaitBlock);
    }

    DbgOut(" Runs: %I64d, Preemptions %I64d Yields %I64d",
           Thread.ResourceUsage.Preemptions + Thread.ResourceUsage.Yields,
           Thread.ResourceUsage.Preemptions,
           Thread.ResourceUsage.Yields);

    DbgOut("\n\n");

    //
    // To avoid bad memory accesses, avoid printing call stacks for non-living
    // or currently running threads.
    //

    if ((Thread.State == ThreadStateRunning) ||
        (Thread.State == ThreadStateExited) ||
        (Thread.State == ThreadStateFirstTime)) {

        return 0;
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
    StackPointer = (UINTN)Thread.KernelStackPointer;
    BasePointer = (UINTN)NULL;
    switch (TargetInformation.MachineType) {
    case MACHINE_TYPE_X86:
        Status = DbgReadMemory(Context,
                               TRUE,
                               StackPointer + 24,
                               sizeof(ULONG),
                               &BasePointer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != sizeof(ULONG))) {
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
                               sizeof(ULONG),
                               &InstructionPointer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != sizeof(ULONG))) {
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

    case MACHINE_TYPE_ARMV7:
    case MACHINE_TYPE_ARMV6:
        Status = DbgReadMemory(Context,
                               TRUE,
                               StackPointer + 32,
                               sizeof(ULONG),
                               &BasePointer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != sizeof(ULONG))) {
            DbgOut("Error: Could not get base pointer at 0x%08I64x.\n",
                   StackPointer + 32);

            if (Status == 0) {
                Status = EINVAL;
            }

            return Status;
        }

        Status = DbgReadMemory(Context,
                               TRUE,
                               StackPointer + 36,
                               sizeof(ULONG),
                               &InstructionPointer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != sizeof(ULONG))) {
            DbgOut("Error: Could not get return address at 0x%08I64x.\n",
                   StackPointer + 36);

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
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

