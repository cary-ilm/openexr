# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

set -ex

bin=../../../bin/exrheader
DIR=`dirname $0`
IMAGES=$DIR/../OpenEXRTest

$bin $IMAGES/tiled.exr




