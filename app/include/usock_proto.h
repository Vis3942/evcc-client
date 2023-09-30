#ifndef USOCK_PROTO_H
#define USOCK_PROTO_H

#include <stdint.h>
#include "psal_types.h"
#include "sl_api.h"

#define CAN2SEP_SERVER_STATUS_ID 0x801
#define CAN2SEP_DER_SETTINGS_T_ID  0x802
#define CAN2SEP_DER_DEFAULT_DERC_T_ID 0x803
#define CAN2SEP_FREQ_DROOP_T_ID 0x804
#define CAN2SEP_DER_CURVE_T_ID 0x805
#define CAN2SEP_DER_DERC_IND_T_ID 0x806
#define CAN2SEP_EVSE_COMMUNICATION_STATUS_ID 0x807 // 1=> IS COMMUNICATING, 0 => LOST COMMUNCATIONS

#define BUF_SIZE 4096
extern char recv_buf[BUF_SIZE];
extern int recv_buf_len;
void send_message(u8_t * send_buf, int send_buf_len, u32_t message_type);
u32_t process_recv_message(char* rd_buf, int rd_buf_len);
void send_mail_message_to_server(void* pMail, u32_t message_type);
void test_server_send_dera_ind();
void test_client_send_derc_ind();
void send_der_setting(sl_der_settings_t der_sett );
void send_default_derc(sl_der_default_derc_t def_derc);
void send_freq_droop_data(sl_xsd_freq_droop_t freq_droop);
void send_curve_data(sl_der_curve_t curve);
void send_communication_status(u8_t status);
void send_derc_ind(sl_der_client_derc_ind * derc_ind);

extern u32_t proto_message_len;
extern u32_t proto_message_type;
u32_t process_message_app_task();

#endif // USOCK_PROTO_H