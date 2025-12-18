#!/usr/bin/env python3
"""
Move untracked repo-root markdown files into notes/ (gitignored) **only when explicitly selected**.

Rationale:
- Some untracked *.md are key results/docs that should be committed, not hidden in a gitignored folder.
- This script is therefore opt-in: you must pass at least one --include-glob to select what to move.

Defaults:
- Only considers untracked files reported by `git status --porcelain`
- Only considers repo-root *.md
- Keeps an explicit allowlist in repo root (e.g. SIMD_REPRODUCIBILITY.md)

Usage:
  python3 scripts/tools/cleanup_untracked_notes.py --dry-run --include-glob 'DRAFT_*.md'
  python3 scripts/tools/cleanup_untracked_notes.py --apply   --include-glob 'DRAFT_*.md'
"""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path


KEEP_IN_ROOT = {
    "SIMD_REPRODUCIBILITY.md",
}


def git_untracked_files(repo_root: Path) -> list[Path]:
    p = subprocess.run(
        ["git", "status", "--porcelain"],
        cwd=str(repo_root),
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    out: list[Path] = []
    for line in p.stdout.splitlines():
        # Untracked lines start with "?? "
        if not line.startswith("?? "):
            continue
        rel = line[3:].strip()
        if not rel:
            continue
        out.append(repo_root / rel)
    return out


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true", help="Actually move files (default is dry-run).")
    ap.add_argument("--dry-run", action="store_true", help="Print actions only (default).")
    ap.add_argument(
        "--include-glob",
        action="append",
        default=[],
        help="Repo-root glob(s) to move (opt-in). Example: --include-glob 'DRAFT_*.md'",
    )
    args = ap.parse_args(argv)

    do_apply = args.apply and not args.dry_run
    repo_root = Path(__file__).resolve().parents[2]
    notes_dir = repo_root / "notes"
    notes_dir.mkdir(parents=True, exist_ok=True)

    untracked = git_untracked_files(repo_root)
    candidates: list[Path] = []

    include_globs = [g for g in args.include_glob if g and g.strip()]
    if not include_globs:
        print("No --include-glob specified; refusing to move any files.")
        print("This is intentional: many untracked *.md are key docs and should be committed instead.")
        return 0

    for p in untracked:
        if p.parent != repo_root:
            continue
        if p.suffix.lower() != ".md":
            continue
        if p.name in KEEP_IN_ROOT:
            continue
        if not any(p.match(g) for g in include_globs):
            continue
        candidates.append(p)

    candidates = sorted(candidates, key=lambda x: x.name.lower())

    if not candidates:
        print("No matching untracked repo-root markdown files to move.")
        return 0

    print("Will move these untracked repo-root markdown files into notes/:")
    for p in candidates:
        print(f"- {p.name}")

    if not do_apply:
        print("")
        print("Dry-run only. Re-run with --apply to perform moves.")
        return 0

    for src in candidates:
        dst = notes_dir / src.name
        # Avoid overwrite; add a numeric suffix if needed
        if dst.exists():
            stem = src.stem
            suf = src.suffix
            i = 1
            while True:
                dst2 = notes_dir / f"{stem}.{i}{suf}"
                if not dst2.exists():
                    dst = dst2
                    break
                i += 1
        os.replace(src, dst)
        print(f"moved: {src.name} -> notes/{dst.name}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(os.sys.argv[1:]))


