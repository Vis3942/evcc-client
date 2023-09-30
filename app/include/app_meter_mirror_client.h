/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
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

#ifndef   APP_METER_MIRROR_CLIENT_H
#define   APP_METER_MIRROR_CLIENT_H

// IANA PEN for Stellantis
#define STELLANTIS_IANA_PEN 57872
/*******************************************************************************
 * Timer Events
 ******************************************************************************/


/*******************************************************************************
 * APP Server Functions
 ******************************************************************************/
void app_MeterMirrorClientHandleMail(kal_mail_id_t mailId, void *pMail);
void EVCCSetupMupReq(sl_meter_mirror_client_send_mup_req *pReq);
void EVCCMmrReq(sl_meter_mirror_client_send_mmr_req *pReq);

#endif  // APP_METER_MIRROR_CLIENT_H
