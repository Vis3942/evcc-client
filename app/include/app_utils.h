/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_utils.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Gordon Lum
 *
 * \brief         Application Code Utilities
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef   APP_UTILS_H
#define   APP_UTILS_H

#include <string.h>

#include "kal_api.h"
#include "sl_api.h"
#include "app_logs.h"


/*******************************************************************************
 * APP Utility Defines
 ******************************************************************************/
#define APP_LOG_INFO(msg)    fprintf(stdout, "## %s\n", msg) //DM_LogMsg(DM_ID_APP, DM_LOG_PRIORITY_INFO,  msg)
#define APP_LOG_WARNING(msg) DM_LogMsg(DM_ID_APP, DM_LOG_PRIORITY_WARNING,  msg)
#define APP_LOG_ERROR(msg)   DM_LogMsg(DM_ID_APP, DM_LOG_PRIORITY_ERROR, msg)

#define APP_DLOG(priority,format,...) DM_LogMsgFunc(DM_ID_APP, priority, format, ## __VA_ARGS__)
#define APP_DLOG_INFO(format,...)     DM_LogMsgFunc(DM_ID_APP, DM_LOG_PRIORITY_INFO, format, ## __VA_ARGS__)
#define APP_DLOG_WARNING(format,...)  DM_LogMsgFunc(DM_ID_APP, DM_LOG_PRIORITY_WARNING, format, ## __VA_ARGS__)
#define APP_DLOG_ERROR(format,...)    DM_LogMsgFunc(DM_ID_APP, DM_LOG_PRIORITY_ERROR, format, ## __VA_ARGS__)

#define APP_DEV_SEC_INFO_FILE_LOCAL   "devcertinfo"
#define APP_ROOT_CERT_FILE_LOCAL      "rootcacert.pem"

/******************************************************************************
  Device Security defines
 ******************************************************************************/
// APP_DEV_SEC_INFO_FILENAME
// Uncomment to load device security information at runtime. In addition:
// -- Requires real file system
// -- Requires device certificate info file named as follows,or modified to
//    match real filename
// -- Requires SL library with run-time loading of certificate enabled
#ifdef OVERRIDE_APP_DEV_SEC_INFO_FILENAME
  #define APP_DEV_SEC_INFO_FILENAME OVERRIDE_APP_DEV_SEC_INFO_FILENAME
  #define APP_ROOT_CERT_FILE_REMOTE  "/etc/kitu/devinfo/rootcacert.pem"
#else
  #define APP_DEV_SEC_INFO_FILENAME "/etc/kitu/devinfo/devcertinfo"
  #define APP_ROOT_CERT_FILE_REMOTE "/etc/kitu/devinfo/rootcacert.pem"
#endif

#define APP_IGNORE_LEADING_ZEROS      (0) 
#define APP_MAX_PEM_LINE              (82)
#define APP_MAX_CERT_CHAIN_LENGTH     (2560)

/*******************************************************************************
 * APP Public Data
 ******************************************************************************/
extern bool  appPassiveMode;  // When set the clients and servers will not do active processing
extern u64_t appSfdi;
extern u8_t appLfdi[];

extern char  *commandLineSecInfoBasename;
extern char  *commandLineSecRootCAFilename;
extern char  *commandLinePEMChainFile;
extern char  *commandLineBasicECPrivKeyFile;

/*******************************************************************************
 * APP Utility Functions
 ******************************************************************************/
void app_SendAppMail(kal_mail_id_t mailId, void *pMail, u16_t mailLen);
void app_SendMail(kal_mail_id_t mailId, void *pMail, u16_t mailLen);
void app_SendMailAndWait(kal_mail_id_t mailId, void *pMail, u16_t mailLen,
                         kal_mail_id_t replyMailId, void *pReplyMail, const size_t sizeReply);
u32_t app_mrIdGenerate(u8_t mrId[]);
void app_LogId(u16_t id);
void app_LogInstanceIds(u8_t numInstanceIDs, sl_obj_instance_id_t instanceIDs[]);
void app_LogStatus(i8_t status);
void app_Syslog(int errorCode, char errorMsg[]);
void app_Terminate(int errorCode, char errorMsg[]);
u32_t app_SimpleHash(u8_t *key, int len);
double app_ElapsedTime(int mode);
void app_MridtoString256(u8_t *mrId, char *text);

bool isInvalidPemChar(u8_t byte);
u8_t *base64_encode(u8_t *src, int len, int *out_len);
u8_t *base64_decode(u8_t *src, int len, int *out_len);
int LoadSecurityInfoFromFile(void);
int LoadSecurityInfoFromItronApi(void);
void PopulateWhiteList(void);
void LoadPrivateKey(void);
void SetPollRate();

extern size_t strnlen(const char *s, size_t maxlen);  // For Iotecha compiler

#endif  // APP_UTILS_H
