# SPDX-License-Identifier: MIT

zephyr_include_directories(include)

add_subdirectory(drivers/kscan)

if(CONFIG_ROAMYBOARD_KSCAN_165_CHAIN)
  target_sources(app PRIVATE src/events/chain_state_changed.c)
endif()
