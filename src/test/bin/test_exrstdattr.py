#!/usr/bin/env python

import sys, os, tempfile, atexit
from subprocess import PIPE, run

print(f"testing exrstdattr in python: {sys.argv}")

exrstdattr = f"{sys.argv[1]}/exrstdattr"
exrheader = f"{sys.argv[1]}/exrheader"
src_dir = f"{sys.argv[2]}"

fd, outimage = tempfile.mkstemp(".exr")
#outimage = "/var/tmp/exrstdattr.exr"

def cleanup():
    print(f"deleting {outimage}")
atexit.register(cleanup)

def find_line(keyword, lines):
    for line in lines:
        if keyword in line:
            return line
    return None

attrs = {
    "screenWindowCenter" : "42 43",
    "screenWindowWidth" : "4.4",
    "pixelAspectRatio" : "1.7",
    "wrapmodes" : "clamp",
    "timeCode" : "12345678 34567890",
    "keyCode" : "1 2 3 4 5 6 20",
    "framesPerSecond" : "48 1",
    "envmap" : "LATLONG",
    "isoSpeed" : "2.1",
    "aperture" : "3.2",
    "expTime" : "4.3",
    "focus" : "5.4",
    "altitude" : "6.5",
    "latitude" : "7.6",
    "longitude" : "8.7",
    "utcOffset" : "9",
    "owner" : "florian",
    "xDensity" : "10.0",
    "lookModTransform" : "lmt",
    "renderingTransform" : "rt",
    "adoptedNeutral" : "1.1 2.2",
    "whiteLuminance" : "17.1",
    "chromaticities" : "1 2 3 4 5 6 7 8",
}

command = [exrstdattr]
for a in attrs:
    command += [f"-{a}"]
    command += attrs[a].split(' ')

image = f"{src_dir}/GrayRampsHorizontal.exr"
command += [image, outimage]

attrs["test_int"] = "42"
attrs["test_float"] = "4.2"
attrs["test_string"] = "forty two"
attrs["capDate"] = "1999:12:31 23:59:59"
attrs["comments"] = "blah blah blah"

command += ["-int", "test_int", attrs["test_int"]]
command += ["-float", "test_float", attrs["test_float"]]
command += ["-string", "test_string", attrs["test_string"]]
command += ["-capDate", attrs["capDate"]]
command += ["-comments", attrs["comments"]]

# no args = usage message
result = run ([exrstdattr], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
assert(result.returncode == 1)
assert(result.stderr.startswith ("usage: "))

print (f"outimage={outimage}")

print(f"command: {command}")
print(attrs)

result = run (command, stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
#print(f"result.returncode={result.returncode}")
#print(f"result.stderr={result.stderr}")
#print(f"result.stdout={result.stdout}")
assert(result.returncode == 0)
#print(result)

result = run ([exrheader, outimage], stdout=PIPE, stderr=PIPE, universal_newlines=True)
print(" ".join(result.args))
output = result.stdout.split('\n')
for o in output:
    print(o)

for a in attrs:
    if a in ["keyCode", "timeCode", "framesPerSecond", "envmap"]:
        continue
    v = attrs[a]
    print(f"'{v}' in find_line({a}) {find_line(a,output)}")
    assert (v in find_line(a, output))

print("success")
