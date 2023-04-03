#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.

set -ex

HERE=$(dirname $0)

bash $HERE/install_doxygen.sh latest
pip3 install -r $HERE/../../../../../docs/requirements.txt

yum -y install wget

wget https://imagemagick.org/archive/binaries/magick -O /usr/local/bin/convert
chmod +x /usr/local/bin/convert
/usr/local/bin/convert --version

which convert
