/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Babblesim test kit entry for HRS service test (same pattern as
 * zephyr/tests/bsim/bluetooth/samples/battery_service/src/main.c).
 * One binary; run with -testid=peripheral or -testid=central.
 */

#include "bstests.h"
#include "hrs_bstest_common.h"

extern struct bst_test_list *test_hrs_peripheral_install(struct bst_test_list *tests);
extern struct bst_test_list *test_hrs_central_install(struct bst_test_list *tests);

static const char *s_running_test_id;

void hrs_bstest_set_running_id(const char *id)
{
	s_running_test_id = id;
}

const char *hrs_bstest_get_running_id(void)
{
	return s_running_test_id;
}

bst_test_install_t test_installers[] = {
	test_hrs_peripheral_install,
	test_hrs_central_install,
	NULL,
};

int main(void)
{
	bst_main();
	return 0;
}
