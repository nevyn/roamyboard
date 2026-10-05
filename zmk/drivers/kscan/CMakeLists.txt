# SPDX-License-Identifier: MIT

if(CONFIG_ROAMYBOARD_KSCAN_165_CHAIN)
  zephyr_library()
  # ZMK's own headers (zmk/event_manager.h) live in the app, not in a module.
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_sources(chain.c kscan_165_chain.c)
endif()
