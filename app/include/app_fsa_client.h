/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_fsa_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application Function Set Assignment Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef APP_FSA_CLIENT_H
#define APP_FSA_CLIENT_H

#include "kal_api.h"

/*******************************************************************************
 * Timer Events
 ******************************************************************************/

/*******************************************************************************
 * APP Server Functions
 ******************************************************************************/
void app_FsaClientHandleMail(kal_mail_id_t mailId, void *pMail);

#endif // APP_FSA_CLIENT_H
