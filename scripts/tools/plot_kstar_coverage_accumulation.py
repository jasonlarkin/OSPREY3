#!/usr/bin/env python3
"""
Plot how coverage accumulates across incremental test-category steps.

Input is a manifest JSON produced by:
  scripts/kstar_coverage_accumulation.sh

The manifest points at LCOV tracefiles (.info). This script parses those tracefiles
and extracts totals for:
  - lines (LH/LF)
  - functions (FNH/FNF)
  - branches (BRH/BRF)

If matplotlib is installed, it writes a PNG (and optionally PDF).
It always prints a small table to stdout, and can write a CSV.
"""

from __future__ import annotations

import argparse
import csv
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple


@dataclass(frozen=True)
class Totals:
    lines_hit: int
    lines_total: int
    funcs_hit: int
    funcs_total: int
    branches_hit: int
    branches_total: int

    def pct(self) -> Dict[str, float]:
        def safe(hit: int, total: int) -> float:
            return (100.0 * hit / total) if total else 0.0

        return {
            "lines_pct": safe(self.lines_hit, self.lines_total),
            "funcs_pct": safe(self.funcs_hit, self.funcs_total),
            "branches_pct": safe(self.branches_hit, self.branches_total),
        }


def parse_lcov_info(path: Path) -> Totals:
    """
    Parse totals from an lcov tracefile.

    Prefer per-record summary lines (LF/LH/FNF/FNH/BRF/BRH) if present.
    Fall back to counting DA/FNDA/BRDA entries if needed.
    """

    # Global sums (across SF records).
    lh = lf = fnh = fnf = brh = brf = 0

    # Per-record fallbacks.
    rec_da_total = rec_da_hit = 0
    rec_fnda_total = rec_fnda_hit = 0
    rec_brda_total = rec_brda_hit = 0
    seen_record_summaries = False

    def flush_record_fallback() -> None:
        nonlocal lh, lf, fnh, fnf, brh, brf
        nonlocal rec_da_total, rec_da_hit, rec_fnda_total, rec_fnda_hit, rec_brda_total, rec_brda_hit

        if rec_da_total or rec_fnda_total or rec_brda_total:
            lf += rec_da_total
            lh += rec_da_hit
            fnf += rec_fnda_total
            fnh += rec_fnda_hit
            brf += rec_brda_total
            brh += rec_brda_hit

        rec_da_total = rec_da_hit = 0
        rec_fnda_total = rec_fnda_hit = 0
        rec_brda_total = rec_brda_hit = 0

    with path.open("r", encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\n")

            if line == "end_of_record":
                if not seen_record_summaries:
                    flush_record_fallback()
                else:
                    # even if we saw record summaries, clear fallbacks
                    rec_da_total = rec_da_hit = 0
                    rec_fnda_total = rec_fnda_hit = 0
                    rec_brda_total = rec_brda_hit = 0
                continue

            if line.startswith("LF:"):
                seen_record_summaries = True
                lf += int(line[3:] or "0")
                continue
            if line.startswith("LH:"):
                seen_record_summaries = True
                lh += int(line[3:] or "0")
                continue
            if line.startswith("FNF:"):
                seen_record_summaries = True
                fnf += int(line[4:] or "0")
                continue
            if line.startswith("FNH:"):
                seen_record_summaries = True
                fnh += int(line[4:] or "0")
                continue
            if line.startswith("BRF:"):
                seen_record_summaries = True
                brf += int(line[4:] or "0")
                continue
            if line.startswith("BRH:"):
                seen_record_summaries = True
                brh += int(line[4:] or "0")
                continue

            # Fallback per-record counting:
            if line.startswith("DA:"):
                # DA:<line>,<count>[,<checksum>]
                rec_da_total += 1
                try:
                    count_part = line.split(",", 2)[1]
                    if int(count_part) > 0:
                        rec_da_hit += 1
                except Exception:
                    # ignore malformed
                    pass
                continue

            if line.startswith("FNDA:"):
                # FNDA:<count>,<function name>
                rec_fnda_total += 1
                try:
                    count_part = line.split(",", 2)[0][5:]
                    if int(count_part) > 0:
                        rec_fnda_hit += 1
                except Exception:
                    pass
                continue

            if line.startswith("BRDA:"):
                # BRDA:<line>,<block>,<branch>,<taken>
                rec_brda_total += 1
                try:
                    taken = line.split(",", 4)[4]
                    if taken != "-" and int(taken) > 0:
                        rec_brda_hit += 1
                except Exception:
                    pass
                continue

    return Totals(
        lines_hit=lh,
        lines_total=lf,
        funcs_hit=fnh,
        funcs_total=fnf,
        branches_hit=brh,
        branches_total=brf,
    )


def load_manifest(path: Path) -> Dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def try_import_matplotlib():
    try:
        import matplotlib.pyplot as plt  # type: ignore
        return plt
    except Exception:
        return None


def main() -> int:
    ap = argparse.ArgumentParser(description="Plot cumulative coverage vs incremental test-category steps.")
    ap.add_argument("manifest_json", type=Path, help="Manifest JSON produced by scripts/kstar_coverage_accumulation.sh")
    ap.add_argument("--out-png", type=Path, default=None, help="Output PNG path (default: alongside manifest)")
    ap.add_argument("--out-pdf", type=Path, default=None, help="Optional output PDF path")
    ap.add_argument("--out-csv", type=Path, default=None, help="Optional output CSV of totals/pcts")
    ap.add_argument("--title", type=str, default=None, help="Plot title override")
    ap.add_argument("--show", action="store_true", help="Show interactive plot window (if available)")
    args = ap.parse_args()

    manifest = load_manifest(args.manifest_json)
    steps = manifest.get("steps", [])
    if not steps:
        raise SystemExit("manifest has no steps")

    rows: List[Tuple[int, str, Optional[str], Path, Totals]] = []
    for step in steps:
        idx = int(step["index"])
        label = str(step["label"])
        ctest_label = step.get("ctest_label_regex", None)
        info_path = Path(step["info_path"])
        if not info_path.exists():
            raise SystemExit(f"missing tracefile: {info_path}")
        totals = parse_lcov_info(info_path)
        rows.append((idx, label, ctest_label, info_path, totals))

    rows.sort(key=lambda r: r[0])

    # Print a concise table.
    print(f"manifest: {args.manifest_json}")
    print(f"mixture:  {manifest.get('mixture')}")
    print("")
    header = ["idx", "label", "lines", "funcs", "branches"]
    print("{:>3}  {:<28}  {:>14}  {:>14}  {:>16}".format(*header))
    for idx, label, _ct, _info, t in rows:
        p = t.pct()
        lines_s = f"{p['lines_pct']:.2f}% ({t.lines_hit}/{t.lines_total})"
        funcs_s = f"{p['funcs_pct']:.2f}% ({t.funcs_hit}/{t.funcs_total})"
        br_s = f"{p['branches_pct']:.2f}% ({t.branches_hit}/{t.branches_total})"
        short = label if len(label) <= 28 else (label[:25] + "...")
        print(f"{idx:>3}  {short:<28}  {lines_s:>14}  {funcs_s:>14}  {br_s:>16}")

    # Optional CSV.
    if args.out_csv:
        with args.out_csv.open("w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(
                [
                    "index",
                    "label",
                    "info_path",
                    "lines_hit",
                    "lines_total",
                    "lines_pct",
                    "funcs_hit",
                    "funcs_total",
                    "funcs_pct",
                    "branches_hit",
                    "branches_total",
                    "branches_pct",
                ]
            )
            for idx, label, _ct, info, t in rows:
                p = t.pct()
                w.writerow(
                    [
                        idx,
                        label,
                        str(info),
                        t.lines_hit,
                        t.lines_total,
                        p["lines_pct"],
                        t.funcs_hit,
                        t.funcs_total,
                        p["funcs_pct"],
                        t.branches_hit,
                        t.branches_total,
                        p["branches_pct"],
                    ]
                )
        print(f"\nwrote csv: {args.out_csv}")

    plt = try_import_matplotlib()
    if plt is None:
        print("\nmatplotlib not available; skipping plot render. Install with: pip install matplotlib")
        return 0

    xs = [idx for idx, *_ in rows]
    labels = [label for _, label, *_ in rows]
    lines_pct = [t.pct()["lines_pct"] for *_, t in rows]
    funcs_pct = [t.pct()["funcs_pct"] for *_, t in rows]
    branches_pct = [t.pct()["branches_pct"] for *_, t in rows]

    out_png = args.out_png or (args.manifest_json.parent / f"coverage_accumulation.{manifest.get('mixture','prod')}.png")

    fig, ax = plt.subplots(figsize=(12.5, 6.5))
    ax.plot(xs, lines_pct, marker="o", label="Lines")
    ax.plot(xs, funcs_pct, marker="o", label="Functions")
    ax.plot(xs, branches_pct, marker="o", label="Branches")

    title = args.title or f"Coverage accumulation ({manifest.get('mixture','prod')})"
    ax.set_title(title)
    ax.set_xlabel("Incremental test-category steps (cumulative)")
    ax.set_ylabel("Coverage (%)")
    ax.grid(True, alpha=0.25)
    ax.legend(loc="lower right")

    # Tick labels: show which label was added at each step (baseline gets its own marker).
    ax.set_xticks(xs)
    ax.set_xticklabels(labels, rotation=35, ha="right")

    # Light annotation: show the last-point percentages near the end.
    ax.annotate(f"{lines_pct[-1]:.2f}%", (xs[-1], lines_pct[-1]), textcoords="offset points", xytext=(6, 6))
    ax.annotate(f"{funcs_pct[-1]:.2f}%", (xs[-1], funcs_pct[-1]), textcoords="offset points", xytext=(6, 6))
    ax.annotate(f"{branches_pct[-1]:.2f}%", (xs[-1], branches_pct[-1]), textcoords="offset points", xytext=(6, 6))

    fig.tight_layout()
    fig.savefig(out_png, dpi=160)
    print(f"\nwrote png: {out_png}")

    if args.out_pdf:
        fig.savefig(args.out_pdf)
        print(f"wrote pdf: {args.out_pdf}")

    if args.show:
        plt.show()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

