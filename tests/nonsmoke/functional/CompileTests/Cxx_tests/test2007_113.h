typedef unsigned char JSUint8;
typedef signed char JSInt8;
typedef unsigned short JSUint16;
typedef short JSInt16;
typedef unsigned int JSUint32;
typedef int JSInt32;
typedef long long JSInt64;
typedef unsigned long long JSUint64;
typedef int JSIntn;
typedef unsigned int JSUintn;
typedef double JSFloat64;
typedef unsigned long JSUptrdiff;
typedef JSIntn JSBool;
typedef JSUint8 JSPackedBool;
typedef long JSWord;
typedef unsigned long JSUword;
typedef JSUintn uintn;
typedef JSUint64 uint64;
typedef JSUint32 uint32;
typedef JSUint16 uint16;
typedef JSUint8 uint8;
typedef JSIntn intn;
typedef JSInt64 int64;
typedef JSInt32 int32;
typedef JSInt16 int16;
typedef JSFloat64 float64;
typedef JSUintn uintN;
typedef struct JSContext JSContext;
typedef struct JSObjectMap JSObjectMap;
typedef JSBool
(* JSContextCallback)(JSContext *cx, uintN contextOp);

/* Use standard headers instead of a copied glibc-internal stdio fragment. */
#include <stddef.h>
#include <stdio.h>
#define PR_VISIBILITY_DEFAULT __attribute__((visibility("default")))
#define PR_IMPORT(__type) extern PR_VISIBILITY_DEFAULT __type

#define NSPR_API(__type) PR_IMPORT(__type)
#define NSPR_DATA_API(__type) PR_IMPORT_DATA(__type)

#define PR_BEGIN_MACRO  do {
#define PR_END_MACRO    } while (0)

#define PR_BEGIN_EXTERN_C       extern "C" {
#define PR_END_EXTERN_C         }
PR_BEGIN_EXTERN_C

typedef unsigned char PRUint8;
typedef signed char PRInt8;
typedef unsigned short PRUint16;
typedef short PRInt16;

typedef unsigned int PRUint32;
typedef int PRInt32;
typedef long long PRInt64;
typedef unsigned long long PRUint64;

typedef int PRIntn;
typedef unsigned int PRUintn;
typedef double          PRFloat64;
typedef size_t PRSize;
typedef PRInt32 PROffset32;
typedef PRInt64 PROffset64;
typedef ptrdiff_t PRPtrdiff;
typedef unsigned long PRUptrdiff;

typedef PRIntn PRBool;
#define PR_TRUE 1
#define PR_FALSE 0
typedef PRUint8 PRPackedBool;

typedef enum { PR_FAILURE = -1, PR_SUCCESS = 0 } PRStatus;

typedef PRUint16 PRUnichar;
typedef long PRWord;
typedef unsigned long PRUword;

#define PR_PUBLIC_API		PR_IMPLEMENT


#if !defined(PROTYPES_H)
#define PROTYPES_H

typedef PRUintn uintn;
#ifndef _XP_Core_
typedef PRIntn intn;
#endif

#include <sys/types.h>

/*
 * uint
 */

#if !defined(XP_BEOS) && !defined(XP_UNIX) || defined(NTO)
typedef PRUintn uint;
#endif

/*
 * uint64
 */

#if !defined(XP_BEOS)
typedef PRUint64 uint64;
#endif

/*
 * uint32
 */

#if !defined(XP_BEOS)
#if !defined(XP_OS2) && !defined(NTO)
typedef PRUint32 uint32;
#else
typedef unsigned long uint32;
#endif
#endif

/*
 * uint16
 */

#if !defined(XP_BEOS)
typedef PRUint16 uint16;
#endif

/*
 * uint8
 */

#if !defined(XP_BEOS)
typedef PRUint8 uint8;
#endif

/*
 * int64
 */

#if !defined(XP_BEOS) && !defined(_PR_AIX_HAVE_BSD_INT_TYPES)
typedef PRInt64 int64;
#endif

/*
 * int32
 */

#if !defined(XP_BEOS) && !defined(_PR_AIX_HAVE_BSD_INT_TYPES) && !defined(HPUX)
#if !defined(XP_OS2) && !defined(NTO)
typedef PRInt32 int32;
#else
typedef long int32;
#endif
#endif

/*
 * int16
 */

#if !defined(XP_BEOS) && !defined(_PR_AIX_HAVE_BSD_INT_TYPES) && !defined(HPUX)
typedef PRInt16 int16;
#endif

/*
 * int8
 */

#if !defined(XP_BEOS) && !defined(_PR_AIX_HAVE_BSD_INT_TYPES) && !defined(HPUX)
typedef PRInt8 int8;
#endif

typedef PRFloat64 float64;
typedef PRUptrdiff uptrdiff_t;
typedef PRUword uprword_t;
typedef PRWord prword_t;


/* Re: prbit.h */
#define TEST_BIT	PR_TEST_BIT
#define SET_BIT		PR_SET_BIT
#define CLEAR_BIT	PR_CLEAR_BIT

/* Re: prarena.h->plarena.h */
#define PRArena PLArena
#define PRArenaPool PLArenaPool
#define PRArenaStats PLArenaStats
#define PR_ARENA_ALIGN PL_ARENA_ALIGN
#define PR_INIT_ARENA_POOL PL_INIT_ARENA_POOL
#define PR_ARENA_ALLOCATE PL_ARENA_ALLOCATE
#define PR_ARENA_GROW PL_ARENA_GROW
#define PR_ARENA_MARK PL_ARENA_MARK
#define PR_CLEAR_UNUSED PL_CLEAR_UNUSED
#define PR_CLEAR_ARENA PL_CLEAR_ARENA
#define PR_ARENA_RELEASE PL_ARENA_RELEASE
#define PR_COUNT_ARENA PL_COUNT_ARENA
#define PR_ARENA_DESTROY PL_ARENA_DESTROY
#define PR_InitArenaPool PL_InitArenaPool
#define PR_FreeArenaPool PL_FreeArenaPool
#define PR_FinishArenaPool PL_FinishArenaPool
#define PR_CompactArenaPool PL_CompactArenaPool
#define PR_ArenaFinish PL_ArenaFinish
#define PR_ArenaAllocate PL_ArenaAllocate
#define PR_ArenaGrow PL_ArenaGrow
#define PR_ArenaRelease PL_ArenaRelease
#define PR_ArenaCountAllocation PL_ArenaCountAllocation
#define PR_ArenaCountInplaceGrowth PL_ArenaCountInplaceGrowth
#define PR_ArenaCountGrowth PL_ArenaCountGrowth
#define PR_ArenaCountRelease PL_ArenaCountRelease
#define PR_ArenaCountRetract PL_ArenaCountRetract

/* Re: prhash.h->plhash.h */
#define PRHashEntry PLHashEntry
#define PRHashTable PLHashTable
#define PRHashNumber PLHashNumber
#define PRHashFunction PLHashFunction
#define PRHashComparator PLHashComparator
#define PRHashEnumerator PLHashEnumerator
#define PRHashAllocOps PLHashAllocOps
#define PR_NewHashTable PL_NewHashTable
#define PR_HashTableDestroy PL_HashTableDestroy
#define PR_HashTableRawLookup PL_HashTableRawLookup
#define PR_HashTableRawAdd PL_HashTableRawAdd
#define PR_HashTableRawRemove PL_HashTableRawRemove
#define PR_HashTableAdd PL_HashTableAdd
#define PR_HashTableRemove PL_HashTableRemove
#define PR_HashTableEnumerateEntries PL_HashTableEnumerateEntries
#define PR_HashTableLookup PL_HashTableLookup
#define PR_HashTableDump PL_HashTableDump
#define PR_HashString PL_HashString
#define PR_CompareStrings PL_CompareStrings
#define PR_CompareValues PL_CompareValues

#endif /* !defined(PROTYPES_H) */

PR_END_EXTERN_C


PR_BEGIN_EXTERN_C
typedef struct PRLock PRLock;
NSPR_API(PRLock*) PR_NewLock(void);
NSPR_API(void) PR_DestroyLock(PRLock *lock);
NSPR_API(void) PR_Lock(PRLock *lock);
NSPR_API(PRStatus) PR_Unlock(PRLock *lock);

PR_END_EXTERN_C

#if !defined(prinrval_h)
#define prinrval_h
PR_BEGIN_EXTERN_C
typedef PRUint32 PRIntervalTime;
NSPR_API(PRIntervalTime) PR_IntervalNow(void);
NSPR_API(PRUint32) PR_TicksPerSecond(void);
NSPR_API(PRIntervalTime) PR_SecondsToInterval(PRUint32 seconds);
NSPR_API(PRIntervalTime) PR_MillisecondsToInterval(PRUint32 milli);
NSPR_API(PRIntervalTime) PR_MicrosecondsToInterval(PRUint32 micro);
NSPR_API(PRUint32) PR_IntervalToSeconds(PRIntervalTime ticks);
NSPR_API(PRUint32) PR_IntervalToMilliseconds(PRIntervalTime ticks);
NSPR_API(PRUint32) PR_IntervalToMicroseconds(PRIntervalTime ticks);
PR_END_EXTERN_C

#endif /* !defined(prinrval_h) */

PR_BEGIN_EXTERN_C
typedef struct PRCondVar PRCondVar;
NSPR_API(PRCondVar*) PR_NewCondVar(PRLock *lock);
NSPR_API(void) PR_DestroyCondVar(PRCondVar *cvar);
NSPR_API(PRStatus) PR_WaitCondVar(PRCondVar *cvar, PRIntervalTime timeout);
NSPR_API(PRStatus) PR_NotifyCondVar(PRCondVar *cvar);
NSPR_API(PRStatus) PR_NotifyAllCondVar(PRCondVar *cvar);

PR_END_EXTERN_C
