#!/bin/bash

# Exit immediately if any command fails
set -e

# Variables
HELP2MAN_VERSION="1.49.3"
HELP2MAN_URL="https://ftp.gnu.org/gnu/help2man/help2man-$HELP2MAN_VERSION.tar.xz"
HELP2MAN_DIR="help2man-$HELP2MAN_VERSION"

# # Install necessary build dependencies
# if command -v apt-get > /dev/null; then
#   echo "Using apt-get to install dependencies..."
#   sudo apt-get update
#   sudo apt-get install -y build-essential wget gettext
# elif command -v apk > /dev/null; then
#   echo "Using apk to install dependencies..."
#   sudo apk update
#   sudo apk add build-base wget gettext-dev
# elif command -v yum > /dev/null; then
#   echo "Using yum to install dependencies..."
#   sudo yum groupinstall -y "Development Tools"
#   sudo yum install -y wget gettext
# else
#   echo "Unsupported package manager. Please install build tools manually."
#   exit 1
# fi

# Download help2man source code
echo "Downloading help2man version $HELP2MAN_VERSION..."
curl -O "$HELP2MAN_URL"

# Extract the downloaded tarball
echo "Extracting help2man..."
tar -xf "$HELP2MAN_DIR.tar.xz"

# Navigate into the help2man source directory
cd "$HELP2MAN_DIR"

# Configure, build, and install help2man
echo "Configuring help2man..."
./configure

echo "Building help2man..."
make

echo "Installing help2man..."
sudo make install

# Verify the installation
if command -v help2man > /dev/null; then
  echo "help2man installed successfully!"
else
  echo "help2man installation failed!"
  exit 1
fi
