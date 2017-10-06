/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    elf.h

Abstract:

    This header contains definitions for the ELF image format.

Author:

    Evan Green 7-Mar-2014

--*/

#ifndef _ELF_H
#define _ELF_H

//
// ------------------------------------------------------------------- Includes
//

#include <stdint.h>

//
// --------------------------------------------------------------------- Macros
//

#define IS_ELF(_ElfHeader) ((_ElfHeader).e_ident[EI_MAG0] == ELFMAG0 && \
                            (_ElfHeader).e_ident[EI_MAG1] == ELFMAG1 && \
                            (_ElfHeader).e_ident[EI_MAG2] == ELFMAG2 && \
                            (_ElfHeader).e_ident[EI_MAG3] == ELFMAG3)

//
// Define macros for accessing the fields of r_info.
//

#define ELF32_R_SYM(_Info)   ((_Info) >> 8)
#define ELF32_R_TYPE(_Info)  ((unsigned char)(_Info))

#define ELF64_R_SYM(_Info)   ((_Info) >> 32)
#define ELF64_R_TYPE(_Info)  ((_Info) & 0xffffffffL)

//
// This macro constructs an r_info value from field values.
//

#define ELF32_R_INFO(_Symbol, _Type) \
    (((_Symbol) << 8) + (unsigned char)(_Type))

#define ELF64_R_INFO(_Symbol, _Type) \
    (((_Symbol) << 32) + ((_Type) & 0xffffffffL))

#define ELF64_R_TYPE_DATA(_Info) (((Elf64_Xword)(_Info) << 32) >> 40)
#define ELF64_R_TYPE_ID(_Info)   (((Elf64_Xword)(_Info) << 56) >> 56)
#define ELF64_R_TYPE_INFO(_Data, _Type)   \
        (((Elf64_Xword)(_Data) << 8) + (Elf64_Xword)(_Type))

//
// These macros compose and decompose values for Move.r_info.
//

#define ELF32_M_SYM(_Info)   ((_Info) >> 8)
#define ELF32_M_SIZE(_Info)  ((unsigned char)(_Info))
#define ELF32_M_INFO(_Symbol, _Size) (((_Symbol) << 8) + (unsigned char)(_Size))

#define ELF64_M_SYM(_Info)   ((_Info) >> 8)
#define ELF64_M_SIZE(_Info)  ((unsigned char)(_Info))
#define ELF64_M_INFO(_Symbol, _Size) (((_Symbol) << 8) + (unsigned char)(_Size))

//
// Define macros for accessing the fields of st_info.
//

#define ELF_ST_BIND(_Info)     ((_Info) >> 4)
#define ELF_ST_TYPE(_Info)     ((_Info) & 0xf)

#define ELF32_ST_BIND(_Info) ELF_ST_BIND(_Info)
#define ELF32_ST_TYPE(_Info) ELF_ST_TYPE(_Info)

#define ELF64_ST_BIND(_Info) ELF_ST_BIND(_Info)
#define ELF64_ST_TYPE(_Info) ELF_ST_TYPE(_Info)

//
// This macro constructs an st_info value from its components.
//

#define ELF32_ST_INFO(_Bind, _Type)   (((_Bind) << 4) + ((_Type) & 0xf))
#define ELF64_ST_INFO(_Bind, _Type)   (((_Bind) << 4) + ((_Type) & 0xf))

//
// This macro accesses the visibility field of the st_other member.
//

#define ELF32_ST_VISIBILITY(_Other)    ((_Other) & 0x3)
#define ELF64_ST_VISIBILITY(_Other)    ((_Other) & 0x3)

//
// ---------------------------------------------------------------- Definitions
//

//
// Indexes into the e_ident array.
//

//
// Magic numbers.
//

#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3

//
// Class of machine
//

#define EI_CLASS    4

//
// Data format
//

#define EI_DATA     5

//
// ELF format version
//

#define EI_VERSION  6

//
// Operating system / ABI identification
//

#define EI_OSABI    7

//
// ABI version
//

#define EI_ABIVERSION   8

//
// Start of architecture identification
//

#define OLD_EI_BRAND    8

//
// Start of padding (SVR4 ABI).
//

#define EI_PAD      9

//
// Size of the e_ident array
//

#define EI_NIDENT   16

//
// Values for the magic number bytes
//

#define ELFMAG0     0x7f
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'
#define ELFMAG      "\177ELF"

//
// Size of the ELF magic string
//

#define SELFMAG     4

//
// Values for e_ident[EI_VERSION] and e_version
//

#define EV_NONE     0
#define EV_CURRENT  1

//
// Values for e_ident[EI_CLASS]
//

#define ELFCLASSNONE    0
#define ELFCLASS32  1
#define ELFCLASS64  2

//
// Values for e_ident[EI_DATA]
//

//
// Unknown data format
//

#define ELFDATANONE 0

//
// Two's complement little-endian
//

#define ELFDATA2LSB 1

//
// Two's complement big-endian
//

#define ELFDATA2MSB 2

//
// Values for e_ident[EI_OSABI]
//

//
// UNIX System V ABI
//

#define ELFOSABI_NONE       0

//
// HP-UX operating system
//

#define ELFOSABI_HPUX       1

//
// NetBSD
//

#define ELFOSABI_NETBSD     2

//
// GNU/Linux
//

#define ELFOSABI_LINUX      3

//
// GNU/Hurd
//

#define ELFOSABI_HURD       4

//
// 86Open IA32 ABI
//

#define ELFOSABI_86OPEN     5

//
// Solaris
//

#define ELFOSABI_SOLARIS    6

//
// AIX
//

#define ELFOSABI_AIX        7

//
// IRIX
//

#define ELFOSABI_IRIX       8

//
// FreeBSD
//

#define ELFOSABI_FREEBSD    9

//
// TRU64 UNIX
//

#define ELFOSABI_TRU64      10

//
// Novell Modesto
//

#define ELFOSABI_MODESTO    11

//
// OpenBSD
//

#define ELFOSABI_OPENBSD    12

//
// Open VMS
//

#define ELFOSABI_OPENVMS    13

//
// HP Non-Stop Kernel
//

#define ELFOSABI_NSK        14

//
// ARM
//

#define ELFOSABI_ARM        97

//
// Standalone (embedded) application.
//

#define ELFOSABI_STANDALONE 255

//
// Symbol used in old spec
//

#define ELFOSABI_SYSV       ELFOSABI_NONE

//
// Monterey
//

#define ELFOSABI_MONTEREY   ELFOSABI_AIX

//
// Values for e_type
//

//
// Unknown type
//

#define ET_NONE     0

//
// Relocatable
//

#define ET_REL      1

//
// Executable
//

#define ET_EXEC     2

//
// Shared object
//

#define ET_DYN      3

//
// Core file
//

#define ET_CORE     4

//
// First operating system specific value
//

#define ET_LOOS     0xFE00

//
// Last operating system specific value
//

#define ET_HIOS     0xFEFF

//
// First processor specific value
//

#define ET_LOPROC   0xFF00

//
// Last processor specific value
//

#define ET_HIPROC   0xFFFF

//
// Values for e_machine
//

//
// Unknown machine
//

#define EM_NONE     0

//
// AT&T WE32100
//

#define EM_M32      1

//
// Sun SPARC
//

#define EM_SPARC    2

//
// Intel i386
//

#define EM_386      3

//
// Motorola 68000
//

#define EM_68K      4

//
// Motorola 88000
//

#define EM_88K      5

//
// Intel i860
//

#define EM_860      7

//
// MIPS R3000 Big-Endian
//

#define EM_MIPS     8

//
// IBM System/370
//

#define EM_S370     9

//
// MIPS R3000 Little-Endian
//

#define EM_MIPS_RS3_LE  10

//
// HP PA-RISC
//

#define EM_PARISC   15

//
// Fujistsu VPP500
//

#define EM_VPP500   17

//
// SPARC v8plus
//

#define EM_SPARC32PLUS  18

//
// Intel 80960
//

#define EM_960      19

//
// PowerPC 32-bit
//

#define EM_PPC      20

//
// PowerPC 64-bit
//

#define EM_PPC64    21

//
// IBM System/390
//

#define EM_S390     22

//
// NEC V800
//

#define EM_V800     36

//
// Fujitsu FR20
//

#define EM_FR20     37

//
// TRW RH-32
//

#define EM_RH32     38

//
// Motorola RCE
//

#define EM_RCE      39

//
// ARM
//

#define EM_ARM      40

//
// Hitachi SH
//

#define EM_SH       42

//
// Sparc v9 64-bit
//

#define EM_SPARCV9  43

//
// Siemens TriCore embedded processor
//

#define EM_TRICORE  44

//
// Argonaut RISC Core
//

#define EM_ARC      45

//
// Hitachi H8/300
//

#define EM_H8_300   46

//
// Hitachi H8/300H
//

#define EM_H8_300H  47

//
// Hitachi H8S
//

#define EM_H8S      48

//
// Hitachi H8/500
//

#define EM_H8_500   49

//
// Intel IA64
//

#define EM_IA_64    50

//
// Stanford MIPS-X
//

#define EM_MIPS_X   51

//
// Motorola ColdFire
//

#define EM_COLDFIRE 52

//
// Motorola M68HC12
//

#define EM_68HC12   53

//
// Fujitsu MMA
//

#define EM_MMA      54

//
// Siemens PCP
//

#define EM_PCP      55

//
// Sony nCPU
//

#define EM_NCPU     56

//
// Denso NDR1 microprocessor
//

#define EM_NDR1     57

//
// Motorola Star*Core processor
//

#define EM_STARCORE 58

//
// Toyota ME16 processor
//

#define EM_ME16     59

//
// STMicroelectronics ST100 processor
//

#define EM_ST100    60

//
// Advanced Logic Corp. TinyJ processor
//

#define EM_TINYJ    61

//
// AMD x86-64
//

#define EM_X86_64   62

//
// AMD x86-64 (compat)
//

#define EM_AMD64    EM_X86_64

//
// ARM 64 bit architecture
//

#define EM_AARCH64  183

//
// Non-standard or deprecated values
//

//
// Intel i486
//

#define EM_486      6

//
// MIPS R4000 Big-Endian
//

#define EM_MIPS_RS4_BE  10

//
// Digital Alpha (standard value)
//

#define EM_ALPHA_STD    41

//
// Alpha
//

#define EM_ALPHA    0x9026

//
// Special section indices
//

//
// Undefined, missing, or irrelevant
//

#define SHN_UNDEF        0

//
// First of the reserved range
//

#define SHN_LORESERVE   0xff00

//
// First processor specific
//

#define SHN_LOPROC  0xff00

//
// Last processor specific
//

#define SHN_HIPROC  0xff1f

//
// First operating system specific
//

#define SHN_LOOS    0xff20

//
// Last operating system specific
//

#define SHN_HIOS    0xff3f

//
// Absolute values
//

#define SHN_ABS     0xfff1

//
// Common data
//

#define SHN_COMMON  0xfff2

//
// Escape, the index is stored elsewhere
//

#define SHN_XINDEX  0xffff

//
// Last of the reserved range
//

#define SHN_HIRESERVE   0xffff

//
// sh_type values
//

//
// Inactive
//

#define SHT_NULL        0

//
// Program defined information
//

#define SHT_PROGBITS        1

//
// Symbol table section
//

#define SHT_SYMTAB      2

//
// String table section
//

#define SHT_STRTAB      3

//
// Relocation section with addends
//

#define SHT_RELA        4

//
// Symbol hash table section
//

#define SHT_HASH        5

//
// Dynamic section
//

#define SHT_DYNAMIC     6

//
// Note section
//

#define SHT_NOTE        7

//
// No-space section
//

#define SHT_NOBITS      8

//
// Relocation section, no addends
//

#define SHT_REL         9

//
// Reserved, purpose unknown
//

#define SHT_SHLIB       10

//
// Dynamic symbol table section
//

#define SHT_DYNSYM      11

//
// Initialization function pointers
//

#define SHT_INIT_ARRAY      14

//
// Termination function pointers
//

#define SHT_FINI_ARRAY      15

//
// Pre-initialization function pointers
//

#define SHT_PREINIT_ARRAY   16

//
// Section group
//

#define SHT_GROUP       17

//
// Section indices
//

#define SHT_SYMTAB_SHNDX    18

//
// First of OS specific semantic values
//

#define SHT_LOOS        0x60000000
#define SHT_LOSUNW      0x6ffffff4
#define SHT_SUNW_dof        0x6ffffff4
#define SHT_SUNW_cap        0x6ffffff5
#define SHT_SUNW_SIGNATURE  0x6ffffff6
#define SHT_SUNW_ANNOTATE   0x6ffffff7
#define SHT_SUNW_DEBUGSTR   0x6ffffff8
#define SHT_SUNW_DEBUG      0x6ffffff9
#define SHT_SUNW_move       0x6ffffffa
#define SHT_SUNW_COMDAT     0x6ffffffb
#define SHT_SUNW_syminfo    0x6ffffffc
#define SHT_SUNW_verdef     0x6ffffffd
#define SHT_GNU_verdef      0x6ffffffd
#define SHT_SUNW_verneed    0x6ffffffe
#define SHT_GNU_verneed     0x6ffffffe
#define SHT_SUNW_versym     0x6fffffff
#define SHT_GNU_versym      0x6fffffff
#define SHT_HISUNW      0x6fffffff

//
// Last of OS specific semantic values
//

#define SHT_HIOS        0x6fffffff

//
// Reserved range for processor
//

#define SHT_LOPROC      0x70000000

//
// Unwind information
//

#define SHT_AMD64_UNWIND    0x70000001

//
// Specific section header types
//

#define SHT_HIPROC      0x7fffffff

//
// Reserved range for applications
//

#define SHT_LOUSER      0x80000000

//
// Specific indices
//

#define SHT_HIUSER      0xffffffff

//
// Flags for sh_flags
//

//
// Section contains writable data
//

#define SHF_WRITE       0x1

//
// Section occupies memory
//

#define SHF_ALLOC       0x2

//
// Section contains instructions
//

#define SHF_EXECINSTR       0x4

//
// Section may be merged
//

#define SHF_MERGE       0x10

//
// Section contains strings
//

#define SHF_STRINGS     0x20

//
// sh_info holds the section index
//

#define SHF_INFO_LINK       0x40

//
// Special ordering requirements
//

#define SHF_LINK_ORDER      0x80

//
// OS-specific processing required
//

#define SHF_OS_NONCONFORMING    0x100

//
// Member of section grup
//

#define SHF_GROUP       0x200

//
// Section contains Thread Local Storage data
//

#define SHF_TLS         0x400

//
// OS-specific semantics
//

#define SHF_MASKOS  0x0ff00000

//
// Processor-specific semantics
//

#define SHF_MASKPROC    0xf0000000

//
// Values for p_type
//

//
// Unused entry
//

#define PT_NULL     0

//
// Loadable segment
//

#define PT_LOAD     1

//
// Dynamic linking information segment
//

#define PT_DYNAMIC  2

//
// Pathname of interpreter
//

#define PT_INTERP   3

//
// Auxiliary information
//

#define PT_NOTE     4

//
// Reserved (not used)
//

#define PT_SHLIB    5

//
// Location of the program header
//

#define PT_PHDR     6

//
// Thread Local Storage segment
//

#define PT_TLS      7

//
// First OS-specific value
//

#define PT_LOOS     0x60000000

//
// AMD64 unwind program header
//

#define PT_SUNW_UNWIND  0x6464e550
#define PT_GNU_EH_FRAME 0x6474e550
#define PT_LOSUNW   0x6ffffffa

//
// Sun-specific segment
//

#define PT_SUNWBSS  0x6ffffffa

//
// Describes the stack segment
//

#define PT_SUNWSTACK    0x6ffffffb

//
// Private
//

#define PT_SUNWDTRACE   0x6ffffffc

//
// Hard/soft capabilities segment
//

#define PT_SUNWCAP  0x6ffffffd
#define PT_HISUNW   0x6fffffff

//
// Last OS-specific value
//

#define PT_HIOS     0x6fffffff

//
// Processor-specific types
//

#define PT_LOPROC   0x70000000
#define PT_HIPROC   0x7fffffff

//
// Values for p_flags: executable, readable, writable.
//

#define PF_X        0x1
#define PF_W        0x2
#define PF_R        0x4
#define PF_MASKOS   0x0ff00000
#define PF_MASKPROC 0xf0000000

//
// Extended program header index
//

#define PN_XNUM     0xffff

//
// Values for d_tag
//

//
// Terminating entry
//

#define DT_NULL     0

//
// String table offset of a needed shared library
//

#define DT_NEEDED   1

//
// Total size in bytes of PLT relocations
//

#define DT_PLTRELSZ 2

//
// Processor-dependent entries
//

#define DT_PLTGOT   3

//
// Address of symbol hash table
//

#define DT_HASH     4

//
// Address of string table
//

#define DT_STRTAB   5

//
// Address of symbol table
//

#define DT_SYMTAB   6

//
// Address of ElfNN_Rela relocations
//

#define DT_RELA     7

//
// Total size of ElfNN_rela relocations
//

#define DT_RELASZ   8

//
// Size of each ElfNN_Rela relocation entry
//

#define DT_RELAENT  9

//
// Size of each string table
//

#define DT_STRSZ    10

//
// Size of each symbol table entry
//

#define DT_SYMENT   11

//
// Address of initialization function
//

#define DT_INIT     12

//
// Address of finalization function
//

#define DT_FINI     13

//
// String table offset of a shared object name
//

#define DT_SONAME   14

//
// String table offset of a library path
//

#define DT_RPATH    15

//
// Indicates "symbolic" linking.
//

#define DT_SYMBOLIC 16

//
// Address of ElfNN_Rel relocations
//

#define DT_REL      17

//
// Total size of ElfNN_Rel relocations
//

#define DT_RELSZ    18

//
// Size of each ElfNN_Rel relocation
//

#define DT_RELENT   19

//
// Type of relocation used for PLT
//

#define DT_PLTREL   20

//
// Reserved (not used).
//

#define DT_DEBUG    21

//
// Indicates there may be relocations in non-writable segments
//

#define DT_TEXTREL  22

//
// Address of PLT relocations
//

#define DT_JMPREL   23
#define DT_BIND_NOW 24

//
// Address of the array of pointers to initialization functions
//

#define DT_INIT_ARRAY   25

//
// Address of the array of pointers to termination functions
//

#define DT_FINI_ARRAY   26

//
// Size in bytes of the array of initialization functions
//

#define DT_INIT_ARRAYSZ 27

//
// Size in bytes of the array of termination functions
//

#define DT_FINI_ARRAYSZ 28

//
// String table offset of a null-terminated library search path string
//

#define DT_RUNPATH  29

//
// Object specific flag values
//

#define DT_FLAGS    30

#define DT_ENCODING 32

//
// Address of the array of pointers to pre-initialization functions
//

#define DT_PREINIT_ARRAY 32

//
// Size in bytes of the array of pre-initialization functions
//

#define DT_PREINIT_ARRAYSZ 33

//
// Number of positive tags
//

#define DT_MAXPOSTAGS   34

//
// First OS-specific value
//

#define DT_LOOS     0x6000000d

//
// Symbol auxiliary name
//

#define DT_SUNW_AUXILIARY   0x6000000d

//
// ld.so.1 info (private)
//

#define DT_SUNW_RTLDINF     0x6000000e

//
// Symbol filter name
//

#define DT_SUNW_FILTER      0x6000000f

//
// Hardware/software
//

#define DT_SUNW_CAP     0x60000010

//
// Last OS-specific
//

#define DT_HIOS     0x6ffff000

//
// DT_* entries which fall between DT_VALRNGHI and DT_VALRNGLO use the
// Dyn.d_un_d_val fields of the Elf*_Dyn structure.
//

#define DT_VALRNGLO 0x6ffffd00

//
// ELF checksum
//

#define DT_CHECKSUM 0x6ffffdf8

//
// PLT padding size
//

#define DT_PLTPADSZ 0x6ffffdf9

//
// Move table entry size
//

#define DT_MOVEENT  0x6ffffdfa

//
// Move table size
//

#define DT_MOVESZ   0x6ffffdfb

//
// Feature holder
//

#define DT_FEATURE_1    0x6ffffdfc

//
// Flags for DT_* entries.
//

#define DT_POSFLAG_1    0x6ffffdfd

//
// Syminfo table size (in bytes)
//

#define DT_SYMINSZ  0x6ffffdfe

//
// Syminfo entry size (in bytes)
//

#define DT_SYMINENT 0x6ffffdff
#define DT_VALRNGHI 0x6ffffdff

//
// DT_* entries which fall between DT_ADDRRNGHI and DT_ADDRRNGLO use the
// Dyn.d_un.d_ptr field of the Elf*_Dyn structure. If any adjustment is made
// to the ELF object after it has been build, these entries will need to be
// adjusted.
//

#define DT_ADDRRNGLO    0x6ffffe00

//
// Configuration information
//

#define DT_CONFIG   0x6ffffefa

//
// Dependency auditing
//

#define DT_DEPAUDIT 0x6ffffefb

//
// Object auditing
//

#define DT_AUDIT    0x6ffffefc

//
// PLT padding (sparcv9)
//

#define DT_PLTPAD   0x6ffffefd

//
// Move table
//

#define DT_MOVETAB  0x6ffffefe

//
// Syminfo table
//

#define DT_SYMINFO  0x6ffffeff
#define DT_ADDRRNGHI    0x6ffffeff

//
// Address of versym section
//

#define DT_VERSYM   0x6ffffff0

//
// Number of RELATIVE relocations
//

#define DT_RELACOUNT    0x6ffffff9
#define DT_RELCOUNT 0x6ffffffa

//
// State flags. See DF_1_* definitions
//

#define DT_FLAGS_1  0x6ffffffb

//
// Address of verdef section
//

#define DT_VERDEF   0x6ffffffc

//
// Number of elements in the verdef section
//

#define DT_VERDEFNUM    0x6ffffffd

//
// Address of verneed section
//

#define DT_VERNEED  0x6ffffffe

//
// Number of elements in the verneed section
//

#define DT_VERNEEDNUM   0x6fffffff

//
// Processor-specific range
//

#define DT_LOPROC   0x70000000
#define DT_DEPRECATED_SPARC_REGISTER    0x7000001

//
// Shared library auxiliary name
//

#define DT_AUXILIARY    0x7ffffffd

//
// Ignored, same as needed
//

#define DT_USED     0x7ffffffe

//
// Shared library filter name
//

#define DT_FILTER   0x7fffffff
#define DT_HIPROC   0x7fffffff

//
// Values for DT_FLAGS
//

//
// Indicates that the object being loaded may make reference to the $ORIGIN
// substitution string.
//

#define DF_ORIGIN   0x0001

//
// Indicates "symbolic" linking
//

#define DF_SYMBOLIC 0x0002

//
// Indicates there may be relocations in non-writable segments
//

#define DF_TEXTREL  0x0004

//
// Indicates that the dynamic linker should process all relocations for the
// object containing this entry before transferring control to the program
//

#define DF_BIND_NOW 0x0008

//
// Indicates that the shared object or executable contains code using a static
// thread-local storage scheme
//

#define DF_STATIC_TLS   0x0010

//
// Values for n_type. Used in core files.
//

//
// Process status
//

#define NT_PRSTATUS 1

//
// Floating point registers
//

#define NT_FPREGSET 2

//
// Process state info
//

#define NT_PRPSINFO 3

//
// Symbol Binding, ELFNN_ST_BIND - st_info
//

//
// Local symbol
//

#define STB_LOCAL   0

//
// Global symbol
//

#define STB_GLOBAL  1

//
// Global symbol with a lower precedence
//

#define STB_WEAK    2

//
// OS specific range
//

#define STB_LOOS    10
#define STB_HIOS    12

//
// Processor specific range
//

#define STB_LOPROC  13
#define STB_HIPROC  15

//
// Symbol type - ELFNN_ST_TYPE - st_info
//

//
// Unspecified type
//

#define STT_NOTYPE  0

//
// Data object
//

#define STT_OBJECT  1

//
// Function
//

#define STT_FUNC    2

//
// Section
//

#define STT_SECTION 3

//
// Source file
//

#define STT_FILE    4

//
// Uninitialized common block
//

#define STT_COMMON  5

//
// Thread local storage object
//

#define STT_TLS     6
#define STT_NUM     7

//
// OS-specific range
//

#define STT_LOOS    10
#define STT_HIOS    12

//
// Processor-specific range
//

#define STT_LOPROC  13
#define STT_HIPROC  15

//
// Symbol visibility - ELFNN_ST_VISIBILITY - st_other
//

//
// Default visibility (see binding)
//

#define STV_DEFAULT 0x0

//
// Special meaning in relocatable objects
//

#define STV_INTERNAL    0x1

//
// Not visible
//

#define STV_HIDDEN  0x2

//
// Visible but not preemptible
//

#define STV_PROTECTED   0x3

//
// Special symbol table indices - Undefined symbol index
//

#define STN_UNDEF   0

#define VER_DEF_CURRENT 1
#define VER_DEF_IDX(x)  VER_NDX(x)

#define VER_FLG_BASE    0x01
#define VER_FLG_WEAK    0x02

#define VER_NEED_CURRENT    1
#define VER_NEED_WEAK   (1u << 15)
#define VER_NEED_HIDDEN VER_NDX_HIDDEN
#define VER_NEED_IDX(x) VER_NDX(x)

#define VER_NDX_LOCAL   0
#define VER_NDX_GLOBAL  1
#define VER_NDX_GIVEN   2

#define VER_NDX_HIDDEN  (1u << 15)
#define VER_NDX(x)  ((x) & ~(1u << 15))

#define CA_SUNW_NULL    0

//
// First hardware capabilities array
//

#define CA_SUNW_HW_1    1

//
// First software capabilities array
//

#define CA_SUNW_SF_1    2

//
// Syminfo flag values
//

//
// The symbol reference has direc association with an object containing a
// definition.
//

#define SYMINFO_FLG_DIRECT  0x0001

//
// Ignored, see SYMINFO_FLG_FILTER
//

#define SYMINFO_FLG_PASSTHRU    0x0002

//
// The symbol is a copy-reloc
//

#define SYMINFO_FLG_COPY    0x0004

//
// The object containing definition should be lazily loaded.
//

#define SYMINFO_FLG_LAZYLOAD    0x0008

//
// The reference should be found directly to the object containing the
// definition.
//

#define SYMINFO_FLG_DIRECTBIND  0x0010

//
// Don't let an external reference directly bind to this symbol.
//

#define SYMINFO_FLG_NOEXTDIRECT 0x0020

//
// The symbol reference is associated to a standard or auxiliary filter.
//

#define SYMINFO_FLG_FILTER  0x0002
#define SYMINFO_FLG_AUXILIARY   0x0040

//
// Syminfo.si_boundto values
//

//
// The symbol is bound to itself.
//

#define SYMINFO_BT_SELF     0xffff

//
// The symbol is bound to its parent.
//

#define SYMINFO_BT_PARENT   0xfffe

//
// The symbol has no special symbol binding
//

#define SYMINFO_BT_NONE     0xfffd

//
// The symbol is defined as external
//

#define SYMINFO_BT_EXTERN   0xfffc

//
// Reserved entries
//

#define SYMINFO_BT_LOWRESERVE   0xff00

//
// Syminfo version values
//

#define SYMINFO_NONE        0
#define SYMINFO_CURRENT     1
#define SYMINFO_NUM     2

//
// Relocation types
// All machine architectures are defined here to allow tools on one to handle
// others.
//

//
// No relocation
//

#define R_386_NONE      0

//
// Add symbol value
//

#define R_386_32        1

//
// Add PC-relative symbol value
//

#define R_386_PC32      2

//
// Add PC-relative GOT offset
//

#define R_386_GOT32     3

//
// Add PC-relative PLT offset
//

#define R_386_PLT32     4

//
// Copy data from the shared object
//

#define R_386_COPY      5

//
// Set GOT entry to data address
//

#define R_386_GLOB_DAT      6

//
// Set GOT entry to code address
//

#define R_386_JMP_SLOT      7

//
// Add load address of shared object
//

#define R_386_RELATIVE      8

//
// Add GOT-relative symbol address
//

#define R_386_GOTOFF        9

//
// Add PC-relative GOT table address
//

#define R_386_GOTPC     10

//
// Negative offset in static TLS block
//

#define R_386_TLS_TPOFF     14

//
// Absolute address of GOT for -ve static TLS
//

#define R_386_TLS_IE        15

//
// GOT entry for negative static TLS block
//

#define R_386_TLS_GOTIE     16

//
// Negative offset relative to static TLD
//

#define R_386_TLS_LE        17

//
// 32-bit offset to GOT (index, offset) pair
//

#define R_386_TLS_GD        18

//
// 32-bit offset to GOT (index, zero) pair
//

#define R_386_TLS_LDM       19

//
// 32-bit offset to GOT (index, offset) pair
//

#define R_386_TLS_GD_32     24

//
// Pushl instruction for Sun ABI GD sequence
//

#define R_386_TLS_GD_PUSH   25

//
// Call instruction for Sun ABI GD sequence
//

#define R_386_TLS_GD_CALL   26

//
// Popl instruction for Sun ABI GD sequence
//

#define R_386_TLS_GD_POP    27

//
// 32-bit offset to GOT (index, zero) pair
//

#define R_386_TLS_LDM_32    28

//
// Pushl instruction for Sun ABI LD sequence
//

#define R_386_TLS_LDM_PUSH  29

//
// Call instruction for Sun ABI LD sequence
//

#define R_386_TLS_LDM_CALL  30

//
// Popl instruction for Sun ABI LD sequence
//

#define R_386_TLS_LDM_POP   31

//
// 32-bit offset from start of TLS block
//

#define R_386_TLS_LDO_32    32

//
// 32-bit offset to GOT static TLS offset entry
//

#define R_386_TLS_IE_32     33

//
// 32-bit offset within static TLS block
//

#define R_386_TLS_LE_32     34

//
// GOT entry containing TLS index
//

#define R_386_TLS_DTPMOD32  35

//
// GOT entry containing TLS offset
//

#define R_386_TLS_DTPOFF32  36

//
// GOT entry of -ve static TLS offset
//

#define R_386_TLS_TPOFF32   37

//
// AArch64 relocations
//

//
// No relocation
//

#define R_AARCH64_NONE              256

//
// Static AArch64 relocations
//

//
// S + A
//

#define R_AARCH64_ABS64             257
#define R_AARCH64_ABS32             258
#define R_AARCH64_ABS16             259

//
// S + A - P
//
#define R_AARCH64_PREL64            260
#define R_AARCH64_PREL32            261
#define R_AARCH64_PREL16            262

//
// Group relocations to create a 16, 32, 48, or 64 bit unsigned ata value
// or address inline.
// S + A
//

#define R_AARCH64_MOVW_UABS_G0          263
#define R_AARCH64_MOVW_UABS_G0_NC       264
#define R_AARCH64_MOVW_UABS_G1          265
#define R_AARCH64_MOVW_UABS_G1_NC       266
#define R_AARCH64_MOVW_UABS_G2          267
#define R_AARCH64_MOVW_UABS_G2_NC       268
#define R_AARCH64_MOVW_UABS_G3          269

//
// Group relocations to create a 16, 32, 48, or 64 bit signed ata or offset
// value inline.
// S + A
//

#define R_AARCH64_MOVW_SABS_G0          270
#define R_AARCH64_MOVW_SABS_G1          271
#define R_AARCH64_MOVW_SABS_G2          272

//
// Relocations to generate 19, 21, and 33 bit PC-relative addresses
// S + A - P
//

#define R_AARCH64_LD_PREL_LO19          273
#define R_AARCH64_ADR_PREL_LO21         274

//
// Page(S + A) - Page(P)
//

#define R_AARCH64_ADR_PREL_PG_HI21      275
#define R_AARCH64_ADR_PREL_PG_HI21_NC       276

//
// S + A
//

#define R_AARCH64_ADD_ABS_LO12_NC       277
#define R_AARCH64_LDST8_ABS_LO12_NC     278
#define R_AARCH64_LDST16_ABS_LO12_NC        284
#define R_AARCH64_LDST32_ABS_LO12_NC        285
#define R_AARCH64_LDST64_ABS_LO12_NC        286
#define R_AARCH64_LDST128_ABS_LO12_NC       299

//
// Relocations for control-flow instructions. All offsets are a multiple of 4.
// S + A - P

#define R_AARCH64_TSTBR14           279
#define R_AARCH64_CONDBR19          280
#define R_AARCH64_JUMP26            282
#define R_AARCH64_CALL26            283

//
// Group relocations to create a 16, 32, 48, or 64-bit PC-relative offset
// inline
// S + A - P
//

#define R_AARCH64_MOVW_PREL_G0          287
#define R_AARCH64_MOVW_PREL_G0_NC       288
#define R_AARCH64_MOVW_PREL_G1          289
#define R_AARCH64_MOVW_PREL_G1_NC       290
#define R_AARCH64_MOVW_PREL_G2          291
#define R_AARCH64_MOVW_PREL_G2_NC       292
#define R_AARCH64_MOVW_PREL_G3          293

//
// Group relocations to create a 16, 32, 48, or 64 bit GOT-relative offset
// inline.
// G(S) - GOT
//

#define R_AARCH64_MOVW_GOTOFF_G0        300
#define R_AARCH64_MOVW_GOTOFF_G0_NC     301
#define R_AARCH64_MOVW_GOTOFF_G1        302
#define R_AARCH64_MOVW_GOTOFF_G1_NC     303
#define R_AARCH64_MOVW_GOTOFF_G2        304
#define R_AARCH64_MOVW_GOTOFF_G2_NC     305
#define R_AARCH64_MOVW_GOTOFF_G3        306

//
// GOT-relative data relocations
// S + A - GOT
//

#define R_AARCH64_GOTREL64          307
#define R_AARCH64_GOTREL32          308

//
// GOT-relative instruction relocations
//

//
// G(S) - P
//

#define R_AARCH64_GOT_LD_PREL19         309

//
// G(S) - GOT
//

#define R_AARCH64_LD64_GOTOFF_LO15      310

//
// Page(G(S)) - Page(P)
//

#define R_AARCH64_ADR_GOT_PAGE          311

//
// G(S)
//

#define R_AARCH64_LD64_GOT_LO12_NC      312

//
// G(S) - Page(GOT)
//

#define R_AARCH64_LD64_GOTPAGE_LO15     313

//
// Relocations for thread-local storage. General dynamic TLS relocations
//

//
// G(TLSIDX(S + A)) - P
//

#define R_AARCH64_TLSGD_ADR_PREL21      512

//
// Page(G(TLSIDX(S + A))) - Page(P)
//

#define R_AARCH64_TLSGD_ADR_PAGE21      513

//
// G(TLSIDX(S + A)
//

#define R_AARCH64_TLSGD_ADD_LO12_NC     514

//
// G(TLSIDX(S + A)) - GOT
//

#define R_AARCH64_TLSGD_MOVW_G1         515
#define R_AARCH64_TLSGD_MOVW_G0_NC      516

//
// Local Dynamic TLS relocations
//

//
// G(LDM(S))) - P
//

#define R_AARCH64_TLSLD_ADR_PREL21      517

//
// Page(G(LDM(S))) - Page(P)
//

#define R_AARCH64_TLSLD_ADR_PAGE21      518

//
// G(LDM(S))
//

#define R_AARCH64_TLSLD_ADD_LO12_NC     519

//
// G(LDM(S)) - GOT
//

#define R_AARCH64_TLSLD_MOVW_G1         520
#define R_AARCH64_TLSLD_MOVW_G0_NC      521

//
// G(LDM(S)) - P
//

#define R_AARCH64_TLSLD_LD_PREL19       522

//
// DTPREL(S + A)
//

#define R_AARCH64_TLSLD_MOVW_DTPREL_G2      523
#define R_AARCH64_TLSLD_MOVW_DTPREL_G1      524
#define R_AARCH64_TLSLD_MOVW_DTPREL_G1_NC   525
#define R_AARCH64_TLSLD_MOVW_DTPREL_G0      526
#define R_AARCH64_TLSLD_MOVW_DTPREL_G0_NC   527
#define R_AARCH64_TLSLD_ADD_DTPREL_HI12     528
#define R_AARCH64_TLSLD_ADD_DTPREL_LO12     529
#define R_AARCH64_TLSLD_ADD_DTPREL_LO12_NC  530
#define R_AARCH64_TLSLD_LDST8_DTPREL_LO12   531
#define R_AARCH64_TLSLD_LDST8_DTPREL_LO12_NC    532
#define R_AARCH64_TLSLD_LDST16_DTPREL_LO12  533
#define R_AARCH64_TLSLD_LDST16_DTPREL_LO12_NC   534
#define R_AARCH64_TLSLD_LDST32_DTPREL_LO12  535
#define R_AARCH64_TLSLD_LDST32_DTPREL_LO12_NC   536
#define R_AARCH64_TLSLD_LDST64_DTPREL_LO12  537
#define R_AARCH64_TLSLD_LDST64_DTPREL_LO12_NC   538

//
// Initial Exec TLS relocations
// G(TPREL(S + A)) - GOT
//

#define R_AARCH64_TLSIE_MOVW_GOTTPREL_G1    539
#define R_AARCH64_TLSIE_MOVW_GOTTPREL_G0_NC 540

//
// Page(G(TPREL(S + A))) - Page(P)
//

#define R_AARCH64_TLSIE_ADR_GOTTPREL_PAGE21 541

//
// G(TPREL(S + A))
//

#define R_AARCH64_TLSIE_LD64_GOTTPREL_LO12_NC   542

//
// G(TPREL(S + A)) - P
//

#define R_AARCH64_TLSIE_LD_GOTTPREL_PREL19  543

//
// Local Exec TLS relocations
// TPREL(S + A)
//

#define R_AARCH64_TLSLE_MOVW_TPREL_G2       544
#define R_AARCH64_TLSLE_MOVW_TPREL_G1       545
#define R_AARCH64_TLSLE_MOVW_TPREL_G1_NC    546
#define R_AARCH64_TLSLE_MOVW_TPREL_G0       547
#define R_AARCH64_TLSLE_MOVW_TPREL_G0_NC    548
#define R_AARCH64_TLSLE_ADD_TPREL_HI12      549
#define R_AARCH64_TLSLE_ADD_TPREL_LO12      550
#define R_AARCH64_TLSLE_ADD_TPREL_LO12_NC   551
#define R_AARCH64_TLSLE_LDST8_TPREL_LO12    552
#define R_AARCH64_TLSLE_LDST8_TPREL_LO12_NC 553
#define R_AARCH64_TLSLE_LDST16_TPREL_LO12   554
#define R_AARCH64_TLSLE_LDST16_TPREL_LO12_NC    555
#define R_AARCH64_TLSLE_LDST32_TPREL_LO12   556
#define R_AARCH64_TLSLE_LDST32_TPREL_LO12_NC    557
#define R_AARCH64_TLSLE_LDST64_TPREL_LO12   558
#define R_AARCH64_TLSLE_LDST64_TPREL_LO12_NC    559

//
// Dynamic relocations
//

#define R_AARCH64_COPY              1024

//
// S + A
//

#define R_AARCH64_GLOB_DAT          1025
#define R_AARCH64_JUMP_SLOT         1026

//
// Delta(S) + A, Delta(P) + A
//

#define R_AARCH64_RELATIVE          1027

//
// DTPREL(S + A)
//

#define R_AARCH64_TLS_DTPREL64          1028

//
// LDM(S)
//

#define R_AARCH64_TLS_DTPMOD64          1029

//
// TPREL(S + A)
//

#define R_AARCH64_TLS_TPREL64           1030

//
// DTPREL(S + A)
//

#define R_AARCH64_TLS_DTPREL32          1031

//
// LDM(S)
//

#define R_AARCH64_TLS_DTPMOD32          1032

//
// DTPREL(S + A)
//

#define R_AARCH64_TLS_TPREL32           1033

//
// Alpha relocations
//

//
// No relocation
//

#define R_ALPHA_NONE        0

//
// Direct 32-bit
//

#define R_ALPHA_REFLONG     1

//
// Direct 64-bit
//

#define R_ALPHA_REFQUAD     2

//
// GP relative 32-bit
//

#define R_ALPHA_GPREL32     3

//
// GP relative 16-bit with optimization
//

#define R_ALPHA_LITERAL     4

//
// Optimization hint for literal
//

#define R_ALPHA_LITUSE      5

//
// Add displacement to GP
//

#define R_ALPHA_GPDISP      6

//
// PC + 4 relative 23-bit shifted
//

#define R_ALPHA_BRADDR      7

//
// PC + 4 relative 16-bit shifted
//

#define R_ALPHA_HINT        8

//
// PC relateive 16 bit
//

#define R_ALPHA_SREL16      9

//
// PC relative 32-bit
//

#define R_ALPHA_SREL32      10

//
// PC relative 64-bit
//

#define R_ALPHA_SREL64      11

//
// OP stack push
//

#define R_ALPHA_OP_PUSH     12

//
// OP stack pop and store
//

#define R_ALPHA_OP_STORE    13

//
// OP stack add
//

#define R_ALPHA_OP_PSUB     14

//
// OP stack right shift
//

#define R_ALPHA_OP_PRSHIFT  15
#define R_ALPHA_GPVALUE     16
#define R_ALPHA_GPRELHIGH   17
#define R_ALPHA_GPRELLOW    18
#define R_ALPHA_IMMED_GP_16 19
#define R_ALPHA_IMMED_GP_HI32   20
#define R_ALPHA_IMMED_SCN_HI32  21
#define R_ALPHA_IMMED_BR_HI32   22
#define R_ALPHA_IMMED_LO32  23

//
// Copy symbol at runtime
//

#define R_ALPHA_COPY        24

//
// Create GOT entry
//

#define R_ALPHA_GLOB_DAT    25

//
// Create PLT entry
//

#define R_ALPHA_JMP_SLOT    26

//
// Adjust by program base
//

#define R_ALPHA_RELATIVE    27

//
// ARM relocations
//

#define R_ARM_NONE          0
#define R_ARM_PC24          1
#define R_ARM_ABS32         2
#define R_ARM_REL32         3
#define R_ARM_PC13          4
#define R_ARM_ABS16         5
#define R_ARM_ABS12         6
#define R_ARM_THM_ABS5      7
#define R_ARM_ABS8          8
#define R_ARM_SBREL32       9
#define R_ARM_THM_PC22      10
#define R_ARM_THM_PC8       11
#define R_ARM_AMP_VCALL9    12
#define R_ARM_SWI24         13
#define R_ARM_THM_SWI8      14
#define R_ARM_XPC25         15
#define R_ARM_THM_XPC22     16
#define R_ARM_BASE_PREL     25
#define R_ARM_GOT_BREL      26

//
// Copy data from shared object
//

#define R_ARM_COPY      20

//
// Set GOT entry to data address
//

#define R_ARM_GLOB_DAT      21

//
// Set GOT entry to code address
//

#define R_ARM_JUMP_SLOT     22

//
// Add load address of shared object
//

#define R_ARM_RELATIVE      23

//
// Add GOT-relative symbol address
//

#define R_ARM_GOTOFF        24

//
// Add PC-relative GOT table address
//

#define R_ARM_GOTPC     25

//
// Add PC-relative GOT offset
//

#define R_ARM_GOT32     26

//
// Add PC-relative PLT offset
//

#define R_ARM_PLT32     27
#define R_ARM_CALL            28
#define R_ARM_JMP24           29
#define R_ARM_THM_MOVW_ABS_NC 47
#define R_ARM_THM_MOVT_ABS    48

//
// This block of PC-relative relocations was added to work around GCC putting
// object relocations in static executables.
//

#define R_ARM_THM_JUMP24        30
#define R_ARM_PREL31            42
#define R_ARM_MOVW_PREL_NC      45
#define R_ARM_MOVT_PREL         46
#define R_ARM_THM_MOVW_PREL_NC  49
#define R_ARM_THM_MOVT_PREL     50
#define R_ARM_THM_JMP6          52
#define R_ARM_THM_ALU_PREL_11_0 53
#define R_ARM_THM_PC12          54
#define R_ARM_REL32_NOI         56
#define R_ARM_ALU_PC_G0_NC      57
#define R_ARM_ALU_PC_G0         58
#define R_ARM_ALU_PC_G1_NC      59
#define R_ARM_ALU_PC_G1         60
#define R_ARM_ALU_PC_G2         61
#define R_ARM_LDR_PC_G1         62
#define R_ARM_LDR_PC_G2         63
#define R_ARM_LDRS_PC_G0          64
#define R_ARM_LDRS_PC_G1          65
#define R_ARM_LDRS_PC_G2          66
#define R_ARM_LDC_PC_G0         67
#define R_ARM_LDC_PC_G1         68
#define R_ARM_LDC_PC_G2         69
#define R_ARM_GOT_PREL          96
#define R_ARM_THM_JUMP11       102
#define R_ARM_THM_JUMP8        103
#define R_ARM_TLS_GD32         104
#define R_ARM_TLS_LDM32        105
#define R_ARM_TLS_IE32         107

#define R_ARM_THM_JUMP19    51
#define R_ARM_GNU_VTENTRY   100
#define R_ARM_GNU_VTINHERIT 101
#define R_ARM_RSBREL32      250
#define R_ARM_THM_RPC22     251
#define R_ARM_RREL32        252
#define R_ARM_RABS32        253
#define R_ARM_RPC24     254
#define R_ARM_RBASE     255

//
// IA-64 relocations
//

#define R_IA_64_NONE        0

//
// Immediate14 S + A
//

#define R_IA_64_IMM14       0x21

//
// Immediate22 S + A
//

#define R_IA_64_IMM22       0x22

//
// Immediate64 S + A
//

#define R_IA_64_IMM64       0x23

//
// Word32 MSB S + A
//

#define R_IA_64_DIR32MSB    0x24

//
// Word32 LSB S + A
//

#define R_IA_64_DIR32LSB    0x25

//
// Word64 MSB S + A
//

#define R_IA_64_DIR64MSB    0x26

//
// Word64 LSB S + A
//

#define R_IA_64_DIR64LSB    0x27

//
// Immediate22 @gprel(S + A)
//

#define R_IA_64_GPREL22     0x2a

//
// Immediate64 @gprel(S + A)
//

#define R_IA_64_GPREL64I    0x2b

//
// Word32 MSB @gprel(S + A)
//

#define R_IA_64_GPREL32MSB  0x2c

//
// Word32 LSB @gprel(S + A)
//

#define R_IA_64_GPREL32LSB  0x2d

//
// Word64 MSB @gprel(S + A)
//

#define R_IA_64_GPREL64MSB  0x2e

//
// Word64 LSB @gprel(S + A)
//

#define R_IA_64_GPREL64LSB  0x2f

//
// Immediate22 @ltoff(S + A)
//

#define R_IA_64_LTOFF22     0x32

//
// Immediate64 @ltoff (S + A)
//

#define R_IA_64_LTOFF64I    0x33

//
// Immediate22 @pltoff(S + A)
//

#define R_IA_64_PLTOFF22    0x3a

//
// Immediate64 @pltoff(S + A)
//

#define R_IA_64_PLTOFF64I   0x3b

//
// Word64 MSB @pltoff(S + A)
//

#define R_IA_64_PLTOFF64MSB 0x3e

//
// Word64 LSB @pltoff(S + A)
//

#define R_IA_64_PLTOFF64LSB 0x3f

//
// Immediate64 @fptr(S + A)
//

#define R_IA_64_FPTR64I     0x43

//
// Word32 MSB @fptr(S + A)
//

#define R_IA_64_FPTR32MSB   0x44

//
// Word32 LSB @fptr(S + A)
//

#define R_IA_64_FPTR32LSB   0x45

//
// Word64 MSB @fptr(S + A)
//

#define R_IA_64_FPTR64MSB   0x46

//
// Word64 LSB @fptr(S + A)
//

#define R_IA_64_FPTR64LSB   0x47

//
// Immediate60 form1 S + A - P
//

#define R_IA_64_PCREL60B    0x48

//
// Immediate21 forms 1, 2, and 3 S + A - P
//

#define R_IA_64_PCREL21B    0x49
#define R_IA_64_PCREL21M    0x4a
#define R_IA_64_PCREL21F    0x4b

//
// Word32/64 MSB and LSB: S + A - P
//

#define R_IA_64_PCREL32MSB  0x4c
#define R_IA_64_PCREL32LSB  0x4d
#define R_IA_64_PCREL64MSB  0x4e
#define R_IA_64_PCREL64LSB  0x4f

//
// Immediate22/64 @ltoff(@fptr(S + A))
//

#define R_IA_64_LTOFF_FPTR22    0x52
#define R_IA_64_LTOFF_FPTR64I   0x53

//
// Word32/64 MSB and LSB: @ltoff(@fptr(S + A))
//

#define R_IA_64_LTOFF_FPTR32MSB 0x54
#define R_IA_64_LTOFF_FPTR32LSB 0x55
#define R_IA_64_LTOFF_FPTR64MSB 0x56
#define R_IA_64_LTOFF_FPTR64LSB 0x57

//
// Word32/64 MSB/LSB: @segrel(S + A)
//

#define R_IA_64_SEGREL32MSB 0x5c
#define R_IA_64_SEGREL32LSB 0x5d
#define R_IA_64_SEGREL64MSB 0x5e
#define R_IA_64_SEGREL64LSB 0x5f

//
// Word32/64 MSB/LSB @secrel(S + A)
//

#define R_IA_64_SECREL32MSB 0x64
#define R_IA_64_SECREL32LSB 0x65
#define R_IA_64_SECREL64MSB 0x66
#define R_IA_64_SECREL64LSB 0x67

//
// Word32/64 MSB/LSB: BD + A
//

#define R_IA_64_REL32MSB    0x6c
#define R_IA_64_REL32LSB    0x6d
#define R_IA_64_REL64MSB    0x6e
#define R_IA_64_REL64LSB    0x6f

//
// Word32/64 MSB/LSB: S + A
//

#define R_IA_64_LTV32MSB    0x74
#define R_IA_64_LTV32LSB    0x75
#define R_IA_64_LTV64MSB    0x76
#define R_IA_64_LTV64LSB    0x77

//
// Immediate21 form1 S + A - P
//

#define R_IA_64_PCREL21BI   0x79

//
// Immediate22/64: S + A - P
//

#define R_IA_64_PCREL22     0x7a
#define R_IA_64_PCREL64I    0x7b

//
// Function descriptor MSB/LSB special
//

#define R_IA_64_IPLTMSB     0x80
#define R_IA_64_IPLTLSB     0x81

//
// Immediate64: A - S
//

#define R_IA_64_SUB     0x85

//
// Immediate22 special
//

#define R_IA_64_LTOFF22X    0x86
#define R_IA_64_LDXMOV      0x87

//
// Immediate14/22/64 @tprel(S + A)
//

#define R_IA_64_TPREL14     0x91
#define R_IA_64_TPREL22     0x92
#define R_IA_64_TPREL64I    0x93

//
// Word64 MSB/LSB @tprel(S + A)
//

#define R_IA_64_TPREL64MSB  0x96
#define R_IA_64_TPREL64LSB  0x97

//
// Immediate22 @ltoff(@tprel(S + A))
//

#define R_IA_64_LTOFF_TPREL22   0x9a

//
// Word64 MSB/LSB @dtpmod(S + A)
//

#define R_IA_64_DTPMOD64MSB 0xa6
#define R_IA_64_DTPMOD64LSB 0xa7

//
// Immediate22 @ltoff(@dtpmod(S + A))
//

#define R_IA_64_LTOFF_DTPMOD22  0xaa

//
// Immediate14/22/64 @dtprel(S + A)
//

#define R_IA_64_DTPREL14    0xb1
#define R_IA_64_DTPREL22    0xb2
#define R_IA_64_DTPREL64I   0xb3

//
// Word32/64 MSB/LSB @dtprel(S + A)
//

#define R_IA_64_DTPREL32MSB 0xb4
#define R_IA_64_DTPREL32LSB 0xb5
#define R_IA_64_DTPREL64MSB 0xb6
#define R_IA_64_DTPREL64LSB 0xb7

//
// Immediate22 @ltof(@dtprel(S + A))
//

#define R_IA_64_LTOFF_DTPREL22  0xba

//
// PowerPC relocations
//

#define R_PPC_NONE      0
#define R_PPC_ADDR32        1
#define R_PPC_ADDR24        2
#define R_PPC_ADDR16        3
#define R_PPC_ADDR16_LO     4
#define R_PPC_ADDR16_HI     5
#define R_PPC_ADDR16_HA     6
#define R_PPC_ADDR14        7
#define R_PPC_ADDR14_BRTAKEN    8
#define R_PPC_ADDR14_BRNTAKEN   9
#define R_PPC_REL24     10
#define R_PPC_REL14     11
#define R_PPC_REL14_BRTAKEN 12
#define R_PPC_REL14_BRNTAKEN    13
#define R_PPC_GOT16     14
#define R_PPC_GOT16_LO      15
#define R_PPC_GOT16_HI      16
#define R_PPC_GOT16_HA      17
#define R_PPC_PLTREL24      18
#define R_PPC_COPY      19
#define R_PPC_GLOB_DAT      20
#define R_PPC_JMP_SLOT      21
#define R_PPC_RELATIVE      22
#define R_PPC_LOCAL24PC     23
#define R_PPC_UADDR32       24
#define R_PPC_UADDR16       25
#define R_PPC_REL32     26
#define R_PPC_PLT32     27
#define R_PPC_PLTREL32      28
#define R_PPC_PLT16_LO      29
#define R_PPC_PLT16_HI      30
#define R_PPC_PLT16_HA      31
#define R_PPC_SDAREL16      32
#define R_PPC_SECTOFF       33
#define R_PPC_SECTOFF_LO    34
#define R_PPC_SECTOFF_HI    35
#define R_PPC_SECTOFF_HA    36

//
// PowerPC TLS relocations
//

#define R_PPC_TLS       67
#define R_PPC_DTPMOD32      68
#define R_PPC_TPREL16       69
#define R_PPC_TPREL16_LO    70
#define R_PPC_TPREL16_HI    71
#define R_PPC_TPREL16_HA    72
#define R_PPC_TPREL32       73
#define R_PPC_DTPREL16      74
#define R_PPC_DTPREL16_LO   75
#define R_PPC_DTPREL16_HI   76
#define R_PPC_DTPREL16_HA   77
#define R_PPC_DTPREL32      78
#define R_PPC_GOT_TLSGD16   79
#define R_PPC_GOT_TLSGD16_LO    80
#define R_PPC_GOT_TLSGD16_HI    81
#define R_PPC_GOT_TLSGD16_HA    82
#define R_PPC_GOT_TLSLD16   83
#define R_PPC_GOT_TLSLD16_LO    84
#define R_PPC_GOT_TLSLD16_HI    85
#define R_PPC_GOT_TLSLD16_HA    86
#define R_PPC_GOT_TPREL16   87
#define R_PPC_GOT_TPREL16_LO    88
#define R_PPC_GOT_TPREL16_HI    89
#define R_PPC_GOT_TPREL16_HA    90

//
// The remaining PowerPC relocations are from the embedded ELF API, and are not
// in the SVR4 ELF ABI.
//

#define R_PPC_EMB_NADDR32   101
#define R_PPC_EMB_NADDR16   102
#define R_PPC_EMB_NADDR16_LO    103
#define R_PPC_EMB_NADDR16_HI    104
#define R_PPC_EMB_NADDR16_HA    105
#define R_PPC_EMB_SDAI16    106
#define R_PPC_EMB_SDA2I16   107
#define R_PPC_EMB_SDA2REL   108
#define R_PPC_EMB_SDA21     109
#define R_PPC_EMB_MRKREF    110
#define R_PPC_EMB_RELSEC16  111
#define R_PPC_EMB_RELST_LO  112
#define R_PPC_EMB_RELST_HI  113
#define R_PPC_EMB_RELST_HA  114
#define R_PPC_EMB_BIT_FLD   115
#define R_PPC_EMB_RELSDA    116

#define R_SPARC_NONE        0
#define R_SPARC_8       1
#define R_SPARC_16      2
#define R_SPARC_32      3
#define R_SPARC_DISP8       4
#define R_SPARC_DISP16      5
#define R_SPARC_DISP32      6
#define R_SPARC_WDISP30     7
#define R_SPARC_WDISP22     8
#define R_SPARC_HI22        9
#define R_SPARC_22      10
#define R_SPARC_13      11
#define R_SPARC_LO10        12
#define R_SPARC_GOT10       13
#define R_SPARC_GOT13       14
#define R_SPARC_GOT22       15
#define R_SPARC_PC10        16
#define R_SPARC_PC22        17
#define R_SPARC_WPLT30      18
#define R_SPARC_COPY        19
#define R_SPARC_GLOB_DAT    20
#define R_SPARC_JMP_SLOT    21
#define R_SPARC_RELATIVE    22
#define R_SPARC_UA32        23
#define R_SPARC_PLT32       24
#define R_SPARC_HIPLT22     25
#define R_SPARC_LOPLT10     26
#define R_SPARC_PCPLT32     27
#define R_SPARC_PCPLT22     28
#define R_SPARC_PCPLT10     29
#define R_SPARC_10      30
#define R_SPARC_11      31
#define R_SPARC_64      32
#define R_SPARC_OLO10       33
#define R_SPARC_HH22        34
#define R_SPARC_HM10        35
#define R_SPARC_LM22        36
#define R_SPARC_PC_HH22     37
#define R_SPARC_PC_HM10     38
#define R_SPARC_PC_LM22     39
#define R_SPARC_WDISP16     40
#define R_SPARC_WDISP19     41
#define R_SPARC_GLOB_JMP    42
#define R_SPARC_7       43
#define R_SPARC_5       44
#define R_SPARC_6       45
#define R_SPARC_DISP64      46
#define R_SPARC_PLT64       47
#define R_SPARC_HIX22       48
#define R_SPARC_LOX10       49
#define R_SPARC_H44     50
#define R_SPARC_M44     51
#define R_SPARC_L44     52
#define R_SPARC_REGISTER    53
#define R_SPARC_UA64        54
#define R_SPARC_UA16        55
#define R_SPARC_TLS_GD_HI22 56
#define R_SPARC_TLS_GD_LO10 57
#define R_SPARC_TLS_GD_ADD  58
#define R_SPARC_TLS_GD_CALL 59
#define R_SPARC_TLS_LDM_HI22    60
#define R_SPARC_TLS_LDM_LO10    61
#define R_SPARC_TLS_LDM_ADD 62
#define R_SPARC_TLS_LDM_CALL    63
#define R_SPARC_TLS_LDO_HIX22   64
#define R_SPARC_TLS_LDO_LOX10   65
#define R_SPARC_TLS_LDO_ADD 66
#define R_SPARC_TLS_IE_HI22 67
#define R_SPARC_TLS_IE_LO10 68
#define R_SPARC_TLS_IE_LD   69
#define R_SPARC_TLS_IE_LDX  70
#define R_SPARC_TLS_IE_ADD  71
#define R_SPARC_TLS_LE_HIX22    72
#define R_SPARC_TLS_LE_LOX10    73
#define R_SPARC_TLS_DTPMOD32    74
#define R_SPARC_TLS_DTPMOD64    75
#define R_SPARC_TLS_DTPOFF32    76
#define R_SPARC_TLS_DTPOFF64    77
#define R_SPARC_TLS_TPOFF32 78
#define R_SPARC_TLS_TPOFF64 79

//
// AMD64 relocations
//

#define R_X86_64_NONE       0

//
// Add 64-bit symbol value
//

#define R_X86_64_64     1

//
// PC-relative 32-bit signed symbol value
//

#define R_X86_64_PC32       2

//
// PC-relative 32-bit GOT offset
//

#define R_X86_64_GOT32      3

//
// PC-relative 32-bit PLT offset
//

#define R_X86_64_PLT32      4

//
// Copy data from the shared object
//

#define R_X86_64_COPY       5

//
// Set GOT entry to data address
//

#define R_X86_64_GLOB_DAT   6

//
// Set GOT entry to code address
//

#define R_X86_64_JMP_SLOT   7

//
// Add the load address of the shared object
//

#define R_X86_64_RELATIVE   8

//
// Add 32-bit signed PC-relative offset to GOT
//

#define R_X86_64_GOTPCREL   9

//
// Add 32-bit zero extended symbol value
//

#define R_X86_64_32     10

//
// Add 32-bit sign-extended symbol value
//

#define R_X86_64_32S        11

//
// Add 16-bit zero extended symbol value
//

#define R_X86_64_16     12

//
// Add 16-bit sign-extended PC-relative symbol value
//

#define R_X86_64_PC16       13

//
// Add 8-bit zero extended symbol value
//

#define R_X86_64_8      14

//
// Add 8-bit sign-extended PC-relative symbol value
//

#define R_X86_64_PC8        15

//
// ID of the module containing the symbol
//

#define R_X86_64_DTPMOD64   16

//
// Offset in the TLS block
//

#define R_X86_64_DTPOFF64   17

//
// Offset in the static TLS block
//

#define R_X86_64_TPOFF64    18

//
// PC-relative offset to GD GOT entry
//

#define R_X86_64_TLSGD      19

//
// PC-relative offset to LD GOT entry
//

#define R_X86_64_TLSLD      20

//
// Offset in TLS block
//

#define R_X86_64_DTPOFF32   21

//
// PC-relative offset to IE GOT entry
//

#define R_X86_64_GOTTPOFF   22

//
// Offset in static TLS block
//

#define R_X86_64_TPOFF32    23

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Common ELF definitions
//

/*++

Structure Description:

    This structure defines an ELF note header. The ".note" section contains an
    array of notes. Each not begins with this header, aligned to a word
    boundary. After the note header is n_namesz bytes of name, padded to the
    next word boundary. Then n_descsz bytes of descriptor, again padded to a
    word boundary. The values of n_namesz and n_descsz to not include the
    padding.

Members:

    n_namesz - Stores the length of the name.

    n_descsz - Stores the length of the descriptor.

    n_type - Stores the note type.

--*/

typedef struct {
    uint32_t n_namesz;
    uint32_t n_descsz;
    uint32_t n_type;
} Elf_Note;

//
// 32-bit ELF definitions
//

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;
typedef uint64_t Elf32_Lword;

typedef Elf32_Word Elf32_Hashelt;

//
// Non-standard class-dependent data type used for abstraction
//

typedef Elf32_Word Elf32_Size;
typedef Elf32_Sword Elf32_Ssize;

/*++

Structure Description:

    This structure defines the ELF32 file header.

Members:

    e_ident - Stores the file identification.

    e_type - Stores the file type.

    e_machine - Stores the machine architecture.

    e_version - Stores the ELF format version.

    e_entry - Stores the entry point.

    e_phoff - Stores the program header file offset.

    e_shoff - Stores the section header file offset.

    e_flags - Stores architecture-specific flags.

    e_ehsize - Stores the size of the ELF header in bytes.

    e_phentsize - Stores the size of the program header entry.

    e_phnum - Stores the size of the program header entries.

    e_shentsize - Stores the size of a section header entry.

    e_shnum - Stores the count of section header entries.

    e_shstrndx - Stores the section name strings section.

--*/

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
} Elf32_Ehdr;

/*++

Structure Description:

    This structure defines the ELF32 section header.

Members:

    sh_name - Stores the index into the section header string table where the
        section name string can bd found.

    sh_type - Stores the section type.

    sh_flags - Stores the section flags.

    sh_addr - Stores the address in the memory image.

    sh_offset - Stores the file offset of the section.

    sh_size - Stores the size of the section in bytes.

    sh_link - Stores the index of a related section.

    sh_info - Stores a value that depends on the section type.

    sh_addralign - Stores the alignment in bytes.

    sh_entsize - Stores the size of each entry in the section.

--*/

typedef struct {
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off sh_offset;
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
} Elf32_Shdr;

/*++

Structure Description:

    This structure defines the ELF32 program header.

Members:

    p_type - Stores the program header type.

    p_offset - Stores the file offset of the contents.

    p_vaddr - Stores the virtual address in the memory image.

    p_paddr - Stores the physical address (not used).

    p_filesz - Stores the size of the contents within the file.

    p_memsz - Stores the size of the contents in memory.

    p_flags - Stores access permission flags.

    p_align - Stores the alignment in memory and in the file.

--*/

typedef struct {
    Elf32_Word p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} Elf32_Phdr;

/*++

Structure Description:

    This structure defines the ELF32 dynamic section.

Members:

    d_tag - Stores the entry type.

    d_un - Stores a union of the two different forms of the value.

        d_val - Stores the integer representation of the value.

        d_ptr - Stores the pointer representation of the value.

--*/

typedef struct {
    Elf32_Sword d_tag;
    union {
        Elf32_Word d_val;
        Elf32_Addr d_ptr;
    } d_un;

} Elf32_Dyn;

/*++

Structure Description:

    This structure defines the ELF32 relocation entry that does not require
    an addend.

Members:

    r_offset - Stores the location to be relocated.

    r_info - Stores the relocation type and symbol index.

--*/

typedef struct {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
} Elf32_Rel;

/*++

Structure Description:

    This structure defines the ELF32 relocation entry that needs an addend
    field.

Members:

    r_offset - Stores the location to be relocated.

    r_info - Stores the relocation type and symbol index.

    r_addend - Stores the addend to throw in there.

--*/

typedef struct {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
    Elf32_Sword r_addend;
} Elf32_Rela;

typedef Elf_Note Elf32_Nhdr;

/*++

Structure Description:

    This structure defines the ELF32 move entry.

Members:

    m_value - Stores the symbol value.

    m_info - Stores the size + index.

    m_poffset - Stores the symbol offset.

    m_repeat - Stores the repeat count.

    m_stride - Stores the stride information.

--*/

typedef struct {
    Elf32_Lword m_value;
    Elf32_Word m_info;
    Elf32_Word m_poffset;
    Elf32_Half m_repeat;
    Elf32_Half m_stride;
} Elf32_Move;

/*++

Structure Description:

    This structure defines the ELF32 hardware/software capabilities array.

Members:

    c_tag - Stores the tag which determines the interpretation of the value.

    c_un - Stores a union of the two different forms of the value.

        c_val - Stores the integer representation of the value.

        c_ptr - Stores the pointer representation of the value.

--*/

typedef struct {
    Elf32_Word c_tag;
    union {
        Elf32_Word c_val;
        Elf32_Addr c_ptr;
    } c_un;

} Elf32_Cap;

/*++

Structure Description:

    This structure defines an ELF32 symbol table entry.

Members:

    st_name - Stores the string table index of the name.

    st_value - Stores the value of the symbol.

    st_size - Stores the size of the associated objct.

    st_info - Stores the type and binding information.

    st_other - Stores a reserved value (not used).

    st_shndx - Stores the section index of the symbol.

--*/

typedef struct {
    Elf32_Word st_name;
    Elf32_Addr st_value;
    Elf32_Word st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Half st_shndx;
} Elf32_Sym;

//
// Structures used by Sun & GNU symbol versioning
//

typedef struct {
    Elf32_Half vd_version;
    Elf32_Half vd_flags;
    Elf32_Half vd_ndx;
    Elf32_Half vd_cnt;
    Elf32_Word vd_hash;
    Elf32_Word vd_aux;
    Elf32_Word vd_next;
} Elf32_Verdef;

typedef struct {
    Elf32_Word vda_name;
    Elf32_Word vda_next;
} Elf32_Verdaux;

typedef struct {
    Elf32_Half vn_version;
    Elf32_Half vn_cnt;
    Elf32_Word vn_file;
    Elf32_Word vn_aux;
    Elf32_Word vn_next;
} Elf32_Verneed;

typedef struct {
    Elf32_Word vna_hash;
    Elf32_Half vna_flags;
    Elf32_Half vna_other;
    Elf32_Word vna_name;
    Elf32_Word vna_next;
} Elf32_Vernaux;

typedef Elf32_Half Elf32_Versym;

typedef struct {
    Elf32_Half si_boundto;
    Elf32_Half si_flags;
} Elf32_Syminfo;

//
// 64-bit ELF definitions
//

typedef uint64_t Elf64_Addr;
typedef uint16_t Elf64_Half;
typedef uint64_t Elf64_Off;
typedef int32_t Elf64_Sword;
typedef int64_t Elf64_Sxword;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Lword;
typedef uint64_t Elf64_Xword;

//
// Define types of dynamic symbol hash table bucket and chain elements.
// This is inconsitent among 64-bit architectures, so a machine dependent
// typedef is required.
//

typedef Elf64_Word Elf64_Hashelt;

//
// Non-standard class-dependent data types used for abstraction.
//

typedef Elf64_Xword Elf64_Size;
typedef Elf64_Sxword Elf64_Ssize;

/*++

Structure Description:

    This structure defines the ELF64 file header.

Members:

    e_ident - Stores the file identification.

    e_type - Stores the file type.

    e_machine - Stores the machine architecture.

    e_version - Stores the ELF format version.

    e_entry - Stores the entry point.

    e_phoff - Stores the program header file offset.

    e_shoff - Stores the section header file offset.

    e_flags - Stores architecture-specific flags.

    e_ehsize - Stores the size of the ELF header in bytes.

    e_phentsize - Stores the size of the program header entry.

    e_phnum - Stores the size of the program header entries.

    e_shentsize - Stores the size of a section header entry.

    e_shnum - Stores the count of section header entries.

    e_shstrndx - Stores the section name strings section.

--*/

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off e_phoff;
    Elf64_Off e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize;
    Elf64_Half e_phnum;
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
} Elf64_Ehdr;

/*++

Structure Description:

    This structure defines the ELF64 section header.

Members:

    sh_name - Stores the index into the section header string table where the
        section name string can bd found.

    sh_type - Stores the section type.

    sh_flags - Stores the section flags.

    sh_addr - Stores the address in the memory image.

    sh_offset - Stores the file offset of the section.

    sh_size - Stores the size of the section in bytes.

    sh_link - Stores the index of a related section.

    sh_info - Stores a value that depends on the section type.

    sh_addralign - Stores the alignment in bytes.

    sh_entsize - Stores the size of each entry in the section.

--*/

typedef struct {
    Elf64_Word sh_name;
    Elf64_Word sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr sh_addr;
    Elf64_Off sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word sh_link;
    Elf64_Word sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} Elf64_Shdr;

/*++

Structure Description:

    This structure defines the ELF64 program header.

Members:

    p_type - Stores the program header type.

    p_flags - Stores access permission flags.

    p_offset - Stores the file offset of the contents.

    p_vaddr - Stores the virtual address in the memory image.

    p_paddr - Stores the physical address (not used).

    p_filesz - Stores the size of the contents within the file.

    p_memsz - Stores the size of the contents in memory.

    p_align - Stores the alignment in memory and in the file.

--*/

typedef struct {
    Elf64_Word p_type;
    Elf64_Word p_flags;
    Elf64_Off p_offset;
    Elf64_Addr p_vaddr;
    Elf64_Addr p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
} Elf64_Phdr;

/*++

Structure Description:

    This structure defines the ELF64 dynamic section.

Members:

    d_tag - Stores the entry type.

    d_un - Stores a union of the two different forms of the value.

        d_val - Stores the integer representation of the value.

        d_ptr - Stores the pointer representation of the value.

--*/

typedef struct {
    Elf64_Sxword d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr d_ptr;
    } d_un;

} Elf64_Dyn;

/*++

Structure Description:

    This structure defines the ELF64 relocation entry that does not require
    an addend.

Members:

    r_offset - Stores the location to be relocated.

    r_info - Stores the relocation type and symbol index.

--*/

typedef struct {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
} Elf64_Rel;

/*++

Structure Description:

    This structure defines the ELF64 relocation entry that needs an addend
    field.

Members:

    r_offset - Stores the location to be relocated.

    r_info - Stores the relocation type and symbol index.

    r_addend - Stores the addend to throw in there.

--*/

typedef struct {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

typedef Elf_Note Elf64_Nhdr;

/*++

Structure Description:

    This structure defines the ELF64 move entry.

Members:

    m_value - Stores the symbol value.

    m_info - Stores the size + index.

    m_poffset - Stores the symbol offset.

    m_repeat - Stores the repeat count.

    m_stride - Stores the stride information.

--*/

typedef struct {
    Elf64_Lword m_value;
    Elf64_Xword m_info;
    Elf64_Xword m_poffset;
    Elf64_Half m_repeat;
    Elf64_Half m_stride;
} Elf64_Move;

/*++

Structure Description:

    This structure defines the ELF64 hardware/software capabilities array.

Members:

    c_tag - Stores the tag which determines the interpretation of the value.

    c_un - Stores a union of the two different forms of the value.

        c_val - Stores the integer representation of the value.

        c_ptr - Stores the pointer representation of the value.

--*/

typedef struct {
    Elf64_Xword c_tag;
    union {
        Elf64_Xword c_val;
        Elf64_Addr c_ptr;
    } c_un;

} Elf64_Cap;

/*++

Structure Description:

    This structure defines an ELF64 symbol table entry.

Members:

    st_name - Stores the string table index of the name.

    st_value - Stores the value of the symbol.

    st_size - Stores the size of the associated objct.

    st_info - Stores the type and binding information.

    st_other - Stores a reserved value (not used).

    st_shndx - Stores the section index of the symbol.

--*/

typedef struct {
    Elf64_Word st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half st_shndx;
    Elf64_Addr st_value;
    Elf64_Xword st_size;
} Elf64_Sym;

//
// Define structures used by Sun & GNU-style symbol versioning.
//

typedef struct {
    Elf64_Half vd_version;
    Elf64_Half vd_flags;
    Elf64_Half vd_ndx;
    Elf64_Half vd_cnt;
    Elf64_Word vd_hash;
    Elf64_Word vd_aux;
    Elf64_Word vd_next;
} Elf64_Verdef;

typedef struct {
    Elf64_Word vda_name;
    Elf64_Word vda_next;
} Elf64_Verdaux;

typedef struct {
    Elf64_Half vn_version;
    Elf64_Half vn_cnt;
    Elf64_Word vn_file;
    Elf64_Word vn_aux;
    Elf64_Word vn_next;
} Elf64_Verneed;

typedef struct {
    Elf64_Word vna_hash;
    Elf64_Half vna_flags;
    Elf64_Half vna_other;
    Elf64_Word vna_name;
    Elf64_Word vna_next;
} Elf64_Vernaux;

typedef Elf64_Half Elf64_Versym;

typedef struct {
    Elf64_Half si_boundto;
    Elf64_Half si_flags;
} Elf64_Syminfo;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

