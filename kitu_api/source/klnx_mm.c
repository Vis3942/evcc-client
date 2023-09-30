/**************************************************************************************************
 *                                                                                                *
 *  Kitu Systems, Inc.     Copyright(C) 2015.  All rights reserved.                               *
 *                                                                                                *
 *  This source code and any compilation or derivative thereof is the proprietary information of  *
 *  Kitu Systems, Inc. and is confidential in nature.                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  klnx_mm.c                                                                                     *
 *                                                                                                *
 *  This file contains the implementation of the Mode Manager.  Because the Mode Manager is a     *
 *  privileged task with unrestricted access to data structures within KAL, MM is implemented as  *
 *  part of KAL since each KAL port will require a corresponding MM port.                         *
 *                                                                                                *
 **************************************************************************************************/


#include <stdbool.h>
#include "kal_api.h"
#include "kal_config.h"
#include "klnx_internal.h"
#include "psal.h"
#include "psal_assert.h"








/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define MM_ENTER_NORMAL_PROCESSING_BIT                KAL_MM_ENTER_NORMAL_PROCESSING_BIT
#define MM_RETURN_TO_OUTER_MODE_BIT                   KAL_MM_RETURN_TO_OUTER_MODE_BIT
#define MM_ENTER_NORMAL_PROCESSING_EVENT              KAL_MM_ENTER_NORMAL_PROCESSING_EVENT
#define MM_RETURN_TO_OUTER_MODE_EVENT                 KAL_MM_RETURN_TO_OUTER_MODE_EVENT
#define MM_MODE_NORMAL                                KAL_MM_MODE_NORMAL
//#define MM_MODE_UPGRADE                               KAL_MM_MODE_UPGRADE
//#define MM_MODE_SLEEP                                 KAL_MM_MODE_SLEEP
#define MM_MODE_DEFAULT                               KAL_MM_MODE_DEFAULT
#define MM_MAILID_SWITCH_MODE_CMD                     (0x0001)
#define MM_MAILID_RETURN_TO_OUTER_MODE_RESP           (0x0002)
#define MM_IDLE_POLL_PERIOD_IN_MS                     (5)
#define MM_IDLE_POLL_EVENT_BIT                        (16)
#define MM_IDLE_POLL_EVENT_MASK                       ((kal_event_t) (1ULL << MM_IDLE_POLL_EVENT_BIT))








/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef kal_MmMode_t  mm_Mode_t;


typedef enum mm_State_e
{
    MM_STATE_INIT,
    MM_STATE_IN_TRANSITION,
    MM_STATE_IN_MODE
}  mm_State_t;


typedef struct kal_MmSwitchModeCmd_s
{
    kal_MmMode_t  newMode;
}  kal_MmSwitchModeCmd_t;

typedef kal_MmSwitchModeCmd_t  mm_SwitchModeCmd_t;








/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
u8_t               MM_TaskHandle=(u8_t)0xFF;            // When assigned, this should be zero!!!!!
static mm_Mode_t   mmMode=MM_MODE_DEFAULT;
static mm_State_t  mmState=MM_STATE_INIT;
static u8_t        mmNumTasks=0;
static u8_t        mmTasksInOuterMode[KAL_CONFIG_TASK_MAX_CONCURRENT]={0};








/**************************************************************************************************
 *                                                                                                *
 *  Local Function Declarations                                                                   *
 *                                                                                                *
 **************************************************************************************************/
static void
KAL_MmTask(void*  p_dummy);

static void
KAL_MmHandleIdlePollEvent(void);

static void
KAL_MmGoFromInitStateToNormalMode(void);

static void
KAL_MmNormalModeHandler(kal_event_t  events);

static void
KAL_MmSwitchMode(kal_MmMode_t  newMode);

static void
KAL_MmIgnoreMail(void);

static void
KAL_MmInTransitionHandler(kal_event_t  events);








/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmCreateModeManagerTask() - this routine is used to create the Mode Manager task.  An     *
 *                                  additional check will be made to ensure that the Mode Manager *
 *                                  has a KAL task handle of zero.                                *
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
KAL_MmCreateModeManagerTask(void)
{
    MM_TaskHandle = KAL_TaskCreate(&KAL_MmTask, NULL, "MoMa", NULL, 0, (u8_t)255);

    PSAL_ASSERT(MM_TaskHandle == 0);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmSendReturnToOuterModeResponse() - this public routine is used by KAL tasks other than   *
 *                                          the Mode Manager to respond to the                    *
 *                                          MM_RETURN_TO_OUTER_MODE_EVENT event.  This routine    *
 *                                          will actually cause mail to be sent to the Mode       *
 *                                          Manager.                                              *
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
KAL_MmSendReturnToOuterModeResponse(void)
{
    kal_mail_details_t  mailDetails;

    mailDetails.pParameterBlock = NULL;
    mailDetails.ParameterLength = 0;
    mailDetails.iPartnerTask = MM_TaskHandle;
    mailDetails.MailId = MM_MAILID_RETURN_TO_OUTER_MODE_RESP;

    (void) KAL_MailSend(NULL, &mailDetails);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmSendSwitchModeCommand() - this public routine is used by any other task (including      *
 *                                  non-KAL tasks) to send a command to the Mode Manager to       *
 *                                  switch modes.  The command is sent via one-way KAL mail to    *
 *                                  the Mode Manager.                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_MmMode_t  newMode - the new mode to which to switch.                                  *
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
KAL_MmSendSwitchModeCommand(kal_MmMode_t  newMode)
{
    kal_mail_details_t     mailDetails;
    kal_MmSwitchModeCmd_t  switchCmd;

    switchCmd.newMode = newMode;
    mailDetails.pParameterBlock = &switchCmd;
    mailDetails.ParameterLength = sizeof(switchCmd);
    mailDetails.iPartnerTask = MM_TaskHandle;
    mailDetails.MailId = MM_MAILID_SWITCH_MODE_CMD;

    (void) KAL_MailSendOneWay(&mailDetails);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmTask() - this private routine is the entry point for the Mode Manager task.  The Mode   *
 *                 Manager is a privileged KAL task in that it accesses KAL internal data         *
 *                 structures directly.  Its framework allows flexible support for a Sleep Mode   *
 *                 Manager, Software Watchdog, Upgrade Manager, etc.                              *
 *                                                                                                *
 *                 The Mode Manager has the concept of states and modes, where the state is of    *
 *                 primary importance.  There are only three states: INIT, IN_TRANSITION, and     *
 *                 IN_MODE.  (Modes are only relevant in the IN_MODE state.)  MM starts in the    *
 *                 INIT state, then waits for all tasks to be waiting in calls to                 *
 *                 KAL_TaskEventWait().  At that point, MM issues the KAL system start event to   *
 *                 all other KAL tasks, transitions to the IN_MODE state, and also issues the     *
 *                 MM_ENTER_NORMAL_PROCESSING_EVENT.  At this point, the MM is in the IN_MODE     *
 *                 state and the mode is NORMAL.  In NORMAL mode, the MM runs the Software        *
 *                 Watchdog (SWD).                                                                *
 *                                                                                                *
 *                 Whenever a mode switch occurs (e.g., from NORMAL to SLEEP), the MM enters the  *
 *                 IN_TRANSITION state and issues the MM_RETURN_TO_OUTER_MODE_EVENT to all other  *
 *                 KAL tasks.  Each KAL task must respond by exiting their NORMAL processing      *
 *                 loops and calling the KAL_MmSendReturnToOuterModeResponse() routine.  This     *
 *                 routine will send mail to the MM.  When MM has received mail from every other  *
 *                 KAL task, it will then wait for all tasks to call in KAL_TaskEventWait(), then *
 *                 will switch to the IN_MODE state.  Depending upon the entered mode, MM may     *
 *                 send events to the KAL tasks instructing them to enter the mode.  (For         *
 *                 example, the MM_ENTER_NORMAL_PROCESSING_EVENT is sent when entering NORMAL     *
 *                 mode.)                                                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  p_dummy - a pointer to task startup parameters.  This variable is not used for     *
 *                       this task.                                                               *
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
static void
KAL_MmTask(void*  p_dummy)
{
    kal_event_t  events;

    /* Wait for KAL_Start() to have been called.  The KAL system start event will be sent only
         to the Mode Manager.  */
    (void) KAL_TaskEventWait(KAL_START_EVENT, 0);

    /* Give all tasks a chance to run before checking that they are all within 
         KAL_TaskEventWait().  */
    KAL_Wait(500);

    /* Set up idle poll timer.  This event is used to see if the system is idle.  */
    (void) KAL_SetTimer(MM_IDLE_POLL_PERIOD_IN_MS, MM_TaskHandle, MM_IDLE_POLL_EVENT_BIT, 1);

    /* Start Mode Manager's infinite loop.  */
    while (1)
    {
        events = KAL_TaskEventWait(KAL_ALL_EVENTS, 0);
        if (mmState == MM_STATE_INIT)
        {
            if (events & KAL_MESSAGE_EVENT)
            {
                KAL_MmIgnoreMail();
            }
            if (events & MM_IDLE_POLL_EVENT_MASK)
            {
                KAL_MmHandleIdlePollEvent();
            }
        }
        else if (mmState == MM_STATE_IN_MODE)
        {
            switch (mmMode)
            {
            case MM_MODE_NORMAL:
                KAL_MmNormalModeHandler(events);
                break;
//            case MM_MODE_UPGRADE:
//                break;
//            case MM_MODE_SLEEP:
//                break;
            }
        }
        else  // mmState == MM_STATE_IN_TRANSITION
        {
            KAL_MmInTransitionHandler(events);
        }
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmNormalModeHandler() - this private routine handles all Mode Manager events in the       *
 *                              NORMAL mode of operation (in the IN_MODE state).                  *
 *                                                                                                *
 *                              At the time of writing, the only events for the Mode Manager in   *
 *                              NORMAL mode are the SWD timer event and incoming mail event.      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_events_t  events - a bitmap of events which should be handled by this routine.        *
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
static void
KAL_MmNormalModeHandler(kal_event_t  events)
{
    kal_mail_details_t      mailDetails;
    kal_message_details_t   msgDetails;
    kal_MmSwitchModeCmd_t*  p_switchModeCmd;

    if (events & KAL_SWD_EVENT)
    {
        KAL_SwdCheck();
    }
    if (events & KAL_MESSAGE_EVENT)
    {
        if (KAL_MailReceive(&msgDetails, &mailDetails) == G2_SUCCESS)
        {
            switch (mailDetails.MailId)
            {
            case MM_MAILID_SWITCH_MODE_CMD:
                p_switchModeCmd = mailDetails.pParameterBlock;
                if (mailDetails.ParameterLength == sizeof(*p_switchModeCmd))
                {
                    KAL_MmSwitchMode(p_switchModeCmd->newMode);
                }
                break;
            default:
                break;
            }
        }
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmHandleIdlePollEvent() - this routine is used to handle the idle poll event.  This task  *
 *                                polls for idleness in order to give all KAL tasks a chance to   *
 *                                run before issuing the KAL system start event.  If all tasks    *
 *                                are idle (in calls to KAL_TaskEventWait()), then the idle poll  *
 *                                timer is cancelled and the KAL system start event is issued (by *
 *                                the Mode Manager) to all KAL tasks (besides itself, of course). *
 *                                After issuing the KAL system start event, Mode Manager will     *
 *                                transition tasks to their NORMAL mode of operation.             *
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
static void
KAL_MmHandleIdlePollEvent(void)
{
    u8_t  index;

    if (KAL_TaskAreAllTasksIdle())
    {
        /* Cancel the repeating idle poll timer.  */
        (void) KAL_CancelTimer(MM_TaskHandle, MM_IDLE_POLL_EVENT_BIT);

        /* Perform "late" initialization (i.e., after all tasks have been created).  */
        KAL_MailInit();

        /* Change the KAL start state.  */
        KAL_TaskSetKalStartedState();
        mmNumTasks = KAL_GetNumTasks();
        
        /* Set the KAL system start event bit for all (other) KAL tasks.  */
        for (index=1; index<mmNumTasks; index++)
        {
            (void) KAL_TaskSetEvent(index, KAL_START_EVENT);
        }

        /* Now, switch to NORMAL mode.  */
        KAL_MmGoFromInitStateToNormalMode();
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmGoFromInitStateToNormalMode() - this routine is used to transition from the INIT state  *
 *                                        to the IN_MODE state, and to specify the NORMAL mode of *
 *                                        operation.  It will issue the                           *
 *                                        MM_START_NORMAL_PROCESSSING_EVENT to all (other) KAL    *
 *                                        tasks, and then will start the Software Watchdog        *
 *                                        system.                                                 *
 *                                                                                                *
 *                                        This routine should not be used to enter NORMAL mode    *
 *                                        from anything other than the INIT state.                *
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
static void
KAL_MmGoFromInitStateToNormalMode(void)
{
    u8_t  index;

    /* A transition to NORMAL mode must come from the OUTER mode.  */
    PSAL_ASSERT(mmState == MM_STATE_INIT);

    mmMode = MM_MODE_NORMAL;
    mmState = MM_STATE_IN_MODE;

    /* Send MM_START_NORMAL_PROCESSING_EVENT to all (other) KAL tasks.  */
    for (index=1; index<mmNumTasks; index++)
    {
        (void) KAL_TaskSetEvent(index, MM_ENTER_NORMAL_PROCESSING_EVENT);
    }

    /* Start the SWD system.  */
    KAL_SwdStart();
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmSwitchMode() - this private routine is used by the Mode Manager to switch from one mode *
 *                       to another mode.  A mode switch may only occur when in the IN_MODE       *
 *                       state.  If in the correct state, this routine will change state to       *
 *                       IN_TRANSITION, store the new mode, then send the RETURN_TO_OUTER_MODE    *
 *                       event to all (other) KAL tasks.  The KAL_MmInTransitionHandler() routine *
 *                       will then take over entering the new mode once every (other) KAL task    *
 *                       has responded to the RETURN_TO_OUTER_MODE event.                         *
 *                                                                                                *
 *                       It is legal to "switch" to the current mode (i.e., specify the current   *
 *                       mode as the new mode).  This will cause all tasks to be commanded to     *
 *                       return to their outer mode, then commanded to re-enter the new mode      *
 *                       (which was then mode they were in before exiting to outer mode).         *
 *                                                                                                *
 *                       This routine is invoked from various MM mode handlers as a result from   *
 *                       receiving mail containing a SWITCH_MODE_CMD mail ID.                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_MmMode_t  newMode - the new mode to which to switch.                                  *
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
static void
KAL_MmSwitchMode(kal_MmMode_t  newMode)
{
    u8_t  index;

    /* Only switch modes if already in a mode.  */
    if (mmState == MM_STATE_IN_MODE)
    {
        /* If the old mode was NORMAL, then stop the SWD timer.  */
        if (mmMode == MM_MODE_NORMAL)
        {
            (void) KAL_CancelTimer(MM_TaskHandle, KAL_SWD_EVENT_BIT);
        }

        /* Filter out invalid modes and only process valid ones.  Right now, the only valid mode 
             is NORMAL mode, but SLEEP and UPGRADE modes are expected some time in the future.  */
        if (newMode == MM_MODE_NORMAL)
        {
            /* If the new mode is the same as the current mode, a transition will be made so the
                 tasks will go to their OUTER loop, then go back to their old mode.  */
            mmState = MM_STATE_IN_TRANSITION;
            mmMode = newMode;
                
            /* Instruct all tasks to return to OUTER mode.  Also, clear the flag which is used to 
                 keep track whether the KAL task is in its outer mode or not.  */
            for (index=1; index<mmNumTasks; index++)
            {
                mmTasksInOuterMode[index] = 0;
                (void) KAL_TaskSetEvent(index, MM_RETURN_TO_OUTER_MODE_EVENT);
            }
        }
/*
        else if (newMode == MM_MODE_SLEEP)
        {
        }
        else if (newMode == MM_MODE_UPGRADE)
        {
        }
  
 */
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmIgnoreMail() - this private routine is used by the Mode Manager task to simply read KAL *
 *                       mail from its queue and ignore it.  (Mail needs to be read from the      *
 *                       queue in order to reclaim the dynamic memory associated with the mail.)  *
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
static void
KAL_MmIgnoreMail(void)
{
    kal_message_details_t  msgDetails;
    kal_mail_details_t     mailDetails;

    /* Dequeue the mail, allowing its memory to be recycled.  */
    (void) KAL_MailReceive(&msgDetails, &mailDetails);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MmInTransitionHandler() - this private routine is used by the Mode Manager to handle all  *
 *                                events when in the IN_TRANSITION state.  The IN_TRANSITION      *
 *                                state is used to wait for all KAL tasks (besides MM) to exit    *
 *                                into their outer mode and call the public                       *
 *                                KAL_MmSendReturnToOuterModeResponse() routine.  Once all        *
 *                                (other) KAL tasks have exited to their outer mode, this routine *
 *                                will cause a state change to the IN_MODE state to a mode        *
 *                                specified in a prior call to KAL_MmSendSwitchModeCommand().     *
 *                                                                                                *
 *                                When other KAL tasks call KAL_MmSendReturnToOuterModeResponse() *
 *                                in response to receiving the RETURN_TO_OUTER_MODE event, mail   *
 *                                is sent the Mode Manager.  Whenever this mail is handled in     *
 *                                this routine, a check is made to see if all (other) tasks have  *
 *                                sent the mail.  When all tasks have, a state change will be     *
 *                                made (to IN_MODE) and a new mode will be selected.              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_events_t  events - a bitmask containing events to be handled by this routine.         *
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
static void
KAL_MmInTransitionHandler(kal_event_t  events)
{
    kal_message_details_t  msgDetails;
    kal_mail_details_t     mailDetails;
    u8_t                   index;
    u8_t                   count;

    if (events & KAL_MESSAGE_EVENT)
    {
        if (KAL_MailReceive(&msgDetails, &mailDetails) == G2_SUCCESS)
        {
            if ((mailDetails.MailId == MM_MAILID_RETURN_TO_OUTER_MODE_RESP) &&
                (mailDetails.ParameterLength == 0))
            {
                if (mailDetails.iPartnerTask < mmNumTasks)
                {
                    mmTasksInOuterMode[mailDetails.iPartnerTask] = 1;

                    /* Check if all tasks are in outer mode.  If so, then transition into the new
                         mode.  */
                    count = 0;
                    for (index=1; index<mmNumTasks; index++)
                    {
                        count += mmTasksInOuterMode[index];
                    }
                    if (count == (mmNumTasks-1))
                    {
                        /* Every task is in its OUTER mode, so change state.  The variable mmMode 
                             should already be programmed with the new mode.  */
                        mmState = MM_STATE_IN_MODE;
                        switch (mmMode)
                        {
                        case MM_MODE_NORMAL:
                            for (index=1; index<mmNumTasks; index++)
                            {
                                (void) KAL_TaskSetEvent(index, MM_ENTER_NORMAL_PROCESSING_EVENT);
                            }
                            KAL_SwdStart();
                            break;
/*
                        case MM_MODE_SLEEP:
                            break;
                        case MM_MODE_UPGRADE:
                            break;
*/
                        }
                    }
                }
            }
        }
    }
}





