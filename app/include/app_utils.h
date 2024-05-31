/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
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

#ifndef APP_UTILS_H
#define APP_UTILS_H

#include "app_logs.h"
#include "kal_api.h"
#include "sl_api.h"

/*******************************************************************************
 * APP Utility Defines
 ******************************************************************************/

/******************************************************************************
  Device Security defines
 ******************************************************************************/
#define APP_DEV_SEC_INFO_FILE_LOCAL "devcertinfo"
#define APP_ROOT_CERT_FILE_LOCAL    "rootcacert.pem"
#define APP_DEV_SEC_INFO_FILENAME   "/etc/kitu/devinfo/devcertinfo"
#define APP_ROOT_CERT_FILE_REMOTE   "/etc/kitu/devinfo/rootcacert.pem"

#define APP_IGNORE_LEADING_ZEROS  (0)
#define APP_MAX_PEM_LINE          (82)
#define APP_MAX_CERT_CHAIN_LENGTH (2560)

/*******************************************************************************
 * APP Public Data
 ******************************************************************************/
extern bool  appPassiveMode; // When set the clients and servers will not do active processing
extern u64_t appSfdi;

extern char *commandLineSecInfoBasename;
extern char *commandLineSecRootCAFilename;
extern char *commandLinePEMChainFile;
extern char *commandLineBasicECPrivKeyFile;

/*******************************************************************************
 * APP Utility Functions
 ******************************************************************************/
void   app_SendAppMail(kal_mail_id_t mailId, void *pMail, u16_t mailLen);
void   app_SendMail(kal_mail_id_t mailId, void *pMail, u16_t mailLen);
void   app_SendMailAndWait(kal_mail_id_t mailId, void *pMail, u16_t mailLen, kal_mail_id_t replyMailId, void *pReplyMail, const size_t sizeReply);
u32_t  app_mrIdGenerate(u8_t mrId[]);
void   app_LogId(u16_t id);
void   app_LogInstanceIds(u8_t numInstanceIDs, sl_obj_instance_id_t instanceIDs[]);
void   app_LogStatus(i8_t status);
void   app_Syslog(int errorCode, char errorMsg[]);
void   app_Terminate(int errorCode, char errorMsg[]);
u32_t  app_SimpleHash(u8_t *key, int len);
double app_ElapsedTime(int mode);
void   app_MridtoString256(u8_t *mrId, char *text);
void   app_ClientChangePollRate(u32_t rate);

int  LoadSecurityInfoFromFile(void);
int  LoadSecurityInfoFromItronApi(void);
void PopulateWhiteList(void);
void LoadPrivateKey(void);

u64_t app_UtilsGetMySfdi(void);
u8_t *app_UtilsGetMyLfdi(void);
void  app_UtilsSetMySfdi(u64_t sfdi);
void  app_UtilsSetMyLfdi(u8_t *lfdi);

#endif // APP_UTILS_H
