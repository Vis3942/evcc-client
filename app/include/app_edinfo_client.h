/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_edev_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application Edinfo Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef APP_EDINFO_CLIENT_H
#define APP_EDINFO_CLIENT_H

#include "kal_api.h"
#include "sl_api.h"
/*******************************************************************************
 * Timer Events
 ******************************************************************************/

/*******************************************************************************
 * APP Server Functions
 ******************************************************************************/
void app_EdinfoClientHandleMail(kal_mail_id_t mailId, void *pMail);
void EVCCSendDevInfo(sl_edinfo_client_send_dinfo_req * req);

#endif // APP_EDINFO_CLIENT_H
