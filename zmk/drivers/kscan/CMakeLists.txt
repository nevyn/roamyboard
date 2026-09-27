# SPDX-License-Identifier: MIT

if(CONFIG_ROAMYBOARD_KSCAN_165_CHAIN)
  zephyr_library()
  zephyr_library_sources(chain.c kscan_165_chain.c)
endif()
