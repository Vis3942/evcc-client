/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_epwrstat_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application End Device Power Status Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef APP_EPWRSTAT_CLIENT_H
#define APP_EPWRSTAT_CLIENT_H

#include "kal_api.h"
#include "sl_api.h"
/*******************************************************************************
 * Timer Events
 ******************************************************************************/

/*******************************************************************************
 * APP Server Functions
 ******************************************************************************/
void app_EpwrstatClientHandleMail(kal_mail_id_t mailId, void *pMail);
void app_EpwrstatClientProcessing( void );
void EVCCSendpwrstat(sl_epwrstat_client_send_ps_req *req);

extern u32_t ps_epwrstat_transaction_id;

#endif // APP_EPWRSTAT_CLIENT_H
