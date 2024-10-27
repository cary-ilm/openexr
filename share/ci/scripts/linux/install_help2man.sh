#!/bin/bash

set -ex

HELP2MAN_VERSION="1.49.3"
HELP2MAN_URL="https://ftp.gnu.org/gnu/help2man/help2man-$HELP2MAN_VERSION.tar.xz"
HELP2MAN_DIR="help2man-$HELP2MAN_VERSION"

if [[ $OSTYPE == *msys* ]]; then
    SUDO=""
    HELP2MAN_BIN="help2man.exe"
else
    SUDO="sudo"
    HELP2MAN_BIN=""
fi

# Download help2man source code
echo "Downloading help2man version $HELP2MAN_VERSION..."
curl -O "$HELP2MAN_URL"

# Extract the downloaded tarball. On ChangeLog is a symlink, which
# Windows doesn't like, so exclude it.
echo "Extracting help2man..."
tar -xf "$HELP2MAN_DIR.tar.xz" --exclude="*/ChangeLog"

# Navigate into the help2man source directory
cd "$HELP2MAN_DIR"

# Configure, build, and install help2man
echo "Configuring help2man..."
./configure

echo "Building help2man..."
make

echo "Installing help2man..."
$SUDO make install $HELP2MAN_BIN

find /usr/local -name help2man

# Verify the installation
./$HELP2MAN_BIN --version 

echo "help2man installed successfully!"


