/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_edev_client.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application End Device Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#include "sl_api.h"
#include <string.h>
#include <stdio.h>
#include "kal_api.h"
#include "app_api.h"
#include "app_config.h"
#include "app_edev_client.h"
#include "app_utils.h"

#if APP_CONFIG_EDEV_CLIENT_ENABLED

/*******************************************************************************
 * Private Constants
 ******************************************************************************/
#define APP_EDEV_CLIENT_SELECT_SERVER_INDEX     0  // Example of choosing a server to register to


/*******************************************************************************
 * Private Data
 ******************************************************************************/

typedef struct edev_server_node {
  sl_edev_client_server_found_ind serverInfo;
  struct edev_server_node         *next;
} edev_server_node_t;

struct {
  bool                          pendingRequest;
  u32_t                         pendingCancel;
  edev_server_node_t            *pServerList;
  u8_t                          numServersFound;
} appEdevClient = {
    false, //   pendingRequest
    0,     //   pendingCancel
    NULL,  //  *pServerListHead
    0,     // numServersFound
};


/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void app_EdevClientHandleServerInd(sl_edev_client_server_found_ind *pInd);
static void app_EdevClientSendEdevRegRequest(sl_cmn_client_server_ind_t *pServer);
static void app_EdevClientHandleSendEdevCnf(sl_edev_client_send_enddev_cnf *pCnf);

/*******************************************************************************
 * app_EdevClientHandleServerInd() -- Only process if status is known (ignore unknown)
 ******************************************************************************/
static void app_EdevClientHandleServerInd(sl_edev_client_server_found_ind *pInd)
{
  int index=-1;
  bool found=false;
  edev_server_node_t *pNode = appEdevClient.pServerList;
  edev_server_node_t *pPrevNode = NULL;
  edev_server_node_t *pNew;

  if (pInd->serverStatus == SL_EDEV_CLIENT_SERVER_STATUS_UNKNOWN)
    return;     // Ignore and wait for a meaningful status

  // First, check if this is a unique server.
  while (pNode != NULL)
  {
    ++index;
    if (pNode->serverInfo.serverDetails.serverHandle == pInd->serverDetails.serverHandle)
    {
      found = true;
      break;
    }
    pPrevNode = pNode;
    pNode = pNode->next;
  }

  // If unique, then add to the end of the list
  if (found == false)
  {
    if (pInd->serverDetails.status == SL_CMN_SERVER_STATUS_LOST)
      return;  // Don't save the server info

    pNew = KAL_Malloc(sizeof(edev_server_node_t));
    if (pNew == NULL)
    {
      APP_LOG_WARNING("app_EdevClientHandleServerInd: Cannot allocate memory for new server node");
      return;
    }
    pNew->serverInfo = *pInd;
    pNew->next = NULL;

    pNode = appEdevClient.pServerList;

    index=0;
    if (pNode == NULL)
    {
      appEdevClient.pServerList = pNew;
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

    appEdevClient.numServersFound++;
  }
  // If not unique, then update the server information in our database
  else
  {
    if (pInd->serverDetails.status == SL_CMN_SERVER_STATUS_LOST)
    {
      // Remove server
      if (pPrevNode == NULL)
        appEdevClient.pServerList = pNode->next;
      else
        pPrevNode->next = pNode->next;

      KAL_Free(pNode);
      appEdevClient.numServersFound--;
    }
    else
    {
      // Update server
      pNode->serverInfo = *pInd;
    }
  }
}

/*******************************************************************************
  * app_EdevClientSendEdevRegRequest()
  ******************************************************************************/
static void app_EdevClientSendEdevRegRequest(sl_cmn_client_server_ind_t *pServer)
{
    sl_edev_client_send_enddev_req *pReq;

    // Check to make sure there is no pending uncomfirmed request (transaction ID is non-zero)
    if (appEdevClient.pendingRequest == true)
        return;     // already pending transaction, so return without action

    // Send the post request, assuming the "root" is the first one in the Server record

    pReq = KAL_Malloc(sizeof(sl_edev_client_send_enddev_req));
    if (pReq == NULL)
    {
        APP_LOG_WARNING("app_EdevClientSendEdevRegRequest: Cannot allocate memory for edev send");
        return;
    }

    pReq->serverSourceInfo.serverHandle = pServer->serverHandle;
    pReq->serverSourceInfo.source.source = pServer->sourceList[0].source;
    pReq->serverSourceInfo.source.fsaId = pServer->sourceList[0].fsaId;

    app_SendMail(SL_EDEV_CLIENT_SEND_ENDDEV_REQ, pReq, sizeof(sl_edev_client_send_enddev_req));

    appEdevClient.pendingRequest = true;
}


/*******************************************************************************
  * app_EdevClientHandleSendEdevCnf()
  ******************************************************************************/
static void app_EdevClientHandleSendEdevCnf(sl_edev_client_send_enddev_cnf *pCnf)
{
    if (appEdevClient.pendingRequest == false)
    {
        APP_LOG_WARNING("app_EdevClientHandleSendEdevCnf: Unexpected confirmation from SEP");
        return;
    }

    appEdevClient.pendingRequest = false;

    if (pCnf->status != SL_CMN_RC_SUCCESS)
    {
        APP_LOG_WARNING("app_EdevClientHandleSendEdevCnf: Unsucessful confirm from SEP");
        // TODO: Go through any other servers in the list and try to register with them
    }
}



/*******************************************************************************
 * app_EdevClientHandleMail()
 ******************************************************************************/
void app_EdevClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
  switch (mailId)
  {
  case SL_EDEV_CLIENT_CANCEL_GET_EDEV_LIST_CNF:
    APP_LOG_INFO("app_EdevClientHandleMail: SL_EDEV_CLIENT_CANCEL_GET_EDEV_LIST_CNF");
    break;
  case SL_EDEV_CLIENT_EDEV_LIST_IND:
    APP_LOG_INFO("app_EdevClientHandleMail: SL_EDEV_CLIENT_EDEV_LIST_IND");
    app_LogStatus(((sl_edev_client_edev_list_ind *)pMail)->results.status);
    break;
  case SL_EDEV_CLIENT_FIND_SERVER_CNF:
    APP_LOG_INFO("app_EdevClientHandleMail: SL_EDEV_CLIENT_FIND_SERVER_CNF");
    app_LogStatus(((sl_edev_client_find_server_cnf *)pMail)->status);
    break;
  case SL_EDEV_CLIENT_GET_EDEV_LIST_CNF:
    APP_LOG_INFO("app_EdevClientHandleMail: SL_EDEV_CLIENT_GET_EDEV_LIST_CNF");
    app_LogStatus(((sl_edev_client_get_edev_list_cnf *)pMail)->status);
    break;
  case SL_EDEV_CLIENT_SEND_ENDDEV_CNF:
    APP_LOG_INFO("app_EdevClientHandleMail: SL_EDEV_CLIENT_SEND_ENDDEV_CNF");
    app_EdevClientHandleSendEdevCnf((sl_edev_client_send_enddev_cnf *) pMail);
    app_LogStatus(((sl_edev_client_send_enddev_cnf *)pMail)->status);
    break;
  case SL_EDEV_CLIENT_SERVER_FOUND_IND:
    APP_LOG_INFO("app_EdevClientHandleMail: SL_EDEV_CLIENT_SERVER_FOUND_IND");
    app_EdevClientHandleServerInd((sl_edev_client_server_found_ind *) pMail);
    break;
  case SL_EDEV_CLIENT_SET_PIN_CNF:
    APP_LOG_INFO("app_EdevClientHandleMail: SL_EDEV_CLIENT_SET_PIN_CNF");
    app_LogStatus(((sl_edev_client_set_pin_cnf *)pMail)->status);
    break;
  }
}

#endif // APP_CONFIG_EDEV_CLIENT_ENABLED


