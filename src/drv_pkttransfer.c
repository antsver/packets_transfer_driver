//**************************************************************************************************
// Driver for variable length packets transfer over serial interfaces
//**************************************************************************************************
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "drv_pkttransfer.h"

//==================================================================================================
//=========================================== MACROS ===============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Frame boundary (0x7E) and byte stuffing (0x7E -> 0x7D 0x5E and 0x7D -> 0x7D 0x5D)
//-----------------------------------------------------------------------------
#define PKTTRANSFER_FRAME_DELIMITER_BYTE    (0x7E)
#define PKTTRANSFER_FRAME_ESCAPE_BYTE       (0x7D)

#define PKTTRANSFER_FRAME_ENCODED_DELIMITER_BYTE    (0x5E)
#define PKTTRANSFER_FRAME_ENCODED_ESCAPE_BYTE       (0x5D)

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//==================================================================================================
//================================ PRIVATE FUNCTIONS DECLARATIONS ==================================
//==================================================================================================
static bool pkttransfer_bytes_for_sending(pkttransfer_t * pkttransfer_inst_p);
static uint8_t pkttransfer_prepare_byte(pkttransfer_t * pkttransfer_inst_p);
static void pkttransfer_process_byte(pkttransfer_t * pkttransfer_inst_p, uint8_t byte);
static void pkttransfer_process_frame(pkttransfer_t * pkttransfer_inst_p);

//==================================================================================================
//==================================== PRIVATE STATIC DATA =========================================
//==================================================================================================

//==================================================================================================
//======================================== PUBLIC DATA =============================================
//==================================================================================================

//==================================================================================================
//=============================== PRIVATE FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//------------------------------------------------------------------------------
// Check if there are bytes to be sent
//------------------------------------------------------------------------------
static bool pkttransfer_bytes_for_sending(pkttransfer_t * pkttransfer_inst_p)
{
    pkttransfer_state_t* state_p = &(pkttransfer_inst_p->state);

    assert(state_p->tx_size >= state_p->sent_size);

    return (state_p->tx_size != 0);
}

//------------------------------------------------------------------------------
// Prepare byte to sending
//
// - read bytes stored in the TX buffer of driver instance
// - adds frame delimiters and encodes escape sequences
//
// Expected frame structure:    | 0x7E |  PAYLOAD  |  CRC16  | 0x7E |
//                                     |<-  byte-stuffing  ->|
//                                        0x7E is 0x7D 0x5E
//                                        0x7D is 0x7D 0x5D
//------------------------------------------------------------------------------
static uint8_t pkttransfer_prepare_byte(pkttransfer_t * pkttransfer_inst_p)
{
    pkttransfer_state_t* state_p = &(pkttransfer_inst_p->state);
    pkttransfer_config_t* config_p = &(pkttransfer_inst_p->config);

    assert((state_p->tx_size != 0) && (state_p->tx_size <= config_p->payload_size_max + PKTTRANSFER_FRAME_CRC_SIZE));
    assert(state_p->tx_size >= state_p->sent_size);

    // If the last byte is already prepared
    if (state_p->sent_size == state_p->tx_size) {
        state_p->sent_size = 0;
        state_p->tx_size = 0;
        state_p->tx_state = PKTTRANSFER_STATE_DELIMITER;
        state_p->sent_packets_cnt++;
        return PKTTRANSFER_FRAME_DELIMITER_BYTE;
    }

    // Prepare next byte
    uint8_t next_payload_byte = config_p->buf_tx_p[state_p->sent_size];

    switch (state_p->tx_state) {

        case PKTTRANSFER_STATE_DELIMITER:
            state_p->tx_state = PKTTRANSFER_STATE_BYTE;
            return PKTTRANSFER_FRAME_DELIMITER_BYTE;

        case PKTTRANSFER_STATE_BYTE:
            if ((next_payload_byte == PKTTRANSFER_FRAME_DELIMITER_BYTE) || (next_payload_byte == PKTTRANSFER_FRAME_ESCAPE_BYTE)) {
                state_p->tx_state = PKTTRANSFER_STATE_ENCODED_BYTE;
                return PKTTRANSFER_FRAME_ESCAPE_BYTE;
            }
            else {
                state_p->sent_size++;
                return next_payload_byte;
            }

        case PKTTRANSFER_STATE_ENCODED_BYTE:
            state_p->sent_size++;
            state_p->tx_state = PKTTRANSFER_STATE_BYTE;
            return (next_payload_byte == PKTTRANSFER_FRAME_DELIMITER_BYTE) ? PKTTRANSFER_FRAME_ENCODED_DELIMITER_BYTE : PKTTRANSFER_FRAME_ENCODED_ESCAPE_BYTE;

        default:
            assert(false);
    }

    assert(false);
    return 0;
}

//------------------------------------------------------------------------------
// Process received byte
//
// - checks frame delimiters and decodes escape sequences
// - stores received bytes in the RX buffer of driver instance
// - calls frame processing
//
// Expected frame structure:    | 0x7E |  PAYLOAD  |  CRC16  | 0x7E |
//                                     |<-  byte-stuffing  ->|
//                                        0x7E is 0x7D 0x5E
//                                        0x7D is 0x7D 0x5D
//------------------------------------------------------------------------------
static void pkttransfer_process_byte(pkttransfer_t * pkttransfer_inst_p, uint8_t byte)
{
    pkttransfer_state_t* state_p = &(pkttransfer_inst_p->state);
    pkttransfer_config_t* config_p = &(pkttransfer_inst_p->config);

    assert(state_p->rx_size <= config_p->payload_size_max + PKTTRANSFER_FRAME_CRC_SIZE);

    switch (state_p->rx_state) {

        case PKTTRANSFER_STATE_DELIMITER:
            if (byte == PKTTRANSFER_FRAME_DELIMITER_BYTE) {
                // start waiting for the first byte
                state_p->sof_detections_cnt++;
                state_p->rx_state = PKTTRANSFER_STATE_BYTE;
            }
            break;

        case PKTTRANSFER_STATE_BYTE:
            if (byte == PKTTRANSFER_FRAME_ESCAPE_BYTE) {
                // escape symbol is detected - wait encoded byte
                state_p->rx_state = PKTTRANSFER_STATE_ENCODED_BYTE;
                break;
            }
            else if (byte == PKTTRANSFER_FRAME_DELIMITER_BYTE) {
                // end of frame is detected - process frame
                pkttransfer_process_frame(pkttransfer_inst_p);
                state_p->rx_size = 0;
                state_p->rx_state = PKTTRANSFER_STATE_DELIMITER;
            }
            else if (state_p->rx_size >= config_p->payload_size_max) {
                // rx buffer overflow is detected - drop frame
                state_p->rx_size = 0;
                state_p->rx_state = PKTTRANSFER_STATE_DELIMITER;
            }
            else {
                // normal byte received - save to buffer
                config_p->buf_rx_p[state_p->rx_size++] = byte;
            }
            break;

        case PKTTRANSFER_STATE_ENCODED_BYTE:
            if (((byte != PKTTRANSFER_FRAME_ENCODED_DELIMITER_BYTE) && (byte != PKTTRANSFER_FRAME_ENCODED_ESCAPE_BYTE)) ||
                (state_p->rx_size >= config_p->payload_size_max)) {
                // wrong escape sequence is detected OR rx buffer overflow is detected - drop frame
                state_p->rx_size = 0;
                state_p->rx_state = PKTTRANSFER_STATE_DELIMITER;
            }
            else {
                // encoded byte is received - save to buffer
                byte = (byte == PKTTRANSFER_FRAME_ENCODED_DELIMITER_BYTE) ? PKTTRANSFER_FRAME_DELIMITER_BYTE : PKTTRANSFER_FRAME_ESCAPE_BYTE;
                config_p->buf_rx_p[state_p->rx_size++] = byte;
                state_p->rx_state = PKTTRANSFER_STATE_BYTE;
            }
            break;

        default:
            assert(false);
    }
}

//------------------------------------------------------------------------------
// Process received frame
//  - frame is stored in the RX buffer of driver instance
//  - checks CRC field
//  - checks length of payload
//  - calls callback to pass received frame to the application
//------------------------------------------------------------------------------
static void pkttransfer_process_frame(pkttransfer_t * pkttransfer_inst_p)
{
    pkttransfer_config_t* config_p = &(pkttransfer_inst_p->config);
    pkttransfer_state_t* state_p = &(pkttransfer_inst_p->state);

    assert(state_p->rx_size <= config_p->payload_size_max + PKTTRANSFER_FRAME_CRC_SIZE);

    // Check size
    if (state_p->rx_size <= PKTTRANSFER_FRAME_CRC_SIZE) {
        return;
    }

    // Check CRC
    uint16_t actual_crc = (config_p->buf_rx_p[state_p->rx_size-1] << 8) | (config_p->buf_rx_p[state_p->rx_size-2]);
    uint16_t expected_crc = pkttransfer_crc16(config_p->buf_rx_p, state_p->rx_size - PKTTRANSFER_FRAME_CRC_SIZE);
    if (actual_crc != expected_crc) {
        return;
    }

    // Pass received frame to application
    state_p->received_packets_cnt++;
    pkttransfer_inst_p->app_itf.app_pkt_cb(pkttransfer_inst_p->app_itf.app_p, config_p->buf_rx_p, state_p->rx_size - PKTTRANSFER_FRAME_CRC_SIZE);
}


//==================================================================================================
//================================ PUBLIC FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Initialize driver instance
//-----------------------------------------------------------------------------
void pkttransfer_init(pkttransfer_t* inst_p, const pkttransfer_hw_itf_t* hw_itf_p, const pkttransfer_app_itf_t* app_itf_p, const pkttransfer_config_t* config_p)
{
    assert((inst_p != NULL) && (hw_itf_p != NULL) && (config_p != NULL));
    assert((config_p->payload_size_max != 0) &&
           (config_p->buf_tx_p != NULL) && (config_p->buf_rx_p != NULL) &&
           (hw_itf_p->rx_cb != NULL)    && (hw_itf_p->rx_is_ready_cb != NULL) &&
           (hw_itf_p->tx_cb != NULL)    && (hw_itf_p->tx_is_avail_cb != NULL));

    memset(inst_p, 0x00, sizeof(pkttransfer_t));
    memcpy(&(inst_p->hw_itf), hw_itf_p, sizeof(pkttransfer_hw_itf_t));
    memcpy(&(inst_p->app_itf), app_itf_p, sizeof(pkttransfer_app_itf_t));
    memcpy(&(inst_p->config), config_p, sizeof(pkttransfer_config_t));
}

//-----------------------------------------------------------------------------
// Deinitialize driver instance
//-----------------------------------------------------------------------------
void pkttransfer_deinit(pkttransfer_t* inst_p)
{
    assert(inst_p != NULL);

    memset(inst_p, 0x00, sizeof(pkttransfer_t));
}

//------------------------------------------------------------------------------
// Check if driver instance is initialized
//------------------------------------------------------------------------------
bool pkttransfer_is_init(const pkttransfer_t * inst_p)
{
    return ((inst_p != NULL) && (inst_p->config.payload_size_max != 0));
}

//-----------------------------------------------------------------------------
// Send packet
//-----------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_UART))
pkttransfer_err_t pkttransfer_send(pkttransfer_t* inst_p, const uint8_t* payload_p, size_t size)
#elif (defined(PKTTRANSFER_OVER_CAN))
pkttransfer_err_t pkttransfer_send(pkttransfer_t* inst_p, const uint8_t* payload_p, size_t size, uint32_t can_id_tx)
#endif
{
    assert(pkttransfer_is_init(inst_p));

    pkttransfer_config_t* config_p = &(inst_p->config);
    pkttransfer_state_t* state_p = &(inst_p->state);

    // If payload exceeds maximum packet lenght
    if (size > config_p->payload_size_max) {
        return PKTTRANSFER_ERR_TX_OVF;
    }

    // If previous packet isn't sent
    if (state_p->tx_size != 0) {
        return PKTTRANSFER_ERR_TX_OVF;
    }

    // Store payload in the buffer
    memcpy(config_p->buf_tx_p, payload_p, size);
    state_p->tx_size = size + PKTTRANSFER_FRAME_CRC_SIZE;
    state_p->sent_size = 0;
#if (defined(PKTTRANSFER_OVER_CAN))
    state_p->can_id_tx = can_id_tx;
#endif

    // Add CRC
    uint16_t crc = pkttransfer_crc16(config_p->buf_tx_p, size);
    config_p->buf_tx_p[size] = (crc & 0xFF);
    config_p->buf_tx_p[size + 1] = (crc >> 8);

    return PKTTRANSFER_ERR_OK;
}

//-----------------------------------------------------------------------------
// Set CAN ID to filter incoming CAN messages
//-----------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_CAN))
void pkttransfer_set_can_id_rx(pkttransfer_t* inst_p, uint32_t can_id_rx)
{
    assert(pkttransfer_is_init(inst_p));

    inst_p->state.can_id_rx = can_id_rx;
}
#endif

//-----------------------------------------------------------------------------
// Driver task
//-----------------------------------------------------------------------------
void pkttransfer_task(pkttransfer_t* inst_p)
{
    assert(pkttransfer_is_init(inst_p));

    pkttransfer_hw_itf_t * hw_itf_p = &(inst_p->hw_itf);

    // If there are bytes to be sent into the low level driver
    if (pkttransfer_bytes_for_sending(inst_p) == true) {

        // If low level driver is ready to send
        if (hw_itf_p->tx_is_avail_cb(hw_itf_p->hw_p) == true) {

            #if (defined(PKTTRANSFER_OVER_UART))

                // Prepare byte
                uint8_t transmit_byte = pkttransfer_prepare_byte(inst_p);

                // Send byte into low level driver
                inst_p->hw_itf.tx_cb(hw_itf_p->hw_p, transmit_byte);

            #elif (defined(PKTTRANSFER_OVER_CAN))

                // Prepare bytes
                uint8_t transmit_buf[PKTTRANSFER_CAN_MGS_SIZE];
                size_t transmit_buf_size = 0;

                for (size_t i = 0; i < sizeof(transmit_buf); i++) {
                    transmit_buf[i] = pkttransfer_prepare_byte(inst_p);
                    transmit_buf_size++;
                    if(pkttransfer_bytes_for_sending(inst_p) == false) {
                        break;
                    }
                }

                // Send bytes into low level driver
                inst_p->hw_itf.tx_cb(hw_itf_p->hw_p, transmit_buf, transmit_buf_size, inst_p->state.can_id_tx);
            #endif
        }
    }


    // If there are received bytes in the low level driver
    if (hw_itf_p->rx_is_ready_cb(hw_itf_p->hw_p) == true) {

        #if (defined(PKTTRANSFER_OVER_UART))

            // Receive byte from low level driver
            uint8_t received_byte = inst_p->hw_itf.rx_cb(hw_itf_p->hw_p);

            // Process received byte, process received frame, pass payload to application
            pkttransfer_process_byte(inst_p, received_byte);

        #elif (defined(PKTTRANSFER_OVER_CAN))

            // Receive bytes from low level driver
            uint8_t received_buf[PKTTRANSFER_CAN_MGS_SIZE];
            size_t received_buf_size = hw_itf_p->rx_cb(hw_itf_p->hw_p, received_buf, inst_p->state.can_id_rx);

            // Process received bytes, process received frame, pass payload to application
            for (size_t i = 0; i < received_buf_size; i++) {
                pkttransfer_process_byte(inst_p, received_buf[i]);
            }

        #endif
    }
}

//-----------------------------------------------------------------------------
// Calculate CRC-16-CCITT (aka CRC-16-HDLC or CRC-16-X25) for entire buffer
//-----------------------------------------------------------------------------
uint16_t pkttransfer_crc16(const uint8_t* data_p, size_t size)
{
    uint16_t poly_reversed = 0x8408; // reversed poly (LSB-first) for 0x1021
    uint16_t crc = 0xFFFF;
    uint16_t xorout = 0xFFFF;
    size_t byte_cnt = 0;
    uint8_t out[2];

    while(byte_cnt < size) {
        // RefIn = true (LSB-first)
        crc ^= ((uint16_t)(data_p[byte_cnt])) & 0x00FF;
        for (size_t i = 0; i < 8; ++i) {
            crc = (crc & 0x0001) ? ((crc >> 1) ^ poly_reversed) : (crc >> 1);
        }
        byte_cnt++;
    }

    // RefOut = true
    out[0] = ((crc & 0xFF00) >> 8);
    out[1] = (crc & 0x00FF);

    // Xor before output
    out[0] ^= (xorout & 0x00FF);
    out[1] ^= ((xorout & 0xFF00) >> 8);

    uint8_t tmp = out[0];
    out[0] = out[1];
    out[1] = tmp;

    // Return as uint16
    crc = ((out[1] & 0x00FF) << 8) | (out[0]);
    return crc;
}
