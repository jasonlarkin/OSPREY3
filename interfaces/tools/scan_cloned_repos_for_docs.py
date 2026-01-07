#!/usr/bin/env python3
"""
Scan cloned repos under interfaces/ for documentation entry points and emit a DOC_INDEX.md.

Finds:
  - Doxygen configs: Doxyfile*, doxygen/ directories
  - Sphinx: conf.py locations
  - MkDocs: mkdocs.yml
  - Prebuilt HTML docs: doc/**/index.html, docs/**/index.html
  - README links mentioning docs sites (best-effort)

No third-party dependencies (stdlib only).
"""

from __future__ import annotations

import argparse
import os
import re
from dataclasses import dataclass
from pathlib import Path


SKIP_DIRS = {
    "tools",
    "namd_doxygen",
    "__pycache__",
}

PRUNE_DIRS_DEFAULT = {
    ".git",
    ".idea",
    ".vscode",
    "__pycache__",
    "build",
    "dist",
    "out",
    "cmake-build-debug",
    "cmake-build-release",
    "node_modules",
    "third_party",
    "third-party",
    "3rdparty",
    "vendor",
    "deps",
    "external",  # huge in some repos; docs usually elsewhere
}

DOC_LIKE_DIRS = {
    "doc",
    "docs",
    "documentation",
    "doxygen",
    "docs-source",
    "sphinx-doc",
    "api",
    "wrappers",
    "plugins",
    "contrib",
    "manual",
    "usersguide",
    "developerguide",
    "reference",
    "website",
}


@dataclass(frozen=True)
class RepoDocs:
    name: str
    path: str
    doxygen: list[str]
    sphinx: list[str]
    mkdocs: list[str]
    html_indexes: list[str]
    readme_doc_links: list[str]


def _is_repo_dir(p: Path) -> bool:
    if not p.is_dir():
        return False
    if p.name in SKIP_DIRS:
        return False
    # treat any directory with a .git folder as a repo clone
    return (p / ".git").exists()


def _rel(root: Path, p: Path) -> str:
    return str(p.relative_to(root)).replace("\\", "/")


def _glob_many(repo: Path, patterns: list[str]) -> list[Path]:
    out: list[Path] = []
    for pat in patterns:
        out.extend(repo.glob(pat))
    # stable order
    return sorted({p for p in out if p.is_file()}, key=lambda x: str(x))


def _readme_links(repo: Path, max_bytes: int = 400_000) -> list[str]:
    # Search common README names at top-level only.
    for name in ["README.md", "README.rst", "README.txt", "README"]:
        p = repo / name
        if p.exists() and p.is_file():
            data = p.read_bytes()[:max_bytes].decode("utf-8", errors="replace")
            # naive URL extraction; keep only doc-like URLs.
            urls = re.findall(r"https?://[^\s)>\"]+", data)
            docish = []
            for u in urls:
                ul = u.lower()
                if any(k in ul for k in ["readthedocs", "documentation", "docs.", "/docs", "doxygen", "manual", "user guide", "api"]):
                    docish.append(u.rstrip(".,;"))
            # dedup preserve order
            seen = set()
            out = []
            for u in docish:
                if u in seen:
                    continue
                seen.add(u)
                out.append(u)
            return out[:50]
    return []


def scan_repo(interfaces_root: Path, repo: Path) -> RepoDocs:
    return scan_repo_fast(interfaces_root, repo)


def scan_repo_fast(
    interfaces_root: Path,
    repo: Path,
    *,
    max_depth: int = 8,
    prune_dirs: set[str] | None = None,
    caps: tuple[int, int, int, int] = (60, 60, 20, 40),
) -> RepoDocs:
    """
    Fast scan: only walk "doc-like" directories and cap results.

    This avoids pathological scans in huge repos (e.g., LAMMPS) while still finding
    the primary doc build entry points.
    """
    prune = prune_dirs or PRUNE_DIRS_DEFAULT
    cap_doxy, cap_sphinx, cap_mkdocs, cap_html = caps

    doxygen_files: list[Path] = []
    sphinx_files: list[Path] = []
    mkdocs_files: list[Path] = []
    html_indexes: list[Path] = []

    # Always check top-level doc configs directly.
    for name in ["Doxyfile", "Doxyfile.in", "mkdocs.yml", "mkdocs.yaml"]:
        p = repo / name
        if p.exists() and p.is_file():
            if name.lower().startswith("doxyfile"):
                doxygen_files.append(p)
            elif name.lower().startswith("mkdocs."):
                mkdocs_files.append(p)

    def want_descend(rel_parts: tuple[str, ...]) -> bool:
        # Allow walking into doc-like directories anywhere within max_depth.
        return any(part.lower() in DOC_LIKE_DIRS for part in rel_parts)

    for root, dirs, files in os.walk(repo):
        root_p = Path(root)
        rel = root_p.relative_to(repo)
        depth = 0 if rel == Path(".") else len(rel.parts)
        if depth > max_depth:
            dirs[:] = []
            continue

        # prune heavy dirs; keep only doc-like dirs unless we're at top-level
        kept_dirs: list[str] = []
        for d in dirs:
            dl = d.lower()
            if dl in prune:
                continue
            if depth == 0:
                # at repo root, only descend into likely doc trees to stay fast
                if dl in DOC_LIKE_DIRS:
                    kept_dirs.append(d)
            else:
                # below root, only keep descending if somewhere along the path is doc-like
                rel_parts = tuple((rel / d).parts)
                if want_descend(rel_parts):
                    kept_dirs.append(d)
        dirs[:] = kept_dirs

        # Scan files in this directory.
        for f in files:
            fl = f.lower()
            fp = root_p / f

            if fl.startswith("doxyfile") or (("doxy" in fl or "doxygen" in fl) and fl.endswith((".in", ".cmakein", ".template"))):
                if len(doxygen_files) < cap_doxy:
                    doxygen_files.append(fp)
                continue

            if fl == "conf.py":
                if len(sphinx_files) < cap_sphinx:
                    sphinx_files.append(fp)
                continue

            if fl in {"mkdocs.yml", "mkdocs.yaml"}:
                if len(mkdocs_files) < cap_mkdocs:
                    mkdocs_files.append(fp)
                continue

            if fl == "index.html":
                # only count index.html if it's plausibly docs output
                rel_str = str(fp.relative_to(repo)).replace("\\", "/").lower()
                if any(seg in rel_str for seg in ["/doc/", "/docs/", "/html/"]):
                    if len(html_indexes) < cap_html:
                        html_indexes.append(fp)

        # Early exit if all caps are met.
        if (
            len(doxygen_files) >= cap_doxy
            and len(sphinx_files) >= cap_sphinx
            and len(mkdocs_files) >= cap_mkdocs
            and len(html_indexes) >= cap_html
        ):
            break

    return RepoDocs(
        name=repo.name,
        path=_rel(interfaces_root, repo),
        doxygen=[_rel(interfaces_root, p) for p in doxygen_files][:60],
        sphinx=[_rel(interfaces_root, p) for p in sphinx_files][:60],
        mkdocs=[_rel(interfaces_root, p) for p in mkdocs_files][:20],
        html_indexes=[_rel(interfaces_root, p) for p in html_indexes][:40],
        readme_doc_links=_readme_links(repo),
    )


def render_index(root: Path, repos: list[RepoDocs]) -> str:
    lines: list[str] = []
    lines.append("# Cloned repo documentation index\n")
    lines.append("Generated by `interfaces/tools/scan_cloned_repos_for_docs.py`.\n")
    lines.append("This file lists *where docs live* in each cloned repo under `interfaces/`.\n")

    for r in sorted(repos, key=lambda x: x.name.lower()):
        lines.append(f"## {r.name}\n")
        lines.append(f"- **path**: `{r.path}`")

        if r.readme_doc_links:
            lines.append("- **README doc links**:")
            for u in r.readme_doc_links:
                lines.append(f"  - `{u}`")

        if r.doxygen:
            lines.append("- **Doxygen configs**:")
            for p in r.doxygen:
                lines.append(f"  - `{p}`")

        if r.sphinx:
            lines.append("- **Sphinx configs** (`conf.py`):")
            for p in r.sphinx:
                lines.append(f"  - `{p}`")

        if r.mkdocs:
            lines.append("- **MkDocs configs**:")
            for p in r.mkdocs:
                lines.append(f"  - `{p}`")

        if r.html_indexes:
            lines.append("- **Prebuilt HTML indexes**:")
            for p in r.html_indexes:
                lines.append(f"  - `{p}`")

        if not (r.readme_doc_links or r.doxygen or r.sphinx or r.mkdocs or r.html_indexes):
            lines.append("- **docs**: (no obvious doc entry points found by heuristics)")

        lines.append("")

    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--interfaces-root",
        default="interfaces",
        help="Path to interfaces directory (default: interfaces)",
    )
    ap.add_argument(
        "--out",
        default="interfaces/DOC_INDEX.md",
        help="Output markdown path",
    )
    args = ap.parse_args()

    interfaces_root = Path(args.interfaces_root).resolve()
    out_path = Path(args.out).resolve()

    repos: list[RepoDocs] = []
    for child in interfaces_root.iterdir():
        if _is_repo_dir(child):
            repos.append(scan_repo(interfaces_root, child))

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(render_index(interfaces_root, repos), encoding="utf-8", newline="\n")
    print(f"Wrote: {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

