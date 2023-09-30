/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_elel_client.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Application End Device Log Event List Client Example Code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef   APP_ELEL_CLIENT_H
#define   APP_ELEL_CLIENT_H

/*******************************************************************************
 * Timer Events
 ******************************************************************************/


/*******************************************************************************
 * APP Server Functions
 ******************************************************************************/
void app_ElelClientHandleMail(kal_mail_id_t mailId, void *pMail);
void app_ElelClientProcessing( void );

#endif  // APP_ELEL_CLIENT_H
