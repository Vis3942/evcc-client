/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_epwrstat_client.c
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

#include "app_epwrstat_client.h"
#include "app_api.h"
#include "app_config.h"
#include "app_utils.h"
#include "kal_api.h"
#include "sl_api.h"
#include <stdio.h>
#include <string.h>

#if APP_CONFIG_EPS_CLIENT_ENABLED

/*******************************************************************************
 * Private Constants
 ******************************************************************************/
#define PWRSTAT_CLIENT_SCHEDULE_DELTA 15 // update every 15 seconds
#define PWRSTAT_CLIENT_SOURCE_TIME    60 // switch power source every 60 seconds
#define PWRSTAT_CLIENT_FILE           "/cygdrive/c/Temp/PowerStatusTemp.txt"
//#define PWRSTAT_FILE    "C:\\Temp\\PowerStatusTemp.txt"

/*******************************************************************************
 * Private Data
 ******************************************************************************/

struct
{
    bool                          serverFound;
    sl_epwrstat_client_server_ind serverInfo;
    u32_t                         nextPwrstatEventTime;
    i16_t                         pwrstatUpTime;
} appEpsClient = {0};
/* EPRI: beg*/
u32_t ps_epwrstat_transaction_id = 0;
/* EPRI: end*/

/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void AppEpwrstatClientHandleServerInd(sl_epwrstat_client_server_ind *pInd);

/*******************************************************************************
 * AppEpwrstatClientHandleServerInd()
 ******************************************************************************/
/**
 * \brief If the power status function set is available in the edev, the edev
 *        server is chosen and processing is started
 * \param pInd - pointer to the server indication message
 * \return none
 */
static void AppEpwrstatClientHandleServerInd(sl_epwrstat_client_server_ind *pInd)
{
    if ((appEpsClient.serverFound == false) && (pInd->status != SL_CMN_SERVER_STATUS_LOST))
    {
        // This starts the processing of the epwrstat client.
        appEpsClient.serverFound = true; // Just use the first server available
        appEpsClient.serverInfo  = *pInd;
        //app_EpwrstatClientProcessing();
    }
}

/* EPRI: beg*/
void EVCCSendpwrstat(sl_epwrstat_client_send_ps_req *req)
{
  if (appEpsClient.serverFound == true)
  {
    req->serverSourceInfo.serverHandle = appEpsClient.serverInfo.serverHandle;
    req->serverSourceInfo.source.source = appEpsClient.serverInfo.sourceList[0].source;
    req->serverSourceInfo.source.fsaId = appEpsClient.serverInfo.sourceList[0].fsaId;
    req->transactionId = app_ClientGenerateTransactionID();
    ps_epwrstat_transaction_id = req->transactionId;

    app_SendMail(SL_EPWRSTAT_CLIENT_SEND_PS_REQ, req, sizeof(sl_epwrstat_client_send_ps_req));
  }
}
/* EPRI: end*/

/*******************************************************************************
 * app_PwrstatClientProcessing()
 ******************************************************************************/
/**
 * \brief This example function does some key handling then schedules EDCs
 * \param none
 * \return none
 */
void app_EpwrstatClientProcessing(void)
{
#ifdef APP_PRELOAD_DEMO

    static sl_epwrstat_client_send_ps_req req;

    if (!appEpsClient.serverFound) return;

#if (defined(STDLIB_BUILD) && defined(CYGWIN) && defined(APP_PS_READ_FROM_FILE))
    // In PC-Cygwin, the program PowerStatus.exe periodically reads the AC/Battery status
    // from the PC and writes the data to the file C:\Temp\PowerStatusTemp.txt.  This
    // function will read that file to get relatively up to date Power Status info.

    int          acline, life_percent, flags;
    long         life_secs, full_life_secs;
    FILE        *fp1;
    char         line[256];
    static i32_t sessionBatteryTime = 0;
    u32_t        timeNow            = KAL_GetClock();

    if (timeNow >= appEpsClient.nextPwrstatEventTime)
    {
        KAL_Memset(&req, 0, sizeof(sl_epwrstat_client_send_ps_req));
        req.serverSourceInfo.serverHandle  = appEpsClient.serverInfo.serverHandle;
        req.serverSourceInfo.source.source = appEpsClient.serverInfo.sourceList[0].source;
        req.serverSourceInfo.source.fsaId  = appEpsClient.serverInfo.sourceList[0].fsaId;

        // Default values in case we cannot read real values from file
        acline         = 0;
        life_percent   = 50;
        life_secs      = 3600;
        full_life_secs = 36000;
        flags          = 0;

        // Try to read real values from file
        if (((fp1 = fopen(PWRSTAT_CLIENT_FILE, "r")) != NULL) && (fgets(line, 100, fp1) != NULL))
        {
            fclose(fp1);
            sscanf(line, "%d, %d, %d, %ld, %ld", &acline, &flags, &life_percent, &life_secs, &full_life_secs);
        }

        // Update Battery Status
        flags &= 0xff;
        if (flags == 255) req.pwrstat.batteryStatus = SL_XSD_BATTERY_STATUS_UNKNOWN;
        else if (flags & 0x80) req.pwrstat.batteryStatus = SL_XSD_BATTERY_STATUS_NA;
        else if (flags & 0x04) req.pwrstat.batteryStatus = SL_XSD_BATTERY_STATUS_DEPELETED;
        else if (flags & 0x02) req.pwrstat.batteryStatus = SL_XSD_BATTERY_STATUS_LOW;
        else req.pwrstat.batteryStatus = SL_XSD_BATTERY_STATUS_NORMAL;

        // Update remaining life of battery in percent
        if (life_percent > 100) req.pwrstat.estChargeRemainingExists = false;
        else req.pwrstat.estChargeRemainingExists = true;
        req.pwrstat.estChargeRemaining = (life_percent * 100);

        // Update remaining time on battery in seconds
        if (life_secs < 0) req.pwrstat.estTimeRemainingExists = false;
        else req.pwrstat.estTimeRemainingExists = true;
        req.pwrstat.estTimeRemaining = life_secs;

        // Update misc info
        req.pwrstat.changedTime              = (i64_t)timeNow;
        req.pwrstat.totalTimeOnBatteryExists = false;

        // Update power source
        if (acline) // On AC (mains) power
        {
            req.pwrstat.currentPowerSource         = SL_XSD_POWER_SOURCE_MAIN;
            req.pwrstat.sessionTimeOnBatteryExists = false;
            sessionBatteryTime                     = 0;
        }
        else // On battery power
        {
            req.pwrstat.currentPowerSource         = SL_XSD_POWER_SOURCE_BATTERY;
            req.pwrstat.sessionTimeOnBatteryExists = true;
            sessionBatteryTime += PWRSTAT_CLIENT_SCHEDULE_DELTA;
            req.pwrstat.sessionTimeOnBattery = sessionBatteryTime;
        }

        // Schedule next update
        appEpsClient.nextPwrstatEventTime = timeNow + PWRSTAT_CLIENT_SCHEDULE_DELTA;

        app_SendMail(SL_EPWRSTAT_CLIENT_SEND_PS_REQ, &req, sizeof(sl_epwrstat_client_send_ps_req));
    }

#else  // !(defined(STDLIB_BUILD) && defined(CYGWIN) && defined(APP_PS_READ_FROM_FILE))

    // No access to real data, so simulate operation of AC and Battery.
    //
    u32_t       timeNow            = KAL_GetClock();
    static u8_t currentPowerSource = SL_XSD_POWER_SOURCE_UNKNOWN;

    if (timeNow >= appEpsClient.nextPwrstatEventTime)
    {
        switch (currentPowerSource)
        {
        case SL_XSD_POWER_SOURCE_UNKNOWN:
            // This is the initial case. Send initial power status.
            KAL_Memset(&(req), 0, sizeof(sl_epwrstat_client_send_ps_req));
            req.serverSourceInfo.serverHandle  = appEpsClient.serverInfo.serverHandle;
            req.serverSourceInfo.source.source = appEpsClient.serverInfo.sourceList[0].source;
            req.serverSourceInfo.source.fsaId  = appEpsClient.serverInfo.sourceList[0].fsaId;

            req.pwrstat.currentPowerSource         = SL_XSD_POWER_SOURCE_MAIN;
            req.pwrstat.batteryStatus              = SL_XSD_BATTERY_STATUS_NA;
            req.pwrstat.estChargeRemainingExists   = false;
            req.pwrstat.estTimeRemainingExists     = false;
            req.pwrstat.sessionTimeOnBatteryExists = false;
            req.pwrstat.totalTimeOnBatteryExists   = false;
            req.pwrstat.estChargeRemaining         = 0;
            req.pwrstat.estTimeRemaining           = 0;
            req.pwrstat.sessionTimeOnBattery       = 0;
            req.pwrstat.changedTime                = KAL_GetClock();

            currentPowerSource = SL_XSD_POWER_SOURCE_MAIN;
            app_SendMail(SL_EPWRSTAT_CLIENT_SEND_PS_REQ, &req, sizeof(sl_epwrstat_client_send_ps_req));
            break;

        case SL_XSD_POWER_SOURCE_MAIN:
            if (appEpsClient.pwrstatUpTime >= PWRSTAT_CLIENT_SOURCE_TIME) // switch to battery
            {
                currentPowerSource                     = SL_XSD_POWER_SOURCE_BATTERY;
                appEpsClient.pwrstatUpTime             = 0;
                req.pwrstat.currentPowerSource         = SL_XSD_POWER_SOURCE_BATTERY;
                req.pwrstat.batteryStatus              = SL_XSD_BATTERY_STATUS_NORMAL;
                req.pwrstat.estChargeRemainingExists   = true;
                req.pwrstat.estTimeRemainingExists     = true;
                req.pwrstat.sessionTimeOnBatteryExists = true;
                req.pwrstat.totalTimeOnBatteryExists   = true;
                req.pwrstat.estChargeRemaining         = 9876;
                req.pwrstat.estTimeRemaining           = 3600;
                req.pwrstat.sessionTimeOnBattery       = 0;
                req.pwrstat.changedTime                = timeNow;
                app_SendMail(SL_EPWRSTAT_CLIENT_SEND_PS_REQ, &req, sizeof(sl_epwrstat_client_send_ps_req));
            }
            appEpsClient.pwrstatUpTime += PWRSTAT_CLIENT_SCHEDULE_DELTA;
            break;
        case SL_XSD_POWER_SOURCE_BATTERY:
            if (appEpsClient.pwrstatUpTime >= PWRSTAT_CLIENT_SOURCE_TIME) // switch to AC mains
            {
                currentPowerSource                     = SL_XSD_POWER_SOURCE_MAIN;
                appEpsClient.pwrstatUpTime             = 0;
                req.pwrstat.currentPowerSource         = SL_XSD_POWER_SOURCE_MAIN;
                req.pwrstat.batteryStatus              = SL_XSD_BATTERY_STATUS_NA;
                req.pwrstat.estChargeRemainingExists   = false;
                req.pwrstat.estTimeRemainingExists     = false;
                req.pwrstat.sessionTimeOnBatteryExists = false;
                req.pwrstat.totalTimeOnBatteryExists   = false;
                req.pwrstat.estChargeRemaining         = 0;
                req.pwrstat.estTimeRemaining           = 0;
                req.pwrstat.sessionTimeOnBattery       = 0;
                req.pwrstat.changedTime                = timeNow;
            }
            else // continue with battery, simulate battery depletion
            {
                // Note: No checking of overflow/underflow
                req.pwrstat.estChargeRemaining -= (PWRSTAT_CLIENT_SCHEDULE_DELTA * 10);
                req.pwrstat.estTimeRemaining -= PWRSTAT_CLIENT_SCHEDULE_DELTA;
                req.pwrstat.sessionTimeOnBattery += PWRSTAT_CLIENT_SCHEDULE_DELTA;
                req.pwrstat.totalTimeOnBattery += PWRSTAT_CLIENT_SCHEDULE_DELTA;
                req.pwrstat.changedTime = timeNow;
            }
            appEpsClient.pwrstatUpTime += PWRSTAT_CLIENT_SCHEDULE_DELTA;
            app_SendMail(SL_EPWRSTAT_CLIENT_SEND_PS_REQ, &req, sizeof(sl_epwrstat_client_send_ps_req));
            break;
        default:
            break;
        }
        appEpsClient.nextPwrstatEventTime = timeNow + PWRSTAT_CLIENT_SCHEDULE_DELTA;
    }
#endif // (defined(STDLIB_BUILD) && defined(CYGWIN) && defined(APP_PS_READ_FROM_FILE))

#endif // APP_PRELOAD_DEMO
}

/*******************************************************************************
 * app_PwrstatClientStart()
 ******************************************************************************/
/**
 * \brief Called after the G2H stack is started to load DRLC server data to G2H Stack
 * \param none
 * \return none
 */
void app_EpwrstatClientStart(void)
{

#ifdef APP_PRELOAD_DEMO

#endif // APP_PRELOAD_DEMO
}
/*EPRI: beg*/
static void HandleEpwrStatusCnf(sl_epwrstat_client_send_ps_cnf * pInd) {
    if (pInd->transactionId == ps_epwrstat_transaction_id) {
        ps_epwrstat_transaction_id = 0;
    }
}
/*EPRI: end*/
/*******************************************************************************
 * app_EpwrstatClientHandleMail()
 ******************************************************************************/
void app_EpwrstatClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
    switch (mailId)
    {
    case SL_EPWRSTAT_CLIENT_SERVER_IND:
        APP_LOG_INFO("app_EpwrstatClientHandleMail: SL_EPWRSTAT_CLIENT_SERVER_IND");
        AppEpwrstatClientHandleServerInd((sl_epwrstat_client_server_ind *)pMail);
        break;
    case SL_EPWRSTAT_CLIENT_SEND_PS_CNF:
        APP_LOG_INFO("app_EpwrstatClientHandleMail: SL_EPWRSTAT_CLIENT_SEND_PS_CNF");
        app_LogStatus(((sl_epwrstat_client_send_ps_cnf *)pMail)->status);
        HandleEpwrStatusCnf((sl_epwrstat_client_send_ps_cnf *)pMail);
        break;
    }
}

#endif // APP_CONFIG_EPS_CLIENT_ENABLED
