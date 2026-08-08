#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.

"""Create a draft GitHub release for a tagged release."""

from __future__ import annotations

import re
import sys
from subprocess import run

from _common import load_release_notes

# Matches a Markdown list-item marker: optional leading whitespace (to allow
# for nested/indented items), then a single "-", "*", or "+" bullet
# character, then at least one space before the item's text.
_LIST_ITEM_RE = re.compile(r"^\s*[-*+]\s+")


def unwrap_paragraphs(text: str) -> str:
    """
    Join line-wrapped text within each paragraph and list item into a single
    line, since GitHub renders every newline in release notes as a hard line
    break rather than reflowing them like Markdown normally would. Blank
    lines (paragraph/item separators) are preserved.
    """
    unwrapped_lines: list[str] = []
    for line in text.splitlines():
        if not line.strip():
            unwrapped_lines.append("")
        elif _LIST_ITEM_RE.match(line) or not unwrapped_lines or not unwrapped_lines[-1]:
            unwrapped_lines.append(line.rstrip())
        else:
            unwrapped_lines[-1] = f"{unwrapped_lines[-1]} {line.strip()}"
    return "\n".join(unwrapped_lines)


def create_draft_release(tag: str, release_notes: str) -> None:
    release_tag = tag.split("-rc")[0]
    run(
        ["gh", "release", "create", tag, "--draft", "--title", release_tag, "-F", "-"],
        input=release_notes,
        text=True,
        check=True,
    )


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: draft.py <tag>   e.g. draft.py v3.4.7", file=sys.stderr)
        sys.exit(1)
    tag = sys.argv[1]
    _release_date, release_notes, _release_version = load_release_notes(tag)
    release_notes = unwrap_paragraphs(release_notes)
    create_draft_release(tag, release_notes)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)

