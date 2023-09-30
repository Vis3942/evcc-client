/**************************************************************************************************\
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  psal_port_linux.c                                                                             *
 *                                                                                                *
 *  This file contains the Linux implementation of the Platform-Specific Abstraction Layer.       *
 *  Because of the many differences between a Linux platform and an embedded target platform,     *
 *  most of the API routines will not perform any actual function.  These routines will be        *
 *  documented below.                                                                             *
 *                                                                                                *
\**************************************************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "psal.h"
//#include "psal_pidfile.h"








/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_Init() - this routine is used to initialize the platform-specific layer.  In essence,    *
 *                for the Linux port, it will do nothing.                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_Init(void)
{
//    PSAL_PidfileCreate();
    PSAL_SighandlerInstall();    
}

/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_SoftReset() - this routine is used to perform a system reset.  For the Linux port,       *
 *                       this call will not do anything.                                          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_SoftReset(void)
{
}

/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_SystemReset() - this routine is used to perform a system reset.  For thie LInux port,    *
 *                       this call will not do anything.                                          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_SystemReset(void)
{
}




/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_HazardLights() - this routine is called when a catastrophic error occurs, and it will    *
 *                        sit in an infinite loop for potential debug purposes.                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      Infinite loop never returns.                                                              *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_HazardLights(const char *func, char *file, u32_t line)
{
    PSAL_FailfileUpdate(PSAL_FAILFILE_FAILURE_CODE_PSALHAZ, (i32_t) line, file);
    exit(EXIT_FAILURE);
}




/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_TurnOnLEDs() - this routine will not do anything.                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  leds - indicates which LED(s) to turn on.                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_TurnOnLEDs(u8_t  leds)
{
}




/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_TurnOffLEDs() - this routine will not do anything.                                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  leds - indicates which LED(s) to turn on.                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_TurnOffLEDs(u8_t  leds)
{
}




/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_ToggleLEDs() - this routine will not do anything.                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  leds - indicates which LED(s) to turn on.                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_ToggleLEDs(u8_t  leds)
{
}




/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_ReadSwitch() - this routine will not do anything.                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  sw - indicates which switch to read.                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_ReadSwitch(u8_t  sw)
{
}




/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_ReadUART() - this routine will print out a warning message.  It is not expected that any *
 *                    code will invoke this routine.                                              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t*  buf - a pointer to where data should be placed.                                    *
 *      u8_t   len - the length to read, in bytes.                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t - the actual number of bytes read.  This Linux implementation will always return 0.  *
 *                                                                                                *
\**************************************************************************************************/
u8_t
PSAL_ReadUART(u8_t*  buf,
              u8_t   len)
{
    fprintf(stderr, "WARNING: a call was made to PSAL_ReadUART()!!\n\n");

    return 0;
}




/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_WriteUART() - this routine will only  print out a warning message.  It is not expected   *
 *                     that any code will invoke this routine.                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  x - a byte to write to the UART.                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_WriteUART(u8_t  x)
{
    fprintf(stderr, "WARNING: a call was made to PSAL_WriteUART()!!\n\n");
}




/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_RegisterForMsTick() - this routine will not do anything except print out a warning       *
 *                             message.  The Kernel Abstraction Layer should be the only code     *
 *                             calling this function, but the Linux port of KAL will be rewritten *
 *                             to use other mechanisms to simulate a millisecond tick.            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      psalMsTickCallbackFuncPtr_t  pMsTickCallbackFunc - a pointer to a function which should   *
 *                                                         be invoked every millisecond tick.     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
\**************************************************************************************************/
void
PSAL_RegisterForMsTick(psalMsTickCallbackFuncPtr_t  pMsTickCallbackFunc)
{
    fprintf(stderr, "WARNING: a call was made to PSAL_RegisterForMsTick()!!\n\n");
}



/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_MsAlive() - this routine will print a warning message as it is not expected to be called *
 *                   by code on the Linux platform.  Instead, the Kernel Abstraction Layer        *
 *                   routine, KAL_GetMilliseconds() should be used.                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      i64_t - this routine will always return 0 since it is not truly implemented.              *
 *                                                                                                *
\**************************************************************************************************/
i64_t
PSAL_MsAlive(void)
{
    fprintf(stderr, "WARNING: a call was made to PSAL_MsAlive()!!\n\n");

    return 0;
}



/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_SecsAlive() - this routine will print a warning message as it is not expected to be      *
 *                     called by code on the Linux platform.                                      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      i64_t - this routine will always return 0 since it is not truly implemented.              *
 *                                                                                                *
\**************************************************************************************************/
i64_t
PSAL_SecsAlive(void)
{
    fprintf(stderr, "WARNING: a call was made to PSAL_SecsAlive()!!\n\n");

    return 0;
}



/**************************************************************************************************\
 *                                                                                                *
 *  PSAL_RandomByte() - this routine will not randomly generate a byte; it will only print a      *
 *                      warning message as it is not expected to be used.  Instead, code should   *
 *                      use the Kernel Abstraction Layer routine KAL_GetRandom() instead.         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t - this routine will always return 0 since it is not truly implemented.               *
 *                                                                                                *
\**************************************************************************************************/
u8_t
PSAL_RandomByte(void)
{
    fprintf(stderr, "WARNING: a call was made to PSAL_RandomByte()!!\n\n");

    return 0;
}



