#!/usr/bin/env python3

import os
import sys
import argparse
from pathlib import Path

def normalize_path(path):
    return path.replace("lib64/", "lib/").replace(".so", "").replace(".dylib", "").replace(".dll", "")

def load_manifest(file_path):
    """Load and return the list of files from the install manifest."""
    with open(file_path, 'r') as file:
        return sorted(normalize_path(line.strip().split("/_install/", 1)[-1]) for line in file if "deflate" not in line and line[0]!='#')

def validate_install(generated_manifest_path, committed_manifest_path, options):
    """Main function to verify the installed files."""

    generated_manifest = load_manifest(generated_manifest_path)
    committed_manifest = load_manifest(committed_manifest_path)

    print("committed_manifest:")
    for l in committed_manifest:
        print(f"  {l}")
    print("generated_manifest:")
    for l in generated_manifest:
        print(f"  {l}")
    
    # Compare manifests
    missing_files = sorted(set(committed_manifest) - set(generated_manifest))
    extra_files = sorted(set(generated_manifest) - set(committed_manifest))

    # Output results
    if missing_files:
        print("Error: The following files should have been installed but weren't:\n  " + '\n  '.join(missing_files))
    if extra_files:
        print("Error: The following files were installed but were not expected:\n  " + '\n  '.join(extra_files))
    
    if missing_files or extra_files:
        return 1

    print("valid.")
    
    return 0

if __name__ == "__main__":

    print(f"validate_install: {sys.argv}")

    parser = argparse.ArgumentParser(description="Validate installed files against committed install manifest.")
    parser.add_argument("generated_manifest", help="Path to the generated install_manifest.txt")
    parser.add_argument("committed_manifest", help="Path to the committed install_manifest.txt")

    args = parser.parse_args()

    print(f"generated_manifest={args.generated_manifest}")
    print(f"committed_manifest={args.committed_manifest}")

    status = validate_install(args.generated_manifest, args.committed_manifest, args)

    sys.exit(status)
    
