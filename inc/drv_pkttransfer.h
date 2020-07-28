//**************************************************************************************************
// Driver for variable length packets transfer over serial interfaces
//**************************************************************************************************
//
// Framing and encoding:
//
//  - application level, raw packet data:          |  PAYLOAD  |
//  - driver level, frame with delimiters:  | 0x7E |  PAYLOAD  |  CRC16  | 0x7E |
//                                                 |<-  byte-stuffing  ->|
//                                                    0x7E is 0x7D 0x5E
//                                                    0x7D is 0x7D 0x5D
//
//  - low level UART sending:    send all bytes of frame one-by-one
//  - low level UART receiving:  receive all bytes of frame one-by-one
//
//  - low level CAN sending:     send all bytes of frame within series of CAN messages (CAN ID should be passed from application)
//  - low level CAN receiving:   receive series of CAN messages and (required CAN ID should be passed from application)
//
//**************************************************************************************************
//
// Driver has two interfaces:
//  - upper level API to be called from application, main loop or OS thread
//  - low level callbacks to be called from driver to access hardware UART and CAN
//
// Driver doesn't depend on certain hardware level or system level:
//  - driver use simple callback functions to access hardware UART and CAN drivers
//  - driver can be used either in bare metal system or within OS thread
//  - low level drivers can either use buffering (possibly with DMA) or not
//
// Driver is supposed to be used with either CAN or UART:
//  - hardware interface is selected using preprocessor directives
//  - all hardware (clocks, GPIO, baudrate etc.) should configured before driver usage
//  - it's possible to add support of another interfaces
//
// Driver can be used in the multithreading environment
//  - all functions are reenterable
//  - driver doesn't use neither internal static data nor memory allocation
//  - driver instance and all packet buffers are supposed to be stored externally
//
// Error handling:
//  - driver uses configurable ranges of error codes
//
//**************************************************************************************************

#ifndef DRV_PKTTRANSFER_H
#define DRV_PKTTRANSFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//==================================================================================================
//=========================================== MACROS ===============================================
//==================================================================================================

//------------------------------------------------------------------------------
// Size of hidden structure 'pkttransfer_t'
//------------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_UART))

    #define PKTTRANSFER_INSTANCE_SIZE (72)

#elif (defined(PKTTRANSFER_OVER_CAN))

    #define PKTTRANSFER_INSTANCE_SIZE (80)

#endif

//-----------------------------------------------------------------------------
// Size of payload in CAN message
//-----------------------------------------------------------------------------
#define PKTTRANSFER_CAN_MGS_SIZE (8)

//-----------------------------------------------------------------------------
// Base error code for driver's errors
// To be used to separate driver's error codes from another error codes in system
// Can be negative
//-----------------------------------------------------------------------------
#define PKTTRANSFER_ERR_CODE_OK   (0L)
#define PKTTRANSFER_ERR_CODE_BASE (1024L)

//-----------------------------------------------------------------------------
// Frame CRC
//-----------------------------------------------------------------------------
#define PKTTRANSFER_FRAME_CRC_SIZE (2)

//-----------------------------------------------------------------------------
// Sanitizing
//-----------------------------------------------------------------------------
#if ((defined(PKTTRANSFER_OVER_UART)) + (defined(PKTTRANSFER_OVER_CAN)) != 1)
    #error "UART or CAN interface must be defined"
#endif

#if (PKTTRANSFER_ERR_CODE_OK == PKTTRANSFER_ERR_CODE_BASE)
    #error "Error code and OK code must be different"
#endif

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Driver instance (structure is hidden in .c file)
//-----------------------------------------------------------------------------
typedef struct pkttransfer_s {
    uint8_t data[PKTTRANSFER_INSTANCE_SIZE];
} pkttransfer_t;

//-----------------------------------------------------------------------------
// Result of operation
// Only codes from 'pkttransfer_res_enum_t' enum can be returned
// Integer is used instead of enum in order to determine size of value
//-----------------------------------------------------------------------------
typedef int32_t pkttransfer_err_t;

typedef enum pkttransfer_err_enum_e {
    // No error
    PKTTRANSFER_ERR_OK = PKTTRANSFER_ERR_CODE_OK,

    // Block of driver's errors
    PKTTRANSFER_ERR_BASE = PKTTRANSFER_ERR_CODE_BASE,
    PKTTRANSFER_ERR_TX_OVF,     // Internal TX buffer overflow
    PKTTRANSFER_ERR_RX_OVF,     // Internal RX buffer overflow
    PKTTRANSFER_ERR_TX_ERR,     // Hardware TX error
    PKTTRANSFER_ERR_RX_ERR,     // Hardware RX error
    PKTTRANSFER_ERR_NO_CONN,    // No hardware connection
    PKTTRANSFER_ERR_CRC,        // CRC error in the received packet
    PKTTRANSFER_ERR_FRAME,      // Framing error in the received packet
} pkttransfer_err_enum_t;

//------------------------------------------------------------------------------
// Frame sending/receiving state
// Integer is used instead of enum in order to determine size of value
//------------------------------------------------------------------------------
typedef int32_t pkttransfer_frame_state_t;

typedef enum pkttransfer_frame_state_enum_e {
    PKTTRANSFER_STATE_DELIMITER = 0,
    PKTTRANSFER_STATE_BYTE,
    PKTTRANSFER_STATE_ENCODED_BYTE,
} pkttransfer_frame_state_enum_t;


#if (defined(PKTTRANSFER_OVER_UART))

//------------------------------------------------------------------------------
// Send byte to UART (or pass it into send buffer of the UART driver)
//
// 'hw_p'   - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
// 'byte'   - byte to be sent
//------------------------------------------------------------------------------
typedef void (*pkttransfer_hw_uart_tx_cb_t)(const void * hw_p, uint8_t byte);

//------------------------------------------------------------------------------
// Get received byte from UART (or read it from receive buffer of the UART driver)
//
// 'hw_p'       - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
//------------------------------------------------------------------------------
typedef uint8_t (*pkttransfer_hw_uart_rx_cb_t)(const void * hw_p);

#elif (defined(PKTTRANSFER_OVER_CAN))

//------------------------------------------------------------------------------
// Send bytes to CAN (or pass them into send buffer of the CAN driver)
//
// 'hw_p'   - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
// 'data_p' - pointer to data to be sent
// 'size'   - size of data (guaranteed - not more than PKTTRANSFER_CAN_MGS_SIZE)
// 'can_id' - ID to be sent in the CAN message
//------------------------------------------------------------------------------
typedef void (*pkttransfer_hw_can_tx_cb_t)(const void * hw_p, const uint8_t* data_p, size_t size, uint32_t can_id);

//------------------------------------------------------------------------------
// Get received bytes from CAN (or read them from receive buffer of the CAN driver)
//
// 'hw_p'           - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
// 'data_out_p'     - pointer to output received data
// 'can_id'         - required ID of received CAN message
//
// Returns - number of bytes copied into output buffer (not more than PKTTRANSFER_CAN_MGS_SIZE)
//------------------------------------------------------------------------------
typedef size_t (*pkttransfer_hw_can_rx_cb_t)(const void * hw_p, uint8_t* data_out_p, uint32_t can_id);

#endif

//------------------------------------------------------------------------------
// Check if possible to send bytes to UART/CAN (or pass them into send buffer of the UART/CAN driver)
//
// 'hw_p'       - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
//------------------------------------------------------------------------------
typedef bool (*pkttransfer_hw_tx_is_avail_cb_t)(const void * hw_p);

//------------------------------------------------------------------------------
// Check if there are received bytes from UART/CAN
//
// 'hw_p'       - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
//------------------------------------------------------------------------------
typedef bool (*pkttransfer_hw_rx_is_ready_cb_t)(const void * hw_p);

//------------------------------------------------------------------------------
// Pass received packet to application
//
// 'app_p'      - pointer to application instance, passed over 'pkttransfer_app_itf_t' structure (can be NULL)
// 'payload_p'  - pointer to received payload
// 'size'       - size of received payload (guaranteed - 1 .. 'pkttransfer_config_t.pkt_len_max')
//------------------------------------------------------------------------------
typedef bool (*pkttransfer_app_pkt_cb_t)(const void * app_p, const uint8_t* payload_p, size_t size);

//------------------------------------------------------------------------------
// Interface to hardware level (callbacks to hardware layer)
//------------------------------------------------------------------------------
typedef struct pkttransfer_hw_itf_s {

    void*                               hw_p;               // Pointer to hardware UART or CAN instance to be passed into callbacks (can be NULL)
    pkttransfer_hw_tx_is_avail_cb_t     tx_is_avail_cb;     // Check if next UART byte or CAN message can be sent
    pkttransfer_hw_rx_is_ready_cb_t     rx_is_ready_cb;     // Check if UART byte or CAN message has been received

#if (defined(PKTTRANSFER_OVER_UART))
    pkttransfer_hw_uart_tx_cb_t            tx_cb;              // Start data sending
    pkttransfer_hw_uart_rx_cb_t            rx_cb;              // Read received data
#elif (defined(PKTTRANSFER_OVER_CAN))
    pkttransfer_hw_can_tx_cb_t             tx_cb;              // Start data sending
    pkttransfer_hw_can_rx_cb_t             rx_cb;              // Read received packet
#endif

} pkttransfer_hw_itf_t;

//------------------------------------------------------------------------------
// Interface to Application level (callbacks to application layer)
//------------------------------------------------------------------------------
typedef struct pkttransfer_app_itf_s {
    void*                               app_p;               // Pointer to application instance to be passed into callbacks (can be NULL)
    pkttransfer_app_pkt_cb_t            app_pkt_cb;          // Pass received packet to application
} pkttransfer_app_itf_t;

//------------------------------------------------------------------------------
// Driver configuration
//------------------------------------------------------------------------------
typedef struct pkttransfer_config_s {
    size_t      payload_size_max;   // maximum size of payload
    uint8_t*    buf_rx_p;           // rx bufer for one payload (payload_size_max + PKTTRANSFER_FRAME_CRC_SIZE) bytes
    uint8_t*    buf_tx_p;           // tx bufer for one payload (payload_size_max + PKTTRANSFER_FRAME_CRC_SIZE) bytes
} pkttransfer_config_t;

//------------------------------------------------------------------------------
// Driver state
//------------------------------------------------------------------------------
typedef struct pkttransfer_state_s {

    // transmitting state
    pkttransfer_frame_state_t tx_state; // current state of receiving
    size_t      tx_size;                // size of data in tx buffer
    size_t      sent_size;              // size of already sent data from tx buffer

    // receiving state
    pkttransfer_frame_state_t rx_state; // current state of receiving
    size_t      rx_size;                // size of data in rx buffer

    // info
    uint32_t    sof_detections_cnt;     // counter for received of start-of-frame delimiters
    uint32_t    received_packets_cnt;   // counter for successfully received packets
    uint32_t    sent_packets_cnt;       // counter for successfully sent packets

#if (defined(PKTTRANSFER_OVER_CAN))
    uint32_t    can_id_rx;              // ID of CAN message to be received
    uint32_t    can_id_tx;              // ID of CAN message to be sent
#endif

} pkttransfer_state_t;

//==================================================================================================
//================================ PUBLIC FUNCTIONS DECLARATIONS ===================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Initialize driver instance
//
// 'inst_p'   - pointer to driver instance (can be already initialized)
// 'hw_itf_p' - pointer to hardware interface (structure will be copied into instance)
// 'config_p' - pointer to configuration structure (structure will be copied into instance)
//-----------------------------------------------------------------------------
void pkttransfer_init(pkttransfer_t* inst_p, const pkttransfer_hw_itf_t* hw_itf_p, const pkttransfer_config_t* config_p);

//-----------------------------------------------------------------------------
// Deinitialize driver instance
//
// 'inst_p' - pointer to driver instance (can be uninitialized)
//-----------------------------------------------------------------------------
void pkttransfer_deinit(pkttransfer_t* inst_p);

//------------------------------------------------------------------------------
// Check if driver instance is initialized
//
// 'inst_p'   - pointer to driver instance
//
// Returns - 'false' is pointer is NULL or driver is not initialized
//------------------------------------------------------------------------------
bool pkttransfer_is_init(const pkttransfer_t * inst_p);

//-----------------------------------------------------------------------------
// Get driver's internal data
//
// 'inst_p'         - pointer to initialized driver instance
// 'hw_itf_out_p'   - pointer to output hardware interface (can be null)
// 'app_itf_out_p'  - pointer to output application interface (can be null)
// 'config_out_p'   - pointer to output configuration structure (can be null)
// 'state_out_p'    - pointer to output state structure (can be null)
//-----------------------------------------------------------------------------
void pkttransfer_get_instance_data(const pkttransfer_t* inst_p,
                                   pkttransfer_hw_itf_t* hw_itf_out_p, pkttransfer_app_itf_t* app_itf_out_p,
                                   pkttransfer_config_t* config_out_p, pkttransfer_state_t* state_out_p);

//-----------------------------------------------------------------------------
// Send packet
//
// Copies packet into instance's internal buffer for further serializing, encoding and sending
//
// 'inst_p'     - pointer to initialized driver instance
// 'payload_p'  - pointer to payload buffer
// 'size'       - size of payload (1 .. 'pkttransfer_config_t.pkt_len_max')
// 'can_id_tx'  - ID field for CAN messages
//
// Returns - 0 if OK, error code otherwise
//-----------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_UART))
pkttransfer_err_t pkttransfer_send(const pkttransfer_t* inst_p, const uint8_t* payload_p, size_t size);
#elif (defined(PKTTRANSFER_OVER_CAN))
pkttransfer_err_t pkttransfer_send(const pkttransfer_t* inst_p, const uint8_t* payload_p, size_t size, uint32_t can_id_tx);
#endif

//-----------------------------------------------------------------------------
// Set CAN ID to filter received CAN messages
//
// 'inst_p'     - pointer to initialized driver instance
// 'can_id_tx'  - ID field for CAN messages
//-----------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_CAN))
void pkttransfer_set_can_id_rx(const pkttransfer_t* inst_p, uint32_t can_id_rx);
#endif

//-----------------------------------------------------------------------------
// Driver task
//
// To be called periodically from bare metal main loop or from OS thread loop
// Calls application callbacks to indicate received packet
// Calls low-level callbacks to transmit/receive packets
//
// 'inst_p' - pointer to initialized driver instance
//-----------------------------------------------------------------------------
void pkttransfer_task(const pkttransfer_t* inst_p);

//-----------------------------------------------------------------------------
// Calculate CRC-16-CCITT (aka CRC-16-HDLC or CRC-16-X25) for entire buffer
//
// x^16 + x^12 + x^5 + 1
// poly 0x1021; init 0xFFFF; xor 0xFFFF; RefIn true; RefOut true; Test 0x906E
//
// 'data_p' - pointer to data buffer
// 'size'   - size of data in buffer
//
// Returns - CRC16 value
//-----------------------------------------------------------------------------
uint16_t pkttransfer_crc16(const uint8_t* data_p, size_t size);

//==================================================================================================
//============================================ TESTS ===============================================
//==================================================================================================

#if (defined(PKTTRANSFER_USE_TESTS))
//-----------------------------------------------------------------------------
// Run all tests on the target platform
//
// Returns - number of failed test or 0 if all tests are successful
//-----------------------------------------------------------------------------
int32_t pkttransfer_run_tests(void);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DRV_PKTTRANSFER_H

