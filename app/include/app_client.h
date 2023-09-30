/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Client Application Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef   APP_CLIENT_H
#define   APP_CLIENT_H

#include "sl_api.h"

/*******************************************************************************
 * Structures
 ******************************************************************************/
typedef struct server_node {
  sl_cmn_client_server_ind_t *pServer;
  struct server_node         *next;
} app_client_server_node_t;

typedef struct program_node {
  u8_t                                mrId[SL_XSD_MRID_LEN];
  void                               *pProgramData;
  sl_cmn_client_server_source_info_t  serverSourceInfo;
  struct program_node                *next;
} app_client_program_node_t;

/*******************************************************************************
 * Timer Events
 ******************************************************************************/

// Note: We are re-using the SERVER Bit and Mask since we will not be running
// the Server Demo and Client Demo simultaneously

#define APP_CLIENT_DEMO_TIMEOUT_BIT    (16)
#define APP_CLIENT_DEMO_TIMEOUT_MASK   (1 << APP_CLIENT_DEMO_TIMEOUT_BIT)

#define APP_CLIENT_DEMO_TIMER_SECS     (1)
#define APP_CLIENT_DEMO_TIMER_MS       ((u32_t)(APP_CLIENT_DEMO_TIMER_SECS*1000))

/*******************************************************************************
 * APP Client Functions
 ******************************************************************************/
void app_ClientHandleSLClientEventInd(u8_t event, bool opt_out);
void app_ClientProcessing(void);
void app_ClientStart(void);

bool app_ClientFindRootIndex(app_client_server_node_t *pServerNode, sl_cmn_client_root_source_info_t rootInfo, u8_t *index);
app_client_server_node_t * app_ClientFindServerNode(sl_cmn_client_server_handle_t serverHandle, app_client_server_node_t *pServerHead);
bool app_ClientRegistered(app_client_server_node_t *pServerHead);
void app_ClientStoreProgramNode(sl_cmn_prog_status_t status, bool dataExists, void *pData, u16_t dataSize, u8_t *mrId,
                                sl_cmn_client_server_source_info_t serverSourceInfo, app_client_program_node_t **ppProgramHead);
void app_ClientStoreServerNode(sl_cmn_client_server_ind_t *pInd, app_client_server_node_t **ppServerHead);

u32_t app_ClientGetTransactionID(void);

#endif  // APP_CLIENT_H
