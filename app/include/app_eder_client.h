/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_eder_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Edev DER Client Application Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef   APP_EDER_CLIENT_H
#define   APP_EDER_CLIENT_H

#include "sl_api.h"
#include "kal_api.h"

/*******************************************************************************
 * APP Client Functions
 ******************************************************************************/
void app_EderClientHandleMail(kal_mail_id_t mailId, void *pMail);
void EVCCSendDercap(sl_eder_client_send_dercap_req *req);
void EVCCSendDersett(sl_eder_client_send_dersett_req *req);
void EVCCSendDera(sl_eder_client_send_dera_req * req);
void EVCCSendDersPeriodic();
void EVCCDersFromVehicleCAN(sl_eder_client_send_ders_req * pReq);

#endif  // APP_EDER_CLIENT_H
