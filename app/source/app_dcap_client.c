/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_dcap_client.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Venkat Sethuraman
 *
 * \brief         Application Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#include "app_logs.h"
#include "app_utils.h"
#include "psal_assert.h"
#include "sl_api.h"
#include <stdio.h>
#include <string.h>

#define APP_DCAP_MAX_SERVERS 4

typedef struct
{
    sl_cmn_client_server_ind_t serverInfo;
    bool                       valid;
} dcap_client_server_info;

static dcap_client_server_info server[APP_DCAP_MAX_SERVERS] = {{{0, 0, {0}, false, 0, {{0}}}, false}};

static void SendGetDcapReq(u32_t index);
static void HandleDcapServerInd(sl_dcap_client_server_ind *pInd);

/*******************************************************************************
 * app_DcapClientHandleMail()
 ******************************************************************************/
void app_DcapClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
    switch (mailId)
    {
    case SL_DCAP_CLIENT_SERVER_IND:
        APP_LOG_INFO("app_DcapClientHandleMail: SL_DCAP_CLIENT_SERVER_IND");
        HandleDcapServerInd((sl_dcap_client_server_ind *)pMail);
        break;

    case SL_DCAP_CLIENT_GET_DCAP_CNF:
        APP_LOG_INFO("app_DcapClientHandleMail:SL_DCAP_CLIENT_GET_DCAP_CNF");
        app_LogStatus(((sl_dcap_client_get_dcap_cnf *)pMail)->status);
        break;
    }
}

/*******************************************************************************
 ** HandleDcapServerInd
 *******************************************************************************/
/**
 ** \brief Handles the SL_DCAP_CLIENT_SERVER_IND. Requests are allowed
 **        following this indication.

 ** \return None
 **/

static void HandleDcapServerInd(sl_dcap_client_server_ind *pInd)
{
    u32_t index = 0;

    PSAL_ASSERT(pInd != NULL);

    if (pInd->status == SL_CMN_SERVER_STATUS_FOUND)
    {
        // Store the new server
        for (index = 0; index < APP_DCAP_MAX_SERVERS; index++)
        {
            if (server[index].valid == false) break; // Found a free slot for the new Server
        }

        if (index == APP_DCAP_MAX_SERVERS)
        {
            APP_DLOG_ERROR("Dropping server, max Server Count reached \n");
            return;
        }

        server[index].valid = true;
        KAL_Memcpy(&(server[index].serverInfo), pInd, sizeof(sl_cmn_client_server_ind_t));

        SendGetDcapReq(index);
    }
    else
    {
        // Find the existing server
        for (index = 0; index < APP_DCAP_MAX_SERVERS; index++)
        {
            if (server[index].serverInfo.serverHandle == pInd->serverHandle)
            {
                if (pInd->status == SL_CMN_SERVER_STATUS_LOST)
                    // Remove the server
                    server[index].valid = false;
                else
                    // Update the server
                    KAL_Memcpy(&(server[index].serverInfo), pInd, sizeof(sl_cmn_client_server_ind_t));

                break;
            }
        }
    }
}

/*******************************************************************************
 * SendGetDcapReq()
 ******************************************************************************/
static void SendGetDcapReq(u32_t index)
{
#ifdef APP_PRELOAD_DEMO
    sl_dcap_client_get_dcap_req req         = {0};
    u32_t                       sourceIndex = 0xFF;

    if (server[index].valid == true)
    {
        sourceIndex                        = server[index].serverInfo.numSources - 1;
        req.serverSourceInfo.serverHandle  = server[index].serverInfo.serverHandle;
        req.serverSourceInfo.source.source = server[index].serverInfo.sourceList[sourceIndex].source;

        app_SendMail(SL_DCAP_CLIENT_GET_DCAP_REQ, &req, sizeof(sl_dcap_client_get_dcap_req));
    }
#else
    (void)index;
#endif
}
