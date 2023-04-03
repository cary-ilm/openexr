#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.

set -ex

HERE=$(dirname $0)

bash $HERE/install_doxygen.sh latest
pip3 install -r $HERE/../../../../../docs/requirements.txt

yum -y install wget

wget https://imagemagick.org/archive/linux/CentOS/x86_64/ImageMagick-7.1.1-6.x86_64.rpm

rpm -Uvh ImageMagick-7.1.1-6.x86_64.rpm

which convert
convert --version
