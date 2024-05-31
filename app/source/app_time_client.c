/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_time_client.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         Application Time Client Code
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
#include <errno.h>

/*******************************************************************************
 * Local Data
 ******************************************************************************/

/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void HandleServerTimeInd(sl_time_client_server_time_ind *pInd);

/*******************************************************************************
 * HandleServerTimeInd()
 ******************************************************************************/
/**
 * \brief Package the time and send up.
 * \param pInd - Pointer to the Time Indication.
 * \return none.
 */
static void HandleServerTimeInd(sl_time_client_server_time_ind *pInd)
{
    time_t                          localSecs;
    i32_t                           timeOffset;
    struct tm                       tmRef;
    struct timespec                 setTSpec;
    app_time_client_server_time_ind ind;

    ind.daylightSavingsEndTime   = pInd->daylightSavingsEndTime;
    ind.daylightSavingsOffset    = pInd->daylightSavingsOffset;
    ind.daylightSavingsStartTime = pInd->daylightSavingsStartTime;
    ind.quality                  = pInd->quality;
    ind.timeZoneOffset           = pInd->timeZoneOffset;
    ind.utcTime                  = pInd->utcTime;

    timeOffset = ind.timeZoneOffset;
    if ((ind.utcTime >= ind.daylightSavingsStartTime) && (ind.utcTime <= ind.daylightSavingsEndTime)) timeOffset += ind.daylightSavingsOffset;

    /* EPRI: beg */
    setTSpec.tv_sec  = ind.utcTime + 1;
    setTSpec.tv_nsec = 0;
    if ( clock_settime( CLOCK_REALTIME, &setTSpec ) != 0 )
        fprintf(stdout, "!!! ERROR ---> !!! clock_settime errno %d EFAULT = %d EINVAL = %d EPERM = %d", errno, EFAULT, EINVAL, EPERM );
    /* EPRI: end*/
    localSecs = (time_t)(ind.utcTime + timeOffset);
    KAL_Memcpy(&tmRef, gmtime(&localSecs), sizeof(tmRef));



    // Just log the UTC time from Time Server
    char utcStr[64];
    snprintf(utcStr, sizeof(utcStr), "Server-Time-Indication (UTC): %u", (u32_t)ind.utcTime);
    APP_LOG_INFO(utcStr);
    snprintf(utcStr, sizeof(utcStr), "LocalTime: %s", asctime(&tmRef));
    APP_LOG_INFO(utcStr);
}

/*******************************************************************************
 * app_TimeClientHandleMail()
 ******************************************************************************/
void app_TimeClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
    switch (mailId)
    {
    case SL_TIME_CLIENT_SERVER_IND:
        APP_LOG_INFO("app_TimeClientHandleMail: SL_TIME_CLIENT_SERVER_IND");
        break;
    case SL_TIME_CLIENT_SERVER_TIME_IND:
        APP_LOG_INFO("app_TimeClientHandleMail: SL_TIME_CLIENT_SERVER_TIME_IND");
        HandleServerTimeInd((sl_time_client_server_time_ind *)pMail);
        break;
    }
}
