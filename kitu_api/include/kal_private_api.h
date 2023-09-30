/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2014.  All rights reserved.                                  *
 *                                                                                                *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc. and is confidential in nature.                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  kal_private_api.h                                                                             *
 *                                                                                                *
 *  This file contains function prototypes for the private API for the Kernel Abstraction Layer.  *
 *  The private API is for Grid2Home use exclusively and should not be released to customers.     *
 *                                                                                                *
 **************************************************************************************************/


#ifndef KAL_PRIVATE_API_H
#define KAL_PRIVATE_API_H


#include <stdbool.h>
#include "kal_api.h"




/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_MAIL_UNIQUE_HANDLE_BITMASK_SYNCQ          (0x80000000)
#define KAL_MAIL_UNIQUE_HANDLE_BITMASK_HIPRI          (0x40000000)
#define KAL_MAIL_UNIQUE_HANDLE_BITMASK_VALUE          (0x3FFFFFFF)








/**************************************************************************************************
 *                                                                                                *
 *  Macros                                                                                        *
 *                                                                                                *
 **************************************************************************************************/
#define KAL_MAIL_IS_HANDLE_SYNC(_handle)              ((((kal_mail_unique_handle_t) _handle) & KAL_MAIL_UNIQUE_HANDLE_BITMASK_SYNCQ) != 0)
#define KAL_MAIL_IS_HANDLE_ASYNC(_handle)             ((((kal_mail_unique_handle_t) _handle) & KAL_MAIL_UNIQUE_HANDLE_BITMASK_SYNCQ) == 0)








/**************************************************************************************************
 *                                                                                                *
 *  External Function Declarations                                                                *
 *                                                                                                *
 **************************************************************************************************/


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
extern u8_t
KAL_MailSendToAsync(kal_message_details_t*    p_msgDetails,
                    kal_mail_details_t*       p_mailDetails,
                    kal_mail_unique_handle_t  h_unique);








#endif  /* matches #ifndef KAL_PRIVATE_API_H  */
