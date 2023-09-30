/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_utils.c
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>

#if defined(STDLIB_BUILD)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#endif
#include "app_config.h"
#include "kal_api.h"
#include "psal_assert.h"
#include "app_config.h"
#include "app_utils.h"


/*******************************************************************************
 * Public Data
 ******************************************************************************/
bool   appPassiveMode = false;  // When set the clients and servers will not do active processing
u64_t  appSfdi=0;
u8_t appLfdi[SL_XSD_LFDI_LEN];


char  *commandLineSecInfoBasename = NULL;
char  *commandLineSecRootCAFilename = NULL;
char  *commandLinePEMChainFile = NULL;
char  *commandLineBasicECPrivKeyFile = NULL;

/*******************************************************************************
 * Private Constants
 ******************************************************************************/

/*******************************************************************************
 * Private Data
 ******************************************************************************/

/*******************************************************************************
 * Local Functions
 ******************************************************************************/


/*******************************************************************************
 * app_SendMail
 ******************************************************************************/
/**
 * \brief Sends a mail message
 * \param mailId - Id of mail to send
 * \param pMail - Data to send in the mail
 * \param mailLen - Length of the mail data
 * \return none
 */
void app_SendMail(kal_mail_id_t mailId, void *pMail, u16_t mailLen)
{
  kal_mail_details_t mailDetails;

  mailDetails.ParameterLength = mailLen;
  mailDetails.pParameterBlock = pMail;
  mailDetails.iPartnerTask = SL_TaskHandle;
  mailDetails.MailId = mailId;

  if ( KAL_MailSend(0,&mailDetails) != 0 )
  {
    APP_LOG_ERROR ("app_SendMail: KAL_MailSend error");
  }
}

/*******************************************************************************
 * app_SendMailAndWait
 ******************************************************************************/
/**
 * \brief Sends a mail message
 * \param mailId - Id of mail to send
 * \param pMail - Data to send in the mail
 * \param mailLen - Length of the mail data
 * \return none
 */
void app_SendMailAndWait(kal_mail_id_t mailId, void *pMail, u16_t mailLen,
                         kal_mail_id_t replyMailId, void *pReplyMail, const size_t sizeReply)
{
    kal_mail_details_t mailDetails;
    kal_message_details_t msgDetails;
    kal_mail_details_t replyMailDetails;
    u8_t retCode;

    PSAL_ASSERT(pReplyMail != NULL);

    mailDetails.ParameterLength = mailLen;
    mailDetails.pParameterBlock = pMail;
    mailDetails.iPartnerTask = SL_TaskHandle;
    mailDetails.MailId = mailId;

    retCode = KAL_MailSyncSendAndWait(NULL, &mailDetails, replyMailId,
            &msgDetails, &replyMailDetails, 0);

    if (retCode != G2_SUCCESS) {
        // What to do, what to do....  Simply return??  No, we need to abort.
        //   Callers will dereference *pReplyMail without examining if it is
        //   set to NULL, so this routine *must* work, else bad, bad mojo will
        //   occur.  For now, abort.  In the future, have the caller examine
        //   the contents before dereferencing.
        switch (retCode) {
        case G2_FAILURE:
            APP_LOG_ERROR ("app_SendMailAndWait: KAL_MailSyncSendAndWait error: G2_FAILURE");
            break;
        case G2_BAD_HTASK:
            APP_LOG_ERROR ("app_SendMailAndWait: KAL_MailSyncSendAndWait error: G2_BAD_HTASK");
            break;
        case G2_BAD_HMESSAGE:
            APP_LOG_ERROR ("app_SendMailAndWait: KAL_MailSyncSendAndWait error: G2_BAD_HMESSAGE");
            break;
        case G2_TIMEOUT:
            APP_LOG_ERROR ("app_SendMailAndWait: KAL_MailSyncSendAndWait error: G2_TIMEOUT");
            break;
        case G2_ALLOCATION_FAIL:
            APP_LOG_ERROR ("app_SendMailAndWait: KAL_MailSyncSendAndWait error: G2_ALLOCATION_FAIL");
            break;
        case G2_SYNCMAIL_NOT_SUPPORTED:
            APP_LOG_ERROR ("app_SendMailAndWait: KAL_MailSyncSendAndWait error: G2_SYNCMAIL_NOT_SUPPORTED");
            break;
        case G2_SYNCMAIL_INVALID:
            APP_LOG_ERROR ("app_SendMailAndWait: KAL_MailSyncSendAndWait error: G2_SYNCMAIL_INVALID");
            break;
        }

        // Die a horrible, screaming death!!
        PSAL_ASSERT_FORCE();
    }
    else {
        KAL_Memcpy(pReplyMail, replyMailDetails.pParameterBlock, sizeReply);
    }
}

/*******************************************************************************
 * app_SendAppMail
 ******************************************************************************/
/**
 * \brief Sends a mail message
 * \param mailId - Id of mail to send
 * \param pMail - Data to send in the mail
 * \param mailLen - Length of the mail data
 * \return none
 */
void app_SendAppMail(kal_mail_id_t mailId, void *pMail, u16_t mailLen)
{
  kal_mail_details_t mailDetails;

  mailDetails.ParameterLength = mailLen;
  mailDetails.pParameterBlock = pMail;
  mailDetails.iPartnerTask = APP_TaskHandle;
  mailDetails.MailId = mailId;

  if ( KAL_MailSend(0,&mailDetails) != 0 )
  {
    APP_LOG_ERROR ("app_SendAppMail: KAL_MailSend error");
  }
}

/*******************************************************************************
* app_mrIdGenerate()
*******************************************************************************/
/**
* \brief  Generates a unique 128-bit mrId[] as follows:
*         mrId[0..4]   - 40-bit SFDI
*         mrId[5..8]   - The current UTC time returned by KAL_GetClock().
*         mrId[9..11]  - AppMrIdCounter, a 24-bit counter that increments for each new mrId
*         mrId[12..15] - The Private Enterprise Number (PEN)
* \param  mrId[]
* \return AppMrIdCounter value used in mrId
*/
u32_t app_mrIdGenerate(u8_t mrId[])
{
  static u32_t  AppMrIdCounter = 0;  // counter part of mrId
  i64_t         ltemp;

  // Place the 40-bit SFDI
  mrId[0] = (u8_t) (appSfdi >> 32);
  mrId[1] = (u8_t) (appSfdi >> 24);
  mrId[2] = (u8_t) (appSfdi >> 16);
  mrId[3] = (u8_t) (appSfdi >> 8);
  mrId[4] = (u8_t) (appSfdi);

  // Create UTC part of mrId
  ltemp = KAL_GetClock();
  mrId[5] = (u8_t)(ltemp >> 24);
  mrId[6] = (u8_t)(ltemp >> 16);
  mrId[7] = (u8_t)(ltemp >> 8);
  mrId[8] = (u8_t)(ltemp);

  // Create counter part of mrId
  AppMrIdCounter++;
  mrId[9] = (u8_t)(AppMrIdCounter >> 16);
  mrId[10] = (u8_t)(AppMrIdCounter >> 8);
  mrId[11] = (u8_t)(AppMrIdCounter);

  // Create PEN part of mrId
  mrId[12] = (u8_t)(IANA_PEN >> 24);
  mrId[13] = (u8_t)(IANA_PEN >> 16);
  mrId[14] = (u8_t)(IANA_PEN >> 8);
  mrId[15] = (u8_t)(IANA_PEN);

  return (AppMrIdCounter);
}


/*******************************************************************************
 * app_LogId
 ******************************************************************************/
/**
 * \brief Outputs a debug message based on status
 * \param status - status to log
 * \return none
 */
void app_LogId(u16_t id)
{
  // Log Id
  char idStr[10];
  snprintf(idStr,sizeof(idStr), "Id=0x%X", id);
  APP_LOG_INFO (idStr);
}

/*******************************************************************************
 * app_LogInstanceIds
 ******************************************************************************/
/**
 * \brief Outputs a debug message containing an array of instance IDs
 * \param status - status to log
 * \return none
 */
void app_LogInstanceIds(u8_t numInstanceIDs, sl_obj_instance_id_t instanceIDs[])
{
  // This function needs to be updated if the max number of instance IDs change.
  PSAL_ASSERT(SL_OBJ_MAX_NUM_INSTANCE_ID == 6);

  switch(numInstanceIDs)
  {
    case 0:
      APP_DLOG_INFO("no Instance IDs associated with the resource.");
      break;
    case 1:
      APP_DLOG_INFO("Instance IDs(1): {%d}",
          instanceIDs[0]);
      break;
    case 2:
      APP_DLOG_INFO("Instance IDs(2): {%d, %d}",
          instanceIDs[0], instanceIDs[1]);
      break;
    case 3:
      APP_DLOG_INFO("Instance IDs(3): {%d, %d, %d}",
          instanceIDs[0], instanceIDs[1], instanceIDs[2]);
      break;
    case 4:
      APP_DLOG_INFO("Instance IDs(4): {%d, %d, %d, %d}",
          instanceIDs[0], instanceIDs[1], instanceIDs[2],
          instanceIDs[3]);
      break;
    case 5:
      APP_DLOG_INFO("Instance IDs(5): {%d, %d, %d, %d, %d}",
          instanceIDs[0], instanceIDs[1], instanceIDs[2],
          instanceIDs[3], instanceIDs[4]);
      break;
    case 6:
      APP_DLOG_INFO("Instance IDs(6): {%d, %d, %d, %d, %d, %d}",
          instanceIDs[0], instanceIDs[1], instanceIDs[2],
          instanceIDs[3], instanceIDs[4], instanceIDs[5]);
      break;

  }
}

/*******************************************************************************
 * app_LogStatus
 ******************************************************************************/
/**
 * \brief Outputs a debug message based on status
 * \param status - status to log
 * \return none
 */
void app_LogStatus(i8_t status)
{
  switch (status)
  {
  case SL_CMN_RC_SUCCESS:
    APP_LOG_INFO("Status = Success");
    break;
  case SL_CMN_RC_OPERATION_IN_PROG:
    APP_LOG_INFO("Status = Operation in Progress");
    break;
  case SL_CMN_RC_OPERATION_DONE:
    APP_LOG_INFO("Status = Operation Done");
    break;
  case SL_CMN_RC_FAIL:
    APP_LOG_ERROR("Status = Err failed");
    break;
  case SL_CMN_RC_FILE_EXISTS:
    APP_LOG_ERROR("Status = Err file exists");
    break;
  case SL_CMN_RC_FILE_DOES_NOT_EXIST:
    APP_LOG_ERROR("Status = Err file does not exist");
    break;
  case SL_CMN_RC_NOT_ALLOWED:
    APP_LOG_ERROR("Status = Err not allowed");
    break;
  case SL_CMN_RC_FS_FULL:
    APP_LOG_ERROR("Status = File system full");
    break;
  case SL_CMN_RC_METHOD_NOT_ALLOWED:
    APP_LOG_ERROR("Status = Err method not allowed");
    break;
  case SL_CMN_RC_TIMEOUT:
    APP_LOG_ERROR("Status = Err timeout");
    break;
  case SL_CMN_RC_NO_SERVER:
    APP_LOG_ERROR("Status = Err no server");
    break;
  case SL_CMN_RC_INVALID_PARAMETERS:
    APP_LOG_INFO("Status = Err invalid parameters");
    break;
  case SL_CMN_RC_MRID_MISMATCH:
    APP_LOG_ERROR("Status = Err MRID mismatch");
    break;
  default:
    APP_LOG_ERROR("Status = Unknown");
    break;
  }
}

/*******************************************************************************
* app_Syslog
*******************************************************************************/
void app_Syslog(int errorCode, char errorMsg[])
{
  char  ident[64];

  fflush(stdout);
  fflush(stderr);
  snprintf(ident, 64, "%lld: Kitu", KAL_GetClock());
  openlog(ident, LOG_PERROR, LOG_USER);
  syslog(LOG_ERR, "%s", errorMsg);
  closelog();
#ifdef YOCTO_OS
  printf("%s: %s\n", ident, errorMsg);
#endif
}

/*******************************************************************************
* app_Terminate
*******************************************************************************/
void app_Terminate(int errorCode, char errorMsg[])
{
  app_Syslog(errorCode, errorMsg);
  raise(SIGTERM);
}

/*******************************************************************************
* app_SimpleHash
*******************************************************************************/
u32_t app_SimpleHash(u8_t *key, int len)
{
  u32_t   hash = 0;
  int     i;

  for (i=0; i < len; ++i)
  {
    hash += key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  return (hash & 0xffffffff);
}

/*******************************************************************************
* app_ElapsedTime
*******************************************************************************/
double app_ElapsedTime(int mode)
{
  static struct timespec  startTime, stopTime;
  double  startSec, startNsec, stopSec, stopNsec;
  double  dStart, dStop;
  
  if (mode != 1) 
  {
    clock_gettime(CLOCK_MONOTONIC, &startTime);
    return (0.0);
  }
  
  clock_gettime(CLOCK_MONOTONIC, &stopTime);
  startSec = (double) startTime.tv_sec;
  startNsec = (double) startTime.tv_nsec;

  stopSec = (double) stopTime.tv_sec;
  stopNsec = (double) stopTime.tv_nsec;
  
  dStart = startSec + startNsec/1000000000.0;
  dStop = stopSec + stopNsec/1000000000.0;
  return (dStop-dStart);
}

/*******************************************************************************
* app_MridtoString
*******************************************************************************/
void app_MridtoString256(u8_t *mrId, char *text)
{
  int   i;
  char  bytestr[8];
  char  line[256] = {0};

#if APP_IGNORE_LEADING_ZEROS
  for (i=0; i < SL_XSD_MRID_LEN; i++) 
  {
    if (mrId[i] != 0) break;
  }
#else  //APP_IGNORE_LEADING_ZEROS
  i = 0;
#endif //APP_IGNORE_LEADING_ZEROS 
  // convert each u8_t to string
  if (i < SL_XSD_MRID_LEN)
  {
    for (; i < SL_XSD_MRID_LEN; i++)
    {
      snprintf(bytestr, 8, "%02x", mrId[i]);
      strncat (line, bytestr, 8);
    }
  }
  else 
  {
    strcat (line, "00");
  }  
  strncpy(text, line, 256);
}

static const u8_t invalidTable[] = "~!@#$%^&*()_-";
static int invalidTableLength = sizeof(invalidTable)-1;

/*******************************************************************************
* isInvalidPemChar()
******************************************************************************/
/**
* \brief Checks to see if byte is an invalid PEM character. This function does
* \brief not check against every invalid character, only a subset.
* \param byte - byte to check against list of invalid characters.
* \return TRUE or FALSE.
*/
bool isInvalidPemChar(u8_t byte)
{
  int i;
  
  for (i=0; i < invalidTableLength; i++)
  {
    if (byte == invalidTable[i]) break;
  }
  if (i >= invalidTableLength) return false;
  else return true;
}

static const u8_t base64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  
  /*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */
/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
u8_t *base64_encode(u8_t *src, int len, int *out_len)
{
	u8_t  *out, *pos, *end, *in;
	int   olen, line_len;

	olen = len * 4 / 3 + 4;       // 3-byte blocks to 4-byte
	olen += olen / 72;            // line feeds
	olen++;                       // nul termination

	if (olen < len) return NULL;  // integer overflow

	if ((out=KAL_Malloc(olen)) == NULL) return NULL;

	end = src + len;
	in = src;
	pos = out;
	line_len = 0;

	while (end - in >= 3) 
  {
		*pos++ = base64_table[in[0] >> 2];
		*pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
		*pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
		*pos++ = base64_table[in[2] & 0x3f];
		in += 3;
		line_len += 4;
		if (line_len >= 72) 
    {
			*pos++ = '\n';
			line_len = 0;
		}
	}

	if (end - in) 
  {
		*pos++ = base64_table[in[0] >> 2];
		if (end - in == 1) 
    {
			*pos++ = base64_table[(in[0] & 0x03) << 4];
			*pos++ = '=';
		} 
    else 
    {
			*pos++ = base64_table[((in[0] & 0x03) << 4) |
					      (in[1] >> 4)];
			*pos++ = base64_table[(in[1] & 0x0f) << 2];
		}
		*pos++ = '=';
		line_len += 4;
	}

	if (line_len) *pos++ = '\n';

	*pos = '\0';
	if (out_len) *out_len = pos - out;
	return out;
}

/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */
/**
 * base64_decode - Base64 decode
 * @src: Data to be decoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
u8_t *base64_decode(u8_t *src, int len, int *out_len)
{
	u8_t  dtable[256], *out, *pos, block[4], tmp;
	int   i, count, olen;
	int   pad = 0;

	KAL_Memset(dtable, 0x80, 256);
	for (i = 0; i < sizeof(base64_table) - 1; i++) dtable[base64_table[i]] = (u8_t) i;
	dtable['='] = 0;

	count = 0;
	for (i = 0; i < len; i++) 
  {
		if (dtable[src[i]] != 0x80) count++;
	}

	if (count == 0 || count % 4) return NULL;

	olen = count / 4 * 3;
	pos = out = KAL_Malloc(olen);
	if (out == NULL) return NULL;

	count = 0;
	for (i = 0; i < len; i++) 
  {
		tmp = dtable[src[i]];
		if (tmp == 0x80) continue;

		if (src[i] == '=') pad++;
		block[count] = tmp;
		count++;

		if (count == 4) 
    {
			*pos++ = (block[0] << 2) | (block[1] >> 4);
			*pos++ = (block[1] << 4) | (block[2] >> 2);
			*pos++ = (block[2] << 6) | block[3];
			count = 0;
			if (pad) 
      {
				if (pad == 1) pos--;
				else if (pad == 2) pos -= 2;
				else  // Invalid padding
        {
					free(out);
					return NULL;
				}
				break;
			}
		}
	}
	*out_len = pos - out;
	return out;
}

/*******************************************************************************
* LoadSecurityInfoFromFile()
*******************************************************************************/
/**
* \brief Called after the SEP2 stack is started to load client security data
*   to SEP2 stack
* \param none
* \return none
*/
int LoadSecurityInfoFromFile(void)
{
  FILE    *certFile, *privFile, *rootFile;
  char    *extPtr;
  char    line[128];
  char    baseName[256], certName[256], privName[256];
  u8_t    *pemBuffer;
  int     i, pemLength, lineLength, reqLength;
  bool    validPEM;
  int     retval = -1;
  int     rootLength = 0;
  int     certLength = 0;
  int     privLength = 0;
  u8_t    *rootBuffer = NULL;
  u8_t    *certBuffer = NULL;
  u8_t    *privBuffer = NULL;
  sl_sec_store_root_ca_cert_req       *rootReq;
  sl_sec_store_cert_req               *certReq;
  sl_sec_store_private_key_req        *privReq;
  long    timeNow = (long)time(NULL);

  /*************************************************************************
  * This section searches for the Root CA Cert file
  *************************************************************************/
  // Check for root cert from command-line
  if (commandLineSecRootCAFilename != NULL)
  {
    rootFile = fopen(commandLineSecRootCAFilename, "r");
    if (rootFile != NULL) 
    {
      printf("[%ld] INFO: Root CA File: %s\n", timeNow, commandLineSecRootCAFilename);
      goto ROOT_CERT_FILE_OK;
    }
  }

  // Check for root cert from local location
  rootFile = fopen(APP_ROOT_CERT_FILE_LOCAL, "r");
  if (rootFile != NULL) 
  {
    printf("[%ld] INFO: Root CA File: %s\n", timeNow, APP_ROOT_CERT_FILE_LOCAL);
    goto ROOT_CERT_FILE_OK;
  }

  // Check for root cert from remote location
  rootFile = fopen(APP_ROOT_CERT_FILE_REMOTE, "r");
  if (rootFile != NULL) 
  {
    printf("[%ld] INFO: Root CA File: %s\n", timeNow, APP_ROOT_CERT_FILE_REMOTE);
    goto ROOT_CERT_FILE_OK;
  }

  // Failed to load Root CA Cert
  return retval;

ROOT_CERT_FILE_OK:
  /*************************************************************************
  * This section searches for the Private Key and Certificate Chain files
  *************************************************************************/
  // Check for security info from command-line
  if (commandLineSecInfoBasename != NULL)
  {
    strncpy(baseName, commandLineSecInfoBasename, 250);
    extPtr = strstr(baseName, ".");
    if (extPtr != NULL) *extPtr = '\0';
    snprintf(certName, 256, "%s.p7b", baseName);
    snprintf(privName, 256, "%s.pem", baseName);
    
    certFile = fopen(certName, "r");
    privFile = fopen(privName, "r");
    if ((certFile != NULL) && (privFile != NULL)) 
    {
      printf("[%ld] INFO: Device Sec Info: Certs= %s, PrivKey= %s\n", timeNow, certName, privName);
      goto CERT_KEY_FILES_OK;
    }
    
    if (certFile != NULL) fclose (certFile);
    if (privFile != NULL) fclose (privFile);
    certFile = NULL;
    privFile = NULL;
  }

  // Check for security info from local location
  strncpy(baseName, APP_DEV_SEC_INFO_FILE_LOCAL, 250);
  extPtr = strstr(baseName, ".");
  if (extPtr != NULL) *extPtr = '\0';
  snprintf(certName, 256, "%s.p7b", baseName);
  snprintf(privName, 256, "%s.pem", baseName);

  certFile = fopen(certName, "r");
  privFile = fopen(privName, "r");
  if ((certFile != NULL) && (privFile != NULL)) 
  {
    printf("[%ld] INFO: Device Sec Info: Certs= %s, PrivKey= %s\n", timeNow, certName, privName);
    goto CERT_KEY_FILES_OK;
  }
  
  if (certFile != NULL) fclose (certFile);
  if (privFile != NULL) fclose (privFile);
  certFile = NULL;
  privFile = NULL;

  // Check for security info from remote location
  strncpy(baseName, APP_DEV_SEC_INFO_FILENAME, 250);
  extPtr = strstr(baseName, ".");
  if (extPtr != NULL) *extPtr = '\0';
  snprintf(certName, 256, "%s.p7b", baseName);
  snprintf(privName, 256, "%s.pem", baseName);
  
  certFile = fopen(certName, "r");
  privFile = fopen(privName, "r");
  if ((certFile != NULL) && (privFile != NULL)) 
  {
    printf("[%ld] INFO: Device Sec Info: Certs= %s, PrivKey= %s\n", timeNow, certName, privName);
    goto CERT_KEY_FILES_OK;
  }
  
  if (certFile != NULL) fclose (certFile);
  if (privFile != NULL) fclose (privFile);
  certFile = NULL;
  privFile = NULL;

  // Failed to load Certificate Chain and Private Key
  return retval;
  
CERT_KEY_FILES_OK:
  /*************************************************************************
  * This section processes the Root CA Cert file
  *************************************************************************/
  // Allocate input PEM buffer
  fseek(rootFile, 0L, SEEK_END);
  errno = 0;
  long fsize = ftell(rootFile);
  if (fsize < 0)
  {
    printf("Encounter error on root CA, errno=%d; %s\n", errno, strerror(errno));
    exit(-errno);
  }
  pemBuffer = (u8_t *)KAL_Malloc(fsize);
  if (pemBuffer == NULL) goto STORE_SEC_EXIT;

  // Copy valid PEM lines from file to buffer
  pemLength = 0;
  validPEM = false;
  fseek(rootFile, 0L, SEEK_SET);
  while (fgets(line, 128, rootFile) != NULL)
  {
    // Skip lines until beginning of PEM block is detected
    if (validPEM == false)
    {
      if (strstr(line, "-----BEGIN") != NULL) validPEM = true;
      continue;
    }
    // Process lines until end of PEM block is detected
    if (validPEM == true)
    {
      if (strstr(line, "-----END") != NULL)
      {
        validPEM = false;
        continue;
      }
    }
    // Ignore PEM lines with bad character counts.
    lineLength = strnlen(line, 128);
    if (lineLength > APP_MAX_PEM_LINE) continue;

    // Ignore PEM lines with invalid characters.
    for (i=0; i < lineLength; i++)
    {
      if (isInvalidPemChar(line[i])) break;
    }
    if (i < lineLength) continue;

    KAL_Memcpy(pemBuffer+pemLength, line, lineLength);
    pemLength += lineLength;
  }
  if (pemLength < 0)
  {
    KAL_Free(pemBuffer);
    goto STORE_SEC_EXIT;
  }
  // Convert from certificate from PEM to DER
  rootBuffer = base64_decode(pemBuffer, pemLength, &rootLength);
  KAL_Free(pemBuffer);
  if (rootBuffer == NULL) goto STORE_SEC_EXIT;

  /*************************************************************************
  * This section processes the PKCS7 certificate chain file
  *************************************************************************/
  // Allocate input PEM buffer
  fseek(certFile, 0L, SEEK_END);
  errno = 0;
  fsize = ftell(certFile);
  if (fsize < 0)
  {
    printf("Encounter error on Certificate file, errno=%d; %s\n", errno, strerror(errno));
    exit(-errno);
  }

  pemBuffer = (u8_t *)KAL_Malloc(fsize);
  if (pemBuffer == NULL) goto STORE_SEC_EXIT;

  // Copy valid PEM lines from file to buffer
  pemLength = 0;
  validPEM = false;
  fseek(certFile, 0L, SEEK_SET);
  while (fgets(line, 128, certFile) != NULL)
  {
    // Skip lines until beginning of PEM block is detected
    if (validPEM == false)
    {
      if (strstr(line, "-----BEGIN") != NULL) validPEM = true;
      continue;
    }
    // Process lines until end of PEM block is detected
    if (validPEM == true)
    {
      if (strstr(line, "-----END") != NULL)
      {
        validPEM = false;
        continue;
      }
    }
    // Ignore PEM lines with bad character counts.
    lineLength = strnlen(line, 128);
    if (lineLength > APP_MAX_PEM_LINE) continue;

    // Ignore PEM lines with invalid characters.
    for (i=0; i < lineLength; i++)
    {
      if (isInvalidPemChar(line[i])) break;
    }
    if (i < lineLength) continue;

    KAL_Memcpy(pemBuffer+pemLength, line, lineLength);
    pemLength += lineLength;
  }
  if (pemLength < 0)
  {
    KAL_Free(pemBuffer);
    goto STORE_SEC_EXIT;
  }
  // Convert certificate chain from PEM to DER
  certBuffer = base64_decode(pemBuffer, pemLength, &certLength);
  KAL_Free(pemBuffer);
  if (certBuffer == NULL) goto STORE_SEC_EXIT;

  /*************************************************************************
  * This section processes the PKCS8 private key file
  *************************************************************************/
  // Allocate input PEM buffer
  fseek(privFile, 0L, SEEK_END);
  errno = 0;
  fsize = ftell(privFile);
  if (fsize < 0)
  {
    printf("Encounter error on private key file, errno=%d; %s\n", errno, strerror(errno));
    exit(-errno);
  }

  pemBuffer = (u8_t *)KAL_Malloc(fsize);
  if (pemBuffer == NULL) goto STORE_SEC_EXIT;

  // Copy valid PEM lines from file to buffer
  pemLength = 0;
  validPEM = false;
  fseek(privFile, 0L, SEEK_SET);
  while (fgets(line, 128, privFile) != NULL)
  {
    // Skip lines until beginning of PEM block is detected
    if (validPEM == false)
    {
      if (strstr(line, "-----BEGIN") != NULL) validPEM = true;
      continue;
    }
    // Process lines until end of PEM block is detected
    if (validPEM == true)
    {
      if (strstr(line, "-----END") != NULL)
      {
        validPEM = false;
        continue;
      }
    }
    // Ignore PEM lines with bad character counts.
    lineLength = strnlen(line, 128);
    if (lineLength > APP_MAX_PEM_LINE) continue;

    // Ignore PEM lines with invalid characters.
    for (i=0; i < lineLength; i++)
    {
      if (isInvalidPemChar(line[i])) break;
    }
    if (i < lineLength) continue;

    KAL_Memcpy(pemBuffer+pemLength, line, lineLength);
    pemLength += lineLength;
  }
  if (pemLength < 0)
  {
    KAL_Free(pemBuffer);
    goto STORE_SEC_EXIT;
  }
  // Convert private key from PEM to DER
  privBuffer = base64_decode(pemBuffer, pemLength, &privLength);
  KAL_Free(pemBuffer);
  if (privBuffer == NULL) goto STORE_SEC_EXIT;

  /*************************************************************************
  * This section creates the Store Root CA Cert request
  *************************************************************************/
  // Allocate memory for the root ca cert request
  reqLength = sizeof(sl_sec_store_root_ca_cert_req) + rootLength;
  rootReq = KAL_Malloc(reqLength);
  if (rootReq == NULL) goto STORE_SEC_EXIT;

  // Store the root certificate length
  rootReq->certLength = rootLength;
  // Copy the certificate 
  KAL_Memcpy(rootReq->certData, rootBuffer, rootLength);

  // Send mail
  app_SendMail(SL_SEC_STORE_ROOT_CA_CERT_REQ, rootReq, reqLength);
  KAL_Free(rootReq);

  /*************************************************************************
  * This section creates the Store Certificate Chain request
  *************************************************************************/
  // Allocate memory for the certificate chain request
  reqLength = sizeof(sl_sec_store_cert_req) + certLength;
  certReq = KAL_Malloc(reqLength);
  if (certReq == NULL) goto STORE_SEC_EXIT;

  // Store the certificate chain length
  certReq->chainFormat = SL_CMN_SECURITY_CERT_CHAIN_PKCS7;
  // Store the certificate chain length
  certReq->chainLength = certLength;
  // Copy the certificate chain 
  KAL_Memcpy(certReq->chainData, certBuffer, certLength);

  // Send mail
  app_SendMail(SL_SEC_STORE_CERT_REQ, certReq, reqLength);
  KAL_Free(certReq);

  /*************************************************************************
  * This section creates the Store Private Key request
  *************************************************************************/
  // Allocate memory for the private key request
  reqLength = sizeof(sl_sec_store_private_key_req) + privLength;
  privReq = KAL_Malloc(reqLength);
  if (privReq == NULL) goto STORE_SEC_EXIT;

  // Store the private key format
  privReq->privKeyFormat = SL_CMN_SECURITY_PRIVATE_KEY_PKCS8;
  // Store the private key length
  privReq->privKeyLength = privLength;
  // Copy the private key
  KAL_Memcpy(privReq->privKeyData, privBuffer, privLength);

  // Send mail
  app_SendMail(SL_SEC_STORE_PRIVATE_KEY_REQ, privReq, reqLength);
  KAL_Free(privReq);

  /*************************************************************************
  * Exit section
  *************************************************************************/
  // Successful exit
  retval = 0;

  // Clean up
STORE_SEC_EXIT:
  if (rootBuffer != NULL) KAL_Free(rootBuffer);
  if (certBuffer != NULL) KAL_Free(certBuffer);
  if (privBuffer != NULL) KAL_Free(privBuffer);
  if (rootFile != NULL) fclose(rootFile);
  if (certFile != NULL) fclose(certFile);
  if (privFile != NULL) fclose(privFile);
  return retval;
}


#define MAX_WL_ENTRIES    (16)
#define NUM_WL_ENTRIES    (9)

typedef struct {
    char value[20];                 // Hex value of LFDI
} HanLFDI;

// Pre-defined whitelist of clients
static HanLFDI clients[NUM_WL_ENTRIES] = 
{
  {0xd3, 0xd2, 0x0b, 0x5c, 0xef, 0x7e, 0x65, 0x31, 0x09, 0xb3, 0x24, 0xd1, 0x63, 0x52, 0x59, 0xb3, 0xa7, 0xa2, 0xda, 0x3e}, // Cert 2386
};


// White-List
HanLFDI WhiteList[MAX_WL_ENTRIES];

int ApiHanGetLFDIs(HanLFDI array[], u32_t *numArrayEntries)
{
  KAL_Memcpy(array, clients, NUM_WL_ENTRIES * sizeof(HanLFDI));
  *numArrayEntries = NUM_WL_ENTRIES;
  return 0;
}

/*******************************************************************************
* ascii2num()   // SPRK-1592
*******************************************************************************/
/**
* \brief Converts ACII hex-digit to number
* \param none
* \return none
*/
int ascii2num(char ch)
{
  if ((ch >= '0') && (ch <= '9')) return (ch - '0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch - 'A' + 10);
  if ((ch >= 'a') && (ch <= 'f')) return (ch - 'a' + 10);
  return 0;
}

/*******************************************************************************
* PopulateWhiteList()   // SPRK-1305
*******************************************************************************/
/**
* \brief Gets and populates the client whitelist from the Itron API
* \param none
* \return none
*/
void PopulateWhiteList(void)
{
  int   reqBaseLength, whiteLength, reqLength;
  u32_t numEntries = MAX_WL_ENTRIES;
  sl_sec_store_client_white_list_req  *req;
  u8_t  *byteptr;
  int   i;

  KAL_Memset(WhiteList, 0x0, sizeof(WhiteList));
  int result = ApiHanGetLFDIs(WhiteList, &numEntries);
  if (result != 0) return;

  reqBaseLength = sizeof(sl_sec_store_client_white_list_req);
  whiteLength = numEntries * sizeof(sl_sec_store_lfdi_whitelist_t);
  reqLength = reqBaseLength + whiteLength;

  req = KAL_Malloc(reqLength);
  req->numClients = numEntries;

  byteptr = (u8_t *) (req + reqBaseLength);

  for (i=0; i < numEntries; i++)
  {
    KAL_Memcpy(byteptr, &WhiteList[i], SL_XSD_LFDI_LEN);

#if DEBUG_WHITELIST
    int index = 0;
    for (j =0; j < SL_XSD_LFDI_LEN; ++j)
    {
      sprintf(str +index, "%02X ", byteptr[j]);
      index += 3;
    }
    str[index++] = '\n';
    str[index] = 0x0;
    LogString(str);
#endif

    byteptr += SL_XSD_LFDI_LEN;
  }

  app_SendMail(SL_SEC_STORE_SERVER_WHITE_LIST_REQ, req, reqLength);
  KAL_Free(req);
}


void SetPollRate()
{
  sl_der_client_set_event_poll_rate_req req2 = {0};
  req2.setPollRate = true;
  req2.pollRate = 20; //2*60; // 2 mins
  app_SendMail(SL_DER_CLIENT_SET_EVENT_POLL_RATE_REQ, &req2, sizeof (sl_der_client_set_event_poll_rate_req)); 
}
