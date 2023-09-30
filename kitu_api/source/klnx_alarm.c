/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  klnx_alarm.c                                                                                  *
 *                                                                                                *
 *  This file contains the implementation of the  Kernel Abstraction Layer Alarm package.  It     *
 *  contains symbolic constants, type definitions, and function implementations.  Although        *
 *  similar to timers, alarms are distinctly different in the following ways:                     *
 *                                                                                                *
 *      1) Alarms occur at (or after) an absolute UTC time relative to an external clock.         *
 *      2) Alarms are not repeating.  To create a repeating alarm, the new absolute UTC time(s)   *
 *         must be recalculated by the caller.                                                    *
 *                                                                                                *
 *  Alarms are placed on queues "owned" by the external clock, and the queue is sorted by expiry  *
 *  time.  The periodic timer tick, from the KAL_TimerHandleTick() routine, is used to check for  *
 *  both alarm expiries and changes to any external clocks.                                       *
 *                                                                                                *
 **************************************************************************************************/


#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include "kal_api.h"
#include "kal_config.h"
#include "klnx_internal.h"








/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_ALARM_FALSE                                      (0)
#define KAL_ALARM_TRUE                                       (1)








/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef struct kal_alarmInfo_s
{
    kal_time_t  expiryTime;
    u8_t        h_task;
    u8_t        bitPosition;
    i8_t        nextIndex;
}  kal_alarmInfo_t;








/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
static kal_alarmInfo_t  kalAlarmInfo[KAL_CONFIG_ALARM_MAX_ALARMS];
static volatile u8_t    kalAlarmFreeHead = KAL_ALARM_INVALID_INDEX;
static sem_t            kalAlarmSemaphoreLock;








/**************************************************************************************************
 *                                                                                                *
 *  Local Function Declarations                                                                   *
 *                                                                                                *
 **************************************************************************************************/
static void  KAL_AlarmGetLock(void);
static void  KAL_AlarmReleaseLock(void);
#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)
static void  KAL_AlarmRemoveQueue(i8_t* const  p_queueHead);
static void  KAL_AlarmHandleQueueExpiries(i8_t* const       p_queueHead,
                                          const kal_time_t  currentTime);
#endif //#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)








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
#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)
void
KAL_AlarmInit(void)
{
    u8_t  index;

    for (index=0; index<KAL_CONFIG_ALARM_MAX_ALARMS; index++)
    {
        kalAlarmInfo[index].expiryTime = 0;
        kalAlarmInfo[index].h_task = 0;
        kalAlarmInfo[index].bitPosition = 0;
        kalAlarmInfo[index].nextIndex = (i8_t)(index + 1u);
    }
    kalAlarmInfo[KAL_CONFIG_ALARM_MAX_ALARMS-1].nextIndex = KAL_ALARM_INVALID_INDEX;
    kalAlarmFreeHead = 0;

    /* Create a binary semaphore to lock routines from concurrent access.  */
    sem_init(&kalAlarmSemaphoreLock, 0, 1);
}
#else
void KAL_AlarmInit(void) {}
#endif //#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)


/**************************************************************************************************
 *                                                                                                *
 *  KAL_SetAlarm() - this routine sets an alarm.  Alarms are absolute UTC times relative to an    *
 *                   external clock.  When an alarm expires, a specified event will be set for a  *
 *                   specified task.                                                              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_time_t  utcTime       - the absolute UTC time for the alarm expiry.  If set in the    *
 *                                  past, the alarm will expire on the next "tick".               *
 *      u8_t        hTask         - a handle to the task which will be notified upon alarm        *
 *                                  expiry.  A value of 0xFF may be used to denote the current    *
 *                                  thread.                                                       *
 *      u8_t        iEvent        - specifies the event (by bit position) to be set upon alarm    *
 *                                  expiry.                                                       *
 *      u8_t        extClockIndex - the index of the external clock reference.  This may be set   *
 *                                  from 0 to (KAL_CONFIG_TIMEBASE_MAX_TIMEBASES-1).  The value   *
 *                                  of KAL_TIMEBASE_NETWORK_CLOCK is the index of the network     *
 *                                  clock.                                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_FAILURE         - upon failure when there is an invalid event or invalid         *
 *                                 external clock index.                                          *
 *            G2_BAD_HTASK       - upon failure when the task handle is invalid.                  *
 *            G2_ALLOCATION_FAIL - a failure indicating an internal error when too many alarms    *
 *                                 are running.                                                   *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SetAlarm(kal_time_t  utcTime,
             u8_t        hTask,
             u8_t        iEvent,
             u8_t        extClockIndex)
{
    u8_t       retCode=G2_BAD_HTASK;
    u8_t       h_task;
    u8_t       searchIndex;
    u8_t       lastIndex;
    u8_t       newIndex;
    pthread_t  hThread;

    /* Check the parameters.  */
    if (KAL_GetThreadHandle(hTask, &hThread, &h_task) == G2_SUCCESS)
    {
        if ((iEvent < (8*sizeof(kal_event_t))) && (extClockIndex < KAL_CONFIG_TIMEBASE_MAX_TIMEBASES))
        {
            /* Obtain the synchronization lock.  */
            KAL_AlarmGetLock();

            /* Check if the external clock has been initialized.  If not, the alarm will not be
                 set.  */
            if (kalTimebaseExtClocks[extClockIndex].inUse == true)
            {
                /* Check if there is an alarm available.  */
                if (kalAlarmFreeHead != KAL_ALARM_INVALID_INDEX)
                {
                    /* Populate the alarm data.  */
                    kalAlarmInfo[kalAlarmFreeHead].expiryTime = utcTime;
                    kalAlarmInfo[kalAlarmFreeHead].h_task = h_task;
                    kalAlarmInfo[kalAlarmFreeHead].bitPosition = iEvent;

                    /* Search the alarm queue for the correct position to place the alarm.  */
                    lastIndex = KAL_ALARM_INVALID_INDEX;
                    searchIndex = kalTimebaseExtClocks[extClockIndex].alarmQueueIndex;
                    while ((searchIndex != KAL_ALARM_INVALID_INDEX) &&
                           (kalAlarmInfo[searchIndex].expiryTime <= utcTime))
                    {
                        lastIndex = searchIndex;
                        searchIndex = kalAlarmInfo[searchIndex].nextIndex;
                    }

                    /* Check to see if we need to insert at the beginning of the queue.  */
                    if (lastIndex == KAL_ALARM_INVALID_INDEX)
                    {
                        newIndex = kalAlarmFreeHead;
                        kalTimebaseExtClocks[extClockIndex].alarmQueueIndex = newIndex;
                        kalAlarmFreeHead = kalAlarmInfo[kalAlarmFreeHead].nextIndex;
                        kalAlarmInfo[newIndex].nextIndex = searchIndex;
                    }
                    else
                    {
                        newIndex = kalAlarmFreeHead;
                        kalAlarmInfo[lastIndex].nextIndex = kalAlarmFreeHead;
                        kalAlarmFreeHead = kalAlarmInfo[kalAlarmFreeHead].nextIndex;
                        kalAlarmInfo[newIndex].nextIndex = searchIndex;
                    }

                    retCode = G2_SUCCESS;
                }
                else
                {
                    retCode = G2_ALLOCATION_FAIL;
                }
            }
            else
            {
                retCode = G2_FAILURE;
            }

            /* Release the synchronization lock.  */
            KAL_AlarmReleaseLock();
        }
        else
        {
            retCode = G2_FAILURE;
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_CancelAlarm() - this routine cancels an alarm which was set from a prior call to          *
 *                      KAL_SetAlarm().                                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_time_t  utcTime       - the absolute UTC time for the alarm expiry.                   *
 *      u8_t        hTask         - a handle to the task which was to be notified upon alarm      *
 *                                  expiry.  A value of 0xFF may be used to denote the current    *
 *                                  thread.                                                       *
 *      u8_t        iEvent        - specifies the event (by bit position) which was to be set     *
 *                                  upon alarm expiry.                                            *
 *      u8_t        extClockIndex - the index of the external clock reference.  This may be set   *
 *                                  from 0 to (KAL_CONFIG_TIMEBASE_MAX_TIMEBASES-1).  The value   *
 *                                  of KAL_TIMEBASE_NETWORK_CLOCK is the index of the network     *
 *                                  clock.                                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS   - upon success.                                                        *
 *            G2_FAILURE   - upon failure when there is an invalid external clock index or the    *
 *                           event is invalid.                                                    *
 *            G2_BAD_HTASK - upon failure when the task handle is invalid.                        *
 *            G2_NOT_FOUND - indicates that the alarm could not be found, perhaps due to expiry.  *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_CancelAlarm(kal_time_t  utcTime,
                u8_t        hTask,
                u8_t        iEvent,
                u8_t        extClockIndex)
{
    u8_t       lastIndex;
    u8_t       searchIndex;
    u8_t       found;
    u8_t       retCode=(u8_t)G2_BAD_HTASK;
    u8_t       h_task;
    pthread_t  hThread;

    /* Check the parameters.  */
    if (KAL_GetThreadHandle(hTask, &hThread, &h_task) == G2_SUCCESS)
    {
        if ((iEvent < (8*sizeof(kal_event_t))) && 
            (extClockIndex < KAL_CONFIG_TIMEBASE_MAX_TIMEBASES))
        {
            /* Obtain the synchronization lock.  */
            KAL_AlarmGetLock();

            /* Check if the external clock has been initialized.  If not, there are no alarms for
               the external clock.  */
            if (kalTimebaseExtClocks[extClockIndex].inUse == true)
            {
                /* Search the alarm queue for the desired alarm.  */
                lastIndex = KAL_ALARM_INVALID_INDEX;
                searchIndex = kalTimebaseExtClocks[extClockIndex].alarmQueueIndex;
                found = KAL_ALARM_FALSE;
                while ((found == KAL_ALARM_FALSE) && (searchIndex != KAL_ALARM_INVALID_INDEX))
                {
                    if ((kalAlarmInfo[searchIndex].expiryTime == utcTime) &&
                        (kalAlarmInfo[searchIndex].h_task == h_task) &&
                        (kalAlarmInfo[searchIndex].bitPosition == iEvent))
                    {
                        found = KAL_ALARM_TRUE;
                    }
                    else
                    {
                        lastIndex = searchIndex;
                        searchIndex = kalAlarmInfo[searchIndex].nextIndex;
                    }
                }

                if (found == KAL_ALARM_TRUE)
                {
                    /* Clear the fields of the removed alarm.  */
                    kalAlarmInfo[searchIndex].expiryTime = 0;
                    kalAlarmInfo[searchIndex].h_task = 0;
                    kalAlarmInfo[searchIndex].bitPosition = 0;

                    /* Remove the alarm from its queue.  */
                    if (lastIndex == KAL_ALARM_INVALID_INDEX)
                    {
                        kalTimebaseExtClocks[extClockIndex].alarmQueueIndex =
                            kalAlarmInfo[searchIndex].nextIndex;
                    }
                    else
                    {
                        kalAlarmInfo[lastIndex].nextIndex = kalAlarmInfo[searchIndex].nextIndex;
                    }

                    /* Place the removed alarm at the from of the free list.  */
                    kalAlarmInfo[searchIndex].nextIndex = kalAlarmFreeHead;
                    kalAlarmFreeHead = searchIndex;

                    retCode = G2_SUCCESS;
                }
                else
                {
                    retCode = G2_NOT_FOUND;
                }
            }
            else
            {
                retCode = G2_FAILURE;
            }

            /* Release the synchronization lock.  */
            KAL_AlarmReleaseLock();
        }
        else
        {
            retCode = G2_FAILURE;
        }
    }

    return retCode;
}




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
#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)
void
KAL_AlarmHandleAlarms(void)
{
    u8_t        extClockIndex;
    kal_time_t  relativeTime;

    /* First, handle all potential adjustments to the external clock timebases.  If a value of
         KAL_TIMEBASE_RETIRE_EXT_CLOCK_VALUE was set for an external clock, it is an indication 
         that the external clock is to be reinitialized (i.e., it is retired and can be reused).  */
    for (extClockIndex=0; extClockIndex < KAL_CONFIG_TIMEBASE_MAX_TIMEBASES; extClockIndex++)
    {
        KAL_GetExtClockLock(extClockIndex);
        if (kalTimebaseExtClocks[extClockIndex].useNewUtcTime)
        {
            kalTimebaseExtClocks[extClockIndex].useNewUtcTime = KAL_ALARM_FALSE;
            if (kalTimebaseExtClocks[extClockIndex].newUtcTime == KAL_TIMEBASE_RETIRE_EXT_CLOCK_VALUE)
            {
                /* Make an additional check that the clock to be retired is not the network
                     clock.  */
                if (extClockIndex != KAL_TIMEBASE_NETWORK_CLOCK)
                {
                    /* The external clock is to be marked as unused.  Any alarms on its queue will
                         be removed silently.  */
                    kalTimebaseExtClocks[extClockIndex].inUse = false;
                    memset(&kalTimebaseExtClocks[extClockIndex].timebase.timeClockWasSetM, 0,
                           sizeof(struct timespec));
                    kalTimebaseExtClocks[extClockIndex].timebase.clockValue = 0;
                    kalTimebaseExtClocks[extClockIndex].newUtcTime = 0;
                    memset(&kalTimebaseExtClocks[extClockIndex].timeNewUtcWasSetM, 0,
                           sizeof(struct timespec));
                    KAL_AlarmRemoveQueue(&(kalTimebaseExtClocks[extClockIndex].alarmQueueIndex));
                }
                else
                {
                    /* The network clock was scheduled to be deleted, but this cannot be done.
                         Print a warning message.  */
                    // TBD - HT: print/log a warning message.
                    fprintf(stderr, "WARNING: attempt was made to retire the network clock.\n");

                    /* Clear the adjustment variables.  */
                    kalTimebaseExtClocks[extClockIndex].newUtcTime = 0;
                    memset(&kalTimebaseExtClocks[extClockIndex].timeNewUtcWasSetM, 0,
                           sizeof(struct timespec));
                }
            }
            else
            {
                /* Adjust the external clock.  */
                kalTimebaseExtClocks[extClockIndex].inUse = true;
                kalTimebaseExtClocks[extClockIndex].timebase.timeClockWasSetM =
                                  kalTimebaseExtClocks[extClockIndex].timeNewUtcWasSetM;
                kalTimebaseExtClocks[extClockIndex].timebase.clockValue =
                                  kalTimebaseExtClocks[extClockIndex].newUtcTime;

                /* Clear the adjustment variables.  */
                kalTimebaseExtClocks[extClockIndex].newUtcTime = 0;
                memset(&kalTimebaseExtClocks[extClockIndex].timeNewUtcWasSetM, 0,
                       sizeof(struct timespec));
            }
        }
        KAL_ReleaseExtClockLock(extClockIndex);
    }

    /* Next, check for alarm expiries.  */
    for (extClockIndex=0; extClockIndex < KAL_CONFIG_TIMEBASE_MAX_TIMEBASES; extClockIndex++)
    {
        if (kalTimebaseExtClocks[extClockIndex].inUse)
        {
            relativeTime = KAL_GetExtClock(extClockIndex);
            KAL_AlarmHandleQueueExpiries(&(kalTimebaseExtClocks[extClockIndex].alarmQueueIndex),
                                         relativeTime);
        }
    }
}
#else
void KAL_AlarmHandleAlarms(void) {}
#endif //#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)



/**************************************************************************************************
 *                                                                                                *
 *  KAL_AlarmHandleQueueExpiries() - this routine, given the head of an alarm queue and the       *
 *                                   (relative) current time, will handle alarm expiries for the  *
 *                                   entire queue.                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      i8_t* const       p_queueHead - a pointer to an index which is the head of an alarm       *
 *                                      queue (array-based list).                                 *
 *      const kal_time_t  currentTime - the current time relative to the external clock.          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)
static void
KAL_AlarmHandleQueueExpiries(i8_t* const       p_queueHead,
                             const kal_time_t  currentTime)
{
    u8_t  searchIndex;
    u8_t  lastIndex;
    u8_t  iterateIndex;

    KAL_AlarmGetLock();

    /* Search for the first alarm *after* the current time.  */
    lastIndex = KAL_ALARM_INVALID_INDEX;
    searchIndex = *p_queueHead;
    while ((searchIndex != KAL_ALARM_INVALID_INDEX) &&
           (kalAlarmInfo[searchIndex].expiryTime <= currentTime))
    {
        lastIndex = searchIndex;
        searchIndex = kalAlarmInfo[searchIndex].nextIndex;
    }

    /* All alarms from the head up to the lastIndex (inclusive) have expired.  If no alarms have
         expired, then the lastIndex will be invalid.  */
    if (lastIndex != KAL_ALARM_INVALID_INDEX)
    {
        iterateIndex = *p_queueHead;
        while (iterateIndex != searchIndex)
        {
            KAL_TaskSetEvent(kalAlarmInfo[iterateIndex].h_task,
                             (1ULL<<kalAlarmInfo[iterateIndex].bitPosition));

            /* Clear out the information after the notification.  */
            kalAlarmInfo[iterateIndex].expiryTime = 0;
            kalAlarmInfo[iterateIndex].h_task = 0;
            kalAlarmInfo[iterateIndex].bitPosition = 0;

            iterateIndex = kalAlarmInfo[iterateIndex].nextIndex;
        }

        /* Now, all alarms from the head up to (but not including) the searchIndex must be
             freed.  */
        kalAlarmInfo[lastIndex].nextIndex = kalAlarmFreeHead;
        kalAlarmFreeHead = *p_queueHead;
        *p_queueHead = searchIndex;
    }

    KAL_AlarmReleaseLock();
}
#endif //#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)




/**************************************************************************************************
 *                                                                                                *
 *  KAL_AlarmRemoveQueue() - this private routine will, given the head of an alarm queue, remove  *
 *                           all alarms and set the head to the equivalent of NULL.               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      i8_t* const  p_queueHead - a pointer to an index which is the head of an alarm queue.     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)
static void
KAL_AlarmRemoveQueue(i8_t* const p_queueHead)
{
    u8_t  currentIndex;
    u8_t  lastIndex;

    KAL_AlarmGetLock();
    lastIndex = KAL_ALARM_INVALID_INDEX;
    currentIndex = *p_queueHead;
    while (currentIndex != KAL_ALARM_INVALID_INDEX)
    {
        /* Clear all fields but not the linkage.  The entire queue will be returned to the free
             list all at once.  */
        kalAlarmInfo[currentIndex].expiryTime = 0;
        kalAlarmInfo[currentIndex].h_task = 0;
        kalAlarmInfo[currentIndex].bitPosition = 0;

        /* Advance pointers.  */
        lastIndex = currentIndex;
        currentIndex = kalAlarmInfo[currentIndex].nextIndex;
    }

    /* Now, link the queue back into the free list.  */
    if (lastIndex != KAL_ALARM_INVALID_INDEX)
    {
        kalAlarmInfo[lastIndex].nextIndex = kalAlarmFreeHead;
        kalAlarmFreeHead = *p_queueHead;
    }

    *p_queueHead = KAL_ALARM_INVALID_INDEX;
    KAL_AlarmReleaseLock();
}
#endif //#if (KAL_CONFIG_ALARM_MAX_ALARMS > 0)




/**************************************************************************************************
 *                                                                                                *
 *  KAL_AlarmGetLock() - this routine obtains a semaphore lock across KAL Alarm routines to       *
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
KAL_AlarmGetLock(void)
{
    while ((sem_wait(&kalAlarmSemaphoreLock) == -1) && (errno == EINTR))
    {
        /* Do nothing.  This is done in case the semaphore call returned due to a signal.  */
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_AlarmReleaseLock() - this routine releases a semaphore lock across the KAL Alarm          *
 *                           routines.  It is important that a call to this routine must follow a *
 *                           prior call to KAL_AlarmGetLock().                                    *
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
KAL_AlarmReleaseLock(void)
{
    sem_post(&kalAlarmSemaphoreLock);
}



