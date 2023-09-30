/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2010 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          psal.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         Interface for Platform Services Abstraction Layer
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef PSAL_H
#define PSAL_H

#include "psal_types.h"
#ifdef STDLIB_BUILD
#include "psal_failfile.h"
#include "psal_sighandler.h"
#endif


/* -------------------------------------------------------------------------------------------------
 *                                         General
 * -------------------------------------------------------------------------------------------------
 */
void PSAL_Init( void );
void PSAL_SoftReset( void );
void PSAL_SystemReset( void );
void PSAL_HazardLights(const char *func, char *file, u32_t line);

/* -------------------------------------------------------------------------------------------------
 *                                             LEDs
 * -------------------------------------------------------------------------------------------------
 */

/* ----- defines ----- */
#define LED1        0x01
#define LED2        0x02
#define LED3        0x04
#define LED4        0x08
#define ALL_LEDS    (LED1|LED2|LED3|LED4)


/* ----- function prototypes ----- */
void  PSAL_TurnOnLEDs ( u8_t leds );
void  PSAL_TurnOffLEDs( u8_t leds );
void  PSAL_ToggleLEDs ( u8_t leds );


/* -------------------------------------------------------------------------------------------------
 *                                             Switches
 * -------------------------------------------------------------------------------------------------
 */
#define SW1   0
#define SW2   1
#define SW3   2
#define SW4   3

/*
 *  Switch mapping for targets with a joystick:
 *
 *                SW1                     SW1 - UP
 *                 |                      SW2 - RIGHT
 *           SW4 --+-- SW2                SW3 - DOWN
 *                 |                      SW4 - LEFT
 *                SW3
 */


void  PSAL_ReadSwitch( u8_t sw );


/* -------------------------------------------------------------------------------------------------
 *                                             UART
 * -------------------------------------------------------------------------------------------------
 */
u8_t  PSAL_ReadUART( u8_t * buf, u8_t len );
void  PSAL_WriteUART( u8_t x );


/* -------------------------------------------------------------------------------------------------
 *                                         Tick Counter
 * -------------------------------------------------------------------------------------------------
 */
typedef void ( * psalMsTickCallbackFuncPtr_t )( i64_t msTicksSinceAlive );

void PSAL_RegisterForMsTick( psalMsTickCallbackFuncPtr_t pMsTickCallbackFunc );
i64_t PSAL_MsAlive( void );
i64_t PSAL_SecsAlive( void );


/* -------------------------------------------------------------------------------------------------
 *                                          Random Byte
 * -------------------------------------------------------------------------------------------------
 */
u8_t PSAL_RandomByte( void );


/* -------------------------------------------------------------------------------------------------
 */
#endif  /* PSAL_H */
