#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.

set -ex

cat /etc/os-release

HERE=$(dirname $0)

bash $HERE/install_doxygen.sh latest
pip3 install -r $HERE/../../../../../docs/requirements.txt

yum -y install wget

sudo apt install ImageMagick

which convert
convert --version

#wget https://imagemagick.org/archive/binaries/magick
#chmod u+x magick

#yum --enablerepo=epel -y install fuse-sshfs # install from EPEL
#user="$(whoami)"
#sudo groupadd fuse
#usermod -a -G fuse "$user"
#sudo modprobe fuse




