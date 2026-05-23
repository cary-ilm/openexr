#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.
#
# Shallow-clone an external repository at a pinned tag or commit.
# Usage: clone_external_repo.sh <repo-url> <dest-dir> <ref>

set -euo pipefail

url="${1:?repo url required}"
dest="${2:?dest dir required}"
ref="${3:?git ref required}"

# Allow release tags (v1.2.3), numeric tags (0.22.0), or full commit SHAs.
if [[ ! "${ref}" =~ ^(v[0-9][0-9A-Za-z._-]*|[0-9][0-9A-Za-z._-]*|[0-9a-f]{40})$ ]]; then
    echo "error: invalid git ref: ${ref}" >&2
    exit 1
fi

export GIT_TERMINAL_PROMPT=0

# Shallow clone
# Suppresses the “You are in 'detached HEAD' state…” warning
    
rm -rf "${dest}"
if [[ "${ref}" =~ ^[0-9a-f]{40}$ ]]; then
    # SHA	
    git -c advice.detachedHead=false clone --depth 1 "${url}" "${dest}"
    git -C "${dest}" checkout --detach "${ref}"
else
    # Tag or branch
    git -c advice.detachedHead=false clone --depth 1 --branch "${ref}" --single-branch "${url}" "${dest}"
fi
