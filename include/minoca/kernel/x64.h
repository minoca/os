/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    x64.h

Abstract:

    This header contains definitions for aspects of the system that are specific
    to the AMD64 architecture.

Author:

    Evan Green 26-May-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the extended state of the x86 architecture. This
    structure is architecturally defined by the FXSAVE and FXRSTOR instructions.

Members:

    Registers - Stores the extended processor state.

--*/

struct _FPU_CONTEXT {
    USHORT Fcw;
    USHORT Fsw;
    USHORT Ftw;
    USHORT Fop;
    ULONG FpuIp;
    USHORT Cs;
    USHORT Reserved1;
    ULONG FpuDp;
    USHORT Ds;
    USHORT Reserved2;
    ULONG Mxcsr;
    ULONG MxcsrMask;
    UCHAR St0Mm0[16];
    UCHAR St1Mm1[16];
    UCHAR St2Mm2[16];
    UCHAR St3Mm3[16];
    UCHAR St4Mm4[16];
    UCHAR St5Mm5[16];
    UCHAR St6Mm6[16];
    UCHAR St7Mm7[16];
    UCHAR Xmm0[16];
    UCHAR Xmm1[16];
    UCHAR Xmm2[16];
    UCHAR Xmm3[16];
    UCHAR Xmm4[16];
    UCHAR Xmm5[16];
    UCHAR Xmm6[16];
    UCHAR Xmm7[16];
    UCHAR Xmm8[16];
    UCHAR Xmm9[16];
    UCHAR Xmm10[16];
    UCHAR Xmm11[16];
    UCHAR Xmm12[16];
    UCHAR Xmm13[16];
    UCHAR Xmm14[16];
    UCHAR Xmm15[16];
    UCHAR Padding[96];
} PACKED ALIGNED64;

/*++

Structure Description:

    This structure outlines a trap frame that will be generated during most
    interrupts and exceptions.

Members:

    Registers - Stores the current state of the machine's registers. These
        values will be restored upon completion of the interrupt or exception.

--*/

struct _TRAP_FRAME {
    ULONG Ds;
    ULONG Es;
    ULONG Fs;
    ULONG Gs;
    ULONG Ss;
    ULONG ErrorCode;
    ULONGLONG Rax;
    ULONGLONG Rbx;
    ULONGLONG Rcx;
    ULONGLONG Rdx;
    ULONGLONG Rsi;
    ULONGLONG Rdi;
    ULONGLONG Rbp;
    ULONGLONG R8;
    ULONGLONG R9;
    ULONGLONG R10;
    ULONGLONG R11;
    ULONGLONG R12;
    ULONGLONG R13;
    ULONGLONG R14;
    ULONGLONG R15;
    ULONGLONG Rip;
    ULONG Cs;
    ULONGLONG Rflags;
    ULONGLONG Rsp;
} PACKED;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
