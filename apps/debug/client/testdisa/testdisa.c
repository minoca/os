/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testdisa.c

Abstract:

    This program tests the disassembler by feeding it instructions as input.

Author:

    Evan Green 21-Jun-2012

Environment:

    Development

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/im.h>
#include "../disasm.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MALLOC malloc
#define FREE free

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
DbgpPrintAddress (
    PDISASSEMBLED_INSTRUCTION Instruction,
    BOOL Print
    );

LONG
DbgpGetFileSize (
    FILE *File
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the program. It collects the
    options passed to it, and invokes the disassembler.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArmInstruction;
    ULONG BytesDisassembled;
    ULONG BytesRead;
    PUCHAR CurrentInstruction;
    DISASSEMBLED_INSTRUCTION Disassembly;
    CHAR DisassemblyBuffer[1024];
    ULONG Failures;
    FILE *File;
    PVOID FileBuffer;
    PSTR Filename;
    LONG FileSize;
    BOOL ForceThumb;
    IMAGE_BUFFER ImageBuffer;
    IMAGE_INFORMATION ImageInformation;
    PUCHAR InstructionStream;
    MACHINE_LANGUAGE Language;
    PSTR LanguageString;
    BOOL PrintDisassembly;
    BOOL Result;
    KSTATUS Status;
    ULONG TextSize;
    ULONGLONG TextVirtualAddress;

    Failures = 0;
    FileBuffer = NULL;
    ForceThumb = FALSE;
    memset(&ImageBuffer, 0, sizeof(IMAGE_BUFFER));
    InstructionStream = NULL;
    PrintDisassembly = TRUE;
    if (ArgumentCount < 2) {
        printf("Usage: testdisa [-q] [-t] <file>\n"
               "Options:\n"
               "    -q   Quiet. Don't print disassembly, only errors.\n"
               "    -t   Force thumb mode. Only applies to ARM images.\n");

        return 1;
    }

    while (TRUE) {
        if (strcasecmp(Arguments[1], "-q") == 0) {
            PrintDisassembly = FALSE;
            Arguments += 1;

        } else if (strcasecmp(Arguments[1], "-t") == 0) {
            ForceThumb = TRUE;
            Arguments += 1;

        } else {
            break;
        }
    }

    //
    // Determine the file size and load the file into memory.
    //

    Filename = Arguments[1];
    File = fopen(Filename, "rb");
    if (File == NULL) {
        Result = FALSE;
        Failures += 1;
        goto MainEnd;
    }

    FileSize = DbgpGetFileSize(File);
    if (FileSize <= 0) {
        Result = FALSE;
        Failures += 1;
        goto MainEnd;
    }

    FileBuffer = MALLOC(FileSize);
    if (FileBuffer == NULL) {
        Result = FALSE;
        Failures += 1;
        goto MainEnd;
    }

    BytesRead = fread(FileBuffer, 1, FileSize, File);
    if (BytesRead != FileSize) {
        Result = FALSE;
        Failures += 1;
        goto MainEnd;
    }

    ImageBuffer.Data = FileBuffer;
    ImageBuffer.Size = FileSize;
    Status = ImGetImageInformation(&ImageBuffer, &ImageInformation);
    if (!KSUCCESS(Status)) {
        Result = FALSE;
        Failures += 1;
        goto MainEnd;
    }

    //
    // Get the text section.
    //

    Result = ImGetImageSection(&ImageBuffer,
                               ".text",
                               (PVOID *)&InstructionStream,
                               &TextVirtualAddress,
                               &TextSize,
                               NULL);

    if (Result == FALSE) {
        printf("Error: Could not load text section for file %s.\n", Filename);
        Failures += 1;
        goto MainEnd;
    }

    //
    // Determine the machine language.
    //

    Language = MachineLanguageInvalid;
    LanguageString = "Unknown";
    switch (ImageInformation.Machine) {
    case ImageMachineTypeX86:
        Language = MachineLanguageX86;
        LanguageString = "x86";
        break;

    case ImageMachineTypeX64:
        Language = MachineLanguageX64;
        LanguageString = "x64";
        break;

    case ImageMachineTypeArm32:
        Language = MachineLanguageArm;
        LanguageString = "ARM";
        if (((ImageInformation.EntryPoint & 0x1) != 0) ||
            (ForceThumb != FALSE)) {

            Language = MachineLanguageThumb2;
            LanguageString = "Thumb2";
        }

        break;

    default:
        printf("Unknown machine type %d!\n", ImageInformation.Machine);
        Failures += 1;
        goto MainEnd;
    }

    if (PrintDisassembly != FALSE) {
        printf("Disassembling %s (%s), VA 0x%llx, 0x%x bytes.\n",
               Filename,
               LanguageString,
               TextVirtualAddress,
               TextSize);
    }

    //
    // Disassemble the file contents.
    //

    BytesDisassembled = 0;
    CurrentInstruction = InstructionStream;
    while (BytesDisassembled < TextSize) {

        //
        // Print the offset from the start of disassembly and disassemble the
        // instruction.
        //

        if (PrintDisassembly != FALSE) {
            printf("\n%08llx: ", TextVirtualAddress + BytesDisassembled);
        }

        Result = DbgDisassemble(TextVirtualAddress + BytesDisassembled,
                                CurrentInstruction,
                                DisassemblyBuffer,
                                sizeof(DisassemblyBuffer),
                                &Disassembly,
                                Language);

        if (Result == FALSE) {
            Failures += 1;
            printf("ERROR decoding instruction, partial string: ");
            DisassemblyBuffer[99] = '\0';
            printf("%s", DisassemblyBuffer);
            goto MainEnd;
        }

        //
        // For ARM, print the binary code first, since it's always a pretty
        // consistent size.
        //

        if (Language == MachineLanguageArm) {
            if (Disassembly.BinaryLength != 4) {
                printf("Error: got %d byte ARM disassembly.\n",
                       Disassembly.BinaryLength);

                Failures += 1;
            }

            ArmInstruction = *((PULONG)CurrentInstruction);
            CurrentInstruction += Disassembly.BinaryLength;
            BytesDisassembled += Disassembly.BinaryLength;
            if (PrintDisassembly != FALSE) {
                printf("%08x  ", ArmInstruction);
            }

        } else if (Language == MachineLanguageThumb2) {
            ArmInstruction = *((PUSHORT)CurrentInstruction);
            if (PrintDisassembly != FALSE) {
                printf(" %04x", ArmInstruction);
            }

            if (Disassembly.BinaryLength == 4) {
                ArmInstruction = *(((PUSHORT)CurrentInstruction) + 1);
                if (PrintDisassembly != FALSE) {
                    printf("%04x  ", ArmInstruction);
                }

            } else if (Disassembly.BinaryLength == 2) {
                if (PrintDisassembly != FALSE) {
                    printf("      ");
                }

            } else if (Disassembly.BinaryLength != 2) {
                printf("Error: Got %d byte Thumb-2 disassembly.\n",
                       Disassembly.BinaryLength);

                Failures += 1;
                ArmInstruction = *((PULONG)CurrentInstruction);
            }

            CurrentInstruction += Disassembly.BinaryLength;
            BytesDisassembled += Disassembly.BinaryLength;
        }

        //
        // Print the mnemonic, which should exist in any case.
        //

        if (Disassembly.Mnemonic == NULL) {
            printf("Error: NULL opcode mnemonic.\n");
            Failures += 1;
        }

        if (PrintDisassembly != FALSE) {
            printf("%s\t", Disassembly.Mnemonic);
        }

        //
        // Attempt to print the first (destination) operand. If the operand
        // is an address, print that as well.
        //

        if (Disassembly.DestinationOperand != NULL) {
            if (strcasecmp(Disassembly.DestinationOperand, "err") == 0) {
                printf("Error: got ERR destination operand!\n");
                Failures += 1;
            }

            if (PrintDisassembly != FALSE) {
                printf("%s", Disassembly.DestinationOperand);
            }

            if (Disassembly.AddressIsDestination != FALSE) {
                if (DbgpPrintAddress(&Disassembly, PrintDisassembly) != 0) {
                    printf("Error: Invalid operand address.\n");
                    Failures += 1;
                }
            }

            //
            // Attempt to print the second (source) operand. If the operand is
            // an address, print that as well.
            //

            if (Disassembly.SourceOperand != NULL) {
                if (strcasecmp(Disassembly.DestinationOperand, "err") == 0) {
                    printf("Error: got ERR source operand!\n");
                    Failures += 1;
                }

                if (PrintDisassembly != FALSE) {
                    printf(", %s", Disassembly.SourceOperand);
                }

                if (Disassembly.AddressIsDestination == FALSE) {
                    if (DbgpPrintAddress(&Disassembly, PrintDisassembly) != 0) {
                        printf("Error: Invalid operand address.\n");
                        Failures += 1;
                    }
                }

                //
                // Attempt to print the third operand. This operand only exists
                // in rare circumstances on x86, and can never be an address.
                // On ARM, third and fourth operands are the norm.
                //

                if (Disassembly.ThirdOperand != NULL) {
                    if (strcasecmp(Disassembly.ThirdOperand, "err") == 0) {
                        printf("Error: got ERR source operand!\n");
                        Failures += 1;
                    }

                    if (PrintDisassembly != FALSE) {
                        printf(", %s", Disassembly.ThirdOperand);
                    }

                    //
                    // Print the fourth operand, which will only ever be set
                    // on ARM.
                    //

                    if ((Disassembly.FourthOperand != NULL) &&
                        (PrintDisassembly != FALSE)) {

                        printf(", %s", Disassembly.FourthOperand);
                    }

                //
                // If the third operand wasn't present, a fourth better not be
                // either.
                //

                } else if (Disassembly.FourthOperand != NULL) {
                    printf("Error: Got fourth operand but no third!\n");
                    Failures += 1;
                }

            } else {

                //
                // If there was no second operand, there should definitely be
                // no third or fourth operand.
                //

                if ((Disassembly.ThirdOperand != NULL) ||
                    (Disassembly.FourthOperand != NULL)) {

                    printf("Error: Got third/fourth operands but no second "
                           "operand!\n");

                    Failures += 1;
                }

            }

        } else {

            //
            // If there was no first operand, there should definitely be no
            // second, third, or fourth operand.
            //

            if ((Disassembly.SourceOperand != NULL) ||
                (Disassembly.ThirdOperand != NULL) ||
                (Disassembly.FourthOperand != NULL)) {

                printf("Error: Got second/third/fourth operand, but no "
                       "first!\n");

                Failures += 1;
            }
        }

        //
        // Print the binary contents for x86 disassembly.
        //

        if ((Language == MachineLanguageX86) ||
            (Language == MachineLanguageX64)) {

            if (Disassembly.BinaryLength == 0) {
                printf("Error: got a zero length instruction\n");
                Failures += 1;
                goto MainEnd;
            }

            if (PrintDisassembly != FALSE) {
                printf(" \t; ");
            }

            while (Disassembly.BinaryLength != 0) {
                if (PrintDisassembly != FALSE) {
                    printf("%02x", *CurrentInstruction);
                }

                CurrentInstruction += 1;
                BytesDisassembled += 1;
                Disassembly.BinaryLength -= 1;
            }
        }
    }

    if (PrintDisassembly != FALSE) {
        printf("\n");
    }

MainEnd:
    if (FileBuffer != NULL) {
        FREE(FileBuffer);
    }

    if (Failures != 0) {
        printf("\n*** %d Failures in disassembly test for file %s! ***\n",
               Failures,
               Filename);

        return 1;

    } else {
        printf("All disassembler tests passed for file %s.\n", Filename);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
DbgpPrintAddress (
    PDISASSEMBLED_INSTRUCTION Instruction,
    BOOL Print
    )

/*++

Routine Description:

    This routine prints an address encoded in a disassembled instruction.

Arguments:

    Instruction - Supplies a pointer to the instruction containing the address
        to decode.

    Print - Supplies a boolean indicating if the value should
        actually be printed.

Return Value:

    Returns 0 on success, or 1 on failure.

--*/

{

    if (Instruction->AddressIsValid == FALSE) {
        return 0;
    }

    if (Print != FALSE) {
        printf(" (0x%08llx)", Instruction->OperandAddress);
    }

    return 0;
}

LONG
DbgpGetFileSize (
    FILE *File
    )

/*++

Routine Description:

    This routine determines the size of an opened file.

Arguments:

    File - Supplies the file handle.

Return Value:

    Returns the file length.

--*/

{

    INT CurrentPosition;
    LONG FileSize;

    CurrentPosition = ftell(File);
    fseek(File, 0, SEEK_END);
    FileSize = ftell(File);
    fseek(File, CurrentPosition, SEEK_SET);
    return FileSize;
}

