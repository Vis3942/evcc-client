/**************************************************************************************************
 *                                                                                                *
 *  Kitu Systems, Inc.     Copyright(C) 2016.  All rights reserved.                               *
 *                                                                                                *
 *  This source code and any compilation or derivative thereof is the proprietary information of  *
 *  Kitu Systems, Inc. and is confidential in nature.                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  psal_sighandler.h                                                                             *
 *                                                                                                *
 *  This file contains the public interface to a module used for setting up certain signal        *
 *  handlers for Kitu software at a platform level.  The types of signals being handled are:      *
 *                                                                                                *
 *      - Illegal Instruction (SIGILL)                                                            *
 *      - Floating-Point Exception (SIGFPE)                                                       *
 *      - Abort (SIGABRT)                                                                         *
 *      - Segmentation Violation (SIGSEGV)                                                        *
 *                                                                                                *
 *  For these types of "crashes," the handler will make an entry to the failure file prior to     *
 *  termination.  A core file, if not previously extant, may be generated, based on system        *
 *  settings.                                                                                     *
 *                                                                                                *
 *  In addition to installing signal handlers for the types of crashes, the module will also set  *
 *  up signal handling so that SIGPIPE is ignored.  This is done so communication errors can be   *
 *  handled by the IP stack instead of causing signals.                                           *
 *                                                                                                *
 *  This module will also change the default disposition for termination signals:                 *
 *                                                                                                *
 *      - Hangup or death of controlling terminal (SIGHUP)                                        *
 *      - Keyboard Interrupt using Ctrl-C (SIGINT)                                                *
 *      - Keyboard Quit using Ctrl-\ (SIGQUIT)                                                    *
 *      - Termination (SIGTERM)                                                                   *
 *                                                                                                *
 *  The disposition of these signals will be to ignore them.  The main() thread will wait for     *
 *  these signals synchronously using sigwaitinfo().  (The behavior of the SIGQUIT signal is      *
 *  modified such that a core dump will not occur.)                                               *
 *                                                                                                *
 *  The application may register a callback routine for a crash signal and a separate callback    *
 *  routine for a termination signal.                                                             *
 *                                                                                                *
 *  This module is for use on Linux systems exclusively.                                          *
 *                                                                                                *
 **************************************************************************************************/


#ifndef PSAL_SIGHANDLER_H
#define PSAL_SIGHANDLER_H


/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef void (*psal_SighandlerCallback_t)(int  signalNum);








/**************************************************************************************************
 *                                                                                                *
 *  Public Function Prototypes                                                                    *
 *                                                                                                *
 **************************************************************************************************/


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
extern void
PSAL_SighandlerInstall(void);




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
extern void
PSAL_SighandlerRegisterCrashCallback(psal_SighandlerCallback_t  pf_callback);




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
extern void
PSAL_SighandlerRegisterTerminationCallback(psal_SighandlerCallback_t  pf_callback);




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
extern void
PSAL_SighandlerCallTerminationCallback(int  signalNum);








#endif  /* matches #ifndef PSAL_SIGHANDLER_H  */
