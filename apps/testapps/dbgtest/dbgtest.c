/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgtest.c

Abstract:

    This module implements the tests used to verify that the debug API is
    working properly.

Author:

    Evan Green 15-May-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>
#include <minoca/debug/dbgproto.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>

//
// --------------------------------------------------------------------- Macros
//

#define DBGTEST_PRINT(...)      \
    if (DbgTestVerbose != FALSE) { \
        printf(__VA_ARGS__);          \
    }

#define DBGTEST_ERROR(...) printf(__VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

#define DEBUG_BREAK_COUNT 5
#define MODULE_LIST_BUFFER_SIZE 256

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
RunAllDebugTests (
    VOID
    );

ULONG
TestBasicDebugConnection (
    VOID
    );

void
UserSignalHandler (
    int SignalNumber,
    siginfo_t *SignalInformation,
    void *Context
    );

void
User2SignalHandler (
    int SignalNumber,
    siginfo_t *SignalInformation,
    void *Context
    );

VOID
TestThreadSpinForever (
    PVOID Parameter
    );

VOID
PrintRegisterContents (
    PBREAK_NOTIFICATION Break
    );

VOID
PrintSignalParameters (
    PSIGNAL_PARAMETERS Parameters
    );

ULONG
SingleStep (
    pid_t Child
    );

ULONG
RangeStep (
    pid_t Child
    );

VOID
ChildSignalLoop (
    pid_t Child,
    volatile UCHAR *Stop
    );

ULONG
PrintLoadedModules (
    pid_t Child
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to enable more verbose debug output.
//

BOOL DbgTestVerbose = FALSE;
BOOL DbgTestPrintRegisters = FALSE;

//
// Keep track of whether or not the child has initialized.
//

volatile BOOL ChildInitialized;

//
// Remember the number of signals received.
//

volatile ULONG User2SignalsReceived;

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the debug test program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG TestsCompleted;

    TestsCompleted = 0;
    while (RunAllDebugTests() == 0) {
        printf("%d: ", TestsCompleted);
        TestsCompleted += 1;
    }

    return 1;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
RunAllDebugTests (
    VOID
    )

/*++

Routine Description:

    This routine executes all debug tests.

Arguments:

    None.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    ULONG Failures;

    Failures = 0;
    Failures += TestBasicDebugConnection();
    if (Failures != 0) {
        DBGTEST_ERROR("*** %d failures in debug tests. ***\n", Failures);

    } else {
        printf("All debug tests pass.\n");
    }

    return Failures;
}

ULONG
TestBasicDebugConnection (
    VOID
    )

/*++

Routine Description:

    This routine tests that a process can perform basic trace operations on
    another process.

Arguments:

    None.

Return Value:

    Returns the number of failures in the test.

--*/

{

    BREAK_NOTIFICATION Break;
    pid_t Child;
    volatile ULONG ChildInitializing;
    ULONG ChildThreadCount;
    ULONG ChildThreadIndex;
    int ExitStatus;
    ULONG Failures;
    struct sigaction OriginalUser1Action;
    struct sigaction OriginalUser2Action;
    pid_t Parent;
    BOOL RestoreUser1;
    int Result;
    SIGNAL_PARAMETERS SignalParameters;
    KSTATUS Status;
    volatile UCHAR Stop;
    ULONG TraceRound;
    struct sigaction User1Action;
    struct sigaction User2Action;
    pid_t WaitPid;

    ChildThreadCount = 5;
    Failures = 0;
    RestoreUser1 = FALSE;
    Stop = FALSE;
    ChildInitialized = FALSE;

    //
    // Wire up the SIGUSR1 handler.
    //

    memset(&User1Action, 0, sizeof(struct sigaction));
    User1Action.sa_sigaction = UserSignalHandler;
    User1Action.sa_flags = SA_SIGINFO;
    Result = sigaction(SIGUSR1, &User1Action, &OriginalUser1Action);
    if (Result < 0) {
        DBGTEST_ERROR("DbgTest: Failed to set sigaction for SIGUSR1. "
                      "Errno %d\n",
                      errno);

        Failures += 1;
        goto TestBasicDebugConnectionEnd;
    }

    RestoreUser1 = TRUE;

    //
    // Fork off a child to be debugged.
    //

    Child = fork();
    if (Child < 0) {
        DBGTEST_ERROR("DbgTest: Failed to fork. Errno %d\n", errno);
        Failures += 1;
        goto TestBasicDebugConnectionEnd;
    }

    //
    // If this is the child, signal the parent that everything's ready.
    //

    if (Child == 0) {
        Child = getpid();
        Parent = getppid();
        DBGTEST_PRINT("Created child %d of parent %d\n", Child, Parent);

        //
        // Allow tracing of this bad boy.
        //

        Status = OsDebug(DebugCommandEnableDebugging, 0, NULL, NULL, 0, 0);
        if (!KSUCCESS(Status)) {
            DBGTEST_ERROR("DbgTest: Failed to enable debugging. Status %d\n",
                          Status);

            exit(1);
        }

        //
        // Signal to the parent process that everything is ready.
        //

        kill(Parent, SIGUSR1);
        DBGTEST_PRINT("Child marked as initialized.\n");

        //
        // Configure the child to ignore SIGUSR2.
        //

        memset(&User2Action, 0, sizeof(struct sigaction));
        User2Action.sa_sigaction = User2SignalHandler;
        User2Action.sa_flags = SA_SIGINFO;
        Result = sigaction(SIGUSR2, &User2Action, &OriginalUser2Action);
        if (Result < 0) {
            DBGTEST_ERROR("DbgTest: Child failed to set sigaction for SIGUSR2. "
                          "errno %d.\n",
                          errno);

            exit(1);
        }

        //
        // Create some extra threads to just hang out.
        //

        for (ChildThreadIndex = 0;
             ChildThreadIndex < ChildThreadCount;
             ChildThreadIndex += 1) {

            ChildInitializing = 0;
            Status = OsCreateThread(NULL,
                                    0,
                                    TestThreadSpinForever,
                                    (PVOID)&ChildInitializing,
                                    NULL,
                                    0,
                                    NULL,
                                    NULL);

            if (!KSUCCESS(Status)) {
                DBGTEST_ERROR("Child %d failed to create thread: %d.\n",
                              getpid(),
                              Status);
            }

            while (ChildInitializing == 0) {
                NOTHING;
            }

            DBGTEST_PRINT("Child dummy thread %d created.\n",
                          ChildThreadIndex + 1);
        }

        //
        // Send signals until someone sets that stop variable to TRUE.
        //

        DBGTEST_PRINT("Child looping forever...\n");
        ChildSignalLoop(Child, &Stop);

        //
        // Restore SIGUSR2.
        //

        Result = sigaction(SIGUSR2, &OriginalUser2Action, NULL);
        if (Result < 0) {
            DBGTEST_ERROR("DbgTest: Child failed to set sigaction for SIGUSR2. "
                          "errno %d.\n",
                          errno);

            exit(1);
        }

        //
        // Return peacefully.
        //

        DBGTEST_PRINT("Child exiting gracefully.\n");
        exit(0);

    //
    // If this is the parent, begin debugging.
    //

    } else {

        //
        // Wait for the child to initialize.
        //

        DBGTEST_PRINT("Waiting for child %d to initialize.\n", Child);
        while (ChildInitialized == FALSE) {
            NOTHING;
        }

        for (TraceRound = 0; TraceRound < DEBUG_BREAK_COUNT; TraceRound += 1) {
            DBGTEST_PRINT("Debugger waiting %d...\n", TraceRound);
            WaitPid = waitpid(-1, &ExitStatus, WUNTRACED);

            //
            // The wait is expected to succeed.
            //

            if (WaitPid < 0) {
                DBGTEST_ERROR("DbgTest: Failed to wait. Errno %d\n", errno);
                Failures += 1;
            }

            //
            // The wait is expected to return the child.
            //

            if (WaitPid != Child) {
                DBGTEST_ERROR("DbgTest: wait() returned %d rather than "
                              "expected child %d.\n",
                              WaitPid,
                              Child);

                Failures += 1;
            }

            //
            // If this is the first round, try getting the loaded module list.
            //

            if (TraceRound == 0) {
                Failures += PrintLoadedModules(Child);
            }

            //
            // If this is the last round, the child should have just exited.
            //

            if (TraceRound == DEBUG_BREAK_COUNT - 1) {

                //
                // The wait is expected to return a macro status of exited.
                //

                if ((WIFSIGNALED(ExitStatus)) || (!WIFEXITED(ExitStatus)) ||
                    (WIFCONTINUED(ExitStatus)) || (WIFSTOPPED(ExitStatus))) {

                    DBGTEST_ERROR("DbgTest: wait() returned unexpected status "
                                  "%x at end.\n",
                                  ExitStatus);

                    Failures += 1;
                }

                if (WEXITSTATUS(ExitStatus) != 0) {
                    DBGTEST_ERROR("DbgTest: Child returned error code of %d\n",
                                  WEXITSTATUS(ExitStatus));

                    Failures += WEXITSTATUS(ExitStatus);
                }

            //
            // This is not the last round.
            //

            } else {

                //
                // The wait is expected to return a macro status of stopped.
                //

                if ((WIFSIGNALED(ExitStatus)) || (WIFEXITED(ExitStatus)) ||
                    (WIFCONTINUED(ExitStatus)) || (!WIFSTOPPED(ExitStatus)) ||
                    (WSTOPSIG(ExitStatus) != SIGUSR2)) {

                    DBGTEST_ERROR("DbgTest: wait() returned unexpected status "
                                  "%x. Signaled %x exited %x cont %x stopped "
                                  "%x stopsig %d\n",
                                  ExitStatus,
                                  WIFSIGNALED(ExitStatus),
                                  WIFEXITED(ExitStatus),
                                  WIFCONTINUED(ExitStatus),
                                  WIFSTOPPED(ExitStatus),
                                  WSTOPSIG(ExitStatus));

                    Failures += 1;
                }

                //
                // If this is the second to last round, write the value of the
                // stop variable so that the child quits.
                //

                if (TraceRound == DEBUG_BREAK_COUNT - 2) {
                    Stop = TRUE;
                    Status = OsDebug(DebugCommandWriteMemory,
                                     Child,
                                     (PVOID)&Stop,
                                     (PVOID)&Stop,
                                     sizeof(Stop),
                                     0);

                    if (!KSUCCESS(Status)) {
                        DBGTEST_ERROR("DbgTest: Failed to write to child "
                                      "memory. Status %d\n",
                                      Status);

                        Failures += 1;
                    }
                }

                //
                // Also try getting and setting some registers.
                //

                memset(&Break, 0, sizeof(Break));
                Status = OsDebug(DebugCommandGetBreakInformation,
                                 Child,
                                 NULL,
                                 &Break,
                                 sizeof(Break),
                                 0);

                if (!KSUCCESS(Status)) {
                    DBGTEST_ERROR("DbgTest: Failed to get registers for "
                                  "child %d. Status %d\n",
                                  Child,
                                  Status);

                    Failures += 1;
                }

                PrintRegisterContents(&Break);
                Status = OsDebug(DebugCommandSetBreakInformation,
                                 Child,
                                 NULL,
                                 &Break,
                                 sizeof(BREAK_NOTIFICATION),
                                 0);

                if (!KSUCCESS(Status)) {
                    DBGTEST_ERROR("DbgTest: Failed to set registers for child "
                                  "%d. Status %d\n",
                                  Child,
                                  Status);

                    Failures += 1;
                }

                //
                // Also try getting and setting the signal information.
                //

                memset(&SignalParameters, 0, sizeof(SIGNAL_PARAMETERS));
                Status = OsDebug(DebugCommandGetSignalInformation,
                                 Child,
                                 NULL,
                                 &SignalParameters,
                                 sizeof(SIGNAL_PARAMETERS),
                                 0);

                if (!KSUCCESS(Status)) {
                    DBGTEST_ERROR("DbgTest: Failed to get signal parameters "
                                  "for child %d. Status %d\n",
                                  Child,
                                  Status);

                    Failures += 1;
                }

                PrintSignalParameters(&SignalParameters);
                Status = OsDebug(DebugCommandSetSignalInformation,
                                 Child,
                                 NULL,
                                 &SignalParameters,
                                 sizeof(SIGNAL_PARAMETERS),
                                 0);

                if (!KSUCCESS(Status)) {
                    DBGTEST_ERROR("DbgTest: Failed to set signal parameters "
                                  "for child %d. Status %d\n",
                                  Child,
                                  Status);

                    Failures += 1;
                }

                //
                // Try a single step, then continue.
                //

                Failures += SingleStep(Child);
                Failures += RangeStep(Child);
                Status = OsDebug(DebugCommandContinue,
                                 Child,
                                 NULL,
                                 NULL,
                                 0,
                                 WSTOPSIG(ExitStatus));

                if (!KSUCCESS(Status)) {
                    DBGTEST_ERROR("DbgTest: Failed to continue. Status %d\n",
                                  Status);

                    Failures += 1;
                }
            }
        }

        DBGTEST_PRINT("Debugger finished. %d errors\n", Failures);
    }

TestBasicDebugConnectionEnd:
    if (RestoreUser1 != FALSE) {
        Result = sigaction(SIGUSR1, &OriginalUser1Action, NULL);
        if (Result < 0) {
            DBGTEST_ERROR("DbgTest: Child failed to set sigaction for SIGUSR2. "
                          "errno %d.\n",
                          errno);

            Failures += 1;
        }
    }

    return Failures;
}

void
UserSignalHandler (
    int SignalNumber,
    siginfo_t *SignalInformation,
    void *Context
    )

/*++

Routine Description:

    This routine implements the SIGUSR1 signal handler, which is sent by the
    child to indicate it has fully initialized.

Arguments:

    SignalNumber - Supplies the signal number, SIGUSR1.

    SignalInformation - Supplies a pointer to the signal information.

    Context - Supplies a context pointer supplied when the signal was wired in.

Return Value:

    None.

--*/

{

    ChildInitialized = TRUE;
    return;
}

void
User2SignalHandler (
    int SignalNumber,
    siginfo_t *SignalInformation,
    void *Context
    )

/*++

Routine Description:

    This routine implements the SIGUSR1 signal handler, which is sent by the
    child to indicate it has fully initialized.

Arguments:

    SignalNumber - Supplies the signal number, SIGUSR1.

    SignalInformation - Supplies a pointer to the signal information.

    Context - Supplies a context pointer supplied when the signal was wired in.

Return Value:

    None.

--*/

{

    User2SignalsReceived += 1;
    return;
}

VOID
TestThreadSpinForever (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements a thread routine that simply spins forever.

Arguments:

    Parameter - Supplies a parameter assumed to be of type PULONG whose
        contents will be set to 0.

Return Value:

    None. This thread never returns voluntarily.

--*/

{

    *((PULONG)Parameter) = 1;
    while (TRUE) {
        NOTHING;
    }

    return;
}

VOID
PrintRegisterContents (
    PBREAK_NOTIFICATION Break
    )

/*++

Routine Description:

    This routine prints register contents of a break notification.

Arguments:

    Break - Supplies a pointer to the initialized break notification to print.

Return Value:

    None.

--*/

{

#if defined(__i386) || defined(__amd64)

    ULONG ByteIndex;

#elif defined(__arm__)

    ULONG Instruction;

#endif

    if (DbgTestPrintRegisters == FALSE) {
        return;
    }

    DBGTEST_PRINT("Break, exception %d, thread ID %x thread Count %x "
                  "process %x.\n",
                  Break->Exception,
                  Break->ProcessorOrThreadNumber,
                  Break->ProcessorOrThreadCount,
                  Break->Process);

    if (Break->ProcessorBlock != 0) {
        DBGTEST_PRINT("Processor block %I64x\n", Break->ProcessorBlock);
    }

    if (Break->ErrorCode != 0) {
        DBGTEST_PRINT("Error code: %x\n", Break->ErrorCode);
    }

#if defined(__i386)

    DBGTEST_PRINT("Modules count %d signature %I64x, "
                  "Instruction pointer %I64x.\nInstruction stream: ",
                  Break->LoadedModuleCount,
                  Break->LoadedModuleSignature,
                  Break->InstructionPointer);

    for (ByteIndex = 0;
         ByteIndex < BREAK_NOTIFICATION_STREAM_SIZE;
         ByteIndex += 1) {

        DBGTEST_PRINT("%02X ", Break->InstructionStream[ByteIndex]);
    }

    DBGTEST_PRINT("\n");
    DBGTEST_PRINT("eax=%08I64x ebx=%08I64x ecx=%08I64x edx=%08I64x "
                  "eip=%08I64x\n"
                  "esi=%08I64x edi=%08I64x ebp=%08I64x esp=%08I64x "
                  "eflags=%08I64x\n",
                  Break->Registers.X86.Eax,
                  Break->Registers.X86.Ebx,
                  Break->Registers.X86.Ecx,
                  Break->Registers.X86.Edx,
                  Break->Registers.X86.Eip,
                  Break->Registers.X86.Esi,
                  Break->Registers.X86.Edi,
                  Break->Registers.X86.Ebp,
                  Break->Registers.X86.Esp,
                  Break->Registers.X86.Eflags);

    DBGTEST_PRINT("cs=%04x ds=%04x es=%04x fs=%04x gs=%04x ss=%04x\n",
                  Break->Registers.X86.Cs,
                  Break->Registers.X86.Ds,
                  Break->Registers.X86.Es,
                  Break->Registers.X86.Fs,
                  Break->Registers.X86.Gs,
                  Break->Registers.X86.Ss);

#elif defined(__arm__)

    RtlCopyMemory(&Instruction, Break->InstructionStream, sizeof(ULONG));
    DBGTEST_PRINT("%08X\n", Instruction);
    DBGTEST_PRINT("r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x\n"
                  "r6=%08x r7=%08x r8=%08x r9=%08x r10=%08x fp=%08x\n"
                  "ip=%08x sp=%08x lr=%08x pc=%08x cpsr=%08x\n",
                  Break->Registers.Arm.R0,
                  Break->Registers.Arm.R1,
                  Break->Registers.Arm.R2,
                  Break->Registers.Arm.R3,
                  Break->Registers.Arm.R4,
                  Break->Registers.Arm.R5,
                  Break->Registers.Arm.R6,
                  Break->Registers.Arm.R7,
                  Break->Registers.Arm.R8,
                  Break->Registers.Arm.R9,
                  Break->Registers.Arm.R10,
                  Break->Registers.Arm.R11Fp,
                  Break->Registers.Arm.R12Ip,
                  Break->Registers.Arm.R13Sp,
                  Break->Registers.Arm.R14Lr,
                  Break->Registers.Arm.R15Pc,
                  Break->Registers.Arm.Cpsr);

#elif defined(__amd64)

    DBGTEST_PRINT("Modules count %d signature %I64x, "
                  "Instruction pointer %I64x.\nInstruction stream: ",
                  Break->LoadedModuleCount,
                  Break->LoadedModuleSignature,
                  Break->InstructionPointer);

    for (ByteIndex = 0;
         ByteIndex < BREAK_NOTIFICATION_STREAM_SIZE;
         ByteIndex += 1) {

        DBGTEST_PRINT("%02X ", Break->InstructionStream[ByteIndex]);
    }

    DBGTEST_PRINT("\n");

#else

#error Unsupported processor architecture

#endif

}

VOID
PrintSignalParameters (
    PSIGNAL_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine prints out signal parameters.

Arguments:

    Parameters - Supplies a pointer to the parameters to print.

Return Value:

    None.

--*/

{

    DBGTEST_PRINT("Signal %d, code %d, error %d, sending process %d user %d, "
                  "value %d\n",
                  Parameters->SignalNumber,
                  Parameters->SignalCode,
                  Parameters->ErrorNumber,
                  Parameters->FromU.SendingProcess,
                  Parameters->SendingUserId,
                  Parameters->Parameter);

    return;
}

ULONG
SingleStep (
    pid_t Child
    )

/*++

Routine Description:

    This routine steps the target by one instruction. This routine assumes the
    child is already stopped.

Arguments:

    Child - Supplies the child to step.

Return Value:

    Returns the number of failures encountered.

--*/

{

    BREAK_NOTIFICATION Break;
    int ExitStatus;
    ULONG Failures;
    KSTATUS Status;
    pid_t WaitPid;

    Failures = 0;

    //
    // Single step that bad boy, and skip the signal that caused the break.
    //

    Status = OsDebug(DebugCommandSingleStep, Child, NULL, NULL, 0, 0);
    if (!KSUCCESS(Status)) {
        DBGTEST_ERROR("DbgTest: Failed to single step child %x: %d\n",
                      Child,
                      Status);
    }

    WaitPid = waitpid(-1, &ExitStatus, WUNTRACED);

    //
    // The wait is expected to succeed.
    //

    if (WaitPid < 0) {
        DBGTEST_ERROR("DbgTest: Failed to wait (ss). Errno %d\n", errno);
        Failures += 1;
    }

    //
    // The wait is expected to return the child.
    //

    if (WaitPid != Child) {
        DBGTEST_ERROR("DbgTest: wait() returned %d rather than "
                      "expected child %d.\n",
                      WaitPid,
                      Child);

        Failures += 1;
    }

    //
    // The wait is expected to return a macro status of stopped.
    //

    if ((WIFSIGNALED(ExitStatus)) || (WIFEXITED(ExitStatus)) ||
        (WIFCONTINUED(ExitStatus)) || (!WIFSTOPPED(ExitStatus)) ||
        (WSTOPSIG(ExitStatus) != SIGTRAP)) {

        DBGTEST_ERROR("DbgTest: wait() (SS) returned unexpected status "
                      "%x. Signaled %x exited %x cont %x stopped "
                      "%x stopsig %d\n",
                      ExitStatus,
                      WIFSIGNALED(ExitStatus),
                      WIFEXITED(ExitStatus),
                      WIFCONTINUED(ExitStatus),
                      WIFSTOPPED(ExitStatus),
                      WSTOPSIG(ExitStatus));

        Failures += 1;
    }

    //
    // Print the new registers.
    //

    memset(&Break, 0, sizeof(Break));
    Status = OsDebug(DebugCommandGetBreakInformation,
                     Child,
                     NULL,
                     &Break,
                     sizeof(Break),
                     0);

    if (!KSUCCESS(Status)) {
        DBGTEST_ERROR("DbgTest: Failed to get registers for "
                      "child %d. Status %d\n",
                      Child,
                      Status);

        Failures += 1;
    }

    DBGTEST_PRINT("Post single step registers:\n");
    PrintRegisterContents(&Break);
    return Failures;
}

ULONG
RangeStep (
    pid_t Child
    )

/*++

Routine Description:

    This routine lets the target go until it hits a specific range.

Arguments:

    Child - Supplies the child to step.

Return Value:

    Returns the number of failures encountered.

--*/

{

    BREAK_NOTIFICATION Break;
    PROCESS_DEBUG_BREAK_RANGE BreakRange;
    int ExitStatus;
    ULONG Failures;
    KSTATUS Status;
    pid_t WaitPid;

    Failures = 0;
    BreakRange.BreakRangeStart = (PVOID)0;
    BreakRange.BreakRangeEnd = (PVOID)(UINTN)MAX_ULONGLONG;
    BreakRange.RangeHoleStart = NULL;
    BreakRange.RangeHoleEnd = NULL;

    //
    // Range step that bad boy, and skip the signal that caused the break.
    //

    Status = OsDebug(DebugCommandRangeStep,
                     Child,
                     NULL,
                     &BreakRange,
                     sizeof(PROCESS_DEBUG_BREAK_RANGE),
                     0);

    if (!KSUCCESS(Status)) {
        DBGTEST_ERROR("DbgTest: Failed to range step child %x: %d\n",
                      Child,
                      Status);
    }

    WaitPid = waitpid(-1, &ExitStatus, WUNTRACED);

    //
    // The wait is expected to succeed.
    //

    if (WaitPid < 0) {
        DBGTEST_ERROR("DbgTest: Failed to wait (rs). Errno %d\n", errno);
        Failures += 1;
    }

    //
    // The wait is expected to return the child.
    //

    if (WaitPid != Child) {
        DBGTEST_ERROR("DbgTest: wait() (rs) returned %d rather than "
                      "expected child %d.\n",
                      WaitPid,
                      Child);

        Failures += 1;
    }

    //
    // The wait is expected to return a macro status of stopped.
    //

    if ((WIFSIGNALED(ExitStatus)) || (WIFEXITED(ExitStatus)) ||
        (WIFCONTINUED(ExitStatus)) || (!WIFSTOPPED(ExitStatus)) ||
        (WSTOPSIG(ExitStatus) != SIGTRAP)) {

        DBGTEST_ERROR("DbgTest: wait() (RS) returned unexpected status "
                      "%x. Signaled %x exited %x cont %x stopped "
                      "%x stopsig %d\n",
                      ExitStatus,
                      WIFSIGNALED(ExitStatus),
                      WIFEXITED(ExitStatus),
                      WIFCONTINUED(ExitStatus),
                      WIFSTOPPED(ExitStatus),
                      WSTOPSIG(ExitStatus));

        Failures += 1;
    }

    //
    // Print the new registers.
    //

    memset(&Break, 0, sizeof(Break));
    Status = OsDebug(DebugCommandGetBreakInformation,
                     Child,
                     NULL,
                     &Break,
                     sizeof(Break),
                     0);

    if (!KSUCCESS(Status)) {
        DBGTEST_ERROR("DbgTest: Failed to get registers for "
                      "child %d (RS). Status %d\n",
                      Child,
                      Status);

        Failures += 1;
    }

    DBGTEST_PRINT("Post range step registers:\n");
    PrintRegisterContents(&Break);
    return Failures;
}

VOID
ChildSignalLoop (
    pid_t Child,
    volatile UCHAR *Stop
    )

/*++

Routine Description:

    This routine spins signaling itself until someone tells it to stop.

Arguments:

    Child - Supplies this process' process identifier.

    Stop - Supplies the address of the boolean that is set to a non-zero value
        when they want it to stop.

Return Value:

    None.

--*/

{

    ULONG ExpectedSignalsReceived;
    int Result;

    User2SignalsReceived = 0;
    while (*Stop == FALSE) {
        ExpectedSignalsReceived = User2SignalsReceived + 1;
        Result = kill(Child, SIGUSR2);
        if (Result < 0) {
            DBGTEST_ERROR("DbgTest: Child failed to send signal to "
                          "itself. errno %d.\n",
                          errno);

            exit(1);
        }

        while (User2SignalsReceived != ExpectedSignalsReceived) {
            NOTHING;
        }
    }

    return;
}

ULONG
PrintLoadedModules (
    pid_t Child
    )

/*++

Routine Description:

    This routine retrieves and prints the list of loaded modules in the client.

Arguments:

    Child - Supplies the child process to query.

Return Value:

    Returns the number of failures encountered.

--*/

{

    ULONG ComputedStructureLength;
    ULONG Failures;
    PMODULE_LIST_HEADER List;
    PLOADED_MODULE_ENTRY Module;
    ULONG ModuleIndex;
    ULONG NameLength;
    ULONGLONG Signature;
    KSTATUS Status;

    Failures = 0;

    //
    // Create a reasonably sized buffer for the request.
    //

    List = malloc(MODULE_LIST_BUFFER_SIZE);
    if (List == NULL) {
        DBGTEST_ERROR("Error: Failed to malloc %d for loaded modules.\n",
                      MODULE_LIST_BUFFER_SIZE);

        Failures += 1;
        goto PrintLoadedModulesEnd;
    }

    Status = OsDebug(DebugCommandGetLoadedModules,
                     Child,
                     NULL,
                     List,
                     MODULE_LIST_BUFFER_SIZE,
                     0);

    if (!KSUCCESS(Status)) {
        DBGTEST_ERROR("Error: Failed to get loaded module list. Status %d\n",
                      Status);

        Failures += 1;
        goto PrintLoadedModulesEnd;
    }

    //
    // Print the list.
    //

    DBGTEST_PRINT("Module List: %d modules, signature 0x%I64x:\n",
                  List->ModuleCount,
                  List->Signature);

    Signature = 0;
    Module = (PLOADED_MODULE_ENTRY)(List + 1);
    for (ModuleIndex = 0; ModuleIndex < List->ModuleCount; ModuleIndex += 1) {
        if (Module->StructureSize < sizeof(LOADED_MODULE_ENTRY)) {
            DBGTEST_ERROR("DbgTest: Module %d had size %d, shouldn't have "
                          "been less than %d.\n",
                          ModuleIndex,
                          Module->StructureSize,
                          sizeof(LOADED_MODULE_ENTRY));

            Failures += 1;
            goto PrintLoadedModulesEnd;
        }

        DBGTEST_PRINT("    %d: %20s StructSize %2d Timestamp %I64x "
                      "LowestAddress %8I64x Size %I64x Process %x\n",
                      ModuleIndex,
                      Module->BinaryName,
                      Module->StructureSize,
                      Module->Timestamp,
                      Module->LowestAddress,
                      Module->Size,
                      Module->Process);

        Signature += Module->Timestamp + Module->LowestAddress;
        NameLength = RtlStringLength(Module->BinaryName) + 1;
        ComputedStructureLength = sizeof(LOADED_MODULE_ENTRY) + NameLength -
                                  (ANYSIZE_ARRAY * sizeof(CHAR));

        if (Module->StructureSize != ComputedStructureLength) {
            DBGTEST_ERROR("DbgTest: Module structure size was reported as "
                          "%x but seems to actually be %x.\n",
                          Module->StructureSize,
                          ComputedStructureLength);

            Failures += 1;
        }

        Module = (PVOID)Module + Module->StructureSize;
    }

    if (Signature != List->Signature) {
        DBGTEST_ERROR("DbgTest: Module signature was reported as %x but "
                      "seems to actually be %x.\n",
                      List->Signature,
                      Signature);

        Failures += 1;
    }

PrintLoadedModulesEnd:
    if (List != NULL) {
        free(List);
    }

    return Failures;
}

