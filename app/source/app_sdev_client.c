/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_sdev_client.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         Application Self Device Client Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#include "app_api.h"
#include "app_utils.h"
#include "kal_api.h"
#include "sl_api.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

/*******************************************************************************
 * Local Data
 ******************************************************************************/

/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void HandleServerSdevInd(sl_sdev_client_server_sdev_ind *pInd);

/*******************************************************************************
 * HandleServerSdevInd()
 ******************************************************************************/
/**
 * \brief Get the Sdev server info, print it out
 * \param pInd - Pointer to the Sdev Indication.
 * \return none.
 */
static void HandleServerSdevInd(sl_sdev_client_server_sdev_ind *pInd)
{
    // Just log the data we got
    char str[256];
    snprintf(str,
             sizeof(str),
             "SDEV Server found: sfdi=%" PRId64 "u, deviceCategory=%x",
             pInd->sfdi,
             pInd->deviceCategoryExists ? pInd->deviceCategory : 0);
    APP_LOG_INFO(str);
}

/*******************************************************************************
 * app_SdevClientHandleMail()
 ******************************************************************************/
void app_SdevClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
    switch (mailId)
    {
    case SL_SDEV_CLIENT_SERVER_IND:
        APP_LOG_INFO("app_SdevClientHandleMail: SL_SDEV_CLIENT_SERVER_IND");
        // TODO: ???
        //SetPollRate();
        break;
    case SL_SDEV_CLIENT_SERVER_SDEV_IND:
        APP_LOG_INFO("app_SdevClientHandleMail: SL_SDEV_CLIENT_SERVER_SDEV_IND");
        HandleServerSdevInd((sl_sdev_client_server_sdev_ind *)pMail);
        // TODO: ???
        //SetPollRate();
        break;
    }
}
