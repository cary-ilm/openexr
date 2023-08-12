
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.
#

import pytest

def test_import():
    import OpenEXR
    assert OpenEXR.__name__ == "OpenEXR"
