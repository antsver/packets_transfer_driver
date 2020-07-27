// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

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
#define RKTTRANSFER_TEST_OK (0)

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//==================================================================================================
//================================ PRIVATE FUNCTIONS DECLARATIONS ==================================
//==================================================================================================
#if (defined(PKTTRANSFER_OVER_UART))

static int32_t pkttransfer_test_init_uart(void);
static int32_t pkttransfer_test_send_uart(void);
static int32_t pkttransfer_test_receive_uart(void);

#elif (defined(PKTTRANSFER_OVER_UART))

static int32_t pkttransfer_test_init_can(void);
static int32_t pkttransfer_test_send_can(void);
static int32_t pkttransfer_test_receive_can(void);

#endif

static int32_t pkttransfer_test_crc(void);

//==================================================================================================
//==================================== PRIVATE STATIC DATA =========================================
//==================================================================================================

//==================================================================================================
//======================================== PUBLIC DATA =============================================
//==================================================================================================

//==================================================================================================
//=============================== PRIVATE FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int32_t pkttransfer_test_crc(void)
{

    return RKTTRANSFER_TEST_OK;
}

#if (defined(PKTTRANSFER_OVER_UART))

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int32_t pkttransfer_test_init_uart(void)
{

    return RKTTRANSFER_TEST_OK;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int32_t pkttransfer_test_send_uart(void)
{

    return RKTTRANSFER_TEST_OK;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int32_t pkttransfer_test_receive_uart(void)
{

    return RKTTRANSFER_TEST_OK;
}

#elif (defined(PKTTRANSFER_OVER_UART))

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int32_t pkttransfer_test_init_can(void)
{

    return RKTTRANSFER_TEST_OK;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int32_t pkttransfer_test_send_can(void)
{

    return RKTTRANSFER_TEST_OK;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int32_t pkttransfer_test_receive_can(void)
{

    return RKTTRANSFER_TEST_OK;
}

#endif

//==================================================================================================
//================================ PUBLIC FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

#if (defined(PKTTRANSFER_USE_TESTS))
//-----------------------------------------------------------------------------
// Run all tests on the target platform
//-----------------------------------------------------------------------------
int32_t pkttransfer_run_tests(void)
{
    int32_t res;

    res = pkttransfer_test_crc();
    if (res != RKTTRANSFER_TEST_OK) {
        return res;
    }

#if (defined(PKTTRANSFER_OVER_UART))

    res = pkttransfer_test_init_uart();
    if (res != RKTTRANSFER_TEST_OK) {
        return res;
    }

    res = pkttransfer_test_send_uart();
    if (res != RKTTRANSFER_TEST_OK) {
        return res;
    }

    res = pkttransfer_test_receive_uart();
    if (res != RKTTRANSFER_TEST_OK) {
        return res;
    }

#elif (defined(PKTTRANSFER_OVER_UART))

    res = pkttransfer_test_init_can(void);
    if (res != RKTTRANSFER_TEST_OK) {
        return res;
    }

    res = pkttransfer_test_send_can(void);
    if (res != RKTTRANSFER_TEST_OK) {
        return res;
    }

    res = pkttransfer_test_receive_can(void);
    if (res != RKTTRANSFER_TEST_OK) {
        return res;
    }

#endif

    return RKTTRANSFER_TEST_OK;
}
#endif
