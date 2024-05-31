/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_meter_mirror_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application Meter Mirror Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef APP_METER_MIRROR_CLIENT_H
#define APP_METER_MIRROR_CLIENT_H

#include "kal_api.h"
#include "sl_api.h"
/*******************************************************************************
 * Timer Events
 ******************************************************************************/

/*******************************************************************************
 * APP Server Functions
 ******************************************************************************/
void app_MeterMirrorClientHandleMail(kal_mail_id_t mailId, void *pMail);
void EVCCSetupMupReq(sl_meter_mirror_client_send_mup_req *pReq);
void EVCCMmrReq(sl_meter_mirror_client_send_mmr_req *pReq);

extern u32_t meter_req_transaction_id;

#endif // APP_METER_MIRROR_CLIENT_H
