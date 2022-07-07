// BLE Service Template
//
// Creates a service for changing LED state over BLE

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sdk_config.h"
#include "app_error.h"
#include "nrf.h"
#include "app_util.h"
#include "nrf_twi_mngr.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_serial.h"
#include "nrfx_gpiote.h"
#include "nrfx_saadc.h"
#include "simple_ble.h"
#include "buckler.h"
#include "nordic_common.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "peer_manager.h"
#include "app_timer.h"
#include "bsp_btn_ble.h"
#include "ble.h"
#include "app_util.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_db_discovery.h"
#include "ble_hrs.h"
#include "ble_hrs_c.h"
#include "ble_conn_state.h"
#include "fds.h"
#include "nrf_crypto.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "ble_lesc.h"
#include "max44009.h"

#define LESC_DEBUG_MODE                 0                                               /**< Set to 1 to use LESC debug keys, allows you to use a sniffer to inspect traffic. */
#define LESC_MITM_NC                    1                                               /**< Use MITM (Numeric Comparison). */

#define CENTRAL_SCANNING_LED            BSP_BOARD_LED_0
#define CENTRAL_CONNECTED_LED           BSP_BOARD_LED_1
#define PERIPHERAL_ADVERTISING_LED      BSP_BOARD_LED_2
#define PERIPHERAL_CONNECTED_LED        BSP_BOARD_LED_3

/** @brief The maximum number of peripheral and central links combined. */
#define NRF_BLE_LINK_COUNT              (NRF_SDH_BLE_PERIPHERAL_LINK_COUNT + NRF_SDH_BLE_CENTRAL_LINK_COUNT)

#define APP_BLE_CONN_CFG_TAG            1                                               /**< A tag identifying the SoftDevice BLE configuration. */

#define SEC_PARAMS_BOND                 1                                               /**< Perform bonding. */
#if LESC_MITM_NC
#define SEC_PARAMS_MITM                 1                                               /**< Man In The Middle protection required. */
#define SEC_PARAMS_IO_CAPABILITIES      BLE_GAP_IO_CAPS_DISPLAY_YESNO                   /**< Display Yes/No to force Numeric Comparison. */
#else
#define SEC_PARAMS_MITM                 0                                               /**< Man In The Middle protection required. */
#define SEC_PARAMS_IO_CAPABILITIES      BLE_GAP_IO_CAPS_NONE                            /**< No I/O caps. */
#endif
#define SEC_PARAMS_LESC                 1                                               /**< LE Secure Connections pairing required. */
#define SEC_PARAMS_KEYPRESS             0                                               /**< Keypress notifications not required. */
#define SEC_PARAMS_OOB                  0                                               /**< Out Of Band data not available. */
#define SEC_PARAMS_MIN_KEY_SIZE         7                                               /**< Minimum encryption key size in octets. */
#define SEC_PARAMS_MAX_KEY_SIZE         16                                              /**< Maximum encryption key size in octets. */

#define BLE_GAP_LESC_P256_SK_LEN        32

#define MIN_CONNECTION_INTERVAL         (uint16_t) MSEC_TO_UNITS(7.5, UNIT_1_25_MS)     /**< Determines minimum connection interval in milliseconds. */
#define MAX_CONNECTION_INTERVAL         (uint16_t) MSEC_TO_UNITS(30, UNIT_1_25_MS)      /**< Determines maximum connection interval in milliseconds. */
#define SLAVE_LATENCY                   0                                               /**< Determines slave latency in terms of connection events. */
#define SUPERVISION_TIMEOUT             (uint16_t) MSEC_TO_UNITS(500, UNIT_10_MS)       /**< Determines supervision time-out in units of 10 milliseconds. */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                           /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                          /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                               /**< Number of attempts before giving up the connection parameter negotiation. */

#define ADV_INTERVAL                    300
#define APP_ADV_DURATION                18000                                           /**< The advertising duration (180 seconds) in units of 10 milliseconds. */


NRF_BLE_GATT_DEF(m_gatt);                                                   /**< GATT module instance. */
NRF_BLE_QWRS_DEF(m_qwr, NRF_SDH_BLE_TOTAL_LINK_COUNT);                      /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                         /**< Advertising module instance. */
BLE_DB_DISCOVERY_DEF(m_db_disc);                                            /**< DB discovery module instance. */

typedef struct
{
    bool           is_connected;
    ble_gap_addr_t address;
} conn_peer_t;


static conn_peer_t        m_connected_peers[NRF_BLE_LINK_COUNT];                         /**< Array of connected peers. */
static uint8_t            m_scan_buffer_data[BLE_GAP_SCAN_BUFFER_MIN];                   /**< Buffer where advertising reports will be stored by the SoftDevice. */


/**@brief Pointer to the buffer where advertising reports will be stored by the SoftDevice. */
static ble_data_t m_scan_buffer =
{
    m_scan_buffer_data,
    BLE_GAP_SCAN_BUFFER_MIN
};

/** @brief Parameters used when scanning. */
static ble_gap_scan_params_t const m_scan_params =
{
    .active            = 1,
    .interval          = SCAN_INTERVAL,
    .window            = SCAN_WINDOW,
    .timeout           = SCAN_DURATION,
    .scan_phys         = BLE_GAP_PHY_1MBPS,
    .filter_policy     = BLE_GAP_SCAN_FP_ACCEPT_ALL,
};

// Intervals for advertising and connections
static simple_ble_config_t ble_config = {
        // c0:98:e5:49:xx:xx
        .platform_id       = 0x49,    // used as 4th octect in device BLE address
        .device_id         = 0x1234, // TODO: replace with your lab bench number
        .adv_name          = "Galaxy_nRF52", // used in advertisements if there is room
        .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),
        .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};

// 32e61089-2b22-4db5-a914-43ce41986c70
static simple_ble_service_t soil_service = {{
    .uuid128 = {0x70,0x6C,0x98,0x41,0xCE,0x43,0x14,0xA9,
                0xB5,0x4D,0x22,0x2B,0x89,0x10,0xE6,0x32}
}};

static simple_ble_char_t soil_char = {.uuid16 = 0x108a};
static bool led_state = true;

//TODO: need to use these somewhere w/ encrypted ...
//GALAXY_SERVICE_UUID = "32e61089-2b22-4db5-a914-43ce41986c70"
//GALAXY_CHAR_UUID    = "32e6108a-2b22-4db5-a914-43ce41986c70"

// ADC Channel 
#define SENSOR_CHANNEL 0

/*******************************************************************************
 *   State for this application
 ******************************************************************************/
// Main application state
simple_ble_app_t* simple_ble_app;


// callback for SAADC events
void saadc_callback (nrfx_saadc_evt_t const * p_event) {
  // don't care about adc callbacks
}

// sample a particular analog channel in blocking mode
nrf_saadc_value_t sample_value (uint8_t channel) {
  nrf_saadc_value_t val;
  ret_code_t error_code = nrfx_saadc_sample_convert(channel, &val);
  APP_ERROR_CHECK(error_code);
  return val;
}

static char * roles_str[] =
{
    "INVALID_ROLE",
    "CENTRAL",
    "PERIPHERAL",
};

//function to initialize RTT library
static void rtt_init(void) {
  // Initialize error code
  ret_code_t error_code = NRF_SUCCESS;

  // initialize RTT library
  error_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(error_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();

}

//function to initialize analog to digital converter 
static void adc_init(void) {
  // Initialize error code
  ret_code_t error_code = NRF_SUCCESS;

  // initialize analog to digital converter
  nrfx_saadc_config_t saadc_config = NRFX_SAADC_DEFAULT_CONFIG;
  saadc_config.resolution = NRF_SAADC_RESOLUTION_12BIT;
  error_code = nrfx_saadc_init(&saadc_config, saadc_callback);
  APP_ERROR_CHECK(error_code);
}

/**@brief Function for initiating scanning. From ble_app_multirole_lesc
 */
static void scan_start(void)
{
    ret_code_t err_code;

    (void) sd_ble_gap_scan_stop();

    err_code = sd_ble_gap_scan_start(&m_scan_params, &m_scan_buffer);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("Scanning");
}

//function for initiating advertising and scanning. From ble_app_multirole_lesc
//TODO edit to advertise the message that we want
static void adv_scan_start(void)
{
    ret_code_t err_code;

    scan_start();

    // Turn on the LED to signal scanning.
    //bsp_board_led_on(CENTRAL_SCANNING_LED);

    printf("in adv_scan_start\n");

    // Start advertising.
    err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);

    printf("advertising\n");
    NRF_LOG_INFO("Advertising");
}

/**@brief Function for assigning new connection handle to available instance of QWR module.
 *
 * @param[in] conn_handle New connection handle.
 */
static void multi_qwr_conn_handle_assign(uint16_t conn_handle)
{
    for (uint32_t i = 0; i < NRF_BLE_LINK_COUNT; i++)
    {
        if (m_qwr[i].conn_handle == BLE_CONN_HANDLE_INVALID)
        {
            ret_code_t err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr[i], conn_handle);
            APP_ERROR_CHECK(err_code);
            break;
        }
    }
}

/**@brief Function for handling BLE Stack events common to both the central and peripheral roles.
 * @param[in] conn_handle Connection Handle.
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(uint16_t conn_handle, ble_evt_t const * p_ble_evt)
{
    char        passkey[BLE_GAP_PASSKEY_LEN + 1];
    uint16_t    role = ble_conn_state_role(conn_handle);

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            m_connected_peers[conn_handle].is_connected = true;
            m_connected_peers[conn_handle].address = p_ble_evt->evt.gap_evt.params.connected.peer_addr;
            multi_qwr_conn_handle_assign(conn_handle);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            memset(&m_connected_peers[conn_handle], 0x00, sizeof(m_connected_peers[0]));
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            NRF_LOG_INFO("%s: BLE_GAP_EVT_SEC_PARAMS_REQUEST", nrf_log_push(roles_str[role]));
            break;

        case BLE_GAP_EVT_PASSKEY_DISPLAY:
            memcpy(passkey, p_ble_evt->evt.gap_evt.params.passkey_display.passkey, BLE_GAP_PASSKEY_LEN);
            passkey[BLE_GAP_PASSKEY_LEN] = 0x00;
            NRF_LOG_INFO("%s: BLE_GAP_EVT_PASSKEY_DISPLAY: passkey=%s match_req=%d",
                         nrf_log_push(roles_str[role]),
                         nrf_log_push(passkey),
                         p_ble_evt->evt.gap_evt.params.passkey_display.match_request);

            if (p_ble_evt->evt.gap_evt.params.passkey_display.match_request)
            {
                on_match_request(conn_handle, role);
            }
            break;

        case BLE_GAP_EVT_AUTH_KEY_REQUEST:
            NRF_LOG_INFO("%s: BLE_GAP_EVT_AUTH_KEY_REQUEST", nrf_log_push(roles_str[role]));
            break;

        case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
            NRF_LOG_INFO("%s: BLE_GAP_EVT_LESC_DHKEY_REQUEST", nrf_log_push(roles_str[role]));
            break;

         case BLE_GAP_EVT_AUTH_STATUS:
             NRF_LOG_INFO("%s: BLE_GAP_EVT_AUTH_STATUS: status=0x%x bond=0x%x lv4: %d kdist_own:0x%x kdist_peer:0x%x",
                          nrf_log_push(roles_str[role]),
                          p_ble_evt->evt.gap_evt.params.auth_status.auth_status,
                          p_ble_evt->evt.gap_evt.params.auth_status.bonded,
                          p_ble_evt->evt.gap_evt.params.auth_status.sm1_levels.lv4,
                          *((uint8_t *)&p_ble_evt->evt.gap_evt.params.auth_status.kdist_own),
                          *((uint8_t *)&p_ble_evt->evt.gap_evt.params.auth_status.kdist_peer));
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            ret_code_t err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        default:
            // No implementation needed.
            break;
    }
}

//function for checking if a link already exists with a new connected peer. From ble_app_multirole_lesc
static bool is_already_connected(ble_gap_addr_t const * p_connected_adr)
{
    for (uint32_t i = 0; i < NRF_BLE_LINK_COUNT; i++)
    {
        if (m_connected_peers[i].is_connected)
        {
            if (m_connected_peers[i].address.addr_type == p_connected_adr->addr_type)
            {
                if (memcmp(m_connected_peers[i].address.addr,
                           p_connected_adr->addr,
                           sizeof(m_connected_peers[i].address.addr)) == 0)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/**@brief Connection parameters requested for connection. */
static ble_gap_conn_params_t const m_connection_param =
{
    MIN_CONNECTION_INTERVAL,
    MAX_CONNECTION_INTERVAL,
    SLAVE_LATENCY,
    SUPERVISION_TIMEOUT
};

//@brief Function for handling File Data Storage events.
static void fds_evt_handler(fds_evt_t const * const p_fds_evt)
{
    if (p_fds_evt->id == FDS_EVT_GC)
    {
        NRF_LOG_DEBUG("GC completed");
    }
}


//@brief Function for handling Peer Manager events.
static void pm_evt_handler(pm_evt_t const * p_evt)
{
    ret_code_t err_code;
    uint16_t role = ble_conn_state_role(p_evt->conn_handle);

    switch (p_evt->evt_id)
    {
        case PM_EVT_BONDED_PEER_CONNECTED:
        {
            NRF_LOG_DEBUG("%s : PM_EVT_BONDED_PEER_CONNECTED: peer_id=%d",
                           nrf_log_push(roles_str[role]),
                           p_evt->peer_id);
        } break;

        case PM_EVT_CONN_SEC_START:
        {
            NRF_LOG_DEBUG("%s : PM_EVT_CONN_SEC_START: peer_id=%d",
                           nrf_log_push(roles_str[role]),
                           p_evt->peer_id);
        } break;

        case PM_EVT_CONN_SEC_SUCCEEDED:
        {
            NRF_LOG_INFO("%s : PM_EVT_CONN_SEC_SUCCEEDED conn_handle: %d, Procedure: %d",
                           nrf_log_push(roles_str[role]),
                           p_evt->conn_handle,
                           p_evt->params.conn_sec_succeeded.procedure);
        } break;

        case PM_EVT_CONN_SEC_FAILED:
        {
            NRF_LOG_DEBUG("%s: PM_EVT_CONN_SEC_FAILED: peer_id=%d, error=%d",
                          nrf_log_push(roles_str[role]),
                          p_evt->peer_id,
                          p_evt->params.conn_sec_failed.error);

        } break;

        case PM_EVT_CONN_SEC_CONFIG_REQ:
        {
            // Reject pairing request from an already bonded peer.
            pm_conn_sec_config_t conn_sec_config = {.allow_repairing = false};
            pm_conn_sec_config_reply(p_evt->conn_handle, &conn_sec_config);
        } break;

        case PM_EVT_STORAGE_FULL:
        {
            // Run garbage collection on the flash.
            err_code = fds_gc();
            if (err_code == FDS_ERR_NO_SPACE_IN_QUEUES)
            {
                // Retry.
            }
        } break;

        case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
        {
            NRF_LOG_DEBUG("%s: PM_EVT_PEER_DATA_UPDATE_SUCCEEDED: peer_id=%d data_id=0x%x action=0x%x",
                           nrf_log_push(roles_str[role]),
                           p_evt->peer_id,
                           p_evt->params.peer_data_update_succeeded.data_id,
                           p_evt->params.peer_data_update_succeeded.action);
        } break;

        case PM_EVT_PEERS_DELETE_SUCCEEDED:
        {
            adv_scan_start();
        } break;

        case PM_EVT_PEER_DATA_UPDATE_FAILED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peer_data_update_failed.error);
        } break;

        case PM_EVT_PEER_DELETE_FAILED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peer_delete_failed.error);
        } break;

        case PM_EVT_PEERS_DELETE_FAILED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peers_delete_failed_evt.error);
        } break;

        case PM_EVT_ERROR_UNEXPECTED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.error_unexpected.error);
        } break;

        case PM_EVT_PEER_DELETE_SUCCEEDED:
        case PM_EVT_LOCAL_DB_CACHE_APPLIED:
        case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
            // This can happen when the local DB has changed.
        case PM_EVT_SERVICE_CHANGED_IND_SENT:
        case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
        default:
            break;
    }
}


// Function for handling advertising events.
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            //bsp_board_led_on(PERIPHERAL_ADVERTISING_LED);
            //bsp_board_led_off(PERIPHERAL_CONNECTED_LED);
            break;

        case BLE_ADV_EVT_IDLE:
        {
            ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
            APP_ERROR_CHECK(err_code);
        } break;

        default:
            // No implementation needed.
            break;
    }
}

//Function for initializing the Peer Manager.
static void peer_manager_init(void)
{
    ble_gap_sec_params_t sec_params;
    ret_code_t err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_params, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_params.bond           = SEC_PARAMS_BOND;
    sec_params.mitm           = SEC_PARAMS_MITM;
    sec_params.lesc           = SEC_PARAMS_LESC;
    sec_params.keypress       = SEC_PARAMS_KEYPRESS;
    sec_params.io_caps        = SEC_PARAMS_IO_CAPABILITIES;
    sec_params.oob            = SEC_PARAMS_OOB;
    sec_params.min_key_size   = SEC_PARAMS_MIN_KEY_SIZE;
    sec_params.max_key_size   = SEC_PARAMS_MAX_KEY_SIZE;
    sec_params.kdist_own.enc  = 1;
    sec_params.kdist_own.id   = 1;
    sec_params.kdist_peer.enc = 1;
    sec_params.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_params);
    //printf("%ld",err_code);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    //printf("%ld",err_code);
    APP_ERROR_CHECK(err_code);

    err_code = fds_register(fds_evt_handler);
    //printf("%ld",err_code);
    APP_ERROR_CHECK(err_code);

    // Generate the ECDH key pair and set public key in the peer-manager.
    err_code = ble_lesc_ecc_keypair_generate_and_set();
    //printf("%ld",err_code);
    APP_ERROR_CHECK(err_code);
    //printf("made it to the end of peer_manager_init\n");
}

static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_HEART_RATE_SERVICE,         BLE_UUID_TYPE_BLE},
                                   {BLE_UUID_RUNNING_SPEED_AND_CADENCE,  BLE_UUID_TYPE_BLE}};


void advertising_init(void)
{
    ret_code_t             err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance      = true;
    init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.advdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;

    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

//TODO: add_galaxy_characteristic function to replace simple function
static void galaxy_add_characteristic(uint8_t read, uint8_t write, uint8_t notify, uint8_t vlen,
                                   uint16_t len, uint8_t* buf,
                                   simple_ble_service_t* service_handle,
                                   simple_ble_char_t* char_handle) {
    ret_code_t err_code;

    // Set characteristic UUID & add it
    ble_uuid_t char_uuid;
    char_uuid.type = service_handle->uuid_handle.type;
    char_uuid.uuid = char_handle->uuid16;

    err_code = sd_ble_uuid_vs_add(&service_handle->uuid128, &char_uuid.type);
    APP_ERROR_CHECK(err_code);

    // Set characteristic metadata
    ble_gatts_char_md_t char_md;
    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.read   = read;
    char_md.char_props.write  = write;
    char_md.char_props.notify = notify;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = NULL;
    char_md.p_sccd_md         = NULL;

    // Configuring Client Characteristic Configuration Descriptor (CCCD) metadata and add to char_md structure
    ble_gatts_attr_md_t cccd_md;
    memset(&cccd_md, 0, sizeof(cccd_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc      = BLE_GATTS_VLOC_STACK;
    char_md.p_cccd_md = &cccd_md;

    // Set attribute metadata
    ble_gatts_attr_md_t attr_md;
    memset(&attr_md, 0, sizeof(attr_md));

    //Set LESC security 
    // Require LESC with MITM (Numeric Comparison)
    BLE_GAP_CONN_SEC_MODE_SET_LESC_ENC_WITH_MITM(&attr_md.write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

    // Require LESC with MITM (Numeric Comparison)
    BLE_GAP_CONN_SEC_MODE_SET_LESC_ENC_WITH_MITM(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

    //OLD sec settings
    //read) BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    //if (write) BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc    = BLE_GATTS_VLOC_USER;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = vlen;

    // Set attribute data
    ble_gatts_attr_t attr_char_value;
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    attr_char_value.p_uuid    = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = len;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = len; // max len can be up to BLE_GATTS_FIX_ATTR_LEN_MAX (510)
    attr_char_value.p_value   = buf;

    err_code = sd_ble_gatts_characteristic_add((service_handle->service_handle), &char_md, &attr_char_value, &(char_handle->char_handle));
    APP_ERROR_CHECK(err_code);

}

/*
simple_ble_app_t* galaxy_ble_init(const simple_ble_config_t* conf) {
    ble_config = conf;

    // Initialize power management
    ret_code_t err_code = NRF_SUCCESS;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);

    // APP_TIMER_INIT must be called before BLE setup that relies on it
    initialize_app_timer();

    // Setup BLE and services
    ble_stack_init();
    printf("ble stack setup\n");
    gap_params_init();
    printf("gap setup\n");
    gatt_init();
    printf("gatt setup\n");
    advertising_init();
    printf("advertising setup\n");
    services_init();
    printf("services setup\n");

    // Create device information service
#if defined(HW_REVISION) || defined(FW_REVISION)
    //simple_ble_device_info_service_automatic();
#endif

    // Enable device firmware updates
#ifdef ENABLE_DFU
    //dfu_init();
#endif

    conn_params_init();
    //peer_manager_init();

    // Initialize our connection state to "not in a connection"
    app.conn_handle = BLE_CONN_HANDLE_INVALID;

    // Return a reference to the application state so that the user of this
    // module has a pointer to the connection handle.
    return &app;
}
*/

static void crypto_init(void) {
  // Initialize error code
  ret_code_t error_code = NRF_SUCCESS;

  //Initialize crypto 
  error_code = nrf_crypto_init();
  APP_ERROR_CHECK(error_code);
}

static void analog_in_init(void) {
  // Initialize error code
  ret_code_t error_code = NRF_SUCCESS;

  // initialize analog inputs
  // configure with 0 as input pin for now
  nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(0);
  channel_config.gain = NRF_SAADC_GAIN1_6; // input gain of 1/6 Volts/Volt, multiply incoming signal by (1/6)
  channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL; // 0.6 Volt reference, input after gain can be 0 to 0.6 Volts

  // specify input pin and initialize that ADC channel
  channel_config.pin_p = BUCKLER_GROVE_A1;
  error_code = nrfx_saadc_channel_init(SENSOR_CHANNEL, &channel_config);
  APP_ERROR_CHECK(error_code);
}

static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

void conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_CONN_HANDLE_INVALID; // Start upon connection.
    cp_init.disconnect_on_fail             = true;
    cp_init.evt_handler                    = NULL;  // Ignore events.
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


static void gatt_init(void) {
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}

int main(void) {

  ret_code_t error_code = NRF_SUCCESS;
  rtt_init();
  adc_init();
  analog_in_init();
  //ble_stack_init();
  crypto_init();
  printf("crypto initialized\n");

  //Initialize BLE LE Secure Connections
  error_code = ble_lesc_init();
  APP_ERROR_CHECK(error_code);
  printf("ble lesc initialized\n");

  // Initialize power management
  ret_code_t err_code = NRF_SUCCESS;
  err_code = nrf_pwr_mgmt_init();
  APP_ERROR_CHECK(err_code);

  // APP_TIMER_INIT must be called before BLE setup that relies on it
  initialize_app_timer();

  // Setup BLE and services
  ble_stack_init();
  printf("ble stack setup\n");
  //gap_params_init(); //wrong permissions idk if we need it
  //printf("gap setup\n");
  gatt_init();
  printf("gatt setup\n");
  services_init();
  printf("services setup\n");
  conn_params_init();
  printf("connection params setup\n");

  uint8_t sensor_val = 1; // placeholder data

  // Setup BLE
  //simple_ble_app = simple_ble_init(&ble_config); //TODO: rewrite for galaxy

  simple_ble_add_service(&soil_service);
  printf("added service\n");
  galaxy_add_characteristic(1, 1, 0, 0,
      sizeof(sensor_val), (uint8_t*)&sensor_val,
      &soil_service, &soil_char);
  printf("added characteristic\n");

  peer_manager_init(); 
  printf("peer manager setup\n");
  advertising_init();
  printf("advertising setup\n");
  
  //printf("what is happening?\n");  

  //advertising_init();

  //printf("advertising initialized\n");

  adv_scan_start();
  printf("advertise started\n");


  // Start Advertising
  //simple_ble_adv_only_name();

  while(1) {
    power_manage();
  }
}

