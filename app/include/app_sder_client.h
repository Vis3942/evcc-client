/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_sder_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Self DER Client Application Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef   APP_SDER_CLIENT_H
#define   APP_SDER_CLIENT_H

#include "sl_api.h"

/*******************************************************************************
 * APP Client Functions
 ******************************************************************************/
void app_SderClientHandleMail(kal_mail_id_t mailId, void *pMail);

#endif  // APP_SDER_CLIENT_H
