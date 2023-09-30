/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  klnx_internal.h                                                                               *
 *                                                                                                *
 *  This file contains PRIVATE symbolic constants, type definitions, and function prototypes for  *
 *  the Linux implementation of the Kernel Abstraction Layer.  This header file should only be    *
 *  included by Linux-version KAL files and not by any user files.                                *
 *                                                                                                *
 **************************************************************************************************/


#ifndef KLNX_INTERNAL_H
#define KLNX_INTERNAL_H


#include <stdbool.h>
#include <semaphore.h>
#include <pthread.h>
#include "psal_types.h"
#include "kal_api.h"
#include "kal_config.h"








/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_TASK_SELF_TASK_HANDLE           ((u8_t)0xFF)
#ifdef OVERRIDE_KAL_TIMER_MILLISECONDS_PER_TICK
#define KAL_TIMER_MILLISECONDS_PER_TICK     OVERRIDE_KAL_TIMER_MILLISECONDS_PER_TICK
#else  // OVERRIDE_KAL_TIMER_MILLISECONDS_PER_TICK
#define KAL_TIMER_MILLISECONDS_PER_TICK     (u8_t)(10)
#endif // OVERRIDE_KAL_TIMER_MILLISECONDS_PER_TICK
#define KAL_ALARM_INVALID_INDEX             (u8_t)(255)
#define KAL_SWD_STATE_RUNNING               (0)
#define KAL_SWD_STATE_DISABLED              (1)
#define KAL_SWD_STATE_REENABLED             (2)








/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef struct kal_mailCtrlBlk_s
{
    struct kal_mailCtrlBlk_s*  p_next;
    u16_t                      parmLength;
    i16_t                      h_message;           /* Handle to message, may be 0xFF if none.  */
    u8_t                       h_senderTask;
    kal_mail_id_t              mailId;              /* Value meaningful between sender/receiver.  */
    kal_mail_unique_handle_t   h_unique;
    u32_t                      parmBlkPlaceholder[1];
}  kal_mailCtrlBlk_t;


typedef struct kal_mailQueue_s
{
    sem_t                              queueLock;
    kal_mailCtrlBlk_t*                 p_lastAccessedByUser;
    kal_mailCtrlBlk_t*                 p_head;
    kal_mailCtrlBlk_t*                 p_tail;
    // fields to support synchronous mail
    kal_mailCtrlBlk_t*                 p_sync;
    void*                              syncCountingSemaphore;
    volatile kal_mail_unique_handle_t  h_syncExpectedUnique;
    kal_mail_id_t                      syncExpectedMailId;
    kal_mailsync_tasktype_t            syncMailTaskType;
}  kal_mailQueue_t;


typedef struct kal_extTimebase_s
{
    struct timespec  timeClockWasSetM;
    kal_time_t       clockValue;
}  kal_extTimebase_t;


typedef struct kal_extClock_s
{
    sem_t              extClockLock;
    bool               inUse;
    kal_extTimebase_t  timebase;
    kal_time_t         newUtcTime;
    struct timespec    timeNewUtcWasSetM;
    volatile u8_t      useNewUtcTime;
    i8_t               alarmQueueIndex;
}  kal_extClock_t;


typedef u8_t  kal_swdState_t;


typedef struct kal_swdInfo_s
{
    kal_swd_tasktype_t  swdRegistrationType;
    kal_swdState_t      swdState;
    u8_t                swdWasPet;
}  kal_swdTaskInfo_t;






/**************************************************************************************************
 *                                                                                                *
 *  Definitions for Global Data                                                                   *
 *                                                                                                *
 **************************************************************************************************/
extern kal_swdTaskInfo_t  kalSwdTaskInfo[KAL_CONFIG_TASK_MAX_CONCURRENT];
extern u32_t              kalTaskDefaultStackSize;
extern u32_t              kalMemoryMaxAllocate;
extern u32_t              kalTimerMillisecondsPerTick;
extern kal_extClock_t     kalTimebaseExtClocks[KAL_CONFIG_TIMEBASE_MAX_TIMEBASES];








/**************************************************************************************************
 *                                                                                                *
 *  Function Prototypes                                                                           *
 *                                                                                                *
 **************************************************************************************************/


/**************************************************************************************************
 *                                                                                                *
 *  KAL_SystemInitialize() - this routine initializes all KAL data structures and should be       *
 *                           called only once upon startup.  Note: according to documentation,    *
 *                           this routine should be part of the public interface.  However, an    *
 *                           undocumented routine, KAL_Init(), exists and there is no existing    *
 *                           implementation or definition of this routine.  Therefore, this has   *
 *                           been moved to a "private" routine which is invoked from KAL_Init().  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_SystemInitialize(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SystemStart() - this routine terminates the initialization state and sends every KAL task *
 *                      a "system start" event.  This routine never returns!!                     *
 *                                                                                                *
 *                      Note: according to documentation, this routine is part of the public API. *
 *                      However, there is neither a definition nor implementation in the code     *
 *                      base prior to this one.  Instead, another routine, KAL_Start(), has been  *
 *                      defined and implemented.  Therefore, this routine has been moved to a     *
 *                      "private" routine which is invoked from KAL_Start().                      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_SystemStart(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MemoryInit() - this routine is used to initialize the Kernel Abstraction Layer's dynamic  *
 *                     memory package.                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_MemoryInit(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_GetNumTasks() - this routine returns the number of tasks in KAL.  Once a task is created, *
 *                      it is never deleted.                                                      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t - the number of tasks created in KAL.                                                *
 *                                                                                                *
 **************************************************************************************************/
extern u8_t
KAL_GetNumTasks(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_GetThreadHandle() - this routine is used to obtain an underlying pthread handle from a    *
 *                          KAL task handle.                                                      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t        h_task    - a KAL task handle.  A value of 0xFF may be used to specify the    *
 *                              calling task.                                                     *
 *      pthread_t*  ph_thread - a pointer to where the pthread handle should be placed.           *
 *      u8_t*       ph_task   - a pointer to where the task handle should be placed.  If 0xFF was *
 *                              specified for h_task, the actual task handle may be discovered.   *
 *                              The task handle will only be written if this pointer is not NULL. *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS   - upon success.                                                        *
 *            G2_BAD_HTASK - if the supplied task handle is invalid.                              *
 *                                                                                                *
 **************************************************************************************************/
extern u8_t
KAL_GetThreadHandle(u8_t        h_task,
                    pthread_t*  ph_thread,
                    u8_t*       ph_task);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TimerHandleTick() - this routine is invoked whenever a timer tick occurs.  It updates the *
 *                          timer list, setting task event flags as necessary.  This routine is   *
 *                          not part of the public KAL Timer API.                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_TimerHandleTick(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SemaphoreInit() - this routine creates internal data structures for maintaining a pool of *
 *                        semaphores.  This should only be called once at startup, before any     *
 *                        threads use any other KAL semaphore routine.                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_SemaphoreInit(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TimebaseInit() - this routine is used to initialize the system time base.  This should    *
 *                       only be invoked once at the startup of the application.                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_TimebaseInit(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TimerInit() - this routine is used to initialize the Kernel Abstraction Layer's Timer     *
 *                    package.  This should only be invoked once at the startup of the            *
 *                    application.                                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_TimerInit(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_AlarmInit() - this routine is used to initialize the Kernel Abstraction Layer's Alarm     *
 *                    package.  This should only be invoked once at the startup of the            *
 *                    application.                                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_AlarmInit(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_AlarmHandleAlarms() - this routine is called from the periodic tick to see if any alarms  *
 *                            have expired.  This routine also makes adjustments to external      *
 *                            clocks as part of its operation.                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_AlarmHandleAlarms(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageInit() - this routine is used to initialize the Kernel Abstraction Layer's mail    *
 *                      handling and message passing package.  This should only be invoked once   *
 *                      at the startup of the application.                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_MessageInit(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailPurgeOld() - this routine should be invoked only by KAL_TaskEventWait() to delete     *
 *                       dynamic memory associated with parameter information which was sent in   *
 *                       the last mail.  This routine will simply return if there is nothing to   *
 *                       delete.  If there is more than one parameter "block" to delete, an error *
 *                       message will be printed, and the additional blocks will be deleted.  A   *
 *                       message is printed because the usage of the mail system is not correct.  *
 *                       The expected usage model is that only one call (at most) is made to      *
 *                       KAL_MailReceive() between calls to KAL_TaskEventWait().                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  h_task - the KAL task handle whose old parameter block should be deleted, if any.   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_MailPurgeOld(u8_t  h_task);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailInit() - this routine is used to initialize the Kernel Abstraction Layer's mail       *
 *                   handling package.  This should only be invoked once at system startup, after *
 *                   all tasks have been created.                                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_MailInit(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_HasSystemStarted() - this routine is used to determine whether the system has started     *
 *                           (via a call to KAL_SystemStart()).                                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: 0        - if the system has not been started.                                      *
 *            non-zero - if the system has been started.                                          *
 *                                                                                                *
 **************************************************************************************************/
extern u8_t
KAL_HasSystemStarted(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskIncrementMailCount() - this routine is used to increment the number of mail messages  *
 *                                 for a KAL task.                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  h_task - a handle to a KAL task.  A value of 0xFF may be used to indicate the       *
 *                     current KAL thread.                                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS   - upon success.                                                        *
 *            G2_BAD_HTASK - if the specified KAL task handle was not valid.                      *
 *                                                                                                *
 **************************************************************************************************/
extern u8_t
KAL_TaskIncrementMailCount(u8_t  h_task);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskDecrementMailCount() - this routine is used to decrement the number of mail messages  *
 *                                 for the invoking KAL task.                                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_TaskDecrementMailCount(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskGetNumMailMsgs() - this routine returns the number of mail messages that are on the   *
 *                             calling task's mail queue as kept by the count in the auxiliary    *
 *                             TCB.                                                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern u16_t
KAL_TaskGetNumMailMsgs(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_GetExtClockLock() - this routine is used to obtain the lock for a specified external      *
 *                          clock.                                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  extClockIndex - the index of the desired external clock.  This should be a value    *
 *                            between 0 and (KAL_CONFIG_TIMEBASE_MAX_TIMEBASES-1).  The value     *
 *                            zero, or KAL_TIMEBASE_NETWORK_CLOCK, is the network clock.          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_GetExtClockLock(u8_t  extClockIndex);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_ReleaseExtClockLock() - this routine is used to release the lock for a specified external *
 *                              clock.  This should only be invoked by a task which has           *
 *                              previously obtained the lock using KAL_GetExtClockLock().         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  extClockIndex - the index of the desired external clock.  This should be a value    *
 *                            between 0 and (KAL_CONFIG_TIMEBASE_MAX_TIMEBASES-1).  The value     *
 *                            zero, or KAL_TIMEBASE_NETWORK_CLOCK, is the network clock.          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_ReleaseExtClockLock(u8_t  extClockIndex);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskGetLock() - this routine obtains a semaphore lock which is used to protect the        *
 *                      auxiliary task control blocks.  The value of errno is preserved for the   *
 *                      caller because its value can be changed within this routine.              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_TaskGetLock(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskReleaseLock() - this routine releases a semaphore lock used to protect the auxiliary  *
 *                          TCBs.                                                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_TaskReleaseLock(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskAreAllTasksIdle() - this routine returns "true" if all tasks in the system (except    *
 *                              the Mode Manager) are waiting in calls to KAL_TaskEventWait().    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      bool: true  - if all tasks are in their KAL_TaskEventWait() calls.                        *
 *            false - if one or more tasks are not in waiting in KAL_TaskEventWait().             *
 *                                                                                                *
 **************************************************************************************************/
extern bool
KAL_TaskAreAllTasksIdle(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskSetKalStartedState() - this routine is used (exclusively by the Mode Manager) to set  *
 *                                 the state of KAL to "started."                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_TaskSetKalStartedState(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmCreateModeManagerTask() - this routine is used to create the Mode Manager task.         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_MmCreateModeManagerTask(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdInit() - this routine is used once at startup to initialize the data structures used   *
 *                  by the SWD system.                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      kalSwdTaskInfo                                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_SwdInit(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdStart() - this routine is used by the Mode Manager to start the SWD.  It is called     *
 *                   just before entering "Normal Mode" of operation.  This routine will          *
 *                   initialize data structures and will set the periodic timer for the SWD event *
 *                   (to the Mode Manager).  Note: the periodic event will only enabled if there  *
 *                   is at least one task that registers to use the SWD or if the success routine *
 *                   is not NULL at the time of invocation.  (If the success routine is NULL at   *
 *                   the time of invocation but is changed later, the SWD periodic timer will     *
 *                   still not be set!!)                                                          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      kalSwdTaskInfo                                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_SwdStart(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdCheck() - this routine, invoked periodically by the Mode Manager, checks to see if     *
 *                   there are any SWD failures.  If a failure or fault is detected, the          *
 *                   corresponding routine will be invoked.  If no failures are detected, the     *
 *                   success routine will be invoked.                                             *
 *                                                                                                *
 *                   In general, this routine will examine the data structures associated with    *
 *                   the SWD for each task.  A count is kept for the number of consecutive SWD    *
 *                   failures per task.  If a task responds to the last SWD event, this routine   *
 *                   will reset the number of consecutive failures to zero.  Otherwise, it will   *
 *                   increment the count and invoke the failure routine.  Then, a check will be   *
 *                   made if the failure reaches the limit where it is elevated to a SWD fault    *
 *                   (at which time the fault routine is invoked).                                *
 *                                                                                                *
 *                   The "general" case above occurs if the task's SWD state is RUNNING.  If it   *
 *                   is DISABLED, then no check will occur.  If the SWD state is REENABLED, then  *
 *                   this routine will change the state to RUNNING but no checking will occur     *
 *                   (until the next SWD event).                                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      kalSwdTaskInfo                                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
KAL_SwdCheck(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_GetMillisecondsUntilNextTimer() - this routine is used to obtain the number of            *
 *                                        milliseconds until the next timer expiry.  Because the  *
 *                                        internal timebase of KAL may be coarser than            *
 *                                        millisecond resolution, the value returned may be an    *
 *                                        approximation.  Any approximation will lean toward an   *
 *                                        earlier expiry.                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u32_t - the (approximate) number of milliseconds before the next timer expiry.  If no     *
 *              timers exist, this value will be (2^32 - 1), or 0xFFFFFFFF.                       *
 *                                                                                                *
 **************************************************************************************************/
extern u32_t
KAL_GetMillisecondsUntilNextTimer(void);




/**************************************************************************************************
 *                                                                                                *
 *  KAL_GetSecondsUntilNextAlarm() - this routine is used to obtain the number of seconds until   *
 *                                   the next alarm expiry.  Because of the unknown time before   *
 *                                   the next alarm "tick", the value of seconds may be up to one *
 *                                   second shorter than the actual expiry.                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u32_t - the (approximate) number of seconds before the next alarm expiry.  If no alarms   *
 *              exist, or if the expiry is very far in the future (in excess of 65 years), this   *
 *              value will be (2^32 - 1), or 0xFFFFFFFF.                                          *
 *                                                                                                *
 **************************************************************************************************/
extern u32_t
KAL_GetSecondsUntilNextAlarm(void);








#endif  /* #ifdef KLNX_INTERNAL_H  */

