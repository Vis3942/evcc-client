#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

//#include "logger.h"
#include "usock_proto.h"
#include "sl_api.h"
#include "app_eder_client.h"
#include "app_epwrstat_client.h"
#include "app_edinfo_client.h"
#include "utils.h"
#include "app_meter_mirror_client.h"

// #define SERVER_APP
#define CLIENT_APP
#ifdef CLIENT_APP
#include "usock_client.h"
#else
#include "usock_server.h"
#endif

u32_t  proto_message_len = 0;
u32_t  proto_message_type = 0;
char recv_buf[BUF_SIZE];
char send_buf[BUF_SIZE];
int recv_buf_len;
int send_buf_len;
char proc_buf[BUF_SIZE];
int proc_buf_len = 0;

// message type is based on defines in sl_api.h

void send_message(u8_t * loc_send_buf, int loc_send_buf_len, u32_t message_type) {
    int sent_len = -1;
    u32_t send_len = (u32_t) (loc_send_buf_len + 8);
    // debug
    printf("proto: in send message\n");
    memset(send_buf, 0, BUF_SIZE);
    memcpy(send_buf, &send_len, 4);
    memcpy((void*)&send_buf[4], &message_type, 4);
    memcpy((void*)&send_buf[8], loc_send_buf, loc_send_buf_len);
    sent_len = usock_write_message(send_buf, send_len);
    fprintf(stdout, "sent %d bytes\n", sent_len);
    //console_log("sent %d bytes", sent_len);
}

void test_process_dera_ind() {
  // data is available in the recv_buf
  int dera_ind_len;
  sl_eder_server_dera_ind ind = {0};
  dera_ind_len = sizeof(sl_eder_server_dera_ind);
  if (recv_buf_len == dera_ind_len) {
    memcpy(&ind, recv_buf, recv_buf_len);
    fprintf(stdout, "Test: recvd dera ind \n");
    // only test message ind contains availability duration as 4800; and maxChargeDuration as 240 secs
    fprintf(stdout, "avail_duration = %d, max charge duration = %d \n", 
        ind.dera.availabilityDuration, ind.dera.maxChargeDuration);

  }
  else 
  {
    fprintf(stdout, "received dera ind but improper lengths; struct len = %d, recvd len = %d \n", dera_ind_len, recv_buf_len);
  }
}

void process_dercap_req() {
  // data is available in the recv_buf
  int dercap_req_len;
  sl_eder_client_send_dercap_req req = {0};
  dercap_req_len = sizeof(sl_eder_client_send_dercap_req);
  if (recv_buf_len == dercap_req_len) {
    memcpy(&req, recv_buf, recv_buf_len);
    EVCCSendDercap(&req);
  }
  else 
  {
    fprintf(stdout, "received dercap ind but improper lengths; struct len = %d, recvd len = %d \n", dercap_req_len, recv_buf_len);
  }
}

void process_ders_req() {
  // data is available in the recv_buf
  int ders_req_len;
  sl_eder_client_send_ders_req req = {0};
  ders_req_len = sizeof(sl_eder_client_send_ders_req);
  if (recv_buf_len == ders_req_len) {
    memcpy(&req, recv_buf, recv_buf_len);
    EVCCDersFromVehicleCAN(&req);
    // fprintf(stdout, "EVccsENDdERS nOT iMPLEMENTED. TODO\n");
  }
  else 
  {
    fprintf(stdout, "received ders req but improper lengths; struct len = %d, recvd len = %d \n", ders_req_len, recv_buf_len);
  }
}

void process_dersett_req() {
  // data is available in the recv_buf
  int dersett_req_len;
  sl_eder_client_send_dersett_req dersett_req = {0};
  dersett_req_len = sizeof(sl_eder_client_send_dersett_req);
  if (recv_buf_len == dersett_req_len) {
    memcpy(&dersett_req, recv_buf, recv_buf_len);
    EVCCSendDersett(&dersett_req);
    // fprintf(stdout, "EVCCSendDersett nOT iMPLEMENTED. TODO\n");
  }
  else 
  {
    fprintf(stdout, "received dersett ind but improper lengths; struct len = %d, recvd len = %d \n", dersett_req_len, recv_buf_len);
  }
}

void process_ps_req() {
  // data is available in the recv_buf
  int ps_req_len;
  sl_epwrstat_client_send_ps_req req = {0};
  ps_req_len = sizeof(sl_epwrstat_client_send_ps_req);
  if (recv_buf_len == ps_req_len) {
    memcpy(&req, recv_buf, recv_buf_len);
    EVCCSendpwrstat(&req);
  }
  else 
  {
    fprintf(stdout, "received ps req but improper lengths; struct len = %d, recvd len = %d \n", ps_req_len, recv_buf_len);
  }
}

void process_dera_req() {
  // data is available in the recv_buf
  int dera_req_len;
  sl_eder_client_send_dera_req req = {0};
  dera_req_len = sizeof(sl_eder_client_send_dera_req);
  if (recv_buf_len == dera_req_len) {
    memcpy(&req, recv_buf, recv_buf_len);
    EVCCSendDera(&req);
  }
  else 
  {
    fprintf(stdout, "received dera req but improper lengths; struct len = %d, recvd len = %d \n", dera_req_len, recv_buf_len);
  }
}

void process_devinfo_req() {
  // data is available in the recv_buf
  int devinfo_req_len;
  sl_edinfo_client_send_dinfo_req req = {0};
  devinfo_req_len = sizeof(sl_edinfo_client_send_dinfo_req);
  if (recv_buf_len == devinfo_req_len) {
    memcpy(&req, recv_buf, recv_buf_len);
    EVCCSendDevInfo(&req);
  }
  else 
  {
    fprintf(stdout, "received devinfo ind but improper lengths; struct len = %d, recvd len = %d \n", devinfo_req_len, recv_buf_len);
  }
}

void writeToFile(char * wr_buf, int wr_buf_len)
{
    int f = open("/tmp/read_mup_bytes.dat", O_RDWR | O_CREAT);
    if (f == -1) {
        perror("mup data wr open failed");
        exit(1);
    }

    int written = write(f, wr_buf, wr_buf_len);
    if (written == -1) {
        perror("write failed");
        close(f);
        exit(1);
    } else if (written != wr_buf_len) {
        fprintf(stderr, "wrote %d bytes, expected %d\n", written, wr_buf_len);
    }
    printf("mup buf sz %d, wrote %d bytes to file %s\n", wr_buf_len, written, "/tmp/read_mup_bytes.dat");
    close(f);

}


void process_mup_setup_req()
{
  // data is available in recv_buf
  int length;
  // SL_METER_MIRROR_CLIENT_SEND_MUP_REQ
  //  TODO: FILL UP MRID OF mup and mrGroup[0], and LFDI IN SEP2 APP
  sl_meter_mirror_client_send_mup_req *pmup_req = NULL;
  length = sizeof(sl_meter_mirror_client_send_mup_req) + 1*sizeof(sl_mr_group_t);
  pmup_req = malloc(length);
  if (pmup_req == NULL)
  {
      fprintf(stdout, "error alloc memory for mup request\n");
      perror("mem_alloc:");
      fprintf(stdout, "usock proto: received MUP Setup Req, unable to process MUP Req setup\n");
      return;
  }
  if (recv_buf_len == length) {
    //writeToFile(recv_buf, recv_buf_len );
    fprintf(stdout, "Before buf cp num read gps %d \n", pmup_req->numMeterReadingGroups);
    fprintf(stdout, "Before buf cp read type exists %d\n", pmup_req->mrGroup[0].readingExists);
    memcpy(pmup_req, recv_buf, recv_buf_len);
    fprintf(stdout, "After buf cp num read gps %d \n", pmup_req->numMeterReadingGroups);
    fprintf(stdout, "After buf cp read type exists %d\n", pmup_req->mrGroup[0].readingExists);
    EVCCSetupMupReq(pmup_req);
  }
  else 
  {
    fprintf(stdout, "received MUP Setup req but improper lengths; struct len = %d, recvd len = %d \n", length, recv_buf_len);
  }
  if (pmup_req != NULL)
  {
    free(pmup_req);
  }
}

void process_mmr_setup_or_data_req()
{
  // data is available in recv_buf
  int length;
  // SL_METER_MIRROR_CLIENT_SEND_MMR_REQ
  //  TODO: FILL UP MRID OF mup and mrGroup[0],
  sl_meter_mirror_client_send_mmr_req *pmmr_req = NULL;
  length = sizeof(sl_meter_mirror_client_send_mmr_req) + 1*sizeof(sl_mr_group_t);
  pmmr_req = malloc(length);
  if (pmmr_req == NULL)
  {
      fprintf(stdout, "error alloc memory for mmr request\n");
      perror("mem_alloc:");
      fprintf(stdout, "usock proto: received MMR Setup or Data Req, unable to process Req\n");
      return;
  }
  if (recv_buf_len == length) {
    memcpy(pmmr_req, recv_buf, recv_buf_len);
    EVCCMmrReq(pmmr_req);
  }
  else 
  {
    fprintf(stdout, "received MMR Setup or Data req but improper lengths; struct len = %d, recvd len = %d \n", length, recv_buf_len);
  }
  if (pmmr_req != NULL)
  {
    free(pmmr_req);
  }
}

/* TODO: Process packets in proc_buf, data
  process both complete and incomplete packets. If complete packets recvd,
  proc_buf_len is 0, else it contains the previous remaining data
  after processing copy to the pipe buffer
*/
void processPackets()
{
    int bytesProcessed = 0;

    while (bytesProcessed < proc_buf_len) {
        if (proc_buf_len - bytesProcessed < 8) {
            // Insufficient data for tag and length
            break;
        }

        int t_length = bytesToInt(proc_buf, bytesProcessed);
        if (t_length == 0) 
        {
          fprintf(stdout, "---->>>>computed length is 0 exit loop discard data<<<<------ \n" );
          // discard packet
          bytesProcessed = proc_buf_len;
          break;
        }
        // int t_tag = bytesToInt(proc_buf, bytesProcessed + 4);
       
        if (proc_buf_len - bytesProcessed < t_length) {
            // Incomplete packet
            fprintf(stdout, "---->>>>ProcPack<<<<------ exp len %d \n", t_length);
            break;
        }

        memcpy(recv_buf, &proc_buf[bytesProcessed] , t_length);
        recv_buf_len = t_length;

        // Process the complete packet
        //processLTVPacket();
        add_to_task_pipe(recv_buf, 0, recv_buf_len);

        // Update the bytes processed
        bytesProcessed += t_length;
        displayTime();
        printf("bytesProc = %d, t_len = %d, proc buf len = %d\n", bytesProcessed, t_length, proc_buf_len);
    }

    if (bytesProcessed < proc_buf_len) {
        // copy the last incomplete packet to the beginning of recv_buf
        // processIncompletePackets(buffer + bytesProcessed, proc_buf_len - bytesProcessed);
        recv_buf_len = proc_buf_len - bytesProcessed;
        memcpy(recv_buf, proc_buf + bytesProcessed, recv_buf_len);
        memcpy(proc_buf, recv_buf, recv_buf_len);
        proc_buf_len = recv_buf_len;
    }
    else 
    {
        proc_buf_len = 0;
    }
}


u32_t process_recv_message(char* rd_buf, int rd_buf_len) {
  // copy rd_buf into proc_buf at proc_buf_len
  fprintf(stdout, "---->>>>====<<<<---- process recv proc buf len[max 4096] %d, rd buf len %d\n", proc_buf_len, rd_buf_len);
  memcpy(&proc_buf[proc_buf_len], rd_buf, rd_buf_len);
  proc_buf_len += rd_buf_len;

  processPackets();

}

u32_t process_message_app_task()
{
  int ii;
  ii = get_num_task_pipe_elements();
  if (ii > 0)
  {
    fprintf(stdout, "process usock message in apptask num elements = %d\n", ii);
 
  }

  while (ii > 0) 
  {
    if (get_from_task_pipe())
    {
      // data now in recv_buf in App_task
      fprintf(stdout, "msg type: 0x%02x, msg len: %d \n", proto_message_type, proto_message_len);
    }
    if (proto_message_type == (u32_t) SL_EDER_CLIENT_SEND_DERCAP_REQ)
    {
      process_dercap_req();        
    }
    else if (proto_message_type == (u32_t) SL_EDER_CLIENT_SEND_DERSETT_REQ)
    {
      process_dersett_req();
    }
    else if (proto_message_type == (u32_t) SL_EDINFO_CLIENT_SEND_DINFO_REQ)
    {
      process_devinfo_req();
    }
    else if (proto_message_type == (u32_t) SL_EDER_CLIENT_SEND_DERA_REQ)
    {
      process_dera_req();
    }
    else if (proto_message_type == (u32_t) SL_EDER_CLIENT_SEND_DERS_REQ)
    {
      process_ders_req();
    }
    else if (proto_message_type == (u32_t) SL_EDER_SERVER_DERA_IND) 
    {
      // data is available in recv_buf
      test_process_dera_ind();
    }
    else if (proto_message_type == (u32_t) SL_EPWRSTAT_CLIENT_SEND_PS_REQ)
    {
      process_ps_req();
    }
    else if (proto_message_type == (u32_t) SL_METER_MIRROR_CLIENT_SEND_MUP_REQ)
    {
      process_mup_setup_req();
    }
    else if (proto_message_type == (u32_t) SL_METER_MIRROR_CLIENT_SEND_MMR_REQ)
    {
      process_mmr_setup_or_data_req();
    }

    ii--;
  }
}


// u32_t process_recv_message(char* rd_buf, int rd_buf_len) {
//     // return the type of message received, and copy the contents of the message into recv_buf
//     u32_t  proto_message_len = 0;
//     u32_t  proto_message_type = 0;
//     int ii;
//     if (rd_buf_len <10)
//     {
//       printf("insufficient message packet recvd: lenght recvd: %d\n", rd_buf_len);
//       return 0; // insufficient data for processing
//     }
//     //debug
//     printf("start of processing message \n");
//     printf("length of packet arg = %d\n", rd_buf_len);
//     ii = 0;
//     while (ii < rd_buf_len)
//     {
//       memcpy(&proto_message_len, &rd_buf[ii], 4);
//       //debug
//       printf("packet len = %d\n", proto_message_len);
//       memcpy(&proto_message_type, &rd_buf[ii+4], 4);
//       printf("-------\n packet type = %02x\n ---------\n", proto_message_type);
//       if (proto_message_len <= 8) {
//           printf("insufficient message packet recvd: length recvd: %d\n", proto_message_len);
//           return 0;
//       }
//       if (ii + proto_message_len <= rd_buf_len) {
//         //debug
//         printf("copying data to recv buf\n");
//         memcpy(recv_buf, (void*)&rd_buf[ii+8], proto_message_len - 8);
//         recv_buf_len = proto_message_len - 8;
//         //console_log("recvd msg type %02x\n", proto_message_type);

//         if (proto_message_type == (u32_t) SL_EDER_CLIENT_SEND_DERCAP_REQ)
//         {
//           process_dercap_req();        
//         }
//         else if (proto_message_type == (u32_t) SL_EDER_CLIENT_SEND_DERSETT_REQ)
//         {
//           process_dersett_req();
//         }
//         else if (proto_message_type == (u32_t) SL_EDINFO_CLIENT_SEND_DINFO_REQ)
//         {
//           process_devinfo_req();
//         }
//         else if (proto_message_type == (u32_t) SL_EDER_CLIENT_SEND_DERA_REQ)
//         {
//           process_dera_req();
//         }
//         else if (proto_message_type == (u32_t) SL_EDER_CLIENT_SEND_DERS_REQ)
//         {
//           process_ders_req();
//         }
//         else if (proto_message_type == (u32_t) SL_EDER_SERVER_DERA_IND) 
//         {
//           // data is available in recv_buf
//           test_process_dera_ind();
//         }
//         else if (proto_message_type == (u32_t) SL_EPWRSTAT_CLIENT_SEND_PS_REQ)
//         {
//           process_ps_req();
//         }


//         // else if (proto_message_type == (u32_t) SL_EDER_SERVER_DERCAP_IND)
//         // {
//         //   // data is available in recv_buf
//         //   process_dercap_ind();
//         // }
//         // else if (proto_message_type == (u32_t) SL_EDER_SERVER_DERSETT_IND)
//         // {
//         //   // data is available in recv_buf
//         //   process_dersett_req();
//         // }
//         // else if (proto_message_type == (u32_t) SL_EDER_SERVER_DERS_IND)
//         // {
//         //   // data is available in recv_buf
//         //   process_ders_ind();
//         // }
//         // else if (proto_message_type == (u32_t) SL_EPWRSTAT_SERVER_PS_IND)
//         // {
//         //   process_ps_ind();
//         // }
//         // else if (proto_message_type == (u32_t) SL_EDINFO_CLIENT_SEND_DINFO_REQ)
//         // {
//         //   process_devinfo_req();
//         // }
//         ii += proto_message_len;
//       }
//       // else {
//       //   printf("recvd msg type %02x, incomplete message\n", proto_message_type);
//       //   //console_log("recvd msg type %02x, incomplete message\n", proto_message_type);
//       //   return 0; // UNKNOWN message type since incomplete message
//       // }
//     }
//     return proto_message_type; // message type
// }

void send_der_setting(sl_der_settings_t der_sett ) {
  // CAN2SEP_DER_SETTINGS_T_ID
  u8_t buf_wr[2048] ;
  int buf_wr_len = 0;
  buf_wr_len = sizeof(der_sett);
  memcpy(buf_wr, &der_sett, buf_wr_len);
  send_message((void *)buf_wr, buf_wr_len, (u32_t) CAN2SEP_DER_SETTINGS_T_ID);
  fprintf(stdout, "Sent EVSE DERSetting to CAN2SEP2 app [id: %d] => (Len: %d) \n", CAN2SEP_DER_SETTINGS_T_ID, buf_wr_len);
}

void send_default_derc(sl_der_default_derc_t def_derc) {
  // CAN2SEP_DER_DEFAULT_DERC_T_ID
  u8_t buf_wr[2048] ;
  int buf_wr_len = 0;
  buf_wr_len = sizeof(def_derc);
  memcpy(buf_wr, &def_derc, buf_wr_len);
  send_message((void *)buf_wr, buf_wr_len, (u32_t) CAN2SEP_DER_DEFAULT_DERC_T_ID);
  fprintf(stdout, "Sent Default DERC to CAN2SEP2 app [id: %d] => (Len: %d) \n", CAN2SEP_DER_DEFAULT_DERC_T_ID, buf_wr_len);
}

void send_derc_ind(sl_der_client_derc_ind * derc_ind) {
  // CAN2SEP_DER_DERC_T_ID
  u8_t buf_wr[2048] ;
  int buf_wr_len = 0;
  buf_wr_len = sizeof(sl_der_client_derc_ind);
  memcpy(buf_wr, derc_ind, buf_wr_len);
  // debug
  fprintf(stdout, "**** DBG <><><><>  send derc len %d, message id %d \n", buf_wr_len, CAN2SEP_DER_DERC_IND_T_ID);
  send_message((void *)buf_wr, buf_wr_len, (u32_t) CAN2SEP_DER_DERC_IND_T_ID);
  fprintf(stdout, "Sent DERC Ind to CAN2SEP2 app [id: %d] => (Len: %d) \n", CAN2SEP_DER_DERC_IND_T_ID, buf_wr_len);
}


void send_freq_droop_data(sl_xsd_freq_droop_t freq_droop) {
  //CAN2SEP_FREQ_DROOP_T_ID
  u8_t buf_wr[2048] ;
  int buf_wr_len = 0;
  buf_wr_len = sizeof(freq_droop);
  memcpy(buf_wr, &freq_droop, buf_wr_len);
  send_message((void *)buf_wr, buf_wr_len, (u32_t) CAN2SEP_FREQ_DROOP_T_ID);
  fprintf(stdout, "Sent Freq Droop Data to CAN2SEP2 app [id: %d] => (Len: %d) \n", CAN2SEP_FREQ_DROOP_T_ID, buf_wr_len);
}

//void send_curve_data(sl_der_curve_t curve) {
// sending dc_ind as opposed to curve only since it contains derc mrdi
void send_curve_data(sl_der_client_dc_ind * curveInd) {
  //sl_der_client_dc_ind
  // CAN2SEP_DER_CURVE_T_ID
  u8_t buf_wr[2048] ;
  int buf_wr_len = 0;
  buf_wr_len = sizeof(*curveInd);
  memcpy(buf_wr, curveInd, buf_wr_len);
  send_message((void *)buf_wr, buf_wr_len, (u32_t) CAN2SEP_DER_CURVE_T_ID);
  fprintf(stdout, "Sent Curve [Type: %d] Data to CAN2SEP2 app [Curve: curve] [id: %d] => (Len: %d) \n", 
      curveInd->curve.curveType, CAN2SEP_DER_CURVE_T_ID, buf_wr_len);
  //debug only
  fprintf(stdout, "Data first four bytes, and last 4 bytes are: \n");
  fprintf(stdout, "%2x, %2x, %2x, %2x, ......., %2x, %2x, %2x, %2x\n",
    buf_wr[0], buf_wr[1], buf_wr[2], buf_wr[3], 
    buf_wr[buf_wr_len-4], buf_wr[buf_wr_len-3], buf_wr[buf_wr_len-2], buf_wr[buf_wr_len-1]);
}

void send_communication_status(u8_t status)
{
  u8_t buf_wr[1];
  buf_wr[0] = status;
  send_message((void*)buf_wr, 1, (u32_t) CAN2SEP_EVSE_COMMUNICATION_STATUS_ID);
  fprintf(stdout, "Sent Comm Status [%d] to CAN2SEP2 app \n", status);
}

void test_server_send_dera_ind() {
  sl_eder_server_dera_ind  tmp_dera_ind = {0};
  u8_t buf_wr[2048] ;
  int buf_wr_len = 0;
  tmp_dera_ind.deraExists = true;
  tmp_dera_ind.dera.availabilityDurationExists = true;
  tmp_dera_ind.dera.availabilityDuration = 4800;
  tmp_dera_ind.dera.maxChargeDurationExists = true;
  tmp_dera_ind.dera.maxChargeDuration = 4*60;
  buf_wr_len = sizeof(tmp_dera_ind);
  memcpy(buf_wr, &tmp_dera_ind, buf_wr_len);
  send_message((void *)buf_wr, buf_wr_len, (u32_t) SL_EDER_SERVER_DERA_IND);
}

void test_client_send_derc_ind() {
  sl_der_client_derc_ind  tmp_derc_ind = {0};
  u8_t buf_wr[2048] ;
  int buf_wr_len = 0;
  tmp_derc_ind.derpMrId[0] = 'a'; 
  tmp_derc_ind.derpMrId[1] = 'b';
  tmp_derc_ind.derpMrId[2] = 'c';
  tmp_derc_ind.derpMrId[3] = 'd';
  tmp_derc_ind.derpMrId[4] = 'e';
  tmp_derc_ind.derpMrId[5] = 'f';
  tmp_derc_ind.derpMrId[6] = '0';
  tmp_derc_ind.derpMrId[7] = '1'; 
  tmp_derc_ind.derpMrId[8] = '2';
  tmp_derc_ind.derpMrId[9] = '3';
  tmp_derc_ind.derpMrId[10] = '4';
  tmp_derc_ind.derpMrId[11] = '5';
  tmp_derc_ind.derpMrId[12] = '6';
  tmp_derc_ind.derpMrId[13] = '7';
  tmp_derc_ind.derpMrId[14] = '8'; 
  tmp_derc_ind.derpMrId[15] = '9';
  tmp_derc_ind.derc.dercBase.rampTmsExists = 0 ;
  tmp_derc_ind.derc.dercBase.rampTms=7623;
  buf_wr_len = sizeof(tmp_derc_ind);
  memcpy(buf_wr, &tmp_derc_ind, buf_wr_len);
  send_message((void *)buf_wr, buf_wr_len, (u32_t) SL_DER_CLIENT_DERC_IND);
}

void send_mail_message_to_server(void* pMail, u32_t message_type) 
{
  int msg_len = 0;
  int sent_len = 0;
  fprintf(stdout, "Sending Mail message type = %02x to server \n", message_type);
  switch (message_type)
  {
    case (u32_t) SL_DER_CLIENT_DC_IND:
      msg_len = sizeof(*(sl_der_client_dc_ind*)pMail);
      break;

    case (u32_t) SL_DER_CLIENT_DERC_IND:
      msg_len = sizeof(*(sl_der_client_derc_ind*)pMail);
      break;

    case (u32_t) SL_DER_CLIENT_DEFAULT_DERC_IND:
      msg_len = sizeof(*(sl_der_client_default_derc_ind*)pMail);
      break;

    case (u32_t) SL_DER_CLIENT_DERP_IND:
      msg_len = sizeof(*(sl_der_client_derp_ind*)pMail);
      break;

    case (u32_t) SL_DER_CLIENT_DERP_LIST_IND:
      msg_len = sizeof(*(sl_der_client_derp_list_ind*)pMail);
      break;

    case (u32_t) SL_DER_CLIENT_DERC_LIST_IND:
      msg_len = sizeof(*(sl_der_client_derc_list_ind*)pMail);
      break;

    case (u32_t) SL_DER_CLIENT_DC_LIST_IND:
      msg_len = sizeof(*(sl_der_client_derc_list_ind*)pMail);
      break;
    
    default:
      break;
  }
  if (msg_len != 0)
  {
    send_buf_len = msg_len + 8;
    memset(send_buf, 0, BUF_SIZE);
    //memcpy(send_mail_buf, *pMail, send_mail_buf_len);
    memcpy(send_buf, &send_buf_len, 4);
    memcpy((void*)&send_buf[4], &message_type, 4);
    memcpy((void*)&send_buf[8], pMail, send_buf_len);
    sent_len = usock_write_message(send_buf, send_buf_len );
    fprintf(stdout, "Sent %d bytes to server\n", send_buf_len);
  }
  else 
  {
    fprintf(stdout, "Client: Send Mail Message: Unsupported message id %d\n", message_type);
  }
}
