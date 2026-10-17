# SPDX-License-Identifier: MIT

zephyr_include_directories(include)

add_subdirectory(drivers/kscan)

# The build ID header is regenerated before every build, so an incremental build cannot keep
# a stale ID; build_id.cmake rewrites it only when the ID changes.
set(ROAMYBOARD_BUILD_ID_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
function(roamyboard_build_id_at_configure)
  set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  set(OUT ${ROAMYBOARD_BUILD_ID_DIR}/roamyboard_build_id.h)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_id.cmake)
endfunction()
roamyboard_build_id_at_configure()
add_custom_target(roamyboard_build_id
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
          -DOUT=${ROAMYBOARD_BUILD_ID_DIR}/roamyboard_build_id.h
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_id.cmake
  BYPRODUCTS ${ROAMYBOARD_BUILD_ID_DIR}/roamyboard_build_id.h
  VERBATIM)
add_dependencies(app roamyboard_build_id)
target_include_directories(app PRIVATE ${ROAMYBOARD_BUILD_ID_DIR})
target_sources(app PRIVATE src/build_id.c)

if(CONFIG_ROAMYBOARD_KSCAN_165_CHAIN)
  target_sources(app PRIVATE src/events/chain_state_changed.c)
endif()

if(CONFIG_ROAMYBOARD_HOST_NAMES)
  target_sources(app PRIVATE src/ble/host_names.c)
endif()

if(CONFIG_ROAMYBOARD_PDA)
  target_sources(app PRIVATE src/events/pda_changed.c src/pda/note.c src/pda/keys.c src/pda/clock.c)
  if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    target_sources(app PRIVATE src/pda/central.c src/pda/host_time.c)
  else()
    target_sources(app PRIVATE
      src/pda/peripheral.c
      src/pda/shell.c
      src/pda/icons.c
      src/pda/gfx.c
      src/pda/apps.c
      src/pda/app_clock.c
      src/pda/app_timer.c
      src/pda/app_calc.c
      src/pda/app_dice.c
      src/pda/app_kana.c
      src/pda/game_breakout.c
      src/pda/game_tetris.c
      src/pda/game_snake.c
      src/pda/game_words.c
      src/pda/game_rain.c
      src/pda/words.c
    )
  endif()
endif()

if(CONFIG_ROAMYBOARD_BEHAVIOR_PDA)
  target_sources(app PRIVATE src/behaviors/behavior_pda.c)
endif()

if(CONFIG_ROAMYBOARD_STATUS_SCREEN)
  target_sources(app PRIVATE
    src/display/bolt.c
    src/display/cat.c
    src/display/cat_frames.c
    src/display/font_pda_12.c
    src/display/font_kana_40.c
    src/display/pda_text.c
    src/display/screen.c
    src/display/status_screen.c
    src/display/util.c
  )
endif()

if(CONFIG_ROAMYBOARD_BEHAVIOR_BOOT_SCREEN OR CONFIG_ROAMYBOARD_LOCAL_OTA)
  target_sources(app PRIVATE src/bootloader.c)
endif()

if(CONFIG_ROAMYBOARD_BEHAVIOR_BOOT_SCREEN)
  target_sources(app PRIVATE src/behaviors/behavior_boot_screen.c)
endif()

if(CONFIG_ROAMYBOARD_LOCAL_OTA)
  target_sources(app PRIVATE src/local_ota.c)
endif()

if(CONFIG_ROAMYBOARD_MIDI)
  target_sources(app PRIVATE src/midi/ble_midi.c)
endif()

if(CONFIG_ROAMYBOARD_BEHAVIOR_MIDI_NOTE)
  target_sources(app PRIVATE src/behaviors/behavior_midi_note.c)
endif()
