/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_sdev_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        David Winn
 *
 * \brief         Application Self Device Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef   APP_SDEV_CLIENT_H
#define   APP_SDEV_CLIENT_H

/*******************************************************************************
 * Timer Events
 ******************************************************************************/


/*******************************************************************************
 * APP Server Functions
 ******************************************************************************/
void app_SdevClientHandleMail(kal_mail_id_t mailId, void *pMail);

#endif  // APP_SDEV_CLIENT_H
