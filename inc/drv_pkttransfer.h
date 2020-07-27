//**************************************************************************************************
// Driver for variable length packets transfer over serial interfaces
//**************************************************************************************************
//
// Framing and encoding:
//
// - application level, raw packet data:          |  PAYLOAD  |
// - driver level, frame with delimiters:  | 0x7E |  PAYLOAD  |  CRC16  | 0x7E |
//                                                |<-  byte-stuffing  ->|
//
// - low level UART sending:    send all bytes of frame one-by-one
// - low level UART receiving:  receive all bytes of frame one-by-one
//
// - low level CAN sending:     send all bytes of frame within series of CAN messages (CAN ID should be passed from application)
// - low level CAN receiving:   receive series of CAN messages and TODO
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
//  - TODO synchronization point
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
#define PKTTRANSFER_INSTANCE_SIZE (40)

//-----------------------------------------------------------------------------
// Base error code for driver's errors
// To be used to separate driver's error codes from another error codes in system
// Can be negative
//-----------------------------------------------------------------------------
#define PKTTRANSFER_ERR_CODE_OK   (0L)
#define PKTTRANSFER_ERR_CODE_BASE (1024L)

//-----------------------------------------------------------------------------
// Maximum possible size of packet
//-----------------------------------------------------------------------------
#define PKTTRANSFER_PACKET_SIZE_MAX (4096UL)

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
// Result of operation
// Only codes from 'pkttransfer_err_t' enum can be returned
// Integer is used instead of enum in order to determine size of return value
//-----------------------------------------------------------------------------
typedef int32_t pkttransfer_res_t;

typedef enum pkttransfer_err_e {
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
} pkttransfer_err_t;

//-----------------------------------------------------------------------------
// Driver instance (structure is hidden in .c file)
//-----------------------------------------------------------------------------
typedef struct pkttransfer_inst_s {
    uint8_t data[PKTTRANSFER_INSTANCE_SIZE];
} pkttransfer_t;

//------------------------------------------------------------------------------
// Hardware callbacks
//------------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_UART))

//------------------------------------------------------------------------------
// Send bytes to UART (or pass them into send buffer of the UART driver)
//
// 'hw_p'   - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
// 'data_p' - pointer to data to be sent
// 'size'   - size of data (guaranteed - not exceed TODO)
//------------------------------------------------------------------------------
typedef void (*pkttransfer_uart_tx_cb_t)(const void * hw_p, const uint8_t data_p, size_t size);

//------------------------------------------------------------------------------
// Get received bytes from UART (or read them from receive buffer of the UART driver)
//
// 'hw_p'       - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
// 'data_out_p' - pointer to output received data
// 'size_max'   - maximum size of data
//
// Returns - number of bytes copied into output buffer
//------------------------------------------------------------------------------
typedef size_t (*pkttransfer_uart_rx_cb_t)(const void * hw_p, uint8_t * data_out_p, size_t size_max);

//------------------------------------------------------------------------------
// Check if possible to send bytes to UART (or pass them into send buffer of the UART driver)
//
// 'hw_p'       - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
//------------------------------------------------------------------------------
typedef bool (*pkttransfer_uart_tx_is_avail_cb_t)(const void * hw_p);

//------------------------------------------------------------------------------
// Check if there are received bytes from UART
//
// 'hw_p'       - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
//------------------------------------------------------------------------------
typedef bool (*pkttransfer_uart_rx_is_ready_cb_t)(const void * hw_p);

#elif (defined(PKTTRANSFER_OVER_CAN))

//------------------------------------------------------------------------------
// Send bytes to CAN (or pass them into send buffer of the CAN driver)
//
// 'hw_p'   - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
// 'data_p' - pointer to data to be sent
// 'size'   - size of data (guaranteed - not exceed TODO)
// 'can_id' - ID to be sent in the CAN message
//------------------------------------------------------------------------------
typedef void (*pkttransfer_can_tx_cb_t)(const void * hw_p, const uint8_t* pkt_p, size_t size, uint32_t can_id);

//------------------------------------------------------------------------------
// Get received bytes from CAN (or read them from receive buffer of the CAN driver)
//
// 'hw_p'           - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
// 'data_out_p'     - pointer to output received data
// 'size_max'       - maximum size of data
// 'can_id_out_p'   - pointer to output ID received the CAN message
//
// Returns - number of bytes copied into output buffer
//------------------------------------------------------------------------------
typedef size_t (*pkttransfer_can_rx_cb_t)(const void * hw_p, uint8_t* data_out_p, size_t size_max, uint32_t* can_id_out_p);

//------------------------------------------------------------------------------
// Check if possible to send bytes to CAN (or pass them into send buffer of the CAN driver)
//
// 'hw_p'       - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
//------------------------------------------------------------------------------
typedef bool (*pkttransfer_can_tx_is_avail_cb_t)(const void * hw_p);

//------------------------------------------------------------------------------
// Check if there are received bytes from CAN
//
// 'hw_p'       - pointer to hardware driver instance, passed over 'pkttransfer_hw_itf_t' structure (can be NULL)
//------------------------------------------------------------------------------
typedef bool (*pkttransfer_can_rx_is_ready_cb_t)(const void * hw_p);

#endif

//------------------------------------------------------------------------------
// System callbacks
//------------------------------------------------------------------------------
//#if (defined(PKTTRANSFER_OVER_UART))
//
//#elif (defined(PKTTRANSFER_OVER_CAN))
//
//#endif

//------------------------------------------------------------------------------
// Application callbacks
//------------------------------------------------------------------------------
//#if (defined(PKTTRANSFER_OVER_UART))
//
//#elif (defined(PKTTRANSFER_OVER_CAN))
//
//#endif

//------------------------------------------------------------------------------
// Interface to hardware level (callbacks to hardware layer)
//------------------------------------------------------------------------------
typedef struct pkttransfer_hw_itf_s {

    void*                               hw_p;               // Pointer to hardware UART or CAN instance to be passed into callbacks (can be NULL)

#if (defined(PKTTRANSFER_OVER_UART))

    pkttransfer_uart_tx_cb_t            tx_cb;              // Start data sending
    pkttransfer_uart_rx_cb_t            rx_cb;              // Read received data
    pkttransfer_uart_tx_is_avail_cb_t   tx_is_avail_cb;     // Check if next byte can be sent
    pkttransfer_uart_rx_is_ready_cb_t   rx_is_ready_cb;     // Check if byte has been received

#elif (defined(PKTTRANSFER_OVER_CAN))

    pkttransfer_can_tx_cb_t             tx_cb;              // Start packet sending
    pkttransfer_can_rx_cb_t             rx_cb;              // Read received packet
    pkttransfer_can_tx_is_avail_cb_t    tx_is_avail_cb;     // Check if next packet can be sent
    pkttransfer_can_rx_is_ready_cb_t    rx_is_ready_cb;     // Check if packet has been received

#endif

} pkttransfer_hw_itf_t;

//------------------------------------------------------------------------------
// Interface to System level (callbacks to synchronization primitives)
//------------------------------------------------------------------------------
//typedef struct pkttransfer_sys_itf_s {
//
//} pkttransfer_sys_itf_t;

//------------------------------------------------------------------------------
// Interface to Application level (callbacks to application layer)
//------------------------------------------------------------------------------
//typedef struct pkttransfer_app_itf_s {
//
//} pkttransfer_app_itf_t;

//------------------------------------------------------------------------------
// Driver configuration
//------------------------------------------------------------------------------
typedef struct pkttransfer_config_s {
    size_t      pkt_len_max;
    size_t      buf_rx_size;
    uint8_t*    buf_rx_p;
    size_t      buf_tx_size;
    uint8_t*    buf_tx_p;
} pkttransfer_config_t;

//------------------------------------------------------------------------------
// Driver state
//------------------------------------------------------------------------------
typedef struct pkttransfer_state_s {
    bool        is_sending;
    bool        is_receiving;
    uint32_t    sent_packets_cnt;
    uint32_t    received_packets_cnt;
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
// Get driver's settings
//
// 'inst_p'         - pointer to initialized driver instance
// 'hw_itf_out_p'   - pointer to output hardware interface (can be null)
// 'config_out_p'   - pointer to output configuration structure (can be null)
//-----------------------------------------------------------------------------
void pkttransfer_get_config(const pkttransfer_t* inst_p, pkttransfer_hw_itf_t* hw_itf_out_p, pkttransfer_config_t* config_out_p);

//-----------------------------------------------------------------------------
// Get driver's state
//
// 'inst_p'         - pointer to driver initialized instance
// 'state_out_p'    - pointer to output state structure (can be null)
//-----------------------------------------------------------------------------
void pkttransfer_get_state(const pkttransfer_t* inst_p, pkttransfer_state_t* state_out_p);

//-----------------------------------------------------------------------------
// Schedule packet sending
//
// Copies packet into instance's internal buffer for further serializing, encoding and sending
//
// 'inst_p' - pointer to initialized driver instance
// 'data_p' - pointer to data buffer
// 'size'   - size of data (1 .. PKTTRANSFER_PACKET_SIZE_MAX)
// 'can_id' - ID field for CAN frame
//
// Returns - 0 if OK, error code otherwise
//-----------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_UART))
pkttransfer_res_t pkttransfer_send_packet(const pkttransfer_t* inst_p, const uint8_t* data_p, size_t size);
#elif (defined(PKTTRANSFER_OVER_CAN))
pkttransfer_res_t pkttransfer_send_packet(const pkttransfer_t* inst_p, const uint8_t* data_p, size_t size, uint32_t can_id);
#endif

//-----------------------------------------------------------------------------
// Get received packet
//
// 'inst_p'     - pointer to driver initialized instance
// 'data_out_p' - pointer to output state structure (can be null)
// 'can_id_out_p'   - pointer to output ID received the CAN messages
//
// Returns - number of bytes copied into output buffer, 0 if there is no received packets
//-----------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_UART))
size_t pkttransfer_rec_packet(const pkttransfer_t* inst_p, uint8_t * data_out_p);
#elif (defined(PKTTRANSFER_OVER_CAN))
size_t pkttransfer_rec_packet(const pkttransfer_t* inst_p, uint8_t * data_out_p, uint32_t* can_id_out_p);
#endif

//-----------------------------------------------------------------------------
// Driver task
//
// To be called periodically from bare metal main loop or from OS thread loop
// Calls application callbacks to indicate received packet
// Calls low-level callbacks to transmit/receive packets
//
// 'inst_p' - pointer to initialized driver instance
//
// Returns - 0 if OK, error code otherwise
//-----------------------------------------------------------------------------
pkttransfer_err_t pkttransfer_task(const pkttransfer_t* inst_p);

//-----------------------------------------------------------------------------
// Calculate CRC-16-CCITT (aka CRC-16-HDLC or CRC-16-X25) for entire buffer
//
// x^16 + x^12 + x^5 + 1
// poly 0x1021; init 0xFFFF; xor 0xFFFF; RefIn true; RefOut true; Test 0x906E
//
// 'data_p' - pointer to data buffer
// 'size'   - size of data in buffer
// 'out_p'  - pointer to output value (for test: 0x90, 0x6E)
//
// Returns - CRC16 value (for test: 0x6E90 little-endian)
//-----------------------------------------------------------------------------
uint16_t pkttransfer_crc16(const uint8_t* data_p, size_t size, uint8_t* out_p);

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

