/******************************************************************************
 * Kitu Systems SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2022 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_logs.h
 *
 * \brief         Application Logging Utilities
 *
 */

#ifndef   APP_LOGS_H
#define   APP_LOGS_H

#ifdef USE_SEPARATE_APP_PROCESS

#include "kal_api.h"


/*******************************************************************************
 * Logging Priority
 ******************************************************************************/
#ifndef DM_LOG_PRIORITY_INFO
#define DM_LOG_DISABLE                              ((u8_t)0)
#define DM_LOG_PRIORITY_INFO                        ((u8_t)1)
#define DM_LOG_PRIORITY_WARNING                     ((u8_t)2)
#define DM_LOG_PRIORITY_ERROR                       ((u8_t)3)
#define DM_LOG_ENABLE                               ((u8_t)4)

#define DM_ID_APP                                   ((u8_t)0x01)
#endif

/*******************************************************************************
 * Error Codes
 ******************************************************************************/
#ifndef DM_RC_SUCCESS
#define DM_RC_SUCCESS                               1
#endif //DM_RC_SUCCESS
#define DM_TaskHandle                               0xFF

/*******************************************************************************
 * Logging functions
 ******************************************************************************/
#define DM_ActivateDMTask(...)

#define DM_Init(...)                                (DM_RC_SUCCESS)


//
//  DM_SetLogPriority() - Set the priority of log messages.
//
////////////////////////////////////////////////////////////////////////////////
void DM_SetLogPriority(u8_t priority);

//
//  DM_LogMsg() - Format and output diagnostic message.
//
////////////////////////////////////////////////////////////////////////////////
void DM_LogMsg(u8_t id, u8_t priority, char *format, ...);

/**
 * This macro may be used to call \ref DM_LogMsg and have the calling
 * function name appended to the front of the print format string.
 */
#define DM_LogMsgFunc(id, priority, format, ...) DM_LogMsg(id, priority, "%s(): " format, __func__, ## __VA_ARGS__)

/**
 * This macro may be used to call \ref DM_LogMsg and have the calling
 * function filename and line number appended to the front of the print format
 * string.
 */
#define DM_LogMsgLoc(id, priority, format, ...) DM_LogMsg(id, priority, "%s+%u: " format, __FILE__, __LINE__, ## __VA_ARGS__)


/*******************************************************************************
 * APP Utility Defines
 ******************************************************************************/
/** Defined in app_utils.h
#define APP_LOG_INFO(msg)                           DM_LogMsg(DM_ID_APP, DM_LOG_PRIORITY_INFO,  msg)
#define APP_LOG_WARNING(msg)                        DM_LogMsg(DM_ID_APP, DM_LOG_PRIORITY_WARNING,  msg)
#define APP_LOG_ERROR(msg)                          DM_LogMsg(DM_ID_APP, DM_LOG_PRIORITY_ERROR, msg)

#define DM_LogMsgFunc(id, priority, format, ...)    DM_LogMsg(id, priority, "%s(): " format, __func__, ## __VA_ARGS__)

#define APP_DLOG(priority,format,...)               DM_LogMsg(DM_ID_APP, priority, format, ## __VA_ARGS__)
#define APP_DLOG_INFO(format,...)                   DM_LogMsg(DM_ID_APP, DM_LOG_PRIORITY_INFO, format, ## __VA_ARGS__)
#define APP_DLOG_WARNING(format,...)                DM_LogMsg(DM_ID_APP, DM_LOG_PRIORITY_WARNING, format, ## __VA_ARGS__)
#define APP_DLOG_ERROR(format,...)                  DM_LogMsg(DM_ID_APP, DM_LOG_PRIORITY_ERROR, format, ## __VA_ARGS__)
**/

#endif // USE_SEPARATE_APP_PROCESS
#endif  // APP_LOGS_H
