/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * HRS central role for Babblesim test kit: tests ble_hrs_client service.
 * Run with -testid=central -d=1. Passes when 5 HRS notifications received.
 */

#include "bs_types.h"
#include "bs_tracing.h"
#include "bstests.h"
#include "babblekit/testcase.h"
#include "hrs_bstest_common.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ble_gap.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/ble_gq.h>
#include <bm/bluetooth/services/ble_hrs_client.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hrs_bstest_central, LOG_LEVEL_INF);

#define CONN_TAG CONFIG_NRF_SDH_BLE_CONN_TAG
#define HRS_TEST_PASS_THRESHOLD 5
#define WAIT_TIME 10 /* seconds */

#define BLE_GAP_AD_TYPE_COMPLETE_LIST_16BIT_UUID 0x03
#define BLE_UUID_HRS_SERVICE 0x180D

extern enum bst_result_t bst_result;

static uint8_t scan_buf[BLE_GAP_SCAN_BUFFER_MIN];
static ble_gap_scan_params_t scan_params = {
	.extended = 0,
	.report_incomplete_evts = 0,
	.active = 0,
	.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
	.scan_phys = BLE_GAP_PHY_AUTO,
	.interval = BLE_GAP_SCAN_INTERVAL_US_MIN * 6 / 625,
	.window = BLE_GAP_SCAN_WINDOW_US_MIN * 6 / 625,
	.timeout = BLE_GAP_SCAN_TIMEOUT_UNLIMITED,
	.channel_mask[4] = (1u << 6) | (1u << 7),
};
static const ble_gap_conn_params_t conn_params = {
	.min_conn_interval = 0x0006, /* 7.5 ms */
	.max_conn_interval = 0x000C, /*  15 ms */
	.slave_latency = 0,
	.conn_sup_timeout = 0x00C8,  /* 2000 ms */
};

BLE_HRS_CLIENT_DEF(ble_hrs_client);
BLE_GQ_DEF(ble_gq);
BLE_DB_DISCOVERY_DEF(ble_db_disc);

static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
static uint32_t hrs_notifications_received;

static bool adv_data_contains_hrs(const uint8_t *data, uint16_t len)
{
	for (uint16_t i = 0; i + 2 <= len;) {
		uint8_t field_len = data[i];
		if (field_len == 0 || i + 1 + field_len > len) {
			break;
		}
		uint8_t type = data[i + 1];
		if (type == BLE_GAP_AD_TYPE_COMPLETE_LIST_16BIT_UUID && field_len >= 2) {
			const uint8_t *uuid = &data[i + 2];
			for (uint8_t j = 0; j + 2 <= field_len; j += 2) {
				if (uuid[j] == (BLE_UUID_HRS_SERVICE & 0xFF) &&
				    uuid[j + 1] == (BLE_UUID_HRS_SERVICE >> 8)) {
					return true;
				}
			}
		}
		i += 1 + field_len;
	}
	return false;
}

static void db_disc_handler(struct ble_db_discovery *db_discovery, struct ble_db_discovery_evt *evt)
{
	ble_hrs_on_db_disc_evt(&ble_hrs_client, evt);
}

static void hrs_c_evt_handler(struct ble_hrs_client *hrs, struct ble_hrs_client_evt *evt)
{
	uint32_t nrf_err;

	LOG_DBG("HRS evt_type=%u", (unsigned)evt->evt_type);
	switch (evt->evt_type) {
	case BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE:
		nrf_err = ble_hrs_client_handles_assign(hrs, evt->conn_handle, &evt->params.peer_db);
		if (nrf_err) {
			LOG_ERR("ble_hrs_client_handles_assign failed, nrf_error %#x", nrf_err);
			break;
		}
		nrf_err = ble_hrs_client_hrm_notif_enable(hrs);
		if (nrf_err) {
			LOG_ERR("ble_hrs_client_hrm_notif_enable failed, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION: {
		int hr = evt->params.hrm.hr_value;
		LOG_INF("Heart Rate = %d", hr);
		hrs_notifications_received++;
		if (hrs_notifications_received == HRS_TEST_PASS_THRESHOLD) {
			TEST_PASS("Received %d notifications", hrs_notifications_received);
		}
		break;
	}

	case BLE_HRS_CLIENT_EVT_ERROR:
		LOG_ERR("HRS client error");
		break;

	default:
		break;
	}
}

static void on_ble_evt(const ble_evt_t *ble_evt, void *ctx)
{
	uint32_t nrf_err;
	const ble_gap_evt_t *gap_evt = &ble_evt->evt.gap_evt;

	const char *running = hrs_bstest_get_running_id();

	if (!running || strcmp(running, HRS_BSTEST_ID_CENTRAL) != 0) {
		/* skip messages not for this peer */
		return;
	}

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_ADV_REPORT: {
		const ble_gap_evt_adv_report_t *r = &gap_evt->params.adv_report;
		if (r->type.connectable && adv_data_contains_hrs(r->data.p_data, r->data.len)) {
			LOG_INF("HRS advertiser found, connecting...");
			nrf_err = sd_ble_gap_connect(&r->peer_addr, &scan_params, &conn_params,
						     CONN_TAG);
			if (nrf_err) {
				TEST_FAIL("sd_ble_gap_connect failed, nrf_error %#x", nrf_err);
			}
		}
	} break;

	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Connected");
		conn_handle = gap_evt->conn_handle;
		nrf_err = ble_db_discovery_start(&ble_db_disc, conn_handle);
		if (nrf_err) {
			TEST_FAIL("ble_db_discovery_start failed, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Disconnected, reason %#x", gap_evt->params.disconnected.reason);
		conn_handle = BLE_CONN_HANDLE_INVALID;
		break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		nrf_err = sd_ble_gap_conn_param_update(
			gap_evt->conn_handle,
			&gap_evt->params.conn_param_update_request.conn_params);
		if (nrf_err) {
			TEST_FAIL("sd_ble_gap_conn_param_update failed, nrf_error %#x", nrf_err);
		}
		break;

	default:
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static uint32_t db_discovery_init(void)
{
	struct ble_db_discovery_config db_init = {
		.evt_handler = db_disc_handler,
		.gatt_queue = &ble_gq,
	};
	return ble_db_discovery_init(&ble_db_disc, &db_init);
}

static uint32_t hrs_c_init(void)
{
	struct ble_hrs_client_config hrs_client_cfg = {
		.evt_handler = hrs_c_evt_handler,
		.gatt_queue = &ble_gq,
		.db_discovery = &ble_db_disc,
	};
	return ble_hrs_client_init(&ble_hrs_client, &hrs_client_cfg);
}

static uint32_t scan_start(void)
{
	uint32_t nrf_err;
	ble_data_t adv_report_buf = {
		.p_data = scan_buf,
		.len = sizeof(scan_buf),
	};

	nrf_err = sd_ble_gap_scan_start(&scan_params, &adv_report_buf);
	if (nrf_err) {
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static void test_hrs_central_init(void)
{
	bst_ticker_set_next_tick_absolute(WAIT_TIME * 1e6);
	bst_result = In_progress;
}

static void test_hrs_central_tick(bs_time_t HW_device_time)
{
	ARG_UNUSED(HW_device_time);
	if (bst_result != Passed) {
		TEST_FAIL("hrs_central failed (not passed after %i seconds, got %u notifications)",
			  WAIT_TIME, (unsigned)hrs_notifications_received);
	}
}

static void test_hrs_central_main(void)
{
	int err;
	uint32_t nrf_err;

	TEST_START("central");
	hrs_bstest_set_running_id(HRS_BSTEST_ID_CENTRAL);

	LOG_INF("BLE HRS service test (central) started");

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

	nrf_err = db_discovery_init();
	if (nrf_err) {
		TEST_FAIL("db_discovery_init failed, nrf_error %#x", nrf_err);
		return;
	}

	nrf_err = hrs_c_init();
	if (nrf_err) {
		TEST_FAIL("hrs_c_init failed, nrf_error %#x", nrf_err);
		return;
	}

	nrf_err = scan_start();
	if (nrf_err) {
		TEST_FAIL("scan_start failed, nrf_error %#x", nrf_err);
		return;
	}

	LOG_INF("Scanning...");

	while (true) {
		k_sleep(K_SECONDS(1));
	}
}

static const struct bst_test_instance test_hrs_central[] = {
	{
		.test_id = "central",
		.test_descr = "HRS service test (ble_hrs_client)"
			      "Scans, connects, receives 5 HR notifications.",
		.test_pre_init_f = test_hrs_central_init,
		.test_tick_f = test_hrs_central_tick,
		.test_main_f = test_hrs_central_main,
	},
	BSTEST_END_MARKER,
};

struct bst_test_list *test_hrs_central_install(struct bst_test_list *tests)
{
	return bst_add_tests(tests, test_hrs_central);
}
