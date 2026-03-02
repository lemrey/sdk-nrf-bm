#!/usr/bin/env bash
# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Build HRS service test for Babblesim (Babblesim test kit: one binary, -testid=peripheral|central).
#
# Prerequisites: BSIM_OUT_PATH, ZEPHYR_BASE, west.
# Prebuilt SoftDevice in nrf-bm-internal (components/softdevice/bsim/s115 and s145)
#
# Usage (from NCS root):
#   source zephyr/tests/bsim/sh_common.source
#   ./nrf-bm/tests/bsim/hrs/compile_hrs_bsim.sh

set -ue

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NRF_BM_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

: "${BSIM_OUT_PATH:?BSIM_OUT_PATH must be defined}"
: "${ZEPHYR_BASE:?ZEPHYR_BASE must be set}"
BOARD="${BOARD:-nrf54l15bsim/nrf54l15/cpuapp}"
BUILD_DIR="${SCRIPT_DIR}/build"

# One binary for both roles (selected at run time with -testid=)
west build -b "${BOARD}" "${NRF_BM_ROOT}/tests/bsim/hrs" -d "${BUILD_DIR}" --no-sysbuild

if [ -f "${BUILD_DIR}/zephyr/zephyr.exe" ]; then
  echo "Done. Run: ./nrf-bm/tests/bsim/hrs/tests_scripts/hrs_service.sh"
else
  echo "Error: no zephyr.exe in ${BUILD_DIR}/zephyr" >&2
  exit 1
fi
