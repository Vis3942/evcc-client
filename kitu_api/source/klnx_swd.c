/**************************************************************************************************
 *                                                                                                *
 *  Kitu Systems, Inc.     Copyright(C) 2015.  All rights reserved.                               *
 *                                                                                                *
 *  This source code and any compilation or derivative thereof is the proprietary information of  *
 *  Kitu Systems, Inc. and is confidential in nature.                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  klnx_swd.c                                                                                    *
 *                                                                                                *
 *  This file contains the implementation of the Software Watchdog system.                        *
 *                                                                                                *
 **************************************************************************************************/


#include <stdbool.h>
#include "kal_config.h"
#include "kal_api.h"
#include "klnx_internal.h"
#include "psal.h"
#include "psal_assert.h"








/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_SWD_DEFAULT_CONFIG_SWD_PERIOD             (1000)
#define KAL_SWD_DEFAULT_CONFIG_NUM_FAILURES_TO_FAULT  (5)
#define KAL_SWD_INVALID_SWD_PERIOD                    (0)








/**************************************************************************************************
 *                                                                                                *
 *  Macros                                                                                        *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_SwdGetLock()                  KAL_TaskGetLock()
#define KAL_SwdReleaseLock()              KAL_TaskReleaseLock()












/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
kal_swdTaskInfo_t                 kalSwdTaskInfo[KAL_CONFIG_TASK_MAX_CONCURRENT];
static u32_t                      kalSwdMissedPetCounters[KAL_CONFIG_TASK_MAX_CONCURRENT];
static kal_swd_failure_handler_t  pf_kalSwdFailureRoutine=NULL;
static kal_swd_fault_handler_t    pf_kalSwdFaultRoutine=NULL;
static kal_swd_success_handler_t  pf_kalSwdSuccessRoutine=NULL;
static u32_t                      kalSwdPeriodInMs=KAL_SWD_DEFAULT_CONFIG_SWD_PERIOD;
static u32_t                      kalSwdNumFailuresForFault=KAL_SWD_DEFAULT_CONFIG_NUM_FAILURES_TO_FAULT;
static u32_t                      kalSwdNewPeriod=KAL_SWD_INVALID_SWD_PERIOD;
static bool                       kalSwdIsRunning=false;








/**************************************************************************************************
 *                                                                                                *
 *  Local Function Declarations                                                                   *
 *                                                                                                *
 **************************************************************************************************/
static u8_t
KAL_SwdGetNumRegistered(void);








/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdRegister() - this routine is used by a task to register its use of the software        *
 *                      watchdog.  Registration is optional.  Not registering is the same as      *
 *                      registering as KAL_SWD_TASKTYPE_NONE.  Registration can only occur before *
 *                      the KAL system start event.                                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_swd_tasktype_t  swdType - type of software watchdog to be registered with the calling *
 *                                    task.  Valid values include KAL_SWD_TASKTYPE_NONE,          *
 *                                    KAL_SWD_TASKTYPE_USERHANDLED, and                           *
 *                                    KAL_SWD_TASKTYPE_AUTOMATIC.                                 *
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
 *      u8_t: G2_SUCCESS   - upon success.                                                        *
 *            G2_FAILURE   - if the swdType is not valid, or if this routine was invoked after    *
 *                           the KAL system start event.                                          *
 *            G2_BAD_HTASK - if this routine was invoked by a non-KAL task.                       *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SwdRegister(kal_swd_tasktype_t  swdType)
{
    pthread_t  h_thread;
    u8_t       h_task;
    u8_t       retCode = (u8_t)G2_FAILURE;

    if (KAL_HasSystemStarted() == 0)
    {
        if (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, &h_thread, &h_task) == G2_SUCCESS)
        {
            if ((swdType == KAL_SWD_TASKTYPE_NONE) ||
                (swdType == KAL_SWD_TASKTYPE_USERHANDLED) ||
                (swdType == KAL_SWD_TASKTYPE_AUTOMATIC))
            {
                kalSwdTaskInfo[h_task].swdRegistrationType = swdType;
                retCode = G2_SUCCESS;
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdPetTheDog() - this routine is used to "pet" the software watchdog for the calling      *
 *                       task.                                                                    *
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
void
KAL_SwdPetTheDog(void)
{
    pthread_t  h_thread;
    u8_t       h_task;
    
    if (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, &h_thread, &h_task) == G2_SUCCESS)
    {
        KAL_SwdGetLock();
        kalSwdTaskInfo[h_task].swdWasPet = 1;
        KAL_SwdReleaseLock();
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdDisable() - this routine is used by a task to disable its SWD.                         *
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
 *      u8_t: G2_SUCCESS   - upon success.                                                        *
 *            G2_FAILURE   - if the task is not registered to use the SWD, or if the task's SWD   *
 *                           is already disabled.                                                 *
 *            G2_BAD_HTASK - if the calling task is not a KAL task.                               *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SwdDisable(void)
{
    pthread_t  h_thread;
    u8_t       h_task;
    u8_t       retCode = (u8_t)G2_FAILURE;
    
    if (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, &h_thread, &h_task) == G2_SUCCESS)
    {
        KAL_SwdGetLock();
        if (((kalSwdTaskInfo[h_task].swdRegistrationType == KAL_SWD_TASKTYPE_USERHANDLED) || 
             (kalSwdTaskInfo[h_task].swdRegistrationType == KAL_SWD_TASKTYPE_AUTOMATIC)) && 
            (kalSwdTaskInfo[h_task].swdState != KAL_SWD_STATE_DISABLED))
        {
            kalSwdTaskInfo[h_task].swdState = KAL_SWD_STATE_DISABLED;
            retCode = G2_SUCCESS;
        }
        KAL_SwdReleaseLock();
    }
    else
    {
        retCode = G2_BAD_HTASK;
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdReenable() - this routine is used to re-enable its SWD.  As described in the Software  *
 *                      Watchdog Design Specification, this routine will change the state of the  *
 *                      calling task's SWD either to RUNNING or REENABLED, depending upon its     *
 *                      previous "pet" status.                                                    *
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
 *      u8_t: G2_SUCCESS   - upon success.                                                        *
 *            G2_FAILURE   - if the calling task is not registered to use the SWD, or if the      *
 *                           task's SWD is already enabled (i.e., RUNNING or REENABLED).          *
 *            G2_BAD_HTASK - if the calling task is not a KAL task.                               *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SwdReenable(void)
{
    pthread_t  h_thread;
    u8_t       h_task;
    u8_t       retCode = (u8_t)G2_FAILURE;

    if (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, &h_thread, &h_task) == G2_SUCCESS)
    {
        KAL_SwdGetLock();
        if (((kalSwdTaskInfo[h_task].swdRegistrationType == KAL_SWD_TASKTYPE_USERHANDLED) || 
             (kalSwdTaskInfo[h_task].swdRegistrationType == KAL_SWD_TASKTYPE_AUTOMATIC)) && 
            (kalSwdTaskInfo[h_task].swdState == KAL_SWD_STATE_DISABLED))
        {
            if (kalSwdTaskInfo[h_task].swdWasPet)
            {
                kalSwdTaskInfo[h_task].swdState = KAL_SWD_STATE_RUNNING;
            }
            else
            {
                kalSwdTaskInfo[h_task].swdState = KAL_SWD_STATE_REENABLED;
            }
            retCode = G2_SUCCESS;
        }
        KAL_SwdReleaseLock();
    }
    else
    {
        retCode = G2_BAD_HTASK;
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdConfigFailureHandler() - this routine is used to change SWD routine that is invoked    *
 *                                  upon SWD failure.                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_swd_failure_handler_t  pf_handler - a pointer to a function which is invoked upon a   *
 *                                              SWD failure.  This parameter may be NULL in which *
 *                                              case no function will be called.                  *
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
 *      u8_t: G2_SUCCESS - upon success.                                                          *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SwdConfigFailureHandler(kal_swd_failure_handler_t  pf_handler)
{
    pf_kalSwdFailureRoutine = pf_handler;

    return G2_SUCCESS;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdConfigFaultHandler() - this routine is used to change the SWD routine that is invoked  *
 *                                upon SWD fault.                                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_swd_fault_handler_t  pf_handler - a pointer to a function which is invoked upon a SWD *
 *                                            fault.  This parameter may be NULL in which case no *
 *                                            function will be called.                            *
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
 *      u8_t: G2_SUCCESS - upon success.                                                          *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SwdConfigFaultHandler(kal_swd_fault_handler_t  pf_handler)
{
    pf_kalSwdFaultRoutine = pf_handler;

    return G2_SUCCESS;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdConfigSuccessHandler() - this routine is used to change the SWD routine that is        *
 *                                  invoked when the SWD system detects no failures or faults.    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_swd_success_handler_t  pf_handler - a pointer to a function which is invoked upon SWD *
 *                                              success.  This parameter may be NULL in which     *
 *                                              case no function will be called.                  *
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
 *      u8_t: G2_SUCCESS - upon success.                                                          *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SwdConfigSuccessHandler(kal_swd_success_handler_t  pf_handler)
{
    pf_kalSwdSuccessRoutine = pf_handler;

    return G2_SUCCESS;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdConfigTimingParameters() - this routine is used to change the period of the SWD check  *
 *                                    and the number of SWD failures which will constitute a SWD  *
 *                                    fault.  Invoking this routine when the SWD is already       *
 *                                    running will cause the new period to start when the next    *
 *                                    SWD occurs (using the old period).                          *
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
 *      u8_t: G2_SUCCESS - upon success.                                                          *
 *            G2_FAILURE - if there is an invalid parameter.                                      *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_SwdConfigTimingParameters(u32_t  periodInMs,
                              u32_t  numFailures)
{
    u8_t  retCode = (u8_t)G2_FAILURE;

    if ((periodInMs != KAL_SWD_INVALID_SWD_PERIOD) && (numFailures != 0))
    {
        kalSwdNumFailuresForFault = numFailures;
        if (periodInMs != kalSwdPeriodInMs)
        {
            kalSwdNewPeriod = periodInMs;
        }

        retCode = G2_SUCCESS;
    }
    return retCode;
}




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
void
KAL_SwdStart(void)
{
    u8_t  numTasks;
    u8_t  index;

    PSAL_ASSERT(MM_TaskHandle == 0);

    if (KAL_SwdGetNumRegistered() != 0)
    {
        numTasks = KAL_GetNumTasks();
        KAL_SwdGetLock();
        for (index=1; index<numTasks; index++)
        {
            kalSwdTaskInfo[index].swdState = KAL_SWD_STATE_RUNNING;
            kalSwdTaskInfo[index].swdWasPet = 1;
            kalSwdMissedPetCounters[index] = 0;
        }
        KAL_SwdReleaseLock();
        kalSwdIsRunning = true;
        (void) KAL_SetTimer(kalSwdPeriodInMs, MM_TaskHandle, KAL_SWD_EVENT_BIT, 1);
    }
    else if (pf_kalSwdSuccessRoutine != NULL)
    {
        kalSwdIsRunning = true;
        (void) KAL_SetTimer(kalSwdPeriodInMs, MM_TaskHandle, KAL_SWD_EVENT_BIT, 1);
    }
}




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
void
KAL_SwdCheck(void)
{
    u8_t  index;
    u8_t  numTasks;
    u8_t  numFailures=0;

    /* See if a new period for the SWD was configured.  If so, then its value will be enacted
         during this SWD check.  */
    if (kalSwdNewPeriod != KAL_SWD_INVALID_SWD_PERIOD)
    {
        /* There is a new period for the SWD.  Copy in the new value, zero out the value which
             indicates that there is a new value, cancel the old repeating timer, and start a new
             repeating timer.  */
        kalSwdPeriodInMs = kalSwdNewPeriod;
        kalSwdNewPeriod = KAL_SWD_INVALID_SWD_PERIOD;

        /* Cancel the existing repeating timer.  */
        (void) KAL_CancelTimer(MM_TaskHandle, KAL_SWD_EVENT_BIT);

        /* Create a new repeating timer.  */
        (void) KAL_SetTimer(kalSwdPeriodInMs, MM_TaskHandle, KAL_SWD_EVENT_BIT, 1);
    }

    /* Check for SWD failures.  */
    numTasks = KAL_GetNumTasks();
    for (index=1; index<numTasks; index++)
    {
        KAL_SwdGetLock();
        if (kalSwdTaskInfo[index].swdRegistrationType != KAL_SWD_TASKTYPE_NONE)
        {
            if (kalSwdTaskInfo[index].swdState != KAL_SWD_STATE_DISABLED)
            {
                if (kalSwdTaskInfo[index].swdState == KAL_SWD_STATE_RUNNING)
                {
                    if (kalSwdTaskInfo[index].swdWasPet)
                    {
                        kalSwdMissedPetCounters[index] = 0;
                        kalSwdTaskInfo[index].swdWasPet = 0;
                        KAL_SwdReleaseLock();
                    }
                    else
                    {
                        /* SWD failure occurred.  */
                        kalSwdMissedPetCounters[index]++;
                        KAL_SwdReleaseLock();
                        numFailures++;
                        
                        /* Invoke the failure routine, if extant.  */
                        if (pf_kalSwdFailureRoutine != NULL)
                        {
                            (*pf_kalSwdFailureRoutine)(index);
                        }

                        /* Check if the failure constitutes a fault.  */
                        if (kalSwdMissedPetCounters[index] >= kalSwdNumFailuresForFault)
                        {
                            if (pf_kalSwdFaultRoutine != NULL)
                            {
                                (*pf_kalSwdFaultRoutine)(index);
                            }
                        }
                    }

                    /* Send out the next SWD event to the task.  */
                    (void) KAL_TaskSetEvent(index, KAL_SWD_EVENT);
                }
                else
                {
                    /* Switch the SWD state to RUNNING, and do not set the next event.  This case
                         does not check the SWD because the task had disabled it and only recently
                         re-enabled it (within the last SWD period).  */
                    kalSwdTaskInfo[index].swdState = KAL_SWD_STATE_RUNNING;
                    KAL_SwdReleaseLock();
                }
            }
            else
            {
                KAL_SwdReleaseLock();
            }
        }
        else
        {
            KAL_SwdReleaseLock();
        }
    }

    if (numFailures == 0)
    {
        /* Invoke the success routine, if extant.  */
        if (pf_kalSwdSuccessRoutine != NULL)
        {
            (*pf_kalSwdSuccessRoutine)();
        }
    }
}




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
void
KAL_SwdInit(void)
{
    if (kalSwdIsRunning == 0)
    {
        KAL_Memset(kalSwdTaskInfo, 0, sizeof(kalSwdTaskInfo));
        KAL_Memset(kalSwdMissedPetCounters, 0, sizeof(kalSwdMissedPetCounters));
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_SwdGetNumRegistered() - this routine returns the number of tasks which are registered to  *
 *                              use the SWD system.                                               *
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
 *      kalSwdTaskInfo                                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t - the number of tasks which are registered to use the SWD.                           *
 *                                                                                                *
 **************************************************************************************************/
static u8_t
KAL_SwdGetNumRegistered(void)
{
    u8_t  index;
    u8_t  numTasks;
    u8_t  retValue=0;

    numTasks = KAL_GetNumTasks();
    KAL_SwdGetLock();
    for (index=1; index<numTasks; index++)
    {
        if ((kalSwdTaskInfo[index].swdRegistrationType == KAL_SWD_TASKTYPE_USERHANDLED) ||
            (kalSwdTaskInfo[index].swdRegistrationType == KAL_SWD_TASKTYPE_AUTOMATIC))
        {
            retValue++;
        }
    }
    KAL_SwdReleaseLock();

    return retValue;
}




