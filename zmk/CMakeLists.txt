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

if(CONFIG_ROAMYBOARD_BEHAVIOR_BOOT_SCREEN)
  target_sources(app PRIVATE src/behaviors/behavior_boot_screen.c)
endif()
