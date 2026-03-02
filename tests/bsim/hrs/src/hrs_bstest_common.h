/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Shared for HRS bstest: one binary runs as either peripheral or central
 * (-testid=peripheral | -testid=central). BLE observers from both roles are
 * linked; use these so only the active role's observer handles events.
 */

#ifndef HRS_BSTEST_COMMON_H__
#define HRS_BSTEST_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#define HRS_BSTEST_ID_PERIPHERAL "peripheral"
#define HRS_BSTEST_ID_CENTRAL    "central"

/** Set by the test main when it starts (peripheral or central). */
void hrs_bstest_set_running_id(const char *id);

/** Used by BLE observers to no-op when not the active role. */
const char *hrs_bstest_get_running_id(void);

#ifdef __cplusplus
}
#endif

#endif /* HRS_BSTEST_COMMON_H__ */
