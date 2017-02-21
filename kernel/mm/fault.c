/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fault.c

Abstract:

    This module implements support for page faults in the kernel.

Author:

    Evan Green 3-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
MmpHandleBadFault (
    PKPROCESS Process,
    PVOID FaultingAddress,
    PTRAP_FRAME TrapFrame,
    ULONG FaultFlags
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
MmHandleFault (
    ULONG FaultFlags,
    PVOID FaultingAddress,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles access faults for the kernel.

Arguments:

    FaultFlags - Supplies a bitfield of flags regarding the fault. See
        FAULT_FLAG_* definitions.

    FaultingAddress - Supplies the address that caused the page fault.

    TrapFrame - Supplies a pointer to the state of the machine when the page
        fault occurred.

Return Value:

    None.

--*/

{

    PKPROCESS CurrentProcess;
    BOOL FaultHandled;
    ULONG Flags;
    PIMAGE_SECTION ImageSection;
    PKPROCESS KernelProcess;
    UINTN PageOffset;
    PKPROCESS Process;
    KSTATUS Status;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    if (Thread == NULL) {

        //
        // The system took a page fault too early.
        //

        KeCrashSystem(CRASH_PAGE_FAULT,
                      (UINTN)FaultingAddress,
                      (UINTN)(ArGetInstructionPointer(TrapFrame)),
                      (UINTN)TrapFrame,
                      FaultFlags);
    }

    //
    // Check for a simple missing directory entry. This doesn't really count
    // as a page fault.
    //

    FaultHandled = MmpCheckDirectoryUpdates(FaultingAddress);
    if (FaultHandled != FALSE) {
        return;
    }

    //
    // The paging thread cannot fault any more than to update its directory.
    //

    ASSERT((MmPagingThread == NULL) ||
           (KeGetCurrentThread() != MmPagingThread));

    //
    // Page faults are not allowed at dispatch or above.
    //

    if (KeGetRunLevel() >= RunLevelDispatch) {
        KeCrashSystem(CRASH_PAGE_FAULT_AT_HIGH_RUN_LEVEL,
                      (UINTN)FaultingAddress,
                      KeGetRunLevel(),
                      (UINTN)TrapFrame,
                      0);
    }

    //
    // Determine the process that owns the faulting section.
    //

    ASSERT(Thread->OwningProcess != NULL);

    Thread->ResourceUsage.PageFaults += 1;
    CurrentProcess = Thread->OwningProcess;
    KernelProcess = PsGetKernelProcess();
    if ((ArIsTrapFrameFromPrivilegedMode(TrapFrame) != FALSE) &&
        (FaultingAddress >= KERNEL_VA_START)) {

        Process = KernelProcess;

    } else {
        Process = CurrentProcess;
    }

    //
    // Ensure that the system is up and running enough to take page faults.
    //

    ASSERT(Process != NULL);

    //
    // Loop until the section stops shrinking.
    //

    ImageSection = NULL;
    while (TRUE) {

        //
        // Find the section that owns the faulting address. This routine takes
        // a reference on the section.
        //

        Status = MmpLookupSection(FaultingAddress,
                                  Process->AddressSpace,
                                  &ImageSection,
                                  &PageOffset);

        //
        // Handle an unknown user mode section.
        //

        if (Status == STATUS_NOT_FOUND) {
            MmpHandleBadFault(Process, FaultingAddress, TrapFrame, FaultFlags);
            goto HandleFaultEnd;
        }

        //
        // No matter what the fault is, if this is a section with no access
        // permissions, then report an access violation.
        //

        if ((ImageSection->Flags & IMAGE_SECTION_ACCESS_MASK) == 0) {
            MmpHandleBadFault(Process,
                              FaultingAddress,
                              TrapFrame,
                              FAULT_FLAG_PERMISSION_ERROR);

            goto HandleFaultEnd;

        //
        // If the fault occurred because the page was not present, resolve the
        // page fault.
        //

        } else if ((FaultFlags & FAULT_FLAG_PAGE_NOT_PRESENT) != 0) {
            Status = MmpPageIn(ImageSection, PageOffset, NULL);

            //
            // If the image section shrunk in the meantime, try the whole thing
            // again.
            //

            if (Status == STATUS_TRY_AGAIN) {
                MmpImageSectionReleaseReference(ImageSection);
                ImageSection = NULL;
                continue;
            }

            if (!KSUCCESS(Status) && (Status != STATUS_TOO_LATE)) {

                //
                // If page in failed for a backed section, then send the
                // fault error to user mode. Otherwise crash.
                //

                Flags = ImageSection->Flags;
                if ((Flags & IMAGE_SECTION_BACKED) != 0) {
                    if (Status == STATUS_END_OF_FILE) {
                        FaultFlags = FAULT_FLAG_OUT_OF_BOUNDS;
                    }

                    MmpHandleBadFault(Process,
                                      FaultingAddress,
                                      TrapFrame,
                                      FaultFlags);

                    goto HandleFaultEnd;
                }

                KeCrashSystem(CRASH_PAGE_IN_ERROR,
                              (UINTN)CurrentProcess,
                              (UINTN)ImageSection,
                              PageOffset,
                              Status);
            }

        //
        // The page was there and the access was not in violation, so this must
        // be a write on a read only page.
        //

        } else if ((FaultFlags & FAULT_FLAG_WRITE) != 0) {

            //
            // If this is a read-only image section then either fault in
            // usermode or crash.
            //

            if ((ImageSection->Flags & IMAGE_SECTION_WRITABLE) == 0) {
                MmpHandleBadFault(Process,
                                  FaultingAddress,
                                  TrapFrame,
                                  FaultFlags);

                goto HandleFaultEnd;

            //
            // Otherwise handle the write fault by breaking any inheritance the
            // page has with its children or parent.
            //

            } else {
                Status = MmpIsolateImageSection(ImageSection, PageOffset);

                //
                // If the image section shrunk in the meantime, try the whole
                // thing again.
                //

                if (Status == STATUS_TRY_AGAIN) {
                    MmpImageSectionReleaseReference(ImageSection);
                    ImageSection = NULL;
                    continue;
                }

                if (Status == STATUS_END_OF_FILE) {
                    MmpHandleBadFault(Process,
                                      FaultingAddress,
                                      TrapFrame,
                                      FAULT_FLAG_OUT_OF_BOUNDS);

                    goto HandleFaultEnd;

                } else if ((!KSUCCESS(Status)) && (Status != STATUS_TOO_LATE)) {
                    MmpHandleBadFault(Process,
                                      FaultingAddress,
                                      TrapFrame,
                                      FaultFlags);

                    goto HandleFaultEnd;
                }
            }

        } else if (((FaultFlags & FAULT_FLAG_PROTECTION_FAULT) != 0) ||
                   ((FaultFlags & FAULT_FLAG_PERMISSION_ERROR) != 0)) {

            MmpHandleBadFault(Process, FaultingAddress, TrapFrame, FaultFlags);
            goto HandleFaultEnd;
        }

        break;
    }

HandleFaultEnd:
    if (ImageSection != NULL) {
        MmpImageSectionReleaseReference(ImageSection);
    }

    //
    // Check for any signals that may have cropped up while handling the fault
    // (such as perhaps a segmentation fault signal).
    //

    PsCheckRuntimeTimers(Thread);
    PsDispatchPendingSignals(Thread, TrapFrame);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
MmpHandleBadFault (
    PKPROCESS Process,
    PVOID FaultingAddress,
    PTRAP_FRAME TrapFrame,
    ULONG FaultFlags
    )

/*++

Routine Description:

    This routine performs actions on a fault that is either going to generate
    a signal for user mode, or a crash for kernel mode (with a few exceptions).

Arguments:

    Process - Supplies a pointer to the process involved.

    FaultingAddress - Supplies the address that caused the page fault.

    TrapFrame - Supplies a pointer to the state of the machine when the page
        fault occurred.

    FaultFlags - Supplies a bitfield of flags regarding the fault. See
        FAULT_FLAG_* definitions.

Return Value:

    None.

--*/

{

    PKPROCESS KernelProcess;

    KernelProcess = PsGetKernelProcess();
    if (Process != KernelProcess) {
        if (ArIsTrapFrameFromPrivilegedMode(TrapFrame) != FALSE) {

            //
            // If this was a user mode copy attempt, then manipulate the trap
            // frame to indicate the attempt failed.
            //

            if (MmpCheckUserModeCopyRoutines(TrapFrame) != FALSE) {
                return;
            }

        } else {
            PsHandleUserModeFault(FaultingAddress,
                                  FaultFlags,
                                  TrapFrame,
                                  Process);

            return;
        }
    }

    KeCrashSystem(CRASH_PAGE_FAULT,
                  (UINTN)FaultingAddress,
                  (UINTN)(ArGetInstructionPointer(TrapFrame)),
                  (UINTN)TrapFrame,
                  FaultFlags);

    return;
}

