# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

find_program(BASH_PROGRAM bash)

if (BASH_PROGRAM)
  add_test(OpenEXR.bin.exrheader ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test_exrheader.sh)
endif()
