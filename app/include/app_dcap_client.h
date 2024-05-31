/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_dcap_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Venkat Sethuraman
 *
 * \brief         DCAP Client Application Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef APP_DCAP_CLIENT_H
#define APP_DCAP_CLIENT_H

#include "kal_api.h"
#include "sl_api.h"

/*******************************************************************************
 * APP Client Functions
 ******************************************************************************/
void app_DcapClientHandleMail(kal_mail_id_t mailId, void *pMail);

#endif // APP_DCAP_CLIENT_H
