/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

/**
 * Returns the build ID of the running firmware: the short git commit that it was built
 * from, exactly 7 lowercase hex digits ("3274ed3"), followed by "-dirty" if tracked files
 * under zmk/ had uncommitted changes ("3274ed3-dirty"), or "unknown" if the build could not run git.
 * The string is static, NUL-terminated and at most 13 characters long. docs/firmware.md
 * describes how the build computes it.
 */
const char *roamyboard_build_id(void);
