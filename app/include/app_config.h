/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
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

#include "app.h"
#include "sl_api.h"

#ifndef VERSION
#define VERSION "1.1.0"
#define APP_CONFIG_SW_VERSION   (VERSION)
#endif

/******************************************************************************
  Application defines
 ******************************************************************************/
#ifdef OVERRIDE_APP_AUTO_START
#define APP_AUTO_START OVERRIDE_APP_AUTO_START
#endif // matches #ifdef OVERRIDE_APP_AUTO_START
#ifdef OVERRIDE_APP_PRELOAD_DEMO
#define APP_PRELOAD_DEMO OVERRIDE_APP_PRELOAD_DEMO
#endif // matches #ifdef OVERRIDE_APP_PRELOAD_EMO
//#define APP_AS_EV_CHARGING_DEMO  // Uncomment for Application Server EV charging demo. APP_PRELOAD_DEMO required.
#if defined(APP_AS_EV_CHARGING_DEMO) && !defined(APP_PRELOAD_DEMO)
#define APP_PRELOAD_DEMO
#endif
#ifdef OVERRIDE_APP_DNS_STANDARD
#define APP_DNS_STANDARD OVERRIDE_APP_DNS_STANDARD
#else
#define APP_DNS_STANDARD SL_DNS_STANDARD_MDNS
#endif                // matches #ifdef OVERRIDE_APP_DNS_STANDARD
#define IANA_PEN 1233 // Itron Private Enterprise Number (PEN)
#define STELLANTIS_IANA_PEN 57872
//#define APP_CONFIG_FLOW_SERVER_ALG_ENABLED  // Uncomment to use flow server algorithm
#define APP_STARTUP_BIT  (17)
#define APP_STARTUP_MASK (1 << APP_STARTUP_BIT)

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
#define APP_CONFIG_CLIENT_DEVICE          1
#define APP_CONFIG_CLIENT_FOR_NOTIFY_ONLY 1

/******************************************************************************
  Task Defines
 ******************************************************************************/
#define APP_STACK_SIZE    1024
#define APP_TASK_PRIORITY 50

#endif // APP_CONFIG_H
