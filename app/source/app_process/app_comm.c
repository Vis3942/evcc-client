/**************************************************************************************************
 *                                                                                                *
 *  Kitu Systems, Inc.     Copyright(C) 2022.  All rights reserved.                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  app_comm.c                                                                                    *
 *                                                                                                *
 *  This file contains symbolic constants, type definitions, global variable declarations, and    *
 *  function implementations for establishing and using a communications channel between an       *
 *  Application process and a Services Layer process.  The communications channel uses a          *
 *  stream-oriented service over UNIX domain sockets.  The data transferred between the processes *
 *  are encapsulated mail which supports both asynchronous and  synchronous mail.  Each piece of  *
 *  mail is encapsulated using the following framing structure:                                   *
 *                                                                                                *
 *      Type                      Description                                                     *
 *      ----                      -----------                                                     *
 *      u16_t                     Length of interprocess packet (not including this length field) *
 *      u16_t                     Length of mail (i.e., length of the Parameter Block)            *
 *      kal_mail_id_t             The Mail ID                                                     *
 *      kal_mail_unique_handle_t  A unique handle specific to synchronous mail (set to            *
 *                                  KAL_MAIL_UNIQUE_HANDLE_UNUSED if not used)                    *
 *      u8_t[]                    Variable-length mail (Parameter Block)                          *
 *                                                                                                *
 *  This framing structure may have padding bytes within it.  However, because the same toolchain *
 *  is used for the same target processor, the native padding should be the same between the two  *
 *  processes.  Also, because UNIX domain sockets are used, the native endianness of the system   *
 *  is used for all multi-byte fields.                                                            *
 *                                                                                                *
 *  Please note that "messaging" is not supported in this interprocess model; only "mail" is      *
 *  supported.                                                                                    *
 *                                                                                                *
 **************************************************************************************************/


/**************************************************************************************************
 *                                                                                                *
 *  Header Files                                                                                  *
 *                                                                                                *
 **************************************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "psal_assert.h"
#include "psal_types.h"
#include "app_comm.h"
#include "kal_api.h"








/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define APP_COMM_CHANNEL_NAME                         "g2hipca.socket"
#define APP_COMM_LENGTH_OF_FRAME_HEADER               (sizeof(u16_t))
#define APP_COMM_IPC_UPLINK_IND                       (0x8001)
#define APP_COMM_UNIX_PATH_MAX                        (96)           // UNIX_PATH_MAX is typically 108, if defined.

#define APP_COMM_AUTOEXEC_SL                          // Uncomment this to have the APP process execute the SL process

#ifdef OVERRIDE_APP_COMM_SL_DM_PORT_NUMBER            // This is the DM port number for the SL process
#define APP_COMM_SL_DM_PORT_NUMBER                    OVERRIDE_APP_COMM_SL_DM_PORT_NUMBER
#else
#define APP_COMM_SL_DM_PORT_NUMBER                    "12346"
#endif

#ifdef OVERRIDE_APP_COMM_SL_IP_AUTOSELECT             // This is the auto-selected interface for the SL process
#define APP_COMM_SL_IP_AUTOSELECT                     OVERRIDE_APP_COMM_SL_IP_AUTOSELECT
#else
#define APP_COMM_SL_IP_AUTOSELECT                     NULL
#endif

#ifdef OVERRIDE_APP_NAME_OF_SERVICES_LAYER_EXECUTABLE
#define APP_NAME_OF_SERVICES_LAYER_EXECUTABLE         OVERRIDE_APP_NAME_OF_SERVICES_LAYER_EXECUTABLE
#else
#define APP_NAME_OF_SERVICES_LAYER_EXECUTABLE         "./sl_api_process"
#endif







/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Nota bene: the type definitions below must have exact counterparts within the Services Layer  *
 *             Application process.  These are defined in .../apps/sl_process/source/sla_comm.c.  *
 *                                                                                                *
 **************************************************************************************************/
typedef struct appComm_IpcHeader_s                // Note: this structure does not contain the
{                                                 //       length of the IPC packet, nor the
    u16_t                     parmBlockLength;    //       parameter block.
    kal_mail_id_t             mailId;
    kal_mail_unique_handle_t  h_uniqueVal;
}  appComm_IpcHeader_t;


typedef struct appComm_IpcPacket_s
{
    appComm_IpcHeader_t  hdr;
    u32_t                parmBlock[1];            // Placekeeper for the parameter block.
}  appComm_IpcPacket_t;








/**************************************************************************************************
 *                                                                                                *
 *  Local Function Definitions                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static i32_t
APP_CommBlockingRead(u8_t*  p_buffer,
                     u16_t  numOctets);


static bool
APP_CommBlockingWrite(u8_t*  p_buffer,
                      u16_t  numOctets);


static void
APP_CommError(void);


static bool
APP_CommGetUplink(appComm_IpcPacket_t**  ap_pkt,
                  u16_t*                 p_pktLength);


static bool
APP_CommSendDownlink(kal_mail_details_t*       p_mailDetails,
                     kal_mail_unique_handle_t  h_unique);


static void
APP_CommUplink(void*  p_dummy);


static void
APP_CommDownlink(void*  p_dummy);


static void
APP_CommSendUplinkToApp(appComm_IpcPacket_t*  p_ipcPkt,
                        u16_t                 pktLength);


static bool
APP_CommSetUpSigchldHandler(void);


static void
APP_CommSigchldHandler(int  signalNum);


static const char*
APP_CommGetFilename(const char*  p_pathname);



extern char *GetInterface(void);



/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
u8_t                  SL_TaskHandle=0xFF;
u8_t                  gh_taskUplink=0xFF;
u8_t                  gh_taskDownlink=0xFF;
i32_t                 slProcessId=0;
static i32_t          appCommListeningDescriptor=-1;
static i32_t          appCommCommDescriptor=-1;
static volatile bool  appCommConnected=false;








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommInit() - this routine is used to initialize the communications channel.  Invocation   *
 *                   of this routine must occur *before* the KAL System Start and *after* the     *
 *                   initialization of KAL.  The following steps will occur:                      *
 *                                                                                                *
 *                       1) Blindly remove the pathname associated with the UNIX Domain socket.   *
 *                       2) A UNIX Domain socket will be created and bound to a well-known name.  *
 *                       3) The socket will be set to listen for inbound connections.             *
 *                       4) Set up a signal handler for SIGCHLD.                                  *
 *                       5) Spawn a child process for the Services Layer process.                 *
 *                       6) Wait until an inbound connection is made (using accept()).            *
 *                       7) Create two KAL tasks for APP_Uplink and APP_Downlink.  See additional *
 *                          documentation pertaining to the Grid2Home Interprocess Communication  *
 *                          Model for definitions of these tasks.                                 *
 *                       8) Assign the vairable "SL_TaskHandle" to be the same as that of         *
 *                          APP_Downlink.                                                         *
 *                                                                                                *
 *                   The reason why this routine must be invoked after a call to KAL_Init() and   *
 *                   before KAL_Start() is because KAL tasks are created here.  Therefore, this   *
 *                   routine must be invoked by the main() thread.                                *
 *                                                                                                *
 *                   Note: both this Application Process and the Services Layer Process are       *
 *                         designed to commit suicide upon a communications error.  Therefore, if *
 *                         either process is killed, the other should self-terminate.  SIGCHLD is *
 *                         used to detect a problem in the Services Layer process prior to        *
 *                         connection.                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      SL_TaskHandle                                                                             *
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
APP_CommInit(void)
{
    struct sockaddr_un  unixDomainName;
    struct sockaddr_un  peerName;
    socklen_t           saLength;
    struct stat         fileStatus;

    /* Check that the Services Layer executable exists.  */
    if (stat(APP_NAME_OF_SERVICES_LAYER_EXECUTABLE, &fileStatus) == 0)
    {
        if (((fileStatus.st_mode & S_IFREG) == 0) ||
            ((fileStatus.st_mode & S_IRUSR) == 0) ||
            ((fileStatus.st_mode & S_IXUSR) == 0))
        {
            fprintf(stderr, "FATAL ERROR: invalid permissions for \"%s\"!!\n\n",
                    APP_NAME_OF_SERVICES_LAYER_EXECUTABLE);
            exit(-1);
        }
    }
    else
    {
        fprintf(stderr, "FATAL ERROR: unable to obtain file status on \"%s\"; %s\n",
                APP_NAME_OF_SERVICES_LAYER_EXECUTABLE, strerror(errno));
        exit(-1);
    }

    /* Create a UNIX Domain socket.  */
    appCommListeningDescriptor = socket(AF_UNIX, SOCK_STREAM, 0);
    if (appCommListeningDescriptor == -1)
    {
        fprintf(stderr, "FATAL ERROR: unable to create a UNIX Domain socket; %s\n",
                strerror(errno));
        exit(-1);
    }

    /* Bind the socket to a well-known abstract name based on PID.  */
    KAL_Memset(&unixDomainName, 0, sizeof(unixDomainName));
    unixDomainName.sun_family = AF_UNIX;
    snprintf(unixDomainName.sun_path, APP_COMM_UNIX_PATH_MAX, "%c%s.%d", 0,
             APP_COMM_CHANNEL_NAME, getpid());
    saLength = sizeof(unixDomainName);
    if (bind(appCommListeningDescriptor, (struct sockaddr*) &unixDomainName, saLength) == -1)
    {
        close(appCommListeningDescriptor);
        fprintf(stderr, 
                "FATAL ERROR: unable to bind a UNIX Domain socket to abstract address %s; %s\n",
                &unixDomainName.sun_path[1], strerror(errno));
        exit(-1);
    }

    /* Mark the socket as listening for an incoming connection.  */
    if (listen(appCommListeningDescriptor, 1) == -1)
    {
        close(appCommListeningDescriptor);
        fprintf(stderr, "FATAL ERROR: unable to mark a UNIX Domain socket as listening; %s\n",
                strerror(errno));
        exit(-1);
    }

    /* Set up a signal handler for SIGCHLD.  */
    if (APP_CommSetUpSigchldHandler() == false)
    {
        close(appCommListeningDescriptor);
        fprintf(stderr, "FATAL ERROR: unable to set up a signal handler for SIGCHLD!!\n\n");
        exit(-1);
    }

#ifdef APP_COMM_AUTOEXEC_SL
    /* Spawn a child process to execute the Services Layer process.  */
    slProcessId = fork();
    if (slProcessId == -1)
    {
        /* Error occurred during fork().  */
        close(appCommListeningDescriptor);
        fprintf(stderr, "FATAL ERROR: unable to fork a process; %s\n", strerror(errno));
        exit(-1);
    }
    else if (slProcessId == 0)
    {
        /* Child process.  Overwrite the process with the Services Layer process.  */
        execl(APP_NAME_OF_SERVICES_LAYER_EXECUTABLE,
              APP_CommGetFilename(APP_NAME_OF_SERVICES_LAYER_EXECUTABLE), 
              "-g", APP_COMM_SL_DM_PORT_NUMBER,
              "-i", GetInterface(),
              (char*) APP_COMM_SL_IP_AUTOSELECT,
              (char*) NULL);
        exit(0);
    }
#endif  // APP_COMM_AUTOEXEC_SL

    /* Wait for an incoming connection.   */
    saLength = sizeof(peerName);
    KAL_Memset(&peerName, 0, saLength);
    appCommCommDescriptor = accept(appCommListeningDescriptor, (struct sockaddr*) &peerName, &saLength);
    if (appCommCommDescriptor == -1)
    {
        close(appCommListeningDescriptor);
        fprintf(stderr, "FATAL ERROR: unable to accept an incoming connection; %s\n",
                strerror(errno));
        exit(-1);
    }
    fprintf(stderr, "INFO: connected to Services Layer process.\n");

    /* Now that a connection has been made, we have the option of whether to de-install the
         SIGCHLD signal handler.  For the sake of increased visibility, instead of de-installing
         the signal handler, a flag will be set so the signal handler won't do anything except
         print a message when SIGCHLD is raised.  */
    appCommConnected = true;

    /* Create the APP_Uplink and APP_Downlink tasks.  These names are defined in the Grid2Home
         Interprocess Communication Model documentation.  */
    gh_taskUplink = KAL_TaskCreate(APP_CommUplink, NULL, "Up", NULL, 0, 100);
    gh_taskDownlink = KAL_TaskCreate(APP_CommDownlink, NULL, "Down", NULL, 0, 100);
    if ((gh_taskUplink == G2_FAILURE) || (gh_taskDownlink == G2_FAILURE))
    {
        close(appCommListeningDescriptor);
        close(appCommCommDescriptor);
        fprintf(stderr,
                "FATAL ERROR: unable to create KAL tasks required for APP_Uplink and/or APP_Downlink!!\n\n");
        exit(-1);
    }
    SL_TaskHandle = gh_taskDownlink;
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommGetUplink() - this routine is used to receive an IPC packet from the Services Layer   *
 *                        Application process via the communications channel.  This routine will  *
 *                        remove the framing protocol used on the communications channel, leaving *
 *                        only the IPC packet.  It will block until a complete IPC packet is      *
 *                        received, or until an error occurs.  If an error does occur, the        *
 *                        communications channel will be closed.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      appComm_IpcPacket_t**  ap_pkt      - the address of a pointer to an IPC packet.  This     *
 *                                           routine will allocate memory for the IPC packet,     *
 *                                           changing the value of the pointer.                   *
 *      u16_t*                 p_pktLength - a pointer to where the packet length will be placed. *
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
 *      true  - if an IPC packet was successfully read.                                           *
 *      false - if an error occurred.                                                             *
 *                                                                                                *
 **************************************************************************************************/
static bool
APP_CommGetUplink(appComm_IpcPacket_t**  ap_pkt,
                  u16_t*                 p_pktLength)
{
    bool   retValue=false;
    i32_t  numBytes;

    /* Read the IPC packet length.  */
    numBytes = APP_CommBlockingRead((u8_t*) p_pktLength, APP_COMM_LENGTH_OF_FRAME_HEADER);
    if (numBytes == APP_COMM_LENGTH_OF_FRAME_HEADER)
    {
        /* Dynamically allocate a buffer for the IPC packet.  */
        *ap_pkt = (appComm_IpcPacket_t*) KAL_Malloc(*p_pktLength);
        if (*ap_pkt != NULL)
        {
            KAL_Memset(*ap_pkt, 0, *p_pktLength);
            numBytes = APP_CommBlockingRead((u8_t*) *ap_pkt, *p_pktLength);
            if (numBytes == *p_pktLength)
            {
                retValue = true;
            }
            else
            {
                /* Since the read failed, release memory for the IPC packet.  */
                KAL_Free(*ap_pkt);
                *ap_pkt = NULL;
                *p_pktLength = 0;

                /* Print error and terminate.  */
                fprintf(stderr,
                        "INFO: communications error with Services Layer Application process!!  Terminating...\n\n");
                exit(-1);
            }
        }
        else
        {
            fprintf(stderr, "ERROR: APP_CommGetUplink(): dynamic memory failure!!");
        }
    }
    else
    {
        /* Print error and terminate.  */
        fprintf(stderr,
                "INFO: communications error with Services Layer Application process!!  Terminating...\n\n");
        exit(-1);
    }

    return retValue;
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommSendDownlink() - this routine is used to send a mail message to the Services Layer    *
 *                           Application process.  This routine will take the mail and reply      *
 *                           information and form it into an IPC packet.  The IPC packet is then  *
 *                           encapsulated in a simple framing protocol and sent to the Services   *
 *                           Layer Application via the communications channel, handling           *
 *                           interrupted transmissions as necessary.  If an unrecoverable error   *
 *                           occurs, the communications channel will be closed.                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_mail_details_t*       p_mailDetails - a pointer to mail which is to be sent to the    *
 *                                                Services Layer Application process.             *
 *      kal_mail_unique_handle_t  h_unique      - the unique handle associated with the mail.     *
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
 *      true  - upon successful transmission of the mail.                                         *
 *      false - if an error occurred.                                                             *
 *                                                                                                *
 **************************************************************************************************/
static bool
APP_CommSendDownlink(kal_mail_details_t*       p_mailDetails,
                     kal_mail_unique_handle_t  h_unique)
{
    u16_t                 ipcPktLength;
    appComm_IpcPacket_t*  p_pkt;
    bool                  retValue=false;

    ipcPktLength = sizeof(appComm_IpcPacket_t) + (u16_t) p_mailDetails->ParameterLength - sizeof(u32_t);
    p_pkt = (appComm_IpcPacket_t*) KAL_Malloc(ipcPktLength);
    if (p_pkt != NULL)
    {
        KAL_Memset(p_pkt, 0, ipcPktLength);
        p_pkt->hdr.parmBlockLength = p_mailDetails->ParameterLength;
        p_pkt->hdr.mailId          = p_mailDetails->MailId;
        p_pkt->hdr.h_uniqueVal     = h_unique;
        KAL_Memcpy(&(p_pkt->parmBlock), p_mailDetails->pParameterBlock, p_mailDetails->ParameterLength);
        if (APP_CommBlockingWrite((u8_t*) &ipcPktLength, APP_COMM_LENGTH_OF_FRAME_HEADER))
        {
            retValue = APP_CommBlockingWrite((u8_t*) p_pkt, ipcPktLength);
        }
        KAL_Free(p_pkt);
    }
    else
    {
        fprintf(stderr, "ERROR: APP_CommSendDownlink(): dynamic memory failure!!\n\n");
    }

    return retValue;
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommBlockingRead() - this routine performs a "blocking" read on the communications        *
 *                           channel between this Application process and the Services Layer      *
 *                           process.  This routine will block until the specified number of      *
 *                           bytes has been read, or until an error occurs.                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t*        p_buffer  - a pointer to a buffer where the data will be stored.             *
 *      const u16_t  numOctets - the number of bytes (octets) to read.                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      appCommCommDescriptor                                                                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      i32_t: -1        - an error occurred.  errno will be set to an appropriate value.         *
 *             -2        - if the peer closed the connection.                                     *
 *             -3        - if the communications channel was already closed.                      *
 *             numOctets - upon success.  This value is greater than or equal to zero.            *
 *                                                                                                *
 **************************************************************************************************/
static i32_t
APP_CommBlockingRead(u8_t*  p_buffer,
                     u16_t  numOctets)
{
    i32_t  bytesRemaining;
    i32_t  bytesRead;

    if (appCommCommDescriptor != -1)
    {
        if (numOctets != 0)
        {
            bytesRemaining = numOctets;
            while (bytesRemaining > 0)
            {
                bytesRead = recv(appCommCommDescriptor, p_buffer, bytesRemaining, 0);
                if (bytesRead == 0)
                {
                    /* Socket was closed by peer.  */
                    APP_CommError();
                    return -2;
                }
                else if (bytesRead > 0)
                {
                    p_buffer += bytesRead;
                    bytesRemaining -= bytesRead;
                }
                else
                {
                    if (errno != EINTR)
                    {
                        APP_CommError();
                        return -1;
                    }
                }
            }
        }
    }
    else
    {
        return -3;
    }

    return numOctets;
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommBlockingWrite() - this routine performs a blocking write to the communications        *
 *                            channel between this Application process and the Services Layer     *
 *                            process.  This routine will block until all of the data has been    *
 *                            sent or an error occurs.                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t*  p_buffer  - a pointer to data which will be written.                               *
 *      u16_t  numOctets - the number of octets to write.                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      appCommCommDescriptor                                                                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      true  - if all of the data was written.                                                   *
 *      false - if an error occurred.                                                             *
 *                                                                                                *
 **************************************************************************************************/
static bool
APP_CommBlockingWrite(u8_t*  p_buffer,
                      u16_t  numOctets)
{
    i32_t  bytesWritten;
    i32_t  bytesRemaining;

    if (appCommCommDescriptor != -1)
    {
        bytesRemaining = numOctets;

        /* Remain in loop until error or everything has been sent.  */
        while (bytesRemaining > 0)
        {
            bytesWritten = send(appCommCommDescriptor, p_buffer, bytesRemaining, 0);
            if (bytesWritten == 0)
            {
                /* The peer shut down the connection.  */
                APP_CommError();
                return false;
            }
            else if (bytesWritten > 0)
            {
                p_buffer += bytesWritten;
                bytesRemaining -= bytesWritten;
            }
            else
            {
                if (errno != EINTR)
                {
                    APP_CommError();
                    return false;
                }
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommError() - this routine is called when an unrecoverable communications error occurs.   *
 *                    It will close both the listening socket as well as the communications       *
 *                    socket, if existent.                                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      appCommListeningDescriptor                                                                *
 *      appCommCommDescriptor                                                                     *
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
APP_CommError(void)
{
    /* Close the listening socket, if extant.  */
    if (appCommListeningDescriptor != -1)
    {
        close(appCommListeningDescriptor);
        appCommListeningDescriptor = -1;
    }

    /* Close the communications socket, if extant.  */
    if (appCommCommDescriptor != -1)
    {
        close(appCommCommDescriptor);
        appCommCommDescriptor = -1;
    }
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommSetUpSigchldHandler() - this routine sets up a signal handler for SIGCHLD.  The child *
 *                                  process is the Services Layer process.  There are, in         *
 *                                  general, two types of situations when SIGCHLD is sent:        *
 *                                                                                                *
 *                                      1) Expected termination: if the Services Layer process is *
 *                                         deliberately terminated, possibly as part of an        *
 *                                         orderly shutdown.  In this case, SIGCHLD can be used   *
 *                                         as a confirmation of termination.                      *
 *                                      2) If the child process terminates unexpectedly (e.g.,    *
 *                                         segmentation fault), the parent process will be        *
 *                                         notified.                                              *
 *                                                                                                *
 *                                  This routine only sets up a signal handler.  It does not      *
 *                                  specify what the signal handler does upon receipt of SIGCHLD. *
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
 *      bool: true  - if the signal handler was installed.                                        *
 *            false - upon failure to install the SIGCHLD handler.                                *
 *                                                                                                *
 **************************************************************************************************/
static bool
APP_CommSetUpSigchldHandler(void)
{
    struct sigaction  sa;

    KAL_Memset(&sa, 0, sizeof(sa));
    sa.sa_handler = APP_CommSigchldHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        fprintf(stderr, "ERROR: unable to install SIGCHLD handler; %s\n\n", strerror(errno));
        return false;
    }

    return true;
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommSigchldHandler() - this routine is the signal handler for SIGCHLD.  This is used to   *
 *                             detect child termination prior to communications channel           *
 *                             connection, in which case it will terminate the process.  If       *
 *                             received after a connection has been made, it will ignore the      *
 *                             signal and allow the application to detect a break in the          *
 *                             communications channel.  Once detected, the application will       *
 *                             self-terminate.                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      int  signal - the signal causing the handler being invoked.                               *
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
APP_CommSigchldHandler(int  signal)
{
    if (appCommConnected == false)
    {
        /* Termination the application.  */
        fprintf(stderr,
                "INFO: signal SIGCHLD received prior to communications channel establishment.  Terminating...\n");
        exit(-1);
    }

    fprintf(stderr, "INFO: signal SIGCHLD received.  Ignoring signal...\n");
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommUplink() - this routine is the entry point of a KAL task used to receive data from    *
 *                     the communications channel to the Services Layer process.  The data        *
 *                     received is un-encapsulated, and is sent as mail to the SL_TaskHandle      *
 *                     task.  The SL_TaskHandle is another name for the gh_taskDownlink task.  It *
 *                     is the responsibility of the gh_taskDownlink task to repackage the IPC     *
 *                     packet and to send mail to the APP_TaskHandle task.                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  p_dummy - a pointer to (unused) task startup parameters.                           *
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
APP_CommUplink(void*  p_dummy)
{
    kal_message_details_t  msgDetails;
    kal_mail_details_t     mailDetails;
    appComm_IpcPacket_t*   p_ipcPkt;
    u16_t                  ipcPktLength;
    kal_event_t            eventMask;

    (void) KAL_SwdRegister(KAL_SWD_TASKTYPE_NONE);

    /* Wait for the KAL system start event.  */
    (void) KAL_TaskEventWait(KAL_START_EVENT, 0);

    /* Wait for the Mode Manager to instruct the task to enter NORMAL mode.  */
    (void) KAL_TaskEventWait(KAL_MM_ENTER_NORMAL_PROCESSING_EVENT, 0);

    /* Infinite loop waiting to receive data from the communications channel.  */
    while (1)
    {
        /* This task should never receive mail.  It waits for incoming IPC packets over the
             communications channel, then sends them to APP_TaskHandle.  For robustness, a check
             is made to read the task's events, but only prints a warning message if an event is
             received.  Although the timeout for checking events is set to the minimum, this task
             will actually spend the vast majority of its time blocked, waiting for the incoming
             packets. */
        eventMask = KAL_TaskEventWait(KAL_ALL_EVENTS, 1);
        if (eventMask)
        {
            if (eventMask & ~KAL_EVENT_TIMEOUT_EVENT)
            {
                /* Print out a warning message.  */
                fprintf(stderr, "WARNING: Uplink task received events: 0x%016llX\n", eventMask);

                /* Check if mail was received.  If so, perform a blind read of the mail to avoid
                     a memory leak.  */
                if (eventMask & KAL_MESSAGE_EVENT)
                {
                    KAL_MailReceive(&msgDetails, &mailDetails);
                }
            }
        }

        /* Perform blocking read for IPC packets.  */
        if (APP_CommGetUplink(&p_ipcPkt, &ipcPktLength))
        {
            /* Now, send the mail to the SL_TaskHandle task.  */
            mailDetails.pParameterBlock = p_ipcPkt;
            mailDetails.ParameterLength = ipcPktLength;
            mailDetails.iPartnerTask    = gh_taskDownlink;
            mailDetails.MailId          = APP_COMM_IPC_UPLINK_IND;
            KAL_MailSend(NULL, &mailDetails);

            /* Free the dynamic memory associated with the IPC packet.  */
            KAL_Free(p_ipcPkt);
            p_ipcPkt = NULL;
        }
        else
        {
            fprintf(stderr,
                    "INFO: communications error with Services Layer Application process!!  Terminating...\n\n");
            exit(-1);
        }
    }
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommDownlink() - this routine is the entry point of a KAL task used to mimic the Services *
 *                       Layer task which actually runs within the Services Layer process.  This  *
 *                       task will send mail received from the APP_TaskHandle to the Services     *
 *                       Layer process via the communications channel.  It will also receive mail *
 *                       from the gh_taskUplink task which contains IPC packet information.  This *
 *                       packet will be reformatted and sent to the APP_TaskHandle task using the *
 *                       KAL_MailSendReply() routine.                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  p_dummy - a pointer to (unused) task startup parameters.                           *
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
APP_CommDownlink(void*  p_dummy)
{
    kal_mail_details_t        mailDetails;
    kal_message_details_t     msgDetails;
    kal_mail_unique_handle_t  h_unique;
    kal_event_t               eventMask;

    /* Register the task as a Synchronous Mail replier.  */
    (void) KAL_MailSyncRegister(KAL_MAILSYNC_TASKTYPE_REPLIER);
    (void) KAL_SwdRegister(KAL_SWD_TASKTYPE_NONE);

    /* Wait for the KAL system start event.  */
    (void) KAL_TaskEventWait(KAL_START_EVENT, 0);

    /* Wait for the Mode Manager to instruct the task to enter NORMAL mode.  */
    (void) KAL_TaskEventWait(KAL_MM_ENTER_NORMAL_PROCESSING_EVENT, 0);

    /* Infinite loop waiting to receive data from the communications channel.  */
    while (1)
    {
        eventMask = KAL_TaskEventWait(KAL_ALL_EVENTS, 0);
        if (eventMask & KAL_MESSAGE_EVENT)
        {
            if (KAL_MailSyncReceive(&msgDetails, &mailDetails, &h_unique) == G2_SUCCESS)
            {
                if (mailDetails.iPartnerTask == APP_TaskHandle)
                {
                    if (!APP_CommSendDownlink(&mailDetails, h_unique))
                    {
                        fprintf(stderr,
                                "INFO: communications error with Services Layer process!!  Terminating...\n\n");
                        exit(-1);
                    }
                }
                else if (mailDetails.iPartnerTask == gh_taskUplink)
                {
                    PSAL_ASSERT(mailDetails.MailId == APP_COMM_IPC_UPLINK_IND);

                    /* All encapsulated IPC packets do not use the unique handle.  Instead, the
                         unique handle is encapsulated as part of the IPC packet itself, and the
                         IPC packet is carried in the mail's parameter block.  */
                    APP_CommSendUplinkToApp(mailDetails.pParameterBlock,
                                            mailDetails.ParameterLength);
                }
                else
                {
                    fprintf(stderr,
                            "WARNING: received mail from unknown entity (0x%02X sent 0x%04hX).  Ignoring...\n",
                            mailDetails.iPartnerTask, mailDetails.MailId);
                }
            }
        }
    }
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommSendUplinkToApp() - this routine is used to send uplink mail which is encapsulated in *
 *                              an IPC packet to the APP_TaskHandle.  It will unencapsulate the   *
 *                              mail and examine the reply information.  If there is reply        *
 *                              information, this routine will send mail up to APP_TaskHandle     *
 *                              using KAL_MailSendReply(); otherwise, KAL_MailSend() will be      *
 *                              used.                                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      appComm_IpcPacket_t*  p_ipcPkt  - a pointer to the IPC packet which, after converting,    *
 *                                        will be sent to APP_TaskHandle as mail.                 *
 *      u16_t                 pktLength - the length of the IPC packet.                           *
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
APP_CommSendUplinkToApp(appComm_IpcPacket_t*  p_ipcPkt,
                        u16_t                 pktLength)
{
    kal_mail_details_t        mailDetails;

    KAL_Memset(&mailDetails, 0, sizeof(mailDetails));

    mailDetails.ParameterLength = p_ipcPkt->hdr.parmBlockLength;
    mailDetails.iPartnerTask    = APP_TaskHandle;
    mailDetails.MailId          = p_ipcPkt->hdr.mailId;
    if (mailDetails.ParameterLength == 0)
    {
        mailDetails.pParameterBlock = NULL;
    }
    else
    {
        mailDetails.pParameterBlock = p_ipcPkt->parmBlock;
    }

    /* Since this task is a synchronous mail replier, use a synchronous mail reply, even if the
         mail is asynchronous.  */
    KAL_MailSyncSendReply(NULL, &mailDetails, p_ipcPkt->hdr.h_uniqueVal);
}








/**************************************************************************************************
 *                                                                                                *
 *  APP_CommGetFilename() - given a pathname, this routine returns the filename portion.          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      const char*  p_pathname - a pointer to a string containing a pathname.                    *
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
static const char*
APP_CommGetFilename(const char*  p_pathname)
{
    i16_t  index;

    index = strlen(p_pathname) - 1;
    while ((index >= 0) && (p_pathname[index] != '/'))
    {
        index--;
    }
    index++;

    return p_pathname+index;
}









