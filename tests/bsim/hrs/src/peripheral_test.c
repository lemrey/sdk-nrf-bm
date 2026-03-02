/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * HRS peripheral role for Babblesim test kit: tests ble_hrs service.
 * Run with -testid=peripheral -d=0.
 */

#include "bs_types.h"
#include "bs_tracing.h"
#include "bstests.h"
#include "babblekit/testcase.h"
#include "hrs_bstest_common.h"

#include <stdio.h>
#include <string.h>
#include <ble_gap.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/services/common.h>
#include <bm/bluetooth/services/ble_hrs.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hrs_bstest_peripheral, LOG_LEVEL_INF);

#define HRS_TEST_PASS_THRESHOLD 5
#define CONN_TAG CONFIG_NRF_SDH_BLE_CONN_TAG
#define WAIT_TIME 10 /* seconds */

extern enum bst_result_t bst_result;

static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
static uint8_t adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static uint8_t hrs_notifications_sent;

BLE_HRS_DEF(ble_hrs);

static void hr_meas_timer_handler(struct k_timer *timer)
{
	uint32_t nrf_err;
	static uint16_t heart_rate_sim = 72;

	nrf_err = ble_hrs_heart_rate_measurement_send(&ble_hrs, heart_rate_sim);
	if (nrf_err) {
		TEST_FAIL("Failed to notify, nrf_err %#x", nrf_err);
		return;
	}

	LOG_INF("Sent HR = %u", (unsigned int)heart_rate_sim);
	hrs_notifications_sent++;

	if (hrs_notifications_sent == HRS_TEST_PASS_THRESHOLD) {
		TEST_PASS("Peripheral sent %u notifications", hrs_notifications_sent);
		k_timer_stop(timer);
	}

	heart_rate_sim = (heart_rate_sim >= 120) ? 60 : (heart_rate_sim + 2);
}
K_TIMER_DEFINE(hr_meas_timer, hr_meas_timer_handler, NULL);

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	const char *running = hrs_bstest_get_running_id();

	if (!running || strcmp(running, HRS_BSTEST_ID_PERIPHERAL) != 0) {
		/* skip messages not for this peer */
		return;
	}

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Disconnected, reason %#x", evt->evt.gap_evt.params.disconnected.reason);
		conn_handle = BLE_CONN_HANDLE_INVALID;
	default:
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		ble_hrs_conn_params_evt(&ble_hrs, evt);
		break;
	default:
		break;
	}
}

static void ble_hrs_evt_handler(struct ble_hrs *hrs, const struct ble_hrs_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_HRS_EVT_NOTIFICATION_ENABLED:
		k_timer_start(&hr_meas_timer, K_MSEC(10), K_MSEC(10));
		break;
	default:
		break;
	}
}

static uint32_t adv_start(void)
{
	uint32_t nrf_err;
	uint8_t adv_data_buf[] = {
		0x02, 0x01, 0x06,
		0x03, 0x03, 0x0D, 0x18,
	};
	ble_gap_adv_data_t gap_adv_data = {
		.adv_data.p_data = adv_data_buf,
		.adv_data.len = sizeof(adv_data_buf),
		.scan_rsp_data.p_data = NULL,
		.scan_rsp_data.len = 0,
	};
	ble_gap_adv_params_t adv_params = {
		.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED,
		.duration = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED,
		.interval = BLE_GAP_ADV_INTERVAL_MIN,
		.filter_policy = BLE_GAP_ADV_FP_ANY,
		.primary_phy = BLE_GAP_PHY_AUTO,
	};

	nrf_err = sd_ble_gap_adv_set_configure(&adv_handle, &gap_adv_data, &adv_params);
	if (nrf_err) {
		TEST_FAIL("sd_ble_gap_adv_set_configure failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = sd_ble_gap_adv_start(adv_handle, CONN_TAG);
	if (nrf_err) {
		TEST_FAIL("sd_ble_gap_adv_start failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static void test_hrs_peripheral_init(void)
{
	bst_ticker_set_next_tick_absolute(WAIT_TIME * 1e6);
	bst_result = In_progress;
}

static void test_hrs_peripheral_tick(bs_time_t HW_device_time)
{
	ARG_UNUSED(HW_device_time);
	if (bst_result != Passed) {
		TEST_FAIL("hrs_peripheral failed (not passed after %i seconds)", WAIT_TIME);
	}
}

static void test_hrs_peripheral_main(void)
{
	int err;
	uint32_t nrf_err;
	uint8_t body_sensor_location = BLE_HRS_BODY_SENSOR_LOCATION_WRIST;
	struct ble_hrs_config hrs_cfg = {
		.evt_handler = ble_hrs_evt_handler,
		.is_sensor_contact_supported = false,
		.body_sensor_location = &body_sensor_location,
		.sec_mode = BLE_HRS_CONFIG_SEC_MODE_DEFAULT,
	};

	TEST_START("peripheral");
	hrs_bstest_set_running_id(HRS_BSTEST_ID_PERIPHERAL);

	LOG_INF("BLE HRS service test (peripheral) started");

	err = nrf_sdh_enable_request();
	if (err) {
		TEST_FAIL("Failed to enable SoftDevice, err %d", err);
		return;
	}

	nrf_err = nrf_sdh_ble_enable(CONN_TAG);
	if (nrf_err) {
		TEST_FAIL("Failed to enable BLE, nrf_error %#x", nrf_err);
		return;
	}

	nrf_err = ble_hrs_init(&ble_hrs, &hrs_cfg);
	if (nrf_err) {
		TEST_FAIL("ble_hrs_init failed, nrf_error %#x", nrf_err);
		return;
	}

	nrf_err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (nrf_err) {
		TEST_FAIL("ble_conn_params_evt_handler_set failed, nrf_error %#x", nrf_err);
		return;
	}

	nrf_err = adv_start();
	if (nrf_err) {
		TEST_FAIL("adv_start failed, nrf_error %#x", nrf_err);
		return;
	}

	LOG_INF("Advertising..");

	while (true) {
		k_sleep(K_SECONDS(1));
	}
}

static const struct bst_test_instance test_hrs_peripheral[] = {
	{
		.test_id = "peripheral",
		.test_descr = "HRS service test (ble_hrs)."
			      " Advertises HRS, accepts connection, sends HR notifications.",
		.test_pre_init_f = test_hrs_peripheral_init,
		.test_tick_f = test_hrs_peripheral_tick,
		.test_main_f = test_hrs_peripheral_main,
	},
	BSTEST_END_MARKER,
};

struct bst_test_list *test_hrs_peripheral_install(struct bst_test_list *tests)
{
	return bst_add_tests(tests, test_hrs_peripheral);
}
