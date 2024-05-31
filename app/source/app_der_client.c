/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
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

#include "app_der_client.h"
#include "app_client.h"
#include "app_config.h"
#include "app_logs.h"
#include "app_utils.h"
#include "kal_api.h"
#include "sl_api.h"
#include <string.h>

#include "usock_proto.h"
#include "utils.h"

// This define is only to be used when sending commands from SECC
// till we debug the issue of CLIENT_DERC_IND is debugged
//   this is using DERC_LIST_INDICATION
#define DERC_EVENT_WORKAROUND 1
int sent_active_derc = 0;
sl_der_client_derc_ind copy_sent_ind = {0};
u32_t sent_derp_req_trans_id = 0;
bool recv_derp_list_ind_for_req = true;
#define MAX_STORED_CURVE_COUNT 20
char curve_mrids_recvd[MAX_STORED_CURVE_COUNT][SL_XSD_MRID_LEN];
int curve_mrids_count = 0;
bool sent_freq_droop_data = false;

/*******************************************************************************
 * Local Data
 ******************************************************************************/

struct
{
    app_client_server_node_t    *pServerList;
    app_client_program_node_t   *pProgramList;
    sl_cmn_client_program_list_t selectedEventProgs;
    sl_cmn_client_root_list_t    selectedEventRoots;
    u8_t                         numEventProgs;
    u8_t                         numEventRoots;
} appDerClient = {
    NULL,    // *pServerList
    NULL,    // *pProgramList
    {{{0}}}, // selectedEventProgs
    {{
        // selectedEventRoots
        {0, {0, 0}},
    }},
    0, // numEventProgs
    0, // numEventRoots
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
 * int DERCContainsCurve( derc )
 ******************************************************************************/
/**
 * \brief checks if the derc contains a curve
 * \param derc - derc being checked
 * \return 1 if it contains a curve, else 0.
 */
static int DERCContainsCurve(sl_der_derc_t *pDerc)
{
    if (pDerc->dercBase.opModFreqDroopExists) {
      return 1;
    }

    if (pDerc->dercBase.opModLVRTMomentaryCessationCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModLVRTMustTripCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModVoltWattCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModHVRTMomentaryCessationCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModHVRTMustTripCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModVoltVarCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModLFRTMustTripCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModHFRTMustTripCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModLVRTMayTripCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModHFRTMayTripCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModLFRTMayTripCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModHVRTMayTripCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModWattPFCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModWattVarCurveExists) {
      return 1;
    }

    if (pDerc->dercBase.opModFreqWattCurveExists) {
      return 1;
    }

    return 0;
}



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
    appDerClient.numEventProgs      = pInd->numListEntries;
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
    appDerClient.numEventRoots      = pInd->numRoots;
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
    app_ClientStoreProgramNode(
        pInd->status, pInd->derpExists, &pInd->derp, sizeof(sl_der_derp_t), pInd->derp.mrId, pInd->serverSourceInfo, &appDerClient.pProgramList);

    SetDerEventPrograms();
}

/* EPRI code: Beg*/

/**
 * @brief sends the event received response and then forwards event to the CAN2sep2 APP
 * 
 * @param derc_ind 
 */
static void process_derc_ind(sl_der_client_derc_ind * derc_ind) {
    u32_t timeNow = KAL_GetClock();

    // // send received response if href exists and bit is set to 1
    // if (derc_ind->respInfo.replyToHrefExists) {
    //     sl_resp_client_send_resp_req req = {0};
    //     req.type = SL_RESP_TYPE_DER;
    //     req.creationTimeExists = true;
    //     req.creationTime = KAL_GetClock();
    //     memcpy(req.endDeviceLFDI, app_UtilsGetMyLfdi(), SL_XSD_LFDI_LEN);
    //     memcpy(req.replyToHref, derc_ind->respInfo.replyToHref, SL_CMN_REPLY_TO_HREF_LEN);
    //     req.statusExists;
    //     req.status = SL_XSD_RESP_STATUS_MSG_RXED;
    //     memcpy(req.subj, derc_ind->derpMrId, SL_XSD_RESP_SUBJ_LEN);
    // }


    // send derc control to CAN2SEP2 app
    send_derc_ind(derc_ind);
}

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
  recv_derp_list_ind_for_req = true;
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
  // check that the curve has not been sent.
  int ii;
  bool found = 0;
  for (ii = 0; ii < curve_mrids_count; ii++)
  {
    if (KAL_Memcmp(&(curve_mrids_recvd[ii][0]), pInd->curve.mrId, SL_XSD_MRID_LEN) == 0) 
    {
      found = 1;
      break;
    }
  }
  if (found == 0) 
  {
    // send to CAN2SEP2 app
    send_curve_data(pInd->curve);
    KAL_Memcpy(&(curve_mrids_recvd[curve_mrids_count][0]), pInd->curve.mrId, SL_XSD_MRID_LEN);
    if (curve_mrids_count < (MAX_STORED_CURVE_COUNT-1)) 
    {
      curve_mrids_count ++;
    }
  }

}

void  app_ClientHandleDERCInd(sl_der_client_derc_ind *pInd)
{
  if (pInd->derc.dercBase.opModFreqDroopExists) {
    if (!sent_freq_droop_data) {
      send_freq_droop_data(pInd->derc.dercBase.opModFreqDroop);
      sent_freq_droop_data = true;
    }
    return;
  }
  // TODO implement this function
  send_derc_ind(pInd);
  //fprintf(stdout, "NOT implemented : send derc indication yet. TODO \n");
}

void app_ClientHandleDERCListInd(sl_der_client_derc_list_ind *pInd)
{
  sl_der_client_derc_ind sendInd = {0};
  int ii, contains_curve;
  if (pInd->results.numList > 0)
  {
    for (ii=0; ii < pInd->results.numList; ii++) {
        contains_curve = DERCContainsCurve(&pInd->list[ii].derc);
        if (contains_curve == 0)
        {
            KAL_Memcpy(&sendInd.derc, &pInd->list[ii].derc, sizeof(sl_der_derc_t));
            // debug
            fprintf(stdout, "=x=x=x=x=x=> DERC ind sts = %d, activeinfo sts %d\n",
            pInd->list[ii].eventStatusCurrStatus, pInd->list[ii].activeInfo.eventRespStatus);

            sendInd.activeInfo.eventRespStatus = pInd->list[ii].activeInfo.eventRespStatus;
            sendInd.eventStatusCurrStatus = pInd->list[ii].eventStatusCurrStatus;
            sendInd.eventStatusDateTime = pInd->list[ii].eventStatusDateTime;
            
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
          // if it is freq droop send it the first time but not other times
          if (!sent_freq_droop_data)
          {
            send_freq_droop_data(pInd->list[ii].derc.dercBase.opModFreqDroop);
            sent_freq_droop_data = true;
          }
        }
    }

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

static void HandleGetDERPListCnf(sl_der_client_get_derp_list_cnf *pInd)
{
    if (pInd->transactionId == sent_derp_req_trans_id) 
    {
        sent_derp_req_trans_id = 0;
        //fprintf(stdout, "<<<==========>>>  received derp req cnf for %d \n ", pInd->transactionId);
    }
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
    sl_der_client_set_event_roots_req req         = {0};
    app_client_server_node_t         *pServerNode = appDerClient.pServerList;
    u8_t                              x;
    bool                              registered = app_ClientRegistered(appDerClient.pServerList);
    int ii;
    fprintf(stdout, "[SetDEREventRoots-funcbeg] =====>>>>> registered = %d\n", registered);

    // Select roots that are from FSA
    // loop through the available servers
    // pServerNode = appDerClient.pServerList;
    while (pServerNode != NULL)
    {
        fprintf(stdout, "[SetDEREventRoots-funcbeg] =====>>>>> numsrcs = %d\n", pServerNode->pServer->numSources);
        // loop through the available roots/sources
        for (x = 0; x < pServerNode->pServer->numSources; x++)
        {
            // debug
            fprintf(stdout, "[SetDEREventRoots-funcbeg] =====>>>>> time synced %d, server registered = %d\n", 
            pServerNode->pServer->sourceList[x].timeSynced, pServerNode->pServer->registered);
            // if it is time synced
            if (pServerNode->pServer->sourceList[x].timeSynced == true)
            {
                // if it meets registration requirements
                if ((!registered) || ((registered) && (pServerNode->pServer->registered)))
                {
                    fprintf(stdout, "[SetDEREventRoots-functop] =====>>>>> source = %d\n", pServerNode->pServer->sourceList[x].source);
                    // if it is from Function Set Assignments
                    if (pServerNode->pServer->sourceList[x].source & SL_CMN_CLIENT_ROOT_SOURCE_FSA)
                    {
                        req.rootList.list[req.numRoots].serverHandle  = pServerNode->pServer->serverHandle;
                        req.rootList.list[req.numRoots].source.source = pServerNode->pServer->sourceList[x].source;
                        req.rootList.list[req.numRoots].source.fsaId  = pServerNode->pServer->sourceList[x].fsaId;
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
                if ((!registered) || ((registered) && (pServerNode->pServer->registered)))
                {
                    fprintf(stdout, "[SetDEREventRoots-funcmid] =====>>>>> source = %d\n", pServerNode->pServer->sourceList[x].source);
                    // if it is not from Function Set Assignments
                    if (!(pServerNode->pServer->sourceList[x].source & SL_CMN_CLIENT_ROOT_SOURCE_FSA))
                    {
                        req.rootList.list[req.numRoots].serverHandle  = pServerNode->pServer->serverHandle;
                        req.rootList.list[req.numRoots].source.source = pServerNode->pServer->sourceList[x].source;
                        req.rootList.list[req.numRoots].source.fsaId  = pServerNode->pServer->sourceList[x].fsaId;
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
    app_client_program_node_t           *pProgramNode;
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

            if (appDerClient.numEventProgs == SL_CMN_CLIENT_MAX_NUM_EVENT_PROGRAMS) break;
        }
        pProgramNode = pProgramNode->next;
    }

    if (!((appDerClient.numEventProgs == 0) && (req.numListEntries == 0))) // Don't send if no active programs and no programs to set
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
            app_MridtoString256(&(req.list.mrId[ii][0]), mrid_str);
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
        HandleGetDERPListCnf((sl_der_client_get_derp_list_cnf *)pMail);
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
        //SetPollRate();
        //app_ClientChangePollRate(derp_pull_secs); // this does not work.. so user dercPulling func       
        break;
    case SL_DER_CLIENT_DERC_LIST_IND:
        APP_LOG_INFO("app_DerClientHandleMail: SL_DER_CLIENT_DERC_LIST_IND");
        app_LogStatus(((sl_der_client_derc_list_ind *)pMail)->results.status);
        app_ClientHandleDERCListInd((sl_der_client_derc_list_ind *)pMail);    
        //SetPollRate();
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
    if (sent_derp_req_trans_id > 0) {
    	APP_DLOG_INFO("<<< DERP Pulling previous request is incomplete. Cannot send req >>>\n", appDerClient.numEventRoots, appDerClient.numEventProgs);
        return;
    }

    if (recv_derp_list_ind_for_req == false) {
    	APP_DLOG_INFO("<<< DERP Pulling did not receive derp list indication. Cannot send req >>>\n", appDerClient.numEventRoots, appDerClient.numEventProgs);
        return;
    }

	APP_DLOG_INFO("<<< DERP Pulling nRoots=%d, nProgs=%d >>>\n", appDerClient.numEventRoots, appDerClient.numEventProgs);
	
	sl_der_client_get_derp_list_req req = {0};
	
    // Get the next DERP List
    req.numResources  = SL_CMN_CLIENT_GET_ENTIRE_LIST;
    req.transactionId = app_ClientGenerateTransactionID();
    sent_derp_req_trans_id = req.transactionId;
    recv_derp_list_ind_for_req = false;
    //fprintf(stdout, "<<<==========>>>  sent derp req id for %d \n ", req.transactionId);

    req.serverSourceInfo.serverHandle  = appDerClient.selectedEventRoots.list[0].serverHandle;
    req.serverSourceInfo.source.source = appDerClient.selectedEventRoots.list[0].source.source;
    req.serverSourceInfo.source.fsaId  = appDerClient.selectedEventRoots.list[0].source.fsaId;
    app_SendMail(SL_DER_CLIENT_GET_DERP_LIST_REQ, &req, sizeof(sl_der_client_get_derp_list_req));
}

void app_DercPulling(void)
{
	APP_LOG_INFO("<<< Pull DERC List >>>\n");
	
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
