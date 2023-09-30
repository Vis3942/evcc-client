/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_der_client.c
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
#include "sl_api.h"
#include "kal_api.h"
#include "app_client.h"
#include "app_config.h"
#include "app_utils.h"
#include "app_der_client.h"

#include "usock_proto.h"

// This define is only to be used when sending commands from SECC
// till we debug the issue of CLIENT_DERC_IND is debugged
//   this is using DERC_LIST_INDICATION
#define DERC_EVENT_WORKAROUND 1
int sent_active_derc = 0;
sl_der_client_derc_ind copy_sent_ind = {0};
/*******************************************************************************
 * Local Data
 ******************************************************************************/

struct {
  app_client_server_node_t    *pServerList;
  app_client_program_node_t   *pProgramList;
  sl_cmn_client_program_list_t selectedEventProgs;
  sl_cmn_client_root_list_t    selectedEventRoots;
  u8_t                         numEventProgs;
  u8_t                         numEventRoots;
} appDerClient = {
    NULL,  // *pServerList
    NULL,  // *pProgramList
    {0},   // selectedEventProgs
    {0},   // selectedEventRoots
    0,     // numEventProgs
    0,     // numEventRoots
};

/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void HandleDerEventProgramsInd(sl_der_client_event_programs_ind *pInd);
static void HandleDerEventRootsInd(sl_der_client_event_roots_ind *pInd);
static void HandleDerpInd(sl_der_client_derp_ind *pInd);
static void HandleDerServerInd(sl_der_client_server_ind *pInd);
static void SetDerEventPrograms(void);
static void SetDerEventRoots(void);


/*******************************************************************************
 * HandleDerEventProgramsInd()
 ******************************************************************************/
/**
 * \brief Handles the SL_DER_CLIENT_EVENT_PROGRAMS_IND message.
 * \param pInd - pointer to the received indication
 * \return none.
 */
static void HandleDerEventProgramsInd(sl_der_client_event_programs_ind *pInd)
{
  // Just store the new list
  appDerClient.selectedEventProgs = pInd->list;
  appDerClient.numEventProgs = pInd->numListEntries;
}

/*******************************************************************************
 * HandleDerEventRootsInd()
 ******************************************************************************/
/**
 * \brief Handles the SL_DER_CLIENT_EVENT_ROOTS_IND message.
 * \param pInd - pointer to the received indication
 * \return none.
 */
static void HandleDerEventRootsInd(sl_der_client_event_roots_ind *pInd)
{
  // Just store the new list
  appDerClient.selectedEventRoots = pInd->rootList;
  appDerClient.numEventRoots = pInd->numRoots;
}

/*******************************************************************************
 * HandleDerpInd()
 ******************************************************************************/
/**
 * \brief Handles the SL_DER_CLIENT_DERP_IND message.
 * \param pInd - pointer to the received indication
 * \return none.
 */
static void HandleDerpInd(sl_der_client_derp_ind *pInd)
{
  // Store the program node
  app_ClientStoreProgramNode(pInd->status, pInd->derpExists, &pInd->derp, sizeof(sl_der_derp_t), pInd->derp.mrId, pInd->serverSourceInfo, &appDerClient.pProgramList);

  SetDerEventPrograms();
}

/* EPRI code: Beg*/
void app_ClientHandleDefaultDERCInd(sl_der_client_default_derc_ind *pInd) 
{
  if (pInd->ddercExists)
  {
    send_default_derc(pInd->dderc);
  }
  else 
  {
    fprintf(stdout, ">>>> DefaultDERC recvd: dderc does not exist\n");
  }
}

void app_ClientHandleDERPListInd(sl_der_client_derp_list_ind *pInd)
{
  int ii;
  int found_dderc = 0;
  if (pInd->results.numList > 0)
  {
    for (ii=0; ii < pInd->results.numList; ii++ )
    {
      if (pInd->list[ii].ddercExists)
      {
        send_default_derc(pInd->list[ii].dderc);
        found_dderc ++;
      }
    }
    if (found_dderc <= 0)
    {
      fprintf(stdout, ">>>> DERP IND list does not contain any DDERC values\n");
    }
  }
  else 
  {
    fprintf(stdout, ">>>> DERP Ind recvd, zero results.\n");
  }
  
}


void app_ClientHandleDCInd(sl_der_client_dc_ind *pInd)
{
  send_curve_data(pInd->curve);
}

void  app_ClientHandleDERCInd(sl_der_client_derc_ind *pInd)
{
  if (pInd->derc.dercBase.opModFreqDroopExists) {
    send_freq_droop_data(pInd->derc.dercBase.opModFreqDroop);
    return;
  }
  // TODO implement this function
  send_derc_ind(pInd);
  //fprintf(stdout, "NOT implemented : send derc indication yet. TODO \n");
}

void app_ClientHandleDERCListInd(sl_der_client_derc_list_ind *pInd)
{
  sl_der_client_derc_ind sendInd = {0};
  if (pInd->results.numList > 0)
  {
    KAL_Memcpy(&sendInd.derc, &pInd->list[0].derc, sizeof(sl_der_derc_t));
    // debug
    fprintf(stdout, "=x=x=x=x=x=> DERC ind sts = %d, activeinfo sts %d\n",
      pInd->list[0].eventStatusCurrStatus, pInd->list[0].activeInfo.eventRespStatus);

    sendInd.activeInfo.eventRespStatus = pInd->list[0].activeInfo.eventRespStatus;
    sendInd.eventStatusCurrStatus = pInd->list[0].eventStatusCurrStatus;
    sendInd.eventStatusDateTime = pInd->list[0].eventStatusDateTime;
    
#ifdef DERC_EVENT_WORKAROUND
    if (sendInd.eventStatusCurrStatus == SL_XSD_EVENT_CURR_STATUS_ACTIVE )
    {
      sendInd.activeInfo.eventRespStatus = SL_XSD_RESP_STATUS_EVENT_STARTED;
      sent_active_derc = 1;
    }
    fprintf(stdout, "=x=x=x=x=x=> in workaround DERC ind sts = %d, activeinfo sts %d\n",
      sendInd.eventStatusCurrStatus, sendInd.activeInfo.eventRespStatus);
    KAL_Memcpy(&copy_sent_ind, &sendInd, sizeof(sl_der_client_derc_ind));
#endif
    send_derc_ind(&sendInd);
  }
  else
  {
    fprintf(stdout, ">>>>>  recvd DERC List ind, numList = %d\n", pInd->results.numList);
#ifdef DERC_EVENT_WORKAROUND
    if (sent_active_derc == 1) 
    {
      sent_active_derc = 0;
      KAL_Memcpy(&sendInd, &copy_sent_ind, sizeof(sl_der_client_derc_ind));
      sendInd.activeInfo.eventRespStatus = SL_XSD_RESP_STATUS_EVENT_COMPLETED;
      send_derc_ind(&sendInd);      
    }
  }
#endif
}

/* EPRI code: End*/

/*******************************************************************************
 * HandleDerServerInd()
 ******************************************************************************/
/**
 * \brief Handles the SL_DER_CLIENT_SERVER_IND message.
 * \param pInd - pointer to the received indication
 * \return none.
 */
static void HandleDerServerInd(sl_der_client_server_ind *pInd)
{

  // debug
  int ii;
  fprintf(stdout, "------<<<< [DER SERVER IND dbg] Num sources %d, registered %d\n", pInd->numSources, pInd->registered);
  for (ii=0; ii < pInd->numSources; ii++)
  {
    fprintf(stdout, "------<<<< [DER SERVER IND dbg]  [%d]: source %d, time syncd %d, fsa id %d\n", ii, 
      pInd->sourceList[ii].source, pInd->sourceList[ii].timeSynced, pInd->sourceList[ii].fsaId);
  }

  // Store the server node
  app_ClientStoreServerNode(pInd, &appDerClient.pServerList);
  
  SetDerEventRoots();
}

/*******************************************************************************
 * SetDerEventRoots()
 ******************************************************************************/
/**
 * \brief Selects event roots from a list of potential roots based on
 *        app-specific priorities.  Up to SL_CMN_CLIENT_MAX_NUM_EVENT_ROOTS
 *        are selected.
 *
 * \return none.
 */
static void SetDerEventRoots(void)
{
  sl_der_client_set_event_roots_req req = {0};
  app_client_server_node_t *pServerNode = appDerClient.pServerList;
  u8_t x;
  bool registered = app_ClientRegistered(appDerClient.pServerList);
  int ii;
  fprintf(stdout, "[SetDEREventRoots-funcbeg] =====>>>>> registered = %d\n", registered);

  // Select roots that are from FSA
  // loop through the available servers
  pServerNode = appDerClient.pServerList;
  while (pServerNode != NULL)
  {
    fprintf(stdout, "[SetDEREventRoots-funcbeg] =====>>>>> numsrcs = %d\n", pServerNode->pServer->numSources);
    // loop through the available roots/sources
    for (x=0; x<pServerNode->pServer->numSources; x++)
    {
      // debug
      fprintf(stdout, "[SetDEREventRoots-funcbeg] =====>>>>> time synced %d, server registered = %d\n", 
        pServerNode->pServer->sourceList[x].timeSynced, pServerNode->pServer->registered);
      // if it is time synced
      if (pServerNode->pServer->sourceList[x].timeSynced == true)
      {
        // if it meets registration requirements
        if ((!registered) ||
            ((registered) && (pServerNode->pServer->registered)))
        {
          fprintf(stdout, "[SetDEREventRoots-functop] =====>>>>> source = %d\n", pServerNode->pServer->sourceList[x].source);
          // if it is from Function Set Assignments
          if (pServerNode->pServer->sourceList[x].source & SL_CMN_CLIENT_ROOT_SOURCE_FSA)
          {
            req.rootList.list[req.numRoots].serverHandle = pServerNode->pServer->serverHandle;
            req.rootList.list[req.numRoots].source.source = pServerNode->pServer->sourceList[x].source;
            req.rootList.list[req.numRoots].source.fsaId = pServerNode->pServer->sourceList[x].fsaId;
            req.numRoots++;

            if (req.numRoots == SL_CMN_CLIENT_MAX_NUM_EVENT_ROOTS)
            {
              // debug
              fprintf(stdout, "[SetDEREventRoots-functop] =====>>>>> numroots = %d\n", req.numRoots);
              for (ii=0; ii<req.numRoots; ii++)
              {
                fprintf(stdout, "[SetDEREventRoots-functop] =====>>>>> [%d]: serv handle %d", ii, req.rootList.list[ii].serverHandle);
                fprintf(stdout, "[SetDEREventRoots-functop] =====>>>>> fsaID [%d], source %d \n", 
                  req.rootList.list[ii].source.fsaId, req.rootList.list[ii].source.source);
              }
              app_SendMail(SL_DER_CLIENT_SET_EVENT_ROOTS_REQ, &req, sizeof(sl_der_client_set_event_roots_req));
              return;
            }
          }
        }
      }
    }
    pServerNode = pServerNode->next;
  }

  // Select any remaining roots to fill selectedRootList

  // loop through the available servers
  pServerNode = appDerClient.pServerList;
  while (pServerNode != NULL)
  {
    fprintf(stdout, "[SetDEREventRoots-funcmid] =====>>>>> numsrcs = %d\n", pServerNode->pServer->numSources);
    // loop through the available roots/sources
    for (x=0; x<pServerNode->pServer->numSources; x++)
    {
      fprintf(stdout, "[SetDEREventRoots-funcmid] =====>>>>> time synced %d, server registered = %d\n", 
        pServerNode->pServer->sourceList[x].timeSynced, pServerNode->pServer->registered);
      // if it is time synced
      if (pServerNode->pServer->sourceList[x].timeSynced == true)
      {
        // if it meets registration requirements
        if ((!registered) ||
            ((registered) && (pServerNode->pServer->registered)))
        {
          fprintf(stdout, "[SetDEREventRoots-funcmid] =====>>>>> source = %d\n", pServerNode->pServer->sourceList[x].source);
          // if it is not from Function Set Assignments
          if (!(pServerNode->pServer->sourceList[x].source & SL_CMN_CLIENT_ROOT_SOURCE_FSA))
          {
            req.rootList.list[req.numRoots].serverHandle = pServerNode->pServer->serverHandle;
            req.rootList.list[req.numRoots].source.source = pServerNode->pServer->sourceList[x].source;
            req.rootList.list[req.numRoots].source.fsaId = pServerNode->pServer->sourceList[x].fsaId;
            req.numRoots++;

            if (req.numRoots == SL_CMN_CLIENT_MAX_NUM_EVENT_ROOTS)
            {
              // debug
              fprintf(stdout, "[SetDEREventRoots-funcbot] =====>>>>> numroots = %d\n", req.numRoots);
              for (ii=0; ii<req.numRoots; ii++)
              {
                fprintf(stdout, "[SetDEREventRoots-funcbot] =====>>>>> [%d]: serv handle %d", ii, req.rootList.list[ii].serverHandle);
                fprintf(stdout, "[SetDEREventRoots-funcbot] =====>>>>> fsaID [%d], source %d \n", 
                  req.rootList.list[ii].source.fsaId, req.rootList.list[ii].source.source);
              }
              app_SendMail(SL_DER_CLIENT_SET_EVENT_ROOTS_REQ, &req, sizeof(sl_der_client_set_event_roots_req));
              return;
            }
          }
        }
      }
    }
    pServerNode = pServerNode->next;
  }

  app_SendMail(SL_DER_CLIENT_SET_EVENT_ROOTS_REQ, &req, sizeof(sl_der_client_set_event_roots_req));
}


/*******************************************************************************
 * SetDerEventPrograms()
 ******************************************************************************/
/**
 * \brief Selects the first SL_CMN_CLIENT_MAX_NUM_EVENT_PROGRAMS programs received
 *        that belong to the selected server/root.
 * \return none.
 */
static void SetDerEventPrograms(void)
{
  sl_der_client_set_event_programs_req req = {0};
  app_client_program_node_t *pProgramNode;
  int ii;
  char mrid_str[256];

  pProgramNode = appDerClient.pProgramList;
  while (pProgramNode != NULL)
  {
    // Only select programs from selected server/roots
    if ((pProgramNode->serverSourceInfo.serverHandle == appDerClient.selectedEventRoots.list[0].serverHandle) &&
        (pProgramNode->serverSourceInfo.source.source & appDerClient.selectedEventRoots.list[0].source.source) &&
        ((appDerClient.selectedEventRoots.list[0].source.source & ~SL_CMN_CLIENT_ROOT_SOURCE_FSA) ||
         (pProgramNode->serverSourceInfo.source.fsaId & appDerClient.selectedEventRoots.list[0].source.fsaId)))
    {
      KAL_Memcpy(req.list.mrId[req.numListEntries], pProgramNode->mrId, SL_XSD_MRID_LEN);
      req.numListEntries++;

      if (appDerClient.numEventProgs == SL_CMN_CLIENT_MAX_NUM_EVENT_PROGRAMS)
        break;
    }
    pProgramNode = pProgramNode->next;
  }

  if (!((appDerClient.numEventProgs == 0) && (req.numListEntries == 0)))  // Don't send if no active programs and no programs to set
  {
    printf("## Set DER Event Program:");
    for (int i=0; i < SL_XSD_MRID_LEN; i++)
    {
      printf("%02x", req.list.mrId[0][i]);
    }
    printf("\n");
    // debug 
    fprintf(stdout, "[SetDEREventPrograms] =====>>>>> num list entries = %d\n", req.numListEntries);
    for (ii=0; ii< req.numListEntries; ii++)
    {
      memset(mrid_str, 0, 256);
      app_MridtoString256(&req.list.mrId[ii], mrid_str);
      fprintf(stdout, "[SetDEREventPrograms] =====>>>>> index [%d]: mrid %s\n", ii, mrid_str);
    }
    app_SendMail(SL_DER_CLIENT_SET_EVENT_PROGRAMS_REQ, &req, sizeof(sl_der_client_set_event_programs_req));
  }
}

// /*******************************************************************************
//  * app_DerpPulling()
//  ******************************************************************************/
// void app_DerpPulling(void)
// {
// 	printf("### DERP nRoots=%d\n, nProgs=%d", appDerClient.numEventRoots, appDerClient.numEventProgs);
	
// 	sl_der_client_get_derp_list_req req = {0};
	
//     // Get the next DERP List
//     req.numResources  = SL_CMN_CLIENT_GET_ENTIRE_LIST;
//     req.transactionId = 12345;

//     req.serverSourceInfo.serverHandle  = appDerClient.selectedEventRoots.list[0].serverHandle;
//     req.serverSourceInfo.source.source = appDerClient.selectedEventRoots.list[0].source.source;
//     req.serverSourceInfo.source.fsaId  = appDerClient.selectedEventRoots.list[0].source.fsaId;
//     app_SendMail(SL_DER_CLIENT_GET_DERP_LIST_REQ, &req, sizeof(sl_der_client_get_derp_list_req));
// }

/*******************************************************************************
 * app_DerClientHandleMail()
 ******************************************************************************/
void app_DerClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
  // debug
  //fprintf(stdout, "<><><><><><> DBG <><><><><><> DER CLIENT handle mail func %d \n", mailId);
  switch (mailId)
  {
  case SL_DER_CLIENT_DC_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_DC_IND");
    app_ClientHandleDCInd((sl_der_client_dc_ind*) pMail);
    break;
  case SL_DER_CLIENT_DERC_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_DERC_IND");
    app_ClientHandleSLClientEventInd(((sl_der_client_derc_ind *)pMail)->activeInfo.eventRespStatus, false);
    app_ClientHandleDERCInd((sl_der_client_derc_ind *) pMail);
    break;
  case SL_DER_CLIENT_DEFAULT_DERC_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_DEFAULT_DERC_IND");
    app_ClientHandleDefaultDERCInd((sl_der_client_default_derc_ind*)pMail);
    break;
  case SL_DER_CLIENT_DERP_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_DERP_IND");
    HandleDerpInd((sl_der_client_derp_ind *)pMail);
    break;
  case SL_DER_CLIENT_SERVER_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_SERVER_IND");
    HandleDerServerInd((sl_der_client_server_ind *)pMail);
    break;
  case SL_DER_CLIENT_GET_DERP_LIST_CNF:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_GET_DERP_LIST_CNF");
    app_LogStatus(((sl_der_client_get_derp_list_cnf *)pMail)->status);
    break;
  case SL_DER_CLIENT_CANCEL_GET_DERP_LIST_CNF:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_CANCEL_GET_DERP_LIST_CNF");
    break;
  case SL_DER_CLIENT_GET_DERC_LIST_CNF:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_GET_DERC_LIST_CNF");
    app_LogStatus(((sl_der_client_get_derc_list_cnf *)pMail)->status);
    break;
  case SL_DER_CLIENT_CANCEL_GET_DERC_LIST_CNF:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_CANCEL_GET_DERC_LIST_CNF");
    break;
  case SL_DER_CLIENT_GET_DC_LIST_CNF:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_GET_DC_LIST_CNF");
    app_LogStatus(((sl_der_client_get_dc_list_cnf *)pMail)->status);
    break;
  case SL_DER_CLIENT_CANCEL_GET_DC_LIST_CNF:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_CANCEL_GET_DC_LIST_CNF");
    break;
  case SL_DER_CLIENT_DERP_LIST_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_DERP_LIST_IND");
    app_LogStatus(((sl_der_client_derp_list_ind *)pMail)->results.status);
    app_ClientHandleDERPListInd((sl_der_client_derp_list_ind*)pMail);

    // SET POLL RATE LOGIC HERE
    SetPollRate();
    break;
  case SL_DER_CLIENT_DERC_LIST_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_DERC_LIST_IND");
    app_LogStatus(((sl_der_client_derc_list_ind *)pMail)->results.status);
    app_ClientHandleDERCListInd((sl_der_client_derc_list_ind *)pMail);    
    SetPollRate();
    break;
  case SL_DER_CLIENT_DC_LIST_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_DC_LIST_IND");
    app_LogStatus(((sl_der_client_dc_list_ind *)pMail)->results.status);
    break;
  case SL_DER_CLIENT_SET_EVENT_PROGRAMS_CNF:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_SET_EVENT_PROGRAMS_CNF");
    app_LogStatus(((sl_der_client_set_event_programs_cnf *)pMail)->status);
    break;
  case SL_DER_CLIENT_SET_EVENT_ROOTS_CNF:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_SET_EVENT_ROOTS_CNF");
    app_LogStatus(((sl_der_client_set_event_roots_cnf *)pMail)->status);
    break;
  case SL_DER_CLIENT_EVENT_PROGRAMS_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_EVENT_PROGRAMS_IND");
    HandleDerEventProgramsInd((sl_der_client_event_programs_ind *)pMail);
    break;
  case SL_DER_CLIENT_EVENT_ROOTS_IND:
    APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_EVENT_ROOTS_IND");
    HandleDerEventRootsInd((sl_der_client_event_roots_ind *)pMail);
    break;
  }
}

/*******************************************************************************
 * app_DerpPulling()
 ******************************************************************************/
void app_DerpPulling(void)
{
	printf("### DERP nRoots=%d, nProgs=%d \n", appDerClient.numEventRoots, appDerClient.numEventProgs);
	
	sl_der_client_get_derp_list_req req = {0};
	
    // Get the next DERP List
    req.numResources  = SL_CMN_CLIENT_GET_ENTIRE_LIST;
    req.transactionId = 12345;

    req.serverSourceInfo.serverHandle  = appDerClient.selectedEventRoots.list[0].serverHandle;
    req.serverSourceInfo.source.source = appDerClient.selectedEventRoots.list[0].source.source;
    req.serverSourceInfo.source.fsaId  = appDerClient.selectedEventRoots.list[0].source.fsaId;
    app_SendMail(SL_DER_CLIENT_GET_DERP_LIST_REQ, &req, sizeof(sl_der_client_get_derp_list_req));
}

void app_DercPulling(void)
{
	printf("### Pull DERC List\n");
	
  // app_client_program_node_t *pProgramNode;

  // pProgramNode = appDerClient.pProgramList;
  // if (pProgramNode != NULL)
  // {
    sl_der_client_get_derc_list_req req = {0};

    // Get the next DERP List
    req.numResources  = SL_CMN_CLIENT_GET_ENTIRE_LIST;
    req.transactionId = 12348;

    memcpy(req.parentMrId, appDerClient.selectedEventProgs.mrId, SL_XSD_MRID_LEN);
    // memcpy(req.parentMrId, pProgramNode->mrId, SL_XSD_MRID_LEN);

    app_SendMail(SL_DER_CLIENT_GET_DERC_LIST_REQ, &req, sizeof(sl_der_client_get_derc_list_req));
  // }
}
