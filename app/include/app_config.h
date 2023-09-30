/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_config.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Gordon Lum
 *
 * \brief         Generic configuration for testing
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "sl_api.h"
#include "app.h"

#ifndef VERSION
#define VERSION "1.1.0"
#define APP_CONFIG_SW_VERSION   (VERSION)
#endif

/******************************************************************************
  Application defines
 ******************************************************************************/
#ifdef OVERRIDE_APP_AUTO_START
#define APP_AUTO_START                                OVERRIDE_APP_AUTO_START
#endif  // matches #ifdef OVERRIDE_APP_AUTO_START
#ifdef OVERRIDE_APP_PRELOAD_DEMO
#define APP_PRELOAD_DEMO                              OVERRIDE_APP_PRELOAD_DEMO
#endif  // matches #ifdef OVERRIDE_APP_PRELOAD_EMO
//#define APP_AS_EV_CHARGING_DEMO  // Uncomment for Application Server EV charging demo. APP_PRELOAD_DEMO required.
#if defined(APP_AS_EV_CHARGING_DEMO) && !defined(APP_PRELOAD_DEMO)
  #define APP_PRELOAD_DEMO
#endif
#ifdef OVERRIDE_APP_DNS_STANDARD
#define APP_DNS_STANDARD                              OVERRIDE_APP_DNS_STANDARD
#else
#define APP_DNS_STANDARD                              SL_DNS_STANDARD_MDNS
#endif  // matches #ifdef OVERRIDE_APP_DNS_STANDARD
#define IANA_PEN        1233                  // Itron Private Enterprise Number (PEN)

//#define APP_CONFIG_FLOW_SERVER_ALG_ENABLED  // Uncomment to use flow server algorithm
#define APP_STARTUP_BIT                (17)
#define APP_STARTUP_MASK               (1 << APP_STARTUP_BIT)


/******************************************************************************
  ZigbeeIP-specific defines
 ******************************************************************************/
// The ZigbeeIP defines below are only applicable if APP_AUTO_START enabled
#define APP_DEVICE_TYPE          APP_6LBR_DEVICE  // 6LBR=ROOT, 6LR=ROUTER
#define APP_CHAN_NUM             15

#define APP_6LBR_SHORT_ADDR      0x1122  // Note that the short addr must be unique for each device
#define APP_6LR_SHORT_ADDR       0x3344
#define APP_HOST_SHORT_ADDR      0x5566

#define APP_IEEE_ADDR            SL_ZB_DEFAULT_IEEE_ADDR
#define APP_6LBR_IEEE_LSB        0x01  // Note that the IEEE addr must be unique for each device
#define APP_6LR_IEEE_LSB         0x02
#define APP_HOST_IEEE_LSB        0x03

/******************************************************************************
  Device Security defines
 ******************************************************************************/
// Uncomment to load device security information at runtime. In addition:
// -- Requires real file system
// -- Requires device certificate info file named as follows,or modified to
//    match real filename
// -- Device certificate info file must be built with proper format (network byte
//    ordered) as follows:
//       2 bytes privateKeyLen
//       x bytes privateKey
//       8 bytes sfdi
//      20 bytes lfdi
//       2 bytes certChainLen
//       y bytes certChain
// -- Requires CLIENT_PKI_DEV_LOAD_AT_RUNTIME to be uncommented in sec_certificate.h
//#define APP_DEV_SEC_INFO_FILENAME "devcertinfo.bin"

//#define APP_PS_READ_FROM_FILE   // For PC-Cygwin platform ONLY !!! Load power status from C:\Temp\PowerStatusTemp.txt

/******************************************************************************
  Client/Server defines
 ******************************************************************************/
#define APP_NUM_CLIENTS_ENABLED ( G2H_CONFIG_ECFG_CLIENT_ENABLED        + \
                                 G2H_CONFIG_SCFG_CLIENT_ENABLED         + \
                                 G2H_CONFIG_DCAP_CLIENT_ENABLED         + \
                                 G2H_CONFIG_DER_CLIENT_ENABLED          + \
                                 G2H_CONFIG_SDINFO_CLIENT_ENABLED       + \
                                 G2H_CONFIG_EDINFO_CLIENT_ENABLED       + \
                                 G2H_CONFIG_DRLC_CLIENT_ENABLED         + \
                                 G2H_CONFIG_EDSTAT_CLIENT_ENABLED       + \
                                 G2H_CONFIG_SDSTAT_CLIENT_ENABLED       + \
                                 G2H_CONFIG_EDER_CLIENT_ENABLED         + \
                                 G2H_CONFIG_EDEV_CLIENT_ENABLED         + \
                                 G2H_CONFIG_EFILE_CLIENT_ENABLED        + \
                                 G2H_CONFIG_FILE_CLIENT_ENABLED         + \
                                 G2H_CONFIG_FLOW_CLIENT_ENABLED         + \
                                 G2H_CONFIG_FSA_CLIENT_ENABLED          + \
                                 G2H_CONFIG_ELEL_CLIENT_ENABLED         + \
                                 G2H_CONFIG_SLEL_CLIENT_ENABLED         + \
                                 G2H_CONFIG_EDRLC_CLIENT_ENABLED        + \
                                 G2H_CONFIG_SDRLC_CLIENT_ENABLED        + \
                                 G2H_CONFIG_METER_CLIENT_ENABLED        + \
                                 G2H_CONFIG_METER_MIRROR_CLIENT_ENABLED + \
                                 G2H_CONFIG_MSG_CLIENT_ENABLED          + \
                                 G2H_CONFIG_SNS_CLIENT_ENABLED          + \
                                 G2H_CONFIG_ENS_CLIENT_ENABLED          + \
                                 G2H_CONFIG_NOTIFY_CLIENT_ENABLED       + \
                                 G2H_CONFIG_PPY_CLIENT_ENABLED          + \
                                 G2H_CONFIG_PRICE_CLIENT_ENABLED        + \
                                 G2H_CONFIG_EPS_CLIENT_ENABLED          + \
                                 G2H_CONFIG_SPS_CLIENT_ENABLED          + \
                                 G2H_CONFIG_RESP_CLIENT_ENABLED         + \
                                 G2H_CONFIG_SDER_CLIENT_ENABLED         + \
                                 G2H_CONFIG_SDEV_CLIENT_ENABLED         + \
                                 G2H_CONFIG_SFILE_CLIENT_ENABLED        + \
                                 G2H_CONFIG_SUBS_CLIENT_ENABLED         + \
                                 G2H_CONFIG_TIME_CLIENT_ENABLED )

#if (APP_NUM_CLIENTS_ENABLED != 0)
#define APP_CONFIG_CLIENT_DEVICE 1
#else
#define APP_CONFIG_CLIENT_DEVICE 0
#endif

#if ((APP_NUM_CLIENTS_ENABLED == 1) && (G2H_CONFIG_NOTIFY_CLIENT_ENABLED == 1))
#define APP_CONFIG_CLIENT_FOR_NOTIFY_ONLY 1
#else
#define APP_CONFIG_CLIENT_FOR_NOTIFY_ONLY 0
#endif

#define APP_NUM_SERVERS_ENABLED ( G2H_CONFIG_BILLING_SERVER_ENABLED     + \
                                 G2H_CONFIG_ECFG_SERVER_ENABLED         + \
                                 G2H_CONFIG_SCFG_SERVER_ENABLED         + \
                                 G2H_CONFIG_DCAP_SERVER_ENABLED         + \
                                 G2H_CONFIG_DER_SERVER_ENABLED          + \
                                 G2H_CONFIG_SDINFO_SERVER_ENABLED       + \
                                 G2H_CONFIG_EDINFO_SERVER_ENABLED       + \
                                 G2H_CONFIG_DRLC_SERVER_ENABLED         + \
                                 G2H_CONFIG_EDSTAT_SERVER_ENABLED       + \
                                 G2H_CONFIG_SDSTAT_SERVER_ENABLED       + \
                                 G2H_CONFIG_EDER_SERVER_ENABLED         + \
                                 G2H_CONFIG_EDEV_SERVER_ENABLED         + \
                                 G2H_CONFIG_EFILE_SERVER_ENABLED        + \
                                 G2H_CONFIG_FILE_SERVER_ENABLED         + \
                                 G2H_CONFIG_FLOW_SERVER_ENABLED         + \
                                 G2H_CONFIG_FSA_SERVER_ENABLED          + \
                                 G2H_CONFIG_ELEL_SERVER_ENABLED         + \
                                 G2H_CONFIG_SLEL_SERVER_ENABLED         + \
                                 G2H_CONFIG_EDRLC_SERVER_ENABLED        + \
                                 G2H_CONFIG_SDRLC_SERVER_ENABLED        + \
                                 G2H_CONFIG_METER_SERVER_ENABLED        + \
                                 G2H_CONFIG_METER_MIRROR_SERVER_ENABLED + \
                                 G2H_CONFIG_MSG_SERVER_ENABLED          + \
                                 G2H_CONFIG_SNS_SERVER_ENABLED          + \
                                 G2H_CONFIG_ENS_SERVER_ENABLED          + \
                                 G2H_CONFIG_NOTIFY_SERVER_ENABLED       + \
                                 G2H_CONFIG_PPY_SERVER_ENABLED          + \
                                 G2H_CONFIG_PRICE_SERVER_ENABLED        + \
                                 G2H_CONFIG_EPS_SERVER_ENABLED          + \
                                 G2H_CONFIG_SPS_SERVER_ENABLED          + \
                                 G2H_CONFIG_RESP_SERVER_ENABLED         + \
                                 G2H_CONFIG_SDER_SERVER_ENABLED         + \
                                 G2H_CONFIG_SDEV_SERVER_ENABLED         + \
                                 G2H_CONFIG_SFILE_SERVER_ENABLED        + \
                                 G2H_CONFIG_SUBS_SERVER_ENABLED         + \
                                 G2H_CONFIG_TIME_SERVER_ENABLED )

#if (APP_NUM_SERVERS_ENABLED != 0)
#define APP_CONFIG_SERVER_DEVICE 1
#else
#define APP_CONFIG_SERVER_DEVICE 0
#endif

#if ((APP_NUM_SERVERS_ENABLED == 1) && (G2H_CONFIG_NOTIFY_SERVER_ENABLED == 1))
#define APP_CONFIG_SERVER_FOR_NOTIFY_ONLY 1
#else
#define APP_CONFIG_SERVER_FOR_NOTIFY_ONLY 0
#endif

/******************************************************************************
  Task Defines
 ******************************************************************************/
#define APP_STACK_SIZE 1024
#define APP_TASK_PRIORITY 50


#endif  // APP_CONFIG_H
