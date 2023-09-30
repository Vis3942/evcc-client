/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          app_tables.c
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \author        Chris Leclercq
 *
 * \brief         Contains function set configuration tables for app example code
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#include "psal_assert.h"
#include "kal_api.h"
#include "app_tables.h"

#include "app_client.h"
#include "app_dcap_client.h"
#include "app_der_client.h"
#include "app_eder_client.h"
#include "app_edev_client.h"
#include "app_edinfo_client.h"
#include "app_elel_client.h"
#include "app_epwrstat_client.h"
#include "app_fsa_client.h"
#include "app_meter_mirror_client.h"
#include "app_sder_client.h"
#include "app_time_client.h"


/*******************************************************************************
 * Extern Defines
 ******************************************************************************/
extern void app_SdevClientHandleMail(kal_mail_id_t mailId, void *pMail);

/*******************************************************************************
 * Local Types
 ******************************************************************************/
typedef void (*timeout_func_t)(u32_t eventMask);
typedef void (*mail_func_t)(kal_mail_id_t mailId, void *pMail);

typedef struct
{
  timeout_func_t timeoutHandler;
  mail_func_t    mailHandler;
} tables_entry_t;

/*******************************************************************************
 * appFuncsetTables - contains the following info for each of the client and server
 *             function sets:
 * - Handle Timeouts Function - Function that is called to handle timeout events
 * - Handle Mail Function - Function that is called to handle mail events
 ******************************************************************************/
const tables_entry_t appFuncsetTables[] =
{
//  Handle Timeouts Function          Handle Mail Function

#if APP_CONFIG_ECFG_CLIENT_ENABLED
    {NULLP,                           app_EconfigClientHandleMail      },
#endif
#if APP_CONFIG_DCAP_CLIENT_ENABLED
    {NULLP,                           app_DcapClientHandleMail         },
#endif
#if APP_CONFIG_DER_CLIENT_ENABLED
    {NULLP,                           app_DerClientHandleMail         },
#endif
#if APP_CONFIG_EDSTAT_CLIENT_ENABLED
    {NULLP,                           app_EdstatClientHandleMail        },
#endif
#if APP_CONFIG_EDINFO_CLIENT_ENABLED
    {NULLP,                           app_EdinfoClientHandleMail       },
#endif
#if APP_CONFIG_DRLC_CLIENT_ENABLED
    {NULLP,                           app_DrlcClientHandleMail        },
#endif
#if APP_CONFIG_EDER_CLIENT_ENABLED
    {NULLP,                           app_EderClientHandleMail        },
#endif
#if APP_CONFIG_EDEV_CLIENT_ENABLED
    {NULLP,                           app_EdevClientHandleMail        },
#endif
#if APP_CONFIG_EFILE_CLIENT_ENABLED
    {NULLP,                           app_EfileClientHandleMail       },
#endif
#if APP_CONFIG_FILE_CLIENT_ENABLED
    {NULLP,                           app_FileClientHandleMail        },
#endif
#if APP_CONFIG_FSA_CLIENT_ENABLED
    {NULLP,                           NULLP                           },
#endif
#if APP_CONFIG_FLOW_CLIENT_ENABLED
    {NULLP,                           app_FlowClientHandleMail        },
#endif
#if APP_CONFIG_SLEL_CLIENT_ENABLED
    {NULLP,                           NULLP                           },
#endif
#if APP_CONFIG_EDRLC_CLIENT_ENABLED
    {NULLP,                           app_EdrlcClientHandleMail         },
#endif
#if APP_CONFIG_METER_CLIENT_ENABLED
    {NULLP,                           app_MeterClientHandleMail       },
#endif
#if APP_CONFIG_METER_MIRROR_CLIENT_ENABLED
    {NULLP,                           app_MeterMirrorClientHandleMail },
#endif
#if APP_CONFIG_MSG_CLIENT_ENABLED
    {NULLP,                           app_MsgClientHandleMail         },
#endif
#if APP_CONFIG_SNS_CLIENT_ENABLED
    {NULLP,                           NULLP                           },
#endif
#if APP_CONFIG_NOTIFY_CLIENT_ENABLED
    {NULLP,                           NULLP                           },
#endif
#if APP_CONFIG_PPY_CLIENT_ENABLED
    {NULLP,                           NULLP                           },
#endif
#if APP_CONFIG_PRICE_CLIENT_ENABLED
    {NULLP,                           app_PriceClientHandleMail       },
#endif
#if APP_CONFIG_EPS_CLIENT_ENABLED
    {NULLP,                           app_EpwrstatClientHandleMail     },
#endif
#if APP_CONFIG_RESP_CLIENT_ENABLED
    {NULLP,                           NULLP                           },
#endif
#if APP_CONFIG_SDER_CLIENT_ENABLED
    {NULLP,                           app_SderClientHandleMail        },
#endif
#if APP_CONFIG_SDEV_CLIENT_ENABLED
    {NULLP,                           app_SdevClientHandleMail        },
#endif
#if APP_CONFIG_SFILE_CLIENT_ENABLED
    {NULLP,                           NULLP                           },
#endif
#if APP_CONFIG_SUBS_CLIENT_ENABLED
    {NULLP,                           NULLP                           },
#endif
#if APP_CONFIG_TIME_CLIENT_ENABLED
    {NULLP,                           app_TimeClientHandleMail        },
#endif
};


/*******************************************************************************
 * APP_HandleMail
 ******************************************************************************/
void APP_HandleMail(kal_mail_id_t mailId, void *pMail)
{
  i8_t x;
  //debug
  fprintf(stdout, "<<<<HandleMail app_tables >>>>> 0x%04x \n", mailId);
  //debug:end
  for (x=0; x<(sizeof(appFuncsetTables)/sizeof(tables_entry_t)); x++)
  {
    if (appFuncsetTables[x].mailHandler != NULLP)
      (*appFuncsetTables[x].mailHandler)(mailId, pMail);
  }
}

/*******************************************************************************
 * APP_HandleTimeouts
 ******************************************************************************/
void APP_HandleTimeouts(u32_t eventMask)
{
  i8_t x;

  for (x=0; x<(sizeof(appFuncsetTables)/sizeof(tables_entry_t)); x++)
  {
    if (appFuncsetTables[x].timeoutHandler != NULLP)
      (*appFuncsetTables[x].timeoutHandler)(eventMask);
  }
}
