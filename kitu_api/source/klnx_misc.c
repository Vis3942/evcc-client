/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  kal_misc.c                                                                                    *
 *                                                                                                *
 *  This file contains the implementation for miscellaneous routines in the Kernel Abstraction    *
 *  Layer.  This includes functions to specify the ability to sleep, and functions to control     *
 *  interrupts.  Note: for the Linux port, none of these functions perform any real work.         *
 *                                                                                                *
 **************************************************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h> 
#include <unistd.h> 
#include "klnx_internal.h"








/**************************************************************************************************
 *                                                                                                *
 *  KAL_Init() - this routine is used to initialize all internal KAL data structures.  It must be *
 *               invoked only once at startup.  This routine will simply invoke                   *
 *               KAL_SystemInitialize().                                                          *
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
 **************************************************************************************************/
void
KAL_Init(void)
{
    KAL_SystemInitialize();
    KAL_RandomSeed(0x12345678);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_Start() - this routine is used to start all tasks.  This will simply invoke the           *
 *                KAL_SystemStart() routine.                                                      *
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
 **************************************************************************************************/
void
KAL_Start(void)
{
    KAL_SystemStart();
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_AllowSleep() - this routine is invoked by a task to indicate that it is in a state which  *
 *                     allows sleep.  For the Linux port, this routine will do nothing.           *
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
 **************************************************************************************************/
void
KAL_AllowSleep(void)
{
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_DisallowSleep() - this routine is invoked by a task to indicate that it is in a state     *
 *                        which does not allow sleep.  For the Linux port, this routine will do   *
 *                        nothing.                                                                *
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
 **************************************************************************************************/
void
KAL_DisallowSleep(void)
{
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_DisableInterrupts() - this routine is used to disable interrupts (at the processor level) *
 *                            on embedded targets.  For the Linux port, this routine will do      *
 *                            nothing.                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u32_t - information about the processor's interrupt mask state prior to the invocation.   *
 *                                                                                                *
 **************************************************************************************************/
u32_t
KAL_DisableInterrupts(void)
{
    return 0;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_EnableInterrupts() - this routine is used to enable interrupts (at the processor level)   *
 *                           on embedded targets.  Specifically, it is used to re-enable the      *
 *                           prior state of interrupt masking when a previous call to             *
 *                           KAL_DisableInterrupts() was made.  For the Linux port, this routine  *
 *                           will do nothing.                                                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u32_t  flags - information about the desired processor's interrupt mask state.            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_EnableInterrupts(u32_t  flags)
{
}


/*******************************************************************************
 * Random Number Generator
 *  Use 3 separate LCG Random Number Generators
******************************************************************************/
#define RAND_GENS   3
#define RAND1_A     (1664525)           // Numerical Recipes
#define RAND1_C     (1013904223)
#define RAND2_A     (214013)            // MS Visual C/C++
#define RAND2_C     (2531011)
#define RAND3_A     (22695477)          // Borland C/C++
#define RAND3_C     (1)

typedef struct {
  u32_t     a;
  u32_t     c;
} rand_lcg_t;

static rand_lcg_t   randgens[RAND_GENS] = { {RAND1_A, RAND1_C}, {RAND2_A, RAND2_C}, {RAND3_A, RAND3_C}};
static u32_t        randlcgstate[RAND_GENS];

/*******************************************************************************
 * rand_lcg()
 ******************************************************************************/
/**
 * \brief P get random number from the next LCG
 */
static u16_t rand_lcg(void)
{
    static  u8_t    index=0;

    if (++index >= RAND_GENS) index = 0;
    randlcgstate[index] = (randlcgstate[index] * randgens[index].a) + randgens[index].c;
    return (u16_t)(randlcgstate[index] >> 14);
}

/*******************************************************************************
 * KAL_GetRandom()
 ******************************************************************************/
/**
 * \brief P get a 32-bit random number
 */
u32_t KAL_GetRandom( void )
{
  int   randdesc;
  u32_t randval;

  // Try using the non-blocking OS random number generator
  if ((randdesc=open("/dev/urandom", O_RDONLY)) >= 0)
  {
    // Note: urandom always returns the number of bytes requested without blocking
    int stat = read(randdesc, &randval, sizeof(u32_t));
    assert(stat == sizeof(u32_t));
    close(randdesc);
    return (randval);
  }
  else  // Fall back to the LCG random number generator
  {
    return ((rand_lcg() << 16) | rand_lcg());
  }
}

/*******************************************************************************
 * KAL_RandomSeed()
 ******************************************************************************/
/**
 * \brief P get random number
 */
void KAL_RandomSeed( u32_t seed )
{
    int     i;
    u32_t   oseed;

    oseed = (u32_t)KAL_GetMilliseconds();
    if (oseed == 0) oseed = 0xabcd1234;
    oseed = (oseed * 0x87654321) ^ seed;
    for (i=0; i < RAND_GENS; i++)  randlcgstate[i] = oseed;
}




/**************************************************************************************************
 *                                                                                                *
 *  dp32() - this debug routine simply prints a 32-bit value to the standard output.  Only the    *
 *           value is printed (i.e., no newline), and it is printed in hexadecimal format.        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u32_t  num - the number to print.                                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
dp32(u32_t  num)
{
    printf("%08X", num);
}




/**************************************************************************************************
 *                                                                                                *
 *  dp16() - this debug routine simply prints a 16-bit value to the standard output.  Only the    *
 *           value is printed (i.e., no newline), and it is printed in hexadecimal format.        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u16_t  num - the number to print.                                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
dp16(u16_t  num)
{
    printf("%04X", num);
}




/**************************************************************************************************
 *                                                                                                *
 *  dp8() - this debug routine simply prints a 32-bit value to the standard output.  Only the     *
 *          value is printed (i.e., no newline), and it is printed in hexadecimal format.         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  num - the number to print.                                                          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
dp8(u8_t  num)
{
    printf("%02X", num);
}




/**************************************************************************************************
 *                                                                                                *
 *  dps() - this debug routine simply prints a string to the standard output.  Only the string is *
 *          is printed (i.e., no newline is added).                                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t*  string - the string to print.  The string must be terminated properly.             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
dps(char*  string)
{
    printf("%s", string);
}




/**************************************************************************************************
 *                                                                                                *
 *  dpnl() - this debug routine simply prints a carriage return and line feed (CR/LF) to the      *
 *           standard output.                                                                     *
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
 **************************************************************************************************/
void
dpnl(void)
{
    printf("\r\n");
}


/*******************************************************************************
 * dparray()
 ******************************************************************************/
/**
 * \brief print array of bytes
 */
void dparray( u8_t *array, u16_t len )
{
    u16_t  i;
    for ( i = 0; i < len; i++ )
    {
        dp8(array[i]);

        if ( ( i%40) == 39 ) dpnl();
        else if ( ( i%10) == 9 ) dps(" ");
    }
}

/*******************************************************************************
 * dpip()
 ******************************************************************************/
/**
 * \brief print ip address
 */
void dpip( u8_t *ip )
{
    if ( ip[0] == 0xFF )
    {
        u8_t i,nz;

        dps("M:");
        dp8(ip[0]);dp8(ip[1]);dps("::");
        for ( i = 2, nz = 0; i<16; i++ )
        {
            if ( nz || ip[i] )
            {
                nz = 1;
                dp8(ip[i]);
            }
        }
    }
    else
    {
        if ( KAL_GetBE16(ip) == 0xFE80 )
        {
          dps("LL:");
        }
        else
        {
          dps("GP:");
        }
        if ((ip[11]==0xff)&&(ip[12]==0xfe))
        {
            dparray( ip+14, 2 );
        }
        else
        {
            dparray( ip+8, 8 );
        }
    }
}




