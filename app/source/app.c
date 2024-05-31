/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application example code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
 $History: $
*/

#include <arpa/inet.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_api.h"
#include "app_config.h"
#include "app_logs.h" // USE_SEPARATE_APP_PROCESS
#include "app_tables.h"
#include "app_utils.h"
#include "fsal.h"
#include "kal_api.h"
#include "psal.h"
#include "sl_api.h"

#include "app_client.h"
#include "app_time_client.h"

#ifdef USE_SEPARATE_APP_PROCESS
#include "app_comm.h"
#else
#include "isal_api.h"
#endif /* matches #ifdef USE_SEPARATE_APP_PROCESS  */

#if defined(STDLIB_BUILD)
#include <getopt.h>
#include <signal.h>
#endif
#include <inttypes.h>
#include <errno.h>

/* EPRI defines and variables: Begin */
#define CLIENT_APP 1
#include "utils.h"
#include "usock_utils.h"
#include "usock_proto.h"
#include "usock_client.h"

u32_t app_loop_count;
int p_evse_test = 0;
/* EPRI defines and variables: End */


/*******************************************************************************
 * APP task data
 ******************************************************************************/
enum
{
    POWERON,
    INIT_COMPLETE,
    START_PENDING,
    START_COMPLETE
};

u8_t  APP_TaskHandle;
u8_t  appCurrentState      = POWERON;
//bool   appServerEnabled = false;
bool  appClientEnabled     = false;
char *p_appPathToConfigDir = NULL;
//char*  p_appConfigFile=NULL;

//#define DEVICE_CONFIG_FILE          "emc.conf"

// SPRK-370
//typedef struct
//{
//    bool  ipaddr_valid;
//    u8_t  ipaddr[SL_CMN_IP_ADDR_LEN];
//    bool  port_valid;
//    u16_t port;
//    bool  scheme_valid;
//    u8_t  scheme;
//    bool  dcap_path_valid;
//    char  dcap_path[256];
//    bool  pin_valid;
//    u32_t pin;
//    bool  control_type_valid;
//    int   control_type;
//    bool  server_lfdi_valid;
//    u8_t  server_lfdi[SL_XSD_LFDI_LEN];
//} server_info_t;

/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void ConfigDns(sl_dns_standard_t standard);
static void HandleMail(kal_mail_id_t mailId, void *mail);
static void HandleAppMail(kal_mail_id_t mailId, void *pMail);
static void HandleSlSecStoreCertCnf(sl_sec_store_cert_cnf *pCnf);
static void SendStartupReq(void);
static void SendSecGetSfdiReq(void);
static void APP_ActivateAppTask(void);
static void APP_Task(void *p_dummy);
#if defined(STDLIB_BUILD)
static void ParseCommandLineParameters(int argc, char *argv[]);
static void PrintUsage(const char *p_execName);
#endif
void ChangeDeviceType(sl_xsd_dev_cat_bitmap_t devicetype);
/*******************************************************************************
 * Local Data
 ******************************************************************************/
u8_t APP_UsockTaskHandle;
static u32_t  APP_UsockStack[APP_STACK_SIZE];
static u32_t APP_Stack[APP_STACK_SIZE];
static char *p_appInterfaceName = NULL;
static char *p_appPortNumString = NULL;

#ifndef USE_SEPARATE_APP_PROCESS
extern u16_t kituHttpPort;
extern u16_t kituHttpsPort;
#endif // USE_SEPARATE_APP_PROCESS

// Accept controls for both EV and EVSE
u32_t commandLineDeviceCategory = SL_XSD_DEVCAT_EVSE | SL_XSD_DEVCAT_ELECTRIC_VEHICLE;
u32_t commandLineDERType        = -1;

/*******************************************************************************
 * GetInterface()
 ******************************************************************************/
char *GetInterface(void)
{
    return p_appInterfaceName;
}

/*******************************************************************************
 * ConfigDns()
 ******************************************************************************/
/**
 * \brief Sends SL_DNS_SERVER_CONFIG_REQ or SL_DNS_CLIENT_CONFIG_REQ to configure
 *        DNS.
 * \param none
 * \return none
 */
static void ConfigDns(sl_dns_standard_t standard)
{
    sl_dns_client_config_req clientReq = {0};

    // Only the standard is set, all other values are left as default
    clientReq.standard       = standard;
    clientReq.standardExists = true;

    app_SendMail(SL_DNS_CLIENT_CONFIG_REQ, &clientReq, sizeof(clientReq));
}

/*******************************************************************************
 * HandleMail
 ******************************************************************************/
/**
 * \brief Processes received mail
 * \param mailId - Id of the mail message
 * \param pMail - Pointer to KAL Mail
 * \return none
 */
static void HandleMail(kal_mail_id_t mailId, void *pMail)
{
    switch (mailId)
    {
    case SL_STARTUP_CNF:
        APP_LOG_INFO("app_HandleMail: SL_STARTUP_CNF");
        appCurrentState = START_COMPLETE;

        // TODO: Check if this is necessary in ver 3.0
		ChangeDeviceType(17);

        // If G2H client was started, start the app client
        if (((sl_startup_cnf *)pMail)->clientStatus == SL_CMN_RC_SUCCESS)
        {
            appClientEnabled = true;
            app_ClientStart();
#ifdef APP_PRELOAD_DEMO
            // Start recurrent timer processing
            if (!appPassiveMode)
            {
                if (KAL_SetTimer(APP_CLIENT_DEMO_TIMER_MS, APP_TaskHandle, (u8_t)APP_CLIENT_DEMO_TIMEOUT_BIT, true) != G2_SUCCESS)
                {
                    APP_LOG_ERROR("KAL_SetTimer Error");
                }
                //fprintf(stdout, " >>>>>>>>      in handle mail timer trig    <<<<<<<<<<");
            }
#endif // APP_PRELOAD_DEMO
        }
        break;
    case SL_SEC_STORE_CERT_CNF:
        HandleSlSecStoreCertCnf((sl_sec_store_cert_cnf *)pMail);
        break;
    case SL_SERVER_CONFIG_DSP_CNF:
        APP_LOG_INFO("app_HandleMail: SL_SERVER_CONFIG_DSP_CNF");
        break;
    case SL_ZB_CONFIG_BORDER_ROUTER_CNF:
        APP_LOG_INFO("app_HandleMail: SL_ZB_CONFIG_BORDER_ROUTER_CNF");
        break;
    case SL_ZB_CONFIG_HOST_CNF:
        APP_LOG_INFO("app_HandleMail: SL_ZB_CONFIG_HOST_CNF");
        break;
    case SL_ZB_CONFIG_ROUTER_CNF:
        APP_LOG_INFO("app_HandleMail: SL_ZB_CONFIG_ROUTER_CNF");
        break;
    case SL_DNS_SERVER_CONFIG_CNF:
        APP_LOG_INFO("app_HandleMail: SL_DNS_SERVER_CONFIG_CNF");
        app_LogStatus(((sl_dns_server_config_cnf *)pMail)->status);
        break;
    case SL_DNS_CLIENT_CONFIG_CNF:
        APP_LOG_INFO("app_HandleMail: SL_DNS_CLIENT_CONFIG_CNF");
        app_LogStatus(((sl_dns_client_config_cnf *)pMail)->status);
        break;
    case SL_SEC_GET_SFDI_CNF:
        appSfdi = ((sl_sec_get_sfdi_cnf *)pMail)->sfdi;
        printf("INFO: MySFDI= %012" PRId64 "\n", appSfdi);
        break;
    }
}

/*******************************************************************************
 * HandleAppMail
 ******************************************************************************/
/**
 * \brief Processes mail received on the APP interface
 * \param mailId - Id of the mail message
 * \param pMail - Pointer to KAL Mail
 * \return none
 */
static void HandleAppMail(kal_mail_id_t mailId, void *pMail)
{
    (void)pMail;

    switch (mailId)
    {
    case APP_PRICE_CLIENT_GET_PRICING_REQ:
        APP_LOG_INFO("app_HandleMail: APP_PRICE_CLIENT_GET_PRICING_REQ");
#if APP_CONFIG_PRICE_CLIENT_ENABLED
        app_PriceClientHandleAppMail(mailId, pMail);
#endif
        break;

    case APP_PRICE_CLIENT_CANCEL_GET_PRICING_REQ:
        APP_LOG_INFO("app_HandleMail: APP_PRICE_CLIENT_CANCEL_GET_PRICING_REQ");
#if APP_CONFIG_PRICE_CLIENT_ENABLED
        app_PriceClientHandleAppMail(mailId, pMail);
#endif
        break;

    case APP_DRLC_CLIENT_GET_EDC_REQ:
        APP_LOG_INFO("app_HandleMail: APP_DRLC_CLIENT_GET_EDC_REQ");
#if APP_CONFIG_DRLC_CLIENT_ENABLED
        app_DrlcClientHandleAppMail(mailId, pMail);
#endif
        break;

    case APP_DRLC_CLIENT_CANCEL_GET_EDC_REQ:
        APP_LOG_INFO("app_HandleMail: APP_DRLC_CLIENT_CANCEL_GET_EDC_REQ");
#if APP_CONFIG_DRLC_CLIENT_ENABLED
        app_DrlcClientHandleAppMail(mailId, pMail);
#endif
        break;

    case APP_TIME_SERVER_ADJUST_TIME_REQ:
        APP_LOG_INFO("app_HandleMail: APP_TIME_SERVER_ADJUST_TIME_REQ");
#if APP_CONFIG_TIME_SERVER_ENABLED
        app_TimeServerHandleAppMail(mailId, pMail);
#endif
        break;
    }
}

/*******************************************************************************
 * HandleSlSecStoreCertCnf()
 ******************************************************************************/
/**
 * \brief Called when SL_SEC_STORE_CERT_CNF is received. Frees memory
 *        used for the SL_SEC_STORE_CERT_REQ.
 * \param none
 * \return none
 */
static void HandleSlSecStoreCertCnf(sl_sec_store_cert_cnf *pCnf)
{
    // Print some status info
    if (pCnf->status == SL_CMN_RC_SUCCESS)
    {
        app_UtilsSetMySfdi(pCnf->sfdi);
        app_UtilsSetMyLfdi(pCnf->lfdi);

        APP_LOG_INFO("APP Client: Security Information successfully loaded.");

        printf("INFO: SFDI= %012" PRId64 "\n", app_UtilsGetMySfdi());
        printf("INFO: LFDI= ");
		//KAL_Memcpy(appLfdi, pCnf->lfdi, SL_XSD_LFDI_LEN);
        for (uint i = 0; i < sizeof(pCnf->lfdi); i++)
        {
            printf("%02x", pCnf->lfdi[i]);
        }
        printf("\n");

    }    
    else
    {
        APP_LOG_ERROR("APP Client: Failed to load security information.");
        //KAL_Memset(appLfdi, 0, SL_XSD_LFDI_LEN);
    }
}


// /*******************************************************************************
//  * LoadPEMECSecurityInfoFromFile()
//  *******************************************************************************/
// /**
//  * \brief Called after the SEP2 stack is started to load client security data
//  *   to SEP2 stack
//  * \param none
//  * \return none
//  */
// int LoadPEMECSecurityInfoFromFile(void)
// {
//     FILE                          *certFile, *privFile, *rootFile;
//     char                           line[128];
//     u8_t                          *pemBuffer, *b64Ptr;
//     size_t                         i, b64Length, pemLength, lineLength, reqLength;
//     bool                           validPEM;
//     int                            retval     = -1;
//     size_t                         rootLength = 0;
//     size_t                         certLength = 0;
//     size_t                         privLength = 0;
//     u8_t                          *rootBuffer = NULL;
//     u8_t                          *certBuffer = NULL;
//     u8_t                          *privBuffer = NULL;
//     sl_sec_store_root_ca_cert_req *rootReq;
//     sl_sec_store_cert_req         *certReq;
//     sl_sec_store_private_key_req  *privReq;
//     i64_t                          timeNow = (i64_t)time(NULL);

//     /*************************************************************************
//      * This section opens the PEM-EC security files from command-line options.
//      *************************************************************************/
//     if ((commandLineSecRootCAFilename == NULL) || (commandLinePEMChainFile == NULL) || (commandLineBasicECPrivKeyFile == NULL))
//     {
//         return retval;
//     }

//     rootFile = fopen(commandLineSecRootCAFilename, "r");
//     certFile = fopen(commandLinePEMChainFile, "r");
//     privFile = fopen(commandLineBasicECPrivKeyFile, "r");

//     if ((rootFile == NULL) || (certFile == NULL) || (privFile == NULL))
//     {
//         if (rootFile != NULL) fclose(rootFile);
//         if (certFile != NULL) fclose(certFile);
//         if (privFile != NULL) fclose(privFile);
//         return retval;
//     }

//     printf("[%" PRId64 "] INFO: Root CA File: %s\n", timeNow, commandLineSecRootCAFilename);
//     printf("[%" PRId64 "] INFO: PEM Cert Chain File: %s\n", timeNow, commandLinePEMChainFile);
//     printf("[%" PRId64 "] INFO: Basic EC Private Key File: %s\n", timeNow, commandLineBasicECPrivKeyFile);

//     /*************************************************************************
//      * This section processes the Root CA Cert file
//      *************************************************************************/
//     // Allocate input PEM buffer
//     fseek(rootFile, 0L, SEEK_END);
//     errno      = 0;
//     long fsize = ftell(rootFile);
//     if (fsize < 0)
//     {
//         printf("Encounter error on root CA, errno=%d; %s\n", errno, strerror(errno));
//         exit(-errno);
//     }

//     pemBuffer = (u8_t *)KAL_Malloc(fsize);
//     if (pemBuffer == NULL) goto PEMEC_SEC_EXIT;

//     // Copy valid PEM lines from file to buffer
//     pemLength = 0;
//     validPEM  = false;
//     fseek(rootFile, 0L, SEEK_SET);
//     while (fgets(line, 128, rootFile) != NULL)
//     {
//         // Skip lines until beginning of PEM block is detected
//         if (validPEM == false)
//         {
//             if (strstr(line, "-----BEGIN") != NULL) validPEM = true;
//             continue;
//         }
//         // Process lines until end of PEM block is detected
//         if (validPEM == true)
//         {
//             if (strstr(line, "-----END") != NULL)
//             {
//                 validPEM = false;
//                 continue;
//             }
//         }
//         // Ignore PEM lines with bad character counts.
//         lineLength = strnlen(line, 128);
//         if (lineLength > APP_MAX_PEM_LINE) continue;

//         // Ignore PEM lines with invalid characters.
//         for (i = 0; i < lineLength; i++)
//         {
//             if (isInvalidPemChar(line[i])) break;
//         }
//         if (i < lineLength) continue;

//         KAL_Memcpy(pemBuffer + pemLength, line, lineLength);
//         pemLength += lineLength;
//     }
//     // Convert from certificate from PEM to DER
//     rootBuffer = base64_decode(pemBuffer, pemLength, &rootLength);
//     KAL_Free(pemBuffer);
//     if (rootBuffer == NULL) goto PEMEC_SEC_EXIT;

//     /*************************************************************************
//      * This section processes the PEM certificate chain file
//      *************************************************************************/
//     // Allocate the DER certBuffer
//     certBuffer = (u8_t *)KAL_Malloc(APP_MAX_CERT_CHAIN_LENGTH);
//     if (certBuffer == NULL) goto PEMEC_SEC_EXIT;

//     // Allocate input PEM buffer
//     fseek(certFile, 0L, SEEK_END);
//     errno = 0;
//     fsize = ftell(certFile);
//     if (fsize < 0)
//     {
//         printf("Encounter error on PEM certificate chain file, errno=%d; %s\n", errno, strerror(errno));
//         exit(-errno);
//     }

//     pemBuffer = (u8_t *)KAL_Malloc(fsize);
//     if (pemBuffer == NULL) goto PEMEC_SEC_EXIT;

//     // Copy valid PEM lines from file to buffer
//     certLength = 0;
//     pemLength  = 0;
//     validPEM   = false;
//     fseek(certFile, 0L, SEEK_SET);

//     while (fgets(line, 128, certFile) != NULL)
//     {
//         // Skip lines until beginning of PEM block is detected
//         if (validPEM == false)
//         {
//             if (strstr(line, "-----BEGIN") != NULL)
//             {
//                 validPEM  = true;
//                 pemLength = 0;
//             }
//             continue;
//         }
//         // Process lines until end of PEM block is detected
//         if (validPEM == true)
//         {
//             if (strstr(line, "-----END") != NULL)
//             {
//                 validPEM = false;
//                 // decode this certificate
//                 b64Ptr = base64_decode(pemBuffer, pemLength, &b64Length);
//                 if (b64Ptr != NULL)
//                 {
//                     // copy this DER certificate to the chain buffer
//                     KAL_Memcpy(certBuffer + certLength, b64Ptr, b64Length);
//                     certLength += b64Length;
//                     KAL_Free(b64Ptr);
//                 }
//                 continue;
//             }
//         }
//         // Ignore PEM lines with bad character counts.
//         lineLength = strnlen(line, 128);
//         if (lineLength > APP_MAX_PEM_LINE) continue;

//         // Ignore PEM lines with invalid characters.
//         for (i = 0; i < lineLength; i++)
//         {
//             if (isInvalidPemChar(line[i])) break;
//         }
//         if (i < lineLength) continue;

//         KAL_Memcpy(pemBuffer + pemLength, line, lineLength);
//         pemLength += lineLength;
//     }
//     KAL_Free(pemBuffer);

//     /*************************************************************************
//      * This section processes the basic EC private key file
//      *************************************************************************/
//     // Allocate input PEM buffer
//     fseek(privFile, 0L, SEEK_END);
//     errno = 0;
//     fsize = ftell(privFile);
//     if (fsize < 0)
//     {
//         printf("Encounter error on EC private key, errno=%d; %s\n", errno, strerror(errno));
//         exit(-errno);
//     }

//     pemBuffer = (u8_t *)KAL_Malloc(fsize);
//     if (pemBuffer == NULL) goto PEMEC_SEC_EXIT;

//     // Copy valid PEM lines from file to buffer
//     pemLength = 0;
//     validPEM  = false;
//     fseek(privFile, 0L, SEEK_SET);
//     while (fgets(line, 128, privFile) != NULL)
//     {
//         // Skip lines until beginning of PEM block is detected
//         if (validPEM == false)
//         {
//             if (strstr(line, "-----BEGIN") != NULL) validPEM = true;
//             continue;
//         }
//         // Process lines until end of PEM block is detected
//         if (validPEM == true)
//         {
//             if (strstr(line, "-----END") != NULL)
//             {
//                 validPEM = false;
//                 continue;
//             }
//         }
//         // Ignore PEM lines with bad character counts.
//         lineLength = strnlen(line, 128);
//         if (lineLength > APP_MAX_PEM_LINE) continue;

//         // Ignore PEM lines with invalid characters.
//         for (i = 0; i < lineLength; i++)
//         {
//             if (isInvalidPemChar(line[i])) break;
//         }
//         if (i < lineLength) continue;

//         KAL_Memcpy(pemBuffer + pemLength, line, lineLength);
//         pemLength += lineLength;
//     }
//     // Convert private key from PEM to DER
//     privBuffer = base64_decode(pemBuffer, pemLength, &privLength);
//     KAL_Free(pemBuffer);
//     if (privBuffer == NULL) goto PEMEC_SEC_EXIT;

//     /*************************************************************************
//      * This section creates the Store Root CA Cert request
//      *************************************************************************/
//     // Allocate memory for the root ca cert request
//     reqLength = sizeof(sl_sec_store_root_ca_cert_req) + rootLength;
//     rootReq   = KAL_Malloc(reqLength);
//     if (rootReq == NULL) goto PEMEC_SEC_EXIT;

//     // Store the root certificate length
//     rootReq->certLength = rootLength;
//     // Copy the certificate
//     KAL_Memcpy(rootReq->certData, rootBuffer, rootLength);

//     // Send mail
//     app_SendMail(SL_SEC_STORE_ROOT_CA_CERT_REQ, rootReq, reqLength);
//     KAL_Free(rootReq);

//     /*************************************************************************
//      * This section creates the Store Certificate Chain request
//      *************************************************************************/
//     // Allocate memory for the certificate chain request
//     reqLength = sizeof(sl_sec_store_cert_req) + certLength;
//     certReq   = KAL_Malloc(reqLength);
//     if (certReq == NULL) goto PEMEC_SEC_EXIT;

//     // Store the certificate chain length
//     certReq->chainFormat = SL_CMN_SECURITY_CERT_CHAIN_PEM;
//     // Store the certificate chain length
//     certReq->chainLength = certLength;
//     // Copy the certificate chain
//     KAL_Memcpy(certReq->chainData, certBuffer, certLength);

//     // Send mail
//     app_SendMail(SL_SEC_STORE_CERT_REQ, certReq, reqLength);
//     KAL_Free(certReq);

//     /*************************************************************************
//      * This section creates the Store Private Key request
//      *************************************************************************/
//     // Allocate memory for the private key request
//     reqLength = sizeof(sl_sec_store_private_key_req) + privLength;
//     privReq   = KAL_Malloc(reqLength);
//     if (privReq == NULL) goto PEMEC_SEC_EXIT;

//     // Store the private key format
//     privReq->privKeyFormat = SL_CMN_SECURITY_PRIVATE_KEY_BASIC_EC;

//     // Store the private key length
//     privReq->privKeyLength = privLength;
//     // Copy the private key
//     KAL_Memcpy(privReq->privKeyData, privBuffer, privLength);

//     // Send mail
//     app_SendMail(SL_SEC_STORE_PRIVATE_KEY_REQ, privReq, reqLength);
//     KAL_Free(privReq);

//     /*************************************************************************
//      * Exit section
//      *************************************************************************/
//     // Successful exit
//     retval = 0;

//     // Clean up
// PEMEC_SEC_EXIT:
//     if (rootBuffer != NULL) KAL_Free(rootBuffer);
//     if (certBuffer != NULL) KAL_Free(certBuffer);
//     if (privBuffer != NULL) KAL_Free(privBuffer);
//     if (rootFile != NULL) fclose(rootFile);
//     if (certFile != NULL) fclose(certFile);
//     if (privFile != NULL) fclose(privFile);
//     return retval;
// }


// /*******************************************************************************
//  * string2lfdi()
//  ******************************************************************************/
// /**
//  * \brief Converts an LFDI string to a u8_t array. (SPRK-370)
//  * \param dest      - destination u8_t array of size SL_XSD_LFDI_LEN
//  * \param src       - char pointer to LFDI string
//  * \return 0 = SUCCESS, -1 = FAIL
//  */
// static int string2lfdi(u8_t dest[], char *src)
// {
//     int   i, length, byteNum;
//     char *cptr;
//     u8_t  highNibble, lowNibble;
//     bool  nextCharIsLowNibble;

//     // see if this is a valid LFDI string
//     length = strlen(src);
//     if ((length > 2 * SL_XSD_LFDI_LEN) || (length < 0)) return -1;
//     for (i = 0; i < length; i++)
//     {
//         if (isxdigit(src[i]) == 0) return -1; // error, not a hex character
//         src[i] = (char)tolower(src[i]);       // convert to lower case for easier processing
//     }

//     // looks like a good LFDI, now converter from char to u8_t
//     KAL_Memset(dest, 0, SL_XSD_LFDI_LEN);

//     byteNum             = SL_XSD_LFDI_LEN - 1; // start destination with the last LFDI byte
//     cptr                = src + length - 1;    // start source with the last input character
//     nextCharIsLowNibble = true;                // last input source character is the low nibble of a byte

//     for (i = 0; i < length; i++)
//     {
//         if (nextCharIsLowNibble) // get low nibble
//         {
//             // convert and save low nibble of byte
//             lowNibble = (*cptr > '9') ? (*cptr - 'a' + 10) : (*cptr - '0');
//         }
//         else // get high nibble
//         {
//             // convert high nibble, save byte to destination
//             highNibble      = (*cptr > '9') ? (*cptr - 'a' + 10) : (*cptr - '0');
//             dest[byteNum--] = (highNibble << 4) + lowNibble;
//         }
//         nextCharIsLowNibble = !nextCharIsLowNibble; // toggle
//         cptr--;
//     }
//     // First character may be just a low nibble. If so, save this byte to the destination.
//     if (nextCharIsLowNibble == false) dest[byteNum] = lowNibble;

//     return 0;
// }

/*******************************************************************************
 * ChangeDeviceType
 ******************************************************************************/
/**
 * \brief This function will change the DER DeviceCategory.
 * \param devicetype - device category bitmap; can be find in sl_api.h.
 * \return none
 */
void ChangeDeviceType(sl_xsd_dev_cat_bitmap_t devicetype)
{
    sl_der_client_set_device_type_req req = {0};

    req.transactionId = 10001;
    req.devicetype    = devicetype;

    app_SendMail(SL_DER_CLIENT_SET_DEVICE_TYPE_REQ, &req, sizeof(sl_der_client_set_device_type_req));
}


// /*******************************************************************************
//  * ParseDeviceConfigFile()
//  ******************************************************************************/
// /**
//  * \brief Parses non-JSON device config file for SEP2 Servera and other info.
//  * \param fp1       - file pointer to configuration file
//  * \param config    - pointer to server info object
//  * \return <none>
//  */
// static void ParseDeviceConfigFile(FILE *fp1, server_info_t *config)
// {
//     char line[256], *token;
//     char seps[] = " \t\r\n";

//     fseek(fp1, 0, SEEK_SET);
//     while (fgets(line, sizeof(line), fp1) != NULL)
//     {
//         token = strtok(line, seps);
//         if (token == NULL) continue;

//         // Try to parse whether to listen to DRLC or DER
//         if (strcasecmp(token, "control_type") == 0)
//         {
//             config->control_type_valid = false;
//             token                      = strtok(NULL, seps);
//             if (token == NULL) continue;
//             config->control_type       = atoi(token);
//             config->control_type_valid = true;
//             continue;
//         }

//         // Try to parse the Server address
//         if (strcasecmp(token, "server_addr") == 0)
//         {
//             config->ipaddr_valid = false;
//             KAL_Memset(config->ipaddr, 0, SL_CMN_IP_ADDR_LEN);
//             if ((token = strtok(NULL, seps)) == NULL) continue;
//             if (inet_pton(AF_INET, token, config->ipaddr) == 1) config->ipaddr_valid = true;
//             continue;
//         }

//         // Try to parse the Server port
//         if (strcasecmp(token, "server_port") == 0)
//         {
//             config->port_valid = false;
//             token              = strtok(NULL, seps);
//             if (token == NULL) continue;
//             config->port       = (u16_t)atoi(token);
//             config->port_valid = true;
//             continue;
//         }

//         // Try to parse the Server scheme
//         if (strcasecmp(token, "server_scheme") == 0)
//         {
//             config->scheme_valid = false;
//             token                = strtok(NULL, seps);
//             if (token == NULL) continue;
//             if (strcasecmp(token, "http") == 0)
//             {
//                 config->scheme       = SL_CMN_SCHEME_HTTP;
//                 config->scheme_valid = true;
//             }
//             else if (strcasecmp(token, "https") == 0)
//             {
//                 config->scheme       = SL_CMN_SCHEME_HTTPS;
//                 config->scheme_valid = true;
//             }
//             continue;
//         }

//         // Try to parse the Server dcap path
//         if (strcasecmp(token, "server_dcap") == 0)
//         {
//             config->dcap_path_valid = false;
//             token                   = strtok(NULL, seps);
//             if (token == NULL) continue;
//             KAL_Memset(config->dcap_path, 0, 256);
//             strncpy(config->dcap_path, token, 255);
//             config->dcap_path_valid = true;
//             continue;
//         }

//         // Try to parse the Server pIN
//         if (strcasecmp(token, "server_pin") == 0)
//         {
//             config->pin_valid = false;
//             token             = strtok(NULL, seps);
//             if (token == NULL) continue;
//             config->pin       = (u32_t)atol(token);
//             config->pin_valid = true;
//             continue;
//         }

//         // Try to parse the Server LFDI
//         if (strcasecmp(token, "server_lfdi") == 0)
//         {
//             config->server_lfdi_valid = false;
//             token                     = strtok(NULL, seps);
//             if (token == NULL) continue;

//             if (string2lfdi(config->server_lfdi, token) == 0) config->server_lfdi_valid = true;
//             continue;
//         }
//     }
// }


/*******************************************************************************
 * SendStartupReq
 ******************************************************************************/
/**
 * \brief Sends messages to configure WPAN and then startup the G2H stack. Used
 *        to start the G2H automatically at power on.
 * \param none
 * \return none
 */
static void SendStartupReq(void)
{
#ifdef APP_AUTO_START

    app_SendMail(SL_STARTUP_REQ, NULL, 0);

    SendSecGetSfdiReq();

#endif // APP_AUTO_START
}

/*******************************************************************************
 * SendSecGetSfdiReq()
 ******************************************************************************/
/**
 * \brief Sends an SL_SEC_GET_SFDI_REQ to the Services Layer.  This should only
 *        be called *after* the SL_STARTUP_REQ.
 * \param none
 * \return none
 */
static void SendSecGetSfdiReq(void)
{
    app_SendMail(SL_SEC_GET_SFDI_REQ, NULL, 0);
}

static void usock_task( void* p_dummy ) {
    kal_event_t            eventMask;
    // printf("Usock client start task: init\n");

    eventMask = KAL_TaskEventWait(KAL_START_EVENT, 0);
    fprintf(stdout,"Usock client task started\n");

    /* EPRI Code: Begin*/
    /*  Initialization code */
    evcc_client_version();
    read_parameters_from_file("evcc_settings.txt");
    app_loop_count = 0;
    usock_init();
    register_process_messages_callback(process_recv_message);
    /* EPRI Code: End*/

    //printf("start task: init completed\n");
    while (1) {
        eventMask = KAL_TaskEventWait( KAL_ALL_EVENTS, 1000 );
        usock_message_handler();
        app_loop_count++;
    }
}


/*******************************************************************************
 * APP_ActivateAppTask()
 ******************************************************************************/
/**
 * \brief Called at initialization to create App task
 * \param none
 * \return none
 */
static void APP_ActivateAppTask(void)
{
    KAL_Memset(APP_Stack, 10, sizeof(APP_Stack));
    APP_TaskHandle = KAL_TaskCreate(APP_Task, NULL, "App", APP_Stack, APP_STACK_SIZE, APP_TASK_PRIORITY);

    if (p_evse_test != 1)
    {
        KAL_Memset( APP_UsockStack, 10, sizeof(APP_UsockStack) );
        APP_UsockTaskHandle = KAL_TaskCreate(usock_task, NULL, "UsockServerTask", 
                                         APP_UsockStack, APP_STACK_SIZE, APP_TASK_PRIORITY - 2);
    }

}

/*******************************************************************************
 * APP_Task()
 ******************************************************************************/
/**
 * \brief APP_Task event handler loop
 * \param p_dummy is a dummy pointer for receiving (unused) startup parameters
 * \return none
 */
static void APP_Task(void *p_dummy)
{
    (void)p_dummy;

    kal_message_details_t messDetails;
    kal_mail_details_t    mailDetails;
    kal_event_t           eventMask;

    // Register as a KAL Synchronous Mail Initiator.
    KAL_MailSyncRegister(KAL_MAILSYNC_TASKTYPE_INITIATOR);

    // Wait for system startup event.
    KAL_TaskEventWait(KAL_START_EVENT, 0);
    APP_LOG_INFO("Kitu Application Started");

    // SPRK-1305: Get and populate the client LFDI white list from the Itron API
    PopulateWhiteList();

    // SPRK-1305: Load certificate information from Itron API before sending sl_startup_req.
    LoadSecurityInfoFromFile();

    // Configure DNS
    ConfigDns(APP_DNS_STANDARD);

    // Perform initialization.
    SendStartupReq();
    appCurrentState = START_PENDING;

    /* Enter infinite loop.  */
    while (1)
    {
        // Get event mask
        eventMask = KAL_TaskEventWait(KAL_ALL_EVENTS, 0);

        // App Preload Demo timer processing
        if (eventMask & APP_CLIENT_DEMO_TIMEOUT_MASK)
        {
            if ((appClientEnabled == true) && (!appPassiveMode)) app_ClientProcessing();

            /* EPRI: beg*/
            if ( (opt_test_charge == 1 ) || (opt_test_discharge == 1) )
            {
                app_task_sec_count ++;
                if (app_task_sec_count % DDERC_PERIODIC_SECS == 0 )
                {
                    send_test_default_derc(true);
                }
                if ( (app_task_sec_count > INIT_SECS_BEFORE_EVENT) && (opt_event_status == -1) )
                {
                    opt_event_status = 0;
                    // need to send dersettings to can2sep2
                    send_test_dersettings(); 
                    // schedule event i.e.  rxed event
                    displayKALTime();
                    send_test_derc_event(opt_event_status);
                }
                else if ( 
                        (app_task_sec_count > ( opt_start_time_from_now_secs + INIT_SECS_BEFORE_EVENT) ) && 
                        (opt_event_status == 0)
                )
                {
                    // START EVENT
                    opt_event_status = 1;
                    displayKALTime();
                    send_test_derc_event(opt_event_status);
                }
                else if ( 
                        (app_task_sec_count > ( opt_duration_secs + opt_start_time_from_now_secs + INIT_SECS_BEFORE_EVENT) ) && 
                        (opt_event_status == 1)
                )
                {
                    // event completed 
                    opt_event_status = 2;
                    displayKALTime();
                    send_test_derc_event(opt_event_status);
                }
            }
            process_message_app_task();
            /* EPRI: end*/
        }

        // Kick startup
        if (eventMask & APP_STARTUP_MASK)
        {
            app_SendMail(SL_STARTUP_REQ, NULL, 0);
            SendSecGetSfdiReq();
        }

        // Handle messages
        if (eventMask & KAL_MESSAGE_EVENT)
        {
            if (KAL_MailReceive(&messDetails, &mailDetails) == G2_SUCCESS)
            {
                if (mailDetails.iPartnerTask == SL_TaskHandle)
                {
                    APP_HandleMail(mailDetails.MailId, mailDetails.pParameterBlock);
                    // Continue calling local function for general message handling
                    HandleMail(mailDetails.MailId, mailDetails.pParameterBlock);
                }
                else if (mailDetails.iPartnerTask == DM_TaskHandle || mailDetails.iPartnerTask == APP_TaskHandle)
                {
                    HandleAppMail(mailDetails.MailId, mailDetails.pParameterBlock);
                }
                else
                {
                    APP_LOG_ERROR("APP_Task: Received mail from unknown source.");
                }
            }
            else
            {
                APP_LOG_ERROR("KAL_MailReceive Error");
            }
        }
    }
}

int convert2int(char * value_str, int low_limit, int high_limit)
{
  long tmpl;
  int tmpi;
  errno = 0;
  tmpl = strtol(value_str, NULL, 10);
  if (errno)
  {
    perror("strtol func: ");
    return -1;

  }
  if ( (tmpl < low_limit) || (tmpl > high_limit) )
  {
    fprintf(stdout, "xxxx Invalid value %d, expected range %d to %d\n",
      (int)tmpl, low_limit, high_limit);
    return -1;
  }

  tmpi = (int) tmpl;
  return tmpi;

}


#ifdef STDLIB_BUILD
/**************************************************************************************************
 *                                                                                                *
 *  ParseCommandLineParameters() - this private routine is used to parse the command-line         *
 *                                 parameters for this application.  See the description for      *
 *                                 PrintUsage() for details on the supported parameters.          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      int    argc   - the number of command-line parameters.                                    *
 *      char*  argv[] - an array of strings containing command-line parameters.                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      p_appPathToConfigDir                                                                      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void ParseCommandLineParameters(int argc, char *argv[])
{
    static struct option longOpts[] = {
        {"secfile", required_argument, NULL, 's'},
#if OVERRIDE_ISAL_LINUX_USE_ANY_IP // Don't need to specify the network interface
        {"interface", required_argument, NULL, 'i'},
#endif
        {"rootfile", required_argument, NULL, 'r'},
        {"http", required_argument, NULL, 't'},
        {"https", required_argument, NULL, 'T'},
        {"g2hterm", required_argument, NULL, 'g'},
        {"configpath", required_argument, NULL, 'p'},
        {"deviceCategory", required_argument, NULL, 'Y'},
        {"DERType", required_argument, NULL, 'D'},
        {"help", no_argument, NULL, 'h'},
        {"evsetest",   no_argument,       NULL, 'x'},
        {"charge",     required_argument, NULL, 'a'},
        {"discharge",  required_argument, NULL, 'b'},
        {"start",     required_argument, NULL, 'e'},
        {"duration",  required_argument, NULL, 'f'},
        {"simulateVehicle", no_argument, NULL, 'z'},
        {NULL, 0, NULL, 0}
    };
    int           opt;
    unsigned long ltemp;
    int           length;

    opt = getopt_long(argc, argv, "s:r:i:t:T:g:p:Y:D:x:a:b:e:f:hz", longOpts, NULL);
    while (opt != -1)
    {
        //fprintf(stdout, "####### >>>> opt processing = %c\n", opt);
        switch (opt)
        {
        case 's':
            commandLineSecInfoBasename = optarg;
            fprintf(stdout, "CMDL: secInfoBasename = %s\n", commandLineSecInfoBasename);
            break;
        case 'r':
            commandLineSecRootCAFilename = optarg;
            fprintf(stdout, "CMDL: rootCAFilename = %s\n", commandLineSecRootCAFilename);
            break;
        case 'c':
            commandLinePEMChainFile = optarg;
            fprintf(stdout, "CMDL: pemCertChainFile = %s\n", commandLinePEMChainFile);
            break;
        case 'd':
            commandLineBasicECPrivKeyFile = optarg;
            fprintf(stdout, "CMDL: basicECPrivKeyFile = %s\n", commandLineBasicECPrivKeyFile);
            break;
        case 'i':
            p_appInterfaceName = optarg;
            break;

#ifndef USE_SEPARATE_APP_PROCESS
        case 't': // HTTP port
            kituHttpPort = atoi(optarg);
            break;

        case 'T': // HTTPS port
            kituHttpsPort = atoi(optarg);
            break;
#endif
        case 'g':
            p_appPortNumString = optarg;
            break;
        case 'p':
            p_appPathToConfigDir = optarg;

            // Need to make sure path does not end with '/'.
            length = strlen(p_appPathToConfigDir) - 1;
            while ((length >= 0) && (p_appPathToConfigDir[length] == '/'))
            {
                p_appPathToConfigDir[length--] = '\0';
            }
            break;
        case 'Y':
            if (sscanf(optarg, "%lx", &ltemp) == 1)
            {
                commandLineDeviceCategory = (u32_t)ltemp;
                fprintf(stdout, "CMDL: deviceCategory = 0x%x\n", commandLineDeviceCategory);
            }
            break;
        case 'D':
            if (sscanf(optarg, "%ld", &ltemp) == 1)
            {
                commandLineDERType = (u32_t)ltemp;
                fprintf(stdout, "CMDL: DERType = %d\n", commandLineDERType);
            }
            break;
        case 'x':
            p_evse_test = 1;   
            break;         
        case 'a':
            opt_test_charge = 1;
            opt_charge_percent = convert2int(optarg, 0, 100);
            fprintf(stdout, "CMDL: charge opt set, pc set to %d \n", opt_charge_percent);
            break;
        case 'b':
            opt_test_discharge = 1;
            opt_discharge_percent = convert2int(optarg, -100, 0);
            fprintf(stdout, "CMDL: discharge opt set, pc set to %d \n", opt_discharge_percent);
            break;
        case 'f':
            opt_duration = 1;
            opt_duration_secs = 60*convert2int(optarg, 1, 60*6); // duration provided in mins
            fprintf(stdout, "CMDL: duration set, value (secs) set to %d \n", opt_duration_secs);
            break;
        case 'e':
            opt_start_time = 1;
            opt_start_time_from_now_secs = 60* convert2int(optarg, 0, 60*6); // start time from now in mins
            fprintf(stdout, "CMDL: start time set, value(secs) set to %d \n", opt_start_time_from_now_secs);
            break;
        case 'z':
            opt_simulate_vehicle = 1;
            fprintf(stdout, "CMDL: vehicle data is simulated %d \n", opt_simulate_vehicle);
            break;
        case 'h':
            PrintUsage(argv[0]);
            exit(EXIT_SUCCESS);
            break;
        default:
            PrintUsage(argv[0]);
            exit(EXIT_FAILURE);
            break;
        }

        opt = getopt_long(argc, argv, "s:r:i:t:T:g:p:Y:D:x:a:b:e:f:hz", longOpts, NULL);
    }
    if ( (opt_test_charge == 1) && (opt_test_discharge == 1) )
    {
        fprintf(stdout, "xxxx ===>>>> either charge or discharge option needs to be provided.\n");
        fprintf(stdout, "xxxx ===>>>> Both options cannot be provided at the same time. Exit app.\n");
        PrintUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
    // debug
    fprintf(stdout, "CMDL: Check options: charge: %d, discharge %d\n", opt_test_charge, opt_test_discharge);
    fprintf(stdout, "CMDL: Check options: starttime: %d, duration %d\n", opt_start_time, opt_duration);
    if ( (opt_test_charge == 1) || (opt_test_discharge == 1) )
    {
        if (!( (opt_start_time == 1) && (opt_duration == 1) ))
        {
        fprintf(stdout, "xxxx ===>>>> charge/discharge options require  \n");
        fprintf(stdout, "xxxx ===>>>> -e <(>start time mins> and -f <(>duration mins> options. Exit app. \n");
        PrintUsage(argv[0]);
        exit(EXIT_FAILURE);
        }
    }

}

/**************************************************************************************************
 *                                                                                                *
 *  PrintUsage() - this private routine is used to display the command-line usage for this        *
 *                 application.  There are 5 optional parameters, each of which has both          *
 *                 short-form and long-form tags:                                                 *
 *                                                                                                *
 *                     1) -s <security file>                                                      *
 *                        --secfile=<security file>                                               *
 *                                                                                                *
 *                        Used to specify the pathname of a security file (e.g.,                  *
 *                        devcertinfo.bin).                                                       *
 *                                                                                                *
 *                     2) -i <interface>                                                          *
 *                        --interface=<interface>                                                 *
 *                                                                                                *
 *                        Used to specify the interface for xmDNS and HTTP(S) for a SEP           *
 *                        (IEEE 2030.5) communication.                                            *
 *                                                                                                *
 *                     3) -p <path to config>                                                     *
 *                        --configpath=<path to config>                                           *
 *                                                                                                *
 *                        Used to specify the path to the configuration files needed by the       *
 *                        application.  This should be a directory.                               *
 *                                                                                                *
 *                     4) -g <port number>                                                        *
 *                        --g2hterm=<port number>                                                 *
 *                                                                                                *
 *                        The port number with which to connect G2H_Terminal.                     *
 *                                                                                                *
 *                     5) -h                                                                      *
 *                        --help                                                                  *
 *                                                                                                *
 *                        Displays the command-line usage of the application, then terminates.    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void PrintUsage(const char *p_execName)
{ // -z simulate vehicle no option
#if OVERRIDE_ISAL_LINUX_USE_ANY_IP
    fprintf(stderr, "Usage: %s {-s <security file>} {-r <rootfile>} {-c <pemCertChainFile>} {-d <basicECPrivKeyFile>}\n", p_execName);
    fprintf(stderr, "          {-i <interface>} {-p <path to config>} {-g <port number>}\n");
    fprintf(stderr, "          {-t <HTTP port>} {-T <HTTPS port>}\n");
    fprintf(stderr, "          {-Y <deviceCategory>} {-D <DERType>} {-z} \n");
    fprintf(stderr, "   or: %s {--secfile=<security file>} {--rootfile=<x>} {--pemCertChainFile=<x>} {--basicECPrivKeyFile=<x>}\n", p_execName);
    fprintf(stderr, "          {--configpath=<path to config>} {--g2hterm=<port number>}\n");
    fprintf(stderr, "          {--http=<HTTP port>} {--https=<HTTPS port>} {--evsetest}\n");
    fprintf(stderr, "          {--deviceCategory=<deviceCategory>} {--DERType=<DERType>}\n");
    fprintf(stderr, "          {-a charge_percent (req -f and -e opts)} {-b discharge percent (req -f -e opts)} {-f duration mins} {-e start in mins from now} \n");
    fprintf(stderr, "\n   where:\n");
    fprintf(stderr, "        <secfile> - basename of the pem cert chain and pem private key file (e.g., /opt/kitu/devcertinfo)\n");
    fprintf(stderr, "        <rootfile> - filename of the pem Root CA certificate file (e.g., /opt/kitu/rootcacert.pem)\n");
    fprintf(stderr, "        <pemCertChainFile> - PEM certificate chain file\n");
    fprintf(stderr, "        <basicECPrivKeyFile> - Basic EC private key file\n");
    fprintf(stderr, "        <path to config> - path to the directory containing configuration files\n");
    fprintf(stderr, "        <port number>    - the port number with which to connect G2H_Terminal\n");
    fprintf(stderr, "        <HTTP port> - HTTP port assigned to embedded server, default is 80\n");
    fprintf(stderr, "        <HTTPs port> - HTTPs port assigned to embedded server, default is 443\n");
    fprintf(stderr, "        <deviceCategory> - Hex bitmask of the device category (sl_xsd_dev_cat_bitmap_t) defaults to macro: DEVICE_CATEGORY\n");
    fprintf(stderr, "        <DERType> - Integer value for the <type> field in DERCap (sl_xsd_der_type_t) default: 0xFFFFFFFF\n\n");
#else  // OVERRIDE_ISAL_LINUX_USE_ANY_IP
    fprintf(stderr, "Usage: %s {-s <security file>} {-r <rootfile>} {-c <pemCertChainFile>} {-d <basicECPrivKeyFile>}\n", p_execName);
    fprintf(stderr, "          {-i <interface>} {-p <path to config>} {-g <port number>}\n");
    fprintf(stderr, "          {-t <HTTP port>} {-T <HTTPS port>} {-e}\n");
    fprintf(stderr, "          {-Y <deviceCategory>} {-D <DERType>} {-z} \n"); 
    fprintf(stderr, "          {-a charge_percent (req -f and -e opts)} {-b discharge percent (req -f -e opts)} {-f duration mins} {-e start in mins from now} \n");
    fprintf(stderr, "   or: %s {--secfile=<security file>} {--rootfile=<x>} {--pemCertChainFile=<x>} {--basicECPrivKeyFile=<x>}\n", p_execName);
    fprintf(stderr, "          {--interface=-<interface>} {--configpath=<path to config>} {--g2hterm=<port number>}\n");
    fprintf(stderr, "          {--http=<HTTP port>} {--https=<HTTPS port>} {--evsetest}\n");
    fprintf(stderr, "          {--deviceCategory=<deviceCategory>} {--DERType=<DERType>}\n");
    fprintf(stderr, "\n   where:\n");
    fprintf(stderr, "        <secfile> - basename of the pem cert chain and pem private key file (e.g., /opt/kitu/devcertinfo)\n");
    fprintf(stderr, "        <rootfile> - filename of the pem Root CA certificate file (e.g., /opt/kitu/rootcacert.pem)\n");
    fprintf(stderr, "        <pemCertChainFile> - PEM certificate chain file\n");
    fprintf(stderr, "        <basicECPrivKeyFile> - Basic EC private key file\n");
    fprintf(stderr, "        <interface>      - the interface name for SEP communications (e.g., wlan0)\n");
    fprintf(stderr, "        <path to config> - path to the directory containing configuration files\n");
    fprintf(stderr, "        <port number>    - the port number with which to connect G2H_Terminal\n");
    fprintf(stderr, "        <HTTP port> - HTTP port assigned to embedded server, default is 80\n");
    fprintf(stderr, "        <HTTPs port> - HTTPs port assigned to embedded server, default is 443\n");
    fprintf(stderr, "        <deviceCategory> - Hex bitmask of the device category (sl_xsd_dev_cat_bitmap_t) defaults to macro: DEVICE_CATEGORY\n");
    fprintf(stderr, "        <DERType> - Integer value for the <type> field in DERCap (sl_xsd_der_type_t) default: 0xFFFFFFFF\n\n");
#endif // OVERRIDE_ISAL_LINUX_USE_ANY_IP
}
#endif

/*******************************************************************************
 * main()
 ******************************************************************************/
/**
 * \brief Entry to G2H Stack
 */
int main(int argc, char *argv[])
{
#ifdef STDLIB_BUILD
    ParseCommandLineParameters(argc, argv);
    printf("\r\n\r\n");
    printf("********************************************************************\r\n");
    printf("*                                                                  *\r\n");
    printf("*                    Kitu Systems Demo Software                    *\r\n");
    printf("*                                                                  *\r\n");
    printf("* Copyright (c) 2016 Kitu Systems, Inc. All rights reserved.       *\r\n");
    printf("*                                                                  *\r\n");
    printf("* THIS SOFTWARE IS PROVIDED 'AS-IS' AND KITU SYSTEMS, INC.         *\r\n");
    printf("* DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING THE      *\r\n");
    printf("* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, *\r\n");
    printf("* AND NON-INFRINGEMENT, WITH RESPECT TO THE SOFTWARE.              *\r\n");
    printf("*                                                                  *\r\n");
    printf("********************************************************************\r\n");
    printf("\r\n");
#endif

#if defined(STDLIB_BUILD)
    /* Suppress generation of SIGPIPE signals.  */
    signal(SIGPIPE, SIG_IGN);
#endif

    // Init Platform
    PSAL_Init();
#ifndef USE_SEPARATE_APP_PROCESS
    FSAL_Init();
#endif // USE_SEPARATE_APP_PROCESS
    KAL_Init();

    // Init DM
    if (DM_Init(p_appPortNumString) != DM_RC_SUCCESS)
    {
        return -1;
    }

    // Startup Tasks
#ifdef USE_SEPARATE_APP_PROCESS
    APP_CommInit();
#else  /* matches #ifdef USE_SEPARATE_APP_PROCESS  */
    SL_ActivateSLTask();
#endif /* matches #else of #ifdef USE_SEPARATE_APP_PROCESS  */

#ifndef USE_SEPARATE_APP_PROCESS
    // Init IP Stack
    ISAL_Init(p_appInterfaceName);
#endif /* matches #ifndef USE_SEPARATE_APP_PROCESS  */

    DM_ActivateDMTask();
    APP_ActivateAppTask();

    // Start the system
    KAL_Start(); //  Does not return

    return (0);
}
