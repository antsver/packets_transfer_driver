// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

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

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//------------------------------------------------------------------------------
// Driver instance
//------------------------------------------------------------------------------
typedef struct pkttransfer_instance_s {
    pkttransfer_hw_itf_t hw_itf;
    pkttransfer_config_t config;
} pkttransfer_instance_t;

//------------------------------------------------------------------------------
// Sanitizing
//------------------------------------------------------------------------------
static_assert(sizeof(pkttransfer_instance_t) == sizeof(pkttransfer_t), "Wrong structure size");

//==================================================================================================
//================================ PRIVATE FUNCTIONS DECLARATIONS ==================================
//==================================================================================================

//==================================================================================================
//==================================== PRIVATE STATIC DATA =========================================
//==================================================================================================

//==================================================================================================
//======================================== PUBLIC DATA =============================================
//==================================================================================================

//==================================================================================================
//=============================== PRIVATE FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//==================================================================================================
//================================ PUBLIC FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Initialize driver instance
//-----------------------------------------------------------------------------
void pkttransfer_init(pkttransfer_t* inst_p, const pkttransfer_hw_itf_t* hw_itf_p, const pkttransfer_config_t* config_p)
{
    assert((inst_p != NULL) && (hw_itf_p != NULL) && (config_p != NULL));
    assert((config_p->pkt_len_max != 0) &&
           (config_p->buf_tx_size != 0) && (config_p->buf_tx_p != NULL) &&
           (config_p->buf_rx_size != 0) && (config_p->buf_rx_p != NULL) &&
           (hw_itf_p->rx_cb != NULL)    && (hw_itf_p->rx_is_ready_cb != NULL) &&
           (hw_itf_p->tx_cb != NULL)    && (hw_itf_p->tx_is_avail_cb != NULL));

    pkttransfer_instance_t * pkttransfer_inst_p = (pkttransfer_instance_t*)inst_p;

    memcpy(&(pkttransfer_inst_p->hw_itf), hw_itf_p, sizeof(pkttransfer_hw_itf_t));
    memcpy(&(pkttransfer_inst_p->config), config_p, sizeof(pkttransfer_config_t));
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
    return ((inst_p != NULL) && (((pkttransfer_instance_t*)inst_p)->config.pkt_len_max != 0));
}

//-----------------------------------------------------------------------------
// Get driver's settings
//-----------------------------------------------------------------------------
void pkttransfer_get_config(const pkttransfer_t* inst_p, pkttransfer_hw_itf_t* hw_itf_out_p, pkttransfer_config_t* config_out_p)
{
    assert(pkttransfer_is_init(inst_p));

    pkttransfer_instance_t * pkttransfer_inst_p = (pkttransfer_instance_t*)inst_p;

    if (hw_itf_out_p != NULL) {
        memcpy(hw_itf_out_p, &(pkttransfer_inst_p->hw_itf), sizeof(pkttransfer_hw_itf_t));
    }

    if (config_out_p != NULL) {
        memcpy(config_out_p, &(pkttransfer_inst_p->config), sizeof(pkttransfer_config_t));
    }
}

//-----------------------------------------------------------------------------
// Get driver's state
//-----------------------------------------------------------------------------
void pkttransfer_get_state(const pkttransfer_t* inst_p, pkttransfer_state_t* state_out_p)
{
    assert(pkttransfer_is_init(inst_p));

    pkttransfer_instance_t * pkttransfer_inst_p = (pkttransfer_instance_t*)inst_p;

    // TODO
}

//-----------------------------------------------------------------------------
// Schedule packet sending
//-----------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_UART))
pkttransfer_res_t pkttransfer_send_packet(const pkttransfer_t* inst_p, const uint8_t* data_p, size_t size)
#elif (defined(PKTTRANSFER_OVER_CAN))
pkttransfer_res_t pkttransfer_send_packet(const pkttransfer_t* inst_p, const uint8_t* data_p, size_t size, uint32_t can_id)
#endif
{
    // TODO
    return PKTTRANSFER_ERR_OK;
}

//-----------------------------------------------------------------------------
// Get received packet
//-----------------------------------------------------------------------------
#if (defined(PKTTRANSFER_OVER_UART))
size_t pkttransfer_rec_packet(const pkttransfer_t* inst_p, uint8_t * data_out_p)
#elif (defined(PKTTRANSFER_OVER_CAN))
size_t pkttransfer_rec_packet(const pkttransfer_t* inst_p, uint8_t * data_out_p, uint32_t* can_id_out_p)
#endif
{
    // TODO
    return PKTTRANSFER_ERR_OK;
}

//-----------------------------------------------------------------------------
// Driver task
//-----------------------------------------------------------------------------
pkttransfer_err_t pkttransfer_task(const pkttransfer_t* inst_p)
{
    // TODO
    return PKTTRANSFER_ERR_OK;
}

//-----------------------------------------------------------------------------
// Calculate CRC-16-CCITT (aka CRC-16-HDLC or CRC-16-X25) for entire buffer
//-----------------------------------------------------------------------------
uint16_t pkttransfer_crc16(const uint8_t* data_p, size_t size, uint8_t* out_p)
{
//  uint16_t poly_normal     = 0x1021; // normal poly (MSB-first)
    uint16_t poly_reversed   = 0x8408; // reversed poly (LSB-first)
    uint16_t crc    = 0xFFFF; // init value
    uint16_t xorout = 0xFFFF; // Xor crc before output
    size_t byte_cnt = 0;

    // For each byte
    while(byte_cnt < size) {

//      // RefIn = false (MSB-first)
//      crc ^= (((uint16_t)(data_p[byte_cnt])) << 8);
//      for (size_t i = 0; i < 8; i++) {
//          crc = (crc & 0x8000) ? ((crc << 1) ^ poly_normal) : (crc << 1);
//      }

        // RefIn = true (LSB-first)
        crc ^= ((uint16_t)(data_p[byte_cnt])) & 0x00FF;
        for (size_t i = 0; i < 8; ++i) {
            crc = (crc & 0x0001) ? ((crc >> 1) ^ poly_reversed) : (crc >> 1);
        }

        byte_cnt++;
    }

    // RefOut = true
    out_p[0] = ((crc & 0xFF00) >> 8);
    out_p[1] = (crc & 0x00FF);

    // Xor before output
    out_p[0] ^= (xorout & 0x00FF);
    out_p[1] ^= ((xorout & 0xFF00) >> 8);

    uint8_t tmp = out_p[0];
    out_p[0] = out_p[1];
    out_p[1] = tmp;

    // Return as uint16
    crc = ((out_p[1] & 0x00FF) << 8) | (out_p[0]);
    return crc;
}
