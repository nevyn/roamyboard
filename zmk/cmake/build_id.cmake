# SPDX-License-Identifier: MIT
#
# Writes the roamyboard build ID (docs/firmware.md, Build ID) to the C header OUT, and
# rewrites the header only when the ID changed. Runs once at configure time (include)
# and before every build (cmake -P). Never fails: without git the ID is "unknown".
#
# Inputs: SOURCE_DIR, a directory inside the repo; OUT, the header to write; optionally the
# environment variables ROAMYBOARD_GIT_DIR and ROAMYBOARD_GIT_WORK_TREE.

set(roamyboard_build_id "unknown")
find_program(ROAMYBOARD_GIT git)

# safe.directory: the build container mounts the repo with another owner than its user.
set(roamyboard_git ${ROAMYBOARD_GIT} -c safe.directory=* -C ${SOURCE_DIR})
# A worktree's git directory lives outside the worktree; docs/firmware.md shows how to mount it.
if(DEFINED ENV{ROAMYBOARD_GIT_DIR})
  list(APPEND roamyboard_git --git-dir=$ENV{ROAMYBOARD_GIT_DIR}
       --work-tree=$ENV{ROAMYBOARD_GIT_WORK_TREE})
endif()

if(NOT ROAMYBOARD_GIT)
  message(STATUS "roamyboard build ID: unknown (git not found)")
else()
  execute_process(
    COMMAND ${roamyboard_git} rev-parse --short=7 HEAD
    RESULT_VARIABLE roamyboard_rc
    OUTPUT_VARIABLE roamyboard_commit
    ERROR_VARIABLE roamyboard_error
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT roamyboard_rc EQUAL 0)
    message(STATUS "roamyboard build ID: unknown (git rev-parse in ${SOURCE_DIR} failed: "
                   "${roamyboard_error})")
  else()
    string(SUBSTRING "${roamyboard_commit}" 0 7 roamyboard_build_id)
    # Only zmk/ goes into the firmware; outside it, Git LFS files look modified to a git
    # without git-lfs, as in the build image. diff compares contents, so a stale index is fine.
    execute_process(
      COMMAND ${roamyboard_git} diff --quiet HEAD -- .
      RESULT_VARIABLE roamyboard_rc
      ERROR_VARIABLE roamyboard_error
      ERROR_STRIP_TRAILING_WHITESPACE)
    if(roamyboard_rc EQUAL 1)
      string(APPEND roamyboard_build_id "-dirty")
    elseif(NOT roamyboard_rc EQUAL 0)
      message(STATUS "roamyboard build ID: cannot tell whether the tree is dirty "
                     "(git diff failed: ${roamyboard_error})")
    endif()
    message(STATUS "roamyboard build ID: ${roamyboard_build_id}")
  endif()
endif()

set(roamyboard_header "#define ROAMYBOARD_BUILD_ID \"${roamyboard_build_id}\"\n")
if(EXISTS ${OUT})
  file(READ ${OUT} roamyboard_old_header)
endif()
if(NOT roamyboard_header STREQUAL roamyboard_old_header)
  file(WRITE ${OUT} "${roamyboard_header}")
endif()
