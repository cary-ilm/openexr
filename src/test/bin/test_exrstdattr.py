#!/usr/bin/env python

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

import sys, os, tempfile, atexit
from subprocess import PIPE, run

print(f"testing exrstdattr: {sys.argv}")

exrstdattr = f"{sys.argv[1]}/exrstdattr"
exrheader = f"{sys.argv[1]}/exrheader"
image_dir = f"{sys.argv[2]}"

fd, outimage = tempfile.mkstemp(".exr")

def cleanup():
    print(f"deleting {outimage}")
atexit.register(cleanup)

# no args = usage message
result = run ([exrstdattr], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 1)
assert(result.stderr.startswith ("usage: "))

# -h = usage message
result = run ([exrstdattr, "-h"], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 1)
assert(result.stderr.startswith ("usage: "))

command = [exrstdattr]
command += ["-part", "0"]
command += ["-screenWindowCenter", "42", "43"]
command += ["-screenWindowWidth", "4.4"]
command += ["-pixelAspectRatio", "1.7"]
command += ["-wrapmodes", "clamp"]
command += ["-timeCode", "12345678", "34567890"]
command += ["-keyCode", "1", "2", "3", "4", "5", "6", "20"]
command += ["-framesPerSecond", "48", "1"]
command += ["-envmap", "LATLONG"]
command += ["-isoSpeed", "2.1"]
command += ["-aperture", "3.2"]
command += ["-expTime", "4.3"]
command += ["-focus", "5.4"]
command += ["-altitude", "6.5"]
command += ["-latitude", "7.6"]
command += ["-longitude", "8.7"]
command += ["-utcOffset", "9"]
command += ["-owner", "florian"]
command += ["-xDensity", "10.0"]
command += ["-lookModTransform", "lmt"]
command += ["-renderingTransform", "rt"]
command += ["-adoptedNeutral", "1.1", "2.2"]
command += ["-whiteLuminance", "17.1"]
command += ["-chromaticities", "1", "2", "3", "4", "5", "6", "7", "8"]
command += ["-int", "test_int", "42"]
command += ["-float", "test_float", "4.2"]
command += ["-string", "test_string", "forty two"]
command += ["-capDate", "1999:12:31 23:59:59"]
command += ["-comments", "blah blah blah"]
image = f"{image_dir}/TestImages/GrayRampsHorizontal.exr"
command += [image, outimage]

result = run (command, stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 0)

result = run ([exrheader, outimage], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert ('file format version: 2, flags 0x0' in result.stdout)
assert ('adoptedNeutral (type v2f): (1.1 2.2)' in result.stdout)
assert ('altitude (type float): 6.5' in result.stdout)
assert ('aperture (type float): 3.2' in result.stdout)
assert ('capDate (type string): "1999:12:31 23:59:59"' in result.stdout)
assert ('channels (type chlist):' in result.stdout)
assert ('Y, 16-bit floating-point, sampling 1 1' in result.stdout)
assert ('chromaticities (type chromaticities):\n'
        '    red   (1 2)\n'
        '    green (3 4)\n'
        '    blue  (5 6)\n'
        '    white (7 8)\n' in result.stdout)
assert ('comments (type string): "blah blah blah"' in result.stdout)
assert ('compression (type compression): pxr24' in result.stdout)
assert ('dataWindow (type box2i): (0 0) - (799 799)' in result.stdout)
assert ('displayWindow (type box2i): (0 0) - (799 799)' in result.stdout)
assert ('envmap (type envmap): latitude-longitude map' in result.stdout)
assert ('expTime (type float): 4.3' in result.stdout)
assert ('focus (type float): 5.4' in result.stdout)
assert ('framesPerSecond (type rational): 48/1 (48)' in result.stdout)
assert ('isoSpeed (type float): 2.1' in result.stdout)
assert ('keyCode (type keycode):\n'
        '    film manufacturer code 1\n'
        '    film type code 2\n'
        '    prefix 3\n'
        '    count 4\n'
        '    perf offset 5\n'
        '    perfs per frame 6\n'
        '    perfs per count 20\n' in result.stdout)
assert ('latitude (type float): 7.6' in result.stdout)
assert ('lineOrder (type lineOrder): increasing y' in result.stdout)
assert ('longitude (type float): 8.7' in result.stdout)
assert ('lookModTransform (type string): "lmt"' in result.stdout)
assert ('owner (type string): "florian"' in result.stdout)
assert ('pixelAspectRatio (type float): 1.7' in result.stdout)
assert ('renderingTransform (type string): "rt"' in result.stdout)
assert ('screenWindowCenter (type v2f): (42 43)' in result.stdout)
assert ('screenWindowWidth (type float): 4.4' in result.stdout)
assert ('test_float (type float): 4.2' in result.stdout)
assert ('test_int (type int): 42' in result.stdout)
assert ('test_string (type string): "forty two"' in result.stdout)
assert ('timeCode (type timecode):\n'
        '    time 12:34:56:38\n'
        '    drop frame 1, color frame 0, field/phase 0\n'
        '    bgf0 0, bgf1 0, bgf2 0\n'
        '    user data 0x34567890\n' in result.stdout)
assert ('type (type string): "scanlineimage"' in result.stdout)
assert ('utcOffset (type float): 9' in result.stdout)
assert ('whiteLuminance (type float): 17.1' in result.stdout)
assert ('wrapmodes (type string): "clamp"' in result.stdout)
assert ('xDensity (type float): 10' in result.stdout)

print("success")
