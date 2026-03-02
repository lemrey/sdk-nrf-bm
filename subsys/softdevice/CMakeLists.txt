#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_SOFTDEVICE_S115 OR CONFIG_SOFTDEVICE_S145)
  zephyr_include_directories("${ZEPHYR_NRF_BM_MODULE_DIR}/components/softdevice/${CONFIG_SOFTDEVICE_SOC_VARIANT_STR}/${CONFIG_SOFTDEVICE_VARIANT_STR}/${CONFIG_SOFTDEVICE_VARIANT_STR}_API/include")
endif()

# When using a linkable SoftDevice for Babblesim, link the library from
# nrf-bm-internal: components/softdevice/bsim/<variant>/libsoftdevice_<variant>.a
# SVCALL_AS_NORMAL_FUNCTION makes BLE API headers declare normal functions (no ARM SVC asm).
if(CONFIG_SOFTDEVICE_BSIM)
  zephyr_compile_definitions(SVCALL_AS_NORMAL_FUNCTION)
  if(NOT DEFINED ZEPHYR_NRF_LITE_INTERNAL_MODULE_DIR)
    message(FATAL_ERROR "nrf-bm-internal module not found. Add nrf-bm-internal to west manifest for bsim builds.")
  endif()
  set(SOFTDEVICE_BSIM_LIB_PATH "${ZEPHYR_NRF_LITE_INTERNAL_MODULE_DIR}/components/softdevice/bsim/${CONFIG_SOFTDEVICE_VARIANT_STR}")
  set(SOFTDEVICE_BSIM_LIB_NAME "softdevice_${CONFIG_SOFTDEVICE_VARIANT_STR}")
  set(SOFTDEVICE_BSIM_LIB_FILE "${SOFTDEVICE_BSIM_LIB_PATH}/lib${SOFTDEVICE_BSIM_LIB_NAME}.a")
  if(NOT EXISTS "${SOFTDEVICE_BSIM_LIB_FILE}")
    message(FATAL_ERROR "SoftDevice library not found: ${SOFTDEVICE_BSIM_LIB_FILE}")
  endif()
  add_library(softdevice_bsim STATIC IMPORTED GLOBAL)
  set_target_properties(softdevice_bsim PROPERTIES IMPORTED_LOCATION "${SOFTDEVICE_BSIM_LIB_FILE}")
  zephyr_link_libraries(softdevice_bsim)
endif()
