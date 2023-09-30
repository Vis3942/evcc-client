/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  klnx_timebase.c                                                                               *
 *                                                                                                *
 *  This file contains the implementation of the time base of the Kernel Abstraction Layer.       *
 *  Linux keeps its own time base (from January 1, 1970), and conversions may be required for     *
 *  having KAL keep its own time bases (e.g., from January 1, 2000).  Part of the functionality   *
 *  of this module is support for multiple external timebases.                                    *
 *                                                                                                *
 **************************************************************************************************/


#include <stdbool.h>
#include <semaphore.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "psal_types.h"
#include "kal_api.h"
#include "kal_config.h"
#include "klnx_internal.h"








/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef struct kal_intTimebase_s
{
    struct timespec  startupTimeM;      /* Relative to the monotonic clock.  */
    struct timespec  startupTimeR;      /* Relative to the real-time clock.  */
}  kal_intTimebase_t;








/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
static kal_intTimebase_t  kalTimebaseSystemTime;
kal_extClock_t            kalTimebaseExtClocks[KAL_CONFIG_TIMEBASE_MAX_TIMEBASES];








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
void
KAL_TimebaseInit(void)
{
    u8_t  index;

    /* Initialize the internal system time base.  */
    clock_gettime(CLOCK_MONOTONIC, &kalTimebaseSystemTime.startupTimeM);
    clock_gettime(CLOCK_REALTIME, &kalTimebaseSystemTime.startupTimeR);

    /* Initialize the external time bases.  */
    for (index=0; index<KAL_CONFIG_TIMEBASE_MAX_TIMEBASES; index++)
    {
        /* Create a binary semaphore to lock accesses to each external clock.  */
        sem_init(&kalTimebaseExtClocks[index].extClockLock, 0, 1);

        /* Mark all external timebases as unused.  This includes the network clock.  */
        kalTimebaseExtClocks[index].inUse = false;

        /* Zero out the external clock values.  */
        memset(&kalTimebaseExtClocks[index].timebase.timeClockWasSetM, 0,
               sizeof(struct timespec));
        kalTimebaseExtClocks[index].timebase.clockValue = 0;

        /* Zero out the fields used to set the external clock to a new value.  */
        kalTimebaseExtClocks[index].newUtcTime = 0;
        memset(&kalTimebaseExtClocks[index].timeNewUtcWasSetM, 0, sizeof(struct timespec));
        kalTimebaseExtClocks[index].useNewUtcTime = 0;

        /* Mark the alarm queue for the external clock as invalid.  */
        kalTimebaseExtClocks[index].alarmQueueIndex = KAL_ALARM_INVALID_INDEX;
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_GetMilliseconds() - this routine returns the number of milliseconds since startup.        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      i64_t - the number of seconds since startup.                                              *
 *                                                                                                *
 **************************************************************************************************/
i64_t
KAL_GetMilliseconds(void)
{
    struct timespec  currentTime;
    i64_t            numMs;

    /* There's no check that the current time is after the system time because they both use the
         monotonic clock (which must always increase as time progresses).  */
    clock_gettime(CLOCK_MONOTONIC, &currentTime);

    /* If necessary, alter the value of nanoseconds in order to prevent an underflow, thereby
         simplifying the calculation later on.  */
    if (currentTime.tv_nsec < kalTimebaseSystemTime.startupTimeM.tv_nsec)
    {
        currentTime.tv_nsec += 1000000000;
        currentTime.tv_sec--;
    }

    numMs = (((currentTime.tv_sec - kalTimebaseSystemTime.startupTimeM.tv_sec) * 1000) +
            ((currentTime.tv_nsec - kalTimebaseSystemTime.startupTimeM.tv_nsec)/1000000)); 

    return numMs;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SetClock() - this routine sets the clock which is the number of elapsed seconds since     *
 *                   January 1, 1970.                                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_time_t  utcTime - the number of elapsed seconds since January 1, 1970.                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_SetClock(kal_time_t  utcTime)
{
    KAL_SetExtClock(utcTime, KAL_TIMEBASE_NETWORK_CLOCK);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_GetClock() - this routine returns the number of seconds that have elapsed since January   *
 *                   1, 1970.                                                                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      kal_time_t - the number of elapsed seconds since January 1, 1970.                         *
 *                                                                                                *
 **************************************************************************************************/
kal_time_t
KAL_GetClock(void)
{
    return KAL_GetExtClock(KAL_TIMEBASE_NETWORK_CLOCK);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_GetExtClockLock() - this routine is used to obtain the lock for a specified external      *
 *                          clock.                                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  extClockIndex - the index of the desired external clock.  This should be a value    *
 *                            between 0 and (KAL_CONFIG_TIMEBASE_MAX_TIMEBASES-1).  The value zero, or   *
 *                            KAL_TIMEBASE_NETWORK_CLOCK, is the network clock.                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_GetExtClockLock(u8_t  extClockIndex)
{
    if (extClockIndex < KAL_CONFIG_TIMEBASE_MAX_TIMEBASES)
    {
        while ((sem_wait(&kalTimebaseExtClocks[extClockIndex].extClockLock) == -1) &&
               (errno == EINTR))
        {
            /* Do nothing.  This is done in case the semaphore call returned due to a signal.  */
        }
    }
}




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
 *                            between 0 and (KAL_CONFIG_TIMEBASE_MAX_TIMEBASES-1).  The value zero, or   *
 *                            KAL_TIMEBASE_NETWORK_CLOCK, is the network clock.                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_ReleaseExtClockLock(u8_t  extClockIndex)
{
    if (extClockIndex < KAL_CONFIG_TIMEBASE_MAX_TIMEBASES)
    {
        sem_post(&kalTimebaseExtClocks[extClockIndex].extClockLock);
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SetExtClock() - this routine sets an external clock which is the number of elapsed        *
 *                      seconds since January 1, 1970.                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_time_t  newUtcTime - the number of elapsed seconds since January 1, 1970.  If this    *
 *                               value is KAL_TIMEBASE_RETIRE_EXT_CLOCK_VALUE, then the external  *
 *                               clock will be marked for retirement (which happens under the     *
 *                               context of the periodic tick when handling alarms in             *
 *                               klnx_alarm.c) - as long as it is not the network clock.          *
 *      u8_t   extClockIndex   - the index of the desired external clock.  This should be a value *
 *                               between 0 and (KAL_CONFIG_TIMEBASE_MAX_TIMEBASES-1).  The value  *
 *                               zero, or KAL_TIMEBASE_NETWORK_CLOCK, is the network clock.       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_SetExtClock(kal_time_t  newUtcTime,
                u8_t        extClockIndex)

{
    if (extClockIndex < KAL_CONFIG_TIMEBASE_MAX_TIMEBASES)
    {
        KAL_GetExtClockLock(extClockIndex);
        if (newUtcTime == 0)
        {
            newUtcTime = 1;
        }
        kalTimebaseExtClocks[extClockIndex].newUtcTime = newUtcTime;
        clock_gettime(CLOCK_MONOTONIC, &kalTimebaseExtClocks[extClockIndex].timeNewUtcWasSetM);
        kalTimebaseExtClocks[extClockIndex].useNewUtcTime = 1;
        KAL_ReleaseExtClockLock(extClockIndex);
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_GetExtClock() - this routine returns the number of seconds that have elapsed since        *
 *                      January 1, 1970 for the specified external clock.  A value of zero will   *
 *                      be returned upon failure.                                                 *
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
 *      kal_time_t - the number of elapsed seconds since January 1, 1970, or                      *
 *                   KAL_TIMEBASE_INVALID_VALUE if the external clock was never initialized or if *
 *                   the external clock index was invalid.                                        *
 *                                                                                                *
 **************************************************************************************************/
kal_time_t
KAL_GetExtClock(u8_t  extClockIndex)
{
    kal_time_t       retValue=KAL_TIMEBASE_INVALID_VALUE;
    struct timespec  currentTime;

    if (extClockIndex < KAL_CONFIG_TIMEBASE_MAX_TIMEBASES)
    {
        KAL_GetExtClockLock(extClockIndex);
        if ((kalTimebaseExtClocks[extClockIndex].inUse == true) ||
            (kalTimebaseExtClocks[extClockIndex].useNewUtcTime != 0))
        {
            clock_gettime(CLOCK_MONOTONIC, &currentTime);

            /* In order to prevent blocking the periodic tick handing of alarms, it is the periodic
                 tick which performs the switchover when an external clock is updated.  However,
                 this leaves a window of time when a user calls this routine and when the switchover
                 occurs.  Therefore, check if a switchover is scheduled, and if it is, use the
                 values stored in the switchover values.  */
            if (kalTimebaseExtClocks[extClockIndex].useNewUtcTime != 0)
            {
                retValue = (kal_time_t) currentTime.tv_sec -
                           (kal_time_t) kalTimebaseExtClocks[extClockIndex].timeNewUtcWasSetM.tv_sec +
                           kalTimebaseExtClocks[extClockIndex].newUtcTime;
                if (currentTime.tv_nsec <
                    kalTimebaseExtClocks[extClockIndex].timeNewUtcWasSetM.tv_nsec)
                {
                    retValue--;
                }
            }
            else
            {
                retValue = (kal_time_t) currentTime.tv_sec -
                           (kal_time_t) kalTimebaseExtClocks[extClockIndex].timebase.timeClockWasSetM.tv_sec +
                           kalTimebaseExtClocks[extClockIndex].timebase.clockValue;
                if (currentTime.tv_nsec <
                    kalTimebaseExtClocks[extClockIndex].timebase.timeClockWasSetM.tv_nsec)
                {
                    retValue--;
                }
            }
        }
        KAL_ReleaseExtClockLock(extClockIndex);
    }

    return retValue;
}




