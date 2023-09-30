#ifndef USOCK_CLIENT_H
#define USOCK_CLIENT_H

#include "psal_types.h"

void usock_message_handler();
void usock_init();
//void usock_task();
int usock_write_message(char* send_buf, int send_buf_len);
void register_process_messages_callback(u32_t (*ptr)(char *buffer, int size));


#endif // USOCK_CLIENT_H