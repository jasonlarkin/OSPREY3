#!/usr/bin/env python3
"""
Compare two repo trees and report likely merge-clash paths.

This is content-based (sha256), not git-based, so it works even if one side isn't a git branch.

Defaults:
- left:  osprey-fork_fresh (this repo root)
- right: ../osprey-fork_modern
- include: src/, scripts/, buildSrc/ (code + tooling)
- exclude: build/, tmp/, plots/, perf_results/, .git/

Output:
- Printed report to stdout (grouped by path prefix)
"""

from __future__ import annotations

import argparse
import hashlib
from collections import defaultdict
from pathlib import Path


DEFAULT_INCLUDE = ["src", "scripts", "buildSrc"]
DEFAULT_EXCLUDE_NAMES = {
    ".git",
    "build",
    "tmp",
    "plots",
    "perf_results",
    "test-results",
    ".gradle",
}


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def iter_files(root: Path, include_roots: list[str]) -> dict[str, Path]:
    out: dict[str, Path] = {}
    for inc in include_roots:
        base = root / inc
        if not base.exists():
            continue
        for p in base.rglob("*"):
            if p.is_dir():
                # prune excluded directories by name
                if p.name in DEFAULT_EXCLUDE_NAMES:
                    # rglob doesn't support prune directly; rely on is_dir check to skip hashing
                    continue
                continue
            if any(part in DEFAULT_EXCLUDE_NAMES for part in p.parts):
                continue
            # ignore obvious binaries by extension
            if p.suffix.lower() in {".jar", ".so", ".dll", ".exe", ".class"}:
                continue
            rel = str(p.relative_to(root)).replace("\\", "/")
            out[rel] = p
    return out


def group_key(path: str) -> str:
    for prefix in [
        "src/main/cc/ConfEcalc/",
        "src/main/java/",
        "src/test/",
        "scripts/tools/",
        "scripts/",
        "buildSrc/",
        "src/",
    ]:
        if path.startswith(prefix):
            return prefix.rstrip("/")
    return "other"


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--left", default=None, help="Left repo root (default: this repo root)")
    ap.add_argument("--right", default="../osprey-fork_modern", help="Right repo root (default: ../osprey-fork_modern)")
    ap.add_argument(
        "--include",
        default=",".join(DEFAULT_INCLUDE),
        help="Comma-separated top-level dirs to compare (default: src,scripts,buildSrc)",
    )
    args = ap.parse_args(argv)

    here = Path(__file__).resolve()
    default_left = here.parents[2]
    left = Path(args.left).resolve() if args.left else default_left
    right = Path(args.right).resolve()
    include_roots = [x.strip() for x in args.include.split(",") if x.strip()]

    left_files = iter_files(left, include_roots)
    right_files = iter_files(right, include_roots)

    common = sorted(set(left_files.keys()) & set(right_files.keys()))
    only_left = sorted(set(left_files.keys()) - set(right_files.keys()))
    only_right = sorted(set(right_files.keys()) - set(left_files.keys()))

    diffs: list[str] = []
    for rel in common:
        lp = left_files[rel]
        rp = right_files[rel]
        try:
            lh = sha256_file(lp)
            rh = sha256_file(rp)
        except Exception:
            continue
        if lh != rh:
            diffs.append(rel)

    print("=== Repo clash report (content-based) ===")
    print(f"left : {left}")
    print(f"right: {right}")
    print(f"include: {', '.join(include_roots)}")
    print("")
    print(f"common paths: {len(common)}")
    print(f"content diffs: {len(diffs)}")
    print(f"only left   : {len(only_left)}")
    print(f"only right  : {len(only_right)}")
    print("")

    grouped: dict[str, list[str]] = defaultdict(list)
    for rel in diffs:
        grouped[group_key(rel)].append(rel)

    for g in sorted(grouped.keys()):
        print(f"[{g}]")
        for rel in grouped[g]:
            print(f"- {rel}")
        print("")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(__import__('sys').argv[1:]))


