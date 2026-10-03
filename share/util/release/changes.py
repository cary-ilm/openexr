#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.

"""Add or update a release section in CHANGES.md with merged PR entries."""

from __future__ import annotations

import json
import re
import sys
from datetime import datetime, timedelta
from subprocess import PIPE, run

from _common import (
    changes_anchor_date_slug,
    format_month_day_year,
    prev_patch_version,
    require_repo_url,
)
from _security import (
    collect_security_refs_for_prs,
    gh_security_advisories_cve_titles,
    oss_fuzz_issue_titles,
)

MERGED_PR_HEADING_RE = re.compile(
    r"^###\s+Merged Pull Requests\s*:?\s*$", re.IGNORECASE
)
MERGED_WORKFLOW_HEADING_RE = re.compile(
    r"^###\s+Merged Workflow Pull Requests\s*:?\s*$", re.IGNORECASE
)
# Recognize both the canonical "Merged Documentation Pull Requests" heading
# and the legacy "Documentation Pull Requests" heading (used in some
# existing CHANGES.md sections before this heading was standardized), so
# pre-existing sections are folded into the canonical one rather than
# preserved as an unrecognized raw subsection.
MERGED_DOCUMENTATION_HEADING_RE = re.compile(
    r"^###\s+(?:Merged\s+)?Documentation Pull Requests\s*:?\s*$", re.IGNORECASE
)
SECURITY_HEADING_RE = re.compile(r"^###\s+Security\s*:?\s*$", re.IGNORECASE)
SUBSECTION_HEADING_RE = re.compile(r"^###\s+")
NEXT_VERSION_HEADING_RE = re.compile(r"^##\s+Version\s", re.IGNORECASE)
PR_BULLET_RE = re.compile(r"^\*\s*\[(\d+)\]\(")


def gh_pr_view(pr_number: str) -> dict:
    result = run(
        [
            "gh",
            "pr",
            "view",
            pr_number,
            "--json",
            "title,author",
        ],
        stdout=PIPE,
        stderr=PIPE,
        universal_newlines=True,
        check=False,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr or "gh pr view failed\n")
        sys.exit(1)
    return json.loads(result.stdout)


def strip_security_from_heading(heading_lines: list[str]) -> list[str]:
    out: list[str] = []
    i = 0
    n = len(heading_lines)
    while i < n:
        if SECURITY_HEADING_RE.match(heading_lines[i].strip()):
            if out and not out[-1].strip():
                out.pop()
            i += 1
            while i < n and not heading_lines[i].startswith("### "):
                i += 1
            continue
        out.append(heading_lines[i])
        i += 1
    return out


def parse_pr_blocks_dict(lines, i):
    """Parse '* [1234](...)' bullet entries starting at lines[i], stopping at
    the end of the list or at the next '### ' subsection heading (whichever
    comes first), so that unrelated subsections (e.g. "Documentation Pull
    Requests") are never swept into the result."""
    out = {}
    n = len(lines)
    while i < n:
        line = lines[i]
        if SUBSECTION_HEADING_RE.match(line.strip()):
            break
        mo = PR_BULLET_RE.match(line.strip())
        if mo:
            pr = mo.group(1)
            bullet = line.strip()
            if i + 1 < n:
                nxt = lines[i + 1]
                if SUBSECTION_HEADING_RE.match(nxt.strip()) or PR_BULLET_RE.match(
                    nxt.strip()
                ):
                    out[pr] = bullet
                    i += 1
                    continue
                out[pr] = "\n".join([bullet, "  " + nxt.strip()])
                i += 2
                continue
            out[pr] = bullet
            i += 1
            continue
        i += 1
    return out, i


def capture_raw_subsection(lines, i):
    """Capture a '### ...' heading and its body verbatim, stopping at the
    next '### ' heading or the end of the list. Used to preserve subsections
    this script doesn't otherwise understand (e.g. "Documentation Pull
    Requests") unchanged, in their original position."""
    n = len(lines)
    start = i
    i += 1
    while i < n and not SUBSECTION_HEADING_RE.match(lines[i].strip()):
        i += 1
    block = lines[start:i]
    while block and not block[-1].strip():
        block.pop()
    return block, i


def parse_section(lines):
    heading = []
    i = 0
    n = len(lines)

    while i < n:
        line = lines[i]
        if MERGED_PR_HEADING_RE.match(line.strip()):
            break
        heading.append(line)
        i += 1

    merged_prs = {}
    if i < n and MERGED_PR_HEADING_RE.match(lines[i].strip()):
        i += 1
        merged_prs, i = parse_pr_blocks_dict(lines, i)

    # A "Merged Documentation Pull Requests" subsection (or the legacy
    # "Documentation Pull Requests" heading) is recognized and parsed like
    # "Merged Pull Requests"/"Merged Workflow Pull Requests", rather than
    # preserved as an opaque raw block, so new documentation-only PRs can be
    # merged into it going forward.
    merged_documentation_prs = {}
    if i < n and MERGED_DOCUMENTATION_HEADING_RE.match(lines[i].strip()):
        i += 1
        merged_documentation_prs, i = parse_pr_blocks_dict(lines, i)

    # Any other subsections between "Merged Pull Requests" and "Merged
    # Workflow Pull Requests" are preserved verbatim rather than parsed, so
    # they aren't lost or merged in.
    extra_before_workflow = []
    while (
        i < n
        and SUBSECTION_HEADING_RE.match(lines[i].strip())
        and not MERGED_WORKFLOW_HEADING_RE.match(lines[i].strip())
    ):
        block, i = capture_raw_subsection(lines, i)
        extra_before_workflow.append(block)

    merged_workflow_prs = {}
    if i < n and MERGED_WORKFLOW_HEADING_RE.match(lines[i].strip()):
        i += 1
        merged_workflow_prs, i = parse_pr_blocks_dict(lines, i)

    # Likewise, preserve any subsections that follow "Merged Workflow Pull
    # Requests".
    extra_after_workflow = []
    while i < n and SUBSECTION_HEADING_RE.match(lines[i].strip()):
        block, i = capture_raw_subsection(lines, i)
        extra_after_workflow.append(block)

    return (
        heading,
        merged_prs,
        merged_documentation_prs,
        merged_workflow_prs,
        extra_before_workflow,
        extra_after_workflow,
    )


def pr_file_paths(pr_number: str) -> list[str]:
    result = run(
        ["gh", "pr", "view", pr_number, "--json", "files", "--jq", "[.files[].path]"],
        stdout=PIPE,
        stderr=PIPE,
        universal_newlines=True,
        check=False,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr or "gh pr view failed\n")
        sys.exit(1)
    return json.loads(result.stdout or "[]")


def pr_is_workflow_only(paths: list[str]) -> bool:
    return bool(paths) and all(p.startswith(".github/workflows/") for p in paths)


def pr_is_documentation_only(paths: list[str]) -> bool:
    """True if every changed file is a Markdown file (any directory) or
    lives under the website/ directory, i.e. the PR only touches
    documentation, not code."""
    return bool(paths) and all(
        p.lower().endswith(".md") or p.startswith("website/") for p in paths
    )


def main() -> None:
    if len(sys.argv) < 3:
        print(
            "Usage: changes.py <tag> <pr-number> ...\n"
            "Example: changes.py v3.4.7 1234 1235",
            file=sys.stderr,
        )
        sys.exit(1)

    tag = sys.argv[1]
    prs = sys.argv[2:]

    with open("CHANGES.md", encoding="utf-8") as f:
        lines = f.read().splitlines()

    base_tag = tag.lstrip("v").split("-rc")[0]
    prev_tag = prev_patch_version(base_tag)

    section_re = re.compile(
        rf"^##\s+Version\s+{re.escape(base_tag)}\b", re.IGNORECASE
    )
    prev_re = (
        re.compile(rf"^##\s+Version\s+{re.escape(prev_tag)}\b", re.IGNORECASE)
        if prev_tag
        else None
    )

    section_index = None
    footer_index = None
    next_version_index = None
    for i, line in enumerate(lines):
        if section_index is None and section_re.match(line):
            section_index = i
        if prev_re is not None and footer_index is None and prev_re.match(line):
            footer_index = i
        if (
            section_index is not None
            and next_version_index is None
            and i > section_index
            and NEXT_VERSION_HEADING_RE.match(line)
        ):
            next_version_index = i

    # If there's no heading for the specific previous patch release (e.g.
    # this is an X.Y.0 release with no X.Y.-1), fall back to the next
    # "## Version" heading of any kind, so the section is bounded instead
    # of running to the end of the file.
    if section_index is not None and footer_index is None:
        footer_index = next_version_index if next_version_index is not None else len(lines)

    header_index = section_index if section_index is not None else footer_index
    if header_index is None:
        print(
            "Could not locate insertion point (no matching version section and no "
            "previous patch release heading in CHANGES.md).",
            file=sys.stderr,
        )
        sys.exit(1)

    release_date = datetime.now() + timedelta(days=2)
    date_str = format_month_day_year(release_date)

    if section_index is not None:
        (
            section_heading,
            merged_prs,
            merged_documentation_prs,
            merged_workflow_prs,
            extra_before_workflow,
            extra_after_workflow,
        ) = parse_section(lines[section_index:footer_index])
        section_heading = strip_security_from_heading(section_heading)
    else:
        section_heading = [f"## Version {base_tag} ({date_str})\n"]
        merged_prs = {}
        merged_documentation_prs = {}
        merged_workflow_prs = {}
        extra_before_workflow = []
        extra_after_workflow = []

    toc, prev_toc = None, None
    toc_re = re.compile(rf"^\*\s+\[Version\s+{re.escape(base_tag)}\]", re.IGNORECASE)
    prev_toc_re = (
        re.compile(rf"^\*\s+\[Version\s+{re.escape(prev_tag)}\]", re.IGNORECASE)
        if prev_tag
        else None
    )
    for i, line in enumerate(lines[:footer_index]):
        if toc is None and toc_re.match(line):
            toc = i
        elif prev_toc is None and prev_toc_re and prev_toc_re.match(line):
            prev_toc = i
            break

    url = require_repo_url()
    for pr_number in prs:
        info = gh_pr_view(pr_number)
        title = info.get("title") or ""
        author_info = info.get("author") or {}
        author_login = author_info.get("login") or ""
        paths = pr_file_paths(pr_number)
        is_workflow = "dependabot" in author_login or pr_is_workflow_only(paths)
        is_documentation = not is_workflow and pr_is_documentation_only(paths)
        title_one_line = " ".join(title.split())
        author_name = author_info.get("name") or ""
        if author_login and author_name:
            title_one_line += f" (by @{author_login}/{author_name})"
        elif author_login:
            title_one_line += f" (by @{author_login})"
        pr_block = f"* [{pr_number}]({url}/pull/{pr_number})\n  {title_one_line}"
        if is_workflow:
            merged_workflow_prs[pr_number] = pr_block
            print(f"workflow PR: {pr_number} {title_one_line}")
        elif is_documentation:
            merged_documentation_prs[pr_number] = pr_block
            print(f"documentation PR: {pr_number} {title_one_line}")
        else:
            merged_prs[pr_number] = pr_block
            print(f"PR: {pr_number} {title_one_line}")

    all_prs_for_security = sorted(
        set(merged_prs) | set(merged_documentation_prs) | set(merged_workflow_prs),
        key=int,
        reverse=True,
    )
    cves, oss_fuzz_issues = collect_security_refs_for_prs(all_prs_for_security)

    advisory_titles = gh_security_advisories_cve_titles()
    oss_fuzz_titles = oss_fuzz_issue_titles(list(dict.fromkeys(oss_fuzz_issues)))

    with open("CHANGES.md", "w", encoding="utf-8", newline="\n") as f:
        if toc is None:
            f.write("\n".join(lines[:prev_toc]) + "\n")
            base_tag_nonum = base_tag.replace(".", "")
            date_str_lower = changes_anchor_date_slug(release_date)
            f.write(
                f"* [Version {base_tag}](#version-{base_tag_nonum}-{date_str_lower}) "
                f"{date_str}\n"
            )
            f.write("\n".join(lines[prev_toc:header_index]) + "\n")
        else:
            f.write("\n".join(lines[:header_index]) + "\n")
        f.write("\n".join(section_heading))
        if cves or oss_fuzz_issues:
            f.write("\n\n### Security\n")
            f.write(
                "\nThis release addresses the following security "
                "vulnerabilities:\n\n"
            )
            for cve in sorted(set(cves), reverse=True):
                title = advisory_titles.get(cve, "")
                suffix = f"\n  {title}" if title else ""
                f.write(f"* [{cve}](https://www.cve.org/CVERecord?id={cve}){suffix}\n")
            for issue_id in sorted(set(oss_fuzz_issues), reverse=True):
                url_issue = f"https://issues.oss-fuzz.com/issues/{issue_id}"
                f.write(f"* OSS-Fuzz [{issue_id}]({url_issue})\n")
                short = oss_fuzz_titles.get(issue_id, "")
                if short:
                    f.write(f"  {short}\n")
        f.write("\n### Merged Pull Requests\n\n")
        for pr, value in sorted(merged_prs.items(), key=lambda kv: int(kv[0]), reverse=True):
            f.write(value + "\n")
        if merged_documentation_prs:
            f.write("\n### Merged Documentation Pull Requests\n\n")
            for pr, value in sorted(
                merged_documentation_prs.items(), key=lambda kv: int(kv[0]), reverse=True
            ):
                f.write(value + "\n")
        for block in extra_before_workflow:
            f.write("\n" + "\n".join(block) + "\n")
        f.write("\n### Merged Workflow Pull Requests\n\n")
        for pr, value in sorted(merged_workflow_prs.items(), key=lambda kv: int(kv[0]), reverse=True):
            f.write(value + "\n")
        for block in extra_after_workflow:
            f.write("\n" + "\n".join(block) + "\n")
        f.write("\n" + "\n".join(lines[footer_index:]))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)
