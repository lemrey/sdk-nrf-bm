#!/usr/bin/env bash
# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

: "${BSIM_OUT_PATH:?BSIM_OUT_PATH must be defined}"
: "${ZEPHYR_BASE:?ZEPHYR_BASE must be set}"

export BOARD="${BOARD:-nrf54l15bsim/nrf54l15/cpuapp}"
source ${ZEPHYR_BASE}/tests/bsim/sh_common.source

simulation_id="hrs_service_test"
verbosity_level=3

# Run the binary where it was built; run from BSIM_OUT_PATH/bin so PHY finds ../lib
TEST_DIR="$(cd "$(dirname "$0")/.." && pwd)"
HRS_EXE="${TEST_DIR}/build/zephyr/zephyr.exe"
[ -f "${HRS_EXE}" ] || { echo "Error: run compile_hrs_bsim.sh first" >&2; exit 1; }

cd "${BSIM_OUT_PATH}/bin"
Execute "${HRS_EXE}" -v=${verbosity_level} -s=${simulation_id} -d=0 -testid=peripheral -rs=23
Execute "${HRS_EXE}" -v=${verbosity_level} -s=${simulation_id} -d=1 -testid=central -rs=6
Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} -D=2 -sim_length=3e6 $@

wait_for_background_jobs
