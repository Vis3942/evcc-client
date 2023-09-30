/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_meter_mirror_client.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application Meter Mirror Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#include "sl_api.h"
#include <string.h>
#include <stdio.h>
#include "kal_api.h"
#include "app_api.h"
#include "app_config.h"
#include "app_meter_mirror_client.h"
#include "app_utils.h"
#include "app_client.h"
#include "utils.h"

#if APP_CONFIG_METER_MIRROR_CLIENT_ENABLED

/*******************************************************************************
 * Private Constants
 ******************************************************************************/
#define DEFAULT_MMR_NUM_MR_GROUPS (SL_METER_MIRROR_MAX_MR_GROUPS)
#define DEFAULT_MMR_NUM_MMRS (4)

#define DEFAULT_MUP_DESCRIPTION "Stellantis PEV MUP"
#define DEFAULT_MUP_ROLE_FLAGS (SL_XSD_ROLE_FLAG_PEV_MASK)
#define DEFAULT_MUP_SERVICE_KIND (SL_XSD_SERVICE_ELECTRICITY)
#define DEFAULT_MUP_STATUS (SL_XSD_STATUS_ON)
#define DEFAULT_MMR_ENERGY_DESCRIPTION "Stellantis PEV Energy"

#define DEFAULT_ACTIVEPOWER_DESCRIPTION "Active Power"
#define DEFAULT_REACTIVEPOWER_DESCRIPTION "Reactive Power"
#define DEFAULT_VOLTAGE_DESCRIPTION "Voltage"
#define DEFAULT_FREQUENCY_DESCRIPTION "Frequency"

/*******************************************************************************
 * Private Data
 ******************************************************************************/

typedef struct
{
  u8_t mrId[SL_XSD_MRID_LEN]; // Note that the MMR mRID is hard coded with IANA_PENA, and an MR identifier
  bool exists;
  bool pendingReq;
  i64_t pendingValue;
  bool pendingCnf;
  u32_t pendingTransId;
} mmr_info_t;

struct
{
  bool serverFound;
  sl_meter_mirror_client_server_ind serverInd;
  u32_t pendingTransId;
  u8_t mupMrId[SL_XSD_MRID_LEN];    // Note that the MUP mRID is hard coded with IANA_PENA, and an MUP identifier
  u8_t group0MrId[SL_XSD_MRID_LEN]; // Note that the MUP mRID is hard coded with IANA_PENA, and an MUP identifier
  bool mupExists;
  mmr_info_t mmrInfo[DEFAULT_MMR_NUM_MMRS];
  bool charging;          // Boolean indicating whether or not charging is in progress.
  u32_t chargeDuration;   // Number of seconds the EV has been charging.
  i32_t timeToNextCharge; // Number of seconds until next charging trigger.
} appMeterMirrorClient = {0};

static sl_cmn_rt_t defaultRtype = {
    SL_XSD_AB_INSTANTANEOUS,      // sl_xsd_acc_behavior_type_t accumulationBehaviour;
    {false, 0},                   // sl_xsd_unit_val_t          calorificValue;
    {false, 0},                   // sl_xsd_unit_val_t          conversionFactor;
    SL_XSD_COMMODITY_ELEC_SECOND, // sl_xsd_commodity_type_t    commodity;
    SL_XSD_DATA_QUAL_NOT_APPL,    // sl_xsd_data_qual_type_t    dataQualifier;
    SL_XSD_FLOW_DIR_FORWARD,      // sl_xsd_flow_dir_type_t     flowDirection;
    0,                            // u32_t                      intervalLength;
    false,                        // bool                       intervalLengthExists;
    SL_XSD_KIND_DEMAND,           // sl_xsd_kind_type_t         kindType;
    0,                            // u8_t                       maxNumIntervals;
    false,                        // bool                       maxNumIntervalsExists;
    0,                            // u8_t                       numConsumBlocks;
    false,                        // bool                       numConsumBlocksExists;
    0,                            // u8_t                       numTouTiers;
    false,                        // bool                       numTouTiersExists;
    SL_XSD_PHASE_CODE_NOT_APPL,   // sl_xsd_phase_code_type_t   phaseCode;
    SL_XSD_PTM_NONE,              // sl_xsd_ptm_type_t          powerOfTenMultiplier;
    0,                            // u32_t                      subIntervalLength;
    false,                        // bool                       subIntervalLengthExists;
    0,                            // u64_t                      supplyLimit;
    false,                        // bool                       supplyLimitExists;
    0,                            // bool                       tieredConsumptionBlocks;
    false,                        // bool                       tieredConsumptionBlocksExists;
    SL_XSD_UOM_WATT,              // sl_xsd_uom_t               uomType;
};

static sl_reading_t defaultReading = {
    SL_XSD_CONS_BLOCK_TYPE_DEFAULT,   // sl_xsd_cons_block_type_t      consumptionBlock;
    false,                            // bool                          consumptionBlockExists;
    0,                                // u32_t                         duration;
    0,                                // u16_t                         localId;
    false,                            // bool                          localIdExists;
    SL_XSD_READ_QUAL_FLAG_VALID_MASK, // sl_xsd_read_qual_flags_type_t qualFlags;
    false,                            // bool                          qualFlagsExists;
    0,                                // sl_xsd_time_type_t            start;
    0,                                // sl_xsd_tou_type_t             touTier;
    false,                            // bool                          touTierExists;
    0,                                // i64_t                         value;
    true,                             // bool                          valueExists;
};

typedef enum
{
  MMR_TYPE_ACTIVEPOWER = 0,
  MMR_TYPE_REACTIVEPOWER = 1,
  MMR_TYPE_VOLTAGE = 2,
  MMR_TYPE_FREQUENCY = 3
} mmr_type_index_e;

char actpwr_buf[1500];
int actpwr_buf_len = 0;
char reactpwr_buf[1500];
int reactpwr_buf_len = 0;
char volt_buf[1500];
int volt_buf_len = 0;
char freq_buf[1500];
int freq_buf_len = 0;

/*******************************************************************************
 * Local Functions
 ******************************************************************************/
static void HandleMeterMirrorServerInd(sl_meter_mirror_client_server_ind *pInd);

/*******************************************************************************
 * StoreMrIds
 ******************************************************************************/
/**
 * \brief This client uses hard-coded mRIDs for the MUP and instantaneous MMR.
 *        This function generates those mRIDs.
 *
 *        NOTE: Currently the hard coded value includes the IANA PENA and an
 *        identifier for MUP and MMR. It should also include a device specific
 *        value to differentiate it from other client devices from the same manufacturer.
 * \return None
 */
static void StoreMrIds(void)
{
  u8_t x;
  //u64_t sfdi = appSfdi;

  // Set mRIDs to zero
  KAL_Memset(&appMeterMirrorClient.mupMrId, 0, SL_XSD_MRID_LEN);
  for (x = 0; x < DEFAULT_MMR_NUM_MR_GROUPS; x++)
    KAL_Memset(&appMeterMirrorClient.mmrInfo[x].mrId, 0, SL_XSD_MRID_LEN);

  // mRIDs are hard coded and include device manufacturer ID (IANA PENA)
  // NOTE: some device specific information (e.g., SFDI or MAC address). In this case,
  // LFDI is used.

  // not used
  // appMeterMirrorClient.mupMrId[0] = 'm';
  // appMeterMirrorClient.mupMrId[1] = 'u';
  // appMeterMirrorClient.mupMrId[2] = 'p';

  KAL_Memcpy(appMeterMirrorClient.mupMrId, appLfdi, 8);
  KAL_Memset(appMeterMirrorClient.mupMrId + 8, 0, 4);

  // appMeterMirrorClient.mupMrId[4] = (u8_t)(appSfdi >> 32);
  // appMeterMirrorClient.mupMrId[5] = (u8_t)(appSfdi >> 24);
  // appMeterMirrorClient.mupMrId[6] = (u8_t)(appSfdi >> 16);
  // appMeterMirrorClient.mupMrId[7] = (u8_t)(appSfdi >> 8);
  // appMeterMirrorClient.mupMrId[8] = (u8_t)(appSfdi);

  appMeterMirrorClient.mupMrId[12] = (u8_t)(STELLANTIS_IANA_PEN >> 24);
  appMeterMirrorClient.mupMrId[13] = (u8_t)(STELLANTIS_IANA_PEN >> 16);
  appMeterMirrorClient.mupMrId[14] = (u8_t)(STELLANTIS_IANA_PEN >> 8);
  appMeterMirrorClient.mupMrId[15] = (u8_t)(STELLANTIS_IANA_PEN);

// GROUP0mRID Not used so this code is just there
  appMeterMirrorClient.group0MrId[0] = 'g';
  appMeterMirrorClient.group0MrId[1] = 'p';
  appMeterMirrorClient.group0MrId[2] = '0';

  appMeterMirrorClient.group0MrId[4] = (u8_t)(appSfdi >> 32);
  appMeterMirrorClient.group0MrId[5] = (u8_t)(appSfdi >> 24);
  appMeterMirrorClient.group0MrId[6] = (u8_t)(appSfdi >> 16);
  appMeterMirrorClient.group0MrId[7] = (u8_t)(appSfdi >> 8);
  appMeterMirrorClient.group0MrId[8] = (u8_t)(appSfdi);

  appMeterMirrorClient.group0MrId[12] = (u8_t)(STELLANTIS_IANA_PEN >> 24);
  appMeterMirrorClient.group0MrId[13] = (u8_t)(STELLANTIS_IANA_PEN >> 16);
  appMeterMirrorClient.group0MrId[14] = (u8_t)(STELLANTIS_IANA_PEN >> 8);
  appMeterMirrorClient.group0MrId[15] = (u8_t)(STELLANTIS_IANA_PEN);

  for (x = 0; x < DEFAULT_MMR_NUM_MMRS; x++)
  {
    // appMeterMirrorClient.mmrInfo[x].mrId[0] = 'm';
    // appMeterMirrorClient.mmrInfo[x].mrId[1] = 'm';
    // appMeterMirrorClient.mmrInfo[x].mrId[2] = 'r';
    // appMeterMirrorClient.mmrInfo[x].mrId[3] = x;

    // appMeterMirrorClient.mmrInfo[x].mrId[4] = (u8_t)(appSfdi >> 32);
    // appMeterMirrorClient.mmrInfo[x].mrId[5] = (u8_t)(appSfdi >> 24);
    // appMeterMirrorClient.mmrInfo[x].mrId[6] = (u8_t)(appSfdi >> 16);
    // appMeterMirrorClient.mmrInfo[x].mrId[7] = (u8_t)(appSfdi >> 8);
    // appMeterMirrorClient.mmrInfo[x].mrId[8] = (u8_t)(appSfdi);

    // appMeterMirrorClient.mmrInfo[x].mrId[12] = (u8_t)(STELLANTIS_IANA_PEN >> 24);
    // appMeterMirrorClient.mmrInfo[x].mrId[13] = (u8_t)(STELLANTIS_IANA_PEN >> 16);
    // appMeterMirrorClient.mmrInfo[x].mrId[14] = (u8_t)(STELLANTIS_IANA_PEN >> 8);
    // appMeterMirrorClient.mmrInfo[x].mrId[15] = (u8_t)(STELLANTIS_IANA_PEN);

    KAL_Memcpy(appMeterMirrorClient.mmrInfo[x].mrId, appMeterMirrorClient.mupMrId, SL_XSD_MRID_LEN);
    appMeterMirrorClient.mmrInfo[x].mrId[10] = (u8_t) (x );  // equivalent of index in sample code
    appMeterMirrorClient.mmrInfo[x].mrId[11] = (u8_t) (x + 1);
  }
}

/*******************************************************************************
 * HandleMeterMirrorServerInd
 ******************************************************************************/
/**
 * \brief Handles the SL_METER_MIRROR_CLIENT_SERVER_IND. At this point the
 *        example starts a timer to begin processing. The next step is to wait
 *        for a MUP Indication.
 * \return None
 */
static void HandleMeterMirrorServerInd(sl_meter_mirror_client_server_ind *pInd)
{
  if ((pInd->status == SL_CMN_SERVER_STATUS_LOST) ||
      (pInd->numSources == 0))
    return; // Cannot use this server

  // Store server information
  appMeterMirrorClient.serverInd = *pInd;
  appMeterMirrorClient.serverFound = true;

  APP_LOG_INFO("HandleMeterMirrorServerInd: Server found!");

  // Store mRID information.
  StoreMrIds();
}

/*******************************************************************************
/*******************************************************************************
 * SendMupReq
 ******************************************************************************/
/**
 * \brief Creates the Mirror Usage Point (MUP) on the server for this device.
 *        Per Section 12.10.3, this post SHALL contain at least the mRID of
 *        the MUP, the mRID of the MirrorMeterReading (MMR), and the Reading Type.
 *        The post MAY also contain MirrorReadingSets and Readings (in this
 *        implementation, they are NOT included).
 * \return None
 */
static void SendMupReq(void)
{
  sl_meter_mirror_client_send_mup_req *pReq;
  u16_t length;
  u8_t descLen;

  length = sizeof(sl_meter_mirror_client_send_mup_req) + DEFAULT_MMR_NUM_MR_GROUPS * sizeof(sl_mr_group_t);
  pReq = KAL_Malloc(length);
  KAL_Memset(pReq, 0, length);

  // Set server information
  pReq->serverSourceInfo.serverHandle = appMeterMirrorClient.serverInd.serverHandle;
  pReq->serverSourceInfo.source.source = appMeterMirrorClient.serverInd.sourceList[0].source;

  // Copy over MUP data
  descLen = strlen(DEFAULT_MUP_DESCRIPTION);
  if (descLen >= SL_XSD_DESCRIPTION_LEN)
  {
    descLen = SL_XSD_DESCRIPTION_LEN - 1;
  }
  strncpy(pReq->description, DEFAULT_MUP_DESCRIPTION, descLen);

  KAL_Memcpy(pReq->mrId, &appMeterMirrorClient.mupMrId, SL_XSD_MRID_LEN);

  pReq->roleFlags = DEFAULT_MUP_ROLE_FLAGS;
  pReq->serviceCategory = DEFAULT_MUP_SERVICE_KIND;
  pReq->status = DEFAULT_MUP_STATUS;

  // Copy over minimal MMR data required for MUP creation
  pReq->numMeterReadingGroups = DEFAULT_MMR_NUM_MR_GROUPS;

  descLen = strlen(DEFAULT_MMR_ENERGY_DESCRIPTION);
  if (descLen >= SL_XSD_DESCRIPTION_LEN)
    descLen = SL_XSD_DESCRIPTION_LEN - 1;
  strncpy(pReq->mrGroup[0].description, DEFAULT_MMR_ENERGY_DESCRIPTION, descLen);

  KAL_Memcpy(pReq->mrGroup[0].mrId, appMeterMirrorClient.mmrInfo[MMR_TYPE_ACTIVEPOWER].mrId, SL_XSD_MRID_LEN);

  // Readings and Reading sets are not included in the MUP
  pReq->mrGroup[0].numReadingSets = 0;
  pReq->mrGroup[0].readingExists = false;

  // Reading Type is included with the MUP
  pReq->mrGroup[0].rtypeExists = true;

  // Set proper rType fields.
  defaultRtype.uomType = SL_XSD_UOM_WATT_HOUR;
  defaultRtype.accumulationBehaviour = SL_XSD_AB_SUMMATION;
  defaultRtype.kindType = SL_XSD_KIND_ENERGY;

  KAL_Memcpy(&pReq->mrGroup[0].rtype, &defaultRtype, sizeof(sl_cmn_rt_t));

  pReq->transactionId = app_ClientGetTransactionID();
  appMeterMirrorClient.pendingTransId = pReq->transactionId;

  app_SendMail(SL_METER_MIRROR_CLIENT_SEND_MUP_REQ, pReq, length);
}

void EVCCSetupMupReq(sl_meter_mirror_client_send_mup_req *pReq)
{
  sl_meter_mirror_client_server_ind *serverInd;

  if (appMeterMirrorClient.serverFound != true)
  {
    // server not found; not creating MUP
    fprintf(stdout, "xxxx --------  MUP server not found cannot create MUP\n");
    return;
  }

  serverInd = &appMeterMirrorClient.serverInd;
  //  TODO: FILL UP MRID OF mup and mrGroup[0], and LFDI IN SEP2 APP
  u16_t length = sizeof(sl_meter_mirror_client_send_mup_req) + DEFAULT_MMR_NUM_MR_GROUPS * sizeof(sl_mr_group_t);

  KAL_Memcpy(pReq->mrId, &appMeterMirrorClient.mupMrId, SL_XSD_MRID_LEN);

  KAL_Memcpy(pReq->mrGroup[0].mrId, appMeterMirrorClient.group0MrId, SL_XSD_MRID_LEN);

  pReq->transactionId = app_ClientGetTransactionID();
  appMeterMirrorClient.pendingTransId = pReq->transactionId;

  // Set server information    
  pReq->serverSourceInfo.serverHandle = serverInd->serverHandle;
  pReq->serverSourceInfo.source.source = serverInd->sourceList[0].source;

  // debug
  fprintf(stdout, "num read gps %d \n", pReq->numMeterReadingGroups);
  fprintf(stdout, "read type exists %d\n", pReq->mrGroup[0].readingExists);
  // KAL_Memcpy(pReq->mrId, AppMupMrId, SL_XSD_MRID_LEN);
  //snprintf(pReq->description, SL_XSD_DESCRIPTION_LEN, "Client %d [%012" PRIu64 "]", commandLineClientId, app_UtilsGetMySfdi());    pReq->roleFlags       = (SL_XSD_ROLE_FLAG_MIRROR_MASK | SL_XSD_ROLE_FLAG_SUB_MASK);    pReq->serviceCategory = SL_XSD_SERVICE_ELECTRICITY;    pReq->status          = SL_XSD_STATUS_ON;    // Create the MRID for first MMR    pReq->numMeterReadingGroups = 1;    KAL_Memcpy(pReq->mrGroup[0].mrId, AppMupMrId, SL_XSD_MRID_LEN);    pReq->mrGroup[0].mrId[8]  = (u8_t)(mmrReading->code >> 8);    pReq->mrGroup[0].mrId[9]  = (u8_t)(mmrReading->code);    pReq->mrGroup[0].mrId[10] = (u8_t)(mmrReading->index);    pReq->mrGroup[0].rtypeExists = true;    if (mmrEntry->dq != SL_XSD_DATA_QUAL_NOT_APPL) pReq->mrGroup[0].rtype.dataQualifier = mmrEntry->dq;    pReq->mrGroup[0].rtype.phaseCode            = mmrEntry->phase; // SPRK-1647    pReq->mrGroup[0].rtype.powerOfTenMultiplier = mmrEntry->ptm;    pReq->mrGroup[0].rtype.uomType              = mmrEntry->uom;has context menu
  app_SendMail(SL_METER_MIRROR_CLIENT_SEND_MUP_REQ, pReq, length);
}

void SendPendingMMRs(int x_skip)
{
  int ii;
  // Check if there is another MMR update pending and if so send the MMR Request.
  //sl_meter_mirror_client_server_ind *serverInd;
  if (appMeterMirrorClient.serverFound != true)
  {
    // server not found; not creating MUP
    fprintf(stdout, "xxxx --------  MUP server not found cannot create MMr in SendPending MMRs\n");
    return;
  }
  //serverInd = &appMeterMirrorClient.serverInd;


  for (ii = 0; ii < DEFAULT_MMR_NUM_MMRS; ii++)
  {
    if (ii != x_skip)
    {
      if (appMeterMirrorClient.mmrInfo[ii].pendingCnf)
      {
        appMeterMirrorClient.mmrInfo[ii].pendingCnf = false;
        break;
      }
    }
  }
  if (ii < DEFAULT_MMR_NUM_MMRS)
  {
    sl_meter_mirror_client_send_mmr_req *pReq;
    u16_t length;
    length = sizeof(sl_meter_mirror_client_send_mmr_req) + DEFAULT_MMR_NUM_MR_GROUPS * sizeof(sl_mr_group_t);
    pReq = KAL_Malloc(length);
    KAL_Memset(pReq, 0, length);

    displayTime();
    switch (ii)
    {
    case MMR_TYPE_ACTIVEPOWER:
      fprintf(stdout, "Send Pending Active Power MMR \n");
      KAL_Memcpy(pReq, actpwr_buf, actpwr_buf_len);
      break;
    case MMR_TYPE_REACTIVEPOWER:
      fprintf(stdout, "Send Pending Reactive Power MMR \n");
      KAL_Memcpy(pReq, reactpwr_buf, reactpwr_buf_len);
      break;
    case MMR_TYPE_VOLTAGE:
      fprintf(stdout, "Send Pending Voltage MMR \n");
      KAL_Memcpy(pReq, volt_buf, volt_buf_len);
      break;
    case MMR_TYPE_FREQUENCY:
      fprintf(stdout, "Send Pending Frequency MMR \n");
      KAL_Memcpy(pReq, freq_buf, freq_buf_len);
      break;
    }
    pReq->transactionId = app_ClientGetTransactionID();
    appMeterMirrorClient.mmrInfo[ii].pendingTransId = pReq->transactionId;
    appMeterMirrorClient.mmrInfo[ii].pendingCnf = true;

    // // Set server information    
    // pReq->serverSourceInfo.serverHandle = serverInd->serverHandle;
    // pReq->serverSourceInfo.source.source = serverInd->sourceList[0].source;

    app_SendMail(SL_METER_MIRROR_CLIENT_SEND_MMR_REQ, pReq, length);

    if (pReq != NULL)
    {
      free(pReq);
    }
  }
}

void EVCCMmrReq(sl_meter_mirror_client_send_mmr_req *pReq)
{
  //  TODO: FILL UP MRID OF mrGroup[0], and mrid of mmr variable
  u8_t descLen_act_pwr;
  u8_t descLen_react_pwr;
  u8_t descLen_volt;
  u8_t descLen_freq;
  u8_t mmr_type_idx;

  //sl_meter_mirror_client_server_ind *serverInd;

  if (appMeterMirrorClient.serverFound != true)
  {
    // server not found; not creating MUP
    fprintf(stdout, "xxxx --------  MUP server not found cannot create MMR\n");
    return;
  }

  //serverInd = &appMeterMirrorClient.serverInd;

  //debug only
  fprintf(stdout, "Active Power: trans id %d pending cnf %d \n", 
    appMeterMirrorClient.mmrInfo[MMR_TYPE_ACTIVEPOWER].pendingTransId,
    appMeterMirrorClient.mmrInfo[MMR_TYPE_ACTIVEPOWER].pendingCnf);
  fprintf(stdout, "Reactive Power: trans id %d pending cnf %d \n", 
    appMeterMirrorClient.mmrInfo[MMR_TYPE_REACTIVEPOWER].pendingTransId,
    appMeterMirrorClient.mmrInfo[MMR_TYPE_REACTIVEPOWER].pendingCnf);
  fprintf(stdout, "Voltage: trans id %d pending cnf %d \n", 
    appMeterMirrorClient.mmrInfo[MMR_TYPE_VOLTAGE].pendingTransId,
    appMeterMirrorClient.mmrInfo[MMR_TYPE_VOLTAGE].pendingCnf);
  fprintf(stdout, "Freq: trans id %d pending cnf %d \n", 
    appMeterMirrorClient.mmrInfo[MMR_TYPE_FREQUENCY].pendingTransId,
    appMeterMirrorClient.mmrInfo[MMR_TYPE_FREQUENCY].pendingCnf);

  descLen_act_pwr = strlen(DEFAULT_ACTIVEPOWER_DESCRIPTION);
  if (descLen_act_pwr >= SL_XSD_DESCRIPTION_LEN)
  {
    descLen_act_pwr = SL_XSD_DESCRIPTION_LEN - 1;
  }
  descLen_react_pwr = strlen(DEFAULT_REACTIVEPOWER_DESCRIPTION);
  if (descLen_react_pwr >= SL_XSD_DESCRIPTION_LEN)
  {
    descLen_react_pwr = SL_XSD_DESCRIPTION_LEN - 1;
  }
  descLen_volt = strlen(DEFAULT_VOLTAGE_DESCRIPTION);
  if (descLen_volt >= SL_XSD_DESCRIPTION_LEN)
  {
    descLen_volt = SL_XSD_DESCRIPTION_LEN - 1;
  }
  descLen_freq = strlen(DEFAULT_FREQUENCY_DESCRIPTION);
  if (descLen_freq >= SL_XSD_DESCRIPTION_LEN)
  {
    descLen_freq = SL_XSD_DESCRIPTION_LEN - 1;
  }

  u16_t length = sizeof(sl_meter_mirror_client_send_mmr_req) + DEFAULT_MMR_NUM_MR_GROUPS * sizeof(sl_mr_group_t);

  KAL_Memcpy(pReq->mupMrId, &appMeterMirrorClient.mupMrId, SL_XSD_MRID_LEN);

  if (strncmp(pReq->mrGroup[0].description, DEFAULT_ACTIVEPOWER_DESCRIPTION, descLen_act_pwr) == 0)
  {
    KAL_Memcpy(pReq->mrGroup[0].mrId, appMeterMirrorClient.mmrInfo[MMR_TYPE_ACTIVEPOWER].mrId, SL_XSD_MRID_LEN);
    mmr_type_idx = 0;
  }
  else if (strncmp(pReq->mrGroup[0].description, DEFAULT_REACTIVEPOWER_DESCRIPTION, descLen_react_pwr) == 0)
  {
    KAL_Memcpy(pReq->mrGroup[0].mrId, appMeterMirrorClient.mmrInfo[MMR_TYPE_REACTIVEPOWER].mrId, SL_XSD_MRID_LEN);
    mmr_type_idx = 1;
  }
  else if (strncmp(pReq->mrGroup[0].description, DEFAULT_VOLTAGE_DESCRIPTION, descLen_volt) == 0)
  {
    KAL_Memcpy(pReq->mrGroup[0].mrId, appMeterMirrorClient.mmrInfo[MMR_TYPE_VOLTAGE].mrId, SL_XSD_MRID_LEN);
    mmr_type_idx = 2;
  }
  else if (strncmp(pReq->mrGroup[0].description, DEFAULT_FREQUENCY_DESCRIPTION, descLen_freq) == 0)
  {
    KAL_Memcpy(pReq->mrGroup[0].mrId, appMeterMirrorClient.mmrInfo[MMR_TYPE_FREQUENCY].mrId, SL_XSD_MRID_LEN);
    mmr_type_idx = 3;
  }

  if ((!appMeterMirrorClient.mupExists == true) ||
      (appMeterMirrorClient.mmrInfo[MMR_TYPE_ACTIVEPOWER].pendingCnf == true) ||
      (appMeterMirrorClient.mmrInfo[MMR_TYPE_REACTIVEPOWER].pendingCnf == true) ||
      (appMeterMirrorClient.mmrInfo[MMR_TYPE_VOLTAGE].pendingCnf == true) ||
      (appMeterMirrorClient.mmrInfo[MMR_TYPE_FREQUENCY].pendingCnf == true))
  {
    fprintf(stdout, "Writing to buffer as all conditions do not match mupExists = %d\n", 
        appMeterMirrorClient.mupExists);
    displayTime();
    // copy to buf and set pendingReq to true
    if (mmr_type_idx == 0)
    {
      KAL_Memcpy(actpwr_buf, pReq, length);
      actpwr_buf_len = length;
      appMeterMirrorClient.mmrInfo[mmr_type_idx].pendingReq = true;
      APP_LOG_INFO("MMR Req: Active Power : Pending\n");
    }
    else if (mmr_type_idx == 1)
    {
      KAL_Memcpy(reactpwr_buf, pReq, length);
      reactpwr_buf_len = length;
      appMeterMirrorClient.mmrInfo[mmr_type_idx].pendingReq = true;
      APP_LOG_INFO("MMR Req: Rective Power : Pending\n");
    }
    else if (mmr_type_idx == 2)
    {
      KAL_Memcpy(volt_buf, pReq, length);
      volt_buf_len = length;
      appMeterMirrorClient.mmrInfo[mmr_type_idx].pendingReq = true;
      APP_LOG_INFO("MMR Req: Voltage : Pending\n");
    }
    else if (mmr_type_idx == 3)
    {
      KAL_Memcpy(freq_buf, pReq, length);
      freq_buf_len = length;
      appMeterMirrorClient.mmrInfo[mmr_type_idx].pendingReq = true;
      APP_LOG_INFO("MMR Req: Frequency : Pending\n");
    }
  }
  else // you can send the mmr req now
  {
    pReq->transactionId = app_ClientGetTransactionID();
    appMeterMirrorClient.mmrInfo[mmr_type_idx].pendingTransId = pReq->transactionId;

    // // Set server information    
    // pReq->serverSourceInfo.serverHandle = serverInd->serverHandle;
    // pReq->serverSourceInfo.source.source = serverInd->sourceList[0].source;

    app_SendMail(SL_METER_MIRROR_CLIENT_SEND_MMR_REQ, pReq, length);
    displayTime();
    if (mmr_type_idx == 0)
    {
      APP_LOG_INFO("MMR Req: Active Power sent\n");
    }
    else if (mmr_type_idx == 1)
    {
      APP_LOG_INFO("MMR Req: Reactive Power sent\n");
    }
    else if (mmr_type_idx == 2)
    {
      APP_LOG_INFO("MMR Req: Voltage sent\n");
    }
    else if (mmr_type_idx == 3)
    {
      APP_LOG_INFO("MMR Req: Frequency sent\n");
    }


  }
}

static void HandleMupCnf(sl_meter_mirror_client_send_mup_cnf *pCnf)
{
  if (appMeterMirrorClient.pendingTransId == pCnf->transactionId)
  {
    appMeterMirrorClient.pendingTransId = 0;
    if (pCnf->status == SL_CMN_RC_SUCCESS)
    {
      appMeterMirrorClient.mupExists = true;
      displayTime();
      APP_LOG_INFO("Created MUP!");
      SendPendingMMRs(-1);
    }
    else 
    {
      fprintf(stdout, "MUP cNF status = %d \n", pCnf->status);
    }
  }
}

static void HandleSendMmrCnf(sl_meter_mirror_client_send_mmr_cnf *pCnf)
{
  int x_skip;

  if (appMeterMirrorClient.mmrInfo[MMR_TYPE_ACTIVEPOWER].pendingTransId == pCnf->transactionId)
  {
    appMeterMirrorClient.mmrInfo[MMR_TYPE_ACTIVEPOWER].pendingTransId = 0;
    appMeterMirrorClient.mmrInfo[MMR_TYPE_ACTIVEPOWER].pendingCnf = false;
    
    displayTime();
    if (pCnf->status == SL_CMN_RC_SUCCESS)
    {
      APP_LOG_INFO("MMR ActivePower updated!!");
      appMeterMirrorClient.mmrInfo[MMR_TYPE_ACTIVEPOWER].exists = true;
    }
    else
    {
      APP_LOG_ERROR("Failure to update Active Power MMR data!!");
    }
    x_skip = MMR_TYPE_ACTIVEPOWER;
  }
  else if (appMeterMirrorClient.mmrInfo[MMR_TYPE_REACTIVEPOWER].pendingTransId == pCnf->transactionId)
  {
    appMeterMirrorClient.mmrInfo[MMR_TYPE_REACTIVEPOWER].pendingTransId = 0;
    appMeterMirrorClient.mmrInfo[MMR_TYPE_REACTIVEPOWER].pendingCnf = false;

    if (pCnf->status == SL_CMN_RC_SUCCESS)
    {
      APP_LOG_INFO("MMR ReactivePower updated!!");
      appMeterMirrorClient.mmrInfo[MMR_TYPE_REACTIVEPOWER].exists = true;
    }
    else
    {
      APP_LOG_ERROR("Failure to update Reactive Power MMR data!!");
    }
    x_skip = MMR_TYPE_REACTIVEPOWER;
  }
  else if (appMeterMirrorClient.mmrInfo[MMR_TYPE_VOLTAGE].pendingTransId == pCnf->transactionId)
  {
    appMeterMirrorClient.mmrInfo[MMR_TYPE_VOLTAGE].pendingTransId = 0;
    appMeterMirrorClient.mmrInfo[MMR_TYPE_VOLTAGE].pendingCnf = false;

    if (pCnf->status == SL_CMN_RC_SUCCESS)
    {
      APP_LOG_INFO("MMR Voltage updated!!");
      appMeterMirrorClient.mmrInfo[MMR_TYPE_VOLTAGE].exists = true;
    }
    else
    {
      APP_LOG_ERROR("Failure to update Voltage MMR data!!");
    }
    x_skip = MMR_TYPE_VOLTAGE;
  }
  else if (appMeterMirrorClient.mmrInfo[MMR_TYPE_FREQUENCY].pendingTransId == pCnf->transactionId)
  {
    appMeterMirrorClient.mmrInfo[MMR_TYPE_FREQUENCY].pendingTransId = 0;
    appMeterMirrorClient.mmrInfo[MMR_TYPE_FREQUENCY].pendingCnf = false;

    if (pCnf->status == SL_CMN_RC_SUCCESS)
    {
      APP_LOG_INFO("MMR Frequency updated!!");
      appMeterMirrorClient.mmrInfo[MMR_TYPE_FREQUENCY].exists = true;
    }
    else
    {
      APP_LOG_ERROR("Failure to update Frequency MMR data!!");
    }
    x_skip = MMR_TYPE_FREQUENCY;
  }
  else
  {
    APP_LOG_ERROR("Mismatch of transaction ID on updating MMR.");
  }
  SendPendingMMRs(x_skip);
}
/******************************************************************************
 * app_MeterMirrorClientHandleMail()
 ******************************************************************************/
void app_MeterMirrorClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
  switch (mailId)
  {
  case SL_METER_MIRROR_CLIENT_SEND_MUP_CNF:
    APP_LOG_INFO("app_MeterMirrorClientHandleMail: SL_METER_MIROR_CLIENT_SEND_MUP_CNF");
    app_LogStatus(((sl_meter_mirror_client_send_mup_cnf *)pMail)->status);
    HandleMupCnf((sl_meter_mirror_client_send_mup_cnf *)pMail);
    break;

  case SL_METER_MIRROR_CLIENT_SEND_MMR_CNF:
    APP_LOG_INFO("app_MeterMirrorClientHandleMail: SL_METER_MIRROR_CLIENT_SEND_MMR_CNF");
    app_LogStatus(((sl_meter_mirror_client_send_mmr_cnf *)pMail)->status);
    HandleSendMmrCnf((sl_meter_mirror_client_send_mmr_cnf*) pMail);
    break;

  case SL_METER_MIRROR_CLIENT_SERVER_IND:
    APP_LOG_INFO("app_MeterMirrorClientHandleMail: SL_METER_MIRROR_CLIENT_SERVER_IND");
    HandleMeterMirrorServerInd((sl_meter_mirror_client_server_ind *)pMail);
    break;

  case SL_METER_MIRROR_CLIENT_MUP_IND:
    APP_LOG_INFO("app_MeterMirrorClientHandleMail: SL_METER_MIRROR_CLIENT_MUP_IND");
    APP_LOG_INFO("app_MeterMirrorClientHandleMail: SL_METER_MIRROR_CLIENT_MUP_IND is not handled in this app");
    break;
  }
}

#endif // APP_CONFIG_METER_MIRROR_CLIENT_ENABLED
