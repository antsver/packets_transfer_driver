//**************************************************************************************************
// TESTS to be run on the target platform
//**************************************************************************************************

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "drv_pkttransfer.h"

//==================================================================================================
//=========================================== MACROS ===============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Payload and buffer sizes
//-----------------------------------------------------------------------------
#define RKTTRANSFER_TEST_PAYLOAD_MAX (512)
#define RKTTRANSFER_TEST_TX_BUF_SIZE (RKTTRANSFER_TEST_PAYLOAD_MAX + PKTTRANSFER_FRAME_CRC_SIZE)
#define RKTTRANSFER_TEST_RX_BUF_SIZE (RKTTRANSFER_TEST_PAYLOAD_MAX + PKTTRANSFER_FRAME_CRC_SIZE)

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Test packets table
//-----------------------------------------------------------------------------
typedef struct pkttransfer_test_packets_table_s {
    uint8_t  payload[RKTTRANSFER_TEST_PAYLOAD_MAX];
    uint8_t  frame[RKTTRANSFER_TEST_PAYLOAD_MAX];
    size_t   payload_size;
    size_t   frame_size;
} pkttransfer_test_packets_table_t;

//==================================================================================================
//================================ PRIVATE FUNCTIONS DECLARATIONS ==================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Test callbacks
//-----------------------------------------------------------------------------
static void pkttransfer_test_app_pkt_cb(const void * app_p, const uint8_t* payload_p, size_t size);

static bool pkttransfer_test_hw_tx_is_avail_cb(const void * hw_p);
static bool pkttransfer_test_hw_rx_is_ready_cb(const void * hw_p);

#if (defined(PKTTRANSFER_OVER_UART))

static void pkttransfer_test_hw_uart_tx_cb(const void * hw_p, uint8_t byte);
static uint8_t pkttransfer_test_hw_uart_rx_cb(const void * hw_p);

#elif (defined(PKTTRANSFER_OVER_CAN))

static void pkttransfer_test_hw_can_tx_cb(const void * hw_p, const uint8_t* data_p, size_t size, uint32_t can_id);
static size_t pkttransfer_test_hw_can_rx_cb(const void * hw_p, uint8_t* data_out_p, uint32_t can_id);

#endif

//-----------------------------------------------------------------------------
// Test functions
//-----------------------------------------------------------------------------
static void pkttransfer_test_crc(void);
static void pkttransfer_test_init(void);
static void pkttransfer_test_send(void);
static void pkttransfer_test_receive(void);

//==================================================================================================
//==================================== PRIVATE STATIC DATA =========================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Test data
//-----------------------------------------------------------------------------
// CRC test data
#define RKTTRANSFER_TEST_CRC_DATA_SIZE (9)
static const uint8_t pkttransfer_test_crc_data[RKTTRANSFER_TEST_CRC_DATA_SIZE] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};

// CAN ID test values
#define RKTTRANSFER_TEST_CAN_ID_TX (1)
#define RKTTRANSFER_TEST_CAN_ID_RX (2)

// Test packets table
#define RKTTRANSFER_TEST_TABLE_SIZE (4)
pkttransfer_test_packets_table_t pkttransfer_test_packets_table[RKTTRANSFER_TEST_TABLE_SIZE] = {
    {
    .payload        = {      0x00                  },
    .frame          = {0x7E, 0x00, 0x78, 0xF0, 0x7E},
    .payload_size   = 1,
    .frame_size     = 5,
    },

    {
    .payload        = {      0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39                  },
    .frame          = {0x7E, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x6E, 0x90, 0x7E},
    .payload_size   = 9,
    .frame_size     = 13,
    },
    {
    .payload        = {      0x01,       0x7D, 0x02,       0x7E                  },
    .frame          = {0x7E, 0x01, 0x7D, 0x5D, 0x02, 0x7D, 0x5E, 0x8B, 0x36, 0x7E},
    .payload_size   = 4,
    .frame_size     = 10,
    },
    {
    .payload        = {            0x7E,       0x7D,       0x7E,       0x7D                  },
    .frame          = {0x7E, 0x7D, 0x5E, 0x7D, 0x5D, 0x7D, 0x5E, 0x7D, 0x5D, 0xC8, 0xB5, 0x7E},
    .payload_size   = 4,
    .frame_size     = 12,
    },
};

//-----------------------------------------------------------------------------
// Driver buffers
//-----------------------------------------------------------------------------
uint8_t tx_buf[RKTTRANSFER_TEST_TX_BUF_SIZE];
uint8_t rx_buf[RKTTRANSFER_TEST_RX_BUF_SIZE];

//-----------------------------------------------------------------------------
// Driver instance
//-----------------------------------------------------------------------------

// Instance
static pkttransfer_t pkttransfer_test_instance;
static pkttransfer_t * const pkttransfer_test_inst_p = &pkttransfer_test_instance;

// Driver hardware interface
static pkttransfer_hw_itf_t hw_itf = {
    .hw_p = NULL,
    .tx_is_avail_cb = pkttransfer_test_hw_tx_is_avail_cb,
    .rx_is_ready_cb = pkttransfer_test_hw_rx_is_ready_cb,
#if (defined(PKTTRANSFER_OVER_UART))
    .tx_cb = pkttransfer_test_hw_uart_tx_cb,
    .rx_cb = pkttransfer_test_hw_uart_rx_cb,
#elif (defined(PKTTRANSFER_OVER_CAN))
    .tx_cb = pkttransfer_test_hw_can_tx_cb,
    .rx_cb = pkttransfer_test_hw_can_rx_cb,
#endif
};

// Driver application interface
static pkttransfer_app_itf_t app_itf = {
    .app_p = NULL,
    .app_pkt_cb = pkttransfer_test_app_pkt_cb,
};

// Driver configuration
static pkttransfer_config_t config = {
    .payload_size_max = RKTTRANSFER_TEST_PAYLOAD_MAX,
    .buf_tx_p = tx_buf,
    .buf_rx_p = rx_buf,
};

// Driver internal data
static pkttransfer_state_t pkttransfer_state_out;

//-----------------------------------------------------------------------------
// Hardware emulation
//-----------------------------------------------------------------------------
static uint8_t hardware_rx_buffer[2*RKTTRANSFER_TEST_PAYLOAD_MAX];
static size_t hardware_rx_buffer_idx = 0;
static size_t hardware_rx_buffer_size = 0;

static uint8_t hardware_tx_buffer[2*RKTTRANSFER_TEST_PAYLOAD_MAX];
static size_t hardware_tx_buffer_idx = 0;

//-----------------------------------------------------------------------------
// Application emulation
//-----------------------------------------------------------------------------
static uint8_t app_buffer[RKTTRANSFER_TEST_PAYLOAD_MAX];
static size_t app_buffer_idx = 0;

//==================================================================================================
//======================================== PUBLIC DATA =============================================
//==================================================================================================

//==================================================================================================
//=============================== PRIVATE FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Test callback
//-----------------------------------------------------------------------------
static void pkttransfer_test_app_pkt_cb(const void * app_p, const uint8_t* payload_p, size_t size)
{
    assert(app_p == NULL);
    assert(payload_p != NULL);
    assert(size != 0);

    for (size_t i = 0; i < size; i++) {
        app_buffer[app_buffer_idx++] = payload_p[i];
    }
}

//-----------------------------------------------------------------------------
// Test callback
//-----------------------------------------------------------------------------
static bool pkttransfer_test_hw_tx_is_avail_cb(const void * hw_p)
{
    assert(hw_p == NULL);
    return true;
}

//-----------------------------------------------------------------------------
// Test callback
//-----------------------------------------------------------------------------
static bool pkttransfer_test_hw_rx_is_ready_cb(const void * hw_p)
{
    assert(hw_p == NULL);

    if ((hardware_rx_buffer_size != 0) && (app_buffer_idx < hardware_rx_buffer_size)) {
        return true;
    }

    return false;
}

#if (defined(PKTTRANSFER_OVER_UART))

//-----------------------------------------------------------------------------
// Test callback
//-----------------------------------------------------------------------------
static void pkttransfer_test_hw_uart_tx_cb(const void * hw_p, uint8_t byte)
{
    assert(hw_p == NULL);
    hardware_tx_buffer[hardware_tx_buffer_idx++] = byte;
}

//-----------------------------------------------------------------------------
// Test callback
//-----------------------------------------------------------------------------
static uint8_t pkttransfer_test_hw_uart_rx_cb(const void * hw_p)
{
    assert(hw_p == NULL);
    assert(hardware_rx_buffer_size != 0);
    assert(hardware_rx_buffer_idx <= hardware_rx_buffer_size);
    uint8_t byte = hardware_rx_buffer[hardware_rx_buffer_idx++];

    if (hardware_rx_buffer_idx == hardware_rx_buffer_size) {
        hardware_rx_buffer_size = 0;
    }

    return byte;
}

#elif (defined(PKTTRANSFER_OVER_CAN))

//-----------------------------------------------------------------------------
// Test callback
//-----------------------------------------------------------------------------
static void pkttransfer_test_hw_can_tx_cb(const void * hw_p, const uint8_t* data_p, size_t size, uint32_t can_id_tx)
{
    assert(hw_p == NULL);
    assert(data_p != NULL);
    assert(size != 0);
    assert(can_id_tx == RKTTRANSFER_TEST_CAN_ID_TX);
    assert(size <= PKTTRANSFER_CAN_MGS_SIZE);

    for (size_t i = 0; i < size; i++) {
        hardware_tx_buffer[hardware_tx_buffer_idx++] = data_p[i];
    }
}

//-----------------------------------------------------------------------------
// Test callback
//-----------------------------------------------------------------------------
static size_t pkttransfer_test_hw_can_rx_cb(const void * hw_p, uint8_t* data_out_p, uint32_t can_id_rx)
{
    assert(hw_p == NULL);
    assert(data_out_p != NULL);
    assert(can_id_rx == RKTTRANSFER_TEST_CAN_ID_RX);
    assert(hardware_rx_buffer_size != 0);
    assert(hardware_rx_buffer_idx < hardware_rx_buffer_size);

    *data_out_p = hardware_rx_buffer[hardware_rx_buffer_idx++];

    if (hardware_rx_buffer_idx == hardware_rx_buffer_size) {
        hardware_rx_buffer_size = 0;
    }

    return 1;
}

#endif

//==================================================================================================
//==================================== TEST FUNCTIONS DEFINITIONS ==================================
//==================================================================================================

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void pkttransfer_test_crc(void)
{
    assert(0x906E == pkttransfer_crc16(pkttransfer_test_crc_data, RKTTRANSFER_TEST_CRC_DATA_SIZE));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void pkttransfer_test_init(void)
{
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == false);

    // Deinit already deinitialized instance
    pkttransfer_deinit(pkttransfer_test_inst_p);
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == false);

    // Init instance
    pkttransfer_init(pkttransfer_test_inst_p, &hw_itf, &app_itf, &config);
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == true);

    // Get instance state with NULL
    pkttransfer_get_state(pkttransfer_test_inst_p, NULL);

    // Get instance data
    pkttransfer_get_state(pkttransfer_test_inst_p, &pkttransfer_state_out);
    assert(pkttransfer_state_out.tx_state == PKTTRANSFER_STATE_DELIMITER);
    assert(pkttransfer_state_out.tx_size == 0);
    assert(pkttransfer_state_out.sent_size == 0);
    assert(pkttransfer_state_out.rx_state == PKTTRANSFER_STATE_DELIMITER);
    assert(pkttransfer_state_out.rx_size == 0);
    assert(pkttransfer_state_out.sof_detections_cnt == 0);
    assert(pkttransfer_state_out.received_packets_cnt == 0);
    assert(pkttransfer_state_out.sent_packets_cnt == 0);

#if (defined(PKTTRANSFER_OVER_CAN))

    // Get CAN ID
    assert(pkttransfer_state_out.can_id_rx == 0);
    assert(pkttransfer_state_out.can_id_tx == 0);

    // Set CAN ID
    pkttransfer_set_can_id_rx(pkttransfer_test_inst_p, RKTTRANSFER_TEST_CAN_ID_RX);
    pkttransfer_get_state(pkttransfer_test_inst_p, &pkttransfer_state_out);
    assert(pkttransfer_state_out.can_id_rx == RKTTRANSFER_TEST_CAN_ID_RX);
    assert(pkttransfer_state_out.can_id_tx == 0);
#endif

    // Reinit instance
    pkttransfer_init(pkttransfer_test_inst_p, &hw_itf, &app_itf, &config);
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == true);

    // Deinit instance
    pkttransfer_deinit(pkttransfer_test_inst_p);
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == false);

    // Deinit already deinitialized instance
    pkttransfer_deinit(pkttransfer_test_inst_p);
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == false);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void pkttransfer_test_send(void)
{
    // Init instance
    pkttransfer_init(pkttransfer_test_inst_p, &hw_itf, &app_itf, &config);
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == true);
    pkttransfer_err_t res;

    // Send overflow
#if (defined(PKTTRANSFER_OVER_UART))
    res = pkttransfer_send(pkttransfer_test_inst_p, pkttransfer_test_packets_table[0].payload, RKTTRANSFER_TEST_PAYLOAD_MAX+1);
#elif (defined(PKTTRANSFER_OVER_CAN))
    res = pkttransfer_send(pkttransfer_test_inst_p, pkttransfer_test_packets_table[0].payload, RKTTRANSFER_TEST_PAYLOAD_MAX+1, RKTTRANSFER_TEST_CAN_ID_TX);
#endif
    assert(res == PKTTRANSFER_ERR_TX_OVF);

    // Send all test packets
    for (size_t pkt_number = 0; pkt_number < RKTTRANSFER_TEST_TABLE_SIZE; pkt_number++) {

        // Test packet from table
        uint8_t* payload = pkttransfer_test_packets_table[pkt_number].payload;
        size_t payload_size = pkttransfer_test_packets_table[pkt_number].payload_size;
        uint8_t* frame = pkttransfer_test_packets_table[pkt_number].frame;
        size_t frame_size = pkttransfer_test_packets_table[pkt_number].frame_size;

        // Send packet to driver
    #if (defined(PKTTRANSFER_OVER_UART))
        res = pkttransfer_send(pkttransfer_test_inst_p, payload, payload_size);
    #elif (defined(PKTTRANSFER_OVER_CAN))
        res = pkttransfer_send(pkttransfer_test_inst_p, payload, payload_size, RKTTRANSFER_TEST_CAN_ID_TX);
    #endif
        assert(res == PKTTRANSFER_ERR_OK);

        // Get state
        pkttransfer_get_state(pkttransfer_test_inst_p, &pkttransfer_state_out);
        assert(pkttransfer_state_out.tx_size == payload_size + PKTTRANSFER_FRAME_CRC_SIZE);
        assert(pkttransfer_state_out.sent_size == 0);
        assert(pkttransfer_state_out.tx_state == PKTTRANSFER_STATE_DELIMITER);

        // Process packet
        hardware_tx_buffer_idx = 0;
        for (size_t i = 0; i < 2 * frame_size; i++) {
            pkttransfer_task(pkttransfer_test_inst_p);
        }
        assert(hardware_tx_buffer_idx == frame_size);
        assert(memcmp(hardware_tx_buffer, frame, frame_size) == 0);

        // Get state
        pkttransfer_get_state(pkttransfer_test_inst_p, &pkttransfer_state_out);
        assert(pkttransfer_state_out.sent_packets_cnt == pkt_number + 1);
        assert(pkttransfer_state_out.tx_size == 0);
        assert(pkttransfer_state_out.sent_size == 0);
        assert(pkttransfer_state_out.tx_state == PKTTRANSFER_STATE_DELIMITER);
    }

    // Deinit instance
    pkttransfer_deinit(pkttransfer_test_inst_p);
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == false);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void pkttransfer_test_receive(void)
{
    // Init instance
    pkttransfer_init(pkttransfer_test_inst_p, &hw_itf, &app_itf, &config);
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == true);
#if (defined(PKTTRANSFER_OVER_CAN))
    pkttransfer_set_can_id_rx(pkttransfer_test_inst_p, RKTTRANSFER_TEST_CAN_ID_RX);
#endif

    // Receive all test packets
    for (size_t pkt_number = 0; pkt_number < RKTTRANSFER_TEST_TABLE_SIZE; pkt_number++) {

        // Test packet from table
        uint8_t* payload = pkttransfer_test_packets_table[pkt_number].payload;
        size_t payload_size = pkttransfer_test_packets_table[pkt_number].payload_size;
        uint8_t* frame = pkttransfer_test_packets_table[pkt_number].frame;
        size_t frame_size = pkttransfer_test_packets_table[pkt_number].frame_size;

        // Emulate packet receiving from driver
        memcpy(hardware_rx_buffer, frame, frame_size);
        hardware_rx_buffer_idx = 0;
        hardware_rx_buffer_size = frame_size;

        // Get state
        pkttransfer_get_state(pkttransfer_test_inst_p, &pkttransfer_state_out);
        assert(pkttransfer_state_out.rx_size == 0);
        assert(pkttransfer_state_out.rx_state == PKTTRANSFER_STATE_DELIMITER);

        // Process packet
        hardware_rx_buffer_idx = 0;
        for (size_t i = 0; i < 2 * frame_size; i++) {
            pkttransfer_task(pkttransfer_test_inst_p);
        }
        assert(app_buffer_idx == payload_size);
        assert(memcmp(app_buffer, payload, payload_size) == 0);

        // Get state
        pkttransfer_get_state(pkttransfer_test_inst_p, &pkttransfer_state_out);
        assert(pkttransfer_state_out.received_packets_cnt == pkt_number + 1);
        assert(pkttransfer_state_out.tx_size == 0);
        assert(pkttransfer_state_out.rx_size == 0);
        assert(pkttransfer_state_out.rx_state == PKTTRANSFER_STATE_DELIMITER);
    }



    // Deinit instance
    pkttransfer_deinit(pkttransfer_test_inst_p);
    assert(pkttransfer_is_init(pkttransfer_test_inst_p) == false);
}

//==================================================================================================
//================================ PUBLIC FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Run all tests on the target platform
//-----------------------------------------------------------------------------
void pkttransfer_run_tests(void)
{
    pkttransfer_test_crc();
    pkttransfer_test_init();
    pkttransfer_test_send();
    pkttransfer_test_receive();
}
