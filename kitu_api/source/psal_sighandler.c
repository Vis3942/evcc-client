/**************************************************************************************************
 *                                                                                                *
 *  Kitu Systems, Inc.     Copyright(C) 2016.  All rights reserved.                               *
 *                                                                                                *
 *  This source code and any compilation or derivative thereof is the proprietary information of  *
 *  Kitu Systems, Inc. and is confidential in nature.                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  psal_sighandler.c                                                                             *
 *                                                                                                *
 *  This file contains the implementation of a module used to set up signal handlers for software *
 *  written by Kitu Systems, Inc.                                                                 *
 *                                                                                                *
 **************************************************************************************************/


/**************************************************************************************************
 *                                                                                                *
 *  Header File Inclusions                                                                        *
 *                                                                                                *
 **************************************************************************************************/
#include <signal.h>
#include <unistd.h>
#include "psal_types.h"
#include "kal_api.h"
#include "psal_failfile.h"
#include "psal_sighandler.h"








/**************************************************************************************************
 *                                                                                                *
 *  Local Routines                                                                                *
 *                                                                                                *
 **************************************************************************************************/
static i32_t
PSAL_SighandlerSignalToFailureValue(int  signalNum);


static void
PSAL_SighandlerCrashHandler(int         signalNum,
                            siginfo_t*  p_sigInfo,
                            void*       p_context);


static void
PSAL_Sighandler(int  signalNum);








/**************************************************************************************************
 *                                                                                                *
 *  Global and File-Scope Variable Declarations                                                   *
 *                                                                                                *
 **************************************************************************************************/
psal_SighandlerCallback_t  pf_psalSighandlerApplicationCrashCallback=NULL;
psal_SighandlerCallback_t  pf_psalSighandlerApplicationTerminationCallback=NULL;








/**************************************************************************************************
 *                                                                                                *
 *  PSAL_SighandlerInstall() - this public routine is used to install process-level signal        *
 *                             handlers.                                                          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
PSAL_SighandlerInstall(void)
{
    struct sigaction  sa;

    /* Trap signals which cause core dumps (crashes).  */
    KAL_Memset(&sa, 0, sizeof(sa));
    (void) sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &PSAL_SighandlerCrashHandler;
    sa.sa_flags = SA_RESETHAND | SA_SIGINFO;
    (void) sigaddset(&sa.sa_mask, SIGILL);
    (void) sigaddset(&sa.sa_mask, SIGFPE);
    (void) sigaddset(&sa.sa_mask, SIGABRT);
    (void) sigaddset(&sa.sa_mask, SIGSEGV);
    (void) sigaction(SIGILL, &sa, NULL);
    (void) sigaction(SIGFPE, &sa, NULL);
    (void) sigaction(SIGABRT, &sa, NULL);
    (void) sigaction(SIGSEGV, &sa, NULL);

    /* Ignore signals which cause the process to terminate.  These signals will be handled 
         synchronously (using the sigwaitinfo() routine) by the main() thread.  */
    KAL_Memset(&sa, 0, sizeof(sa));
    (void) sigemptyset(&sa.sa_mask);
    sa.sa_handler = &PSAL_Sighandler;
    (void) sigaction(SIGHUP, &sa, NULL);
    (void) sigaction(SIGINT, &sa, NULL);
    (void) sigaction(SIGQUIT, &sa, NULL);
    (void) sigaction(SIGTERM, &sa, NULL);

    /* Ignore the SIGPIPE signal to allow the IP stack to trap errors.  */
    KAL_Memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    (void) sigaction(SIGPIPE, &sa, NULL);
}





/**************************************************************************************************
 *                                                                                                *
 *  PSAL_SighandlerRegisterCrashCallback() - this public routine is used to register an           *
 *                                           application callback which is invoked whenever a     *
 *                                           "crash" signal occurs.  A NULL parameter is          *
 *                                           acceptable to deregister a previously-registered     *
 *                                           callback.                                            *
 *                                                                                                *
 *                                           The registered callback should only use AS-Safe      *
 *                                           routines.  It should be noted that after the         *
 *                                           callback is invoked, the PSAL crash handler will     *
 *                                           terminate the process by allowing the crash to       *
 *                                           occur.  The intent of the callback is to perform any *
 *                                           specialized cleanup prior to process termination.    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      psal_SighandlerCallback_t  pf_callback - a pointer to a callback function, or NULL.       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
PSAL_SighandlerRegisterCrashCallback(psal_SighandlerCallback_t  pf_callback)
{
    pf_psalSighandlerApplicationCrashCallback = pf_callback;
}




/**************************************************************************************************
 *                                                                                                *
 *  PSAL_SighandlerRegisterTerminationCallback() - this public routine is used to register an     *
 *                                                 application callback which is invoked whenever *
 *                                                 a termination signal occurs.  A NULL parameter *
 *                                                 is acceptable to deregister a                  *
 *                                                 previously-registered callback.                *
 *                                                                                                *
 *                                                 The registered callback should only use        *
 *                                                 AS-Safe routines.  It should be noted that     *
 *                                                 after the callback is invoked, the PSAL        *
 *                                                 termination handler will terminate the process *
 *                                                 by calling _exit().  The intent of the         *
 *                                                 callback is to perform any specialized cleanup *
 *                                                 prior to process termination.                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      psal_SighandlerCallback_t  pf_callback - a pointer to a callback function, or NULL.       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
PSAL_SighandlerRegisterTerminationCallback(psal_SighandlerCallback_t  pf_callback)
{
    pf_psalSighandlerApplicationTerminationCallback = pf_callback;
}




/**************************************************************************************************
 *                                                                                                *
 *  PSAL_SighandlerCallTerminationCallback() - this public routine is used to invoke the          *
 *                                             application callback associated with a termination *
 *                                             signal (SIGHUP, SIGINT, SIGQUIT, and SIGTERM).     *
 *                                             This routine will simply call the termination      *
 *                                             callback, if one is registered.  It is the         *
 *                                             responsibility of the caller to terminate the      *
 *                                             process.                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      int  signalNum - the termination signal, one of SIGHUP, SIGINT, SIGQUIT, or SIGTERM.      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
PSAL_SighandlerCallTerminationCallback(int  signalNum)
{
    if (pf_psalSighandlerApplicationTerminationCallback != NULL)
    {
        (*pf_psalSighandlerApplicationTerminationCallback)(signalNum);
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  PSAL_SighandlerSignalToFailureValue() - this private routine maps a signal number to a        *
 *                                          failure code.  The set of failure codes used is a     *
 *                                          subset of those defined in psal_failfile.h.           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      int  signalNum - the signal number.                                                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      i32_t - the corresponding failure code.                                                   *
 *                                                                                                *
 **************************************************************************************************/
static i32_t
PSAL_SighandlerSignalToFailureValue(int  signalNum)
{
    i32_t  retCode=PSAL_FAILFILE_FAILURE_CODE_UNKNOWN;

    switch (signalNum)
    {
    case SIGILL:
        retCode = PSAL_FAILFILE_FAILURE_CODE_SIGILL;
        break;
    case SIGFPE:
        retCode = PSAL_FAILFILE_FAILURE_CODE_SIGFPE;
        break;
    case SIGABRT:
        retCode = PSAL_FAILFILE_FAILURE_CODE_SIGABRT;
        break;
    case SIGSEGV:
        retCode = PSAL_FAILFILE_FAILURE_CODE_SIGSEGV;
        break;
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  PSAL_SigHandlerCrashHandler() - this private routine is the signal handler for various        *
 *                                  crashes.                                                      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      int         signalNum - the signal number.                                                *
 *      siginfo_t*  p_sigInfo - a pointer to signal information.                                  *
 *      void*       p_context - a pointer to the user context (of type ucontext_t).               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void
PSAL_SighandlerCrashHandler(int         signalNum,
                            siginfo_t*  p_sigInfo,
                            void*       p_context)
{
    /* Update the failure file.  */
    PSAL_FailfileUpdate(PSAL_SighandlerSignalToFailureValue(signalNum), p_sigInfo->si_code,
                        p_sigInfo->si_addr);

    /* If extant, invoke the application callback routine.  */
    if (pf_psalSighandlerApplicationCrashCallback != NULL)
    {
        (*pf_psalSighandlerApplicationCrashCallback)(signalNum);
    }

    /* Do nothing and let the actual default signal handler fire.  This can cause a core dump to
         be generated, if the system is properly configured.  */
}




/**************************************************************************************************
 *                                                                                                *
 *  PSAL_SigHandler() - this private routine is the signal handler for various termination        *
 *                      signals. This subroutine is used to handle signals from raise(). The      *
 *                      main() thread will handle the termination after returning                 *
 *                      from a call to sigwaitinfo().  The termination signals                    *
 *                      cannot be ignored (using SIG_IGN) because the main()                      *
 *                      thread will never receive notification that the signal                    *
 *                      occurred.  (Oddly, other threads can, and only if the                     *
 *                      main() thread has the signal blocked in its mask.                         *
 *                      However, if the main() thread does not have the signal                    *
 *                      blocked, no threads receive the signal if SIG_IGN is                      *
 *                      used.)                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      int  signalNum - the signal number.                                                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void
PSAL_Sighandler(int  signalNum)
{
    if (pf_psalSighandlerApplicationTerminationCallback != NULL)
    {
        (*pf_psalSighandlerApplicationTerminationCallback)(signalNum);
    }

    exit(signalNum);
}
