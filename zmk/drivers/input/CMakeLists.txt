# SPDX-License-Identifier: MIT

if(CONFIG_ROAMYBOARD_INPUT_IQS5XX)
  zephyr_library()
  # ZMK's own headers (zmk/event_manager.h) live in the app, not in a module.
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_sources(iqs5xx_logic.c iqs5xx.c)
endif()
