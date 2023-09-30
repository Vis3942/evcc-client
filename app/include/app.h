/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef   APP_H
#define   APP_H

#include <stdbool.h>
#include "kal_api.h"
#include "sl_api.h"
#include "app_config.h"

/*******************************************************************************
 * APP Defines
 ******************************************************************************/
#define APP_6LBR_DEVICE   0  // 6LowPAN Border Router Device (ROOT)
#define APP_6LR_DEVICE    1  // 6LowPAN Router Device (ROUTER)
#define APP_HOST_DEVICE   2  // Non-Router Device

/*******************************************************************************
 * APP Public Data
 ******************************************************************************/
extern u8_t   APP_TaskHandle;
extern u8_t   appCurrentState;
extern bool   appServerEnabled;
extern bool   appClientEnabled;
#if defined(STDLIB_BUILD)
extern char*  p_appPathToConfigDir;
#endif

/*******************************************************************************
 * APP Functions
 ******************************************************************************/






#endif  // APP_H
