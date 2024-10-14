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
        return sorted(line.strip().replace("lib64/", "lib/") for line in file if "deflate" not in line)

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

def verify_conditions(generated_manifest, options):
    """Verify if specific conditions are met based on given options."""
    errors = []

    if options.OPENEXR_INSTALL_PKG_CONFIG == 'OFF':
        pc_file = [f for f in generated_manifest if f.endswith("OpenEXR.pc")]
        if pc_file:
            errors.append(f"Error: 'OpenEXR.pc' found in the manifest.")

    if options.OPENEXR_BUILD_EXAMPLES == 'OFF':
        example_files = [f for f in generated_manifest if "share/docs/examples" in f]
        if example_files:
            errors.append(f"Error: Files in 'share/docs/examples' found: {', '.join(example_files)}")

    if options.OPENEXR_BUILD_TOOLS == 'OFF':
        example_files = [f for f in generated_manifest if "bin" in f]
        if example_files:
            errors.append(f"Error: Files in 'share/docs/examples' found: {', '.join(example_files)}")

    if options.BUILD_SHARED_LIBS:
        static_files = [f for f in generated_manifest if f.endswith(".a")]
        if static_files:
            errors.append(f"Error: Static library files (.a) found: {', '.join(static_files)}")

    return errors

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

def validate_install(generated_manifest_path, committed_manifest_path, base_path, options):
    """Main function to verify the installed files."""
    # Normalize paths
    base_path = Path(base_path)
    print(f"base_path: {base_path}")
    generated_manifest = load_manifest(generated_manifest_path)
    committed_manifest = load_manifest(committed_manifest_path)

    # Normalize paths and suffixes
    shared_suffix = get_shared_object_suffix()
    generated_manifest = [normalize_path(check_suffix(path), base_path) for path in generated_manifest]
    committed_manifest = [normalize_path(check_suffix(path), base_path) for path in committed_manifest]

    # Apply libsuffix handling
#    generated_manifest = apply_libsuffix(generated_manifest, options.libsuffix, shared_suffix)

    print("committed_manifest:")
    for l in committed_manifest:
        print(f"  {l}")
    print("generated_manifest:")
    for l in generated_manifest:
        print(f"  {l}")
    
    # Compare manifests
    missing_files, extra_files = compare_manifests(generated_manifest, committed_manifest)

    if options.OPENEXR_BUILD_PYTHON == 'OFF':
        missing_files = [line for lin in missing_files if "python/" not in line]

    # Verify additional conditions
    condition_errors = verify_conditions(generated_manifest, options)

    # Output results
    if missing_files:
        print("Error: Files missing from installation:\n  " + '\n  '.join(missing_files))
    if extra_files:
        print("Error: Unexpected files installed:\n  " + '\n  '.join(extra_files))
    
    if condition_errors:
        for error in condition_errors:
            print(error)
        sys.exit(1)

    if not missing_files and not extra_files and not condition_errors:
        print("Success: The installed files match the committed manifest.")

if __name__ == "__main__":

    print(f"validate_install: {sys.argv}")

    parser = argparse.ArgumentParser(description="Validate installed files against committed install manifest.")
    parser.add_argument("generated_manifest", help="Path to the generated install_manifest.txt")
    parser.add_argument("committed_manifest", help="Path to the committed install_manifest.txt")
    parser.add_argument("install_base_path", help="Base install path to normalize file paths")
    parser.add_argument("BUILD_SHARED_LIBS", help="Error if static library files (.a) are found")
    parser.add_argument("OPENEXR_INSTALL_PKG_CONFIG", help="Error if 'OpenEXR.pc' is found")
    parser.add_argument("OPENEXR_INSTALL_DOCS", help="Error if files in 'docs' are found")
    parser.add_argument("OPENEXR_BUILD_EXAMPLES", help="Error if files in 'share/docs/examples' are found")
    parser.add_argument("OPENEXR_BUILD_TOOLS", help="Error if files in 'bin' are found")
    parser.add_argument("OPENEXR_BUILD_PYTHON", help="Error if files in 'python' are found")

    args = parser.parse_args()

    print(f"generated_manifest={args.generated_manifest}")
    print(f"committed_manifest={args.committed_manifest}")
    print(f"install_base_path={args.install_base_path}")
    print(f"BUILD_SHARED_LIBS={args.BUILD_SHARED_LIBS}")
    print(f"OPENEXR_INSTALL_PKG_CONFIG={args.OPENEXR_INSTALL_PKG_CONFIG}")
    print(f"OPENEXR_INSTALL_DOCS={args.OPENEXR_INSTALL_DOCS}")
    print(f"OPENEXR_BUILD_EXAMPLES={args.OPENEXR_BUILD_EXAMPLES}")
    print(f"OPENEXR_BUILD_TOOLS={args.OPENEXR_BUILD_TOOLS}")
    print(f"OPENEXR_BUILD_PYTHON={args.OPENEXR_BUILD_PYTHON}")

    validate_install(args.generated_manifest, args.committed_manifest, args.install_base_path, args)
