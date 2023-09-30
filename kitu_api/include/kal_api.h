/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          kal_api.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         Procedure definitions for the KAL module
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef KAL_API_H
#define KAL_API_H

#include <stdlib.h>
#include "platform_config.h"
#include "platform_test_config.h"
#include "psal_types.h"

/*******************************************************************************
 * Status enumeration
 ******************************************************************************/
#define G2_SUCCESS                 (0)
#define G2_FAILURE                 (0xFF)    // General failure
#define G2_BAD_HTASK               (1)       // Invalid hTask value
#define G2_BAD_HMESSAGE            (2)       // Invalid hMessage value
#define G2_ALLOCATION_FAIL         (3)       // Cannot allocate memory/resource required
#define G2_BAD_ATTRIBUTE           (4)       // Attribute value does not exist
#define G2_INVALID_LENGTH          (5)       // Length out of range
#define G2_INVALID_OFFSET          (6)       // Offset out of range
#define G2_NOT_FOUND               (7)
#define G2_TIMEOUT                 (8)
#define G2_NO_ROUTE                (9)
#define G2_STATE_ERROR             (10)
#define G2_SYNCMAIL_NOT_SUPPORTED  (11)
#define G2_SYNCMAIL_INVALID        (12)

/*******************************************************************************
 * Synchronous mail task type enumeration and type
 ******************************************************************************/
#define KAL_MAILSYNC_TASKTYPE_NONE       (0)
#define KAL_MAILSYNC_TASKTYPE_INITIATOR  (1)
#define KAL_MAILSYNC_TASKTYPE_REPLIER    (2)

typedef u8_t                        kal_mailsync_tasktype_t;

/*******************************************************************************
 * Software watchdog task type enumeration and type
 ******************************************************************************/
#define KAL_SWD_TASKTYPE_NONE            (0)
#define KAL_SWD_TASKTYPE_USERHANDLED     (1)
#define KAL_SWD_TASKTYPE_AUTOMATIC       (2)

typedef u8_t                        kal_swd_tasktype_t;

/*******************************************************************************
 * Type Definitions
 ******************************************************************************/
typedef i64_t                       kal_time_t;
typedef u64_t                       kal_event_t;
typedef void                        (*kal_swd_failure_handler_t)(u8_t  h_task);
typedef void                        (*kal_swd_fault_handler_t)(u8_t  h_task);
typedef void                        (*kal_swd_success_handler_t)(void);

/*******************************************************************************
 * Event mask bits
 ******************************************************************************/
#define KAL_MESSAGE_BIT                       ((u8_t) 0)              // Bit 0
#define KAL_START_BIT                         ((u8_t) 1)              // Bit 1
#define KAL_EVENT_TIMEOUT_BIT                 ((u8_t) 2)              // Bit 2
#define KAL_SWD_EVENT_BIT                     ((u8_t) 3)              // Bit 3
#define KAL_MM_RETURN_TO_OUTER_MODE_BIT       ((u8_t) 8)              // Bit 8
#define KAL_MM_ENTER_NORMAL_PROCESSING_BIT    ((u8_t) 9)              // Bit 9
#define KAL_MESSAGE_EVENT                     ((kal_event_t) (1ULL << KAL_MESSAGE_BIT))
#define KAL_MAIL_EVENT                        KAL_MESSAGE_EVENT
#define KAL_START_EVENT                       ((kal_event_t) (1ULL << KAL_START_BIT))
#define KAL_EVENT_TIMEOUT_EVENT               ((kal_event_t) (1ULL << KAL_EVENT_TIMEOUT_BIT))  // WaitForEvent timeout
#define KAL_SWD_EVENT                         ((kal_event_t) (1ULL << KAL_SWD_EVENT_BIT))
#define KAL_MM_RETURN_TO_OUTER_MODE_EVENT     ((kal_event_t) (1ULL << KAL_MM_RETURN_TO_OUTER_MODE_BIT))
#define KAL_MM_ENTER_NORMAL_PROCESSING_EVENT  ((kal_event_t) (1ULL << KAL_MM_ENTER_NORMAL_PROCESSING_BIT))
#define KAL_ALL_EVENTS                        ((kal_event_t) 0xFFFFFFFFFFFFFFFFULL)
#define KAL_ALL_SYSTEM_EVENTS                 ((kal_event_t) 0x000000000000FFFFULL)
#define KAL_ALL_USER_EVENTS                   ((kal_event_t) 0xFFFFFFFFFFFF0000ULL)

/*******************************************************************************
 * Task externals
 ******************************************************************************/
extern u8_t   MM_TaskHandle;
extern u8_t   MAC_TaskHandle;
extern u8_t   APP_TaskHandle;
extern u8_t   LWIP_TaskHandler;
extern u8_t   NWK_TaskHandle;
extern u8_t   ZBPRO_TaskHandle;


/*******************************************************************************
 * Start of procedure prototypes
 ******************************************************************************/

/*******************************************************************************
 * kal_main procedures
 ******************************************************************************/
void         KAL_Init( void );
u8_t         KAL_TaskCreate( void               (*pTaskEntry)(void*),
                             void*              pStartParms,
                             const char* const  taskName,
                             u32_t*             pStack,
                             const u16_t        stackSizeInLongs,
                             const u8_t         Priority );
void         KAL_Start( void );
kal_event_t  KAL_TaskEventWait( kal_event_t  EventFilter, 
                                u32_t        Milliseconds );
u8_t         KAL_TaskSetEvent( u8_t         hTask, 
                               kal_event_t  fEventBits );
void         KAL_AllowSleep( void );
void         KAL_DisallowSleep( void );

/*******************************************************************************
 * kal_memory procedures
 ******************************************************************************/
#if defined(KAL_DEBUG_MEMORY_LINUX)
#define       KAL_Malloc(x)          KAL_MallocDebug(x, __FILE__, __LINE__)
extern void*  KAL_MallocDebug( size_t size , char*  filename, u32_t  linenum);
#define       KAL_Calloc(x, y)       KAL_CallocDebug(x, y, __FILE__, __LINE__)
extern void*  KAL_CallocDebug( size_t num, size_t size, char*  filename, u32_t  linenum );
#define       KAL_Realloc(x, y)      KAL_ReallocDebug(x, y, __FILE__, __LINE__)
extern void*  KAL_ReallocDebug( void * ptr, size_t size, char*  filename, u32_t  linenum );
#define       KAL_Free(x)            KAL_FreeDebug(x, __FILE__, __LINE__)
extern void   KAL_FreeDebug( void * pointer, char*  filename, u32_t  linenum );
#define       KAL_FreePtr(x)         KAL_FreePtrDebug(x, __FILE__, __LINE__)
extern void   KAL_FreePtrDebug( void**  a_ptr, char*  filename, u32_t  linenum);
#define       KAL_AllocateStatic(x)  KAL_AllocateStaticDebug(x, __FILE__, __LINE__)
extern void*  KAL_AllocateStaticDebug( size_t Length, char*  filename, u32_t  linenum );
extern void   KAL_MemoryStatus(char*  p_string);
#else
extern void*  KAL_Malloc(size_t  size);
extern void*  KAL_Calloc(size_t  num, size_t  size);
extern void*  KAL_Realloc(void*  ptr, size_t  size);
extern void   KAL_Free(void*  pointer);
extern void   KAL_FreePtr(void**  a_ptr);
extern void*  KAL_AllocateStatic(size_t  length);
#define KAL_MemoryStatus(x)
#endif

void*   KAL_Memset( void *Dest, int c, size_t Count );
void*   KAL_Memcpy( void *Dest, const void *Src, size_t Count );
void*   KAL_MemcpyReverse( void *Dest, const void *Src, size_t Count );
int     KAL_Memcmp( const void *buf1, const void *buf2, size_t Count );

/* deprecated, move away from these, will be deleted soon */
void*   KAL_AllocateMemory( size_t Length );
u8_t    KAL_FreeMemory( size_t Length, void *Address );

/*******************************************************************************
 * kal_timer procedures
 ******************************************************************************/
u8_t    KAL_SetTimer( u32_t Milliseconds, u8_t hTask, u8_t iEvent, u8_t bRecurrent );
u8_t    KAL_CancelTimer( u8_t hTask, u8_t iEvent );
u8_t    KAL_Wait( u32_t Milliseconds );

/*******************************************************************************
 * kal_alarm procedures
 ******************************************************************************/
#define KAL_TIMEBASE_NETWORK_CLOCK                  (0)
#define KAL_TIMEBASE_INVALID_VALUE                  (0x7FFFFFFFFFFFFFFFLL)
#define KAL_TIMEBASE_RETIRE_EXT_CLOCK_VALUE         (KAL_TIMEBASE_INVALID_VALUE)  // To retire, set using this value.

u8_t        KAL_SetAlarm( kal_time_t utcTime, u8_t hTask, u8_t iEvent, u8_t extClockIndex );
u8_t        KAL_CancelAlarm( kal_time_t utcTime, u8_t hTask, u8_t iEvent, u8_t extClockIndex );
kal_time_t  KAL_GetExtClock( u8_t extClockIndex );
void        KAL_SetExtClock( kal_time_t newUtcTime, u8_t extClockIndex );

/*******************************************************************************
 * kal_message procedures
 ******************************************************************************/
typedef struct kal_message_details
{
    u8_t    *MessageAddress;
    size_t  MessageLength;
    i16_t   iMessage;
} kal_message_details_t;

typedef u16_t  kal_mail_id_t;

typedef struct kal_mail_details
{
    void          *pParameterBlock;
    size_t        ParameterLength;
    u8_t          iPartnerTask;
    kal_mail_id_t MailId;
} kal_mail_details_t;

typedef u32_t                       kal_mail_unique_handle_t;

#define KAL_MAIL_UNIQUE_HANDLE_UNUSED     (0)
#define KAL_MAIL_WAIT_DEFAULT_TIMEOUT_MS  (2000)

u8_t    KAL_MessageAllocate( size_t MaximumLength, size_t InitialOffset,
                             size_t InitialLength, kal_message_details_t *pDetails );
u8_t    KAL_MessageFree( kal_message_details_t *phMessage );
u8_t    KAL_MessageMoveDesc( kal_message_details_t *pMsgDTo, kal_message_details_t *pMsgDFrom );
u8_t    KAL_MessageDuplicate( kal_message_details_t *pMsgDTo, kal_message_details_t *pMsgDFrom );
u8_t    KAL_MessageAddHeader( kal_message_details_t *phMessage, size_t HdrLength );

u8_t    KAL_MessageRemoveHeader( kal_message_details_t *phMessage, size_t HdrLength );
u8_t    KAL_MessageAddTrailer( kal_message_details_t *phMessage, size_t TrailerLength );
u8_t    KAL_MessageRemoveTrailer( kal_message_details_t *phMessage, size_t TrailerLength );
u8_t    KAL_MessageAddSpace( kal_message_details_t *phMessage, size_t Offset, size_t Length );
u8_t    KAL_MessageRemoveSpace( kal_message_details_t *phMessage, size_t Offset, size_t Length );
u8_t    KAL_MailSend( kal_message_details_t *phMessage, kal_mail_details_t *phMail );
u8_t    KAL_MailReceive( kal_message_details_t *phMessage, kal_mail_details_t *phMail );

extern u8_t    
KAL_MailQueueIsEmpty(void);

extern u8_t    
KAL_MailSyncRegister(kal_mailsync_tasktype_t  taskType);

extern u8_t    
KAL_MailSyncGetRegistration(u8_t                      taskId, 
                            kal_mailsync_tasktype_t*  p_taskType);

extern u8_t    
KAL_MailSyncSendAndWait(kal_message_details_t*  p_sendMsgDetails,
                        kal_mail_details_t*     p_sendMailDetails,
                        kal_mail_id_t           replyMailId,
                        kal_message_details_t*  p_replyMsgDetails,
                        kal_mail_details_t*     p_replyMailDetails,
                        u32_t                   timeout);

extern u8_t
KAL_MailSyncReceive(kal_message_details_t*     p_msgDetails,
                    kal_mail_details_t*        p_mailDetails,
                    kal_mail_unique_handle_t*  ph_unique);

extern u8_t
KAL_MailSyncSendReply(kal_message_details_t*    p_msgDetails,
                      kal_mail_details_t*       p_mailDetails,
                      kal_mail_unique_handle_t  h_unique);

extern u8_t
KAL_MailSendOneWay(kal_mail_details_t*  p_mailDetails);

/*******************************************************************************
 * Variations on KAL_MessageAllocate with offsets for headers
 ******************************************************************************/
/* offset for multicast transports (ICMP,UDP,TCP) is IP header(40) + Hop by Hop (8) */
#define MC_OFFS         (48)
/* offset for transports (ICMP,UDP,TCP) is IP header(40) */
#define IPH_OFFS        (40)
/* space for HbH header or RH4 header. Max is 24 allowing up to 8 hops RH4 */
#define RPL_HDR         (24)
/* extra space for rpl header and IP header is IP header(40) + RH4 up to 8 hops (24) */
#define TOT_HDR         (IPH_OFFS+RPL_HDR)
/* Maximun size of the ZigBee Pro Network header */
#define NWK_HDR         (25)

#define KAL_AllocateMSDU(Len,pMsgD)  KAL_MessageAllocate(Len,0,Len,pMsgD)
#define KAL_AllocateNPDU(Len,pMsgD)  KAL_MessageAllocate(Len+RPL_HDR,RPL_HDR,Len,pMsgD)
#define KAL_AllocateMCAST(Len,pMsgD) KAL_MessageAllocate(Len+MC_OFFS,MC_OFFS,Len,pMsgD)
#define KAL_AllocateNSDU(Len,pMsgD)  KAL_MessageAllocate(Len+TOT_HDR,TOT_HDR,Len,pMsgD)
#define KAL_AllocateASDU(Len,pMsgD)  KAL_MessageAllocate(Len+NWK_HDR,NWK_HDR,Len,pMsgD)



/*******************************************************************************
 * kal_semaphore procedures
 ******************************************************************************/
void    *KAL_SemaphoreCreate( u8_t Count );
u8_t    KAL_SemaphoreDelete( void *pSemaphore );
u8_t    KAL_SemaphorePend( void *pSemaphore, u32_t Milliseconds );
u8_t    KAL_SemaphorePost( void *pSemaphore );


/*******************************************************************************
 * KAL Software watchdog procedures
 ******************************************************************************/
extern u8_t
KAL_SwdRegister(kal_swd_tasktype_t  swdType);

extern void
KAL_SwdPetTheDog(void);

extern u8_t
KAL_SwdDisable(void);

extern u8_t
KAL_SwdReenable(void);

extern u8_t
KAL_SwdConfigFailureHandler(kal_swd_failure_handler_t  pf_handler);

extern u8_t
KAL_SwdConfigFaultHandler(kal_swd_fault_handler_t  pf_handler);

extern u8_t
KAL_SwdConfigSuccessHandler(kal_swd_success_handler_t  pf_handler);

extern u8_t
KAL_SwdConfigTimingParameters(u32_t  periodInMs,
                              u32_t  numFailures);

/*******************************************************************************
 * KAL Mode Manager symbolic constants, types, and procedures
 ******************************************************************************/
typedef enum kal_MmMode_e
{
    KAL_MM_MODE_NORMAL,
//    KAL_MM_MODE_UPGRADE,
//    KAL_MM_MODE_SLEEP,
    KAL_MM_MODE_DEFAULT = KAL_MM_MODE_NORMAL
}  kal_MmMode_t;

extern void
KAL_MmSendSwitchModeCommand(kal_MmMode_t  newMode);

extern void
KAL_MmSendReturnToOuterModeResponse(void);

/*******************************************************************************
 * kal_other procedures
 ******************************************************************************/
void        KAL_EnableInterrupts( u32_t flags );
u32_t       KAL_DisableInterrupts( void );
u32_t       KAL_GetRandom( void );
void        KAL_RandomSeed( u32_t seed );
i64_t       KAL_GetMilliseconds( void );
void        KAL_SetClock(kal_time_t  utcTime);
kal_time_t  KAL_GetClock(void);

#define KAL_GetLE16(x)    (((u16_t)(x)[1]<<8)+(x)[0])
#define KAL_GetLE24(x)    (((u32_t)(x)[2]<<16)+((u32_t)(x)[1]<<8)+(x)[0])
#define KAL_GetLE32(x)    (((u32_t)(x)[3]<<24)+((u32_t)(x)[2]<<16)+((u32_t)(x)[1]<<8)+(x)[0])
#define KAL_GetLE40(x)    (((u64_t)(x)[4]<<32)+((u64_t)(x)[3]<<24)+((u64_t)(x)[2]<<16)+((u64_t)(x)[1]<<8)+(x)[0])
#define KAL_GetLE48(x)    (((u64_t)(x)[5]<<40)+((u64_t)(x)[4]<<32)+((u64_t)(x)[3]<<24)+\
                           ((u64_t)(x)[2]<<16)+((u64_t)(x)[1]<<8)+(x)[0])
#define KAL_GetLE56(x)    (((u64_t)(x)[6]<<48)+((u64_t)(x)[5]<<40)+((u64_t)(x)[4]<<32)+\
                           ((u64_t)(x)[3]<<24)+((u64_t)(x)[2]<<16)+((u64_t)(x)[1]<<8)+(x)[0])
#define KAL_GetLE64(x)    (((u64_t)(x)[7]<<56)+((u64_t)(x)[6]<<48)+((u64_t)(x)[5]<<40)+((u64_t)(x)[4]<<32)+\
                           ((u64_t)(x)[3]<<24)+((u64_t)(x)[2]<<16)+((u64_t)(x)[1]<<8)+(x)[0])
#define KAL_PutLE16(x,y)  {(y)[0]=((x)&0xff);(y)[1]=((x)>>8);}
#define KAL_PutLE24(x,y)  {(y)[0]=((x)&0xff);(y)[1]=(((x)>>8)&0xff);(y)[2]=(((x)>>16)&0xff);}
#define KAL_PutLE32(x,y)  {(y)[0]=((x)&0xff);(y)[1]=(((x)>>8)&0xff);(y)[2]=(((x)>>16)&0xff);(y)[3]=(((x)>>24)&0xff);}
#define KAL_PutLE40(x,y)  {(y)[0]=((x)&0xff);(y)[1]=(((x)>>8)&0xff);(y)[2]=(((x)>>16)&0xff);(y)[3]=(((x)>>24)&0xff);\
                          (y)[4]=(((x)>>32)&0xff);}
#define KAL_PutLE48(x,y)  {(y)[0]=((x)&0xff);(y)[1]=(((x)>>8)&0xff);(y)[2]=(((x)>>16)&0xff);(y)[3]=(((x)>>24)&0xff);\
                          (y)[4]=(((x)>>32)&0xff);(y)[5]=(((x)>>40)&0xff);}
#define KAL_PutLE56(x,y)  {(y)[0]=((x)&0xff);(y)[1]=(((x)>>8)&0xff);(y)[2]=(((x)>>16)&0xff);(y)[3]=(((x)>>24)&0xff);\
                          (y)[4]=(((x)>>32)&0xff);(y)[5]=(((x)>>40)&0xff);(y)[6]=(((x)>>48)&0xff);}
#define KAL_PutLE64(x,y)  {(y)[0]=((x)&0xff);(y)[1]=(((x)>>8)&0xff);(y)[2]=(((x)>>16)&0xff);(y)[3]=(((x)>>24)&0xff);\
                          (y)[4]=(((x)>>32)&0xff);(y)[5]=(((x)>>40)&0xff);(y)[6]=(((x)>>48)&0xff);(y)[7]=(((x)>>56)&0xff);}

#define KAL_GetBE16(x)    (((u16_t)(x)[0]<<8)+(x)[1])
#define KAL_GetBE32(x)    (((u32_t)(x)[0]<<24)+((u32_t)(x)[1]<<16)+((u32_t)(x)[2]<<8)+(x)[3])
#define KAL_GetBE64(x)    (((u64_t)(x)[0]<<56)+((u64_t)(x)[1]<<48)+((u64_t)(x)[2]<<40)+((u64_t)(x)[3]<<32)+\
                           ((u64_t)(x)[4]<<24)+((u64_t)(x)[5]<<16)+((u64_t)(x)[6]<<8)+(x)[7])
#define KAL_PutBE16(x,y)  {(y)[1]=((x)&0xff);(y)[0]=((x)>>8);}
#define KAL_PutBE32(x,y)  {(y)[3]=((x)&0xff);(y)[2]=(((x)>>8)&0xff);(y)[1]=(((x)>>16)&0xff);(y)[0]=(((x)>>24)&0xff);}

#define KAL_MIN(x,y)      (((x)>(y))?(y):(x))
#define KAL_MAX(x,y)      (((x)>(y))?(x):(y))


#endif // KAL_API_H
