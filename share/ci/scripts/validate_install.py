#!/usr/bin/env python3

import os
import sys
import argparse
from pathlib import Path

# Suffixes for shared object files by platform
SHARED_OBJECT_SUFFIXES = {
    'linux': '.so',
    'darwin': '.dylib',
    'win32': '.dll'
}

def get_shared_object_suffix():
    """Return the shared object suffix for the current platform."""
    platform = sys.platform
    return SHARED_OBJECT_SUFFIXES.get(platform, '')

def normalize_path(path, base_path):
    """Normalize the path by stripping the base path, removing leading slashes, and normalizing slashes."""
    normalized = os.path.normpath(path.replace(str(base_path), ''))
    if normalized.startswith(os.sep):
        normalized = normalized[1:]  # Remove the leading '/'
    return normalized

def load_manifest(file_path):
    """Load and return the list of files from the install manifest."""
    with open(file_path, 'r') as file:
        return sorted(line.strip().replace("lib64/", "lib/").replace(".dylib", ".so").replace(".dll", ".so").split("/_install/", 1)[-1] for line in file if "deflate" not in line and line[0]!='#')

def compare_manifests(generated_manifest, committed_manifest):
    """Compare the generated and committed manifests."""
    # Find differences
    missing_files = set(committed_manifest) - set(generated_manifest)
    extra_files = set(generated_manifest) - set(committed_manifest)

    return missing_files, extra_files

def check_suffix(filename):
    """Check if the file is a shared object file and adjust the suffix."""
    for suffix in SHARED_OBJECT_SUFFIXES.values():
        if filename.endswith(suffix):
            return filename.rsplit(suffix, 1)[0]
    return filename

def apply_libsuffix(files, libsuffix, shared_suffix):
    """Remove the given libsuffix from files in the 'lib' directory before the shared object suffix."""
    if not libsuffix:
        return files
    updated_files = []
    for file in files:
        if "lib" in file and file.endswith(shared_suffix):
            # Remove the libsuffix before the shared object suffix
            file = file.replace(libsuffix + shared_suffix, shared_suffix)
        updated_files.append(file)
    return updated_files

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
    missing_files, extra_files = compare_manifests(generated_manifest, committed_manifest)

    missing_files = sorted(missing_files)
    extra_files = sorted(extra_files)
    
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
    
