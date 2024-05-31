/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_edinfo_client.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application EDevice Info Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#include "app_edinfo_client.h"
#include "app_api.h"
#include "app_config.h"
#include "app_utils.h"
#include "kal_api.h"
#include "psal.h"
#include "sl_api.h"
#include <stdio.h>
#include <string.h>

#if APP_CONFIG_EDINFO_CLIENT_ENABLED

/*******************************************************************************
 * Private Constants
 ******************************************************************************/
#define APP_EDINFO_CLIENT_SELECT_SERVER_INDEX 0 // Optional, index to select server to send dinfo

/*******************************************************************************
 * Private Data
 ******************************************************************************/

typedef struct app_edinfo_server_node
{
    sl_edinfo_client_server_ind    serverInfo;
    struct app_edinfo_server_node *next;
} app_edinfo_server_node_t;

struct
{
    bool                          pendingRequest;
    app_edinfo_server_node_t     *pServerList;
    u8_t                          numServersFound;
    bool                          serverSelected;
    sl_cmn_client_server_handle_t selectedServerHandle;
} appEdinfoClient = {
    false, // pendingRequest;
    NULL,  // *serverList;
    0,     // numServersFound;
    false, // serverSelected;
    0      // selectedServerHandle;
};


/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void app_EdinfoClientHandleServerInd(sl_edinfo_client_server_ind *pInd);
static void app_EdinfoClientLoadEdinfo(sl_cmn_client_server_ind_t *pServer);
static void app_EdinfoClientHandleSendDinfoCnf(sl_edinfo_client_send_dinfo_cnf *pCnf);

/*******************************************************************************
 * app_EdinfoClientHandleServerInd()
 ******************************************************************************/
static void app_EdinfoClientHandleServerInd(sl_edinfo_client_server_ind *pInd)
{
    app_edinfo_server_node_t *pNode = appEdinfoClient.pServerList;
    app_edinfo_server_node_t *pNew;
    int                       index = 0;

    // If it is a new Server, add it to the end of the current list (or beginning if list empty)
    // and send a new dinfo request
    if (pInd->status == SL_CMN_SERVER_STATUS_FOUND)
    {
        pNew = KAL_Malloc(sizeof(app_edinfo_server_node_t));
        if (pNew == NULL)
        {
            APP_LOG_WARNING("app_EdinfoClientHandleServerInd: Cannot allocate memory for new server node");
            return;
        }

        pNew->serverInfo = *pInd;
        pNew->next       = NULL;

        if (pNode == NULL)
        {
            appEdinfoClient.pServerList = pNew;
        }
        else
        {
            while (pNode->next != NULL)
            {
                pNode = pNode->next;
                index++;
            }
            pNode->next = pNew;
            index++;
        }
        // // Send the devinfo info to the new server
        // if (index == APP_EDINFO_CLIENT_SELECT_SERVER_INDEX)
        // {
        //     app_EdinfoClientLoadEdinfo(pInd);
        // }
        appEdinfoClient.numServersFound++;
    }
    // otherwise (not new), update the information in the existing list
    else
    {
        while ((pNode != NULL) && (pNode->serverInfo.serverHandle != pInd->serverHandle))
        {
            pNode = pNode->next;
        }

        if (pNode != NULL)
        {
            pNode->serverInfo = *pInd;
        }
        else
        {
            APP_LOG_WARNING("app_EdinfoClientHandleServerInd: Update to server that is not on the list!");
        }
    }

    // TODO: Handle the case of server lost (removed from list)
}

/* EPRI: beg*/
void EVCCSendDevInfo(sl_edinfo_client_send_dinfo_req * req) 
{
    app_edinfo_server_node_t    *pNode = appEdinfoClient.pServerList;
    int index=0;
    sl_edinfo_client_server_ind *pInd;
    pInd = (sl_edinfo_client_server_ind*)KAL_Malloc(sizeof(sl_edinfo_client_server_ind));
    //debug
    //fprintf(stdout, "DEV info HWVer %s \n", req->devInfo.mfHWVersion);
    req->devInfo.mfDate = KAL_GetClock();
    if (appEdinfoClient.numServersFound > 0) 
    {
        if (pNode != NULL)
        {
            while ( (pNode->next != NULL) && (index != APP_EDINFO_CLIENT_SELECT_SERVER_INDEX) )
            {
                pNode = pNode->next;
                index++;
            }
            *pInd = pNode->serverInfo;           
        }
        if (pInd != NULL)
        {
            req->serverSourceInfo.serverHandle =pInd->serverHandle;
            req->serverSourceInfo.source.source =pInd->sourceList[0].source;
            req->serverSourceInfo.source.fsaId =pInd->sourceList[0].fsaId;
        }

        //fprintf(stdout, "DEV info sent to server \n" );
        app_SendMail(SL_EDINFO_CLIENT_SEND_DINFO_REQ, req, sizeof(sl_edinfo_client_send_dinfo_req));

        if (pInd != NULL) 
        {
            KAL_Free(pInd);
        }
    }
}
/* EPRI: end*/


/*******************************************************************************
 * app_EdinfoClientLoadEdinfo()
 ******************************************************************************/
/**
 * \brief Sends requests to the SEP Dinfo Server to create a dinfo resource
 * \param none
 * \return none
 */
static void app_EdinfoClientLoadEdinfo(sl_cmn_client_server_ind_t *pServer)
{
#ifdef APP_PRELOAD_DEMO
    sl_edinfo_client_send_dinfo_req *pReq;

    if (appEdinfoClient.pendingRequest == true)
    {
        return;
    }

    pReq = KAL_Malloc(sizeof(sl_edinfo_client_send_dinfo_req));
    KAL_Memset(pReq, 0, sizeof(sl_edinfo_client_send_dinfo_req));

    // First fill in the device info
#if APP_CONFIG_DRLC_CLIENT_ENABLED
    pReq->devInfo.drlcCapa.averageEnergy.mult  = 3;
    pReq->devInfo.drlcCapa.averageEnergy.value = 32;
    pReq->devInfo.drlcCapa.maxDemand.mult      = 3;
    pReq->devInfo.drlcCapa.maxDemand.value     = 4;
    pReq->devInfo.drlcCapa.optionsImplemented  = SL_XSD_DRLC_OPTIONS_TEMP_SETPOINT | SL_XSD_DRLC_OPTIONS_TEMP_OFFSET;
    pReq->devInfo.drlcCapaExists               = true;
#endif // APP_CONFIG_DRLC_CLIENT_ENABLED

    pReq->includeFuncSets = true;

    // Model Info
    pReq->devInfo.mfDate = KAL_GetClock();
    //snprintf(pReq->devInfo.mfHWVersion, sizeof(pReq->devInfo.mfHWVersion), "%s", "HW Rel 0.0.1");
    //snprintf(pReq->devInfo.mfModel, sizeof(pReq->devInfo.mfModel), "%s", "Grid2home");
    //pReq->devInfo.mfId = IANA_PEN;
    //snprintf(pReq->devInfo.mfInfo, sizeof(pReq->devInfo.mfInfo), "%s", "Kitu Systems");
    //snprintf(pReq->devInfo.mfModel, sizeof(pReq->devInfo.mfModel), "%s", "Grid2home Thermostat");
    //snprintf(pReq->devInfo.mfSerialNum, sizeof(pReq->devInfo.mfSerialNum), "%s", "1234567890");
    //snprintf(pReq->devInfo.mfHWVersion, sizeof(pReq->devInfo.mfHWVersion), "%s", "HW Rel 0.0.1");
    snprintf(pReq->devInfo.mfHWVersion, sizeof(pReq->devInfo.mfHWVersion), "%s", "J3072 Certified");
    snprintf(pReq->devInfo.mfModel, sizeof(pReq->devInfo.mfModel), "%s", "DiamondElectric");
    pReq->devInfo.mfId = 57872; //IANA_PEN;
    snprintf(pReq->devInfo.mfInfo, sizeof(pReq->devInfo.mfInfo), "%s", "OBCM");
    snprintf(pReq->devInfo.mfModel, sizeof(pReq->devInfo.mfModel), "%s", "A142C4Pacifica");
    snprintf(pReq->devInfo.mfSerialNum, sizeof(pReq->devInfo.mfSerialNum), "%s", "DZ453820913");

    pReq->devInfo.swActivationTime = KAL_GetClock();
    snprintf(pReq->devInfo.swVersion, sizeof(pReq->devInfo.swVersion), "%s", APP_CONFIG_SW_VERSION);
    pReq->devInfo.primaryPower   = SL_XSD_POWER_SOURCE_MAIN;
    pReq->devInfo.secondaryPower = SL_XSD_POWER_SOURCE_NONE;

    // Lastly, fill the server info before sending the request
    pReq->serverSourceInfo.serverHandle  = pServer->serverHandle;
    pReq->serverSourceInfo.source.source = pServer->sourceList[0].source;
    pReq->serverSourceInfo.source.fsaId  = pServer->sourceList[0].fsaId;

    appEdinfoClient.pendingRequest       = true;
    appEdinfoClient.selectedServerHandle = pServer->serverHandle;

    app_SendMail(SL_EDINFO_CLIENT_SEND_DINFO_REQ, pReq, sizeof(sl_edinfo_client_send_dinfo_req));

    KAL_Free(pReq);

#endif
}

/*******************************************************************************
 * app_EdinfoClientHandleSendDinfoCnf()
 ******************************************************************************/
/**
 * \brief Handle confirmation of the dinfo request
 * \param none
 * \return none
 */
static void app_EdinfoClientHandleSendDinfoCnf(sl_edinfo_client_send_dinfo_cnf *pCnf)
{
    if (appEdinfoClient.pendingRequest == false)
    {
        APP_LOG_WARNING("app_EdinfoClientHandleSendDinfoCnf: Unexpected confirmation from SEP");
        return;
    }

    appEdinfoClient.pendingRequest = false;

    if (pCnf->status != SL_CMN_RC_SUCCESS)
    {
        APP_LOG_WARNING("app_EdinfoClientHandleSendDinfoCnf: Unsucessful confirm from SEP");
        // TODO: Go through any other servers in the list and try to register with them
    }
    else
    {
        appEdinfoClient.serverSelected = true;
    }
}


/*******************************************************************************
 * app_EdinfoClientHandleMail()
 ******************************************************************************/
void app_EdinfoClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
    switch (mailId)
    {
    case SL_EDINFO_CLIENT_SERVER_IND:
        APP_LOG_INFO("app_EdinfoClientHandleMail: SL_EDINFO_CLIENT_SERVER_IND");
        app_EdinfoClientHandleServerInd((sl_edinfo_client_server_ind *)pMail);

        break;

    case SL_EDINFO_CLIENT_SEND_DINFO_CNF:
        APP_LOG_INFO("app_EdinfoClientHandleMail: SL_EDINFO_CLIENT_SEND_DINFO_CNF");
        app_LogStatus(((sl_edinfo_client_send_dinfo_cnf *)pMail)->status);
        app_EdinfoClientHandleSendDinfoCnf((sl_edinfo_client_send_dinfo_cnf *)pMail);
        break;
    }
}

#endif // APP_CONFIG_EDINFO_CLIENT_ENABLED
