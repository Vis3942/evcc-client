/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          kal_internal.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         Internal structure and procedure definitions for KAL module
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */
#ifndef KAL_INTERNAL_H
#define KAL_INTERNAL_H

#include "psal_assert.h"
#include "kal_api.h"
#include "dm_api.h"         // for DM_LogMsg
#include "platform_test_config.h"

/*******************************************************************************
 * kal asserts
 ******************************************************************************/
#ifdef NDEBUG
#define KAL_ASSERT(x,y)
#define KAL_ASSERT_FORCE()
#define KAL_ASSERT_COMPILE(x)
#else
//#define KAL_ASSERT(x)                 PSAL_ASSERT( x )
#define KAL_ASSERT_FORCE()            PSAL_ASSERT_FORCE()
#define KAL_ASSERT_COMPILE(x)         PSAL_ASSERT_COMPILE( x )
#if !defined( MAL_TESTER )
#define KAL_ASSERT(message, assertion)      \
            if(!(assertion))  {\
                DM_LogMsg(DM_ID_KAL, DM_LOG_PRIORITY_CRITICAL, message);    \
                KAL_Wait(1000);             \
                PSAL_ASSERT(assertion);      \
            }
#endif

#endif

#define KAL_ASSERT_STATIC(x)          PSAL_ASSERT_STATIC( x )


/*******************************************************************************
 * Configuration values
 ******************************************************************************/
#define NO_OF_QUEUES                (10)    // Number of block sizes
#define MIN_BLOCK_SIZE              (8)     // Minimum allocation size (bytes)

#if defined(OVERRIDE_KAL_CONFIG_TIMER_MAX_CONCURRENT)
#define NO_OF_TIMER_CONTROL_BLOCKS  OVERRIDE_KAL_CONFIG_TIMER_MAX_CONCURRENT
#else
#define NO_OF_TIMER_CONTROL_BLOCKS  (20)    // Allocated in Timer_Initialize()
#endif

#if defined(OVERRIDE_KAL_CONFIG_MESSAGE_MAX_CONCURRENT)
#define NO_OF_MESSAGES              OVERRIDE_KAL_CONFIG_MESSAGE_MAX_CONCURRENT
#else
#define NO_OF_MESSAGES              (20)    // Maximum messages co-existing
#endif

#if defined(OVERRIDE_KAL_CONFIG_SEMAPHORE_MAX_INSTANCES)
#define NO_OF_SEMAPHORES            OVERRIDE_KAL_CONFIG_SEMAPHORE_MAX_INSTANCES)
#else
#define NO_OF_SEMAPHORES            (20)    // Maximum coexisting
#endif

#if defined(OVERRIDE_KAL_CONFIG_TASK_MAX_CONCURRENT)
#define NO_OF_TASKS                 OVERRIDE_KAL_CONFIG_TASK_MAX_CONCURRENT)
#else
#define NO_OF_TASKS                 (8)     // Maximum number of tasks
#endif

#if defined(OVERRIDE_KAL_CONFIG_TIMEBASE_MAX_TIMEBASES)
#define NO_OF_TIMEBASES             OVERRIDE_KAL_CONFIG_TIMEBASE_MAX_TIMEBASES
#else
#define NO_OF_TIMEBASES             (1)     // Number of timebases (external clocks)
#endif

#if defined(OVERRIDE_KAL_CONFIG_ALARM_MAX_ALARMS)
#define NO_OF_ALARMS                OVERRIDE_KAL_CONFIG_ALARM_MAX_ALARMS
#else
#define NO_OF_ALARMS                (20)    // Maximum number of alarms across all timebases
#endif

/*******************************************************************************
 * Heap definition
 ******************************************************************************/

extern u8_t KAL_Heap[];
#define HEAP_START                  (KAL_Heap)
#ifdef STDLIB_BUILD
#define HEAP_LENGTH                 (0x80000)
#else
#ifdef ZIGBEE_PRO
#define HEAP_LENGTH                 (0x9000)
#else
#define HEAP_LENGTH                 (0x9000)
#endif
#endif

/*******************************************************************************
 * Task Control Blocks
 ******************************************************************************/

typedef struct kal_task
{
    struct kal_task*                   Next;
    struct kal_mail*                   Mail;                   // Tail of circular asynchronous mail queue
    struct kal_mail*                   p_sync;                 // Head of synchronous mail "queue"
    void*                              syncCountingSemaphore;  // Self-suspension semaphore for sync mail
    volatile kal_mail_unique_handle_t  h_syncExpectedUnique;   // Unique handle for sync mail
    u32_t                              StackPointer;           // Current stack pointer for task
    kal_event_t                        EventMask;              // Event bits for task
    kal_event_t                        EventFilter;            // Filter on EventMask
    kal_mail_id_t                      syncExpectedMailId;     // Sync mail reply mail ID
    u8_t                               Priority;               // Priority 0-255, 255 is high
    u8_t                               iTask;
    u8_t                               bInUse: 1;              // 1 if this task allocated
    u8_t                               bPermitSleep: 1;        // Sleep allowed
    u8_t                               bEventTimer: 1;         // Event timeout running
    u8_t                               State: 2;               // Task state
#define TASK_READY          (0)
#define TASK_CURRENT        (1)
#define TASK_WAIT_EVENT     (2)
#define TASK_WAIT_SEMAPHORE (3)
    u8_t                               iSemaphore;
    u8_t                               syncMailTaskType;       // Type of synchronous mail task
} kal_task_t;

/*******************************************************************************
 * Timebase and External Clock Control Blocks
 ******************************************************************************/
typedef struct kal_extTimebase_s
{
    i64_t       timeClockWasSet;                               // Based off of msTicksAlive
    kal_time_t  clockValue;                                    // In elapsed seconds
}  kal_extTimebase_t;

typedef struct kal_extClock_s
{
    kal_extTimebase_t  timebase;
    u8_t               inUse;
    i8_t               alarmQueueIndex;
}  kal_extClock_t;

#define KAL_ALARM_INVALID_INDEX                       (-1)

/*******************************************************************************
 * Semaphore Control Blocks
 ******************************************************************************/

typedef struct kal_semaphore
{
    kal_task_t          *WaitQueue;
    u8_t                Value;
    u8_t                bInUse: 1;
    u8_t                pad[2];             // keep multiple of 4 bytes
} kal_semaphore_t;

/*******************************************************************************
 * Timer Control Block
 ******************************************************************************/
typedef struct kal_timer
{
    i32_t               TimeToLive;         // Time until timer expires
    i32_t               RechargeTime;       // Initial value for recurrent timer
    u8_t                iNextTimer;         // Index of next chained control block
    u8_t                iTask;              // Index of associated task
    u8_t                iEvent : 6;         // Bit number in event mask for notify
    u8_t                bInUse : 1;
    u8_t                bRecurrent : 1;
    u8_t                pad[1];             // keep multiple of 4 bytes
} kal_timer_t;

/*******************************************************************************
 * Structures for Messaging
 ******************************************************************************/
typedef struct kal_message
{
    u16_t               Length;             // Length of data within message
    u16_t               Offset;             // Offset of data within message
    u16_t               iChunk;             // Index of Chunk within Heap
    u8_t                iOwnerTask;         // Owning task
    u8_t                iQ;                 // Queue number for Chunk
} kal_message_t;

typedef struct kal_mail
{
    struct kal_mail*          Next;               // Chain
    u16_t                     ParameterLength;    // Length of ParameterBlock
    u8_t                      iMessage;           // index of associated message
    u8_t                      iSendingTask;       // Index of sending task
    kal_mail_id_t             MailId;
    kal_mail_unique_handle_t  h_unique;           // Unique handle for synchronous mail
    u32_t                     ParameterBlock[1];  // Place holder ensuring alignment
                                                  // Real length given by ParameterLength
} kal_mail_t;
#define KAL_MAIL_HDRLEN (sizeof(kal_mail_t)-4)

/*******************************************************************************
 * Structure and Defines used to Debug dynamic memory allocation
 ******************************************************************************/
#define KAL_MEMORY_FREE_FLAG       0xADDE // "DEAD"
#define KAL_MEMORY_ALLOCED_FLAG    0xBEBA // "BABE"
#define KAL_MEMORY_CLEAR           0xFF

#define KAL_DEBUG_NUM_ACTIVE_TASKS 5

enum {
  KAL_DEBUG_UNDEFINED_MARKER = 0,
  KAL_DEBUG_DNS_MARKER       = 1,
  KAL_DEBUG_EXI_MARKER       = 2,
  KAL_DEBUG_FSAL_MARKER      = 3,
  KAL_DEBUG_HTTP_MARKER      = 4,
  KAL_DEBUG_SEC_MARKER       = 5,
  KAL_DEBUG_SEP_MARKER       = 6,
  KAL_DEBUG_XML_MARKER       = 7,
  KAL_DEBUG_LWIP_MARKER      = 8,
  KAL_DEBUG_NUM_MARKERS
};

#ifdef KOS_DEBUG_MEMORY
void * KAL_MallocDebug( size_t size, u8_t marker );
void * KAL_CallocDebug( size_t num, size_t size, u8_t marker );
void * KAL_ReallocDebug( void * ptr, size_t size, u8_t marker );
#define KAL_CALLOC_DNS(size)       KAL_CallocDebug(num,size,KAL_DEBUG_DNS_MARKER)
#define KAL_MALLOC_DNS(size)       KAL_MallocDebug(size,KAL_DEBUG_DNS_MARKER)
#define KAL_CALLOC_EXI(num,size)   KAL_CallocDebug(num,size,KAL_DEBUG_EXI_MARKER)
#define KAL_MALLOC_EXI(size)       KAL_MallocDebug(size,KAL_DEBUG_EXI_MARKER)
#define KAL_CALLOC_FSAL(size)      KAL_CallocDebug(num,size,KAL_DEBUG_FSAL_MARKER)
#define KAL_MALLOC_FSAL(size)      KAL_MallocDebug(size,KAL_DEBUG_FSAL_MARKER)
#define KAL_CALLOC_HTTP(num,size)  KAL_CallocDebug(num,size,KAL_DEBUG_HTTP_MARKER)
#define KAL_MALLOC_HTTP(size)      KAL_MallocDebug(size,KAL_DEBUG_HTTP_MARKER)
#define KAL_CALLOC_SEC(size)       KAL_CallocDebug(num,size,KAL_DEBUG_SEC_MARKER)
#define KAL_MALLOC_SEC(size)       KAL_MallocDebug(size,KAL_DEBUG_SEC_MARKER)
#define KAL_CALLOC_SEP(num,size)   KAL_CallocDebug(num,size,KAL_DEBUG_SEP_MARKER)
#define KAL_MALLOC_SEP(size)       KAL_MallocDebug(size,KAL_DEBUG_SEP_MARKER)
#define KAL_CALLOC_XML(num,size)   KAL_CallocDebug(num,size,KAL_DEBUG_XML_MARKER)
#define KAL_MALLOC_XML(size)       KAL_MallocDebug(size,KAL_DEBUG_XML_MARKER)
#define KAL_CALLOC_LWIP(num,size)  KAL_CallocDebug(num,size,KAL_DEBUG_LWIP_MARKER)
#define KAL_MALLOC_LWIP(size)      KAL_MallocDebug(size,KAL_DEBUG_LWIP_MARKER)

#define KAL_REALLOC_HTTP(num,size) KAL_ReallocDebug(num,size,KAL_DEBUG_HTTP_MARKER)
#define KAL_REALLOC_EXI(num,size)  KAL_ReallocDebug(num,size,KAL_DEBUG_EXI_MARKER)
#define KAL_REALLOC_LWIP(num,size) KAL_ReallocDebug(num,size,KAL_DEBUG_LWIP_MARKER)
#else
#define KAL_CALLOC_DNS(num,size)   KAL_Calloc(num,size)
#define KAL_MALLOC_DNS(size)       KAL_Malloc(size)
#define KAL_CALLOC_EXI(num,size)   KAL_Calloc(num,size)
#define KAL_MALLOC_EXI(size)       KAL_Malloc(size)
#define KAL_CALLOC_FSAL(num,size)  KAL_Calloc(num,size)
#define KAL_MALLOC_FSAL(size)      KAL_Malloc(size)
#define KAL_CALLOC_HTTP(num,size)  KAL_Calloc(num,size)
#define KAL_MALLOC_HTTP(size)      KAL_Malloc(size)
#define KAL_CALLOC_SEC(num,size)   KAL_Calloc(num,size)
#define KAL_MALLOC_SEC(size)       KAL_Malloc(size)
#define KAL_CALLOC_SEP(num,size)   KAL_Calloc(num,size)
#define KAL_MALLOC_SEP(size)       KAL_Malloc(size)
#define KAL_CALLOC_XML(num,size)   KAL_Calloc(num,size)
#define KAL_MALLOC_XML(size)       KAL_Malloc(size)
#define KAL_CALLOC_LWIP(num,size)  KAL_Calloc(num,size)
#define KAL_MALLOC_LWIP(size)      KAL_Malloc(size)

#define KAL_REALLOC_HTTP(num,size) KAL_Realloc(num,size)
#define KAL_REALLOC_EXI(num,size)  KAL_Realloc(num,size)
#define KAL_REALLOC_LWIP(num,size) KAL_Realloc(num,size)
#endif

typedef struct {
  u32_t totalDynamicMemory;
  u32_t freeDynamicMemory;
#ifdef KOS_DEBUG_MEMORY
  u32_t allocatedDynamicMemory;
  u32_t allocatedTaskMemory[KAL_DEBUG_NUM_ACTIVE_TASKS];
  u32_t allocatedModuleMemory[KAL_DEBUG_NUM_MARKERS];
  u16_t allocBufCount[NO_OF_QUEUES];
#endif
  u16_t freeBufCount[NO_OF_QUEUES];
} kal_debug_mem_stats_t;

#ifdef KOS_DEBUG_MEMORY  //---------------Start

// sizeof this struct must be a multiple of u32_t and <= MIN_BLOCK_SIZE
typedef struct {
  u16_t                 UseFlag;
  u8_t                  ModuleMarker;
  u8_t                  TaskId;
} kal_debug_chunk_t;
#define KAL_MEMDEBUG_HDR_SIZE sizeof(kal_debug_chunk_t)

#else  //------------------- ! KOS_DEBUG_MEMORY

#define KAL_MEMDEBUG_HDR_SIZE 0

#endif //-----------------------------------End

/*******************************************************************************
 * Structures used by dynamic memory allocation
 ******************************************************************************/

#ifdef KOS_DEBUG_MEMORY  //---------------Start

typedef struct kal_chunk {
  kal_debug_chunk_t     Debug;
  union {
    u8_t                Buffer[MIN_BLOCK_SIZE-sizeof(kal_debug_chunk_t)];
    struct kal_chunk   *Next;
  }                  u;
} kal_chunk_t;

#else  //------------------- ! KOS_DEBUG_MEMORY

typedef struct kal_chunk {
  union {
    u8_t                Buffer[MIN_BLOCK_SIZE];
    struct kal_chunk   *Next;
  }                  u;
} kal_chunk_t;

#endif //-----------------------------------End

/*******************************************************************************
 * External data
 ******************************************************************************/

extern kal_task_t             *KAL_CurrentTask;
extern kal_task_t             KAL_TaskTable[];
extern kal_chunk_t            *Heap;
extern kal_mail_t             *KAL_OldTaskMail[];
extern kal_semaphore_t        *pSemaphores;
extern kal_chunk_t            *Queues[NO_OF_QUEUES];
extern i64_t                  LastTickTime;
extern kal_extClock_t         kalTimebaseExtClocks[NO_OF_TIMEBASES];

KAL_ASSERT_COMPILE( extern u8_t Kal_State; )

/*******************************************************************************
 * External data
 ******************************************************************************/
#define KAL_STATE_NOINIT    0
#define KAL_STATE_INIT      1
#define KAL_STATE_START     2

/*******************************************************************************
 * Start of procedure prototypes
 ******************************************************************************/
/*******************************************************************************
 * kal_task procedures
 ******************************************************************************/
void    KAL_TaskInitialize( void );
void    KAL_TaskStart( void );
void    KAL_TaskResume( u32_t StackPointer );
u32_t   KAL_TaskSuspend( u32_t *StackPointer );
u32_t   KAL_TaskInit( void(*pTaskEntry)(void*), void *pStack );
void    KAL_TaskActivateNext( void );
void    KAL_TaskActiveQueue( kal_task_t *pTask );
void    KAL_Scheduler( void );

/*******************************************************************************
 * kal_memory procedures
 ******************************************************************************/
void    KAL_MemoryInitialize( void );
void    KAL_MemoryStart( void );
u8_t    KAL_Size2Queue( size_t Length );
u8_t    KAL_FreeChunk( u8_t iQ, kal_chunk_t *pBuff );
void KAL_DumpMemStats( kal_debug_mem_stats_t *pStats );

/*******************************************************************************
 * kal_timer procedures
 ******************************************************************************/
void    KAL_TimerInitialize( void );
void    KAL_TimerStart( void );
void    KAL_TimerTick( i64_t TimeNow );
u8_t    KAL_TimerHasExpired( u8_t *phTask );
void    KAL_TimerAdjust(u32_t deltaTime);
void    KAL_TimerPoll();

/*******************************************************************************
 * kal_message procedures
 ******************************************************************************/
void    KAL_MessageInitialize( void );
void    KAL_MessageStart( void );

/*******************************************************************************
 * kal_mail procedures
 ******************************************************************************/
void    KAL_MailStart(void);

/*******************************************************************************
 * kal_semaphore procedures
 ******************************************************************************/
void    KAL_SemaphoreInitialize( void);

/*******************************************************************************
 * KAL Timebase Procedures
 ******************************************************************************/
void    KAL_TimebaseInit(void);

/*******************************************************************************
 * KAL Alarm Procedures
 ******************************************************************************/
void    KAL_AlarmInit(void);
void    KAL_AlarmRemoveQueue(i8_t* const p_queueHead);
u8_t    KAL_AlarmHasExpired(u8_t*  p_notificationTaskHandle);


#endif // KAL_INTERNAL_H
