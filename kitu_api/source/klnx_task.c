/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  klnx_task.c                                                                                   *
 *                                                                                                *
 *  This file contains the implementation of the Kernel Abstraction Layer's scheduler which uses  *
 *  underlying Linux pthreads.  This file contains symbolic constants, type definitions, and      *
 *  function implementations.  There is a public API to be used by higher layers, and a "private" *
 *  API to be used by other KAL routines.  The private API is used to unify timers, mailboxes,    *
 *  and tasks on top of pthreads.                                                                 *
 *                                                                                                *
 **************************************************************************************************/


#include <stdbool.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include "kal_api.h"
#include "kal_config.h"
#include "klnx_internal.h"
#include "psal.h"
#include "psal_assert.h"
#include "psal_sighandler.h"
#include "platform_test_config.h"








/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_TASK_FALSE                      (0)
#define KAL_TASK_TRUE                       (1)
#define KAL_TASK_DEFAULT_STACK_SIZE         (u32_t)(8*1024)      /* All KAL tasks will use an 8K stack.  */








/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef struct kal_taskAuxTcb_s
{
    pthread_t             h_thread;
    sem_t                 selfSuspensionSemaphore;
    volatile u16_t        numMailMsgs;
    volatile kal_event_t  eventFlags;
    kal_event_t           eventMask;
    volatile bool         willSleep;            /* Part of a "two-phase" lock using only one lock.  */
    volatile bool         willWake;             /* Part of a "two-phase" lock using only one lock.  */
    volatile bool         inTaskEventWait;
}  kal_taskAuxTcb_t;


typedef void  (*kal_taskFuncPtr_t)(void*);


typedef struct kal_taskBootstrapInfo_s
{
    kal_taskFuncPtr_t  pf_taskEntry;
    void*              p_startupParms;
    char*              taskName;
}  kal_taskBootstrapInfo_t;








/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
static kal_taskAuxTcb_t  kalTaskAuxTcbs[KAL_CONFIG_TASK_MAX_CONCURRENT];
static u8_t              kalTaskNumTasks;
static u8_t              kalHasStarted;
static sem_t             kalTaskSemaphoreLock;
static pthread_attr_t    kalTaskPthreadsAttributes;
static i32_t             kalTaskPthreadsPriorityMin;
static i32_t             kalTaskPthreadsPriorityMax;
static i32_t             kalTaskPthreadsPriorityRange;
static pthread_t         kalTaskPeriodicTimerThread;
u32_t                    kalTaskDefaultStackSize=KAL_TASK_DEFAULT_STACK_SIZE;
volatile sig_atomic_t    SigTermWait = 0;







/**************************************************************************************************
 *                                                                                                *
 *  Local Function Declarations                                                                   *
 *                                                                                                *
 **************************************************************************************************/
static void   
KAL_TaskInit(void);

static i32_t  
KAL_TaskConvertPriority(u8_t  taskPriority);

static void*  
KAL_TaskBootstrap(void*  p_parm);

static void
KAL_TaskBlockSignals(void);

static void   
KAL_TaskCreatePeriodicTimerThread(void);

#if 0
static void*  
KAL_TaskPeriodicTick(void*  p_parm);
#endif

static void*  
KAL_TaskHiResPeriodicTick(void*  p_parm);








/**************************************************************************************************
 *                                                                                                *
 *  KAL_SystemInitialize() - this routine initializes all KAL data structures and should be       *
 *                           called only once upon startup.                                       *
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
void
KAL_SystemInitialize(void)
{
    int  schedulerScope;
    int  schedulerPolicy;

    srandom((u32_t) time(NULL));

    KAL_MemoryInit();
    KAL_TimebaseInit();
    KAL_TaskInit();
    KAL_TimerInit();
    KAL_SemaphoreInit();
    KAL_MessageInit();
    KAL_AlarmInit();

#ifndef G2H_ANDROID
    pthread_attr_getscope(&kalTaskPthreadsAttributes, &schedulerScope);
#else
	schedulerScope = PTHREAD_SCOPE_SYSTEM;
#endif

    if (schedulerScope == PTHREAD_SCOPE_SYSTEM)
    {
        pthread_attr_setscope(&kalTaskPthreadsAttributes, PTHREAD_SCOPE_PROCESS);
    }
    pthread_attr_getschedpolicy(&kalTaskPthreadsAttributes, &schedulerPolicy);
    if (schedulerPolicy != SCHED_RR)
    {
        pthread_attr_setschedpolicy(&kalTaskPthreadsAttributes, SCHED_RR);
    }
    kalTaskPthreadsPriorityMin = sched_get_priority_min(SCHED_RR);
    kalTaskPthreadsPriorityMax = sched_get_priority_max(SCHED_RR);
    kalTaskPthreadsPriorityRange = kalTaskPthreadsPriorityMax - kalTaskPthreadsPriorityMin;

    /* Only do this if it is supported in the pthreads implementation.  */
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
    {
        size_t  defaultStackSize=0;

        pthread_attr_getstacksize(&kalTaskPthreadsAttributes, &defaultStackSize);
        if (defaultStackSize < kalTaskDefaultStackSize)
        {
            pthread_attr_setstacksize(&kalTaskPthreadsAttributes,
                                      (size_t) kalTaskDefaultStackSize);
        }
    }
#endif

    /* Create the special Mode Manager task.  This is done early to ensure that the task handle
         for the Mode Manager is zero.  */
    KAL_MmCreateModeManagerTask();
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SystemStart() - this routine terminates the initialization state and sends every KAL task *
 *                      a "system start" event.  This routine never returns!!                     *
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
void
KAL_SystemStart(void)
{
    siginfo_t  sigInfo;
    sigset_t   sigSet;

    /* Block application signals and handle all termination signals.  */
    (void) sigemptyset(&sigSet);
    (void) sigaddset(&sigSet, SIGALRM);
    (void) sigaddset(&sigSet, SIGCHLD);
    (void) sigaddset(&sigSet, SIGUSR1);
    (void) sigaddset(&sigSet, SIGUSR2);
    (void) pthread_sigmask(SIG_BLOCK, &sigSet, NULL);
    
#if defined(ZERO_START_TIME)
    KAL_SetClock(0);
#else  /* matches #if defined(ZERO_START_TIME)  */
    /* Initialize the network clock to be the real-time clock value.  */
    KAL_SetClock((kal_time_t) time(NULL));
#endif  /* matches #else of #if defined(ZERO_START_TIME)  */

    /* Before starting any KAL tasks, create a high-priority thread for implementing the periodic
         timer tick.  */
    KAL_TaskCreatePeriodicTimerThread();

    /* Now, start the Mode Manager by setting the KAL system start event for it.  Technically,
         KAL has not fully started yet.  The Mode Manager will start KAL from the perspective of
         all other tasks.  */
    KAL_TaskSetEvent(MM_TaskHandle, KAL_START_EVENT);

    /* Handle all termination signals.  */
    (void) sigemptyset(&sigSet);
    (void) sigaddset(&sigSet, SIGHUP);
    (void) sigaddset(&sigSet, SIGINT);
    (void) sigaddset(&sigSet, SIGQUIT);
    (void) sigaddset(&sigSet, SIGTERM);
    (void) pthread_sigmask(SIG_UNBLOCK, &sigSet, NULL);
    while ((sigwaitinfo(&sigSet, &sigInfo) == -1) && (errno == EINTR))
    {
        ;  /* Do nothing.  A different signal interrupted the system call.  */
    }

    /* A termination signal was received.  Call the termination callback routine before exiting.  */
    PSAL_SighandlerCallTerminationCallback(sigInfo.si_signo);
    exit(EXIT_SUCCESS);
}




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
u8_t
KAL_HasSystemStarted(void)
{
    return kalHasStarted;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskCreate() - this routine is used to create a new KAL task, subject to the KAL          *
 *                     scheduler.                                                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void               (*pTaskEntry)(void*) - a pointer to a function which will be the entry *
 *                                                point for the new KAL task.                     *
 *      void*              pStartParms          - a pointer to startup parameters for the new     *
 *                                                task.  A value of NULL may be used.             *
 *      const char* const  taskName             - a name to be given to the task.                 *
 *      u32_t*             pStack               - a pointer to the stack for the task.  NOTE: for *
 *                                                the Linux port, this value may be NULL.         *
 *      const u16_t        stackSizeInLongs     - the size of the stack in 4-byte longwords.      *
 *      const u8_t         priority             - the priority for the task, between 1 and 255    *
 *                                                (255 being the highest priority).               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_FAILURE - upon any failure.                                                      *
 *            other      - a handle to the newly created task.                                    *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_TaskCreate(void               (*pTaskEntry)(void*),
               void*              pStartParms,
               const char* const  taskName,
               u32_t*             pStack,
               const u16_t        stackSizeInLongs,
               const u8_t         priority)
{
    u8_t                      retValue = (u8_t)G2_FAILURE;
    pthread_t                 h_thread;
    struct sched_param        schedulerParms;
    kal_taskBootstrapInfo_t*  p_threadParm;

    if ((priority != 0) && (kalHasStarted == KAL_TASK_FALSE))
    {
        /* Create the auxiliary task control block entry and obtain a task handle before actually
             creating the thread.  */
        KAL_TaskGetLock();
        if (kalTaskNumTasks < KAL_CONFIG_TASK_MAX_CONCURRENT)
        {
            if (sem_init(&kalTaskAuxTcbs[kalTaskNumTasks].selfSuspensionSemaphore, 0, 0) == 0)
            {
                kalTaskAuxTcbs[kalTaskNumTasks].eventFlags = 0x00000000;
                kalTaskAuxTcbs[kalTaskNumTasks].eventMask = 0x00000000;
                kalTaskAuxTcbs[kalTaskNumTasks].willSleep = false;
                kalTaskAuxTcbs[kalTaskNumTasks].willWake = false;

                /* Finally, create the thread and place its handle into the auxiliary TCB.  */
                schedulerParms.sched_priority = KAL_TaskConvertPriority(priority);
                pthread_attr_setschedparam(&kalTaskPthreadsAttributes, &schedulerParms);
#if 0
                if (pStack != NULL)
                {
                    pthread_attr_setstack(&kalTaskPthreadsAttributes, pStack, stackSizeInLongs*sizeof(u32_t));
                }
#endif
                p_threadParm = (kal_taskBootstrapInfo_t*) malloc(sizeof(kal_taskBootstrapInfo_t));
                if (p_threadParm != NULL)
                {
                    p_threadParm->pf_taskEntry = pTaskEntry;
                    p_threadParm->p_startupParms = pStartParms;
                    p_threadParm->taskName = (char*) taskName;
                    pthread_create(&h_thread, &kalTaskPthreadsAttributes,
                                   KAL_TaskBootstrap, (void*) p_threadParm);
                    kalTaskAuxTcbs[kalTaskNumTasks].h_thread = h_thread;
                    retValue = kalTaskNumTasks++;
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: unable to allocate memory for thread parameters in KAL_TaskCreate()!!\n");
                }
            }
            else
            {
                fprintf(stderr,
                        "ERROR: unable to create a POSIX semaphore in KAL_TaskCreate()!! (%d)\n",
                        errno);
            }
        }

        KAL_TaskReleaseLock();
    }

    return retValue;
}



/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskEventWait() - this routine is used by a KAL-based task to return control to the       *
 *                        scheduler until an event bit becomes set in its task event mask.  The   *
 *                        event mask (filter) is used to select particular event bits of          *
 *                        interest, but may be set of 0xFFFFFFFFFFFFFFFFULL (or KAL_ALL_EVENTS)   *
 *                        to wait for any event.  A timeout value is specified in case the KAL    *
 *                        task wishes to perform non-event-based processing, but this may be set  *
 *                        to zero to specify an infinite timeout.                                 *
 *                                                                                                *
 *                        If a task registers to use the automatic Software Watchdog, then the    *
 *                        handling of the SWD event will occur within this routine.  The data     *
 *                        structures used by the SWD system is protected against simultaneous     *
 *                        access using the same locking mechanism as this module.  Therefore, no  *
 *                        special locking is required to access SWD data.                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_event_t  eventFilter - a 64-bit field indicating for which event flags the task is    *
 *                                 waiting.  There are reserved bits in the filter:               *
 *                                     Bit  0 - signifies that there is a message for the task on *
 *                                              its message queue.                                *
 *                                     Bit  1 - used on system startup, set when the system       *
 *                                              starts.                                           *
 *                                     Bit  2 - signifies that the timeout expired on the call to *
 *                                              this routine.                                     *
 *                                     Bit  3 - used for the SWD event.                           *
 *                                     Bit  8 - used to stop NORMAL mode and to drop a task into  *
 *                                              its OUTER mode loop.                              *
 *                                     Bit  9 - used to start NORMAL mode from a task's OUTER     *
 *                                              mode loop.                                        *
 *                                 There are sixteen reserved system bits (0 to 15).  This        *
 *                                 parameter should never be set to zero!!                        *
 *      u32_t  milliseconds      - the timeout value to wait for a desired event, or zero for an  *
 *                                 infinite timeout.                                              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      kal_event_t - the event flags bit-ANDed with the event filter.  The task must process all *
 *                    of the bits which are set in this return value because they will be         *
 *                    cleared.  If the event filter does not include the timer expiry (Bit 2),    *
 *                    that bit will still be set if an expiry occurred.                           *
 *                                                                                                *
 **************************************************************************************************/
kal_event_t
KAL_TaskEventWait(kal_event_t  eventFilter,
                  u32_t        milliseconds)
{
    kal_event_t      retValue = 0;        // SPRK-1018
    kal_event_t      dbgEventMask = 0;    // SPRK-1018
    pthread_t        h_thread;
    u8_t             index;
    struct timespec  timeoutValue;
    u32_t            seconds;
    u32_t            nanoseconds;
    i32_t            waitStatus;

    /* Check if the event filter is empty.  If it is, then log an error message.  */
    if (eventFilter == 0)
    {
        fprintf(stderr, "ERROR: KAL_TaskEventWait() invoked with an event filter of 0x0000000000000000!!\n");
        PSAL_ASSERT_FORCE();
    }

    /* Check if a (non-infinite) timeout is used.  If so, calculate the absolute time of the 
         expiry before locking.  Note: there is a chance that this calculation will be unnecessary
         because there will be an event already waiting.  */
    if (milliseconds != 0)
    {
        seconds = milliseconds / 1000;
        nanoseconds = (milliseconds - (seconds * 1000)) * 1000000;
        if (clock_gettime(CLOCK_REALTIME, &timeoutValue) == 0)
        {
            timeoutValue.tv_sec += seconds;
            timeoutValue.tv_nsec += nanoseconds;
            if (timeoutValue.tv_nsec >= 1000000000)
            {
                timeoutValue.tv_sec++;
                timeoutValue.tv_nsec -= 1000000000;
            }
        }
        else
        {
            /* Scheduler won't work correctly if this happens.  Simply return; there's no
               lock held now.  */
            fprintf(stderr,
                    "ERROR: unable to obtain the current hi-res clock in KAL_TaskEventWait()!!\n");
            PSAL_ASSERT_FORCE();
        }
    }

    /* Obtain the lock.  */
    KAL_TaskGetLock();

    /* Find out which task is invoking to determine its auxiliary TCB.  */
    h_thread = pthread_self();
    index=0;
    while ((index < kalTaskNumTasks) &&
           (pthread_equal(kalTaskAuxTcbs[index].h_thread, h_thread) == 0))
    {
        index++;
    }

    if (index < kalTaskNumTasks)
    {
        /* Remove old mail parameter blocks.  Release the lock to avoid deadlock if another task
             sends mail to this routine.  That routine would perform <LockMail, LockTCB>.  If
             the lock was not released, then this task would perform <LockTCB, LockMail>, resulting
             in a deadlock.  Note: the deletion of old parameter blocks does not require the TCB
             lock, only the mailbox lock.  */
        KAL_TaskReleaseLock();

        KAL_MailPurgeOld(index);

        KAL_TaskGetLock();

        /* Update flag indicating that the task is calling KAL_TaskEventWait().  */
        kalTaskAuxTcbs[index].inTaskEventWait = true;

        kalTaskAuxTcbs[index].eventMask = eventFilter;
        if (kalTaskAuxTcbs[index].numMailMsgs != 0)
        {
            kalTaskAuxTcbs[index].eventFlags |= KAL_MESSAGE_EVENT;
        }

        /* Handle SWD event if task is registered for automatic SWD handling.  */
        if (kalSwdTaskInfo[index].swdRegistrationType == KAL_SWD_TASKTYPE_AUTOMATIC)
        {
            /* Ensure that the SWD event bit is set so that event will wake up the task.  */
            kalTaskAuxTcbs[index].eventMask |= KAL_SWD_EVENT;

            if (kalTaskAuxTcbs[index].eventFlags & KAL_SWD_EVENT)
            {
                /* The SWD event has occurred so handle it.  Do not invoke the KAL_SwdPetTheDog()
                     routine as it protects its data using the same synchronization mechanism as
                     this routine and the auxiliary TCBs.  To prevent potential deadlock, just
                     modify the data structure directly.  */
                kalSwdTaskInfo[index].swdWasPet = 1;

                /* Now, clear the event from the event flags.  */
                kalTaskAuxTcbs[index].eventFlags &= ~KAL_SWD_EVENT;
            }
        }
        retValue = kalTaskAuxTcbs[index].eventFlags & kalTaskAuxTcbs[index].eventMask;

        if (retValue == 0)
        {
            /* There is nothing of interest, so suspend.  */
            if (milliseconds == 0)
            {
                kalTaskAuxTcbs[index].willSleep = true;

                do
                {
                    KAL_TaskReleaseLock();
                    while ((sem_wait(&kalTaskAuxTcbs[index].selfSuspensionSemaphore) == -1) &&
                           (errno == EINTR))
                    {
                        /* Restart suspension if awoken by an interrupt.  */
                    }

                    /*
                     *  Sleeping now ...
                     */

                    KAL_TaskGetLock();
                    kalTaskAuxTcbs[index].willWake = false;
                    if (kalTaskAuxTcbs[index].numMailMsgs != 0)
                    {
                        kalTaskAuxTcbs[index].eventFlags |= KAL_MESSAGE_EVENT;
                    }

                    /* Handle SWD event if task is registered for automatic SWD handling.  */
                    if ((kalSwdTaskInfo[index].swdRegistrationType == KAL_SWD_TASKTYPE_AUTOMATIC) &&
                        (kalTaskAuxTcbs[index].eventFlags & KAL_SWD_EVENT))
                    {
                        /* The SWD event has occurred so handle it.  Do not invoke the 
                             KAL_SwdPetTheDog() routine as it protects its data using the same 
                             synchronization mechanism as this routine and the auxiliary TCBs.  
                             To prevent potential deadlock, just modify the data structure 
                             directly.  */
                        kalSwdTaskInfo[index].swdWasPet = 1;
                            
                        /* Now, clear the event from the event flags.  */
                        kalTaskAuxTcbs[index].eventFlags &= ~KAL_SWD_EVENT;
                    }

                    retValue = kalTaskAuxTcbs[index].eventFlags & kalTaskAuxTcbs[index].eventMask;
                }  while (retValue == 0);

                kalTaskAuxTcbs[index].willSleep = false;
                kalTaskAuxTcbs[index].eventFlags &= ~retValue;

                /* Update flag indicating that the task no longer waiting in 
                     KAL_TaskEventWait().  */
                kalTaskAuxTcbs[index].inTaskEventWait = false;
            }
            else
            {
                kalTaskAuxTcbs[index].willSleep = true;

                do
                {
                    KAL_TaskReleaseLock();

                    while (((waitStatus=sem_timedwait(&kalTaskAuxTcbs[index].selfSuspensionSemaphore,
                                                      &timeoutValue)) == -1) && (errno == EINTR))
                    {
                        /* Restart suspension if awoken by an interrupt.  */
                    }

                    /*
                     *  Sleeping now ...
                     */

                    KAL_TaskGetLock();

                    /* Check reason for wakeup.  */
                    if (waitStatus != 0)
                    {
                        if (errno == ETIMEDOUT)
                        {
                            /* Sets Bit 2 for timer expiry.  */
                            retValue |= KAL_EVENT_TIMEOUT_EVENT;

                            /* Now, eliminate a race condition where an event arrived for a task
                                 which has just awoken due to a timeout.  */
                            if (kalTaskAuxTcbs[index].willWake == true)
                            {
                                /* The self-suspension semaphore has been posted due to the race
                                     condition.  Eliminate the extra post by performing a blind
                                     pend.  Because the pend should return immediately, the lock on
                                     the task control blocks will be kept during the operation.  */
                                while ((sem_wait(&kalTaskAuxTcbs[index].selfSuspensionSemaphore) == -1) &&
                                       (errno == EINTR))
                                {
                                    /* Restart suspension if awoken by an interrupt.  */
                                }
                            }
                        }
                        else
                        {
                            fprintf(stderr,
                                    "ERROR: received an unexpected error code from sem_timedwait()!! (%d)\n",
                                    errno);
                        }
                    }
                    else
                    {
                        /* Check if there is mail.  */
                        if (kalTaskAuxTcbs[index].numMailMsgs != 0)
                        {
                            kalTaskAuxTcbs[index].eventFlags |= KAL_MESSAGE_EVENT;
                        }

                        /* Handle SWD event if task is registered for automatic SWD handling.  */
                        if ((kalSwdTaskInfo[index].swdRegistrationType == 
                             KAL_SWD_TASKTYPE_AUTOMATIC) &&
                            (kalTaskAuxTcbs[index].eventFlags & KAL_SWD_EVENT))
                        {
                            /* The SWD event has occurred so handle it.  Do not invoke the 
                                 KAL_SwdPetTheDog() routine as it protects its data using the same 
                                 synchronization mechanism as this routine and the auxiliary TCBs.  
                                 To prevent potential deadlock, just modify the data structure 
                                 directly.  */
                            kalSwdTaskInfo[index].swdWasPet = 1;
                        
                            /* Now, clear the event from the event flags.  */
                            kalTaskAuxTcbs[index].eventFlags &= ~KAL_SWD_EVENT;
                        }

                        retValue = kalTaskAuxTcbs[index].eventFlags & kalTaskAuxTcbs[index].eventMask;
                    }
                }  while (retValue == 0);

                kalTaskAuxTcbs[index].willWake = false;
                kalTaskAuxTcbs[index].willSleep = false;
                kalTaskAuxTcbs[index].eventFlags &= ~retValue;
                
                /* Update flag indicating that the task no longer waiting in 
                     KAL_TaskEventWait().  */
                kalTaskAuxTcbs[index].inTaskEventWait = false;
            }
        }
        else
        {
            kalTaskAuxTcbs[index].eventFlags &= ~retValue;
        }

        /* While we still have the lock, clear the event mask.  */
        dbgEventMask = kalTaskAuxTcbs[index].eventMask;
        kalTaskAuxTcbs[index].eventMask = 0x00000000;
    }
    else
    {
        fprintf(stderr, "ERROR: a non-KAL pthread invoked KAL_TaskEventWait()!!\n");
        PSAL_ASSERT_FORCE();
    }

    /* Release the lock.  */
    KAL_TaskReleaseLock();

    if (retValue == 0)
    {
        fprintf(stderr, "WARNING: KAL_TaskEventWait() returns a zero value!!\n");
        fprintf(stderr, "WARNING:     Event Flags: 0x%0llX\n", kalTaskAuxTcbs[index].eventFlags);
        fprintf(stderr, "WARNING:     Event Mask:  0x%0llX\n", dbgEventMask);
    }

    return retValue;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskSetEvent() - this routine is used to set one or more event flags in a task's          *
 *                       auxiliary task control block.  If the task is suspended, it will be      *
 *                       woken up if the new event flags matches the task's event mask.           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t         hTask      - a handle to the task which will be have its event flags set.    *
 *      kal_event_t  fEventBits - a 64-bit field indicating which flags should be set.            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS   - upon success.                                                        *
 *            G2_BAD_HTASK - upon failure when the specified task handle is invalid.              *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_TaskSetEvent(u8_t         hTask,
                 kal_event_t  fEventBits)
{
    pthread_t  h_thread;
    u8_t       retCode=G2_BAD_HTASK;

    if (KAL_GetThreadHandle(hTask, &h_thread, &hTask) == G2_SUCCESS)
    {
        /* Obtain the lock.  */
        KAL_TaskGetLock();

        if (hTask < kalTaskNumTasks)
        {
            /* Bit 0 is special.  If it is set, clear it.  */
            fEventBits &= 0xFFFFFFFFFFFFFFFELL;
            kalTaskAuxTcbs[hTask].eventFlags |= fEventBits;
            retCode = G2_SUCCESS;
            if ((kalTaskAuxTcbs[hTask].eventFlags & kalTaskAuxTcbs[hTask].eventMask) != 0)
            {
                /* An event has occurred which may wake up the thread, if suspended.  Check, and wake
                   it up, if it is not already scheduled to be woken up.  */
                if ((kalTaskAuxTcbs[hTask].willSleep == true) &&
                    (kalTaskAuxTcbs[hTask].willWake == false))
                {
                    kalTaskAuxTcbs[hTask].willWake = true;
                    sem_post(&kalTaskAuxTcbs[hTask].selfSuspensionSemaphore);
                }
            }
        }

        /* Release the lock.  */
        KAL_TaskReleaseLock();
    }
    
    return retCode;
}








/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskInit() - this routine is used to initialize the Kernel Abstraction Layer's scheduler. *
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
static void
KAL_TaskInit(void)
{
    u8_t  index;

    for (index=0; index<KAL_CONFIG_TASK_MAX_CONCURRENT; index++)
    {
        memset(&kalTaskAuxTcbs[index], 0, sizeof(kal_taskAuxTcb_t));
    }
    kalTaskNumTasks = 0;
    kalHasStarted = KAL_TASK_FALSE;

    /* Create a binary semaphore to lock the auxiliary TCB from concurrent access.  */
    sem_init(&kalTaskSemaphoreLock, 0, 1);

    /* Obtain default pthreads attributes.  If needed (and permissible), attributes can be
         overwritten when threads are created.  */
    pthread_attr_init(&kalTaskPthreadsAttributes);

    kalTaskPthreadsPriorityMin = 0;
    kalTaskPthreadsPriorityRange = 0;
}




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
u8_t
KAL_GetNumTasks(void)
{
    return kalTaskNumTasks;
}




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
u8_t
KAL_GetThreadHandle(u8_t        h_task,
                    pthread_t*  ph_thread,
                    u8_t*       ph_task)
{
    u8_t  retCode=G2_BAD_HTASK;
    u8_t  index;

    if (h_task == KAL_TASK_SELF_TASK_HANDLE)
    {
        *ph_thread = pthread_self();
        index = 0;
        KAL_TaskGetLock();
        while ((index < kalTaskNumTasks) &&
               (pthread_equal(kalTaskAuxTcbs[index].h_thread, *ph_thread) == 0))
        {
            index++;
        }
        KAL_TaskReleaseLock();
        *ph_task = index;
        if (index < kalTaskNumTasks)
        {
            retCode = G2_SUCCESS;
        }
        else
        {
            fprintf(stderr, "ERROR: a non-KAL pthread invoked KAL_GetThreadHandle()!!\n");
        }
    }
    else
    {
        KAL_TaskGetLock();
        if (h_task < kalTaskNumTasks)
        {
            *ph_thread = kalTaskAuxTcbs[h_task].h_thread;
            KAL_TaskReleaseLock();
            *ph_task = h_task;
            retCode = G2_SUCCESS;
        }
        else
        {
            KAL_TaskReleaseLock();
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskIncrementMailCount() - this routine is used to increment the number of mail messages  *
 *                                 for a KAL task.  As a result of this routine, a KAL task may   *
 *                                 be immediately woken up.                                       *
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
u8_t
KAL_TaskIncrementMailCount(u8_t  h_task)
{
    u8_t       retCode=G2_BAD_HTASK;
    u8_t       index;
    pthread_t  currentThread;

    if (h_task == KAL_TASK_SELF_TASK_HANDLE)
    {
        currentThread = pthread_self();
        index = 0;
        KAL_TaskGetLock();

        /* Find the task.  */
        while ((index < kalTaskNumTasks) &&
               (pthread_equal(kalTaskAuxTcbs[index].h_thread, currentThread) == 0))
        {
            index++;
        }
        if (index < kalTaskNumTasks)
        {
            /* Increment the number of mail message.  */
            kalTaskAuxTcbs[index].numMailMsgs++;
            retCode = G2_SUCCESS;

            /* Check to see if the task is waiting for a message.  */
            if (kalTaskAuxTcbs[index].eventMask & KAL_MESSAGE_EVENT)
            {
                /* Now, see if the task needs to be woken up.  */
                if ((kalTaskAuxTcbs[index].willSleep == true) &&
                    (kalTaskAuxTcbs[index].willWake == false))
                {
                    /* Wake up the thread.  */
                    kalTaskAuxTcbs[index].willWake = true;
                    sem_post(&kalTaskAuxTcbs[index].selfSuspensionSemaphore);
                }
            }
        }
        KAL_TaskReleaseLock();
    }
    else
    {
        KAL_TaskGetLock();
        if (h_task < kalTaskNumTasks)
        {
            kalTaskAuxTcbs[h_task].numMailMsgs++;
            retCode = G2_SUCCESS;

            /* Check to see if the task is waiting for a message.  */
            if (kalTaskAuxTcbs[h_task].eventMask & KAL_MESSAGE_EVENT)
            {
                /* Now, see if the task needs to be woken up.  */
                if ((kalTaskAuxTcbs[h_task].willSleep == true) &&
                    (kalTaskAuxTcbs[h_task].willWake == false))
                {
                    /* Wake up the thread.  */
                    kalTaskAuxTcbs[h_task].willWake = true;
                    sem_post(&kalTaskAuxTcbs[h_task].selfSuspensionSemaphore);
                }
            }
        }
        KAL_TaskReleaseLock();
    }

    return retCode;
}




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
void
KAL_TaskDecrementMailCount(void)
{
    pthread_t  currentThread;
    u8_t       index;

    currentThread = pthread_self();
    index = 0;
    KAL_TaskGetLock();
    while ((index < kalTaskNumTasks) &&
           (pthread_equal(kalTaskAuxTcbs[index].h_thread, currentThread) == 0))
    {
        index++;
    }
    if (index < kalTaskNumTasks)
    {
        if (kalTaskAuxTcbs[index].numMailMsgs > 0)
        {
            kalTaskAuxTcbs[index].numMailMsgs--;
        }
        else
        {
            fprintf(stderr, "ERROR: attempt to decrement zero mail count was made!!\n");
        }
    }
    KAL_TaskReleaseLock();
}




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
 *      u16_t - the number of queued mail messages.                                               *
 *                                                                                                *
 **************************************************************************************************/
u16_t
KAL_TaskGetNumMailMsgs(void)
{
    pthread_t  h_thread;
    u16_t      numMailMsgs=0;
    u8_t       h_task;

    if (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, &h_thread, &h_task) == G2_SUCCESS)
    {
        KAL_TaskGetLock();
        numMailMsgs = kalTaskAuxTcbs[h_task].numMailMsgs;
        KAL_TaskReleaseLock();
    }

    return numMailMsgs;
}




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
void
KAL_TaskGetLock(void)
{
    int  oldErrno=errno;

    while ((sem_wait(&kalTaskSemaphoreLock) == -1) && (errno == EINTR))
    {
        /* Do nothing.  This is done in case the semaphore call returned due to a signal.  */
    }

    errno = oldErrno;
}




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
void
KAL_TaskReleaseLock(void)
{
    sem_post(&kalTaskSemaphoreLock);
}




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
bool
KAL_TaskAreAllTasksIdle(void)
{
    u8_t  index;
    bool  retCode=true;

    /* Mode Manager is always task #0, so skip checking it.  */
    index = 1;
    while ((retCode == true) && (index < kalTaskNumTasks))
    {
        retCode = kalTaskAuxTcbs[index++].inTaskEventWait;
    }

    return retCode;
}




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
void
KAL_TaskSetKalStartedState(void)
{
    KAL_TaskGetLock();
    kalHasStarted = KAL_TASK_TRUE;
    KAL_TaskReleaseLock();
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskConvertPriority() - this routine converts a task priority to a pthreads thread        *
 *                              priority.                                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8t  taskPriority - a KAL task priority.                                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      i32_t - the converted thread priority.                                                    *
 *                                                                                                *
 **************************************************************************************************/
static i32_t
KAL_TaskConvertPriority(u8_t  taskPriority)
{
    return ((((taskPriority - 1) * kalTaskPthreadsPriorityRange) / 254) +
            kalTaskPthreadsPriorityMin);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskBootstrap() - this routine is the initial entry point of every pthread created on     *
 *                        behalf of a KAL task.                                                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  p_parm - a pointer to a parameter block.                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void* - this routine should never exit; therefore, it never returns a value.              *
 *                                                                                                *
 **************************************************************************************************/
static void*
KAL_TaskBootstrap(void*  p_parm)
{
    kal_taskFuncPtr_t  p_func;
    void*              p_startParms;
    char               taskName[5];

    /* Get the function pointer to the KAL task.  */
    p_func = ((kal_taskBootstrapInfo_t*) p_parm)->pf_taskEntry;
    p_startParms = ((kal_taskBootstrapInfo_t*) p_parm)->p_startupParms;
    strncpy(taskName, ((kal_taskBootstrapInfo_t*) p_parm)->taskName, 4);
    taskName[4] = '\0';
    free(p_parm);

    /* Set up thread's signal mask to mask out termination and application signals.  */
    KAL_TaskBlockSignals();
    
    /* Force synchronization with respect to the thread creator.  Creator already has the lock,
         so obtaining the lock will let the creator finish accessing the auxilliary TCB.  */
    KAL_TaskGetLock();
    KAL_TaskReleaseLock();

    /* Jump to the KAL task.  It is expected that the KAL task will perform its initialization
         and wait using KAL_TaskEventWait() until the KAL system starts.  */
    (*p_func)(p_startParms);

    /* The KAL Task should never exit.  Just in case, remain in an infinite loop and never exit.  */
    fprintf(stderr, "ERROR: a KAL task terminated its infinite loop!!\n");
    while (1);

    return NULL;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskBlockSignals() - this private routine is used to block application and termination    *
 *                           signals by setting the thread's signal mask using pthread_sigmask(). *
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
static void
KAL_TaskBlockSignals(void)
{
    sigset_t           sigSet;

    (void) sigemptyset(&sigSet);
    (void) sigaddset(&sigSet, SIGHUP);
    //(void) sigaddset(&sigSet, SIGINT);        // SPRK-1156_Handle_signal_from_raise
    (void) sigaddset(&sigSet, SIGQUIT);
    //(void) sigaddset(&sigSet, SIGTERM);       // SPRK-1156_Handle_signal_from_raise
    (void) sigaddset(&sigSet, SIGALRM);
    (void) sigaddset(&sigSet, SIGCHLD);
    (void) sigaddset(&sigSet, SIGUSR1);
    (void) sigaddset(&sigSet, SIGUSR2);
    (void) pthread_sigmask(SIG_BLOCK, &sigSet, NULL);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskCreatePeriodicTimerThread() - this routine is used to create a special pthread thread *
 *                                        which is used to implement a periodic tick.  The thread *
 *                                        will be given a high priority in the system.            *
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
static void
KAL_TaskCreatePeriodicTimerThread(void)
{
    struct sched_param  schedulerParms;

    schedulerParms.sched_priority = kalTaskPthreadsPriorityMax;
    KAL_TaskGetLock();
    pthread_attr_setschedparam(&kalTaskPthreadsAttributes, &schedulerParms);
    pthread_create(&kalTaskPeriodicTimerThread, &kalTaskPthreadsAttributes,
                   KAL_TaskHiResPeriodicTick, NULL);
    KAL_TaskReleaseLock();
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskPeriodicTick() - this routine is the entry point for a special pthreads thread which  *
 *                           is used to implement a periodic tick.  This tick is used to invoke   *
 *                           the KAL_TimerHandleExpiry() routine.  NOTE: although this thread is  *
 *                           not correlated to a KAL task, it will still use KAL routines.  It is *
 *                           safe to do so because of the implementation of various locks done    *
 *                           "below" the KAL layer within the KAL implementation.                 *
 *                                                                                                *
 *                           There is an important assumption made which is that the periodic     *
 *                           tick will not sleep for more than one second.                        *
 *                                                                                                *
 *                           In order to reduce drift, the thread will obtain the time it woke    *
 *                           up, calculate the elasped time after handling all tick-related       *
 *                           duties, then recalculate the amount of time to sleep.  Although      *
 *                           there will still be drift, this will account for the handling (which *
 *                           could take some time due to resource locking).                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  p_parm - a pointer to a parameter block.                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void* - this routine should never exit; therefore, it never returns a value.              *
 *                                                                                                *
 **************************************************************************************************/
#if 0
static void*
KAL_TaskPeriodicTick(void*  p_parm)
{
    struct timespec  currentTime;
    struct timespec  expiryTime;
    struct timespec  timeoutValue;
    u32_t            deltaNanoseconds;

    KAL_TaskBlockSignals();
    deltaNanoseconds = KAL_TIMER_MILLISECONDS_PER_TICK * 1000000;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    expiryTime.tv_sec = currentTime.tv_sec;
    expiryTime.tv_nsec = currentTime.tv_nsec + deltaNanoseconds;
    if (expiryTime.tv_nsec >= 1000000000)
    {
        expiryTime.tv_sec++;
        expiryTime.tv_nsec -= 1000000000;
    }
    timeoutValue.tv_sec = 0;
    timeoutValue.tv_nsec = deltaNanoseconds;
    while (1)
    {
        while ((nanosleep(&timeoutValue, &timeoutValue) == -1) && (errno == EINTR))
        {
            /* Do nothing, sleep was interrupted.  Go back to sleep.  */
        }
        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        while ((currentTime.tv_sec > expiryTime.tv_sec) || 
               ((currentTime.tv_sec == expiryTime.tv_sec) && 
                (currentTime.tv_nsec >= expiryTime.tv_nsec)))
        {
            KAL_TimerHandleTick();
            expiryTime.tv_nsec += deltaNanoseconds;
            if (expiryTime.tv_nsec >= 1000000000)
            {
                expiryTime.tv_sec++;
                expiryTime.tv_nsec -= 1000000000;
            }
            clock_gettime(CLOCK_MONOTONIC, &currentTime);
        }

        /* Recalculate timeout value.  */
        timeoutValue.tv_nsec = expiryTime.tv_nsec - currentTime.tv_nsec;
    }

    /* Just in case, never return.  */
    fprintf(stderr, "ERROR: periodic tick thread terminated its infinite loop!!\n");
    return NULL;
}
#endif



/**************************************************************************************************
 *                                                                                                *
 *  KAL_TaskHiResPeriodicTick() - this routine is the entry point for a special pthreads thread   *
 *                                which is used to implement a periodic tick.  Similar to         *
 *                                KAL_TaskPeriodicTick(), this routine is a high-resolution       *
 *                                version which was designed to eliminate drift.  Since it is     *
 *                                based on the clock_nanosleep() function, it may not be portable *
 *                                for some Linux systems.                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  p_parm - a pointer to a parameter block.                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void* - this routine should never exit; therefore, it never returns a value.              *
 *                                                                                                *
 **************************************************************************************************/
static void*
KAL_TaskHiResPeriodicTick(void*  p_parm)
{
    struct timespec  currentTime;
    struct timespec  wakeupTime;
    u32_t            nanoseconds;
    i32_t            retValue;

    KAL_TaskBlockSignals();
    //u32_t            seconds;
    //seconds = KAL_TIMER_MILLISECONDS_PER_TICK / 1000;
    //if (seconds > 0)
    //{
    //    fprintf(stderr, "FATAL ERROR: periodic tick is too large (must be less than 1 second)!!\n");
    //    exit(-1);
    //}
    nanoseconds = KAL_TIMER_MILLISECONDS_PER_TICK * 1000000;
#if !ITRON_API_ENABLED    
    if (clock_gettime(CLOCK_MONOTONIC, &currentTime) != 0)
    {
        fprintf(stderr, "FATAL ERROR: unable to read the real-time clock!!\n");
        exit(-1);
    }
#endif

    while (1)
    {
#if ITRON_API_ENABLED      // Dos not support clock_nanosleep()
        wakeupTime.tv_nsec = nanoseconds;
        wakeupTime.tv_sec = 0;
        while ((retValue=nanosleep(&wakeupTime, NULL) == -1)
#else //ITRON_API_ENABLED        
        /* Calculate the next wakeup time. */
        wakeupTime.tv_nsec = currentTime.tv_nsec + nanoseconds;
        if (wakeupTime.tv_nsec >= 1000000000)
        {
            wakeupTime.tv_nsec -= 1000000000;
            wakeupTime.tv_sec = currentTime.tv_sec + 1;
        }
        else
        {
            wakeupTime.tv_sec = currentTime.tv_sec;
        }

        /* Store a running version of the wakeup time in the variable currentTime.  */
        currentTime = wakeupTime;

        while ((retValue=clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeupTime, NULL) == -1)
#endif //ITRON_API_ENABLED        
               && (errno == EINTR))
        {
            /* Do nothing, sleep was interrupted.  */
        }

        if (retValue != 0)
        {
            fprintf(stderr, "FATAL ERROR: periodic tick thread encountered error!!  (%d)\n", errno);
            exit(-1);
        }

        KAL_TimerHandleTick();
    }

    /* Just in case, never return.  */
    fprintf(stderr, "ERROR: periodic tick thread terminated its infinite loop!!\n");
    while (1);

    return NULL;
}




