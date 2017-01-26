/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    queue.h

Abstract:

    This header contains definitions and macros for various queue structures.

Author:

    Chris Stevens 25-Jan-2017

--*/

#ifndef _QUEUE_H
#define _QUEUE_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// --------------------------------------------------------------------- Macros
//

#ifdef __cplusplus

extern "C" {

#endif

//
// This macros declares a singly-linked list head structure.
//

#define SLIST_HEAD(_HeadName, _Type) \
    struct _HeadName {               \
        struct _Type *slh_first;     \
    }

//
// This macro evaluates to an initializer for a singly-linked list head.
//

#define SLIST_HEAD_INITIALIZER(_Head) { NULL }

//
// This macro determines whether or not a singly-linked list is empty.
//

#define SLIST_EMPTY(_Head) ((_Head)->slh_first == NULL)

//
// This macros declares a single-linked list element structure.
//

#define SLIST_ENTRY(_Type)      \
    struct {                    \
        struct _Type *sle_next; \
    }

//
// This macro returns the first element of a singly-linked list.
//

#define SLIST_FIRST(_Head) ((_Head)->slh_first)

//
// This macro iterates over the singly-linked list given by _Head, assigning
// each element to _Variable.
//

#define SLIST_FOREACH(_Variable, _Head, _MemberName)      \
    for ((_Variable) = (_Head)->slh_first;                \
         (_Variable) != NULL;                             \
         (_Variable) = (_Variable)->_MemberName.sle_next)

//
// This macro initializes the head of a singly-linked list.
//

#define SLIST_INIT(_Head) ((_Head)->slh_first = NULL)

//
// This macro inserts the new element at the singly-linked list head.
//

#define SLIST_INSERT_HEAD(_Head, _New, _MemberName)        \
    do {                                                   \
        (_New)->_MemberName.sle_next = (_Head)->slh_first; \
        (_Head)->slh_first = (_New);                       \
                                                           \
    } while (0)

//
// This macro inserts the new element after the existing singly-linked list
// element.
//

#define SLIST_INSERT_AFTER(_Existing, _New, _MemberName)                  \
    do {                                                                  \
        (_New)->_MemberName.sle_next = (_Existing)->_MemberName.sle_next; \
        (_Existing)->_MemberName.sle_next = (_New);                       \
                                                                          \
    } while (0)

//
// This macro returns the next element in a singly-linked list.
//

#define SLIST_NEXT(_ListEntry, _MemberName) ((_ListEntry)->_MemberName.sle_next)

//
// This macro removes the element at the head of a singly-linked list.
//

#define SLIST_REMOVE_HEAD(_Head, _MemberName) \
    (_Head)->slh_first = (_Head)->slh_first->_MemberName.sle_next;

//
// This macro removes an arbitrary element in a singly-linked list.
//

#define SLIST_REMOVE(_Head, _ListEntry, _Type, _MemberName)                   \
    do {                                                                      \
        struct _Type *_CurrentEntry = (_Head)->slh_first;                     \
        if ((_CurrentEntry) == (_ListEntry)) {                                \
            (_Head)->slh_first = (_ListEntry)->_MemberName.sle_next;          \
                                                                              \
        } else {                                                              \
            while ((_CurrentEntry)->_MemberName.sle_next != (_ListEntry)) {   \
                (_CurrentEntry) = (_CurrentEntry)->_MemberName.sle_next;      \
            }                                                                 \
                                                                              \
            (_CurrentEntry)->_MemberName.sle_next =                           \
                                          (_ListEntry)->_MemberName.sle_next; \
        }                                                                     \
                                                                              \
    } while (0)

//
// This macro declares a singly-linked tail queue head structure. The last
// entry actually stores the address of the last element's next pointer.
//

#define STAILQ_HEAD(_HeadName, _Type) \
    struct _HeadName {                \
        struct _Type *stqh_first;     \
        struct _Type **stqh_last;     \
    }

//
// This macro evaluates to an initializer for a singly-linked tail queue head.
//

#define STAILQ_HEAD_INITIALIZER(_Head) { NULL, &((_Head).stqh_first) }

//
// This macro concatenates the tail queue specified by _Head2 onto the end of
// the tail queue specified by _Head1. _Head2 should be empty at the end of the
// macro.
//

#define STAILQ_CONCAT(_Head1, _Head2)                                         \
    do {                                                                      \
        if ((_Head2)->stqh_first != NULL) {                                   \
            *((_Head1)->stqh_last) = (_Head2)->stqh_first;                    \
            (_Head1)->stqh_last = (_Head2)->stqh_last;                        \
            STAILQ_INIT(_Head2);                                              \
        }                                                                     \
                                                                              \
    } while (0)

//
// This macro determines whether or not a tail queue is empty.
//

#define STAILQ_EMPTY(_Head) ((_Head)->stqh_first == NULL)

//
// This macros declares a tail queue element structure.
//

#define STAILQ_ENTRY(_Type) \
    struct {                \
        struct _Type *stqe_next; \
    }

//
// This macro returns the first element of a singly-linked tail queue.
//

#define STAILQ_FIRST(_Head) ((_Head)->stqh_first)

//
// This macro iterates over the singly-linked tail queue given by _Head,
// assigning each element to _Variable.
//

#define STAILQ_FOREACH(_Variable, _Head, _MemberName)      \
    for ((_Variable) = (_Head)->stqh_first;                \
         (_Variable) != NULL;                              \
         (_Variable) = (_Variable)->_MemberName.stqe_next)

//
// This macro initializes the head of a singly-linked tail queue.
//

#define STAILQ_INIT(_Head)                           \
    do {                                             \
        (_Head)->stqh_first = NULL;                  \
        (_Head)->stqh_last = &((_Head)->stqh_first); \
    } while (0)

//
// This macro inserts the _New tail queue element after the _Existing tail
// queue element.
//

#define STAILQ_INSERT_AFTER(_Head, _Existing, _New, _MemberName)            \
    do {                                                                    \
        (_New)->_MemberName.stqe_next = (_Existing)->_MemberName.stqe_next; \
        (_Existing)->_MemberName.stqe_next = (_New);                        \
        if ((_New)->_MemberName.stqe_next == NULL) {                        \
            (_Head)->stqh_last = &((_New)->_MemberName.stqe_next);          \
        }                                                                   \
                                                                            \
    } while (0)

//
// This macro inserts the _Entry tail queue element at the head of the
// singly-linked tail queue.
//

#define STAILQ_INSERT_HEAD(_Head, _New, _MemberName)               \
    do {                                                           \
        (_New)->_MemberName.stqe_next = (_Head)->stqh_first;       \
        (_Head)->stqh_first = (_New);                              \
        if ((_New)->_MemberName.stqe_next == NULL) {               \
            (_Head)->stqh_last = &((_New)->_MemberName.stqe_next); \
        }                                                          \
                                                                   \
    } while (0)

//
// This macro inserts the _Entry tail queue element at the end of the
// singly-ilnked tail queue.
//

#define STAILQ_INSERT_TAIL(_Head, _New, _MemberName)           \
    do {                                                       \
        (_New)->_MemberName.stqe_next = NULL;                  \
        *((_Head)->stqh_last) = (_New);                        \
        (_Head)->stqh_last = &((_New)->_MemberName.stqe_next); \
                                                               \
    } while (0)

//
// This macro returns the next element in a singly-linked tail queue.
//

#define STAILQ_NEXT(_Entry, _MemberName) ((_Entry)->_MemberName.stqe_next)

//
// This macro removes the element at the head of a singly-linked tail queue.
//

#define STAILQ_REMOVE_HEAD(_Head, _MemberName)                            \
    do {                                                                  \
        (_Head)->stqh_first = (_Head)->stqh_first->_MemberName.stqe_next; \
        if ((_Head)->stqh_first == NULL) {                                \
            (_Head)->stqh_last = &((_Head)->stqh_first);                  \
        }                                                                 \
                                                                          \
    } while (0)

//
// This macro removes the given _Entry tail queue element from the
// singly-linked tail queue.
//

#define STAILQ_REMOVE(_Head, _Entry, _Type, _MemberName)                      \
    do {                                                                      \
        struct _Type *_CurrentEntry = (_Head)->stqh_first;                    \
        if ((_CurrentEntry) == (_Entry)) {                                    \
            STAILQ_REMOVE_HEAD(_Head, _MemberName);                           \
                                                                              \
        } else {                                                              \
            while ((_CurrentEntry)->_MemberName.stqe_next != (_Entry)) {      \
                (_CurrentEntry) = (_CurrentEntry)->_MemberName.stqe_next;     \
            }                                                                 \
                                                                              \
            (_CurrentEntry)->_MemberName.stqe_next =                          \
                                             (_Entry)->_MemberName.stqe_next; \
                                                                              \
            if ((_Entry)->_MemberName.stqe_next == NULL) {                    \
                (_Head)->stqh_last =                                          \
                                   &((_CurrentEntry)->_MemberName.stqe_next); \
            }                                                                 \
        }                                                                     \
                                                                              \
    } while (0)

//
// This macro declares a doubly-linked queue head structure.
//

#define LIST_HEAD(_HeadName, _Type) \
    struct _HeadName {              \
        struct _Type *lh_first;     \
    }

//
// This macro evaluates to an initializer for a doubly-linked list head.
//

#define LIST_HEAD_INITIALIZER(_Head) { NULL }

//
// This macro determines whether or not a doubly linked list is empty.
//

#define LIST_EMPTY(_Head) ((_Head)->lh_first == NULL)

//
// This macro declares a doubly-linked list element structure. In order to
// perform INSERT_BEFORE without being passed the head of the list, the
// previous pointer actually needs to store the address of the previous
// element's next pointer. In the case of the head, this is the address of the
// first entry pointer.
//

#define LIST_ENTRY(_Type)       \
    struct {                    \
        struct _Type *le_next;  \
        struct _Type **le_prev; \
    }

//
// This macro returns the first element of a doubly-linked list.
//

#define LIST_FIRST(_Head) ((_Head)->lh_first)

//
// This macro iterates over the doubly-linked list given by _Head, assigning
// each element to _Variable.
//

#define LIST_FOREACH(_Variable, _Head, _MemberName)      \
    for ((_Variable) = (_Head)->lh_first;                \
         (_Variable) != NULL;                            \
         (_Variable) = (_Variable)->_MemberName.le_next)

//
// This macro initializes the head of a doubly-linked list.
//

#define LIST_INIT(_Head) ((_Head)->lh_first = NULL)

//
// This macro inserts the _New doubly-linked list entry after the _Existing
// entry.
//

#define LIST_INSERT_AFTER(_Existing, _New, _MemberName)                       \
    do {                                                                      \
        (_New)->_MemberName.le_next = (_Existing)->_MemberName.le_next;       \
        (_New)->_MemberName.le_prev = &((_Existing)->_MemberName.le_next);    \
        if ((_Existing)->_MemberName.le_next != NULL) {                       \
            (_Existing)->_MemberName.le_next->_MemberName.le_prev =           \
                                              &((_New)->_MemberName.le_next); \
        }                                                                     \
                                                                              \
        (_Existing)->_MemberName.le_next = (_New);                            \
                                                                              \
    } while (0)

//
// This macro inserts the _New doubly-linked list entry before the _Existing
// entry.
//

#define LIST_INSERT_BEFORE(_Existing, _New, _MemberName)                   \
    do {                                                                   \
        (_New)->_MemberName.le_next = (_Existing);                         \
        (_New)->_MemberName.le_prev = (_Existing)->_MemberName.le_prev;    \
        *((_Existing)->_MemberName.le_prev) = (_New);                      \
        (_Existing)->_MemberName.le_prev = &((_New)->_MemberName.le_next); \
                                                                           \
    } while (0)

//
// This macro inserts the _New doubly-linked list entry at the head of the list.
//

#define LIST_INSERT_HEAD(_Head, _New, _MemberName)                            \
    do {                                                                      \
        (_New)->_MemberName.le_next = (_Head)->lh_first;                      \
        (_New)->_MemberName.le_prev = &((_Head)->lh_first);                   \
        if ((_Head)->lh_first != NULL) {                                      \
            (_Head)->lh_first->_MemberName.le_prev =                          \
                                              &((_New)->_MemberName.le_next); \
        }                                                                     \
                                                                              \
        (_Head)->lh_first = (_New);                                           \
                                                                              \
    } while (0)

//
// This macro gets the next element in the doubly-linked list.
//

#define LIST_NEXT(_Entry, _MemberName) ((_Entry)->_MemberName.le_next)

//
// This macro removes an entry from a doubly-linked list.
//

#define LIST_REMOVE(_Entry, _MemberName)                                      \
    do {                                                                      \
        if ((_Entry)->_MemberName.le_next != NULL) {                          \
            (_Entry)->_MemberName.le_next->_MemberName.le_prev =              \
                                               (_Entry)->_MemberName.le_prev; \
        }                                                                     \
                                                                              \
        *((_Entry)->_MemberName.le_prev) = (_Entry)->_MemberName.le_next;     \
                                                                              \
    } while (0)

//
// This macro swaps the doubly-linked list from _Head1 with the doubly-linked
// list of _Head2.
//

#define LIST_SWAP(_Head1, _Head2, _Type, _MemberName)                        \
    do {                                                                     \
        struct _Type *_SwapEntry = (_Head1)->lh_first;                       \
        (_Head1)->lh_first = (_Head2)->lh_first;                             \
        (_Head2)->lh_first = (_SwapEntry);                                   \
        if ((_Head1)->lh_first != NULL) {                                    \
            (_Head1)->lh_first->_MemberName.le_prev = &((_Head1)->lh_first); \
        }                                                                    \
                                                                             \
        if ((_Head2)->lh_first != NULL) {                                    \
            (_Head2)->lh_first->_MemberName.le_prev = &((_Head2)->lh_first); \
        }                                                                    \
                                                                             \
    } while (0)

//
// This macro defines the structure for a doubly-linked tail queue head. The
// LastEntry is actually a pointer to the last entry's next field. It makes
// this messy and hard to follow, but the macro definitions force this
// implementation.
//

#define TAILQ_HEAD(_HeadName, _Type) \
    struct _HeadName {               \
        struct _Type *tqh_first;     \
        struct _Type **tqh_last;     \
    }

//
// This macro evaluates to an initializer for a doubly-linked tail queue.
//

#define TAILQ_HEAD_INITIALIZER(_Head) { NULL, &((_Head).tqh_first) }

//
// This macro concatenates all the entrys from the _Head2 tail queue onto the
// _Head1 tail queue. _Head2 should be empty at the end of the macro.
//

#define TAILQ_CONCAT(_Head1, _Head2, _MemberName)                           \
    do {                                                                    \
        if ((_Head2)->tqh_first != NULL) {                                  \
            *((_Head1)->tqh_last) = (_Head2)->tqh_first;                    \
            (_Head2)->tqh_first->_MemberName.tqe_prev = (_Head1)->tqh_last; \
            (_Head1)->tqh_last = (_Head2)->tqh_last;                        \
            TAILQ_INIT(_Head2);                                             \
        }                                                                   \
                                                                            \
    } while (0)

//
// This macro determins whether or not a doubly-linked tail queue is empty.
//

#define TAILQ_EMPTY(_Head) ((_Head)->tqh_first == NULL)

//
// This macro defines the structure for a doubly-linked tail queue entry.
//

#define TAILQ_ENTRY(_Type)       \
    struct {                     \
        struct _Type *tqe_next;  \
        struct _Type **tqe_prev; \
    }

//
// This macro returns the first entry in the doubly-linked tail queue.
//

#define TAILQ_FIRST(_Head) ((_Head)->tqh_first)

//
// This macro iterates over the doubly-linked tail queue given by _Head,
// assigning each element to _Variable.
//

#define TAILQ_FOREACH(_Variable, _Head, _MemberName)       \
    for ((_Variable) = TAILQ_FIRST(_Head);                 \
         (_Variable) != NULL;                              \
         (_Variable) = TAILQ_NEXT(_Variable, _MemberName))

//
// This macro iterates over the doubly-linked tail queue given by _Head,
// assigning each element to _Variable. It does so in reverse order.
//

#define TAILQ_FOREACH_REVERSE(_Variable, _Head, _HeadName, _MemberName) \
    for ((_Variable) = TAILQ_LAST(_Head, _HeadName);                    \
         (_Variable) != NULL;                                           \
         (_Variable) = TAILQ_PREV(_Variable, _HeadName, _MemberName))   \

//
// This macro initializes a doubly-linked tail queue head structure.
//

#define TAILQ_INIT(_Head)                          \
    do {                                           \
        (_Head)->tqh_first = NULL;                 \
        (_Head)->tqh_last = &((_Head)->tqh_first); \
                                                   \
    } while (0)

//
// This macro inserts a _New tail queue entry after the _Existing entry.
//

#define TAILQ_INSERT_AFTER(_Head, _Existing, _New, _MemberName)               \
    do {                                                                      \
        (_New)->_MemberName.tqe_next = (_Existing)->_MemberName.tqe_next;     \
        (_New)->_MemberName.tqe_prev = &((_Existing)->_MemberName.tqe_next);  \
        if ((_Existing)->_MemberName.tqe_next != NULL) {                      \
            (_Existing)->_MemberName.tqe_next->_MemberName.tqe_prev =         \
                                             &((_New)->_MemberName.tqe_next); \
                                                                              \
        } else {                                                              \
            (_Head)->tqh_last = &((_New)->_MemberName.tqe_next);              \
        }                                                                     \
                                                                              \
        (_Existing)->_MemberName.tqe_next = (_New);                           \
                                                                              \
    } while (0)

//
// This macro inserts a _New tail queue entry before the _Existing entry.
//

#define TAILQ_INSERT_BEFORE(_Existing, _New, _MemberName)                    \
    do {                                                                     \
        (_New)->_MemberName.tqe_next = (_Existing);                          \
        (_New)->_MemberName.tqe_prev = (_Existing)->_MemberName.tqe_prev;    \
        *((_Existing)->_MemberName.tqe_prev) = (_New);                       \
        (_Existing)->_MemberName.tqe_prev = &((_New)->_MemberName.tqe_next); \
                                                                             \
    } while (0)

//
// This macro inserts a _New tail queue entry at the head of the tail queue.
//

#define TAILQ_INSERT_HEAD(_Head, _New, _MemberName)                           \
    do {                                                                      \
        (_New)->_MemberName.tqe_next = (_Head)->tqh_first;                    \
        (_New)->_MemberName.tqe_prev = &((_Head)->tqh_first);                 \
        if ((_Head)->tqh_first != NULL) {                                     \
            (_Head)->tqh_first->_MemberName.tqe_prev =                        \
                                             &((_New)->_MemberName.tqe_next); \
                                                                              \
        } else {                                                              \
            (_Head)->tqh_last = &((_New)->_MemberName.tqe_next);              \
        }                                                                     \
                                                                              \
        (_Head)->tqh_first = (_New);                                          \
                                                                              \
    } while (0)

//
// This macro inserts a _New tail queue entry at the end of the tail queue.
//

#define TAILQ_INSERT_TAIL(_Head, _New, _MemberName)          \
    do {                                                     \
        (_New)->_MemberName.tqe_next = NULL;                 \
        (_New)->_MemberName.tqe_prev = (_Head)->tqh_last;    \
        *((_Head)->tqh_last) = (_New);                       \
        (_Head)->tqh_last = &((_New)->_MemberName.tqe_next); \
                                                             \
    } while (0)

//
// This macro returns the last entry in the doubly-linked tail queue. This is
// gnarly, but forced by the way these macros are defined. What is this doing?
// The head's last entry actually stores the address of the last element's next
// pointer. So it points to the nameless structure that makes up a tail queue
// entry. As it's nameless, its pointers cannot be accessed. Both the _Type and
// _MemberName would be needed to get the base pointer for the _Type structure
// from just this previous next address. What then? Cast it to a _HeadName;
// it's got the same pointer structure! Yikes. The second pointer of the entry
// is actually the address of the previous entry's next field. Dereferencing
// that yields the address of the entry after the previous entry - that is, the
// last entry.
//

#define TAILQ_LAST(_Head, _HeadName) \
    (*(((struct _HeadName *)((_Head)->tqh_last))->tqh_last))

//
// This macro returns the next entry in the tail queue.
//

#define TAILQ_NEXT(_Entry, _MemberName) ((_Entry)->_MemberName.tqe_next)

//
// This macro returns the previous entry in the tail queue. See the comments
// about TAILQ_LAST to get a sense of what's going on here.
//

#define TAILQ_PREV(_Entry, _HeadName, _MemberName) \
    (*(((struct _HeadName *)((_Entry)->_MemberName.tqe_prev))->tqh_last))

//
// This macro removes the given doubly-linked tail queue entry from the tail
// queue.
//

#define TAILQ_REMOVE(_Head, _Entry, _MemberName)                              \
    do {                                                                      \
        if ((_Entry)->_MemberName.tqe_next == NULL) {                         \
            (_Head)->tqh_last = (_Entry)->_MemberName.tqe_prev;               \
                                                                              \
        } else {                                                              \
            (_Entry)->_MemberName.tqe_next->_MemberName.tqe_prev =            \
                                              (_Entry)->_MemberName.tqe_prev; \
        }                                                                     \
                                                                              \
        *((_Entry)->_MemberName.tqe_prev) = (_Entry)->_MemberName.tqe_next;   \
                                                                              \
    } while (0)

//
// This macro swaps the tail queue in _Head1 with the tail queue in _Head2;
//

#define TAILQ_SWAP(_Head1, _Head2, _Type, _MemberName)                        \
    do {                                                                      \
        struct _Type *_SwapFirst = (_Head1)->tqh_first;                        \
        struct _Type **_SwapLast = (_Head1)->tqh_last;                        \
        (_Head1)->tqh_first = (_Head2)->tqh_first;                            \
        (_Head1)->tqh_last = (_Head2)->tqh_last;                              \
        (_Head2)->tqh_first = (_SwapFirst);                                   \
        (_Head2)->tqh_last = (_SwapLast);                                     \
        if ((_Head1)->tqh_first != NULL) {                                    \
            (_Head1)->tqh_first->_MemberName.tqe_prev =                       \
                                                      &((_Head1)->tqh_first); \
                                                                              \
        } else {                                                              \
            (_Head1)->tqh_last = &((_Head1)->tqh_first);                      \
        }                                                                     \
                                                                              \
        if ((_Head2)->tqh_first != NULL) {                                    \
            (_Head2)->tqh_first->_MemberName.tqe_prev =                       \
                                                      &((_Head2)->tqh_first); \
                                                                              \
        } else {                                                              \
            (_Head2)->tqh_last = &((_Head2)->tqh_first);                      \
        }                                                                     \
                                                                              \
    } while (0)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#ifdef __cplusplus

}

#endif
#endif

