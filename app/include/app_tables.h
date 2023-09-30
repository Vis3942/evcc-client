/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_tables.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         APP function set configuration tables
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef   APP_TABLES_H
#define   APP_TABLES_H

#include "psal_types.h"

void APP_HandleMail(kal_mail_id_t mailId, void *pMail);
void APP_HandleTimeouts(u32_t eventMask);


#endif
