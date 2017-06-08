/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

    ULONG BytesRead;
    PVOID Data;
    ULONG DataSize;
    REGISTERS_UNION LocalRegisters;
    ULONGLONG Preemptions;
    ULONG Registers32[7];
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

    switch (TargetInformation.MachineType) {
    case MACHINE_TYPE_X86:

        //
        // The stack should look like: magic, flags, esp, edi, esi, ebx, ebp,
        // eip.
        // ESP is after the call, so ignore that one.
        //

        Status = DbgReadMemory(Context,
                               TRUE,
                               StackPointer + sizeof(ULONG),
                               sizeof(ULONG) * 7,
                               Registers32,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != sizeof(ULONG) * 7)) {
            DbgOut("Error: Could not get thread registers at 0x%08I64x.\n",
                   StackPointer + 4);

            if (Status == 0) {
                Status = EINVAL;
            }

            return Status;
        }

        LocalRegisters.X86.Eflags = Registers32[0];
        LocalRegisters.X86.Esp = StackPointer + (sizeof(ULONG) * 8);
        LocalRegisters.X86.Edi = Registers32[2];
        LocalRegisters.X86.Esi = Registers32[3];
        LocalRegisters.X86.Ebx = Registers32[4];
        LocalRegisters.X86.Ebp = Registers32[5];
        LocalRegisters.X86.Eip = Registers32[6];
        break;

    case MACHINE_TYPE_ARM:

        //
        // The context swap code does push {r4-r12,r14}, so read all those
        // off.
        //

        Status = DbgReadMemory(Context,
                               TRUE,
                               StackPointer + sizeof(ULONG),
                               10 * sizeof(ULONG),
                               &(LocalRegisters.Arm.R4),
                               &BytesRead);

        if ((Status != 0) || (BytesRead != 10 * sizeof(ULONG))) {
            DbgOut("Error: Could not register context at 0x%08I64x.\n",
                   StackPointer + 4);

            if (Status == 0) {
                Status = EINVAL;
            }

            return Status;
        }

        //
        // Put that last R14 and CPSR back in its spot.
        //

        LocalRegisters.Arm.R15Pc = LocalRegisters.Arm.R13Sp;
        LocalRegisters.Arm.Cpsr = LocalRegisters.Arm.R12Ip;
        LocalRegisters.Arm.R12Ip = 0;
        LocalRegisters.Arm.R13Sp = StackPointer + 4 + (10 * sizeof(ULONG));
        break;

    case MACHINE_TYPE_X64:

        //
        // TODO: Make !thread work for x64 once context switching is added.
        //

        DbgOut("TODO: X64 !thread\n");
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

