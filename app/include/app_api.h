/******************************************************************************
 * KITU SYSTEMS SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Kitu Systems, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Kitu Systems, Inc and is confidential in nature.
 *
 * \brief         Application Layer public header file
 *
 * \file          app_api.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         APIs supported by the application
 *
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef APP_API_H
#define APP_API_H

#include "psal_types.h"
#include "sl_api.h"

/*******************************************************************************
 * Mail Ids
 ******************************************************************************/

// ================= REQUESTS  ==================
// -------------------- DRLC --------------------
#define APP_DRLC_CLIENT_GET_EDC_REQ         0x10
#define APP_DRLC_CLIENT_CANCEL_GET_EDC_REQ  0x12
#define APP_DRLC_CLIENT_GET_DATA_REQ        0x14
#define APP_DRLC_CLIENT_CANCEL_GET_DATA_REQ 0x16
#define APP_DRLC_SERVER_DATA_REQ            0x18

// -------------------- EDEV --------------------
#define APP_EDEV_CLIENT_FIND_SERVER_REQ          0x30
#define APP_EDEV_CLIENT_GET_EDEV_LIST_REQ        0x31
#define APP_EDEV_CLIENT_CANCEL_GET_EDEV_LIST_REQ 0x32
#define APP_EDEV_SERVER_CREATE_EDEV_REQ          0x34

// -------------------- Price --------------------
#define APP_PRICE_CLIENT_GET_PRICING_REQ        0x20
#define APP_PRICE_CLIENT_CANCEL_GET_PRICING_REQ 0x22

// -------------------- Response-----------------
#define APP_RESP_CLIENT_SEND_RESP_REQ 0x40

// -------------------- Time --------------------
#define APP_TIME_SERVER_SET_TIME_REQ    0x50
#define APP_TIME_SERVER_ADJUST_TIME_REQ 0x51

// ======= CONFIRMATIONS & INDICATIONS ==========
// -------------------- DRLC --------------------
#define APP_DRLC_CLIENT_GET_EDC_CNF         0x80
#define APP_DRLC_CLIENT_GET_EDC_IND         0x81
#define APP_DRLC_CLIENT_CANCEL_GET_EDC_CNF  0x82
#define APP_DRLC_CLIENT_SERVER_IND          0x83
#define APP_DRLC_CLIENT_GET_DATA_CNF        0x84
#define APP_DRLC_CLIENT_CANCEL_GET_DATA_CNF 0x86
#define APP_DRLC_CLIENT_DATA_IND            0x87
#define APP_DRLC_SERVER_DATA_CNF            0x88

// -------------------- EDEV --------------------
#define APP_EDEV_CLIENT_SERVER_FOUND_IND         0xA0
#define APP_EDEV_CLIENT_FIND_SERVER_CNF          0xA1
#define APP_EDEV_CLIENT_GET_EDEV_LIST_CNF        0xA2
#define APP_EDEV_CLIENT_CANCEL_GET_EDEV_LIST_CNF 0xA3
#define APP_EDEV_CLIENT_EDEV_LIST_IND            0xA4
#define APP_EDEV_SERVER_CREATE_EDEV_CNF          0xA5

// -------------------- Price --------------------
#define APP_PRICE_CLIENT_GET_PRICING_CNF        0x90
#define APP_PRICE_CLIENT_GET_PRICING_IND        0x91
#define APP_PRICE_CLIENT_CANCEL_GET_PRICING_CNF 0x92

// -------------------- Response-----------------
#define APP_RESP_SERVER_RESP_IND      0xB0
#define APP_RESP_CLIENT_SEND_RESP_CNF 0xB1

// -------------------- Time --------------------
#define APP_TIME_SERVER_SET_TIME_CNF    0xC0
#define APP_TIME_CLIENT_SERVER_TIME_IND 0xC1
#define APP_TIME_SERVER_ADJUST_TIME_CNF 0xC2

/*******************************************************************************
 * Common Typedefs and Definitions
 ******************************************************************************/

/**
 * Maximum number of characters in the description field.
 */
#define APP_DESCRIPTION_LEN 32

/******************************************************************************/
/**
 * \struct app_unit_val_t
 * Type for specification of a specific value, with units and power of
 * ten multiplier.
 */
typedef struct
{
    /**
     * Boolean indicating if the unit value information exists in the structure
     * or not.
     */
    bool exists;
    /**
     * Multiplier for 'unit'.
     */
    i8_t multiplier;
    /**
     * Unit associated with value.
     */
    u8_t unit;
    /**
     * Value in units specified.
     */
    i32_t value;
} app_unit_val_t;

/*******************************************************************************
 * DRLC interface
 ******************************************************************************/

/**
 * Maximum number of end device controls returned in the
 * app_drlc_client_get_edc_ind.
 */
#define APP_DRLC_CLIENT_MAX_NUM_EDCS 10

/**
 * Type of EDC data requesting and returned in the
 * app_drlc_client_get_edc_ind.
 */
typedef u8_t app_drlc_client_edc_type_t;
#define APP_DRLC_CLIENT_EDC_TYPE_TARGET_REDUCTION (0)
#define APP_DRLC_CLIENT_EDC_TYPE_DUTY_CYCLE       (1)
#define APP_DRLC_CLIENT_EDC_TYPE_SETPOINT         (2)

/******************************************************************************/
/**
 * \struct app_drlc_client_edc_target_reduction_t
 * Structure containing target reduction type data from the EDC.
 */
typedef struct
{
    /**
     * Threshold value to which a device should reduce its load. Indicates the
     * requested amount of the relevant commodity to be reduced, as defined by the
     * targetReductionType.
     */
    u16_t tarRedValue;
    /**
     * Threshold type to which a device should reduce its load. Indicates the type of
     * reduction requested.
     */
    u8_t targRedType;
} app_drlc_client_edc_target_reduction_t;

/******************************************************************************/
/**
 * \struct app_drlc_client_edc_duty_cycle_t
 * Structure containing duty cycle type data from the EDC.
 */
typedef struct
{
    /**
     * Contains the maximum On state duty cycle applied by the end device, as a
     * percentage of time.
     */
    u8_t dutyCycle;
    /**
     * Boolean indicating whether or not the dutyCycle field is present in the
     * EDC. Field not present indicates that this field has not been used by the
     * end device.
     */
    bool dutyCycleExists;
    /**
     * The value change requested for the load adjustment percentage, in tenths
     * of a percent. The value should be subtracted from the normal setting.
     */
    u8_t loadAdjustPercentOffset;
    /**
     * Boolean indicating whether or not the loadAdjustPercentOffset field is
     * present in the EDC. Field not present indicates that this field has not
     * been used by the end device.
     */
    bool loadAdjustPercentOffsetExists;
} app_drlc_client_edc_duty_cycle_t;

/******************************************************************************/
/**
 * \struct app_drlc_client_edc_setpoint_t
 * Structure containing setpoint type data from the EDC.
 */
typedef struct
{
    /**
     * Cooling Temperature Set Point / 100 = temperature in degrees Celsius where
     * -273.15C<= temperature <= 327.67C, corresponding to a Cooling Temperature
     * Set Point. The maximum resolution this format allows is 0.01C. The field
     * not present indicates that this field has not been used by the end device.
     */
    u16_t coolSetpoint;
    bool  coolSetpointExists;
    /**
     * The value change requested for the cooling offset, in degree C / 10. The
     * value should be added to the normal set point for cooling.
     */
    u8_t coolOffset;
    bool coolOffsetExists;
    /**
     * Heating Temperature Set Point / 100 = temperature in degrees Celsius where
     * -273.15C <= temperature <= 327.67C, corresponding to a Heating Temperature
     * Set Point. The maximum resolution this format allows is 0.01C. The field
     * not present indicates that this field has not been used by the end device.
     */
    u16_t heatSetpoint;
    bool  heatSetpointExists;
    /**
     * The value change requested for the heating offset, in degree C / 10.
     * The value should be subtracted for heating.
     */
    u8_t heatOffset;
    bool heatOffsetExists;
} app_drlc_client_edc_setpoint_t;

/******************************************************************************/
/**
 * \union app_drlc_client_edc_type_data_t
 * Union of EDC data types, set based upon the type of data requested.
 */
typedef union
{
    app_drlc_client_edc_target_reduction_t tarRed;
    app_drlc_client_edc_duty_cycle_t       dutyCycle;
    app_drlc_client_edc_setpoint_t         setpoint;
} app_drlc_client_edc_type_data_t;

/******************************************************************************/
/**
 * \struct app_drlc_client_edc_data_t
 * Type for a EDC.
 */
typedef struct
{
    /**
     * Array providing a description of the EDC, with max length of
     * SL_XSD_DESCRIPTION_LEN. Character string must be null terminated.
     */
    char description[APP_DESCRIPTION_LEN];
    /**
     * Indicates the type of provider sending the DRP.
     */
    u8_t primacy;
    /**
     * A flag to indicate if the EDC is considered a mandatory event as defined by
     * the service provider issuing the EDC. The flag alerts the client/user that
     * they will be subject to penalty or ineligibility based on the service
     * provider's program rules for that EndDeviceCategory.
     */
    bool mandatory;
    /**
     * Indicates that the event intends to increase consumption.
     * TRUE: Indicates the intention to increase usage value
     * FALSE: indicates the intention to decrease usage
     */
    bool loadShiftForward;
    /**
     * The overrideDuration attribute provides a duration, in seconds, for which
     * a client device is allowed to override an EDC as contractually agreed upon
     * with a service provider.
     */
    u16_t overrideDuration;
    /**
     * The time at which the EDC starts. Note that any start time randomization is
     * calculated into this value.
     */
    i64_t randomizedStart;
    /**
     * The length of time the EDC is active following the start time. Note that any
     * start time or end time randomization is calculated into this value.
     */
    u32_t randomizedDuration;
    /**
     * Type of EDC data that is included in edcData. This is defined in the request,
     * and indicates how the edc_type_data_t union should be resolved.
     */
    app_drlc_client_edc_type_t edcType;
    /**
     * Union containing the requested edcData.
     */
    app_drlc_client_edc_type_data_t edcData;
} app_drlc_client_edc_data_t;

/******************************************************************************/
/**
 * \struct app_drlc_client_get_edc_req
 * Request for end device control data that meets specific requirements as
 * defined in the request.
 */
typedef struct
{
    /**
     * Indicates the type of provider sending the DRP. This is the primacy
     * of interest to the requester.
     */
    u8_t primacy;
    /**
     * Type of EDC data that is requested.
     */
    app_drlc_client_edc_type_t edcType;
    /**
     * Number of hours of data that is being requested.
     */
    u8_t numHours;
} app_drlc_client_get_edc_req;

/******************************************************************************/
/**
 * \struct app_drlc_client_get_edc_cnf
 * Confirmation for end device control data that meets specific requirements as
 * defined in the request.
 */
typedef struct
{
    /**
     * Number of hours of data that is being requested.
     */
    u8_t numHours;
    /**
     * Type of EDC data that is requested.
     */
    app_drlc_client_edc_type_t edcType;
    /**
     * Status field indicated the result of the request. Note that the status
     * values will match the sl_cmn_rc_status_t sl_api.h.
     */
    i8_t status;
} app_drlc_client_get_edc_cnf;

/******************************************************************************/
/**
 * \struct app_drlc_client_cancel_get_edc_req
 * Request to cancel the previous request for end device control data.
 */
// typedef struct {
//} app_drlc_client_cancel_get_edc_req;

/******************************************************************************/
/**
 * \struct app_drlc_client_cancel_get_edc_cnf
 * Confirmation for cancelling request for end device control data.
 */
typedef struct
{
    u8_t status;
} app_drlc_client_cancel_get_edc_cnf;

/******************************************************************************/
/**
 * \struct app_drlc_client_get_edc_ind
 * Indication containing requesting pricing intervals.
 */
typedef struct
{
    /**
     * Status field indicated the result of the request. Note that the status
     * values will match the sl_cmn_rc_status_t sl_api.h.
     */
    i8_t status;
    /**
     * Number of hours of data that is being requested.
     */
    u8_t numHours;
    /**
     * Number of EDCs returned in the data[] array of this indication.
     */
    u8_t numEntries;
    /**
     * EDC data array.
     */
    app_drlc_client_edc_data_t data[APP_DRLC_CLIENT_MAX_NUM_EDCS];
} app_drlc_client_get_edc_ind;

/******************************************************************************/
/**
 * \struct app_drlc_client_server_ind
 * This indication is sent to the Customer Client application unsolicited from
 * the DRLC Client to indicate that a new DRLC server has been found. The contents
 * of this message are TBD. The client application may use this indication as a
 * trigger to start requesting DRLC information from the DRLC client. If
 * DRLC information is requested before this indication is sent, the DRLC client
 * will respond with a status of SL_CMN_RC_NO_SERVER.
 *
 * Dependencies: None.
 */
// typedef struct {
//
//} app_drlc_client_server_ind;

/******************************************************************************/
/**
 * \struct app_drlc_client_get_data_req
 * Request for DRLC data (this will include both DRP and EDC information). The
 * amount type can be specified as a quantity, an amount of time, or ALL. This
 * refers to the event data associated with DRLC (i.e., EDCs). In all cases,
 * all DRPs will be returned, and EDCs from all DRPs meeting the requested
 * amountType and eventAmount will be returned.
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation and indication
     * messages with the initial request message.
     */
    u32_t transactionId;
    /**
     * Type of amount that is being requested. If this is set to SL_CMN_AMOUNT_QUANTITY,
     * then the eventAmount field is in units of number of events. If this is set to
     * SL_CMN_AMOUNT_TIME_IN_HOURS, then the eventAmount field indicates how many hours
     * out is the start time of interest (e.g., get events with a start time within
     * the next 24 hours). If this is set to SL_CMN_AMOUNT_ALL, the eventAmount field
     * is ignored and all available programs and events associated will be returned.
     */
    sl_cmn_amount_type_t amountType;
    /**
     * This field means either number of events or number of events with a start
     * time within eventAmount.
     */
    u8_t eventAmount;
} app_drlc_client_get_data_req;

/******************************************************************************/
/**
 * \struct app_drlc_client_get_data_cnf
 * This confirmation is sent to the Customer Application in response to an
 * SL_DRLC_CLIENT_GET_EDC_LIST_REQ. It contains a transaction ID so the client
 * app can map it to the request. It also contains a status field which indicates
 * success if the request is being handled, or an error status in case the
 * request is not being handled. No indications should be expected by the app
 * client if an error status is returned in the request.
 *
 * Dependencies: None.
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation and indication
     * messages with the initial request message.
     */
    u32_t transactionId;
    /**
     * Status indicating whether the request was successful or not. If the
     * request is successful, received data will be sent in indications.
     */
    sl_cmn_rc_t status;
} app_drlc_client_get_data_cnf;

/******************************************************************************/
/**
 * \struct sl_drlc_client_cancel_get_data_req
 * This request is used by the Customer Application to cancel an in-progress
 * request for DRLC data. Note that until the SL_DRLC_CLIENT_CANCEL_GET_DATA_CNF
 * is received, the client app may still receive indications associated with
 * the original request.
 *
 * Dependencies: The APP_DRLC_CLIENT_SERVER_IND must be received by the client
 * app before this request will be handled by the DRLC client.
 */
typedef struct
{
    /**
     * transactionId is used to match the cancel request with the original
     * request.
     */
    u32_t transactionId;
} sl_drlc_client_cancel_get_data_req;

/******************************************************************************/
/**
 * \struct sl_drlc_client_cancel_get_data_cnf
 * This confirmation indicates that the associated request has been handled
 * and the associated processing has been stopped.
 *
 * Dependencies: None.
 */
typedef struct
{
    /**
     * transactionId is used to match the cancel confirmation with the original
     * request.
     */
    u32_t transactionId;
} sl_drlc_client_cancel_get_data_cnf;

/******************************************************************************/
/**
 * typedef app_drlc_data_type_t
 * This type defines the type of data structures that can be returned in the
 * app_drlc_client_data_ind.
 */
typedef u8_t app_drlc_data_type_t;
#define APP_DRLC_DATA_TYPE_DRP (0)
#define APP_DRLC_DATA_TYPE_EDC (1)

/******************************************************************************/
/**
 * \struct app_drlc_client_data_ind
 * This indication contains DRLC data as requested in the APP_DRLC_CLIENT_DATA_REQ.
 *
 * Dependencies: None.
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation and indication
     * messages with the initial request message.
     */
    u32_t transactionId;
    /**
     * Status indicating the current state of the operation. If the DRLC client
     * is still processing data, it will return a status of SL_CMN_RC_OPERATON_IN_PROGRESS.
     * If it is done handling the request, it will return a status of SL_CMN_RC_OPERATON_DONE.
     * If there has been an error, it will return the appropriate error status and stop
     * processing. If an error status is received, the app client should not
     * expect to receive any more indications.
     */
    sl_cmn_rc_t status;
    /**
     * Start offset indicates the total number of this data type that have been sent in previous
     * indications.
     */
    u8_t startOffset;
    /**
     * Type of data included in the indication (either DRP or EDC).
     */
    app_drlc_data_type_t dataType;
    /**
     * DRLC data (either a DRP or an EDC)
     */
    union
    {
        sl_drlc_client_drp_ind drp;
        sl_drlc_client_edc_ind edc;
    } data;
} app_drlc_client_data_ind;

/******************************************************************************/
/**
 * \struct app_drlc_server_data_req
 * This request contains data that should be loaded into the SEP2 server. Note
 * that not all data will be CREATED. It may be canceled if appropriate.
 *
 * Dependencies: None.
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation and indication
     * messages with the initial request message.
     */
    u32_t transactionId;
    /**
     * Type of data included in the request (either DRP or EDC).
     */
    app_drlc_data_type_t dataType;
    /**
     * DRLC data (either a DRP or an EDC, based on the dataType)
     */
    union
    {
        sl_drlc_client_drp_ind drp;
        sl_drlc_client_edc_ind edc;
    } data;
} app_drlc_server_data_req;

/******************************************************************************/
/**
 * \struct app_drlc_server_data_cnf
 * This indicates whether the associated request was handled properly or not.
 *
 * Dependencies: None.
 */
typedef struct
{
    /**
     * transactionId is used to match the cancel confirmation with the original
     * request.
     */
    u32_t transactionId;
    /**
     * Status indicating whether the request was successful or not.
     */
    sl_cmn_rc_t status;
} app_drlc_server_data_cnf;

/*******************************************************************************
 * EDEV interface
 ******************************************************************************/

/******************************************************************************/
/**
 * \struct app_edev_client_find_server_req
 * This request is used by the Customer Application to trigger the SL task to
 * look for an EDEV server which has an EDEV instance for this device.
 *
 * Dependencies: None.
 */
// typedef struct {
//   // empty?
//} app_edev_client_find_server_req;

/******************************************************************************/
/**
 * \struct app_edev_client_find_server_cnf
 * This confirmation is used to indicate whether the SL_EDEV_CLIENT_FIND_SERVER_REQ
 * will be handled or not. Actual server information will be provided in the
 * SL_EDEV_CLIENT_SERVER_FOUND_IND.
 *
 * Dependencies: None.
 */
typedef sl_edev_client_find_server_cnf app_edev_client_find_server_cnf;

/******************************************************************************/
/**
 * \struct sl_edev_client_server_found_ind
 * This indication is used to indicate to the Customer Application that
 * an EDEV server for this device was found.
 *
 * Dependencies: None.
 */
typedef sl_edev_client_server_found_ind app_edev_client_server_found_ind;

/******************************************************************************/
/**
 * \struct app_edev_client_get_edev_list_req
 * This request is used by the Customer Application to request some number
 * of EDEVs. The APP_EDEV_CLIENT_GET_EDEV_LIST_CNF will be sent as an
 * acknowledgment of the request, and the EDEVs themselves will be sent in
 * the APP_EDEV_CLIENT_EDEV_LIST_IND message. While the application may request as
 * many EDEVs as it wants up to 254 (255 is defined as requesting all available EDEVs)
 * only up to SL_EDEV_CLIENT_MAX_NUM_EDEVs will be returned in a single indication.
 * Therefore this request may generate multiple indications.
 *
 * Dependencies: The APP_EDEV_CLIENT_SERVER_FOUND_IND must be received by the client
 * app before this request will be handled by the EDEV client. An error will be
 * returned if there is no EDEV server.
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation and indication
     * messages with the initial request message.
     */
    u32_t transactionId;
    /**
     * This is the number of EDEVs that are being requested by the application, where
     * SL_CMN_CLIENT_GET_ENTIRE_LIST may be used to indicate that the application is
     * requesting all available EDEVs. Note that there may be fewer than numEdevs
     * available so less than this number may be returned.
     */
    u8_t numEdev;
} app_edev_client_get_edev_list_req;

/******************************************************************************/
/**
 * \struct app_edev_client_get_edev_list_cnf
 * This confirmation is sent to the Customer Application in response to an
 * APP_EDEV_CLIENT_GET_EDEV_LIST_REQ. It contains a transaction ID so the client
 * app can map it to the request. It also contains a status field which indicates
 * success if the request is being handled, or an error status in case the
 * request is not being handled. No indications should be expected by the app
 * client if an error status is returned in the request.
 *
 * Dependencies: None.
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation and indication
     * messages with the initial request message.
     */
    u32_t transactionId;
    /**
     * Status indicating whether the request was successful or not. If the
     * request is successful, received DRPs will be sent in indications.
     */
    sl_cmn_rc_t status;
} app_edev_client_get_edev_list_cnf;

/******************************************************************************/
/**
 * \struct app_edev_client_cancel_get_edev_list_req
 * This request is used by the Customer Application to cancel an in-progress
 * request for EDEVs. Note that until the APP_EDEV_CLIENT_CANCEL_GET_EDEV_LIST_CNF
 * is received, the client app may still receive indications associated with
 * the original request.
 *
 * Dependencies: The APP_EDEV_CLIENT_SERVER_IND must be received by the client
 * app before this request will be handled by the EDEV client.
 */
typedef struct
{
    /**
     * transactionId is used to match the cancel request with the original
     * request.
     */
    u32_t transactionId;
} app_edev_client_cancel_get_edev_list_req;

/******************************************************************************/
/**
 * \struct app_edev_client_cancel_get_edev_list_cnf
 * This confirmation indicates that the associated request has been handled
 * and the associated EDEV processing has been stopped.
 *
 * Dependencies: None.
 */
typedef struct
{
    /**
     * transactionId is used to match the cancel confirmation with the original
     * request.
     */
    u32_t transactionId;
} app_edev_client_cancel_get_edev_list_cnf;

/******************************************************************************/
/**
 * \struct app_edev_client_edev_list_ind
 * This indication is sent in response to the APP_EDEV_CLIENT_GET_EDEV_LIST_REQ.
 * It will contain some number of EDEVs up to SL_EDEV_CLIENT_MAX_NUM_EDEVS. One
 * request can generate multiple indications. Each indication will have the same
 * transaction ID as the request, as well as a status field which indicates
 * whether the request is still being handled (OPERATION_IN_PROGRESS), or if
 * all the requested or available EDEVS have been returned (OPERATION_DONE). If
 * an error status is returned, the client app should not expect any further
 * indications.
 *
 * Dependencies: None.
 */
typedef sl_edev_client_edev_list_ind app_edev_client_edev_list_ind;

/******************************************************************************/
/**
 * \struct app_edev_server_create_enddev_req
 * This request is used by the Customer Application to create an end device
 * instance for a particular device (defined by its SFDI) on the SL server.
 * The request include fields to enable various function sets within the
 * EDEV instance (e.g., enableFlowReservation). Note that even if those
 * booleans are set to TRUE in this messages, they will only be supported
 * if those function sets are supported within the SL server.
 *
 * Dependencies: None.
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation/indication
     * messages with the initial request message.
     */
    u32_t transactionId;
    /**
     * EDEV configuration information.
     */
    sl_edev_client_edev_t edevData;
} app_edev_server_create_enddev_req;

/******************************************************************************/
/**
 * \struct app_edev_server_create_enddev_cnf
 * This confirmation is sent in response to the SL_EDEV_SERVER_CREATE_ENDDEV_REQ
 * and provides a status of the operation. It also contains a unique ID for the
 * EDEV that should be used to refer to this EDEV when referring to this EDEV.
 *
 * Dependencies: None.
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation/indication
     * messages with the initial request message.
     */
    u32_t transactionId;
    /**
     * Status of the creation operation.
     */
    sl_cmn_rc_t status;
} app_edev_server_create_enddev_cnf;

/*******************************************************************************
 * PRICE interface
 ******************************************************************************/

/**
 * Maximum number of pricing intervals returned in the
 * app_price_client_get_pricing_ind.
 */
#define APP_PRICE_CLIENT_MAX_NUM_PRICING_DATA 10
/**
 * Maximum number of consumption blocks supported within a pricing interval.
 */
#define APP_PRICE_CLIENT_MAX_NUM_CONS_BLOCK 10

/******************************************************************************/
/**
 * \struct app_price_client_cons_block_t
 * Type for specification of a consumption block for a pricing interval.
 */
typedef struct
{
    u8_t  consBlockNum;
    bool  priceIncluded;
    i32_t price;
    u64_t startValue;
} app_price_client_cons_block_t;

/******************************************************************************/
/**
 * \struct app_price_client_cons_block_t
 * Type for a pricing interval.
 */
typedef struct
{
    /**
     * Text description of the pricing TP data of interest to the requester.
     */
    char description[APP_DESCRIPTION_LEN];
    /**
     * Indicates the type of currency for this pricing data. This is the currency
     * of interest to the requester.
     */
    u16_t currency;
    /**
     * Indicates the type of provider sending the TP. This is the primacy
     * of interest to the requester.
     */
    u8_t primacy;
    /**
     * The kind of service associated with this pricing data.
     */
    u8_t serviceKind;
    /**
     * Indicates the power of ten multiplier for the price attribute.
     */
    i8_t pricePowerOfTenMult;
    /**
     * Specifies the minimum flow rate (e.g., kW for electricity) for which this RC applies,
     * for the usage point and given rate / tariff. In combination with endLimit, allows a
     * service provider to define the demand or output characteristics for the particular
     * tariff design. If a server includes the startLimit attribute, then it SHALL also include
     * endLimit attribute.
     */
    app_unit_val_t flowRateStartLimit;
    /**
     * Specifies the maximum flow rate (e.g. kW for electricity) for which this RC applies,
     * for the usage point and given rate / tariff. In combination with startLimit, allows
     * a service provider to define the demand or output characteristics for the particular
     * tariff design. If a server includes the startLimit attribute, then it SHALL also include
     * endLimit attribute.
     */
    app_unit_val_t flowRateEndLimit;
    /**
     * The time at which the pricing TTI starts. Note that any start time randomization is
     * calculated into this value.
     */
    i64_t randomizedStart;
    /**
     * The length of time the pricing TTI is active following the start time. Note that any
     * start time or end time randomization is calculated into this value.
     */
    u32_t randomizedDuration;
    /**
     * Number of consumption tariff intervals associated with this pricing TTI.
     */
    u8_t numConsBlocks;
    /**
     * Consumption tariff interfval data associated with the pricing TTI.
     */
    app_price_client_cons_block_t consBlock[APP_PRICE_CLIENT_MAX_NUM_CONS_BLOCK];
} app_price_client_pricing_data_t;

/******************************************************************************/
/**
 * \struct app_price_client_get_pricing_req
 * Request for pricing interval data that meets specific requirements as
 * defined in the request.
 */
typedef struct
{
    /**
     * Text description of the pricing TP data of interest to the requester.
     */
    char description[APP_DESCRIPTION_LEN];
    /**
     * Number of hours of data that is being requested.
     */
    u8_t numHours;
    /**
     * Currency type of interest to the requester.
     */
    u16_t currency; // 840 = US currency
    /**
     * Indicates the type of provider sending the TP. This is the primacy
     * of interest to the requester.
     */
    u8_t primacy;
    /**
     * The kind of service of interest.
     */
    u8_t serviceKind;
} app_price_client_get_pricing_req;

/******************************************************************************/
/**
 * \struct app_price_client_get_pricing_cnf
 * Confirmation for pricing interval data that meets specific requirements as
 * defined in the request.
 */
typedef struct
{
    /**
     * Text description of the pricing TP data as sent in the request.
     */
    char description[APP_DESCRIPTION_LEN];
    /**
     * Number of hours of data that is being requested.
     */
    u8_t numHours;
    /**
     * Status field indicated the result of the request. Note that the status
     * values will match the sl_cmn_rc_status_t sl_api.h.
     */
    i8_t status;
} app_price_client_get_pricing_cnf;

/******************************************************************************/
/**
 * \struct app_price_client_cancel_get_pricing_req
 * Request to cancel the previous request for pricing data.
 */
// typedef struct {
//} app_price_client_cancel_get_pricing_req;

/******************************************************************************/
/**
 * \struct app_drlc_client_cancel_get_edc_cnf
 * Confirmation for cancelling request for pricing data.
 */
typedef struct
{
    u8_t status;
} app_price_client_cancel_get_pricing_cnf;

/******************************************************************************/
/**
 * \struct app_price_client_get_pricing_ind
 * Indication containing requesting pricing intervals.
 */
typedef struct
{
    /**
     * Text description of the pricing TP data as sent in the request.
     */
    char description[APP_DESCRIPTION_LEN];
    /**
     * Status field indicated the result of the request. Note that the status
     * values will match the sl_cmn_rc_status_t sl_api.h.
     */
    i8_t status;
    /**
     * Number of hours of data that is being requested.
     */
    u8_t numHours;
    /**
     * Number of pricing entities returned in the data[] array of this indication.
     */
    u8_t numEntries;
    /**
     * Pricing data array.
     */
    app_price_client_pricing_data_t data[APP_PRICE_CLIENT_MAX_NUM_PRICING_DATA];
} app_price_client_get_pricing_ind;

/*******************************************************************************
 * RESP interface
 ******************************************************************************/

/******************************************************************************/
/**
 * \struct app_resp_data_t
 * This indication is sent to the Customer Application to indicate that a
 * response has been posted to the server.
 *
 * Dependencies: None
 */
typedef struct
{
    /**
     * HREF that responses should be sent to. Please note that currently the HREF
     * is assumed to be relative rather than fully qualified. The max length of
     * the HREF is SL_CMN_REPLY_TO_HREF_LEN is arbitrary and assumes the URI is
     * not fully qualified.
     */
    u8_t replyToHref[SL_CMN_REPLY_TO_HREF_LEN];
    /**
     * Creation time associated with the response.
     */
    i64_t creationTime;
    /**
     * boolean indicating whether the creation time is included in the indication.
     */
    bool creationTimeExists;
    /**
     * LFDI of the device creating the response.
     */
    u8_t endDeviceLFDI[SL_XSD_LFDI_LEN];
    /**
     * Status included in the response.
     */
    sl_xsd_resp_status_type_t status;
    /**
     * Boolean indicating whether the status is valid.
     */
    bool statusExists;
    /**
     * MRID of the resource that is the subject of the response.
     */
    u8_t subj[SL_XSD_RESP_SUBJ_LEN];
    /**
     * Type of response.
     */
    sl_resp_type_t type;
    /**
     * In case the type is SL_RESP_TYPE_DR, the drRespData is required.
     */
    sl_resp_type_drresp_t drRespData;
} app_resp_data_t;

/******************************************************************************/
/**
 * \struct app_resp_server_resp_ind
 * This indication is sent to the Customer Application to indicate that a
 * response has been posted to the server.
 *
 * Dependencies: None
 */
typedef app_resp_data_t app_resp_server_resp_ind;

/******************************************************************************/
/**
 * \struct sl_resp_client_send_resp_req
 * This request is used by the Customer Application to tell the SL CLient to
 * send a response to the server.
 *
 * Dependencies: None
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation/indication
     * messages with the initial request message.
     */
    u32_t           transactionId;
    app_resp_data_t data;
} app_resp_client_send_resp_req;

/******************************************************************************/
/**
 * \struct app_resp_client_send_resp_cnf
 * This confirmation is sent to the Customer Application to indicate whether
 * the response was successfully sent to the server or not.
 *
 * Dependencies: None
 */
typedef struct
{
    /**
     * transactionId is used to match the associated confirmation/indication
     * messages with the initial request message.
     */
    u32_t transactionId;
    /**
     * Status of the operation.
     */
    sl_cmn_rc_t status;
} app_resp_client_send_resp_cnf;

/*******************************************************************************
 * Time Interface
 ******************************************************************************/

/******************************************************************************/
/**
 * \struct app_time_data_t
 * Structure containing SEP2 specific time information.
 *
 * Dependencies: none.
 */
typedef struct
{
    /**
     * The current time.
     */
    i64_t utcTime;
    /**
     * Time at which daylight savings begins (apply dstOffset).
     */
    i64_t daylightSavingsStartTime;
    /**
     * Time at which daylight savings ends (dstOffset is no longer applied).
     */
    i64_t daylightSavingsEndTime;
    /**
     * Daylight savings time offset from local standard time.
     */
    i32_t daylightSavingsOffset;
    /**
     * Local time zone offset from currentTime, not including any daylight savings
     * time offsets.
     */
    i32_t timeZoneOffset;
    /**
     * Quality of the time source.
     */
    sl_xsd_quality_t quality;
} app_time_data_t;

/******************************************************************************/
/**
 * \struct app_time_client_server_time_ind
 * This indication is sent when the SL Client receives a time update from
 * the primary time server.
 *
 * Dependencies: none.
 */
typedef app_time_data_t app_time_client_server_time_ind;

/******************************************************************************/
/**
 * \struct app_time_server_set_time_req
 * This request is used to set the timebase used by the SEP server. This information
 * is also served to clients in the time link.
 *
 * Dependencies: none.
 */
typedef app_time_data_t app_time_server_set_time_req;

/******************************************************************************/
/**
 * \struct app_time_server_set_time_cnf
 * This confirmation is used to indicate whether the requested time was
 * successfully applied.
 *
 * Dependencies: none.
 */
typedef struct
{
    /**
     * Status of the operation.
     */
    i8_t status;
} app_time_server_set_time_cnf;

/******************************************************************************/
/**
 * \struct app_time_server_adjust_time_req
 * Request to adjust time.
 */
typedef struct
{
    /**
     * The amount of time to adjust.  A positive value will adjust time forward,
     * a negative value will adjust time backward.
     */
    i32_t delta;
} app_time_server_adjust_time_req;

/******************************************************************************/
/**
 * \struct app_time_server_adjust_time_cnf
 * Confirmation for adjusting time.
 */
typedef struct
{
    u8_t status;
} app_time_server_adjust_time_cnf;

#endif
