/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_eder_client.c
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

#include "app_client.h"
#include "app_config.h"
#include "app_utils.h"
#include "kal_api.h"
#include "sl_api.h"
#include <string.h>
#include <usock_proto.h>

extern u32_t commandLineDERType;
/* EPRI: beg*/
// store the client_server_ind as it is required when messages are recevied from CAN2SEP2 app
sl_eder_client_server_ind localInd = {0};
short localIndExists = 0; // 1 => localInd contains valid indication
sl_eder_client_send_ders_req dersReq = {0};
int ders_req_len = 0;
short ders_req_received_vehicle = 0; // 1 => at least one message was received
static int ders_succ_count = 0;
static int ders_failure_count = 0;
static u32_t ders_req_transaction_id = 0;
u32_t dera_req_transaction_id = 0;
u32_t dercap_req_transaction_id = 0;
u32_t dersett_req_transaction_id = 0;
/* EPRI: end*/

#ifdef APP_PRELOAD_DEMO
/*******************************************************************************
 * SendDera()
 ******************************************************************************/
static void SendDera(sl_eder_client_server_ind *pInd)
{
    sl_eder_client_send_dera_req req = {0};

    req.serverSourceInfo.serverHandle  = pInd->serverDetails.serverHandle;
    req.serverSourceInfo.source.source = pInd->serverDetails.sourceList[0].source;
    req.serverSourceInfo.source.fsaId  = pInd->serverDetails.sourceList[0].fsaId;
    req.dera.availabilityDuration      = 30;

    app_SendMail(SL_EDER_CLIENT_SEND_DERA_REQ, &req, sizeof(sl_eder_client_send_dera_req));
}
#endif

/* EPRI: beg*/
void EVCCSendDera(sl_eder_client_send_dera_req * req) 
{
  if (localIndExists) 
  {
    req->serverSourceInfo.serverHandle = localInd.serverDetails.serverHandle;
    req->serverSourceInfo.source.source = localInd.serverDetails.sourceList[0].source;
    req->serverSourceInfo.source.fsaId = localInd.serverDetails.sourceList[0].fsaId;
    req->transactionId = app_ClientGenerateTransactionID();
    dera_req_transaction_id = req->transactionId;

    app_SendMail(SL_EDER_CLIENT_SEND_DERA_REQ, req, sizeof(sl_eder_client_send_dera_req));
  }
}

static void HandleDeraCnf(sl_eder_client_send_dera_cnf * pInd) {
  if (pInd->transactionId == dera_req_transaction_id) {
    dera_req_transaction_id = 0;
  }
}

/* EPRI: end*/


#ifdef APP_PRELOAD_DEMO
/*******************************************************************************
 * SendDercap()
 ******************************************************************************/
static void SendDercap(sl_eder_client_server_ind *pInd)
{
    sl_eder_client_send_dercap_req req = {0};

    req.serverSourceInfo.serverHandle  = pInd->serverDetails.serverHandle;
    req.serverSourceInfo.source.source = pInd->serverDetails.sourceList[0].source;
    req.serverSourceInfo.source.fsaId  = pInd->serverDetails.sourceList[0].fsaId;
    /* These variables do not exist anymore
    req.dercap.rtgMaxChargeRateExists = true;
    req.dercap.rtgMaxChargeRateMultiplier = SL_XSD_PTM_MILI;
    req.dercap.rtgMaxChargeRateValue = 100;
    */

    // SPRK-4172: Add CLI option for DERType
    if (commandLineDERType != 0xFFFFFFFF)
    {
        req.dercap.type = commandLineDERType;
    }

    app_SendMail(SL_EDER_CLIENT_SEND_DERCAP_REQ, &req, sizeof(sl_eder_client_send_dercap_req));
}
#endif

/* EPRI: beg*/
void EVCCSendDercap(sl_eder_client_send_dercap_req * req) 
{
  if (localIndExists) 
  {
    req->serverSourceInfo.serverHandle = localInd.serverDetails.serverHandle;
    req->serverSourceInfo.source.source = localInd.serverDetails.sourceList[0].source;
    req->serverSourceInfo.source.fsaId = localInd.serverDetails.sourceList[0].fsaId;
    req->transactionId = app_ClientGenerateTransactionID();
    dercap_req_transaction_id = req->transactionId;

    app_SendMail(SL_EDER_CLIENT_SEND_DERCAP_REQ, req, sizeof(sl_eder_client_send_dercap_req));
  }
}

static void HandleDercapCnf(sl_eder_client_send_dercap_cnf * pInd) {
  if (pInd->transactionId == dercap_req_transaction_id) {
    dercap_req_transaction_id = 0;
  }
}

/* EPRI: end*/

#ifdef APP_PRELOAD_DEMO
/*******************************************************************************
 * SendDers()
 ******************************************************************************/
static void SendDers(sl_eder_client_server_ind *pInd)
{
    sl_eder_client_send_ders_req req = {0};

    req.serverSourceInfo.serverHandle  = pInd->serverDetails.serverHandle;
    req.serverSourceInfo.source.source = pInd->serverDetails.sourceList[0].source;
    req.serverSourceInfo.source.fsaId  = pInd->serverDetails.sourceList[0].fsaId;
    req.ders.genConnectStatusDateTime  = KAL_GetClock();
    req.ders.genConnectStatusExists    = true;
    req.ders.genConnectStatusValue     = SL_XSD_CONN_STATUS_AVAILABLE;

    app_SendMail(SL_EDER_CLIENT_SEND_DERS_REQ, &req, sizeof(sl_eder_client_send_ders_req));
}
#endif

/* EPRI: beg*/
void EVCCSendDersPeriodic() 
{
  // debug
  static int display_count = 0;
  if (display_count % 60 == 0)
  {
    fprintf(stdout, "<<<<=======>>>>  eder local ind %d, ders_recv ev %d\n", localIndExists, ders_req_received_vehicle);
  }
  display_count++;
  // server exits and new req recvd from vehicle and prev transaction sts received
  //if ( (localIndExists) && (ders_req_received_vehicle) && (ders_req_transaction_id == 0)) 
  // the condition of ders_req_received_vehicle is slowing the system to about 30 secs to a minute
  // so removing that constraint
  if ( (localIndExists) && (ders_req_transaction_id == 0)) 
  {
    dersReq.serverSourceInfo.serverHandle = localInd.serverDetails.serverHandle;
    dersReq.serverSourceInfo.source.source = localInd.serverDetails.sourceList[0].source;
    dersReq.serverSourceInfo.source.fsaId = localInd.serverDetails.sourceList[0].fsaId;
    dersReq.transactionId = app_ClientGenerateTransactionID();
    ders_req_transaction_id = dersReq.transactionId;
    app_SendMail(SL_EDER_CLIENT_SEND_DERS_REQ, &dersReq, sizeof(sl_eder_client_send_ders_req));
    ders_req_received_vehicle = 0;
  }
}

void EVCCDersFromVehicleCAN(sl_eder_client_send_ders_req * pReq)
{
  KAL_Memcpy(&dersReq, pReq, ders_req_len);
  ders_req_received_vehicle = 1;
}
/* EPRI: end*/


#ifdef APP_PRELOAD_DEMO
/*******************************************************************************
 * SendDersett()
 ******************************************************************************/
static void SendDersett(sl_eder_client_server_ind *pInd)
{
    sl_eder_client_send_dersett_req req = {0};

    req.serverSourceInfo.serverHandle  = pInd->serverDetails.serverHandle;
    req.serverSourceInfo.source.source = pInd->serverDetails.sourceList[0].source;
    req.serverSourceInfo.source.fsaId  = pInd->serverDetails.sourceList[0].fsaId;

    app_SendMail(SL_EDER_CLIENT_SEND_DERSETT_REQ, &req, sizeof(sl_eder_client_send_dersett_req));
}
#endif

/* EPRI: beg*/
void EVCCSendDersett(sl_eder_client_send_dersett_req *req) 
{
  if (localIndExists) 
  {
    req->serverSourceInfo.serverHandle = localInd.serverDetails.serverHandle;
    req->serverSourceInfo.source.source = localInd.serverDetails.sourceList[0].source;
    req->serverSourceInfo.source.fsaId = localInd.serverDetails.sourceList[0].fsaId;
    req->transactionId = app_ClientGenerateTransactionID();
    dersett_req_transaction_id = req->transactionId;

    app_SendMail(SL_EDER_CLIENT_SEND_DERSETT_REQ, req, sizeof(sl_eder_client_send_dersett_req));
  }
}

static void HandleDersettCnf(sl_eder_client_send_dersett_cnf * pInd) {
  if (pInd->transactionId == dersett_req_transaction_id) {
    dersett_req_transaction_id = 0;
  }
}

/* EPRI: end*/


/*******************************************************************************
 * AppEderClientHandleServerInd()
 ******************************************************************************/
static void AppEderClientHandleServerInd(sl_eder_client_server_ind *pInd)
{
  /* EPRI: beg*/
  int tmp_sz;
  if (pInd->serverDetails.status == SL_CMN_SERVER_STATUS_FOUND)
  {
    tmp_sz = sizeof(sl_eder_client_server_ind);
    memset(&localInd, 0, tmp_sz);
    memcpy(&localInd, pInd, tmp_sz);
    localIndExists = 1;
    ders_req_len = sizeof(sl_eder_client_send_ders_req);
  }
  /* EPRI: end*/
// #ifdef APP_PRELOAD_DEMO
//   if (pInd->serverDetails.status == SL_CMN_SERVER_STATUS_FOUND)
//   {
//     if (pInd->services.dera)
//       SendDera(pInd);

//     if (pInd->services.dercap)
//       SendDercap(pInd);

//     if (pInd->services.ders)
//       SendDers(pInd);

//     if (pInd->services.dersett)
//       SendDersett(pInd);
//   }
// #endif
}

void HandleMonitorDersStatus(sl_eder_client_send_ders_cnf* pInd)
{
  i8_t status = pInd->status;
  if (status == SL_CMN_RC_SUCCESS)
  {
    ders_succ_count ++;
    ders_failure_count = 0;
  }
  else 
  {
    ders_failure_count++;
    ders_succ_count = 0;
  }
  if ( (ders_succ_count == 3) && (ders_failure_count == 0) )
  {
    // send (LOCEvse) comm is true (1)
    send_communication_status((u8_t)1);
  }
  else if ( (ders_failure_count == 10) && (ders_succ_count == 0) )
  {
    // send (LOCEvse) comm is false (0)
    send_communication_status((u8_t)0);
  }
  if (pInd->transactionId == ders_req_transaction_id) 
  {
    ders_req_transaction_id = 0;
  }
  else 
  {
    fprintf(stdout, " <<<xxx>>>> received invalid resp for derstatus exp %d, recvd %d", pInd->transactionId, ders_req_transaction_id);
  }
}

/*******************************************************************************
 * app_EderClientHandleMail()
 ******************************************************************************/
void app_EderClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
    switch (mailId)
    {
    case SL_EDER_CLIENT_SERVER_IND:
        APP_LOG_INFO("app_EderClientHandleMail: SL_EDER_CLIENT_SERVER_IND");
        AppEderClientHandleServerInd((sl_eder_client_server_ind *)pMail);
        break;
    case SL_EDER_CLIENT_SEND_DERSETT_CNF:
        APP_LOG_INFO("app_EderClientHandleMail: SL_EDER_CLIENT_SEND_DERSETT_CNF");
        app_LogStatus(((sl_eder_client_send_dersett_cnf *)pMail)->status);
        HandleDersettCnf((sl_eder_client_send_dersett_cnf *)pMail);
        break;
    case SL_EDER_CLIENT_SEND_DERA_CNF:
        APP_LOG_INFO("app_EderClientHandleMail: SL_EDER_CLIENT_SEND_DERA_CNF");
        app_LogStatus(((sl_eder_client_send_dera_cnf *)pMail)->status);
        HandleDeraCnf((sl_eder_client_send_dera_cnf *)pMail);
        break;
    case SL_EDER_CLIENT_SEND_DERCAP_CNF:
        APP_LOG_INFO("app_EderClientHandleMail: SL_EDER_CLIENT_SEND_DERCAP_CNF");
        app_LogStatus(((sl_eder_client_send_dercap_cnf *)pMail)->status);
        HandleDercapCnf((sl_eder_client_send_dercap_cnf *)pMail);
        break;
    case SL_EDER_CLIENT_SEND_DERS_CNF:
        APP_LOG_INFO("app_EderClientHandleMail: SL_EDER_CLIENT_SEND_DERS_CNF");
        app_LogStatus(((sl_eder_client_send_ders_cnf *)pMail)->status);
        HandleMonitorDersStatus((sl_eder_client_send_ders_cnf *)pMail);
        break;
    }
}
