#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include "kal_api.h"
#include "utils.h"
#include "usock_proto.h"
#include "app_meter_mirror_client.h"

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
int opt_simulate_vehicle = 0;
int g_transaction_id = 0;

int derp_pull_secs=30;
int derc_pull_secs=30;
int derc_pull_offset_secs=15;

// for simulate vehdata
#define DEFAULT_MMR_DESCRIPTION    "Stellantis PEV Data"
#define DEFAULT_MMR_NUM_MR_GROUPS     (SL_METER_MIRROR_MAX_MR_GROUPS)
#define DEFAULT_NUM_MMRS (4)
#define DEFAULT_ACTIVEPOWER_DESCRIPTION  "Active Power"
#define DEFAULT_REACTIVEPOWER_DESCRIPTION "Reactive Power"
#define DEFAULT_VOLTAGE_DESCRIPTION  "Voltage"
#define DEFAULT_FREQUENCY_DESCRIPTION "Frequency"
// Default Reading Type information required for Instantaneous demand data
// See SEP2.0 specification, table 12-3.
static sl_cmn_rt_t defaultRtype = {
  SL_XSD_AB_INSTANTANEOUS,       //sl_xsd_acc_behavior_type_t accumulationBehaviour;
  {false, 0},                    // sl_xsd_unit_val_t          calorificValue;
  {false, 0},                    // sl_xsd_unit_val_t          conversionFactor;
  SL_XSD_COMMODITY_ELEC_SECOND,  // sl_xsd_commodity_type_t    commodity;
  SL_XSD_DATA_QUAL_NOT_APPL,     // sl_xsd_data_qual_type_t    dataQualifier;
  SL_XSD_FLOW_DIR_FORWARD,       // sl_xsd_flow_dir_type_t     flowDirection;
  0,                             // u32_t                      intervalLength;
  false,                         // bool                       intervalLengthExists;
  SL_XSD_KIND_DEMAND,            // sl_xsd_kind_type_t         kindType;
  0,                             // u8_t                       maxNumIntervals;
  false,                         // bool                       maxNumIntervalsExists;
  0,                             // u8_t                       numConsumBlocks;
  false,                         // bool                       numConsumBlocksExists;
  0,                             // u8_t                       numTouTiers;
  false,                         // bool                       numTouTiersExists;
  SL_XSD_PHASE_CODE_NOT_APPL,    // sl_xsd_phase_code_type_t   phaseCode;
  SL_XSD_PTM_NONE,               // sl_xsd_ptm_type_t          powerOfTenMultiplier;
  0,                             // u32_t                      subIntervalLength;
  false,                         // bool                       subIntervalLengthExists;
  0,                             // u64_t                      supplyLimit;
  false,                         // bool                       supplyLimitExists;
  0,                             // bool                       tieredConsumptionBlocks;
  false,                         // bool                       tieredConsumptionBlocksExists;
  SL_XSD_UOM_WATT,               // sl_xsd_uom_t               uomType;
};

time_t getCurrentTime()
{
  time_t secs = time(NULL);
  return secs;
}

// for simulate vehdata: end

int startsWith(const char *a, const char *b)
{
   if(strncmp(a, b, strlen(b)) == 0) return 1;
   return 0;
}

#define DELIM_PARAM "="
void read_parameters_from_file(char * param_file_name)
{
  // default values if not specified in file
  derp_pull_secs = 30;
  derc_pull_secs = 30;
  derc_pull_offset_secs = 10;
  FILE *file = fopen (param_file_name, "r");
  size_t tmpst;
  if (file != NULL)
  {
    char line[1024];
    while(fgets(line, sizeof(line), file) != NULL)
    {
      char *cfline;
      cfline = strstr((char *)line,DELIM_PARAM);
      cfline = cfline + strlen(DELIM_PARAM);
      if (startsWith(line, "derp_pull_secs"))
      {
        tmpst = strlen(cfline);
        cfline[tmpst-1] = '\0';
        derp_pull_secs = atoi(cfline);
      }
      else if (startsWith(line, "derc_pull_secs"))
      {
        tmpst = strlen(cfline);
        cfline[tmpst-1] = '\0';
        derc_pull_secs = atoi(cfline);
      }
      else if (startsWith(line, "derc_pull_offset_secs"))
      {
        tmpst = strlen(cfline);
        cfline[tmpst-1] = '\0';
        derc_pull_offset_secs = atoi(cfline);
      }

    } // End while
    fclose(file);
    // debug
    fprintf(stdout, "Params from %s\n", param_file_name);
    fprintf(stdout, "derp interval secs = %d, derc interval secs = %d, derc offset secs=%d", 
        derp_pull_secs, derc_pull_secs, derc_pull_offset_secs);
  } 
  else 
  {
    fprintf(stdout, "Unable to locate %s file. Using default values.\n", param_file_name);
  } // End if file
}

u8_t FromHex(char c)
{
  switch(c)
    {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': return 10;
    case 'b': return 11;
    case 'c': return 12;
    case 'd': return 13;
    case 'e': return 14;
    case 'f': return 15;
    case 'A': return 10;
    case 'B': return 11;
    case 'C': return 12;
    case 'D': return 13;
    case 'E': return 14;
    case 'F': return 15;
    default : return 0xFF;
    }
    return 0xFF;
}

void hexString2ByteArray (const char * hex_str, u8_t * data_array, int max_len_data_array)
{
  int first_elem_single_digit;
  int tmpi;
  int ii, jj;  
  int b1, b2;
  int num_elements = strlen(hex_str);
  if (num_elements % 2 == 1) 
  {
    first_elem_single_digit = 1;
    tmpi = (num_elements + 1)/ 2;
  }
  else 
  {
    first_elem_single_digit = 0;
    tmpi = (num_elements)/ 2;  
  }
  //fprintf(stdout, "[%d] => %s\n", num_elements, hex_str );
  if (tmpi <= max_len_data_array)
  {
    ii = 0;
    jj = ii;
    while (ii < tmpi)
    {
      if (ii == 0)
      {
        if (first_elem_single_digit == 1)
        {
          data_array[0] = FromHex(hex_str[0]);
          jj = 1;
        }
        else 
        {
          b1 = FromHex(hex_str[0]);
          b2 = FromHex(hex_str[1]);
          if ( (b1 != 0xFF) && (b2 != 0xFF) )
          {
            data_array[0] = (b1 << 4) | (b2);
          }
          else
          {
            data_array[0] = 0xFF;
          }
          jj = 2;
        }
        ii++;
      }
      else
      {
        b1 = FromHex(hex_str[jj]);
        b2 = FromHex(hex_str[jj+1]);
        if ( (b1 != 0xFF) && (b2 != 0xFF) )
        {
          data_array[ii] = (b1 << 4) | (b2);
        }
        else
        {
          data_array[ii] = 0xFF;
        }
        // debug
        // fprintf(stdout, "dbg: %c%c => %d\n", hex_str[jj], hex_str[jj+1], data_array[ii]);
        ii++;
        jj += 2;
      }
    }
  }
  else
  {
    for (ii=0; ii<max_len_data_array; ii++)
    {
      data_array[ii] = 0xFF;
    } 
  }
}
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
  //fprintf(stdout, "bytesToInt 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", bytes[start_idx], bytes[start_idx + 1],
  //      bytes[start_idx+2], bytes[start_idx+3]);
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


void simulate_veh_send_mup_req()
{
  int descLen;
  int ii;
  // SL_METER_MIRROR_CLIENT_SEND_MUP_REQ
  //  TODO: FILL UP MRID OF mup and mrGroup[0], and LFDI IN SEP2 APP
  sl_meter_mirror_client_send_mup_req *pmup_req;
  int length = sizeof(sl_meter_mirror_client_send_mup_req) + 1*sizeof(sl_mr_group_t);
  pmup_req = malloc(length);
  if (pmup_req == NULL) 
  {
      printf("error alloc memory for mup request\n");
      perror("mem_alloc:");
      printf("failed to allocate Memory for MUP Request, unable to send MUP Request to EVCC\n");
      return;
  }    
  descLen = strlen(DEFAULT_MMR_DESCRIPTION);
  if (descLen >= SL_XSD_DESCRIPTION_LEN)
  descLen = SL_XSD_DESCRIPTION_LEN - 1;
  strncpy(pmup_req->mrGroup[0].description, DEFAULT_MMR_DESCRIPTION, descLen);

  pmup_req->roleFlags = SL_XSD_ROLE_FLAG_PEV_MASK;
  pmup_req->serviceCategory = SL_XSD_SERVICE_ELECTRICITY;
  pmup_req->status = SL_XSD_STATUS_ON;
  pmup_req->numMeterReadingGroups = 1;

  // Readings and Reading sets are not included in the MUP
  pmup_req->mrGroup[0].numReadingSets = 0;
  pmup_req->mrGroup[0].readingExists  = false;

  pmup_req->mrGroup[0].rtypeExists = true;
  pmup_req->mrGroup[0].rtype.accumulationBehaviour = SL_XSD_AB_INSTANTANEOUS;
  pmup_req->mrGroup[0].rtype.flowDirection = SL_XSD_FLOW_DIR_FORWARD; // or SL_XSD_FLOW_DIR_REVERSE
  pmup_req->mrGroup[0].rtype.phaseCode = 132;
  pmup_req->mrGroup[0].rtype.powerOfTenMultiplier = SL_XSD_PTM_NONE;
  pmup_req->mrGroup[0].rtype.uomType = SL_XSD_UOM_WATT;
  pmup_req->mrGroup[0].rtype.kindType = SL_XSD_KIND_POWER;


  EVCCSetupMupReq(pmup_req);

  if (pmup_req != NULL)
  {
      free(pmup_req);
  }
}

void simulate_veh_send_mmr_setup_or_data(int typeof_req, bool sendReading)
{
    // if sendReading is true, values are included else it is a setup request
    u16_t length;
    u8_t descLen;
    // typeof_req == 1 (Active Power)
    // typeof_req == 2 (Reactive Power)
    // typeof_req == 3 (Voltage)
    // typeof_req == 4 (Freqency)
    // any other value do nothing

    // in SEP2 app, look at the value of mrGroup[0].description to figure what this MMR represents
    if ((typeof_req < 1) || (typeof_req > 4))
    {
        return;
    }

    sl_meter_mirror_client_send_mmr_req *pmmr_req = {0};
    length = sizeof(sl_meter_mirror_client_send_mmr_req) + DEFAULT_MMR_NUM_MR_GROUPS*sizeof(sl_mr_group_t);
    pmmr_req = malloc(length);
    memset(pmmr_req, 0, length);

    pmmr_req->numMeterReadingGroups = DEFAULT_MMR_NUM_MR_GROUPS;

    if (sendReading)
    {
        pmmr_req->mrGroup[0].readingExists = 1;
        pmmr_req->mrGroup[0].reading.duration = 0;
        pmmr_req->mrGroup[0].reading.start = (i64_t)getCurrentTime();

    }

    switch (typeof_req)
    {
    case 1:
        // Copy description field
        descLen = strlen(DEFAULT_ACTIVEPOWER_DESCRIPTION);
        if (descLen >= SL_XSD_DESCRIPTION_LEN)
        descLen = SL_XSD_DESCRIPTION_LEN - 1;
        strncpy(pmmr_req->mrGroup[0].description, DEFAULT_ACTIVEPOWER_DESCRIPTION, descLen);

        defaultRtype.accumulationBehaviour = SL_XSD_AB_INSTANTANEOUS;
        defaultRtype.flowDirection = SL_XSD_FLOW_DIR_FORWARD; // or SL_XSD_FLOW_DIR_REVERSE
        defaultRtype.phaseCode = 132;
        defaultRtype.powerOfTenMultiplier = SL_XSD_PTM_NONE;
        defaultRtype.uomType = SL_XSD_UOM_WATT;

        if (sendReading)
        {
            pmmr_req->mrGroup[0].reading.valueExists = true;
            pmmr_req->mrGroup[0].reading.value = 6400;
        }

        break;

    case 2:
        // Copy description field
        descLen = strlen(DEFAULT_REACTIVEPOWER_DESCRIPTION);
        if (descLen >= SL_XSD_DESCRIPTION_LEN)
        descLen = SL_XSD_DESCRIPTION_LEN - 1;
        strncpy(pmmr_req->mrGroup[0].description, DEFAULT_REACTIVEPOWER_DESCRIPTION, descLen);

        defaultRtype.accumulationBehaviour = SL_XSD_AB_INSTANTANEOUS;
        defaultRtype.flowDirection = SL_XSD_FLOW_DIR_FORWARD; // or SL_XSD_FLOW_DIR_REVERSE
        defaultRtype.phaseCode = 132;
        defaultRtype.powerOfTenMultiplier = SL_XSD_PTM_NONE;
        defaultRtype.uomType = SL_XSD_UOM_REACT_POWER;

        if (sendReading)
        {
            pmmr_req->mrGroup[0].reading.valueExists = true;
            pmmr_req->mrGroup[0].reading.value = 200;
        }


        break;
    case 3:
        // Copy description field
        descLen = strlen(DEFAULT_VOLTAGE_DESCRIPTION);
        if (descLen >= SL_XSD_DESCRIPTION_LEN)
        descLen = SL_XSD_DESCRIPTION_LEN - 1;
        strncpy(pmmr_req->mrGroup[0].description, DEFAULT_VOLTAGE_DESCRIPTION, descLen);

        defaultRtype.accumulationBehaviour = SL_XSD_AB_INSTANTANEOUS;
        defaultRtype.flowDirection = SL_XSD_FLOW_DIR_FORWARD; // or SL_XSD_FLOW_DIR_REVERSE
        defaultRtype.phaseCode = 132;
        defaultRtype.powerOfTenMultiplier = SL_XSD_PTM_NONE;
        defaultRtype.uomType = SL_XSD_UOM_VOLT_SQ;
   
        if (sendReading)
        {
            pmmr_req->mrGroup[0].reading.valueExists = true;
            pmmr_req->mrGroup[0].reading.value = 11000;
        }

        break;
    case 4:
        // Copy description field
        descLen = strlen(DEFAULT_FREQUENCY_DESCRIPTION);
        if (descLen >= SL_XSD_DESCRIPTION_LEN)
        descLen = SL_XSD_DESCRIPTION_LEN - 1;
        strncpy(pmmr_req->mrGroup[0].description, DEFAULT_FREQUENCY_DESCRIPTION, descLen);

        defaultRtype.accumulationBehaviour = SL_XSD_AB_INSTANTANEOUS;
        defaultRtype.flowDirection = SL_XSD_FLOW_DIR_FORWARD; // or SL_XSD_FLOW_DIR_REVERSE
        defaultRtype.phaseCode = 132;
        defaultRtype.powerOfTenMultiplier = SL_XSD_PTM_NONE;
        defaultRtype.uomType = SL_XSD_UOM_FREQ;

        if (sendReading)
        {
            pmmr_req->mrGroup[0].reading.valueExists = true;
            pmmr_req->mrGroup[0].reading.value = 6000;
        }

        break;
    default:
        return;
    }

    // Reading sets are not included in these MMRs.
    pmmr_req->mrGroup[0].numReadingSets = 0;

    pmmr_req->mrGroup[0].rtypeExists = 1;
    memcpy(&pmmr_req->mrGroup[0].rtype, &defaultRtype, sizeof(sl_cmn_rt_t));

    EVCCMmrReq(pmmr_req);

    if (pmmr_req != NULL)
    {
        free(pmmr_req);
    }
}

