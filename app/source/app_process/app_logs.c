/******************************************************************************
 * Kitu Systems SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2022 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \file          app_logs.c
 *
 * \brief         Application Logging Utilities
 *
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>
#include <time.h>

#include "app_logs.h"

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

/*******************************************************************************
 * Public Data
 ******************************************************************************/

/*******************************************************************************
 * Private Constants
 ******************************************************************************/
#define LOG_BUFSIZ 256

/*******************************************************************************
 * Private Data
 ******************************************************************************/
static void *dmLogLock = NULL;

static u8_t logPriority = DM_LOG_ENABLE; // Enable all logs

#if defined(CONSOLE_LOG_DATE)
#define FORMAT "%F %T"
#else
#define FORMAT "%T"
#endif

// Send the log to console
#ifdef APP_LOG_TO_CONSOLE // In Makefile

//# define CONSOLE_LOG_DATE        // Include date in message
#define CONSOLE_LOG_TIME // Include time in message
#define CONSOLE_LOG_MSEC // Include milliseconds for time

#endif // APP_LOG_TO_CONSOLE

// If we want to log to the syslog
//#define DM_CONFIG_LOG_TO_SYSLOG
#ifdef DM_CONFIG_LOG_TO_SYSLOG
int syslogMap[] = {LOG_INFO, LOG_WARNING, LOG_ERR};
#endif // DM_CONFIG_LOG_TO_SYSLOG

static char *logNames[] = {"Info", "Warn", "Err"};

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

//
//  DM_SetLogPriority() - Set the priority of log messages.
//
////////////////////////////////////////////////////////////////////////////////
void DM_SetLogPriority(u8_t priority)
{
    logPriority = priority > DM_LOG_ENABLE ? DM_LOG_ENABLE : priority;
}

//
//  DM_LogMsg() - Format and output diagnostic message.
//
////////////////////////////////////////////////////////////////////////////////
void DM_LogMsg(u8_t id, u8_t priority, char *format, ...)
{
    (void)id;
    if (format == NULL) return;

    static char buf[LOG_BUFSIZ];

    // Filter based on the priority
    if (priority > logPriority) return;

    // Check if we need to create the lock
    if (!dmLogLock)
    {
        dmLogLock = KAL_SemaphoreCreate(1);
    }

    // Lock the buffer from concurrent accesses.
    KAL_SemaphorePend(dmLogLock, 0);

    // Output to buffer - include timestamp, ID, and priority.

#if defined(CONSOLE_LOG_TIME)

    time_t    now = time(NULL);
    struct tm tm;

    strftime(buf, sizeof(buf), FORMAT, localtime_r(&now, &tm));

    fputs(buf, stdout);

#if defined(CONSOLE_LOG_MSEC)

    struct timeval tv;

    gettimeofday(&tv, NULL);

    printf(".%03lu", tv.tv_usec / 1000);

#endif // defined(CONSOLE_LOG_MSEC)

    printf(" ");

#endif // defined(CONSOLE_LOG_TIME)

    u16_t nbytes  = 0;
    i16_t idbytes = 0;
    int   rcode;
    char *pbuf;

    // Print the log level
    idbytes = snprintf(buf, LOG_BUFSIZ, "<APP |%-4s> ", logNames[priority - 1]);
    if (idbytes < 0) idbytes = 0;
    nbytes = idbytes;

    if (strchr(format, '%') == NULL)
    {
        if (strlen(format) > LOG_BUFSIZ)
        {
            // log is too big to append id/priority, just send it out
            pbuf   = format;
            nbytes = strlen(format);
            rcode  = 0;
        }
        else
        {
            rcode = snprintf(buf + idbytes, LOG_BUFSIZ - idbytes, "%s", format);
            pbuf  = buf;
        }
    }
    else
    {
        va_list ap;
        va_start(ap, format);
        rcode = vsnprintf(buf + idbytes, LOG_BUFSIZ - idbytes, format, ap);
        va_end(ap);
        pbuf = buf;
    }

    if (rcode == -1 || rcode > LOG_BUFSIZ)
    {
        nbytes = LOG_BUFSIZ;
    }
    else
    {
        nbytes += (uint)rcode;
    }

    if ((nbytes > 0) && (nbytes <= LOG_BUFSIZ))
    {
        if (buf[nbytes - 1] == '\n')
        {
            buf[--nbytes] = '\0';
        }
    }

    // Send log to console
    fputs(pbuf, stdout);
    fputc('\n', stdout);
    fflush(stdout);

#if defined(DM_CONFIG_LOG_TO_SYSLOG)
    // Send log to syslog
    syslog(syslogMap[priority], "%s", pbuf);
#endif // defined(DM_CONFIG_LOG_TO_SYSLOG)

    // Unlock the buffer.
    KAL_SemaphorePost(dmLogLock);
}
