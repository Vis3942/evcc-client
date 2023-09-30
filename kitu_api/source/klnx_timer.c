/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  klnx_timer.c                                                                                  *
 *                                                                                                *
 *  This file contains the implementation of the Kernel Abstraction Layer's Timer package.  It    *
 *  contains symbolic constants, type definitions, and function implementations.  Please note     *
 *  that due to the nature of timers, the underlying Linux implementation may not have the same   *
 *  timer resolution as on other targets.                                                         *
 *                                                                                                *
 *  Timers are held in the system in an array-based linked list sorted on expiry.  The expiry     *
 *  values are deltas from the previous timer.  For example, if three timers with expiries of 3,  *
 *  5, and 18 are simultaneously added to the system, they will be kept in nodes with relative    *
 *  expiries of:                                                                                  *
 *                                                                                                *
 *      +-----+    +-----+    +-----+                                                             *
 *      |  3  |--->|  2  |--->| 13  |                                                             *
 *      +-----+    +-----+    +-----+                                                             *
 *                                                                                                *
 *  Whenever a tick occurs, the first timer is the only one that needs its value decremented.     *
 *  More than one timer may have the same expiry time by having its relative count be zero.       *
 *  Upon timer expiry, all timers at the front of the list that all have relative tick counts of  *
 *  zero will be handled.                                                                         *
 *                                                                                                *
 **************************************************************************************************/


#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "klnx_internal.h"
#include "kal_api.h"








/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_TIMER_FALSE                     (0)
#define KAL_TIMER_TRUE                      (1)
#define KAL_TIMER_INVALID_INDEX             (255)








/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef struct kal_timerInfo_s
{
    u8_t       h_task;                                     /* The KAL task handle.  */
    u8_t       isRecurring;
    u8_t       bitPosition;                                /* Value between 0 and 31.  */
    i8_t       nextIndex;                                  /* Next node in list.  */
    pthread_t  h_thread;                                   /* Underlying pthread thread handle.  */
    u32_t      initialTicks;
    u32_t      ticksRemaining;
}  kal_timerInfo_t;








/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
static kal_timerInfo_t  kalTimerInfo[KAL_CONFIG_TIMER_MAX_CONCURRENT];
static volatile u8_t    kalTimerUsedHead;
static volatile u8_t    kalTimerFreeHead;
static sem_t            kalTimerSemaphoreLock;
u32_t                   kalTimerMillisecondsPerTick=KAL_TIMER_MILLISECONDS_PER_TICK;
static u32_t            kalTimerTickCounter;
static u16_t            kalTimerTicksPerSecond;








/**************************************************************************************************
 *                                                                                                *
 *  Local Function Declarations                                                                   *
 *                                                                                                *
 **************************************************************************************************/
static void  KAL_TimerGetLock(void);
static void  KAL_TimerReleaseLock(void);
static void  KAL_TimerHandleExpiry(void);








/**************************************************************************************************
 *                                                                                                *
 *  KAL_SetTimer() - this routine is used to set a timer.  Upon timer expiry, the specified event *
 *                   bit will be set in the task's event mask.                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u32_t  milliseconds - the expiry time in milliseconds.                                    *
 *      u8_t   hTask        - a handle to the task which will be notified upon timer expiry.  A   *
 *                            value of 0xFF may be used to denote the current thread.             *
 *      u8_t   iEvent       - specifies the event (by bit position) to be set upon timer expiry.  *
 *      u8_t   bRecurrent   - a Boolean indicating whether the timer is recurrent (zero for       *
 *                            non-recurrent, non-zero for recurrent).                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HTASK       - upon failure when the specified task handle is invalid.        *
 *            G2_ALLOCATION_FAIL - a failure indicating an internal error when too many timers    *
 *                                 are already running, or if a repeating timer is set with a     *
 *                                 zero timeout value.                                            *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SetTimer(u32_t  milliseconds,
             u8_t   hTask,
             u8_t   iEvent,
             u8_t   bRecurrent)
{
    u8_t       retCode=G2_BAD_HTASK;
    u8_t       h_task;
    u8_t       searchIndex;
    u8_t       lastIndex;
    u8_t       newIndex;
    u8_t       found;
    u32_t      ticks;
    pthread_t  hThread;

    /* Check the parameters.  */
    if ((iEvent < (8*sizeof(kal_event_t))) && (KAL_GetThreadHandle(hTask, &hThread, &h_task) == G2_SUCCESS))
    {
        /* BAD things can happen if the caller specifies a recurring timer with a timeout of 0.
             Check for this.  */
        if ((bRecurrent) && (milliseconds == 0))
        {
            return G2_ALLOCATION_FAIL;
        }

        /* Obtain a synchronization lock.  */
        KAL_TimerGetLock();

        if (kalTimerFreeHead != KAL_TIMER_INVALID_INDEX)
        {
            /* Store known values.  */
            kalTimerInfo[kalTimerFreeHead].h_task = h_task;
            kalTimerInfo[kalTimerFreeHead].isRecurring = bRecurrent;
            kalTimerInfo[kalTimerFreeHead].bitPosition = iEvent;
            kalTimerInfo[kalTimerFreeHead].h_thread = hThread;
            ticks = (milliseconds + kalTimerMillisecondsPerTick - 1) / kalTimerMillisecondsPerTick;
            kalTimerInfo[kalTimerFreeHead].initialTicks = ticks;

            /* Traverse the list of used timers, decrementing the milliseconds as you go.  The
               ticksRemaining field is a delta from the previous timer.  */
            lastIndex = KAL_TIMER_INVALID_INDEX;
            searchIndex = kalTimerUsedHead;
            found = KAL_TIMER_FALSE;
            while ((found == KAL_TIMER_FALSE) && (searchIndex != KAL_TIMER_INVALID_INDEX))
            {
                if (ticks < kalTimerInfo[searchIndex].ticksRemaining)
                {
                    found = KAL_TIMER_TRUE;
                }
                else
                {
                    ticks -= kalTimerInfo[searchIndex].ticksRemaining;
                    lastIndex = searchIndex;
                    searchIndex = kalTimerInfo[searchIndex].nextIndex;
                }
            }

            kalTimerInfo[kalTimerFreeHead].ticksRemaining = ticks;

            /* Now, need to decrement remaining time in next timer, if extant.  */
            if (searchIndex != KAL_TIMER_INVALID_INDEX)
            {
                kalTimerInfo[searchIndex].ticksRemaining -= kalTimerInfo[kalTimerFreeHead].ticksRemaining;
            }

            /* Perform linkage.  */
            if (lastIndex == KAL_TIMER_INVALID_INDEX)
            {
                /* Add to the beginning of the list.  */
                kalTimerUsedHead = kalTimerFreeHead;
                kalTimerFreeHead = kalTimerInfo[kalTimerFreeHead].nextIndex;
                kalTimerInfo[kalTimerUsedHead].nextIndex = searchIndex;
            }
            else
            {
                /* Insert timer in list.  */
                newIndex = kalTimerFreeHead;
                kalTimerInfo[lastIndex].nextIndex = newIndex;
                kalTimerFreeHead = kalTimerInfo[newIndex].nextIndex;
                kalTimerInfo[newIndex].nextIndex = searchIndex;
            }

            retCode = G2_SUCCESS;
        }
        else
        {
            retCode = G2_ALLOCATION_FAIL;
        }

        /* Release the synchronization lock.  */
        KAL_TimerReleaseLock();
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_CancelTimer() - this routine is used to cancel a timer.                                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  hTask  - a handle to the task which was to be notified upon timer expiry.  A value  *
 *                     of 0xFF may be used to denote the current thread.                          *
 *      u8_t  iEvent - specifies the event (by bit position) which was to be set upon timer       *
 *                     expiry.                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS   - upon success.                                                        *
 *            G2_BAD_HTASK - upon failure when the specified task handle is invalid.              *
 *            G2_NOT_FOUND - indicates that the timer could not be found, perhaps due to expiry.  *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_CancelTimer(u8_t   hTask,
                u8_t   iEvent)
{
    u8_t       searchIndex;
    u8_t       lastIndex;
    u8_t       found;
    u8_t       retCode=G2_BAD_HTASK;
    pthread_t  searchThread;

    /* Check the parameters.  */
    if ((iEvent < (8*sizeof(kal_event_t))) && (KAL_GetThreadHandle(hTask, &searchThread, &hTask) == G2_SUCCESS))
    {
        /* Obtain a synchronization lock.  */
        KAL_TimerGetLock();

        /* Search the timer list for the desired timer.  */
        lastIndex = KAL_TIMER_INVALID_INDEX;
        searchIndex = kalTimerUsedHead;
        found = KAL_TIMER_FALSE;
        while ((found == KAL_TIMER_FALSE) && (searchIndex != KAL_TIMER_INVALID_INDEX))
        {
            if ((pthread_equal(kalTimerInfo[searchIndex].h_thread, searchThread) != 0) &&
                (kalTimerInfo[searchIndex].bitPosition == iEvent))
            {
                found = KAL_TIMER_TRUE;
            }
            else
            {
                lastIndex = searchIndex;
                searchIndex = kalTimerInfo[searchIndex].nextIndex;
            }
        }

        if (found == KAL_TIMER_TRUE)
        {
            retCode = G2_SUCCESS;

            /* Remove the timer from the list and place in front of the free list.  Also, since
               a node is being removed, its ticksRemaining must be added to the next node, if
               extant.  */
            if (lastIndex == KAL_TIMER_INVALID_INDEX)
            {
                kalTimerUsedHead = kalTimerInfo[searchIndex].nextIndex;
                if (kalTimerUsedHead != KAL_TIMER_INVALID_INDEX)
                {
                    kalTimerInfo[kalTimerUsedHead].ticksRemaining +=
                        kalTimerInfo[searchIndex].ticksRemaining;
                }
            }
            else
            {
                kalTimerInfo[lastIndex].nextIndex = kalTimerInfo[searchIndex].nextIndex;
                if (kalTimerInfo[searchIndex].nextIndex != KAL_TIMER_INVALID_INDEX)
                {
                    kalTimerInfo[kalTimerInfo[searchIndex].nextIndex].ticksRemaining +=
                        kalTimerInfo[searchIndex].ticksRemaining;
                }
            }
            kalTimerInfo[searchIndex].nextIndex = kalTimerFreeHead;
            kalTimerFreeHead = searchIndex;

            /* Just to be clean, clear other fields in the removed timer node.  */
            kalTimerInfo[searchIndex].h_task = 0;
            kalTimerInfo[searchIndex].isRecurring = KAL_TIMER_FALSE;
            kalTimerInfo[searchIndex].bitPosition = 0;
            kalTimerInfo[searchIndex].initialTicks = 0;
            kalTimerInfo[searchIndex].ticksRemaining = 0;
        }
        else
        {
            retCode = G2_NOT_FOUND;
        }

        /* Release the synchronization lock.  */
        KAL_TimerReleaseLock();
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_Wait() - this routine is used to suspend the invoking thread for the specified time in    *
 *               order to let the other tasks run.                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u32_t  milliseconds - the number of milliseconds to suspend the calling thread.           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_TIMEOUT - successful return.                                                     *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_Wait(u32_t  milliseconds)
{
    struct timespec  timeout;

    /* No lock needed since each thread only affects itself.  */
    timeout.tv_sec  = (time_t)(milliseconds / 1000);
    timeout.tv_nsec = (long)((milliseconds - (timeout.tv_sec * 1000)) * 1000000);
    while ((nanosleep(&timeout, &timeout) == -1) && (errno == EINTR))
    {
        /* Do nothing.  The operation was interrupted, so go back to sleep.  Note: the timeout
             is updated with the remaining time when the interrupt occurs.  */
    }

    return G2_TIMEOUT;
}




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
void
KAL_TimerInit(void)
{
    u8_t  index;

    /* Initialize the timer lists.  */
    for (index=0; index<KAL_CONFIG_TIMER_MAX_CONCURRENT; index++)
    {
        kalTimerInfo[index].h_task = 0;
        kalTimerInfo[index].isRecurring = KAL_TIMER_FALSE;
        kalTimerInfo[index].bitPosition = 0;
        kalTimerInfo[index].nextIndex = index + 1;

        /* Since the underlying pthread_t type is opaque, a "safe" assignment method must be
             used.  */
        memset(&kalTimerInfo[index].h_thread, 0, sizeof(kalTimerInfo[index].h_thread));
        kalTimerInfo[index].initialTicks = 0;
        kalTimerInfo[index].ticksRemaining = 0;
    }
    kalTimerInfo[KAL_CONFIG_TIMER_MAX_CONCURRENT-1].nextIndex = KAL_TIMER_INVALID_INDEX;
    kalTimerUsedHead = KAL_TIMER_INVALID_INDEX;
    kalTimerFreeHead = 0;

    /* Create a binary semaphore to lock routines from concurrent access.  */
    sem_init(&kalTimerSemaphoreLock, 0, 1);

    /* Initialize the tick counter and calculate the number of ticks per second.  */
    kalTimerTickCounter = 0;
    kalTimerTicksPerSecond = (u16_t)((1000 + kalTimerMillisecondsPerTick - 1) /
                             (kalTimerMillisecondsPerTick));
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TimerGetLock() - this routine obtains a semaphore lock across KAL Timer routines to       *
 *                       prevent concurrent accesses.  The Set and Cancel API routines will       *
 *                       obtain the lock upon entry, and release the lock on exit.                *
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
KAL_TimerGetLock(void)
{
    while ((sem_wait(&kalTimerSemaphoreLock) == -1) && (errno == EINTR))
    {
        /* Do nothing.  This is done in case the semaphore call returned due to a signal.  */
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TimerReleaseLock() - this routine releases a semaphore lock across the KAL Timer          *
 *                           routines.  It is important that a call to this routine must follow a *
 *                           prior call to KAL_TimerGetLock().                                    *
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
KAL_TimerReleaseLock(void)
{
    sem_post(&kalTimerSemaphoreLock);
}



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
void
KAL_TimerHandleTick(void)
{
    kalTimerTickCounter++;

    /* Obtain lock.  */
    KAL_TimerGetLock();

    if (kalTimerUsedHead != KAL_TIMER_INVALID_INDEX)
    {
        /* Because there's nothing preventing the user from setting a 0 ms timer, we must handle
             any zero timers before decrementing the first non-zero timer.  */
        while ((kalTimerUsedHead != KAL_TIMER_INVALID_INDEX) &&
               (kalTimerInfo[kalTimerUsedHead].ticksRemaining == 0))
        {
            /* The KAL_TimerHandleExpiry() routine will automatically update the kalTimerUsedHead
                 variable.  */
            KAL_TimerHandleExpiry();
        }

        if (kalTimerUsedHead != KAL_TIMER_INVALID_INDEX)
        {
            /* Now, decrement the timer.  */
            kalTimerInfo[kalTimerUsedHead].ticksRemaining--;

            /* Handle additional timer expiries.  */
            while ((kalTimerUsedHead != KAL_TIMER_INVALID_INDEX) &&
                   (kalTimerInfo[kalTimerUsedHead].ticksRemaining == 0))
            {
                /* The KAL_TimerHandleExpiry() routine will automatically update the
                     kalTimerUsedHead variable.  */
                KAL_TimerHandleExpiry();
            }
        }
    }

    /* Release lock.  */
    KAL_TimerReleaseLock();

    /* Now, check for alarm expiries.  This check occurs only once per second (roughly).*/
    if ((kalTimerTickCounter % kalTimerTicksPerSecond) == 0)
    {
        KAL_AlarmHandleAlarms();
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_TimerHandleExpiry() - this routine is used to handle a single timer expiry at the head of *
 *                            the used timer list.  It should only be invoked by                  *
 *                            KAL_TimerTickAccounting() and under the semaphore lock.  As part of *
 *                            the handling, the used and free lists may be altered.               *
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
KAL_TimerHandleExpiry(void)
{
    u8_t  searchIndex;
    u8_t  lastIndex;
    u8_t  temp;
    u8_t  found;

    /* Do NOT lock since the caller maintains the lock.  */

    /* First, handle all of the task control.  The call should not fail, but even if it does,
         there's not much that can be done.  */
    u64_t bitPosition = 1ULL << kalTimerInfo[kalTimerUsedHead].bitPosition;
    KAL_TaskSetEvent(kalTimerInfo[kalTimerUsedHead].h_task, bitPosition);

    /* Next, see if the timer is recurring.  */
    if (kalTimerInfo[kalTimerUsedHead].isRecurring != 0)
    {
        temp = kalTimerUsedHead;
        kalTimerUsedHead = kalTimerInfo[temp].nextIndex;
        kalTimerInfo[temp].ticksRemaining = kalTimerInfo[temp].initialTicks;

        /* Recurring timer is simply reinserted into the timer list, so a search is necessary.  */
        lastIndex = KAL_TIMER_INVALID_INDEX;
        searchIndex = kalTimerUsedHead;
        found = KAL_TIMER_FALSE;
        while ((found == KAL_TIMER_FALSE) && (searchIndex != KAL_TIMER_INVALID_INDEX))
        {
            if (kalTimerInfo[temp].ticksRemaining < kalTimerInfo[searchIndex].ticksRemaining)
            {
                found = KAL_TIMER_TRUE;
            }
            else
            {
                kalTimerInfo[temp].ticksRemaining -= kalTimerInfo[searchIndex].ticksRemaining;
                lastIndex = searchIndex;
                searchIndex = kalTimerInfo[searchIndex].nextIndex;
            }
        }

        /* If there is a timer after the insertion point, its remaining time needs to be
             decremented.  */
        if (searchIndex != KAL_TIMER_INVALID_INDEX)
        {
            kalTimerInfo[searchIndex].ticksRemaining -= kalTimerInfo[temp].ticksRemaining;
        }

        /* Link the existing "node" into the list.  */
        if (lastIndex == KAL_TIMER_INVALID_INDEX)
        {
            kalTimerInfo[temp].nextIndex = kalTimerUsedHead;
            kalTimerUsedHead = temp;
        }
        else
        {
            kalTimerInfo[temp].nextIndex = searchIndex;
            kalTimerInfo[lastIndex].nextIndex = temp;
        }
    }
    else
    {
        /* The timer is not recurring, so simply return it to the free list.  */
        kalTimerInfo[kalTimerUsedHead].h_task = 0;
        kalTimerInfo[kalTimerUsedHead].isRecurring = KAL_TIMER_FALSE;
        kalTimerInfo[kalTimerUsedHead].bitPosition = 0;

        /* Since the underlying pthread_t type is opaque, a "safe" assignment method must be
             used.  */
        memset(&(kalTimerInfo[kalTimerUsedHead].h_thread), 0, sizeof(kalTimerInfo[kalTimerUsedHead].h_thread));
        kalTimerInfo[kalTimerUsedHead].initialTicks = 0;
        kalTimerInfo[kalTimerUsedHead].ticksRemaining = 0;

        /* Finally, change the linkage of the used and free lists.  */
        temp = kalTimerUsedHead;
        kalTimerUsedHead = kalTimerInfo[temp].nextIndex;
        kalTimerInfo[temp].nextIndex = kalTimerFreeHead;
        kalTimerFreeHead = temp;
    }

    /* Do NOT unlock since the caller maintains the lock.  */
}




