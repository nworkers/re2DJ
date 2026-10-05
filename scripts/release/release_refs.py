#!/usr/bin/env python3
"""Write the "Resolved issues and commits" section of a GitHub Release body.

Usage:
  scripts/release/release_refs.py <tag> [--previous <tag>] [--repo owner/name]

Reads the commits from the previous v* tag to <tag>, the issues each one
closes (Closes/Fixes/Resolves #N in its message) and the PR it came from,
and prints a Markdown section for the Release body. The note file in
docs/release-notes/ cannot hold its own squash commit's ID, which exists only
after the merge, so release.yml appends this section at publish time.
Issue titles and PRs come from the gh CLI when it is available and
authenticated; otherwise issues are listed by number and PRs are left out.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys

CLOSING = re.compile(r"\b(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?)\s*:?\s+#(\d+)\b", re.IGNORECASE)


def git(*args: str) -> str:
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout


def gh_json(*args: str):
    if shutil.which("gh") is None:
        return None
    try:
        result = subprocess.run(["gh", *args], capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if result.returncode != 0:
        return None
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError:
        return None


def repository() -> str:
    if os.environ.get("GITHUB_REPOSITORY"):
        return os.environ["GITHUB_REPOSITORY"]
    url = git("remote", "get-url", "origin").strip()
    match = re.search(r"github\.com[:/](.+?)(?:\.git)?$", url)
    if match is None:
        sys.exit(f"cannot tell the GitHub repository from the origin URL {url!r}; pass --repo")
    return match.group(1)


def previous_tag(tag: str) -> str | None:
    try:
        return git("describe", "--tags", "--abbrev=0", "--match", "v*", f"{tag}^").strip() or None
    except subprocess.CalledProcessError:
        return None


def commits(previous: str | None, tag: str) -> list[tuple[str, str, str]]:
    span = f"{previous}..{tag}" if previous else tag
    log = git("log", "--format=%H%x1f%s%x1f%B%x1e", span)
    entries = []
    for record in log.split("\x1e"):
        record = record.strip("\n")
        if not record:
            continue
        sha, subject, body = record.split("\x1f", 2)
        entries.append((sha, subject, body))
    return entries


def cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tag")
    parser.add_argument("--previous", help="the tag to start from (default: the previous v* tag)")
    parser.add_argument("--repo", help="owner/name (default: GITHUB_REPOSITORY or the origin URL)")
    options = parser.parse_args()

    repo = options.repo or repository()
    base = f"https://github.com/{repo}"
    previous = options.previous or previous_tag(options.tag)
    issue_titles: dict[int, str | None] = {}

    rows = []
    for sha, subject, body in commits(previous, options.tag):
        numbers = sorted({int(n) for n in CLOSING.findall(body)})
        issues = []
        for number in numbers:
            if number not in issue_titles:
                data = gh_json("issue", "view", str(number), "--repo", repo, "--json", "title")
                issue_titles[number] = data.get("title") if isinstance(data, dict) else None
            title = issue_titles[number]
            link = f"[#{number}]({base}/issues/{number})"
            issues.append(f"{link} {cell(title)}" if title else link)
        pulls = gh_json("api", f"repos/{repo}/commits/{sha}/pulls")
        pr_links = []
        if isinstance(pulls, list):
            pr_links = [f"[#{p['number']}]({base}/pull/{p['number']})" for p in pulls
                        if isinstance(p, dict) and p.get("merged_at")]
        rows.append(f"| [`{sha[:7]}`]({base}/commit/{sha}) | {cell(subject)} | "
                    f"{'<br>'.join(issues) or '-'} | {', '.join(pr_links) or '-'} |")

    print("## 해결된 이슈와 커밋 / Resolved issues and commits")
    print()
    if rows:
        print("| 커밋 / Commit | 제목 / Subject | 이슈 / Issues | PR |")
        print("| --- | --- | --- | --- |")
        print("\n".join(rows))
    else:
        print("커밋 없음 / No commits")
    if not issue_titles:
        print()
        print("해결된 이슈: 없음 / Resolved issues: none")
    if previous:
        print()
        print(f"비교 / Compare: [{previous}...{options.tag}]({base}/compare/{previous}...{options.tag})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
