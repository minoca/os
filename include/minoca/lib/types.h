/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    types.h

Abstract:

    This header contains definitions for basic types.

Author:

    Evan Green 21-Jun-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// Define VOID rather than typedef it because C++ allows the void keyword in
// an empty function argument list, but not the void type.
//

#define VOID void

//
// Define type limits.
//

#define BITS_PER_BYTE     (8)

#ifdef __CHAR_UNSIGNED__

#define MIN_CHAR          0U
#define MAX_CHAR          (__SCHAR_MAX__ * 2U + 1U)

#else

#define MIN_CHAR          (-__SCHAR_MAX__ - 1)
#define MAX_CHAR          (__SCHAR_MAX__)

#endif

#define MAX_WCHAR         (__WCHAR_MAX__)
#define MIN_WCHAR         (-MAX_WCHAR - 1)
#define MAX_UCHAR         (0xFF)
#define MAX_USHORT        (0xFFFF)
#define MAX_SHORT         (0x7FFF)
#define MIN_SHORT         (0x8000)
#define MAX_LONG          ((LONG)0x7FFFFFFF)
#define MIN_LONG          ((LONG)0x80000000)
#define MAX_ULONG         ((ULONG)0xFFFFFFFF)
#define MAX_LONGLONG      (0x7FFFFFFFFFFFFFFFLL)
#define MIN_LONGLONG      (0x8000000000000000LL)
#define MAX_ULONGLONG     (0xFFFFFFFFFFFFFFFFULL)
#define DOUBLE_INFINITY   __builtin_inf()
#define DOUBLE_NAN        __builtin_nan("")
#define DOUBLE_HUGE_VALUE __builtin_huge_val()

#define NOTHING
#define ANYSIZE_ARRAY 1

#define INVALID_HANDLE ((HANDLE)-1)

#define _1KB 1024
#define _2KB (2 * _1KB)
#define _4KB (4 * _1KB)
#define _8KB (8 * _1KB)
#define _64KB (64 * _1KB)
#define _128KB (128 * _1KB)
#define _512KB (512 * _1KB)
#define _1MB (1024 * _1KB)
#define _2MB (2 * _1MB)
#define _1GB (1024 * _1MB)
#define _1TB (1024ULL * _1GB)

#define PACKED __attribute__((__packed__))
#define NO_RETURN __attribute__((__noreturn__))
#define __USED __attribute__((used))
#define __NOINLINE __attribute__((noinline))
#define ALIGNED(_Alignment) __attribute__((aligned(_Alignment)))
#define ALIGNED16 ALIGNED(16)
#define ALIGNED32 ALIGNED(32)
#define ALIGNED64 ALIGNED(64)

//
// Error out if the architecture is unknown.
//

#ifdef __WINNT__

#define __DLLIMPORT __declspec(dllimport)
#define __DLLEXPORT __declspec(dllexport)
#define __DLLPROTECTED __DLLEXPORT

#else

#define __DLLIMPORT __attribute__ ((visibility ("default")))
#define __DLLEXPORT __attribute__ ((visibility ("default")))

#ifdef __ELF__

#define __DLLPROTECTED __attribute__ ((visibility ("protected")))

#else

#define __DLLPROTECTED __DLLEXPORT

#endif
#endif

#if defined(__i386)

#define MAX_INTN    ((INTN)0x7FFFFFFF)
#define MAX_UINTN   ((UINTN)0xFFFFFFFF)

#elif defined(__amd64)

#define MAX_INTN    ((INTN)0x7FFFFFFFFFFFFFFF)
#define MAX_UINTN   ((UINTN)0xFFFFFFFFFFFFFFFF)

#elif defined(__arm__)

#define MAX_INTN    ((INTN)0x7FFFFFFF)
#define MAX_UINTN   ((UINTN)0xFFFFFFFF)

#else

#error No known architecture was defined.

#endif

#define MAX_ADDRESS (PVOID)MAX_UINTN

//
// ------------------------------------------------------ Data Type Definitions
//

typedef unsigned char BYTE, *PBYTE;
typedef unsigned short WORD, *PWORD;
typedef unsigned int DWORD, *PDWORD;

typedef char CHAR, *PCHAR;
typedef signed char SCHAR, *PSCHAR;
typedef unsigned char UCHAR, *PUCHAR;
typedef const unsigned char CUCHAR, *PCUCHAR;
typedef __WCHAR_TYPE__ WCHAR, *PWCHAR;
typedef short SHORT, *PSHORT;
typedef unsigned short USHORT, *PUSHORT;
typedef int INT, *PINT;
typedef unsigned int UINT, *PUINT;
typedef __WINT_TYPE__ WINT, *PWINT;
typedef int LONG, *PLONG;
typedef unsigned int ULONG, *PULONG;
typedef long long LONGLONG, *PLONGLONG;
typedef unsigned long long ULONGLONG, *PULONGLONG;

#if __SIZEOF_LONG__ != __SIZEOF_POINTER__
#error INTN and UINTN definitions are wrong.
#endif

typedef long INTN, *PINTN;
typedef unsigned long UINTN, *PUINTN;

typedef unsigned long long PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

typedef void *PVOID;
typedef const void *PCVOID;
typedef char *PSTR;
typedef const char *PCSTR;
typedef WCHAR *PWSTR;
typedef const WCHAR *PCWSTR;

typedef PVOID HANDLE, *PHANDLE;

typedef enum _BOOL {
    FALSE = 0,
    TRUE = 1
} BOOL, *PBOOL;

typedef struct _LIST_ENTRY LIST_ENTRY, *PLIST_ENTRY;
struct _LIST_ENTRY {
    PLIST_ENTRY Next;
    PLIST_ENTRY Previous;
};

typedef struct _UUID {
    ULONG Data[4];
} UUID, *PUUID;

#if !defined(NULL)

#define NULL ((PVOID)0)

#endif

/*++

Structure Description:

    This structure defines a spin lock.

Members:

    LockHeld - Stores TRUE if the lock is acquired by someone, or FALSE if the
        lock is free.

    OwningThread - Stores a pointer to the KTHREAD that holds the lock if the
        lock is held.

--*/

typedef struct _KSPIN_LOCK {
    volatile ULONG LockHeld;
    volatile PVOID OwningThread;
} KSPIN_LOCK, *PKSPIN_LOCK;

//
// --------------------------------------------------------------------- Macros
//

//
// This macro initializes a linked list by pointing the next and previous links
// to itself. _Head is a PLIST_ENTRY.
//

#define INITIALIZE_LIST_HEAD(_Head) \
    (_Head)->Next = (_Head);        \
    (_Head)->Previous = (_Head);

//
// This macro inserts an entry (_New) into an existing linked list after
// _Existing. Both parameters are of type PLIST_ENTRY.
//

#define INSERT_AFTER(_New, _Existing)     \
    (_New)->Next = (_Existing)->Next;     \
    (_New)->Previous = (_Existing);       \
    (_Existing)->Next->Previous = (_New); \
    (_Existing)->Next = (_New);

//
// This macro inserts an entry (_New) into an existing linked list before an
// existing element (_Existing). Both parameters are of type PLIST_ENTRY.
//

#define INSERT_BEFORE(_New, _Existing)        \
    (_New)->Next = (_Existing);               \
    (_New)->Previous = (_Existing)->Previous; \
    (_Existing)->Previous->Next = (_New);     \
    (_Existing)->Previous = (_New);

//
// This macro removes a value (_ListEntry) from a linked list. The parameter is
// of type PLIST_ENTRY.
//

#define LIST_REMOVE(_ListEntry)                            \
    (_ListEntry)->Next->Previous = (_ListEntry)->Previous; \
    (_ListEntry)->Previous->Next = (_ListEntry)->Next;

//
// This macro moves the contents of one list to another. The source list
// must not be empty. This macro leaves the source list head trashed (it does
// not modify it), so the source list head must be reinitialized before it can
// be used again.
//

#define MOVE_LIST(_SourceListHead, _DestinationListHead)                \
    (_DestinationListHead)->Next = (_SourceListHead)->Next;             \
    (_DestinationListHead)->Previous = (_SourceListHead)->Previous;     \
    (_DestinationListHead)->Next->Previous = (_DestinationListHead);    \
    (_DestinationListHead)->Previous->Next = (_DestinationListHead);

//
// This macro appends a list to another. This append list must not be empty.
// This macro leaves the append list head trashed (it does not modify it), so
// the append list head must be reinitialized before it can be used again.
//

#define APPEND_LIST(_AppendListHead, _ExistingListHead)                 \
    (_AppendListHead)->Previous->Next = (_ExistingListHead);            \
    (_AppendListHead)->Next->Previous = (_ExistingListHead)->Previous;  \
    (_ExistingListHead)->Previous->Next = (_AppendListHead)->Next;      \
    (_ExistingListHead)->Previous = (_AppendListHead)->Previous;

//
// This macro returns nonzero if the given list head is an empty list. The
// parameter is of type PLIST_ENTRY.
//

#define LIST_EMPTY(_ListHead) \
    ((_ListHead)->Next == (_ListHead))

//
// This macro retrieves the data structure associated with a particular list
// entry. _ListEntry is a PLIST_ENTRY that points to the list entry. _Type is
// the type of the containing record. _MemberName is the name of the LIST_ENTRY
// in the containing record.
//

#define LIST_VALUE(_ListEntry, _Type, _MemberName) \
    PARENT_STRUCTURE(_ListEntry, _Type, _MemberName)

//
// This macro retrieves the parent structure given a pointer to one of its
// field members. _MemberPointer is a pointer to a field within the structure.
// _ParentType is the type of the structure. _MemberName is the name of the
// member within the structure
//

#define PARENT_STRUCTURE(_MemberPointer, _ParentType, _MemberName) \
    (_ParentType *)((PVOID)(_MemberPointer) -                      \
                    ((PVOID)(&(((_ParentType *)0)->_MemberName))))

//
// The ALIGN_RANGE_DOWN macro aligns the given Value to the granularity of
// Size, truncating any remainder. This macro is only valid for Sizes that are
// powers of two.
//

#define ALIGN_RANGE_DOWN(_Value, _Size) \
    ((_Value) & ~((_Size) - 1LL))

//
// The ALIGN_RANGE_UP macro aligns the given Value to the granularity of Size,
// rounding up to a Size boundary if there is any remainder. This macro is only
// valid for Sizes that are a power of two.
//

#define ALIGN_RANGE_UP(_Value, _Size) \
    ALIGN_RANGE_DOWN((_Value) + (_Size) - 1LL, (_Size))

//
// The IS_ALIGNED macro returns a non-zero value if the given value is aligned
// to the given size. It returns zero otherwise.
//

#define IS_ALIGNED(_Value, _Size) \
    (ALIGN_RANGE_DOWN((_Value), (_Size)) == (_Value))

//
// The REMAINDER macro returns the remainder of the Value when aligned down
// to the granularity of the given Size.
//

#define REMAINDER(_Value, _Size) \
    ((_Value) & ((_Size) - 1LL))

//
// The ALIGN_POINTER_DOWN macro aligns the given Pointer to the granularity of
// Size, truncating any remainder. This macro is only valid for Sizes that are
// powers of two.
//

#define ALIGN_POINTER_DOWN(_Pointer, _Size) \
    (PVOID)(UINTN)((UINTN)(_Pointer) & ~((_Size) - 1LL))

//
// The ALIGN_POINTER_UP macro aligns the given Pointer to the granularity of
// Size, rounding up to a Size boundary if there is any remainder. This macro
// is only valid for Sizes that are a power of two.
//

#define ALIGN_POINTER_UP(_Pointer, _Size) \
    ALIGN_POINTER_DOWN((PVOID)(_Pointer) + (_Size) - 1LL, (_Size))

//
// The IS_POINTER_ALIGNED macro returns a non-zero value if the given pointer
// is aligned to the given size. It returns zero otherwise.
//

#define IS_POINTER_ALIGNED(_Pointer, _Size) \
    (ALIGN_POINTER_DOWN((_Pointer), (_Size)) == (_Pointer))

//
// The POWER_OF_2 macro returns a non-zero value if the given value is a power
// of 2. 0 will qualify as a power of 2 with this macro.
//

#define POWER_OF_2(_Value) (((_Value) & ((_Value) - 1LL)) == 0)

