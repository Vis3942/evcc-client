/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  klnx_semaphore.c                                                                              *
 *                                                                                                *
 *  This file contains the Linux implementation of the Kernel Abstraction Layer Semaphore         *
 *  package.  Semaphores in this implementation use POSIX semaphores, not pthreads semaphores.    *
 *  The semaphores defined in this package is for software sitting on top of KAL.  Within KAL     *
 *  itself, POSIX semaphores are used directly.                                                   *
 *                                                                                                *
 **************************************************************************************************/


#include <semaphore.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "psal_types.h"
#include "kal_api.h"
#include "kal_config.h"







/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_SEMAPHORE_FALSE                     (0)
#define KAL_SEMAPHORE_TRUE                      (1)
#define KAL_SEMAPHORE_LOW_GUARD_BAND_VALUE      0xDEAD1010L
#define KAL_SEMAPHORE_HIGH_GUARD_BAND_VALUE     0xDEADBEEFL








/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef struct kal_semaphore_s
{
    u32_t  lowGuardBand;
    i8_t   inUse;
    sem_t  posixSemaphore;
    u32_t  highGuardBand;
}  kal_semaphore_t;








/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
static kal_semaphore_t  kalSemaphores[KAL_CONFIG_SEMAPHORE_MAX_INSTANCES];
static i16_t            kalSemaphoreIndex=0;
static sem_t            kalSemaphoreLock;








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
void
KAL_SemaphoreInit(void)
{
    /* Create a binary semaphore to enforce concurrent calls to KAL_SemaphoreCreate().  */
    sem_init(&kalSemaphoreLock, 0, 1);

    /* Initialize the internal data structure.  */
    for (kalSemaphoreIndex=0; kalSemaphoreIndex<KAL_CONFIG_SEMAPHORE_MAX_INSTANCES; kalSemaphoreIndex++)
    {
        kalSemaphores[kalSemaphoreIndex].lowGuardBand = (u32_t)KAL_SEMAPHORE_LOW_GUARD_BAND_VALUE;
        kalSemaphores[kalSemaphoreIndex].inUse = KAL_SEMAPHORE_FALSE;
        memset(&kalSemaphores[kalSemaphoreIndex].posixSemaphore, 0,
               sizeof(kalSemaphores[kalSemaphoreIndex].posixSemaphore));
        kalSemaphores[kalSemaphoreIndex].highGuardBand = (u32_t)KAL_SEMAPHORE_HIGH_GUARD_BAND_VALUE;
    }
    kalSemaphoreIndex = 0;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SemaphoreCreate() - this routine creates a semaphore, and returns the semaphore to the    *
 *                          caller.  There is a fixed number of coexisting semaphores which use   *
 *                          this API, and there is a chance that a new semaphore cannot be        *
 *                          created.                                                              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  count - the initial count for the semaphore.                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void*: non-NULL - upon success.                                                           *
 *             NULL     - opon failure.                                                           *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_SemaphoreCreate(u8_t  count)
{
    i8_t   index;
    i8_t   found;
    i32_t  status;
    void*  retValue=NULL;

    /* This routine needs to be thread-safe, therefore it uses its own semaphore (not counted as
         one used by the API) to prevent concurrent access.  */
    while (((status=sem_wait(&kalSemaphoreLock)) == -1) && (errno == EINTR))
    {
    }

    if (status == 0)
    {
        /* Starting at kalSemaphoreIndex, search for next entry not in use.  Keep track of initial
           starting point in case all semaphores are in use.  */
        index = 0;
        found = (kalSemaphores[kalSemaphoreIndex].inUse == KAL_SEMAPHORE_FALSE);
        while ((!found) && (index < KAL_CONFIG_SEMAPHORE_MAX_INSTANCES))
        {
            index++;
            if (++kalSemaphoreIndex == KAL_CONFIG_SEMAPHORE_MAX_INSTANCES)
            {
                kalSemaphoreIndex = 0;
            }
            found = (kalSemaphores[kalSemaphoreIndex].inUse == KAL_SEMAPHORE_FALSE);
        }

        if (found)
        {
            kalSemaphores[kalSemaphoreIndex].inUse = KAL_SEMAPHORE_TRUE;
            sem_init(&kalSemaphores[kalSemaphoreIndex].posixSemaphore, 0, count);
            retValue = &kalSemaphores[kalSemaphoreIndex];
            if (++kalSemaphoreIndex == KAL_CONFIG_SEMAPHORE_MAX_INSTANCES)
            {
                kalSemaphoreIndex = 0;
            }
        }
    }

    /* Release the thread-safe lock.  */
    sem_post(&kalSemaphoreLock);

    return retValue;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SemaphoreDelete() - this routine deletes a semaphore.                                     *
 *                                                                                                *
 *                          Note: if threads are pending on the semaphore when it is deleted, the *
 *                                results are undefined.  In other words, use this routine with   *
 *                                extreme care.                                                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  pSemaphore - a pointer to the semaphore to delete.                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS - upon success deletion of the semaphore.                                *
 *            G2_FAILURE - upon failure.                                                          *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SemaphoreDelete(void*  pSemaphore)
{
    u8_t              retCode = (u8_t)G2_FAILURE;
    kal_semaphore_t*  p_semaphore=(kal_semaphore_t*) pSemaphore;

    /* Unlike creation, deletion may be performed without a lock (since a search of the table is
         not necessary).  To prevent discord with concurrent calls to KAL_SemaphoreCreate(), the
         last field changed is the inUse field.  Now, check to see if the parameter is a valid
         semaphore.  */
    if ((p_semaphore->lowGuardBand == KAL_SEMAPHORE_LOW_GUARD_BAND_VALUE) &&
        (p_semaphore->highGuardBand == KAL_SEMAPHORE_HIGH_GUARD_BAND_VALUE) &&
        (p_semaphore->inUse == KAL_SEMAPHORE_TRUE))
    {
        sem_destroy(&p_semaphore->posixSemaphore);
        memset(&p_semaphore->posixSemaphore, 0, sizeof(p_semaphore->posixSemaphore));
        retCode = G2_SUCCESS;
        p_semaphore->inUse = KAL_SEMAPHORE_FALSE;
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SemaphorePend() - this routine is used to obtain a semaphore.  If the semaphore count is  *
 *                        zero, the calling routine will suspend until it can either obtain the   *
 *                        semaphore (by another thread calling KAL_SemaphorePost()) or time out.  *
 *                        The timeout value may be specified as zero if an infinite timeout is    *
 *                        desired.                                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  pSemaphore   - a pointer to the semaphore upon which to pend.                      *
 *      u32_t  milliseconds - the amount of time to type to obtain the semaphore.  A value of     *
 *                            zero indicates an infinite timeout.                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS - upon success on obtaining the semaphore.                               *
 *            G2_TIMEOUT - if the semaphore was not available before the specified timeout.       *
 *            G2_FAILURE - upon failure.                                                          *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SemaphorePend(void*  pSemaphore,
                  u32_t  milliseconds)
{
    u32_t             seconds;
    u32_t             nanoseconds;
    kal_semaphore_t*  p_semaphore=(kal_semaphore_t*) pSemaphore;
    struct timespec   timeoutValue;
    u8_t              retCode = (u8_t)G2_FAILURE;
    i32_t             retValue;

    if (milliseconds == 0)
    {
        /* No timeout so use the underlying POSIX sem_wait() routine.  */
        while (((retValue = sem_wait(&p_semaphore->posixSemaphore)) == -1) && (errno == EINTR))
        {
            /* Call was interrupted by an interrupt.  Do nothing and continue.  */
        }

        if (retValue == 0)
        {
            retCode = G2_SUCCESS;
        }
    }
    else
    {
        /* A timeout is used, so the underlying POSIX sem_timedwait() routine will be used.  This
             requires calculations from the current time because the timeout in the sem_timedwait()
             routine is an absolute time.  */

        /* Calculate the timeout in terms of seconds and nanoseconds.  */
        seconds = milliseconds / 1000;
        milliseconds -= (seconds * 1000);
        nanoseconds = milliseconds * 1000000;

        /* An absolute time must be calculated for a timeout.  */
        if (clock_gettime(CLOCK_REALTIME, &timeoutValue) == 0)
        {
            timeoutValue.tv_sec += seconds;
            timeoutValue.tv_nsec += nanoseconds;
            if (timeoutValue.tv_nsec >= 1000000000)
            {
                timeoutValue.tv_sec++;
                timeoutValue.tv_nsec -= 1000000000;
            }

            while (((retValue=sem_timedwait(&p_semaphore->posixSemaphore, &timeoutValue)) == -1) &&
                   (errno == EINTR))
            {
                /* Call was interrupted by an interrupt.  Do nothing and continue.  */
            }

            if (retValue == 0)
            {
                retCode = G2_SUCCESS;
            }
            else if (errno == ETIMEDOUT)
            {
                retCode = G2_TIMEOUT;
            }
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SemaphorePost() - this routine posts to the semaphore, incrementing its value and waking  *
 *                        up any thread waiting for it.                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  pSemaphore - a pointer to the semaphore to post.                                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS - upon success.                                                          *
 *            G2_FAILURE - upon failure.                                                          *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SemaphorePost(void*  pSemaphore)
{
    u8_t              retCode = (u8_t)G2_FAILURE;
    kal_semaphore_t*  p_semaphore=(kal_semaphore_t*) pSemaphore;

    if (sem_post(&p_semaphore->posixSemaphore) == 0)
    {
        retCode = G2_SUCCESS;
    }

    return retCode;
}




