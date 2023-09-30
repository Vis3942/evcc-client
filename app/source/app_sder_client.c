/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_sder_client.c
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
#include "app_client.h"
#include "app_config.h"
#include "app_utils.h"

#include "usock_proto.h"
/*******************************************************************************
 * GetDerSettings()
 ******************************************************************************/
static void GetDerSettings(sl_sder_client_server_ind *pInd)
{
  APP_LOG_INFO("GetDerSettings");
  sl_sder_client_get_dersett_req req = {0};

  req.serverSourceInfo.serverHandle = pInd->serverDetails.serverHandle;
  req.serverSourceInfo.source.source = pInd->serverDetails.sourceList[0].source;
  req.serverSourceInfo.source.fsaId = pInd->serverDetails.sourceList[0].fsaId;

  app_SendMail(SL_SDER_CLIENT_GET_DERSETT_REQ, &req, sizeof(sl_sder_client_get_dersett_req));
}

/*******************************************************************************
 * AppSderClientHandleGetDerSett()
 ******************************************************************************/
static void AppSderClientHandleGetDerSett(sl_sder_client_get_dersett_cnf *pInd)
{
  APP_DLOG_INFO("SDER DERSettings: updatedTime=%d", pInd->dersett.updatedTime);
  send_der_setting(pInd->dersett);
}

/*******************************************************************************
 * AppSderClientHandleServerInd()
 ******************************************************************************/
static void AppSderClientHandleServerInd(sl_sder_client_server_ind *pInd)
{
  if (pInd->serverDetails.status == SL_CMN_SERVER_STATUS_FOUND)
  {
    APP_DLOG_INFO("SDER found with %s %s %s %s",
        pInd->services.dera ? "DERAvailability" : "",
        pInd->services.dercap ? "DERCapability" : "",
        pInd->services.ders ? "DERSettings" : "",
        pInd->services.dersett ? "DERStatus" : "");

    if (pInd->services.dersett)
    {
      GetDerSettings(pInd);
    }
  }
}

/*******************************************************************************
 * app_SderClientHandleMail()
 ******************************************************************************/
void app_SderClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
  switch (mailId)
  {
  case SL_SDER_CLIENT_SERVER_IND:
    APP_LOG_INFO("app_SderClientHandleMail: SL_SDER_CLIENT_SERVER_IND");
    AppSderClientHandleServerInd((sl_sder_client_server_ind *) pMail);
    // TODO: ????
    SetPollRate();
    break;
  case SL_SDER_CLIENT_GET_DERSETT_CNF:
    APP_LOG_INFO("app_SderClientHandleMail: SL_SDER_CLIENT_GET_DERSETT_CNF");
    AppSderClientHandleGetDerSett((sl_sder_client_get_dersett_cnf *) pMail);
    break;
  }
}
