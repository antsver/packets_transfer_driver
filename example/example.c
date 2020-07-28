//**************************************************************************************************
// Example of driver usage 
//**************************************************************************************************

#include "drv_pkttransfer.h"

//==================================================================================================
//=========================================== MACROS ===============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Payload size and buffer sizes
// TODO - place your value here
//-----------------------------------------------------------------------------
#define RKTTRANSFER_PAYLOAD_MAX (512)

//-----------------------------------------------------------------------------
// CAN ID values
// TODO - place your values here
//-----------------------------------------------------------------------------
#define RKTTRANSFER_CAN_ID_TX (1)
#define RKTTRANSFER_CAN_ID_RX (2)

//==================================================================================================
//=============================== PRIVATE FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Application callback
//-----------------------------------------------------------------------------
static void pkttransfer_app_pkt_cb(const void * app_p, const uint8_t* payload_p, size_t size);
static void pkttransfer_app_pkt_cb(const void * app_p, const uint8_t* payload_p, size_t size)
{
    (void)app_p;
    (void)payload_p;
    (void)size;

	// Process received packet here
	// TODO - place your code here
}

//-----------------------------------------------------------------------------
// Hardware callback
//-----------------------------------------------------------------------------
static bool pkttransfer_hw_tx_is_avail_cb(const void * hw_p);
static bool pkttransfer_hw_tx_is_avail_cb(const void * hw_p)
{
    (void)hw_p;

    // Check if next UART byte or CAN packet can be sent
	// TODO - place your code here

    return false;
}

//-----------------------------------------------------------------------------
// Hardware callback
//-----------------------------------------------------------------------------
static bool pkttransfer_hw_rx_is_ready_cb(const void * hw_p);
static bool pkttransfer_hw_rx_is_ready_cb(const void * hw_p)
{
    (void)hw_p;

    // Check if there are received UART bytes or CAN packets 
	// TODO - place your code here

    return false;
}

#if (defined(PKTTRANSFER_OVER_UART))

	//-----------------------------------------------------------------------------
	// Hardware callback
	//-----------------------------------------------------------------------------
	static void pkttransfer_hw_uart_tx_cb(const void * hw_p, uint8_t byte);
	static void pkttransfer_hw_uart_tx_cb(const void * hw_p, uint8_t byte)
	{
	    (void)hw_p;
	    (void)byte;

		// Send byte to UART driver 
		// TODO - place your code here
	}
	
	//-----------------------------------------------------------------------------
	// Hardware UART RX callback
	//-----------------------------------------------------------------------------
	static uint8_t pkttransfer_hw_uart_rx_cb(const void * hw_p);
	static uint8_t pkttransfer_hw_uart_rx_cb(const void * hw_p)
	{
	    (void)hw_p;

		// Read received byte from UART driver 
		// TODO - place your code here

	    return 0;
	}

#elif (defined(PKTTRANSFER_OVER_CAN))

	//-----------------------------------------------------------------------------
	// Hardware CAN TX callback
	//-----------------------------------------------------------------------------
	static void pkttransfer_hw_can_tx_cb(const void * hw_p, const uint8_t* data_p, size_t size, uint32_t can_id_tx);
	static void pkttransfer_hw_can_tx_cb(const void * hw_p, const uint8_t* data_p, size_t size, uint32_t can_id_tx)
	{
        (void)hw_p;
        (void)data_p;
        (void)size;
        (void)can_id_tx;

		// Send CAN packet to CAN driver 
		// TODO - place your code here
	}
	
	//-----------------------------------------------------------------------------
	// Hardware CAN RX callback
	//-----------------------------------------------------------------------------
	static size_t pkttransfer_hw_can_rx_cb(const void * hw_p, uint8_t* data_out_p, uint32_t can_id_rx);
	static size_t pkttransfer_hw_can_rx_cb(const void * hw_p, uint8_t* data_out_p, uint32_t can_id_rx)
	{
        (void)hw_p;
        (void)data_out_p;
        (void)can_id_rx;

		// Read received CAN packet from CAN driver 
		// TODO - place your code here

        return 0;
	}

#endif


//==================================================================================================
//======================================== PRIVATE DATA ============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Driver buffers
//-----------------------------------------------------------------------------
static uint8_t pkttransfer_tx_buf[RKTTRANSFER_PAYLOAD_MAX + PKTTRANSFER_FRAME_CRC_SIZE];
static uint8_t pkttransfer_rx_buf[RKTTRANSFER_PAYLOAD_MAX + PKTTRANSFER_FRAME_CRC_SIZE];

//-----------------------------------------------------------------------------
// Driver hardware interface
//-----------------------------------------------------------------------------
static const pkttransfer_hw_itf_t hw_itf = {
    .hw_p = NULL,
    .tx_is_avail_cb = pkttransfer_hw_tx_is_avail_cb,
    .rx_is_ready_cb = pkttransfer_hw_rx_is_ready_cb,
#if (defined(PKTTRANSFER_OVER_UART))
    .tx_cb = pkttransfer_hw_uart_tx_cb,
    .rx_cb = pkttransfer_hw_uart_rx_cb,
#elif (defined(PKTTRANSFER_OVER_CAN))
    .tx_cb = pkttransfer_hw_can_tx_cb,
    .rx_cb = pkttransfer_hw_can_rx_cb,
#endif
};

//-----------------------------------------------------------------------------
// Driver application interface
//-----------------------------------------------------------------------------
static const pkttransfer_app_itf_t app_itf = {
    .app_p = NULL,
    .app_pkt_cb = pkttransfer_app_pkt_cb,
};

//-----------------------------------------------------------------------------
// Driver configuration
//-----------------------------------------------------------------------------
static const pkttransfer_config_t config = {
    .payload_size_max = RKTTRANSFER_PAYLOAD_MAX,
    .buf_tx_p = pkttransfer_tx_buf,
    .buf_rx_p = pkttransfer_rx_buf,
};


//==================================================================================================
//================================== MAIN FUNCTION OR OS THREAD ====================================
//==================================================================================================

int main(void);
int main(void) 
{
    // Init all hardware here 
	// TODO - place your code here

	
	// Init driver instance  
	pkttransfer_t pkttransfer_instance;
	pkttransfer_init(&pkttransfer_instance, &hw_itf, &app_itf, &config);

	
	// Prepare packet
	// TODO - place your code here
    uint8_t payload[RKTTRANSFER_PAYLOAD_MAX];
    
    
    // Send packet
#if (defined(PKTTRANSFER_OVER_UART))
    pkttransfer_send(&pkttransfer_instance, payload, RKTTRANSFER_PAYLOAD_MAX);
#elif (defined(PKTTRANSFER_OVER_CAN))
    pkttransfer_send(&pkttransfer_instance, payload, RKTTRANSFER_PAYLOAD_MAX, RKTTRANSFER_CAN_ID_TX);
    pkttransfer_set_can_id_rx(&pkttransfer_instance, RKTTRANSFER_CAN_ID_RX);
#endif
   	
	
	// Main loop
	while(1) {
		
		// Call frame processing
		pkttransfer_task(&pkttransfer_instance);
		
	}
}
