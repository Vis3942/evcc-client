#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include "kal_api.h"
#include "utils.h"
#include "usock_proto.h"

char task_pipe_buf[TASK_PIPE_SIZE][BUF_SIZE];
int task_pipe_len[TASK_PIPE_SIZE];
int tp_front = 0;
int tp_rear = 0;
int tp_count = 0;
sem_t mutex;
bool sem_initialized = 0;

int opt_test_charge = 0; // 1 => user specified -c option in command line with value
int opt_charge_percent = -1; // default value; if -c option is present, this needs to be a value following that
int opt_test_discharge = 0; // 1 => user specified -d option in command line with value
int opt_discharge_percent = -1;
int opt_start_time = 0; // 1 => user specified
int opt_start_time_from_now_secs = 0;
int opt_duration = 0; // 1 => user specified
int opt_duration_secs = 0;
int app_task_sec_count = 0;
int opt_event_status = -1; // 0 => scheduled, 1 => started, 2=> completed




char * evcc_client_app_name = "EVCC Kitu IEEE 2030.5 client App [IPv6]";

void evcc_client_version () {
  printf("App: %s -- Version: %04x d | compiled: %s\n", evcc_client_app_name, EVCC_CLIENT_VERSION,  __DATE__);
}

void init_semaphore()
{
  if (sem_init(&mutex, 0, 1) == -1)
  { 
    perror("sem_init"); exit(EXIT_FAILURE); 
  } 
  sem_initialized = 1;
}

void add_to_task_pipe(char *buf, int start_idx, int buf_len)
{
  if (tp_count == TASK_PIPE_SIZE) 
  {
    fprintf(stdout, "xxx>>>   TASK PIPE BUFFER is full. Cannot add data \n");
    return;
  }

  printf("--------------->>>>> enter critical section: ADD to TASK PIPE\n");
  if (!sem_initialized)
  {
    init_semaphore();
  }
  sem_wait(&mutex);
  //printf("--------------->>>>> entered critical section: ADD to TASK PIPE: after sem_wait\n");
  memcpy(task_pipe_buf[tp_rear], &buf[start_idx], buf_len);
  task_pipe_len[tp_rear] = buf_len;
  tp_rear = (tp_rear + 1) % TASK_PIPE_SIZE;
  tp_count++;
  //printf("--------------->>>>> entered critical section: ADD to TASK PIPE: after data copy\n");
	sem_post(&mutex);
  printf("--------------->>>>> exit critical section: ADD to TASK PIPE\n");
  
}

int get_from_task_pipe()
{
  int sem_sts;
  /*
   * copy the contents of task pipe to recv_buf for processing
   * return 0 if no data in task pipe buffer, else after copying to 
   * recv_buf return 1
  */
  if (tp_count == 0) 
  {
    //fprintf(stdout, "xxx>>> task buffer is empty\n");
    return 0;
  }

  if (!sem_initialized)
  {
    init_semaphore();
  }
  sem_sts = sem_trywait(&mutex);
  if (sem_sts != 0)
  {
    return 0;
  }

  printf("--------------->>>>> enter critical section: GET from TASK PIPE\n");

  memcpy(&proto_message_len, &task_pipe_buf[tp_front][0], 4);
  memcpy(&proto_message_type, &task_pipe_buf[tp_front][4], 4);

  recv_buf_len = task_pipe_len[tp_front]-8;
  memcpy(recv_buf, &task_pipe_buf[tp_front][8], recv_buf_len);

  tp_front = ( tp_front + 1) % TASK_PIPE_SIZE;
  tp_count --;

	sem_post(&mutex);
  printf("--------------->>>>> exit critical section: GET from TASK PIPE\n");

  return 1; // succesfully copied data to recv_buf 
}

int get_num_task_pipe_elements ()
{
  return tp_count;
}

int bytesToInt(unsigned char* bytes, int start_idx)
{
  int result = 0;
  // debug
  fprintf(stdout, "bytesToInt 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", bytes[start_idx], bytes[start_idx + 1],
        bytes[start_idx+2], bytes[start_idx+3]);
  result |= bytes[start_idx  + 3] << 24;
  result |= bytes[start_idx  + 2] << 16;
  result |= bytes[start_idx  + 1] << 8;
  result |= bytes[start_idx];
  return result;
}

void displayTime() 
{
  // time_t rawtime;
  // struct tm *timeinfo;
  // char buffer[80];

  // time(&rawtime);
  // timeinfo = localtime(&rawtime);

  // strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S");
  // fprintf(stdout, "%s.%03d", buffer, milliseconds)
  fprintf(stdout, "[%lu] ", (unsigned long)time(NULL));
}

void displayKALTime()
{
  i64_t tmpi64 = KAL_GetClock();
  fprintf(stdout, "[%lld] ", tmpi64);
}

void send_test_derc_event(int event_status)
{
  // event_status == 0, => scheduled event
  // event_status == 1, => started event
  // event_status == 2, => completed event
  sl_der_client_derc_ind  derc_ind = {0};
  
  derc_ind.derc.dercBase.opModFixedWExists = true;
  if (opt_test_charge == 1)
  {
    fprintf(stdout, "----->>>>> Test Setup Charge% => %d\n", opt_charge_percent);
    derc_ind.derc.dercBase.opModFixedW = -opt_charge_percent * 100;
  }
  else if (opt_test_discharge == 1)
  {
    fprintf(stdout, "----->>>>> Test Setup Discharge% => %d\n", opt_discharge_percent);
    derc_ind.derc.dercBase.opModFixedW = opt_discharge_percent * 100;
  }
  else 
  {
    fprintf(stdout, "----->>>>> Test Setup Charge/Discharge% => %d\n", 0);
    derc_ind.derc.dercBase.opModFixedW = 0;
  }
  if (event_status == 0)
  {
    fprintf(stdout, "----->>>>> Test Setup DERC Event => SCHEDULED\n");
    derc_ind.eventStatusCurrStatus = SL_XSD_EVENT_CURR_STATUS_SCHEDULED;
    derc_ind.activeInfo.eventRespStatus = SL_XSD_RESP_STATUS_MSG_RXED;
  }
  else if (event_status == 1)
  {
    fprintf(stdout, "----->>>>> Test Setup DERC Event => ACTIVE\n");
    derc_ind.eventStatusCurrStatus = SL_XSD_EVENT_CURR_STATUS_ACTIVE;
    derc_ind.activeInfo.eventRespStatus = SL_XSD_RESP_STATUS_EVENT_STARTED;
  }
  else if (event_status == 2)
  {
    fprintf(stdout, "----->>>>> Test Setup DERC Event => COMPLETED\n");
    derc_ind.eventStatusCurrStatus == 0;
    derc_ind.activeInfo.eventRespStatus = SL_XSD_RESP_STATUS_EVENT_COMPLETED;
  }
  else {
    fprintf(stdout, "invalid event status in Test DERC Event options %d, expected (0,1,2)\n", event_status);
    return;
  }
  send_derc_ind(&derc_ind);
}


void send_test_default_derc(bool permit_status)
{
  sl_der_default_derc_t dderc = {0};
  dderc.mrId[0] = 0x65;
  dderc.mrId[1] = 0x0F;
  dderc.mrId[2] = 0xD0;
  dderc.mrId[3] = 0x59;
  dderc.mrId[4] = 0x00;
  dderc.mrId[5] = 0x00;
  dderc.mrId[6] = 0x03;
  dderc.mrId[7] = 0x00;
  dderc.mrId[8] = 0x00;
  dderc.mrId[9] = 0x04;
  dderc.mrId[10] = 0xD1;

  strncpy(dderc.description, "Test Default DERC", 17);
  dderc.dercBase.opModEnergizeExists = true;
  dderc.dercBase.opModEnergize = permit_status;

  send_default_derc(dderc);
}

void send_test_dersettings()
{
  sl_der_settings_t der_sett = {0};
  der_sett.setGradWValue = 0;
  der_sett.setMaxChargeRateWExists = true;
  der_sett.setMaxChargeRateWValue = 5000;
  der_sett.setMaxChargeRateWMultiplier = -1;
  der_sett.setMaxVExists = true;
  der_sett.setMaxVValue = 2600;
  der_sett.setMaxVMultiplier = -1;
  der_sett.setMaxVarExists = true;
  der_sett.setMaxVarValue = 1000;
  der_sett.setMaxChargeRateWMultiplier = 0;
  der_sett.setMaxVarNegativeExists = true;
  der_sett.setMaxVarNegativeValue = -1000;
  der_sett.setMaxVarNegativeMultiplier = 0;
  der_sett.setMaxWValue = 0;
  der_sett.setMaxWMultiplier = 0;
  der_sett.setMaxWhExists = true;
  der_sett.setMaxWhValue = 7;
  der_sett.setMaxWhMultiplier = 3;
  der_sett.setMinVExists = true;
  der_sett.setMinVValue = 2000;
  der_sett.setMinVMultiplier = -1;
  der_sett.setVNomExists = true;
  der_sett.setVNomValue = 2080;
  der_sett.setVNomMultiplier = -1;

  der_sett.setVRefExists = true;
  der_sett.setVRefValue = 0;
  der_sett.setVRefMultiplier = 0;

  der_sett.updatedTime = (i64_t) time(NULL);
  fprintf(stdout, "======>>>>>>>>  DER setting updated time %lld\n", der_sett.updatedTime);

  send_der_setting(der_sett);

}

