# SPDX-License-Identifier: MIT

zephyr_include_directories(include)

add_subdirectory(drivers/kscan)

if(CONFIG_ROAMYBOARD_KSCAN_165_CHAIN)
  target_sources(app PRIVATE src/events/chain_state_changed.c)
endif()

if(CONFIG_ROAMYBOARD_STATUS_SCREEN)
  target_sources(app PRIVATE
    src/display/bolt.c
    src/display/cat.c
    src/display/cat_frames.c
    src/display/screen.c
    src/display/status_screen.c
    src/display/util.c
  )
endif()
