/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *                                                                                                *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  klnx_message.c                                                                                *
 *                                                                                                *
 *  This file contains the Linux implementation of the Kernel Abstraction Layer's mail handling   *
 *  and message passing package.  Information pertaining to a message is split between a hidden   *
 *  control block and data which is owned by the user (in a kal_message_details_t data            *
 *  structure).                                                                                   *
 *                                                                                                *
 *  Mail handling always involves parameter information but message content is optional.  If a    *
 *  message is "sent" along with parameter information in mail (heretofore called a mail message, *
 *  different than a simple "message"), the message is not copied.  Instead, a handle to the      *
 *  actual message is sent.  Because mail handling requires access to the auxiliary task control  *
 *  blocks, data structures pertaining to mail will not be placed within this file and will be    *
 *  placed within klnx_internal.h.                                                                *
 *                                                                                                *
 *  Therefore, in an effort to restrict access to where it is necessary, type definitions are     *
 *  made in three locations:                                                                      *
 *                                                                                                *
 *      klnx_message.c (this file)                                                                *
 *      klnx_internal.h                                                                           *
 *      kal_api.h                                                                                 *
 *                                                                                                *
 *  A fourth location, kal_private_api.h, is used for symbolic constants, macros, and function    *
 *  prototypes which are used by Grid2Home software but should not be given to customers.         *
 *                                                                                                *
 **************************************************************************************************/


#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "psal_assert.h"
#include "kal_api.h"
#include "kal_config.h"
#include "kal_private_api.h"
#include "klnx_internal.h"








/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_MESSAGE_FALSE                     (0)
#define KAL_MESSAGE_TRUE                      (1)
#define KAL_MESSAGE_INVALID_HANDLE            (0x7FFF)
#define KAL_MAIL_NON_KAL_TASK                 (0xFF)
#define KAL_MAIL_FALSE                        (0)
#define KAL_MAIL_TRUE                         (1)
#define KAL_MAIL_MAX_MAILBOXES                KAL_CONFIG_TASK_MAX_CONCURRENT








/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef struct kal_messageCtrlBlk_s
{
    i16_t      inUse;
    i16_t      nextIndex;
    u16_t      currentOffset;
    u16_t      userLength;
    pthread_t  h_ownerThread;
    u8_t*      p_msgBuffer;
    u16_t      msgBufferLength;
}  kal_messageCtrlBlk_t;








/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
static kal_messageCtrlBlk_t  kalMessageMcbs[KAL_CONFIG_MESSAGE_MAX_CONCURRENT];
static i16_t                 kalMessageFreeIndex;
static sem_t                 kalMessageSemaphoreLock;
static u8_t                  kalMailInitialized;
static kal_mailQueue_t       kalMailQueues[KAL_MAIL_MAX_MAILBOXES];
static u8_t                  kalMailNumTasks=KAL_MESSAGE_FALSE;



// Logging
#if KAL_MESSAGE_LOG_ENABLE
static u32_t nMessage = 0;
#endif //KAL_MESSAGE_LOG_ENABLE




/**************************************************************************************************
 *                                                                                                *
 *  Local Function Declarations                                                                   *
 *                                                                                                *
 **************************************************************************************************/
static void  
KAL_MessageGetLock(void);

static void  
KAL_MessageReleaseLock(void);

static void  
KAL_MailGetLock(u8_t  h_mailbox);

static void  
KAL_MailReleaseLock(u8_t  h_mailbox);

static u8_t
KAL_MailReceiveFromAsync(kal_message_details_t*     p_msgDetails,
                         kal_mail_details_t*        p_mailDetails,
                         kal_mail_unique_handle_t*  ph_unique);

static void
KAL_MailSyncIncrementUniqueHandle(volatile kal_mail_unique_handle_t*  ph_unique);








/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageInit() - this routine is used to initialize the Kernel Abstraction Layer's mail    *
 *                      handling and message passing package.  This should only be invoked once   *
 *                      at the startup of the application.                                        *
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
KAL_MessageInit(void)
{
    i16_t  index;

    for (index=0; index<KAL_CONFIG_MESSAGE_MAX_CONCURRENT; index++)
    {
        kalMessageMcbs[index].inUse = KAL_MESSAGE_FALSE;
        kalMessageMcbs[index].nextIndex = index + 1;
        kalMessageMcbs[index].currentOffset = 0;
        kalMessageMcbs[index].userLength = 0;
        memset(&kalMessageMcbs[index].h_ownerThread, 0, sizeof(pthread_t));
        kalMessageMcbs[index].p_msgBuffer = NULL;
        kalMessageMcbs[index].msgBufferLength = 0;
    }

    kalMessageMcbs[KAL_CONFIG_MESSAGE_MAX_CONCURRENT-1].nextIndex = KAL_MESSAGE_INVALID_HANDLE;
    kalMessageFreeIndex = 0;

    sem_init(&kalMessageSemaphoreLock, 0, 1);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageAllocate() - this routine is called to allocate a message with specified initial   *
 *                          offset and maximum length.  Actually, three separate activities are   *
 *                          involved.  First, a message control block is obtained.  Second, a     *
 *                          buffer is dynamically allocated and linked into the control block.    *
 *                          Third, fields are initialized in both the MCB and the user's          *
 *                          information block.                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      size_t                  maximumLength - the (expected) maximum length of the message.     *
 *                                              This should attempt to take header and trailer    *
 *                                              sizes of lower-layer entities into account.       *
 *      size_t                  initialOffset - the initial offset.  This is typically the sum of *
 *                                              the (expected) maximum header lengths added by    *
 *                                              lower layers.                                     *
 *      size_t                  initialLength - the initial length of the message.                *
 *      kal_message_details_t*  pDescriptor   - a pointer to where information about the message  *
 *                                              will be stored.                                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_INVALID_LENGTH  - if the initial offset and length exceed the maximum size.      *
 *            G2_ALLOCATION_FAIL - if there are no free message buffers available.                *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageAllocate(size_t                  maximumLength,
                    size_t                  initialOffset,
                    size_t                  initialLength,
                    kal_message_details_t*  pDescriptor)
{
    u8_t   retCode=G2_INVALID_LENGTH;
    u8_t*  p_msgBuffer;

    if ((initialOffset + initialLength) <= maximumLength)
    {
        /* Obtain the lock for the message control blocks.  */
        KAL_MessageGetLock();

        if (kalMessageFreeIndex != KAL_MESSAGE_INVALID_HANDLE)
        {
            if (kalMessageMcbs[kalMessageFreeIndex].inUse == KAL_MESSAGE_FALSE)
            {
                p_msgBuffer = (u8_t*) malloc(maximumLength);
                if (p_msgBuffer != NULL)
                {
#if KAL_MESSAGE_LOG_ENABLE
                    fprintf(stdout, "<%s >KAL_MessageAllocate: Message# %u, Index=%d\n",
                            SYS_NAME, ++nMessage, kalMessageFreeIndex);
#endif //KAL_MESSAGE_LOG_ENABLE
                    /* Fill in (hidden) control block data.  */
                    kalMessageMcbs[kalMessageFreeIndex].inUse = KAL_MESSAGE_TRUE;
                    kalMessageMcbs[kalMessageFreeIndex].currentOffset = (u16_t)initialOffset;
                    kalMessageMcbs[kalMessageFreeIndex].userLength = (u16_t)initialLength;
                    kalMessageMcbs[kalMessageFreeIndex].h_ownerThread = pthread_self();
                    kalMessageMcbs[kalMessageFreeIndex].p_msgBuffer = p_msgBuffer;
                    kalMessageMcbs[kalMessageFreeIndex].msgBufferLength = (u16_t)maximumLength;

                    /* Copy relevant data in fields of user-owned data.  */
                    pDescriptor->iMessage = kalMessageFreeIndex;
                    pDescriptor->MessageAddress = p_msgBuffer + initialOffset;
                    pDescriptor->MessageLength = initialLength;

                    /* Update free control block list.  */
                    kalMessageFreeIndex = kalMessageMcbs[kalMessageFreeIndex].nextIndex;

                    retCode = G2_SUCCESS;
                }
                else
                {
                    retCode = G2_ALLOCATION_FAIL;
                }
            }
            else
            {
                fprintf(stderr, "ERROR: KAL_MessageAllocate() has in-use MCB on the free list!!\n");
            }
        }
        else
        {
            retCode = G2_ALLOCATION_FAIL;
        }

        /* Release the lock.  */
        KAL_MessageReleaseLock();
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageFree() - this routine is used to deallocate a message buffer and return its        *
 *                      associated control block back to the system.  The user data will also be  *
 *                      cleared.                                                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pDetails - a pointer to the user's message details, obtained      *
 *                                         either when the message was created or when the        *
 *                                         message was received.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS      - upon success.                                                     *
 *            G2_BAD_HTASK    - error when the calling task does not own the message.  In this    *
 *                              case, the message will not be deleted and an error message will   *
 *                              be printed.                                                       *
 *            G2_BAD_HMESSAGE - error if the message handle was invalid.                          *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageFree(kal_message_details_t*  pDetails)
{
    u8_t  retCode=G2_BAD_HMESSAGE;
    i16_t h_message;

    h_message = pDetails->iMessage;
    KAL_MessageGetLock();
    if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
    {
        //if (pthread_equal(kalMessageMcbs[h_message].h_ownerThread, pthread_self()) != 0)
      if (1)  // [CL]: This check is also commented out in KBP, fix the usage issue here when it gets fixed in KBP
        {
            /* The message is valid, and the caller is the owner.  Go ahead and delete the
                 message's buffer.  */
            free(kalMessageMcbs[h_message].p_msgBuffer);

            /* Clear the fields in the associated message control block.  */
            kalMessageMcbs[h_message].inUse = KAL_MESSAGE_FALSE;
            kalMessageMcbs[h_message].currentOffset = 0;
            kalMessageMcbs[h_message].userLength = 0;
            memset(&kalMessageMcbs[h_message].h_ownerThread, 0,
                   sizeof(kalMessageMcbs[h_message].h_ownerThread));
            kalMessageMcbs[h_message].p_msgBuffer = NULL;
            kalMessageMcbs[h_message].msgBufferLength = 0;

            /* Finally, place the message control block back on the free list.  */
            kalMessageMcbs[h_message].nextIndex = kalMessageFreeIndex;
            kalMessageFreeIndex = h_message;

            /* Be nice and clear the user's message details.  */
            pDetails->MessageAddress = NULL;
            pDetails->MessageLength = 0;
            pDetails->iMessage = KAL_MESSAGE_INVALID_HANDLE;

#if KAL_MESSAGE_LOG_ENABLE
            fprintf(stdout, "<%s >KAL_MessageFree: Message# %u, Index=%d\n",
                    SYS_NAME, --nMessage, h_message);
#endif //KAL_MESSAGE_LOG_ENABLE
        }
        else
        {
            /* The calling task is not the owner of the message.  */
            fprintf(stderr,
                    "ERROR: the caller of KAL_MessageFree() is not the owner of the message!!\n");
            retCode = G2_BAD_HTASK;
        }
    }
    KAL_MessageReleaseLock();

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageMoveDesc() - this routine is used to move a message descriptor (i.e., the          *
 *                          kal_message_details_t) from one location to another, then             *
 *                          invalidating the original copy.                                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  p_dest - the location to where the message descriptor should be   *
 *                                       moved.                                                   *
 *      kal_message_details_t*  p_src  - the location where the original message descriptor       *
 *                                       resides.                                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HMESSAGE    - error if the message descriptor is invalid, or if the pointer  *
 *                                 to the associated message is NULL.                             *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageMoveDesc(kal_message_details_t*  p_dest,
                    kal_message_details_t*  p_src)
{
    /* Check of the source descriptor is valid.  */
    if (p_src == NULL)
    {
        return G2_BAD_HMESSAGE;
    }

    /* Check if the message specified by the source descriptor is valid.  */
    if ((p_src->MessageAddress == NULL) || (p_src->iMessage >= KAL_CONFIG_MESSAGE_MAX_CONCURRENT))
    {
        return G2_BAD_HMESSAGE;
    }

    /* Move the contents of the message descriptor from the source to the destination.  */
    *p_dest = *p_src;

    /* Invalidate the original message descriptor.  */
    p_src->MessageAddress = NULL;
    p_src->MessageLength = 0;
    p_src->iMessage = KAL_MESSAGE_INVALID_HANDLE;
    return G2_SUCCESS;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageDuplicate() - this routine is used to duplicate a message.  A message consists of  *
 *                           a message control block and a piece of dynamic memory which is       *
 *                           linked to the control block.  To duplicate a message, a new control  *
 *                           block will be obtained (if possible), and a new piece of dynamic     *
 *                           memory will be allocated and linked to the new control block.  The   *
 *                           entire contents of dynamic memory will be copied from the source to  *
 *                           the destination, not just the user portion.                          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pMsgDTo   - the address of a structure containing the user's      *
 *                                          message details.  This parameter may not be NULL, and *
 *                                          this structure will be overwritten.                   *
 *      kal_message_details_t*  pMsgDFrom - a pointer to the user's message details which will be *
 *                                          duplicated.  This parameter cannot be NULL.           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HMESSAGE    - error if the message handle of the message to duplicate was    *
 *                                 invalid.                                                       *
 *            G2_INVALID_LENGTH  - error if the header size causes the buffer to exceed the limit *
 *                                 for an allocation.  The allocation limit value is defined in   *
 *                                 kal_memory.c as kalMemoryMaxAllocate.                          *
 *            G2_ALLOCATION_FAIL - error if the new size cannot be allocated.                     *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageDuplicate(kal_message_details_t*  pMsgDTo,
                     kal_message_details_t*  pMsgDFrom)
{
    u8_t*  p_msgBuffer;
    u8_t   retCode=G2_BAD_HMESSAGE;
    i16_t  h_message;

    if (pMsgDFrom != NULL)
    {
        h_message = pMsgDFrom->iMessage;
        if (h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT)
        {
            KAL_MessageGetLock();

            /* There's no need to check if the message will fit because the message already fits
                 in the source.  Check to see if there is a free message control block.  */
            if (kalMessageFreeIndex != KAL_MESSAGE_INVALID_HANDLE)
            {
                PSAL_ASSERT(kalMessageMcbs[kalMessageFreeIndex].inUse == KAL_MESSAGE_FALSE);

                p_msgBuffer = (u8_t*) malloc(kalMessageMcbs[h_message].msgBufferLength);
                if (p_msgBuffer != NULL)
                {
                    /* Fill in (hidden) control block data.  Most of it is copied from the source
                         message control block, but the pointer to dynamic memory is different.  */
                    kalMessageMcbs[kalMessageFreeIndex].inUse = KAL_MESSAGE_TRUE;
                    kalMessageMcbs[kalMessageFreeIndex].currentOffset = 
                        kalMessageMcbs[h_message].currentOffset;
                    kalMessageMcbs[kalMessageFreeIndex].userLength = 
                        kalMessageMcbs[h_message].userLength;
                    kalMessageMcbs[kalMessageFreeIndex].h_ownerThread = pthread_self();
                    kalMessageMcbs[kalMessageFreeIndex].p_msgBuffer = p_msgBuffer;
                    kalMessageMcbs[kalMessageFreeIndex].msgBufferLength = 
                        kalMessageMcbs[h_message].msgBufferLength;

                    /* Copy the dynamic memory buffer from the source to the destination.  */
                    memcpy(kalMessageMcbs[kalMessageFreeIndex].p_msgBuffer,
                           kalMessageMcbs[h_message].p_msgBuffer,
                           kalMessageMcbs[h_message].msgBufferLength);

                    /* Copy relevant data from the new message control block to the user-owned
                         data structure.  */
                    pMsgDTo->iMessage = kalMessageFreeIndex;
                    pMsgDTo->MessageAddress = p_msgBuffer + 
                        kalMessageMcbs[kalMessageFreeIndex].currentOffset;
                    pMsgDTo->MessageLength = kalMessageMcbs[kalMessageFreeIndex].userLength;

                    /* Update the free control block list.  */
                    kalMessageFreeIndex = kalMessageMcbs[kalMessageFreeIndex].nextIndex;

                    retCode = G2_SUCCESS;
                }
                else
                {
                    retCode = G2_ALLOCATION_FAIL;

                    /* Although the caller should handle this return code properly, be nice and
                         invalidate the values in the user's message details.  */
                    pMsgDTo->iMessage = KAL_MESSAGE_INVALID_HANDLE;
                    pMsgDTo->MessageLength = 0;
                    pMsgDTo->MessageAddress = NULL;
                }
            }
            else
            {
                retCode = G2_ALLOCATION_FAIL;
            }
            KAL_MessageReleaseLock();
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageAddHeader() - this routine is used to increase the length of a message by          *
 *                           obtaining room for a header.  Because there might already be         *
 *                           existing space (the user's message does not necessarily begin at the *
 *                           beginning of the buffer), the offset may be decreased to make the    *
 *                           space available.  However, if there is not enough space for a        *
 *                           header, this routine will allocate a new buffer with sufficient      *
 *                           space.                                                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pDetails  - a pointer to the user's message details, obtained     *
 *                                          either when the message was created or when the       *
 *                                          message was received.                                 *
 *      size_t                  HdrLength - the length of the header to be added.                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HTASK       - error when the calling task does not own the message.  In this *
 *                                 case, the message will not be deleted and an error message     *
 *                                 will be printed.                                               *
 *            G2_BAD_HMESSAGE    - error if the message handle was invalid.                       *
 *            G2_INVALID_LENGTH  - error if the header size causes the buffer to exceed the limit *
 *                                 for an allocation.  The allocation limit value is defined in   *
 *                                 kal_memory.c as kalMemoryMaxAllocate.                          *
 *            G2_ALLOCATION_FAIL - error if the new size cannot be allocated.                     *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageAddHeader(kal_message_details_t*  pDetails,
                     size_t                  HdrLength)
{
    u8_t   retCode=G2_BAD_HMESSAGE;
    i16_t  h_message;
    u8_t*  p_newBuf;
    u16_t  newSize;

    h_message = pDetails->iMessage;
    KAL_MessageGetLock();
    if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
    {
        if (pthread_equal(kalMessageMcbs[h_message].h_ownerThread, pthread_self()) != 0)
        {
            /* Check to see if there is sufficient space in the existing buffer.  */
            if (HdrLength <= kalMessageMcbs[h_message].currentOffset)
            {
                kalMessageMcbs[h_message].currentOffset -= (u16_t)HdrLength;
                kalMessageMcbs[h_message].userLength += (u16_t)HdrLength;
                pDetails->MessageAddress = kalMessageMcbs[h_message].p_msgBuffer +
                                               kalMessageMcbs[h_message].currentOffset;
                pDetails->MessageLength = kalMessageMcbs[h_message].userLength;

                retCode = G2_SUCCESS;
            }
            else
            {
                /* Need to reallocate a buffer.  Also, check the size of the new buffer.  */
                newSize = kalMessageMcbs[h_message].msgBufferLength -
                              kalMessageMcbs[h_message].currentOffset + (u16_t)HdrLength;
                if (newSize <= kalMemoryMaxAllocate)
                {
                    p_newBuf = (u8_t*) malloc(newSize);
                    if (p_newBuf != NULL)
                    {
                        /* Now, copy data from old buffer to new buffer.  */
                        memcpy(p_newBuf+HdrLength,
                               kalMessageMcbs[h_message].p_msgBuffer+kalMessageMcbs[h_message].currentOffset,
                               kalMessageMcbs[h_message].userLength);

                        /* Delete the old buffer.  */
                        free(kalMessageMcbs[h_message].p_msgBuffer);

                        /* Update the message control block information.  */
                        kalMessageMcbs[h_message].p_msgBuffer = p_newBuf;
                        kalMessageMcbs[h_message].msgBufferLength = newSize;
                        kalMessageMcbs[h_message].currentOffset = 0;
                        kalMessageMcbs[h_message].userLength += (u16_t)HdrLength;

                        /* Update the user's message details.  */
                        pDetails->MessageAddress = kalMessageMcbs[h_message].p_msgBuffer +
                                                       kalMessageMcbs[h_message].currentOffset;
                        pDetails->MessageLength = kalMessageMcbs[h_message].userLength;

                        retCode = G2_SUCCESS;
                    }
                    else
                    {
                        retCode = G2_ALLOCATION_FAIL;
                    }
                }
                else
                {
                    retCode = G2_INVALID_LENGTH;
                }
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }
    KAL_MessageReleaseLock();

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageRemoveHeader() - this routine is used to adjust the message control block and      *
 *                              user's message details as if message header data was removed.     *
 *                              This function will only change pointer and offset values; it      *
 *                              will not actually shrink the underlying buffer.                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pDetails  - a pointer to the user's message details, obtained     *
 *                                          either when the message was created or when the       *
 *                                          message was received.                                 *
 *      size_t                  HdrLength - the length of the header to be removed.               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HTASK       - error when the calling task does not own the message.  In this *
 *                                 case, the message will not be deleted and an error message     *
 *                                 will be printed.                                               *
 *            G2_BAD_HMESSAGE    - error if the message handle was invalid.                       *
 *            G2_INVALID_LENGTH  - error if the header size exceeds the length of the message.    *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageRemoveHeader(kal_message_details_t*  pDetails,
                        size_t                  HdrLength)
{
    u8_t   retCode=G2_BAD_HMESSAGE;
    i16_t  h_message;

    h_message = pDetails->iMessage;
    KAL_MessageGetLock();
    if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
    {
        if (pthread_equal(kalMessageMcbs[h_message].h_ownerThread, pthread_self()) != 0)
        {
            if (HdrLength <= kalMessageMcbs[h_message].userLength)
            {
                kalMessageMcbs[h_message].currentOffset += (u16_t)HdrLength;
                kalMessageMcbs[h_message].userLength -= (u16_t)HdrLength;

                pDetails->MessageAddress = kalMessageMcbs[h_message].p_msgBuffer +
                                               kalMessageMcbs[h_message].currentOffset;
                pDetails->MessageLength = kalMessageMcbs[h_message].userLength;

                retCode = G2_SUCCESS;
            }
            else
            {
                retCode = G2_INVALID_LENGTH;
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }
    KAL_MessageReleaseLock();

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageAddTrailer() - this routine is used to increase the length of a message by         *
 *                            obtaining room for a trailer.  Because there might already be       *
 *                            existing space (the user's message does not necessarily end at the  *
 *                            end of the buffer), the user's length may be increased to make the  *
 *                            space available.  However, if there is not enough space for a       *
 *                            trailer, this routine will allocate a new buffer with sufficient    *
 *                            space.                                                              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pDetails      - a pointer to the user's message details, obtained *
 *                                              either when the message was created or when the   *
 *                                              message was received.                             *
 *      size_t                  TrailerLength - the length of the trailer to be added.            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HTASK       - error when the calling task does not own the message.  In this *
 *                                 case, the message will not be deleted and an error message     *
 *                                 will be printed.                                               *
 *            G2_BAD_HMESSAGE    - error if the message handle was invalid.                       *
 *            G2_INVALID_LENGTH  - error if the trailer size causes the buffer to exceed the      *
 *                                 limit for an allocation.  The allocation limit value is        *
 *                                 defined in kal_memory.c as kalMemoryMaxAllocate.               *
 *            G2_ALLOCATION_FAIL - error if the new size cannot be allocated.                     *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageAddTrailer(kal_message_details_t*  pDetails,
                      size_t                  TrailerLength)
{
    u8_t   retCode=G2_BAD_HMESSAGE;
    i16_t  h_message;
    u8_t*  p_newBuf;
    u16_t  newSize;

    h_message = pDetails->iMessage;
    KAL_MessageGetLock();
    if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
    {
        if (pthread_equal(kalMessageMcbs[h_message].h_ownerThread, pthread_self()) != 0)
        {
            /* Check to see if there is sufficient space in the existing buffer.  */
            newSize = kalMessageMcbs[h_message].currentOffset +
                          kalMessageMcbs[h_message].userLength + (u16_t)TrailerLength;
            if (newSize > kalMessageMcbs[h_message].msgBufferLength)
            {
                if (newSize <= kalMemoryMaxAllocate)
                {
                    p_newBuf = (u8_t*) malloc(newSize);
                    if (p_newBuf != NULL)
                    {
                        /* Copy data from the old buffer to the new buffer.  */
                        memcpy(p_newBuf + kalMessageMcbs[h_message].currentOffset,
                               kalMessageMcbs[h_message].p_msgBuffer +
                                   kalMessageMcbs[h_message].currentOffset,
                               kalMessageMcbs[h_message].userLength);

                        /* Delete the old buffer.  */
                        free(kalMessageMcbs[h_message].p_msgBuffer);

                        /* Update the message control block information.  */
                        kalMessageMcbs[h_message].p_msgBuffer = p_newBuf;
                        kalMessageMcbs[h_message].msgBufferLength = newSize;
                        kalMessageMcbs[h_message].userLength += (u16_t)TrailerLength;

                        /* Update the user's message details.  */
                        pDetails->MessageAddress = kalMessageMcbs[h_message].p_msgBuffer +
                                                       kalMessageMcbs[h_message].currentOffset;
                        pDetails->MessageLength = kalMessageMcbs[h_message].userLength;

                        retCode = G2_SUCCESS;
                    }
                    else
                    {
                        retCode = G2_ALLOCATION_FAIL;
                    }
                }
                else
                {
                    retCode = G2_INVALID_LENGTH;
                }
            }
            else
            {
                /* There is enough space, so simply increase the length values.  */
                kalMessageMcbs[h_message].userLength += (u16_t)TrailerLength;
                pDetails->MessageLength = kalMessageMcbs[h_message].userLength;
                retCode = G2_SUCCESS;
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }
    KAL_MessageReleaseLock();

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageRemoveTrailer() - this routine is used to adjust the message control block and     *
 *                               user's message details as if message trailer data was removed.   *
 *                               This function will only change pointer and offset values; it     *
 *                               will not actually shrink the underlying buffer.                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pDetails      - a pointer to the user's message details, obtained *
 *                                              either when the message was created or when the   *
 *                                              message was received.                             *
 *      size_t                  TrailerLength - the length of the trailer to be removed.          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HTASK       - error when the calling task does not own the message.  In this *
 *                                 case, the message will not be deleted and an error message     *
 *                                 will be printed.                                               *
 *            G2_BAD_HMESSAGE    - error if the message handle was invalid.                       *
 *            G2_INVALID_LENGTH  - error if the trailer size exceeds the length of the message.   *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageRemoveTrailer(kal_message_details_t*  pDetails,
                         size_t                  TrailerLength)
{
    u8_t  retCode=G2_BAD_HMESSAGE;
    i16_t h_message;

    h_message = pDetails->iMessage;
    KAL_MessageGetLock();
    if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
    {
        if (pthread_equal(kalMessageMcbs[h_message].h_ownerThread, pthread_self()) != 0)
        {
            if (TrailerLength <= kalMessageMcbs[h_message].userLength)
            {
                kalMessageMcbs[h_message].userLength -= (u16_t)TrailerLength;
                pDetails->MessageLength = kalMessageMcbs[h_message].userLength;

                retCode = G2_SUCCESS;
            }
            else
            {
                retCode = G2_INVALID_LENGTH;
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }
    KAL_MessageReleaseLock();

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageAddSpace() - this routine is used to increase the length of a message for an       *
 *                          "extension header" which is inserted into the middle of a message.    *
 *                          For example:                                                          *
 *                                                                                                *
 *                              +------+--+---------+--------+                                    *
 *                              |      |  |         |        |                                    *
 *                              +------+--+---------+--------+                                    *
 *                                     |  |         |                                             *
 *                                     |  |         message end                                   *
 *                                     |  insertion point                                         *
 *                                     message start                                              *
 *                                                                                                *
 *                           The caller wished to insert "Length" bytes "Offset" bytes from the   *
 *                           start of the message.  After the call, the buffer may look like:     *
 *                                                                                                *
 *                              +---+--+--+---------+--------+                                    *
 *                              |   |  |XX|         |        |                                    *
 *                              +---+--+--+---------+--------+                                    *
 *                                  |               |                                             *
 *                                  |               message end                                   *
 *                                  |                                                             *
 *                                  message start                                                 *
 *                                                                                                *
 *                           In the example shown, there is sufficient header space for the       *
 *                           extension header.  If there is not enough space, this routine will   *
 *                           allocate a new buffer with sufficient space.  Upon return from this  *
 *                           routine, the caller's details will have the message start and the    *
 *                           overall used message length.  The blank "hole" can be derived from   *
 *                           the message start using the "Offset" and "Length" data.              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pDetails - a pointer to the user's message details, obtained      *
 *                                         either when the message was created or when the        *
 *                                         message was received.                                  *
 *      size_t                  Offset   - the offset from the start of the user message where    *
 *                                         bytes should be "inserted".                            *
 *      size_t                  Length   - the number of bytes to insert.
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HTASK       - error when the calling task does not own the message.  In this *
 *                                 case, the message will not be deleted and an error message     *
 *                                 will be printed.                                               *
 *            G2_BAD_HMESSAGE    - error if the message handle was invalid.                       *
 *            G2_INVALID_LENGTH  - error if the header size causes the buffer to exceed the limit *
 *                                 for an allocation.  The allocation limit value is defined in   *
 *                                 kal_memory.c as kalMemoryMaxAllocate.                          *
 *            G2_ALLOCATION_FAIL - error if the new size cannot be allocated.                     *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageAddSpace(kal_message_details_t*  pDetails,
                    size_t                  Offset,
                    size_t                  Length)
{
    u8_t   retCode=G2_BAD_HMESSAGE;
    i16_t  h_message;
    u8_t*  p_newBuf;
    u16_t  newSize;
    u16_t  newOffset;

    h_message = pDetails->iMessage;
    KAL_MessageGetLock();
    if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
    {
        if (pthread_equal(kalMessageMcbs[h_message].h_ownerThread, pthread_self()) != 0)
        {
            /* Space is taken exclusively from the header.  Check to see if there is sufficient
                 room or whether a reallocation is necessary.  */
            if (Length <= kalMessageMcbs[h_message].currentOffset)
            {
                /* The Length specifies where to move existing bytes.  Offset specifies how
                     many bytes to move.  */
                newOffset = kalMessageMcbs[h_message].currentOffset - (u16_t)Length;
                memmove(kalMessageMcbs[h_message].p_msgBuffer+newOffset,
                        kalMessageMcbs[h_message].p_msgBuffer+
                            kalMessageMcbs[h_message].currentOffset,
                        Offset);
                kalMessageMcbs[h_message].currentOffset = newOffset;
                kalMessageMcbs[h_message].userLength += (u16_t)Length;
                pDetails->MessageAddress = kalMessageMcbs[h_message].p_msgBuffer +
                                               kalMessageMcbs[h_message].currentOffset;
                pDetails->MessageLength = kalMessageMcbs[h_message].userLength;

                retCode = G2_SUCCESS;
            }
            else
            {
                /* A reallocation is required.  The question is whether or not there should be
                     "extra" room for additional headers.  This issue of extra space is left to
                     the user.  However, since the user has mis-estimated the amount of room,
                     it is not up to this implementation to second-guess the user.  Hence, there
                     will be zero extra bytes for header information after the reallocation.  */
                newSize = kalMessageMcbs[h_message].msgBufferLength -
                              kalMessageMcbs[h_message].currentOffset + (u16_t)Length;
                if (newSize <= kalMemoryMaxAllocate)
                {
                    p_newBuf = (u8_t*) malloc(newSize);
                    if (p_newBuf != NULL)
                    {
                        /* Copy from the old buffer in two pieces.  */
                        memcpy(p_newBuf,
                               kalMessageMcbs[h_message].p_msgBuffer +
                                   kalMessageMcbs[h_message].currentOffset,
                               Offset);
                        memcpy(p_newBuf + Offset + Length,
                               kalMessageMcbs[h_message].p_msgBuffer +
                                   kalMessageMcbs[h_message].currentOffset + Offset,
                               kalMessageMcbs[h_message].userLength - Offset);

                        /* Delete the old buffer.  */
                        free(kalMessageMcbs[h_message].p_msgBuffer);

                        /* Update the message control block information.  */
                        kalMessageMcbs[h_message].p_msgBuffer = p_newBuf;
                        kalMessageMcbs[h_message].currentOffset = 0;
                        kalMessageMcbs[h_message].msgBufferLength = newSize;
                        kalMessageMcbs[h_message].userLength += (u16_t)Length;

                        /* Update the user's message details.  */
                        pDetails->MessageAddress = kalMessageMcbs[h_message].p_msgBuffer +
                                                       kalMessageMcbs[h_message].currentOffset;
                        pDetails->MessageLength = kalMessageMcbs[h_message].userLength;

                        retCode = G2_SUCCESS;
                    }
                    else
                    {
                        retCode = G2_ALLOCATION_FAIL;
                    }
                }
                else
                {
                    retCode = G2_INVALID_LENGTH;
                }
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }
    KAL_MessageReleaseLock();

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageRemoveSpace() - this routine is used to adjust the message control block and       *
 *                             user's message details as if data was removed from the middle of   *
 *                             the message.  The freed space will be added to the header.  This   *
 *                             function will only change pointer and offset values; it will not   *
 *                             actually shrink the underlying buffer.  To illustrate:             *
 *                                                                                                *
 *                                 +------+--+---------+--------+                                 *
 *                                 |      |  |XXXX     |        |                                 *
 *                                 +------+--+---------+--------+                                 *
 *                                        |  |         |                                          *
 *                                        |  |         message end                                *
 *                                        |  deletion point                                       *
 *                                        message start                                           *
 *                                                                                                *
 *                             will become:                                                       *
 *                                                                                                *
 *                                 +----------+--+-----+--------+                                 *
 *                                 |          |  |     |        |                                 *
 *                                 +----------+--+-----+--------+                                 *
 *                                            |        |                                          *
 *                                            |        message end                                *
 *                                            message start                                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pDetails - a pointer to the user's message details, obtained      *
 *                                         either when the message was created or when the        *
 *                                         message was received.                                  *
 *      size_t                  Offset   - the offset from the start of the user message where    *
 *                                         bytes should be "deleted".                             *
 *      size_t                  Length   - the number of bytes to delete.                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HTASK       - error when the calling task does not own the message.  In this *
 *                                 case, the message will not be deleted and an error message     *
 *                                 will be printed.                                               *
 *            G2_BAD_HMESSAGE    - error if the message handle was invalid.                       *
 *            G2_INVALID_LENGTH  - error if the header size exceeds the length of the message, or *
 *                                 if the "Offset" parameter exceeds the user data.               *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MessageRemoveSpace(kal_message_details_t*  pDetails,
                       size_t                  Offset,
                       size_t                  Length)
{
    u8_t  retCode=G2_BAD_HMESSAGE;
    i16_t h_message;

    h_message = pDetails->iMessage;
    KAL_MessageGetLock();
    if ((h_message< KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
    {
        if (pthread_equal(kalMessageMcbs[h_message].h_ownerThread, pthread_self()) != 0)
        {
            if (Length != 0)
            {
                /* At most, the user can remove from the Offset up to the end of the user data.
                   Check if the Offset and Length parameters are valid.  */
                if ((Offset + Length) <= kalMessageMcbs[h_message].userLength)
                {
                    /* Move the data.  This will destructively overwrite the extension header.  */
                    memmove(kalMessageMcbs[h_message].p_msgBuffer +
                                kalMessageMcbs[h_message].currentOffset + Length,
                            kalMessageMcbs[h_message].p_msgBuffer +
                                kalMessageMcbs[h_message].currentOffset,
                            Offset);

                    /* Update the message control block information.  */
                    kalMessageMcbs[h_message].userLength -= (u16_t)Length;
                    kalMessageMcbs[h_message].currentOffset += (u16_t)Length;

                    /* Update the user's message details.  */
                    pDetails->MessageAddress = kalMessageMcbs[h_message].p_msgBuffer +
                                                   kalMessageMcbs[h_message].currentOffset;
                    pDetails->MessageLength = kalMessageMcbs[h_message].userLength;

                    retCode = G2_SUCCESS;
                }
                else
                {
                    retCode = G2_INVALID_LENGTH;
                }
            }
            else
            {
                /* A length of zero means there's really nothing to do.  Check if the Offset is
                     valid.  */
                if (Offset <= kalMessageMcbs[h_message].userLength)
                {
                    retCode = G2_SUCCESS;
                }
                else
                {
                    retCode = G2_INVALID_LENGTH;
                }
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }
    KAL_MessageReleaseLock();

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageGetLock() - this routine obtains a semaphore lock to protect accesses to the       *
 *                         message control blocks.                                                *
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
static void
KAL_MessageGetLock(void)
{
    while ((sem_wait(&kalMessageSemaphoreLock) == -1) && (errno == EINTR))
    {
        /* Do nothing.  This is done in case the semaphore call returned due to a signal.  */
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MessageReleaseLock() - this routine releases a semaphore lock across the KAL message      *
 *                             manipulation routines.  It is important that a call to this        *
 *                             routine must follow a prior call to KAL_MessageGetLock().          *
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
static void
KAL_MessageReleaseLock(void)
{
    sem_post(&kalMessageSemaphoreLock);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailQueueIsEmpty() - this routine is used to determine whether the calling task's         *
 *                           asynchronous mail queue is empty or not.  Note that there will       *
 *                           always be a race condition where mail is queued just after this      *
 *                           routine is called.                                                   *
 *                                                                                                *
 *                           Instead of examining the queue itself, this routine will use the     *
 *                           KAL_TaskGetNumMailMsgs() routine to determine whether or not the     *
 *                           mail queue is empty.  This is required because of a potential race   *
 *                           condition.                                                           *
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
 *      u8_t: 0 - if the mail queue is not empty.                                                 *
 *            1 - if the mail queue is empty.                                                     *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MailQueueIsEmpty(void)
{
    return (KAL_TaskGetNumMailMsgs() == 0);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailSend() - this routine is used to send mail to a task.  Mail minimally consists of a   *
 *                   block of parameters.  There is also an optional reference to a message, but  *
 *                   the message will never be copied into the mail.                              *
 *                                                                                                *
 *                   When a mail message is sent (i.e., a message reference within mail), the     *
 *                   ownership of the message will change to the destination task.                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pMessageDetails - a pointer to information pertaining to user     *
 *                                                details about a message.  (All information      *
 *                                                about a message is split between user details   *
 *                                                and a hidden control block.)  This parameter    *
 *                                                may be NULL if there is no associated message.  *
 *      kal_mail_details_t*     pMailDetails    - a pointer to parameter information and          *
 *                                                destination task.  The parameter information    *
 *                                                will be copied into the new mail.               *
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
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_BAD_HMESSAGE    - if the pMailDetails parameter is NULL.                         *
 *            G2_BAD_HTASK       - if the destination KAL task handle (in the structure pointed   *
 *                                 at by pMailDetails) is not valid, or if the current thread is  *
 *                                 not the owner of the message.                                  *
 *            G2_ALLOCATION_FAIL - if there is not enough dynamic memory for the mail.            *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MailSend(kal_message_details_t*  pMessageDetails,
             kal_mail_details_t*     pMailDetails)
{
    return KAL_MailSendToAsync(pMessageDetails, pMailDetails, KAL_MAIL_UNIQUE_HANDLE_UNUSED);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailReceive() - this routine is called by a KAL task to receive mail.  It is expected     *
 *                      that the caller only invokes this routine in response to having Bit 0 in  *
 *                      its event flags set.                                                      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  pMessageDetails - a pointer to where information pertaining to a  *
 *                                                message should be placed.  This will only be    *
 *                                                used if there is a message reference within the *
 *                                                mail.  If there is no associated message, the   *
 *                                                fields within this structure will be zeroed.    *
 *      kal_mail_details_t*     pMailDetails    - a pointer to where parameter information will   *
 *                                                be placed.                                      *
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
 *      u8_t: G2_SUCCESS - upon success.                                                          *
 *            G2_FAILURE - if any error occurs.                                                   *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MailReceive(kal_message_details_t*  pMessageDetails,
                kal_mail_details_t*     pMailDetails)
{
    kal_mail_unique_handle_t  h_unique;  // Unused local variable.
    
    if (KAL_MailReceiveFromAsync(pMessageDetails, pMailDetails, &h_unique) != G2_SUCCESS)
    {
        return G2_FAILURE;
    }

    return G2_SUCCESS;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailSyncRegister() - this routine is used for registering the calling task's usage of     *
 *                           synchronous mail.  It should only be invoked before the KAL system   *
 *                           start event.  If a KAL task does not use synchronous mail at all,    *
 *                           then this routine does not need to be invoked by that task.          *
 *                                                                                                *
 *                           As per the "KAL Synchronous Mail Design Specification", a task       *
 *                           can only be an initiator, a replier, or not use synchronous mail at  *
 *                           all.                                                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_mailsync_tasktype_t  taskType - the type of "user" of synchronous mail, one of:       *
 *                                              KAL_MAILSYNC_TASKTYPE_NONE                        *
 *                                              KAL_MAILSYNC_TASKTYPE_INITIATOR                   *
 *                                              KAL_MAILSYNC_TASKTYPE_REPLIER                     *
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
 *      u8_t: G2_SUCCESS       - upon success.                                                    *
 *            G2_FAILURE       - if the routine is invoked after the KAL system start event.      *
 *            G2_BAD_ATTRIBUTE - if the registration type is invalid.                             *
 *            G2_BAD_HTASK     - if the caller is not a KAL task.                                 *
 *                                                                                                *
 **************************************************************************************************/
u8_t  
KAL_MailSyncRegister(kal_mailsync_tasktype_t  taskType)
{
    pthread_t  h_thread;
    u8_t       retCode=(u8_t)G2_FAILURE;
    u8_t       h_task;

    if (!KAL_HasSystemStarted())
    {
        if (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, &h_thread, &h_task) == G2_SUCCESS)
        {
            // Do not need to obtain lock on mail queue control block because system has not
            //   yet started and only this routine sets the synchronous mail registration 
            //   values.
            switch (taskType)
            {
            case KAL_MAILSYNC_TASKTYPE_NONE:
            case KAL_MAILSYNC_TASKTYPE_INITIATOR:
            case KAL_MAILSYNC_TASKTYPE_REPLIER:
                kalMailQueues[h_task].syncMailTaskType = taskType;
                retCode = G2_SUCCESS;
                break;
            default:
                retCode = G2_BAD_ATTRIBUTE;
                break;
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailSyncGetRegistration() - this routine is used for determining a specified task's       *
 *                                  synchronous mail registration status.  It should only be      *
 *                                  invoked after the KAL system start event.                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t                      taskId     - the KAL task ID for which synchronous mail         *
 *                                             registration status is requested.                  *
 *      kal_mailsync_tasktype_t*  p_taskType - a pointer to where the registration status will be *
 *                                             placed.  This parameter cannot be NULL.            *
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
 *      u8_t: G2_SUCCESS       - upon success.                                                    *
 *            G2_FAILURE       - if the routine is invoked before the KAL system start event.     *
 *            G2_BAD_HTASK     - if the specified KAL task ID is not valid.                       *
 *                                                                                                *
 **************************************************************************************************/
u8_t  
KAL_MailSyncGetRegistration(u8_t                      taskId,
                            kal_mailsync_tasktype_t*  p_taskType)
{
    pthread_t  h_thread;
    u8_t       retCode=(u8_t)G2_FAILURE;
    u8_t       h_task;

    if (KAL_HasSystemStarted())
    {
        if (KAL_GetThreadHandle(taskId, &h_thread, &h_task) == G2_SUCCESS)
        {
            KAL_MailGetLock(h_task);
            *p_taskType = kalMailQueues[h_task].syncMailTaskType;
            KAL_MailReleaseLock(h_task);
            retCode = G2_SUCCESS;
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailSyncSendAndWait() - this routine is used to initiate synchronous mail.  A specified   *
 *                              piece of mail is delivered to the replier's asynchronous mail     *
 *                              queue and the caller self-suspends until either the expected      *
 *                              synchronous reply mail is received or the timeout occurs.  If the *
 *                              reply was received, the reply mail (and possible associated KAL   *
 *                              message) is returned via parameters.                              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*  p_sendMsgDetails   - the address of a structure containing KAL    *
 *                                                   message information, or NULL if no KAL       *
 *                                                   message is associated with the KAL mail.     *
 *      kal_mail_details_t*     p_sendMailDetails  - the address of a structure containing KAL    *
 *                                                   mail information.  This parameter cannot be  *
 *                                                   NULL.                                        *
 *      kal_mail_id_t           replyMailId        - the expected mail ID of the synchronous      *
 *                                                   reply.                                       *
 *      kal_message_details_t*  p_replyMsgDetails  - the address of a structure for a KAL message *
 *                                                   which will be filled in when the synchronous *
 *                                                   reply is received.  This parameter cannot be *
 *                                                   NULL.  The content of the structure is       *
 *                                                   undefined if the return code is not          *
 *                                                   G2_SUCCESS.                                  *
 *      kal_mail_details_t*     p_replyMailDetails - the address of a structure for KAL mail      *
 *                                                   which will be filled in when the synchronous *
 *                                                   reply is received.  This parameter cannot be *
 *                                                   NULL.  The content of the structure is       *
 *                                                   undefined if the return code is not          *
 *                                                   G2_SUCCESS.                                  *
 *      u32_t                   timeout            - the amount of time to wait for the           *
 *                                                   synchronous reply, in milliseconds.  A value *
 *                                                   of zero indicates an infinite timeout.       *
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
 *      u8_t: G2_SUCCESS                - upon success.                                           *
 *            G2_FAILURE                - if the routine is invoked before the KAL system start   *
 *                                        event.                                                  *
 *            G2_BAD_HTASK              - if the caller is not a KAL task, or if the destination  *
 *                                        task handle is invalid.                                 *
 *            G2_BAD_HMESSAGE           - if the KAL message information is invalid.              *
 *            G2_TIMEOUT                - if no reply was received before the timeout occurred.   *
 *            G2_ALLOCATION_FAIL        - if the operation failed due to lack of heap.            *
 *            G2_SYNCMAIL_NOT_SUPPORTED - if the operation failed because the mail receiver is    *
 *                                        not a synchronous mail replier.                         *
 *            G2_SYNCMAIL_INVALID       - if the operation failed because the calling task is not *
 *                                        a synchronous mail initiator.                           *
 *                                                                                                *
 **************************************************************************************************/
u8_t    
KAL_MailSyncSendAndWait(kal_message_details_t*  p_sendMsgDetails,
                        kal_mail_details_t*     p_sendMailDetails,
                        kal_mail_id_t           replyMailId,
                        kal_message_details_t*  p_replyMsgDetails,
                        kal_mail_details_t*     p_replyMailDetails,
                        u32_t                   timeout)
{
    pthread_t                 h_srcThread;
    kal_mailCtrlBlk_t*        p_syncMail;
    u8_t                      retCode=(u8_t)G2_FAILURE;
    u8_t                      h_srcTask;
    i16_t                     h_message;
    kal_mail_unique_handle_t  h_unique;
    kal_mailsync_tasktype_t   destTaskType;

    PSAL_ASSERT(p_sendMailDetails != NULL);
    PSAL_ASSERT(p_replyMsgDetails != NULL);
    PSAL_ASSERT(p_replyMailDetails != NULL);

    if (KAL_HasSystemStarted())
    {
        if (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, &h_srcThread, &h_srcTask) == G2_SUCCESS)
        {
            if (KAL_MailSyncGetRegistration(p_sendMailDetails->iPartnerTask, &destTaskType) == 
                G2_SUCCESS)
            {
                if (destTaskType == KAL_MAILSYNC_TASKTYPE_REPLIER)
                {
                    KAL_MailGetLock(h_srcTask);
                    if (kalMailQueues[h_srcTask].syncMailTaskType == KAL_MAILSYNC_TASKTYPE_INITIATOR)
                    {
                        KAL_MailSyncIncrementUniqueHandle(&kalMailQueues[h_srcTask].h_syncExpectedUnique);
                        h_unique = kalMailQueues[h_srcTask].h_syncExpectedUnique;
                        kalMailQueues[h_srcTask].syncExpectedMailId = replyMailId;
                        KAL_MailReleaseLock(h_srcTask);

                        retCode = KAL_MailSendToAsync(p_sendMsgDetails, p_sendMailDetails, h_unique);
                    }
                    else
                    {
                        KAL_MailReleaseLock(h_srcTask);
                        retCode = G2_SYNCMAIL_INVALID;
                    }
                }
                else
                {
                    retCode = G2_SYNCMAIL_NOT_SUPPORTED;
                }
            }
            else
            {
                retCode = G2_BAD_HTASK;
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }

        if (retCode == G2_SUCCESS)
        {
            // Block on counting semaphore with timeout.
            retCode = KAL_SemaphorePend(kalMailQueues[h_srcTask].syncCountingSemaphore, timeout);

            PSAL_ASSERT(retCode != G2_FAILURE);

            KAL_MailGetLock(h_srcTask);

            // Check if the synchronous queue is empty.
            p_syncMail = kalMailQueues[h_srcTask].p_sync;
            if (p_syncMail == NULL)
            {
                // No synchronous reply is present so a timeout should have occurred.  Double-
                //   check this.
                PSAL_ASSERT(retCode == G2_TIMEOUT);
                
                // Increment the expected unique handle value to prevent a late-arriving reply
                //   from being queued as a synchronous reply.
                KAL_MailSyncIncrementUniqueHandle(&kalMailQueues[h_srcTask].h_syncExpectedUnique);

                KAL_MailReleaseLock(h_srcTask);
            }
            else
            {
                // Dequeue the synchronous reply, and place the reply on the list of mail to be 
                //   recycled.
                kalMailQueues[h_srcTask].p_sync = NULL;
                p_syncMail->p_next = kalMailQueues[h_srcTask].p_lastAccessedByUser;
                kalMailQueues[h_srcTask].p_lastAccessedByUser = p_syncMail;

                // Check for race condition where timeout occurred but before the calling task
                //   could handle the timeout the synchronous reply was received.
                if (retCode == G2_TIMEOUT)
                {
                    (void) KAL_SemaphorePend(kalMailQueues[h_srcTask].syncCountingSemaphore, 0);
                    retCode = G2_SUCCESS;
                }

                KAL_MailReleaseLock(h_srcTask);

                // Outside of the lock of the mail queue control block, copy out the reply 
                //   information for mail and possible associated message.
                p_replyMailDetails->pParameterBlock = &p_syncMail->parmBlkPlaceholder[0];
                p_replyMailDetails->ParameterLength = p_syncMail->parmLength;
                p_replyMailDetails->iPartnerTask = p_syncMail->h_senderTask;
                p_replyMailDetails->MailId = p_syncMail->mailId;
                p_replyMsgDetails->MessageAddress = NULL;
                p_replyMsgDetails->MessageLength = 0;
                p_replyMsgDetails->iMessage = KAL_MESSAGE_INVALID_HANDLE;
                if (p_syncMail->h_message != KAL_MESSAGE_INVALID_HANDLE)
                {
                    h_message = p_syncMail->h_message;
                    KAL_MessageGetLock();
                    if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
                        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
                    {
                        p_replyMsgDetails->MessageAddress = kalMessageMcbs[h_message].p_msgBuffer +
                                                            kalMessageMcbs[h_message].currentOffset;
                        p_replyMsgDetails->MessageLength = kalMessageMcbs[h_message].userLength;
                        p_replyMsgDetails->iMessage = h_message;
                    }

                    KAL_MessageReleaseLock();
                }
            }
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailSyncReceive() - this routine is used by a synchronous mail replier to read mail off   *
 *                          of its asynchronous queue.  It differs from KAL_MailReceive() in that *
 *                          the opaque information is obtained so it can be passed along when the *
 *                          reply is sent.                                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*     p_msgDetails  - pointer to a structure which will be filled in *
 *                                                 with KAL message information.  This parameter  *
 *                                                 cannot be NULL.  The content of this structure *
 *                                                 is undefined if the return code is not         *
 *                                                 G2_SUCCESS.                                    *
 *      kal_mail_details_t*        p_mailDetails - pointer to a structure which will be filled in *
 *                                                 with KAL mail information.  This parameter     *
 *                                                 cannot be NULL.  The content of this structure *
 *                                                 is undefined if the return code is not         *
 *                                                 G2_SUCCESS.                                    *
 *      kal_mail_unique_handle_t*  ph_unique     - pointer to an opaque piece of data             *
 *                                                 representing a handle to a unique value which  *
 *                                                 will be filled in.  This parameter cannot be   *
 *                                                 NULL.  The content of the unique handle is     *
 *                                                 undefined if the return code is not            *
 *                                                 G2_SUCCESS.                                    *
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
 *      u8_t: G2_SUCCESS                - upon success.                                           *
 *            G2_FAILURE                - if invoked before the KAL system start event, or if     *
 *                                        there is no mail on the asynchronous queue.             *
 *            G2_BAD_HTASK              - if the caller is not a KAL task.                        *
 *            G2_SYNCMAIL_NOT_SUPPORTED - if the operation failed because the caller is not a     *
 *                                        synchronous mail replier.                               *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MailSyncReceive(kal_message_details_t*     p_msgDetails,
                    kal_mail_details_t*        p_mailDetails,
                    kal_mail_unique_handle_t*  ph_unique)
{
    pthread_t  h_thread;
    u8_t       h_task;
    u8_t       retCode=G2_BAD_HTASK;

    // Verify that the caller is a synchronous mail replier.
    if (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, &h_thread, &h_task) == G2_SUCCESS)
    {
        if (kalMailQueues[h_task].syncMailTaskType == KAL_MAILSYNC_TASKTYPE_REPLIER)
        {
            retCode = KAL_MailReceiveFromAsync(p_msgDetails, p_mailDetails, ph_unique);
        }
        else
        {
            retCode = G2_SYNCMAIL_NOT_SUPPORTED;
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailSyncSendReply() - this routine is used by a synchronous mail replier to send all      *
 *                            reply mail (i.e., confirmations), synchronous or asynchronous.  The *
 *                            h_unique parameter will specify whether the mail is synchronous or  *
 *                            asynchronous.  If a synchronous reply is sent, there is no check    *
 *                            whether the mail receipient is a synchronous mail initiator         *
 *                            (because the unique handle should not match).                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*    p_msgDetails  - pointer to a structure which contains           *
 *                                                associated KAL message information.  This       *
 *                                                parameter may be NULL if there is no associated *
 *                                                KAL message.  If this parameter is not NULL,    *
 *                                                then the structure's contents will be cleared   *
 *                                                if the mail was successfully delivered.         *
 *      kal_mail_details_t*       p_mailDetails - pointer to a structure which contains KAL mail  *
 *                                                information.  This parameter cannot be NULL.    *
 *      kal_mail_unique_handle_t  h_unique      - the unique handle which was obtained from the   *
 *                                                initiating mail message in a prior call to      *
 *                                                KAL_MailSyncReceive().                          *
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
 *      u8_t: G2_SUCCESS                - upon success.                                           *
 *            G2_FAILURE                - if invoked before the KAL system start event.           *
 *            G2_BAD_HTASK              - if the caller is not a KAL task, or if the destination  *
 *                                        task handle (in the mail information) is not a KAL      *
 *                                        task.                                                   *
 *            G2_BAD_HMESSAGE           - if the KAL message information is not valid (invalid    *
 *                                        KAL message handle or that the caller is not the owner  *
 *                                        of the message).                                        *
 *            G2_TIMEOUT                - if the mail was discarded either due to a timeout       *
 *                                        occurring before the reply was sent or if the reply     *
 *                                        mail ID does not match the expected mail ID.            *
 *            G2_ALLOCATION_FAIL        - if the operation failed due to insufficient heap.       *
 *            G2_SYNCMAIL_NOT_SUPPORTED - if the operation failed because the caller is not a     *
 *                                        synchronous mail replier.                               *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MailSyncSendReply(kal_message_details_t*    p_msgDetails,
                      kal_mail_details_t*       p_mailDetails,
                      kal_mail_unique_handle_t  h_unique)
{
    pthread_t           h_srcThread;
    pthread_t           h_destThread;
    kal_mailCtrlBlk_t*  p_mail;
    u8_t                h_srcTask;
    u8_t                h_destTask;
    i16_t               h_message;
    u8_t                retCode=(u8_t)G2_FAILURE;

    PSAL_ASSERT(p_mailDetails != NULL);

    if (KAL_HasSystemStarted())
    {
        if ((KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, 
                                 &h_srcThread, &h_srcTask) == G2_SUCCESS) && 
            (KAL_GetThreadHandle(p_mailDetails->iPartnerTask, 
                                 &h_destThread, &h_destTask) == G2_SUCCESS))
        {
            // Check whether the mail is synchronous or asynchronous.
            if (KAL_MAIL_IS_HANDLE_ASYNC(h_unique))
            {
                retCode = KAL_MailSendToAsync(p_msgDetails, p_mailDetails, h_unique);
            }
            else
            {
                // The mail is a synchronous reply.  First, check if the associated KAL message 
                //   is valid.
                retCode = G2_SUCCESS;
                if (p_msgDetails == NULL)
                {
                    h_message = KAL_MESSAGE_INVALID_HANDLE;
                }
                else
                {
                    KAL_MessageGetLock();

                    // Check for invalid KAL message handle.
                    h_message = p_msgDetails->iMessage;
                    if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
                        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
                    {
                        // Check for caller is the owner of the KAL message.
                        if (pthread_equal(h_srcThread, kalMessageMcbs[h_message].h_ownerThread) != 0)
                        {
                            // Change ownership of the KAL message from the sender to the receiver.
                            kalMessageMcbs[h_message].h_ownerThread = h_destThread;
                        }
                        else
                        {
                            retCode = G2_BAD_HMESSAGE;
                        }
                    }
                    else
                    {
                        retCode = G2_BAD_HMESSAGE;
                    }

                    KAL_MessageReleaseLock();
                }

                // Proceed if the KAL message information is valid.
                if (retCode == G2_SUCCESS)
                {
                    // Create a mail message.  The size is based on the length of the parameter
                    //   block.  Therefore, we add the length of the mail control block minus 
                    //   the placekeeper for the variable-length parameter block.
                    p_mail = (kal_mailCtrlBlk_t*) malloc(sizeof(kal_mailCtrlBlk_t) + 
                                                         p_mailDetails->ParameterLength - 
                                                         sizeof(u32_t));
                    if (p_mail != NULL)
                    {
                        // Fill in fields of the mail control block.
                        p_mail->p_next = NULL;
                        p_mail->parmLength = (u16_t)(p_mailDetails->ParameterLength);
                        p_mail->h_message = h_message;
                        p_mail->h_senderTask = h_srcTask;
                        p_mail->mailId = p_mailDetails->MailId;
                        p_mail->h_unique = KAL_MAIL_UNIQUE_HANDLE_UNUSED;

                        // Copy the variable-length parameter block.
                        memcpy(&p_mail->parmBlkPlaceholder[0], p_mailDetails->pParameterBlock,
                               p_mailDetails->ParameterLength);

                        // Attempt to place the mail on the destination's synchronous queue.
                        KAL_MailGetLock(h_destTask);

                        if ((h_unique == kalMailQueues[h_destTask].h_syncExpectedUnique) &&
                            (p_mailDetails->MailId == 
                             kalMailQueues[h_destTask].syncExpectedMailId))
                        {
                            // The synchronous mail queue should be empty.  Verify this.
                            PSAL_ASSERT(kalMailQueues[h_destTask].p_sync == NULL);

                            // Enqueue the message to the destination's synchronous queue.
                            kalMailQueues[h_destTask].p_sync = p_mail;

                            // Increment the destination's expected unique handle value to 
                            //   prevent accidental replays from being queued.
                            KAL_MailSyncIncrementUniqueHandle(&kalMailQueues[h_destTask].h_syncExpectedUnique);
                            (void) KAL_SemaphorePost(kalMailQueues[h_destTask].syncCountingSemaphore);
                            KAL_MailReleaseLock(h_destTask);

                            // Clear the caller's message information since it is no longer the
                            //   owner.
                            if (p_msgDetails != NULL)
                            {
                                p_msgDetails->MessageAddress = NULL;
                                p_msgDetails->MessageLength = 0;
                                p_msgDetails->iMessage = KAL_MESSAGE_INVALID_HANDLE;
                            }
                        }
                        else
                        {
                            // Either the unique handle does not match the expected value or
                            //   the mail ID does not match the expected value.  In either
                            //   case, drop the mail.  Also, reverse the change of ownership
                            //   of the associated KAL message back to the caller.
                            KAL_MailReleaseLock(h_destTask);
                            free(p_mail);
                            if (h_message != KAL_MESSAGE_INVALID_HANDLE)
                            {
                                KAL_MessageGetLock();
                                kalMessageMcbs[h_message].h_ownerThread = h_srcThread;
                                KAL_MessageReleaseLock();
                            }

                            retCode = G2_TIMEOUT;
                        }
                    }
                    else
                    {
                        // Allocation failed for the KAL mail.  Reverse the change of ownership
                        //   back to the caller.
                        if (h_message != KAL_MESSAGE_INVALID_HANDLE)
                        {
                            KAL_MessageGetLock();
                            kalMessageMcbs[h_message].h_ownerThread = h_srcThread;
                            KAL_MessageReleaseLock();
                        }

                        retCode = G2_ALLOCATION_FAIL;
                    }
                }
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailSendOneWay() - this routine is used to send one-way mail to a KAL task.  The sender   *
 *                         does not need to be a KAL task.                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_mail_details_t*  p_mailDetails - pointer to a structure which contains KAL mail       *
 *                                           information.  This parameter cannot be NULL.         *
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
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_FAILURE         - if invoked before the KAL system start event, or if the        *
 *                                 parameter "p_mailDetails" is NULL.                             *
 *            G2_BAD_HTASK       - if the destination is not a KAL task.                          *
 *            G2_ALLOCATION_FAIL - if the operation failed due to insufficient heap.              *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MailSendOneWay(kal_mail_details_t*  p_mailDetails)
{
    kal_mailCtrlBlk_t*  p_mail;
    pthread_t           h_destThread;
    u8_t                h_destTask;
    u8_t                retCode=(u8_t)G2_FAILURE;

    if ((KAL_HasSystemStarted()) && (p_mailDetails != NULL))
    {
        if (KAL_GetThreadHandle(p_mailDetails->iPartnerTask, &h_destThread, &h_destTask) 
            == G2_SUCCESS)
        {
            p_mail = (kal_mailCtrlBlk_t*) malloc(sizeof(kal_mailCtrlBlk_t) + 
                                                 p_mailDetails->ParameterLength - sizeof(u32_t));
            if (p_mail != NULL)
            {
                p_mail->p_next = NULL;
                p_mail->parmLength = (u16_t)(p_mailDetails->ParameterLength);
                p_mail->h_message = KAL_MESSAGE_INVALID_HANDLE;
                p_mail->h_senderTask = (u8_t)KAL_MAIL_NON_KAL_TASK;
                p_mail->mailId = p_mailDetails->MailId;
                p_mail->h_unique = KAL_MAIL_UNIQUE_HANDLE_UNUSED;
                if (p_mailDetails->ParameterLength > 0)
                {
                    memcpy(&p_mail->parmBlkPlaceholder[0], p_mailDetails->pParameterBlock,
                           p_mailDetails->ParameterLength);
                }

                KAL_MailGetLock(h_destTask);
                if (kalMailQueues[h_destTask].p_head == NULL)
                {
                    kalMailQueues[h_destTask].p_head = p_mail;
                }
                else
                {
                    kalMailQueues[h_destTask].p_tail->p_next = p_mail;
                }
                kalMailQueues[h_destTask].p_tail = p_mail;
                KAL_MailReleaseLock(h_destTask);
                KAL_TaskIncrementMailCount(h_destTask);
            }
            else
            {
                retCode = G2_ALLOCATION_FAIL;
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailInit() - this routine is used to initialize the Kernel Abstraction Layer's mail       *
 *                   handling package.  This should only be invoked once at system startup, after *
 *                   all tasks have been created.                                                 *
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
KAL_MailInit(void)
{
    u8_t  index;

    kalMailNumTasks = KAL_GetNumTasks();
    for (index=0; index<kalMailNumTasks; index++)
    {
        sem_init(&kalMailQueues[index].queueLock, 0, 1);
        kalMailQueues[index].p_lastAccessedByUser = NULL;
        kalMailQueues[index].p_head = NULL;
        kalMailQueues[index].p_tail = NULL;

        // Initialize fields to support synchronous mail
        kalMailQueues[index].p_sync   = NULL;
        kalMailQueues[index].syncCountingSemaphore = KAL_SemaphoreCreate(0);
        kalMailQueues[index].h_syncExpectedUnique = (KAL_MAIL_UNIQUE_HANDLE_BITMASK_SYNCQ + 1);
        kalMailQueues[index].syncExpectedMailId = 0;

        // Because of "late initialization" of mail queues, do not set the syncMailTaskType
        //   field as it has already been initialized.
    }
    kalMailInitialized = KAL_MESSAGE_TRUE;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailPurgeOld() - this routine should be invoked only by KAL_TaskEventWait() to delete     *
 *                       dynamic memory associated with parameter information which was sent in   *
 *                       the last mail.  This routine will simply return if there is nothing to   *
 *                       delete.                                                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  h_task - the KAL task handle whose old parameter block should be deleted, if any.   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_MailPurgeOld(u8_t  h_task)
{
    kal_mailCtrlBlk_t*  p_temp;

    if (kalMailInitialized)
    {
        if (h_task < kalMailNumTasks)
        {
            KAL_MailGetLock(h_task);
            p_temp = kalMailQueues[h_task].p_lastAccessedByUser;
            if (p_temp != NULL)
            {
                kalMailQueues[h_task].p_lastAccessedByUser = p_temp->p_next;
                free(p_temp);
                p_temp = kalMailQueues[h_task].p_lastAccessedByUser;
                if (p_temp != NULL)
                {
                    //  fprintf(stderr,
                    //          "ERROR: in KAL_MailPurgeOld(), there are multiple old parameter blocks!!  (%d)\n",
                    //          h_task);
                    while (p_temp != NULL)
                    {
                        kalMailQueues[h_task].p_lastAccessedByUser = p_temp->p_next;
                        free(p_temp);
                        p_temp = kalMailQueues[h_task].p_lastAccessedByUser;
                    }
                }
            }
            KAL_MailReleaseLock(h_task);
        }
        else
        {
            fprintf(stderr,
                    "ERROR: call to KAL_MailPurgeOld() with a bad mailbox handle!!  (%d)\n",
                    h_task);
        }
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailGetLock() - this routine obtains a semaphore lock to protect accesses to a specified  *
 *                      mailbox.                                                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  h_mailbox - the handle of the mailbox to lock.  The handle to the task may also be  *
 *                        used.                                                                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void
KAL_MailGetLock(u8_t  h_mailbox)
{
    if (h_mailbox < kalMailNumTasks)
    {
        while ((sem_wait(&kalMailQueues[h_mailbox].queueLock) == -1) && (errno == EINTR))
        {
            /* Do nothing.  This is done in case the semaphore call returned due to a signal.  */
        }
    }
    else
    {
        fprintf(stderr,
                "ERROR: call to KAL_MailGetLock() with a bad mailbox handle!!  (%d)\n",
                h_mailbox);
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailReleaseLock() - this routine releases a semaphore lock for a specified mailbox.       *
 *                          It is important that a call to this routine must follow a prior call  *
 *                          to KAL_MailGetLock() with the same mailbox handle.                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      u8_t  h_mailbox - the handle of the mailbox to lock.  The handle to the task may also be  *
 *                        used.                                                                   *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void
KAL_MailReleaseLock(u8_t  h_mailbox)
{
    if (h_mailbox < kalMailNumTasks)
    {
        sem_post(&kalMailQueues[h_mailbox].queueLock);
    }
    else
    {
        fprintf(stderr,
                "ERROR: call to KAL_MailReleaseLock() with a bad mailbox handle!!  (%d)\n",
                h_mailbox);
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailSendToAsync() - this routine is used to send mail to a task's asynchronous mail       *
 *                          queue.  Mail minimally consists of a block of parameters.  There is   *
 *                          also an optional reference to a message, but the message will never   *
 *                          be copied into the mail.                                              *
 *                                                                                                *
 *                          When a mail message is sent (i.e., a message reference within mail),  *
 *                          the ownership of the message will change to the destination task.     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*    p_msgDetails  - a pointer to information pertaining to user     *
 *                                                details about a message.  (All information      *
 *                                                about a message is split between user details   *
 *                                                and a hidden control block.)  This parameter    *
 *                                                may be NULL if there is no associated message.  *
 *                                                If this is not NULL, then the contents of the   *
 *                                                structure will be cleared if the mail was       *
 *                                                successfully delivered.                         *
 *      kal_mail_details_t*       p_mailDetails - a pointer to parameter information and          *
 *                                                destination task.  The parameter information    *
 *                                                will be copied into the new mail.  This         *
 *                                                parameter cannot be NULL.                       *
 *      kal_mail_unique_handle_t  h_unique      - unique value associated with the mail. This     *
 *                                                value may be KAL_MAIL_UNIQUE_HANDLE_UNUSED if   *
 *                                                the mail is being sent via KAL_MailSend()       *
 *                                                (i.e., if the mail is not synchronous).         *
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
 *      u8_t: G2_SUCCESS         - upon success.                                                  *
 *            G2_FAILURE         - if this routine is invoked before the KAL system start event.  *
 *            G2_BAD_HMESSAGE    - if the pMailDetails parameter is NULL.                         *
 *            G2_BAD_HTASK       - if the destination KAL task handle (in the structure pointed   *
 *                                 at by pMailDetails) is not valid, or if the current thread is  *
 *                                 not the owner of the message.                                  *
 *            G2_ALLOCATION_FAIL - if there is not enough dynamic memory for the mail.            *
 *                                                                                                *
 **************************************************************************************************/
u8_t
KAL_MailSendToAsync(kal_message_details_t*    p_msgDetails,
                    kal_mail_details_t*       p_mailDetails,
                    kal_mail_unique_handle_t  h_unique)
{
    u8_t                retCode=(u8_t)G2_FAILURE;
    i16_t               h_message;
    kal_mailCtrlBlk_t*  p_mail;
    pthread_t           h_destThread;
    pthread_t           h_srcThread;
    u8_t                h_destTask;
    u8_t                h_srcTask;

    PSAL_ASSERT(p_mailDetails != NULL);

    if (KAL_HasSystemStarted())
    {
        if ((KAL_GetThreadHandle(p_mailDetails->iPartnerTask,
                                 &h_destThread, &h_destTask) == G2_SUCCESS) &&
            (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE,
                                 &h_srcThread, &h_srcTask) == G2_SUCCESS))
        {
            retCode = G2_SUCCESS;
            if (p_msgDetails == NULL)
            {
                h_message = KAL_MESSAGE_INVALID_HANDLE;
            }
            else
            {
                // Check for invalid KAL message handle.
                KAL_MessageGetLock();
                h_message = p_msgDetails->iMessage;
                if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) &&
                    (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
                {
                    // Check for caller is the owner of the KAL message.
                    if (pthread_equal(h_srcThread, kalMessageMcbs[h_message].h_ownerThread) != 0)
                    {
                        // Change ownership of the KAL message from the sender to the receiver.
                        kalMessageMcbs[h_message].h_ownerThread = h_destThread;
                    }
                    else
                    {
                        retCode = G2_BAD_HMESSAGE;
                    }
                }
                else
                {
                    retCode = G2_BAD_HMESSAGE;
                }

                KAL_MessageReleaseLock();
            }

            if (retCode == G2_SUCCESS)
            {
                // Create a mail message.  The size is based on the length of the parameter block.
                //   Therefore, we add the length of the mail control block minus the placekeeper
                //   for the variable-length parameter block.  */
                p_mail = (kal_mailCtrlBlk_t*) malloc(sizeof(kal_mailCtrlBlk_t) +
                                                     p_mailDetails->ParameterLength - 
                                                     sizeof(u32_t));
                if (p_mail != NULL)
                {
                    // Fill in fields of the mail control block.
                    p_mail->p_next = NULL;
                    p_mail->parmLength = (u16_t)(p_mailDetails->ParameterLength);
                    p_mail->h_message = h_message;
                    p_mail->h_senderTask = h_srcTask;
                    p_mail->mailId = p_mailDetails->MailId;
                    p_mail->h_unique = h_unique;

                    // Copy the variable-length parameter block.
                    memcpy(&p_mail->parmBlkPlaceholder[0], p_mailDetails->pParameterBlock,
                           p_mailDetails->ParameterLength);

                    // Now, send this message to the recipient's mailbox (i.e., asynchronous queue).
                    KAL_MailGetLock(h_destTask);
                    if (kalMailQueues[h_destTask].p_head == NULL)
                    {
                        kalMailQueues[h_destTask].p_head = p_mail;
                    }
                    else
                    {
                        kalMailQueues[h_destTask].p_tail->p_next = p_mail;
                    }
                    kalMailQueues[h_destTask].p_tail = p_mail;
                    KAL_MailReleaseLock(h_destTask);
                    KAL_TaskIncrementMailCount(h_destTask);

                    // Clear the caller's message information since it is no longer the owner.
                    if (p_msgDetails != NULL)
                    {
                        p_msgDetails->MessageAddress = NULL;
                        p_msgDetails->MessageLength = 0;
                        p_msgDetails->iMessage = KAL_MESSAGE_INVALID_HANDLE;
                    }
                }
                else
                {
                    // Allocation failed.  Reverse change to the ownership of the message.
                    if (h_message != KAL_MESSAGE_INVALID_HANDLE)
                    {
                        KAL_MessageGetLock();
                        kalMessageMcbs[h_message].h_ownerThread = h_srcThread;
                        KAL_MessageReleaseLock();
                    }

                    retCode = G2_ALLOCATION_FAIL;
                }
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailReceiveFromAsync() - this routine is used by a KAL task to read mail off of its       *
 *                               asynchronous queue.  It is used by both the KAL_MailReceive()    *
 *                               and KAL_MailSyncReceive() routines.  A parameter,                *
 *                               bypassSyncReplierCheck, is used to bypass the check if the       *
 *                               caller is a synchronous mail replier.                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      kal_message_details_t*     p_msgDetails  - pointer to a structure which will be filled in *
 *                                                 with KAL message information.  This parameter  *
 *                                                 cannot be NULL.  The content of this structure *
 *                                                 is undefined if the return code is not         *
 *                                                 G2_SUCCESS.                                    *
 *      kal_mail_details_t*        p_mailDetails - pointer to a structure which will be filled in *
 *                                                 with KAL mail information.  This parameter     *
 *                                                 cannot be NULL.  The content of this structure *
 *                                                 is undefined if the return code is not         *
 *                                                 G2_SUCCESS.                                    *
 *      kal_mail_unique_handle_t*  ph_unique     - pointer to an opaque piece of data             *
 *                                                 representing a handle to a unique value which  *
 *                                                 will be filled in.  This parameter cannot be   *
 *                                                 NULL.  The content of the unique handle is     *
 *                                                 undefined if the return code is not            *
 *                                                 G2_SUCCESS.                                    *
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
 *      u8_t: G2_SUCCESS   - upon success.                                                        *
 *            G2_FAILURE   - if invoked before the KAL system start event, or if there is no mail *
 *                           on the asynchronous queue.                                           *
 *            G2_BAD_HTASK - if the caller is not a KAL task.                                     *
 *                                                                                                *
 **************************************************************************************************/
static u8_t
KAL_MailReceiveFromAsync(kal_message_details_t*     p_msgDetails,
                         kal_mail_details_t*        p_mailDetails,
                         kal_mail_unique_handle_t*  ph_unique)
{
    pthread_t           h_thread;
    kal_mailCtrlBlk_t*  p_mail;
    u8_t                retCode=(u8_t)G2_FAILURE;
    u8_t                h_task;
    i16_t               h_message;

    PSAL_ASSERT(p_msgDetails != NULL);
    PSAL_ASSERT(p_mailDetails != NULL);
    PSAL_ASSERT(ph_unique != NULL);

    if (KAL_HasSystemStarted())
    {
        if (KAL_GetThreadHandle(KAL_TASK_SELF_TASK_HANDLE, &h_thread, &h_task) == G2_SUCCESS)
        {
            KAL_MailGetLock(h_task);

            if (kalMailQueues[h_task].p_head != NULL)
            {
                // Remove the first piece of mail from the asynchronous queue and place on list
                //   of mail to recycle.
                p_mail = kalMailQueues[h_task].p_head;
                kalMailQueues[h_task].p_head = p_mail->p_next;
                p_mail->p_next = kalMailQueues[h_task].p_lastAccessedByUser;
                kalMailQueues[h_task].p_lastAccessedByUser = p_mail;
                KAL_MailReleaseLock(h_task);

                // Decrement the count of mail on the asynchronous queue.  This must be done
                //   outside the mail queue control block lock.
                KAL_TaskDecrementMailCount();

                // Outside of the lock, copy out the mail information and possible associated
                //   message.
                p_mailDetails->pParameterBlock = &p_mail->parmBlkPlaceholder[0];
                p_mailDetails->ParameterLength = p_mail->parmLength;
                p_mailDetails->iPartnerTask = p_mail->h_senderTask;
                p_mailDetails->MailId = p_mail->mailId;
                p_msgDetails->MessageAddress = NULL;
                p_msgDetails->MessageLength = 0;
                p_msgDetails->iMessage = KAL_MESSAGE_INVALID_HANDLE;
                if (p_mail->h_message != KAL_MESSAGE_INVALID_HANDLE)
                {
                    h_message = p_mail->h_message;
                    KAL_MessageGetLock();
                    if ((h_message < KAL_CONFIG_MESSAGE_MAX_CONCURRENT) && 
                        (kalMessageMcbs[h_message].inUse == KAL_MESSAGE_TRUE))
                    {
                        p_msgDetails->MessageAddress = kalMessageMcbs[h_message].p_msgBuffer +
                            kalMessageMcbs[h_message].currentOffset;
                        p_msgDetails->MessageLength = kalMessageMcbs[h_message].userLength;
                        p_msgDetails->iMessage = h_message;
                    }

                    KAL_MessageReleaseLock();
                }

                // Copy the unique handle value.
                *ph_unique = p_mail->h_unique;

                retCode = G2_SUCCESS;
            }
            else
            {
                KAL_MailReleaseLock(h_task);
            }
        }
        else
        {
            retCode = G2_BAD_HTASK;
        }
    }

    return retCode;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MailSyncIncrementUniqueHandle() - this private routine is used to increment the value of  *
 *                                        a synchronous unique handle.  The handle value consists *
 *                                        of three groups of bits:                                *
 *                                                                                                *
 *                                            1) A bit indicating whether the unique handle is    *
 *                                               for synchronous or asynchronous mail.            *
 *                                            2) A reserved bit.                                  *
 *                                            3) A 30-bit field used for a value.  All zeroes in  *
 *                                               this field signifies asynchronous mail.          *
 *                                                                                                *
 *                                        Therefore, this routine will increment the 30-bit field *
 *                                        (and will ensure that its value is not zero) and will   *
 *                                        ensure that the synchronous bit is set.                 *
 *                                                                                                *
 *                                        This routine does not lock the unique handle.  It is    *
 *                                        expected that the caller properly lock the resource     *
 *                                        before invoking this routine.                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      volatile kal_mail_unique_handle_t*  ph_unique - a pointer the a synchronous unique handle *
 *                                                      value.  This value will be altered by     *
 *                                                      this routine.                             *
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
KAL_MailSyncIncrementUniqueHandle(volatile kal_mail_unique_handle_t*  ph_unique)
{
    kal_mail_unique_handle_t  h_newValue;

    h_newValue = ((*ph_unique) & KAL_MAIL_UNIQUE_HANDLE_BITMASK_VALUE) + 1;
    if (h_newValue == 0)
    {
        h_newValue++;
    }
    *ph_unique = (KAL_MAIL_UNIQUE_HANDLE_BITMASK_SYNCQ | h_newValue);
}



