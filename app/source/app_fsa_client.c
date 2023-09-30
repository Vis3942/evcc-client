/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_fsa_client.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application Load Shed Availability Client Example Code
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
#include "app_fsa_client.h"
#include "app_utils.h"

#if APP_CONFIG_FSA_CLIENT_ENABLED

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
 * app_FsaClientHandleMail()
 ******************************************************************************/
void app_FsaClientHandleMail(kal_mail_id_t mailId, void *pMail)
{
  switch (mailId)
  {

  }
}

#endif // APP_CONFIG_FSA_CLIENT_ENABLED


