/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          kal_config.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         Configuration definitions for the KAL module
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef KAL_CONFIG_H
#define KAL_CONFIG_H

/*******************************************************************************
 * KAL Alarm configuration
 ******************************************************************************/
#if defined(OVERRIDE_KAL_CONFIG_ALARM_MAX_ALARMS)
#define KAL_CONFIG_ALARM_MAX_ALARMS               OVERRIDE_KAL_CONFIG_ALARM_MAX_ALARMS
#else
#define KAL_CONFIG_ALARM_MAX_ALARMS               (50)
#endif

/*******************************************************************************
 * KAL Message configuration
 ******************************************************************************/
#if defined(OVERRIDE_KAL_CONFIG_MESSAGE_MAX_CONCURRENT)
#define KAL_CONFIG_MESSAGE_MAX_CONCURRENT         OVERRIDE_KAL_CONFIG_MESSAGE_MAX_CONCURRENT
#else
#define KAL_CONFIG_MESSAGE_MAX_CONCURRENT         (50)  /* Cannot exceed 254! */
#endif

#if (KAL_CONFIG_MESSAGE_MAX_CONCURRENT == 0)
#error KAL_CONFIG_MESSAGE_MAX_CONCURRENT cannot be 0
#endif

/*******************************************************************************
 * KAL Semaphore configuration
 ******************************************************************************/
#if defined(OVERRIDE_KAL_CONFIG_SEMAPHORE_MAX_INSTANCES)
#define KAL_CONFIG_SEMAPHORE_MAX_INSTANCES        OVERRIDE_KAL_CONFIG_SEMAPHORE_MAX_INSTANCES
#else
#define KAL_CONFIG_SEMAPHORE_MAX_INSTANCES        (20)
#endif

#if (KAL_CONFIG_SEMAPHORE_MAX_INSTANCES == 0)
#error KAL_CONFIG_SEMAPHORE_MAX_INSTANCES cannot be 0
#endif

/*******************************************************************************
 * KAL Task configuration
 ******************************************************************************/
#if defined(OVERRIDE_KAL_CONFIG_TASK_MAX_CONCURRENT)
#define KAL_CONFIG_TASK_MAX_CONCURRENT            OVERRIDE_KAL_CONFIG_TASK_MAX_CONCURRENT
#else
#define KAL_CONFIG_TASK_MAX_CONCURRENT            (25)
#endif

#if (KAL_CONFIG_TASK_MAX_CONCURRENT == 0)
#error KAL_CONFIG_TASK_MAX_CONCURRENT cannot be 0
#endif

/*******************************************************************************
 * KAL Timebase configuration
 ******************************************************************************/
#if defined(OVERRIDE_KAL_CONFIG_TIMEBASE_MAX_TIMEBASES)
#define KAL_CONFIG_TIMEBASE_MAX_TIMEBASES         OVERRIDE_KAL_CONFIG_TIMEBASE_MAX_TIMEBASES
#else
#define KAL_CONFIG_TIMEBASE_MAX_TIMEBASES         (5)
#endif

#if (KAL_CONFIG_TIMEBASE_MAX_TIMEBASES == 0)
#error KAL_CONFIG_TIMEBASE_MAX_TIMEBASES cannot be 0
#endif

/*******************************************************************************
 * KAL Timer configuration
 ******************************************************************************/
#if defined(OVERRIDE_KAL_CONFIG_TIMER_MAX_CONCURRENT)
#define KAL_CONFIG_TIMER_MAX_CONCURRENT           OVERRIDE_KAL_CONFIG_TIMER_MAX_CONCURRENT
#else
#define KAL_CONFIG_TIMER_MAX_CONCURRENT           (50)
#endif

#if (KAL_CONFIG_TIMER_MAX_CONCURRENT == 0)
#error KAL_CONFIG_TIMER_MAX_CONCURRENT cannot be 0
#endif

#endif // KAL_CONFIG_H
