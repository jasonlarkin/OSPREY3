#!/usr/bin/env python3
"""
One-shot Rosetta interface acquisition + extraction.

  - RosettaCommons docs may be unreachable from WSL.
  - Manual fetching/grepping is slow.
  - ONE command that refreshes the dossier with concrete interface facts.

  1) Fetches a small, fixed set of Rosetta interface artifacts from GitHub/Raw URLs.
  2) Caches them under interfaces/rosetta/docs/autofetch/<name>/ (page.html/page.txt/links.json/summary.md).
  3) Extracts key signatures into Markdown.
  4) Rewrites interfaces/rosetta/interface_inventory.md between:
       <!-- AUTO:ROSETTA_API_START -->
       <!-- AUTO:ROSETTA_API_END -->
"""

from __future__ import annotations

import argparse
import json
import os
import re
import time
import urllib.parse
import urllib.request
from dataclasses import dataclass
from html.parser import HTMLParser
from pathlib import Path


@dataclass(frozen=True)
class Target:
    name: str
    url: str


DEFAULT_TARGETS: list[Target] = [
    # Code tree pages (helpful when the full repo is not cloned).
    Target("github_tree_protocols", "https://github.com/RosettaCommons/rosetta/tree/main/source/src/protocols"),
    Target("github_tree_core_scoring", "https://github.com/RosettaCommons/rosetta/tree/main/source/src/core/scoring"),
    Target("github_tree_core_pose", "https://github.com/RosettaCommons/rosetta/tree/main/source/src/core/pose"),
    # Raw headers: primary OO interfaces.
    Target("Mover_hh", "https://raw.githubusercontent.com/RosettaCommons/rosetta/main/source/src/protocols/moves/Mover.hh"),
    Target("ScoreFunction_hh", "https://raw.githubusercontent.com/RosettaCommons/rosetta/main/source/src/core/scoring/ScoreFunction.hh"),
    Target("Pose_hh", "https://raw.githubusercontent.com/RosettaCommons/rosetta/main/source/src/core/pose/Pose.hh"),
    Target("EnergyMethod_hh", "https://raw.githubusercontent.com/RosettaCommons/rosetta/main/source/src/core/scoring/methods/EnergyMethod.hh"),
    Target(
        "EnergyMethodCreator_hh",
        "https://raw.githubusercontent.com/RosettaCommons/rosetta/main/source/src/core/scoring/methods/EnergyMethodCreator.hh",
    ),
    Target("ScoringManager_hh", "https://raw.githubusercontent.com/RosettaCommons/rosetta/main/source/src/core/scoring/ScoringManager.hh"),
]


class _HtmlTextAndLinks(HTMLParser):
    def __init__(self, base_url: str) -> None:
        super().__init__(convert_charrefs=True)
        self._base_url = base_url
        self._title_parts: list[str] = []
        self._in_title = False
        self._in_script = False
        self._in_style = False

        self._text_parts: list[str] = []

        self._in_a = False
        self._a_href: str | None = None
        self._a_text_parts: list[str] = []
        self._links: list[dict[str, str]] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attrs_d = dict(attrs)
        if tag == "title":
            self._in_title = True
            return
        if tag == "script":
            self._in_script = True
            return
        if tag == "style":
            self._in_style = True
            return
        if tag == "a":
            href = attrs_d.get("href")
            if href:
                self._in_a = True
                self._a_href = href
                self._a_text_parts = []
            return
        if tag in {"p", "br", "li", "ul", "ol", "pre", "div", "h1", "h2", "h3", "h4"}:
            self._text_parts.append("\n")

    def handle_endtag(self, tag: str) -> None:
        if tag == "title":
            self._in_title = False
            return
        if tag == "script":
            self._in_script = False
            return
        if tag == "style":
            self._in_style = False
            return
        if tag == "a" and self._in_a:
            txt = " ".join("".join(self._a_text_parts).split()).strip()
            href = (self._a_href or "").strip()
            if href:
                self._links.append(
                    {"text": txt, "href": urllib.parse.urljoin(self._base_url, href)},
                )
            self._in_a = False
            self._a_href = None
            self._a_text_parts = []
            return
        if tag in {"p", "li", "pre", "div"}:
            self._text_parts.append("\n")

    def handle_data(self, data: str) -> None:
        if self._in_script or self._in_style:
            return
        if self._in_title:
            self._title_parts.append(data)
            return
        if data and not data.isspace():
            self._text_parts.append(data)
        if self._in_a:
            self._a_text_parts.append(data)

    def title(self) -> str:
        return " ".join("".join(self._title_parts).split()).strip()

    def text(self) -> str:
        raw = "".join(self._text_parts)
        raw = re.sub(r"[ \t\r\f\v]+", " ", raw)
        raw = re.sub(r"\n{3,}", "\n\n", raw)
        return raw.strip() + "\n"

    def links(self) -> list[dict[str, str]]:
        seen: set[str] = set()
        out: list[dict[str, str]] = []
        for l in self._links:
            href = l.get("href", "")
            if not href or href in seen:
                continue
            seen.add(href)
            out.append(l)
        return out


def _http_get(url: str, timeout_s: int) -> bytes:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "osprey-interfaces-rosetta-autofetch/1.0",
            "Accept": "text/html,application/xhtml+xml,text/plain",
        },
        method="GET",
    )
    with urllib.request.urlopen(req, timeout=timeout_s) as resp:
        return resp.read()


def _write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def _write_bytes(path: Path, b: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b)


def fetch_target(t: Target, out_dir: Path, *, timeout_s: int, skip_existing: bool) -> Path:
    """
    Returns path to page.txt
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    page_txt = out_dir / "page.txt"
    if skip_existing and page_txt.exists():
        return page_txt

    html_bytes = _http_get(t.url, timeout_s=timeout_s)
    _write_bytes(out_dir / "page.html", html_bytes)

    parser = _HtmlTextAndLinks(base_url=t.url)
    parser.feed(html_bytes.decode("utf-8", errors="replace"))
    title = parser.title() or "(no title)"
    text = parser.text()
    links = parser.links()

    _write_text(out_dir / "page.txt", text)
    _write_text(out_dir / "links.json", json.dumps(links, indent=2) + "\n")

    stamp = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    md = [
        "# Web doc snapshot",
        "",
        f"- Source: `{t.url}`",
        f"- Title: `{title}`",
        f"- Generated: `{stamp}`",
        "",
        "## Top links",
        "",
    ]
    for l in links[:80]:
        txt = l.get("text") or l.get("href") or ""
        href = l.get("href") or ""
        if href:
            md.append(f"- [{txt}]({href})")
    md.append("")
    _write_text(out_dir / "summary.md", "\n".join(md))

    return page_txt


def _extract_first(lines: list[str], pattern: str) -> str | None:
    rx = re.compile(pattern)
    for ln in lines:
        m = rx.search(ln)
        if m:
            return m.group(0).strip()
    return None


def _extract_signature_block(lines: list[str], start_pat: str, end_pat: str, max_lines: int = 12) -> str | None:
    """
    Extract a multi-line signature block starting from a line matching start_pat.
    Stops after finding end_pat or after max_lines.
    """
    start_rx = re.compile(start_pat)
    end_rx = re.compile(end_pat)
    for i, ln in enumerate(lines):
        if start_rx.search(ln):
            acc = [ln.rstrip()]
            for j in range(i + 1, min(len(lines), i + 1 + max_lines)):
                acc.append(lines[j].rstrip())
                if end_rx.search(lines[j]):
                    break
            return "\n".join([a for a in acc if a.strip()])
    return None


def _read_lines(p: Path) -> list[str]:
    return p.read_text(encoding="utf-8", errors="replace").splitlines()


def extract_rosetta_api(autofetch_root: Path) -> str:
    """
    Build a Markdown section with concrete OO signatures extracted from cached page.txt files.
    """
    def lp(name: str) -> Path:
        return autofetch_root / name / "page.txt"

    mover = _read_lines(lp("Mover_hh"))
    scorefxn = _read_lines(lp("ScoreFunction_hh"))
    pose = _read_lines(lp("Pose_hh"))
    energymethod = _read_lines(lp("EnergyMethod_hh"))
    creator = _read_lines(lp("EnergyMethodCreator_hh"))
    scoringmgr = _read_lines(lp("ScoringManager_hh"))

    out: list[str] = []
    out.append("## Auto-extracted Rosetta interfaces (from cached headers)")
    out.append("")
    out.append("Generated by `interfaces/tools/rosetta_autofetch_and_extract.py`.")
    out.append("")

    # Mover
    out.append("### `Mover` (operator)")
    out.append("")
    cls = _extract_first(mover, r"\bclass\s+Mover\b.*")
    if cls:
        out.append(f"- **base**: `{cls}`")
    for sig in [
        _extract_first(mover, r"\bclone\(\)\s+const\b.*;"),
        _extract_first(mover, r"\bfresh_instance\(\)\s+const\b.*;"),
        _extract_first(mover, r"\bget_name\(\)\s+const\b.*;"),
        _extract_first(mover, r"\bapply\(\s*pose::Pose\s*&\s*\w*\s*\)\s*;"),
    ]:
        if sig:
            out.append(f"- `{sig}`")
    out.append("")

    # ScoreFunction
    out.append("### `ScoreFunction` (objective)")
    out.append("")
    cls = _extract_first(scorefxn, r"\bclass\s+ScoreFunction\b.*")
    if cls:
        out.append(f"- **base**: `{cls}`")
    for sig in [
        _extract_first(scorefxn, r"\bscore\(\s*pose::Pose\s*&\s*\w*\s*\)\s+const\s*;"),
        _extract_first(scorefxn, r"\bset_weight\(\s*ScoreType\s+const\s*&\s*\w+,\s*Real\s+const\s+\w+\s*\)\s*;"),
        _extract_first(scorefxn, r"\bget_weight\(\s*ScoreType\s+const\s*&\s*\w+\s*\)\s+const\s*;"),
    ]:
        if sig:
            out.append(f"- `{sig}`")
    out.append("")

    # Pose
    out.append("### `Pose` (state)")
    out.append("")
    cls = _extract_first(pose, r"\bclass\s+Pose\b.*")
    if cls:
        out.append(f"- **base**: `{cls}`")
    for sig in [
        _extract_first(pose, r"\bconformation\(\)\s+const\b"),
        _extract_first(pose, r"^\s*conformation\(\)\s*$"),
        _extract_first(pose, r"\benergies\(\)\s+const\s*\{"),
        _extract_first(pose, r"^\s*energies\(\)\s*\{"),
        _extract_first(pose, r"\bset_new_conformation\(\s*conformation::ConformationCOP\s+\w+\s*\)\s*;"),
        _extract_first(pose, r"\bset_new_energies_object\(\s*scoring::EnergiesOP\s+\w+\s*\)\s*;"),
    ]:
        if sig:
            out.append(f"- `{sig}`")
    out.append("")

    # EnergyMethod
    out.append("### `EnergyMethod` (score term)")
    out.append("")
    cls = _extract_first(energymethod, r"\bclass\s+EnergyMethod\b.*")
    if cls:
        out.append(f"- **base**: `{cls}`")

    # clone is multi-line in the header; grab the block.
    clone_block = _extract_signature_block(energymethod, r"^\s*EnergyMethodOP\s*$", r"clone\(\)\s+const\s+=\s+0\s*;", max_lines=5)
    if clone_block:
        out.append("- **clone**:")
        out.append("")
        out.append("```")
        out.append(clone_block)
        out.append("```")

    finalize_block = _extract_signature_block(energymethod, r"finalize_total_energy\(", r"\)\s+const\s*;", max_lines=8)
    if finalize_block:
        out.append("- **whole-structure finalize hook**:")
        out.append("")
        out.append("```")
        out.append(finalize_block)
        out.append("```")

    for sig in [
        _extract_first(energymethod, r"\bEnergyMethodType\b"),
        _extract_first(energymethod, r"\bmethod_type\(\)\s+const\s*=\s*0\s*;"),
        _extract_first(energymethod, r"\bindicate_required_context_graphs\("),
        _extract_first(energymethod, r"\bcore::Size\s+version\(\)\s+const\s*=\s*0\s*;"),
        _extract_first(energymethod, r"\bScoreTypes\s+const\s*&\s*score_types\(\)\s+const\b"),
        _extract_first(energymethod, r"\bEnergyMethod\(\s*EnergyMethodCreatorOP\s+\w+\s*\)\s*;"),
    ]:
        if sig:
            out.append(f"- `{sig.strip()}`")
    out.append("")

    # EnergyMethodCreator
    out.append("### `EnergyMethodCreator` (term factory/registration)")
    out.append("")
    cls = _extract_first(creator, r"\bclass\s+EnergyMethodCreator\b.*")
    if cls:
        out.append(f"- **base**: `{cls}`")

    create_block = _extract_signature_block(creator, r"create_energy_method\(", r"\)\s+const\s*=\s*0\s*;", max_lines=6)
    if create_block:
        out.append("- **create method**:")
        out.append("")
        out.append("```")
        out.append(create_block)
        out.append("```")
    for sig in [
        _extract_first(creator, r"\bScoreTypes\b"),
        _extract_first(creator, r"\bscore_types_for_method\(\)\s+const\s*=\s*0\s*;"),
    ]:
        if sig:
            out.append(f"- `{sig.strip()}`")
    out.append("")

    # ScoringManager (keep minimal: mainly need to know it is the registry).
    out.append("### `ScoringManager` (registry/singleton seam)")
    out.append("")
    cls = _extract_first(scoringmgr, r"\bclass\s+ScoringManager\b.*")
    if cls:
        out.append(f"- **base**: `{cls}`")
    # Provide at least one hint of the registry mechanism.
    for sig in [
        _extract_first(scoringmgr, r"EnergyMethodCreator"),
        _extract_first(scoringmgr, r"register"),
        _extract_first(scoringmgr, r"get_instance|getInstance|instance\("),
    ]:
        if sig:
            out.append(f"- `{sig.strip()}`")
    out.append("")

    return "\n".join(out).rstrip() + "\n"


def update_file_between_markers(path: Path, start: str, end: str, new_block: str) -> None:
    txt = path.read_text(encoding="utf-8", errors="replace")
    if start not in txt or end not in txt:
        raise RuntimeError(f"Missing markers in {path}. Expected {start} and {end}.")
    pre, rest = txt.split(start, 1)
    _, post = rest.split(end, 1)
    out = pre + start + "\n\n" + new_block + "\n" + end + post
    path.write_text(out, encoding="utf-8", newline="\n")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--out-root",
        default="interfaces/rosetta/docs/autofetch",
        help="Root directory for cached artifacts",
    )
    ap.add_argument("--timeout", type=int, default=30)
    ap.add_argument("--no-fetch", action="store_true", help="Skip fetching; only extract from existing cache")
    ap.add_argument("--skip-existing", action="store_true", help="Skip fetching targets that already have page.txt")
    ap.add_argument(
        "--update-dossier",
        action="store_true",
        help="Rewrite interfaces/rosetta/interface_inventory.md between AUTO markers",
    )
    args = ap.parse_args()

    out_root = Path(args.out_root)
    out_root.mkdir(parents=True, exist_ok=True)

    if not args.no_fetch:
        for t in DEFAULT_TARGETS:
            fetch_target(
                t,
                out_dir=out_root / t.name,
                timeout_s=args.timeout,
                skip_existing=args.skip_existing,
            )

    api_md = extract_rosetta_api(out_root)
    _write_text(out_root / "EXTRACTED_API.md", api_md)
    print(f"Wrote: {out_root / 'EXTRACTED_API.md'}")

    if args.update_dossier:
        inv = Path("interfaces/rosetta/interface_inventory.md")
        update_file_between_markers(
            inv,
            "<!-- AUTO:ROSETTA_API_START -->",
            "<!-- AUTO:ROSETTA_API_END -->",
            api_md,
        )
        print(f"Updated: {inv}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

