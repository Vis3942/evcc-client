/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_client.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#include <string.h>
#include <stdlib.h>
#include "kal_api.h"
#include "sl_api.h"
#include "app_config.h"
#include "app_client.h"
#include "app_utils.h"
#if APP_CONFIG_EPS_CLIENT_ENABLED
#include "app_epwrstat_client.h"
#endif
#include "fsal.h"


#include "usock_proto.h"
#include "app_der_client.h"
#include "app_eder_client.h"

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

/*******************************************************************************
 * Local Data
 ******************************************************************************/

/*******************************************************************************
 * app_ClientGetTransactionID()
 ******************************************************************************/
/**
 * \brief Provides a unique transaction ID by maintaining an internal counter.
 * \param none
 * \return none
 */
u32_t app_ClientGetTransactionID(void)
{
  static u32_t transactionID = 0;

  transactionID++;

  return(transactionID);
}


/*******************************************************************************
 * app_ClientFindRootIndex()
 ******************************************************************************/
/**
 * \brief Finds the root/source info in pServerNode that matches rootInfo
 * \param pServerNode - The server node to search
 * \param rootInfo - The root info to match
 * \param index - Returns the index of the matching info if it exists
 * \return true if match found, else false
 */
bool app_ClientFindRootIndex(app_client_server_node_t *pServerNode, sl_cmn_client_root_source_info_t rootInfo, u8_t *index)
{
  u8_t x;

  for (x=0; x<pServerNode->pServer->numSources; x++)
  {
    // check if rootInfo source is in the server source list
    if ((pServerNode->pServer->sourceList[x].source & rootInfo.source) == rootInfo.source)
      // check if rootInfo fsaId is in the server source list
      if ((pServerNode->pServer->sourceList[x].fsaId & rootInfo.fsaId) == rootInfo.fsaId)
      {
        *index = x;
        return(true);
      }
  }

  return(false);
}

/*******************************************************************************
 * app_ClientFindServerNode()
 ******************************************************************************/
/**
 * \brief Finds a server node based on serverHandle
 * \param serverHandle - serverHandle to match
 * \return Pointer to matching server node if it exists
 */
app_client_server_node_t * app_ClientFindServerNode(sl_cmn_client_server_handle_t serverHandle, app_client_server_node_t *pServerHead)
{
  app_client_server_node_t *pServerNode = pServerHead;

  while (pServerNode != NULL)
  {
    if (pServerNode->pServer->serverHandle == serverHandle)
      return(pServerNode);

    pServerNode = pServerNode->next;
  }

  return(NULL);
}

/*******************************************************************************
 * app_ClientRegistered()
 ******************************************************************************/
/**
 * \brief Determines if the client is registered on any of the servers in the list
 * \param ppServerHead - pointer to the head of the linked list to store the data
 * \return true if registered on at least one server
 */
bool app_ClientRegistered(app_client_server_node_t *pServerHead)
{
  app_client_server_node_t *pServerNode;

  // First determine if there is already a node with a matching serverHandle.
  // If so, overwrite it or delete it if lost
  pServerNode = pServerHead;
  while (pServerNode != NULL)
  {
    if (pServerNode->pServer->registered == true)
      return(true);
    pServerNode = pServerNode->next;
  }

  return(false);
}

/*******************************************************************************
 * app_ClientStoreProgramNode()
 ******************************************************************************/
/**
 * \brief Store received program information.
 * \param status - Status of the program
 * \param dataExists - Indicates whether the program data is provided
 * \param pData - pointer to the program data, NULL if dataExists is false
 * \param dataSize - size of the program data
 * \param serverSourceInfo - server and source of the program
 * \param mrId - pinter to mrId of the program
 * \param pProgramHead - List the program belongs to
 * \return none.
 */
void app_ClientStoreProgramNode(sl_cmn_prog_status_t status, bool dataExists, void *pData, u16_t dataSize, u8_t *mrId,
                                sl_cmn_client_server_source_info_t serverSourceInfo, app_client_program_node_t **ppProgramHead)
{
  app_client_program_node_t *pCompareNode;
  app_client_program_node_t *pPrevCompareNode;
  app_client_program_node_t *pTailNode = NULL;
  app_client_program_node_t *pNewNode = NULL;

  if (status == SL_CMN_PROG_STATUS_NONE)
    return;  // the server has no programs

  if (SL_CMN_CLIENT_ROOT_SOURCE_FSA & serverSourceInfo.source.source == 0)
  {
    printf("#####>> received non FSA source: %02x\n",serverSourceInfo.source.source);
    return;
  }

  if (dataExists == false)
    return;

  printf("######>> received DERP from FSA %d\n", dataExists);

  // First determine if there is already a node with a matching mrId.
  // If so, overwrite it or delete it if lost
  pCompareNode = *ppProgramHead;
  pPrevCompareNode = NULL;
  while (pCompareNode != NULL)
  {
    if (KAL_Memcmp(pCompareNode->mrId, mrId, SL_XSD_MRID_LEN) == 0)
    {
      // found existing node
      pNewNode = pCompareNode;

      if (status == SL_CMN_PROG_STATUS_DELETE)
      {
        // Unlink and free node
        if (pCompareNode == *ppProgramHead)
          *ppProgramHead = pCompareNode->next;
        else
          pPrevCompareNode->next = pCompareNode->next;

        if (pCompareNode->pProgramData != NULL)
          KAL_Free(pCompareNode->pProgramData);
        KAL_Free(pCompareNode);

        // Exit, processing complete
        return;
      }

      // Stop looping through program list
      break;
    }
    pTailNode = pCompareNode;
    pPrevCompareNode = pCompareNode;
    pCompareNode = pCompareNode->next;
  }

  if (pNewNode == NULL)
  {
    // Did not find the node so create one and add it to the list.
    pNewNode = KAL_Malloc(sizeof(app_client_program_node_t));
    KAL_Memset(pNewNode, 0, sizeof(app_client_program_node_t));

    if (dataExists == true)
      pNewNode->pProgramData = KAL_Malloc(dataSize);

    KAL_Memcpy(pNewNode->mrId, mrId, SL_XSD_MRID_LEN);

    // Add the new program to the tail of the list.
    if (pTailNode != NULL)
      pTailNode->next = pNewNode;
    else
      *ppProgramHead = pNewNode;
  }

  pNewNode->serverSourceInfo = serverSourceInfo;

  if (dataExists == true)
  {
    // Copy Data
    KAL_Memcpy(pNewNode->pProgramData, pData, dataSize);
  }
  else if (pNewNode->pProgramData != NULL)
  {
    // Free old data
    KAL_Free(pNewNode->pProgramData);
    pNewNode->pProgramData = NULL;
  }
}

/*******************************************************************************
 * app_ClientStoreServerNode()
 ******************************************************************************/
/**
 * \brief Store received server/source information.
 * \param pInd - pointer to the received data.
 * \param ppServerHead - pointer to the head of the linked list to store the data
 * \return none.
 */
void app_ClientStoreServerNode(sl_cmn_client_server_ind_t *pInd, app_client_server_node_t **ppServerHead)
{
  app_client_server_node_t *pCompareNode;
  app_client_server_node_t *pPrevCompareNode;
  app_client_server_node_t *pTailNode = NULL;
  app_client_server_node_t *pNewNode = NULL;

  // First determine if there is already a node with a matching serverHandle.
  // If so, overwrite it or delete it if lost
  pCompareNode = *ppServerHead;
  pPrevCompareNode = NULL;
  while (pCompareNode != NULL)
  {
    if (pCompareNode->pServer->serverHandle == pInd->serverHandle)
    {
      // found existing node
      pNewNode = pCompareNode;

      if (pInd->status == SL_CMN_SERVER_STATUS_LOST)
      {
        // Unlink and free node
        if (pCompareNode == *ppServerHead)
          *ppServerHead = pCompareNode->next;
        else
          pPrevCompareNode->next = pCompareNode->next;

        if (pCompareNode->pServer != NULL)
          KAL_Free(pCompareNode->pServer);
        KAL_Free(pCompareNode);

        // Exit, processing complete
        return;
      }

      // Stop looping through server list
      break;
    }
    pTailNode = pCompareNode;
    pPrevCompareNode = pCompareNode;
    pCompareNode = pCompareNode->next;
  }

  if (pNewNode == NULL)
  {
    // Did not find the node so create one and add it to the list.
    pNewNode = KAL_Malloc(sizeof(app_client_server_node_t));
    KAL_Memset(pNewNode, 0, sizeof(app_client_server_node_t));

    pNewNode->pServer = KAL_Malloc(sizeof(sl_cmn_client_server_ind_t));

    // Add the new server to the tail of the list.
    if (pTailNode != NULL)
      pTailNode->next = pNewNode;
    else
      *ppServerHead = pNewNode;
  }

  // Copy Data
  KAL_Memcpy(pNewNode->pServer, pInd, sizeof(sl_cmn_client_server_ind_t));
}

// void  app_ClientHandleDERCInd(sl_der_client_derc_ind *pInd)
// {
//   if (pInd->derc.dercBase.opModFreqDroopExists) {
//     send_freq_droop_data(pInd->derc.dercBase.opModFreqDroop);
//   }

// }


/*******************************************************************************
 * app_ClientHandleSLClientEventInd()
 ******************************************************************************/
/**
 * \brief Handles client events
 * \param event - event rxed from the SL client
 * \return none
 */
void app_ClientHandleSLClientEventInd(u8_t event, bool opt_out)
{
  switch (event)
  {
  case SL_XSD_RESP_STATUS_MSG_RXED:
    APP_LOG_INFO("app_HandleMail: Rxed Event Rxed Event");
    break;
  case SL_XSD_RESP_STATUS_EVENT_STARTED:
    if (opt_out == true)
      APP_LOG_INFO("app_HandleMail: Rxed Event Start Event (Opted-Out)");
    else
    {
      APP_LOG_INFO("app_HandleMail: Rxed Event Start Event");
    }
    break;
  case SL_XSD_RESP_STATUS_EVENT_COMPLETED:
    APP_LOG_INFO("app_HandleMail: Rxed Event Complete Event");
    break;
  case SL_XSD_RESP_STATUS_EVENT_CANCELED:
    APP_LOG_INFO("app_HandleMail: Rxed Event Complete(Canceled) Event");
    break;
  case SL_XSD_RESP_STATUS_EVENT_SUPERSEDED:
    APP_LOG_INFO("app_HandleMail: Rxed Event Complete(Superseded) Event");
    break;
  case SL_XSD_RESP_STATUS_EVENT_COMPLETE_NO_PARTICIPATION:
    APP_LOG_INFO("app_HandleMail: Rxed Event Complete(No Participation) Event");
    break;
  case SL_XSD_RESP_STATUS_REJECT_CTRL_NOT_APPLICABLE:
  case SL_XSD_RESP_STATUS_REJECT_INVALID_EVENT:
  case SL_XSD_RESP_STATUS_REJECT_EVENT_RX_AFTER_EXP:
    APP_LOG_INFO("app_HandleMail: Rxed Event Rejected Event");
    break;
  case SL_XSD_RESP_STATUS_ABORTED_ALTERNATE_EVENT:
    APP_LOG_INFO("app_HandleMail: Rxed Event Complete(Aborted Alternate) Event");
    break;
  case SL_XSD_RESP_STATUS_OPT_OUT:
    APP_LOG_INFO("app_HandleMail: Rxed Event Opt-Out Event");
    break;
  case SL_XSD_RESP_STATUS_OPT_IN:
    APP_LOG_INFO("app_HandleMail: Rxed Event Opt-In Event");
    break;
  case SL_XSD_RESP_STATUS_EVENT_PART_COMPLETE_OPT_OUT:
    APP_LOG_INFO("app_HandleMail: Rxed Event Complete(Partial Opt-Out) Event");
    break;
  case SL_XSD_RESP_STATUS_EVENT_PART_COMPLETE_OPT_IN:
    APP_LOG_INFO("app_HandleMail: Rxed Event Complete(Partial Opt-In) Event");
    break;
  }
}

/*******************************************************************************
* app_ClientProcessing()
******************************************************************************/
/**
 * \brief This function is called periodically on timer expiration and triggers
 *        the individual function sets to perform periodic processing.
 * \param none
 * \return none
 */
extern void app_DercPulling(void);
extern void app_DerpPulling(void);
void app_ClientProcessing( void )
{
  //app_EpwrstatClientProcessing();

  // Pull DERCList every 120 seconds
  static u16_t dercTimer = 0;
  if (dercTimer++ >= 120)
  {
    app_DercPulling();
    dercTimer = 0;
  }

  // Pull DERPList every 63 seconds
  // optional (DefaultDERC is not getting polled, so using this function to get the DefaultDerc and DERPList )
#if 1
  static u16_t derpTimer = 0;
  if (derpTimer++ >= 63)
  {
    app_DerpPulling();
    derpTimer = 0;
  }
#endif

  // post status every second as required by SUNSPEC Profile
  EVCCSendDersPeriodic();

}

/*******************************************************************************
 * app_ClientStart()
 ******************************************************************************/
/**
 * \brief Called after the G2H stack is started to load client data to G2H Stack
 * \param none
 * \return none
 */
void app_ClientStart(void)
{
#ifdef APP_PRELOAD_DEMO
#endif

#ifdef APP_RPI_ENABLED
  RPi_setup(RPI_EVSE_PIN);
  RPi_set_io(RPI_EVSE_PIN, 1);  // enable charging (default)
#endif
}
