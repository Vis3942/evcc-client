/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_elel_client.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application End Device Log Event List Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#include "app_elel_client.h"
#include "app_api.h"
#include "app_config.h"
#include "app_utils.h"
#include "kal_api.h"
#include "sl_api.h"
#include <stdio.h>
#include <string.h>

#if APP_CONFIG_ELEL_CLIENT_ENABLED

/*******************************************************************************
 * Private Constants
 ******************************************************************************/

/*******************************************************************************
 * Private Data
 ******************************************************************************/

struct
{
    bool                      serverFound;
    sl_elel_client_server_ind serverInfo;
} appElelClient = {0};

/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void AppElelClientHandleServerInd(sl_elel_client_server_ind *pInd);

/*******************************************************************************
 * AppElelClientHandleServerInd()
 ******************************************************************************/
/**
 * \brief If the log event function set is available in the edev, the edev
 *        server is chosen and processing is started
 * \param pInd - pointer to the server indication message
 * \return none
 */
static void AppElelClientHandleServerInd(sl_elel_client_server_ind *pInd)
{
    sl_elel_client_send_le_req req = {0};

    if ((appElelClient.serverFound == false) && (pInd->status != SL_CMN_SERVER_STATUS_LOST))
    {
        // This starts the processing of the elel client
        appElelClient.serverFound = true;
        appElelClient.serverInfo  = *pInd;

        // Post a log event
        req.serverSourceInfo.serverHandle  = pInd->serverHandle;
        req.serverSourceInfo.source.source = pInd->sourceList[0].source;

        req.logEvent.createdDateTime    = KAL_GetClock();
        req.logEvent.extendedDataExists = true;
        req.logEvent.extendedData       = 12345678;
        req.logEvent.functionSet        = SL_XSD_FUNCTION_SET_GENERAL;
        req.logEvent.logEventCode       = 10;
        req.logEvent.logEventID         = 20;
        req.logEvent.logEventPEN        = 30;
        req.logEvent.profileID          = SL_XSD_PROFILE_ID_VENDOR_DEFINED;

        app_SendMail(SL_ELEL_CLIENT_SEND_LE_REQ, &req, sizeof(sl_elel_client_send_le_req));
    }
}

/*******************************************************************************
 * app_ElelClientStart()
 ******************************************************************************/
/**
 * \brief Called after the G2H stack is started
 * \param none
 * \return none
 */
void app_ElelClientStart(void)
{

#ifdef APP_PRELOAD_DEMO

#endif // APP_PRELOAD_DEMO
}

/*******************************************************************************
 * app_ElelClientHandleMail()
 ******************************************************************************/
void app_ElelClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
    switch (mailId)
    {
    case SL_ELEL_CLIENT_SERVER_IND:
        APP_LOG_INFO("app_ElelClientHandleMail: SL_ELEL_CLIENT_SERVER_IND");
        AppElelClientHandleServerInd((sl_elel_client_server_ind *)pMail);
        break;
    case SL_ELEL_CLIENT_SEND_LE_CNF:
        APP_LOG_INFO("app_ElelClientHandleMail: SL_ELEL_CLIENT_SEND_LE_CNF");
        app_LogStatus(((sl_elel_client_send_le_cnf *)pMail)->status);
        break;
    }
}

#endif // APP_CONFIG_ELEL_CLIENT_ENABLED
