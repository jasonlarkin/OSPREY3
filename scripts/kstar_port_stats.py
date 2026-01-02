#!/usr/bin/env python3

"""
Summarize Java->C++ K* port progress.

Goals:
- Count VERBATIM vs SYNTHESIZED tests from `src/main/cpp/kstar/TEST_PORTING_STATUS.md`
- Estimate "tests to port" targets from `src/main/cpp/kstar/OSPREY_TESTS_TO_PORT.md`
- Report basic LOC for `src/main/cpp/kstar` and `src/test/cpp/kstar`

This script intentionally uses simple parsing heuristics (regex + line scanning)
because the markdown is maintained by humans. If the docs change, update the
heuristics here accordingly.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Iterable


DEFAULT_STATUS_MD = "src/main/cpp/kstar/TEST_PORTING_STATUS.md"
DEFAULT_TARGETS_MD = "src/main/cpp/kstar/OSPREY_TESTS_TO_PORT.md"
DEFAULT_MAIN_CPP_DIR = "src/main/cpp/kstar"
DEFAULT_TEST_CPP_DIR = "src/test/cpp/kstar"


@dataclass(frozen=True)
class LocStats:
    files: int
    total_lines: int
    nonempty_lines: int


@dataclass(frozen=True)
class TestCounts:
    total: int
    pass_: int
    skip: int
    pending: int
    removed: int
    other: int


@dataclass(frozen=True)
class CategorizedTestCounts:
    verbatim: TestCounts
    synthesized: TestCounts
    unknown: TestCounts


@dataclass(frozen=True)
class TargetCounts:
    # These are "best-effort estimates" from OSPREY_TESTS_TO_PORT.md.
    total_estimated: int
    astar_node: int
    conf_index: int
    conf_ranker: int
    conf_search_cache: int
    sma_star: int
    sequence_pruner: int
    simple_partition_function: int
    kstar_score: int
    kstar: int
    bbkstar: int
    mskstar: int


@dataclass(frozen=True)
class Report:
    repo_root: str
    status_md: str
    targets_md: str
    loc_main_cpp_kstar: LocStats
    loc_test_cpp_kstar: LocStats
    tests: CategorizedTestCounts
    targets: TargetCounts


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _walk_files(root: Path, exts: set[str]) -> list[Path]:
    out: list[Path] = []
    if not root.exists():
        return out
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        if p.suffix.lower() in exts:
            out.append(p)
    return out


def _count_loc(paths: Iterable[Path]) -> LocStats:
    files = 0
    total_lines = 0
    nonempty_lines = 0
    for p in paths:
        try:
            txt = _read_text(p)
        except (OSError, UnicodeDecodeError):
            # Skip unreadable files; this is a reporting tool, not a build step.
            continue
        files += 1
        lines = txt.splitlines()
        total_lines += len(lines)
        nonempty_lines += sum(1 for ln in lines if ln.strip() != "")
    return LocStats(files=files, total_lines=total_lines, nonempty_lines=nonempty_lines)


_STATUS_RE = re.compile(r"^\s*-\s*\[(?P<status>[A-Z_]+)\]\s*(?P<rest>.*)$")
_ALLOWED_TEST_STATUSES = {
    # "real test-case" statuses used in TEST_PORTING_STATUS.md
    "PASS",
    "SKIP",
    "PENDING",
    "IN_PROGRESS",
    "REMOVED",
    "DONE",
}


def _bucket_from_line(rest: str) -> str:
    """
    Decide whether a markdown bullet describes a VERBATIM or SYNTHESIZED test.
    Heuristics:
    - Explicit markers win.
    - Otherwise, *_VERBATIM.* / *.VERBATIM.* implies verbatim.
    - Otherwise unknown.
    """
    upper = rest.upper()
    if "SYNTHESIZED" in upper:
        return "synthesized"
    if "VERBATIM" in upper:
        return "verbatim"
    if "_VERBATIM" in rest or ".VERBATIM" in rest:
        return "verbatim"
    return "unknown"


def _normalize_status(s: str) -> str:
    # Keep these aligned with how we write the docs.
    if s == "PASS":
        return "pass"
    if s == "SKIP":
        return "skip"
    if s == "PENDING":
        return "pending"
    if s == "IN_PROGRESS":
        return "pending"
    if s == "REMOVED":
        return "removed"
    if s == "DONE":
        return "pass"
    return "other"


def _empty_counts() -> TestCounts:
    return TestCounts(total=0, pass_=0, skip=0, pending=0, removed=0, other=0)


def _add_count(c: TestCounts, status: str) -> TestCounts:
    total = c.total + 1
    pass_ = c.pass_ + (1 if status == "pass" else 0)
    skip = c.skip + (1 if status == "skip" else 0)
    pending = c.pending + (1 if status == "pending" else 0)
    removed = c.removed + (1 if status == "removed" else 0)
    other = c.other + (1 if status == "other" else 0)
    return TestCounts(total=total, pass_=pass_, skip=skip, pending=pending, removed=removed, other=other)


def parse_test_porting_status(md_text: str) -> CategorizedTestCounts:
    verbatim = _empty_counts()
    synthesized = _empty_counts()
    unknown = _empty_counts()

    # Track context from headings like:
    # "### A* Node Tests (7/7 VERBATIM from ...)"
    # so that bullets under that section inherit the classification even if the
    # individual bullet line doesn't contain "VERBATIM"/"SYNTHESIZED".
    context_bucket: str | None = None

    for line in md_text.splitlines():
        # Update context on headings.
        stripped = line.strip()
        if stripped.startswith("#"):
            up = stripped.upper()
            if "SYNTHESIZED" in up:
                context_bucket = "synthesized"
            elif "VERBATIM" in up:
                context_bucket = "verbatim"
            else:
                # Leave context as-is; many headings are neutral.
                pass

        m = _STATUS_RE.match(line)
        if not m:
            continue
        status_raw = m.group("status").strip()
        rest = m.group("rest").strip()

        # Only count bullets that look like individual test-case entries.
        # This intentionally excludes non-test checklist items like:
        # - [AVAIL] energy matrix files
        # - [IN PROGRESS] suite summaries
        # - [DONE] suite headings (unless they name a test explicitly)
        if status_raw not in _ALLOWED_TEST_STATUSES:
            continue
        if "`" not in rest:
            continue

        bucket = _bucket_from_line(rest)
        if bucket == "unknown" and context_bucket is not None:
            bucket = context_bucket
        status = _normalize_status(status_raw)

        if bucket == "verbatim":
            verbatim = _add_count(verbatim, status)
        elif bucket == "synthesized":
            synthesized = _add_count(synthesized, status)
        else:
            unknown = _add_count(unknown, status)

    return CategorizedTestCounts(verbatim=verbatim, synthesized=synthesized, unknown=unknown)


def _find_int_after(pattern: str, text: str, default: int = 0) -> int:
    m = re.search(pattern, text, flags=re.IGNORECASE | re.MULTILINE)
    if not m:
        return default
    try:
        return int(m.group(1))
    except ValueError:
        return default


def parse_targets(md_text: str) -> TargetCounts:
    # Prefer explicit "(N tests)" header annotations when available.
    astar_node = _find_int_after(r"TestLinkedConfAStarNode\.java\s*\(\s*(\d+)\s*tests", md_text, default=0)
    conf_index = _find_int_after(r"TestConfIndex\.java\s*\(\s*(\d+)\s*tests", md_text, default=0)
    conf_ranker = _find_int_after(r"TestConfRanker\.java\s*\(\s*(\d+)\s*tests", md_text, default=0)
    conf_search_cache = _find_int_after(r"TestConfSearchCache\.java\s*\(\s*(\d+)\s*tests", md_text, default=0)
    sma_star = _find_int_after(r"TestSMAStar\.java\s*\(\s*(\d+)\s*tests", md_text, default=0)
    sequence_pruner = _find_int_after(r"TestSequencePruner\.java\s*\(\s*(\d+)\s*test", md_text, default=0)
    kstar_score = _find_int_after(r"TestKStarScore\.java\s*\(\s*(\d+)\s*\+?\s*tests", md_text, default=0)

    # Partition function: infer from the listed variant counts if present.
    # In our checklist doc we list:
    # - 2RL0 Protein (8)
    # - 2RL0 Ligand (8)
    # - 2RL0 Complex (10)
    # - 1GUA11 Protein (2)
    # - 1GUA11 Ligand (2)
    # - 1GUA11 Complex (2)
    # - No Positions Protein (8)
    # Total = 40.
    pf_counts = []
    for key in [
        r"2RL0 Protein\s*\(\s*(\d+)\s*tests\)",
        r"2RL0 Ligand\s*\(\s*(\d+)\s*tests\)",
        r"2RL0 Complex\s*\(\s*(\d+)\s*tests\)",
        r"1GUA11 Protein\s*\(\s*(\d+)\s*tests\)",
        r"1GUA11 Ligand\s*\(\s*(\d+)\s*tests\)",
        r"1GUA11 Complex\s*\(\s*(\d+)\s*tests\)",
        r"No Positions Protein\s*\(\s*(\d+)\s*tests\)",
    ]:
        v = _find_int_after(key, md_text, default=0)
        if v:
            pf_counts.append(v)
    simple_partition_function = sum(pf_counts) if pf_counts else 0

    # K* suites are approximate in the checklist; count the explicit "(N tests)" if present.
    kstar = _find_int_after(r"TestKStar\.java\s*\(\s*(\d+)\s*\+?\s*tests\)", md_text, default=0)
    bbkstar = _find_int_after(r"TestBBKStar\.java\s*\(\s*(\d+)\s*\+?\s*tests\)", md_text, default=0)
    mskstar = _find_int_after(r"TestMSKStar\.java\s*\(\s*(\d+)\s*tests\)", md_text, default=0)

    total_estimated = (
        astar_node
        + conf_index
        + conf_ranker
        + conf_search_cache
        + sma_star
        + sequence_pruner
        + simple_partition_function
        + kstar_score
        + kstar
        + bbkstar
        + mskstar
    )

    return TargetCounts(
        total_estimated=total_estimated,
        astar_node=astar_node,
        conf_index=conf_index,
        conf_ranker=conf_ranker,
        conf_search_cache=conf_search_cache,
        sma_star=sma_star,
        sequence_pruner=sequence_pruner,
        simple_partition_function=simple_partition_function,
        kstar_score=kstar_score,
        kstar=kstar,
        bbkstar=bbkstar,
        mskstar=mskstar,
    )


def _find_repo_root(start: Path) -> Path:
    cur = start.resolve()
    for _ in range(20):
        if (cur / ".git").exists():
            return cur
        if cur.parent == cur:
            break
        cur = cur.parent
    return start.resolve()


def _print_human(report: Report) -> None:
    t = report.tests
    trg = report.targets

    def fmt_counts(name: str, c: TestCounts) -> str:
        return (
            f"{name}: total={c.total} pass={c.pass_} skip={c.skip} pending={c.pending} removed={c.removed} other={c.other}"
        )

    print("kstar_port_stats")
    print(f"repo_root: {report.repo_root}")
    print(f"status_md: {report.status_md}")
    print(f"targets_md: {report.targets_md}")
    print("")
    print("LOC (C++ kstar)")
    print(
        f"  main: files={report.loc_main_cpp_kstar.files} lines={report.loc_main_cpp_kstar.total_lines} nonempty={report.loc_main_cpp_kstar.nonempty_lines}"
    )
    print(
        f"  test: files={report.loc_test_cpp_kstar.files} lines={report.loc_test_cpp_kstar.total_lines} nonempty={report.loc_test_cpp_kstar.nonempty_lines}"
    )
    print("")
    print("Ported tests (from TEST_PORTING_STATUS.md)")
    print(f"  {fmt_counts('VERBATIM', t.verbatim)}")
    print(f"  {fmt_counts('SYNTHESIZED', t.synthesized)}")
    print(f"  {fmt_counts('UNKNOWN', t.unknown)}")
    print("")
    print("Targets (estimated from OSPREY_TESTS_TO_PORT.md)")
    print(f"  total_estimated={trg.total_estimated}")
    print(
        "  "
        + ", ".join(
            [
                f"astar_node={trg.astar_node}",
                f"conf_index={trg.conf_index}",
                f"conf_ranker={trg.conf_ranker}",
                f"conf_search_cache={trg.conf_search_cache}",
                f"sma_star={trg.sma_star}",
                f"sequence_pruner={trg.sequence_pruner}",
                f"simple_partition_function={trg.simple_partition_function}",
                f"kstar_score={trg.kstar_score}",
                f"kstar={trg.kstar}",
                f"bbkstar={trg.bbkstar}",
                f"mskstar={trg.mskstar}",
            ]
        )
    )

    if trg.total_estimated > 0:
        pct = round(100.0 * t.verbatim.pass_ / trg.total_estimated, 1)
        print("")
        print(f"VERBATIM pass fraction (vs estimated targets): {t.verbatim.pass_}/{trg.total_estimated} = {pct}%")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Summarize Java->C++ K* port progress")
    ap.add_argument("--repo-root", default=None, help="Repo root (defaults to auto-detected .git ancestor)")
    ap.add_argument("--status-md", default=DEFAULT_STATUS_MD, help="Path to TEST_PORTING_STATUS.md (relative to repo root)")
    ap.add_argument("--targets-md", default=DEFAULT_TARGETS_MD, help="Path to OSPREY_TESTS_TO_PORT.md (relative to repo root)")
    ap.add_argument("--out-json", default=None, help="Write full report JSON to this path")
    ap.add_argument("--json", action="store_true", help="Print report JSON to stdout")
    args = ap.parse_args(argv)

    here = Path.cwd()
    repo_root = Path(args.repo_root).resolve() if args.repo_root else _find_repo_root(here)

    status_md = (repo_root / args.status_md).resolve()
    targets_md = (repo_root / args.targets_md).resolve()

    if not status_md.is_file():
        print(f"missing status md: {status_md}", file=sys.stderr)
        return 2
    if not targets_md.is_file():
        print(f"missing targets md: {targets_md}", file=sys.stderr)
        return 2

    main_cpp_dir = repo_root / DEFAULT_MAIN_CPP_DIR
    test_cpp_dir = repo_root / DEFAULT_TEST_CPP_DIR

    code_exts = {".hpp", ".h", ".cpp", ".cc", ".cxx"}
    main_files = _walk_files(main_cpp_dir, code_exts)
    test_files = _walk_files(test_cpp_dir, code_exts)

    report = Report(
        repo_root=str(repo_root),
        status_md=str(status_md.relative_to(repo_root)),
        targets_md=str(targets_md.relative_to(repo_root)),
        loc_main_cpp_kstar=_count_loc(main_files),
        loc_test_cpp_kstar=_count_loc(test_files),
        tests=parse_test_porting_status(_read_text(status_md)),
        targets=parse_targets(_read_text(targets_md)),
    )

    if args.out_json:
        out_path = Path(args.out_json)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(asdict(report), indent=2) + "\n", encoding="utf-8")

    if args.json:
        print(json.dumps(asdict(report), indent=2))
    else:
        _print_human(report)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))


