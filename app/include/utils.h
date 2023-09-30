#ifndef UTILS_H
#define UTILS_H

#include "usock_proto.h"

// version definition => byte major version.byte minor version 
#define EVCC_CLIENT_VERSION 0x0100 // version 1.0

void evcc_client_version();

#define TASK_PIPE_SIZE 6

void add_to_task_pipe(char *buf, int start_idx, int buf_len);
int get_from_task_pipe();
int get_num_task_pipe_elements ();
int bytesToInt(unsigned char* bytes, int start_idx);
void displayTime();
void send_test_derc_event(int event_status);
void send_test_default_derc(bool permit_status);
void send_test_dersettings(void);
void displayKALTime(void);

extern int opt_test_charge; 
extern int opt_charge_percent; 
extern int opt_test_discharge; 
extern int opt_discharge_percent;
extern int opt_start_time;
extern int opt_start_time_from_now_secs;
extern int opt_duration;
extern int opt_duration_secs;
extern int app_task_sec_count;
#define INIT_SECS_BEFORE_EVENT 30
#define DDERC_PERIODIC_SECS 45
extern int opt_event_status;


#endif // UTILS_H



