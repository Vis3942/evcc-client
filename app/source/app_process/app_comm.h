/**************************************************************************************************
 *                                                                                                *
 *  Kitu Systems, Inc.     Copyright(C) 2022.  All rights reserved.                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  app_comm.h                                                                                    *
 *                                                                                                *
 *  This file contains symbolic constants, type definitions, and external variable definitions    *
 *  for a communications channel between an Application process and a Services Layer process.     *
 *  The communications channel uses a stream-oriented service over UNIX domain sockets.  The data *
 *  transferred between the processes are encapsulated mail which supports both asynchronous and  *
 *  synchronous mail.  Each piece of mail is encapsulated using the following framing structure:  *
 *                                                                                                *
 *      Type                      Description                                                     *
 *      ----                      -----------                                                     *
 *      u16_t                     Length of interprocess packet (not including this length field) *
 *      u16_t                     Length of mail (i.e., length of the Parameter Block)            *
 *      kal_mail_id_t             The Mail ID                                                     *
 *      kal_mail_id_t             The reply Mail ID (used for synchronous mail; set to 0 if not   *
 *                                  used)                                                         *
 *      kal_mail_unique_handle_t  A unique handle specific to synchronous mail (set to            *
 *                                  KAL_MAIL_UNIQUE_HANDLE_UNUSED if not used)                    *
 *      u8_t[]                    Variable-length mail (Parameter Block)                          *
 *                                                                                                *
 *  This framing structure may have padding bytes within it.  However, because the same toolchain *
 *  is used for the same target processor, the native padding should be the same between the two  *
 *  processes.  Also, because UNIX domain sockets are used, the native endianness of the system   *
 *  is used for all multi-byte fields.                                                            *
 *                                                                                                *
 *  Please note that "messaging" is not supported in this interprocess model.                     *
 *                                                                                                *
 **************************************************************************************************/


#ifndef APP_COMM_H
#define APP_COMM_H


/**************************************************************************************************
 *                                                                                                *
 *  Header Files                                                                                  *
 *                                                                                                *
 **************************************************************************************************/
#include "psal_types.h"








/**************************************************************************************************
 *                                                                                                *
 *  Global Variable Definitions                                                                   *
 *                                                                                                *
 **************************************************************************************************/
extern u8_t  SL_TaskHandle;
extern u8_t  gh_taskUplink;
extern u8_t  gh_taskDownlink;



/**************************************************************************************************
 *                                                                                                *
 *  Function Prototypes                                                                           *
 *                                                                                                *
 **************************************************************************************************/


/**************************************************************************************************
 *                                                                                                *
 *  APP_CommInit() - this routine is used to initialize the communications channel.  Invocation   *
 *                   of this routine must occur *before* the KAL System Start and *after* the     *
 *                   initialization of KAL.  The following steps will occur:                      *
 *                                                                                                *
 *                       1) Blindly remove the pathname associated with the UNIX Domain socket.   *
 *                       2) A UNIX Domain socket will be created and bound to a well-known name.  *
 *                       3) The socket will be set to listen for inbound connections.             *
 *                       4) Spawn a child process for the Services Layer process.                 *
 *                       5) Wait until an inbound connection is made (using accept()).            *
 *                       6) Create two KAL tasks for APP_Uplink and APP_Downlink.  See additional *
 *                          documentation pertaining to the Grid2Home Interprocess Communication  *
 *                          model for definitions of these tasks.                                 *
 *                       7) Assign the vairable "SL_TaskHandle" to be the same as that of         *
 *                          APP_Downlink.                                                         *
 *                                                                                                *
 *                   The reason why this routine must be invoked after a call to KAL_Init() and   *
 *                   before KAL_Start() is because KAL tasks are created here.                    *
 *                                                                                                *
 *                   Note: both this Application Process and the Services Layer Process are       *
 *                         designed to commit suicide upon a communications error.  Therefore, if *
 *                         either process is killed, the other should self-terminate.             *
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
APP_CommInit(void);








#endif  /* matches #ifndef APP_COMM_H  */
