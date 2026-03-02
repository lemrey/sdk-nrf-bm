# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# HRS service Babblesim test (single app, test kit).
# One binary, run with -testid=peripheral or -testid=central.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hrs_service_bstest)

target_sources(app PRIVATE
  src/main.c
  src/peripheral_test.c
  src/central_test.c
)

add_subdirectory(${ZEPHYR_BASE}/tests/bsim/babblekit babblekit)
target_link_libraries(app PRIVATE babblekit)

zephyr_include_directories(
  ${BSIM_COMPONENTS_PATH}/libUtilv1/src/
  ${BSIM_COMPONENTS_PATH}/libPhyComv1/src/
)
