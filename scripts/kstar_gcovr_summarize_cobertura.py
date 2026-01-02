#!/usr/bin/env python3

import argparse
import os
import sys
import xml.etree.ElementTree as ET


def pct(x: float) -> float:
    return round(100.0 * x, 1)

def parse_condition_coverage(s: str) -> tuple[int, int] | None:
    # Example formats seen in Cobertura:
    #  - "50% (1/2)"
    #  - "100% (2/2)"
    s = s.strip()
    lpar = s.find("(")
    rpar = s.find(")")
    if lpar == -1 or rpar == -1 or rpar <= lpar + 1:
        return None
    inner = s[lpar + 1 : rpar].strip()
    if "/" not in inner:
        return None
    num_s, den_s = inner.split("/", 1)
    try:
        num = int(num_s.strip())
        den = int(den_s.strip())
    except ValueError:
        return None
    if den < 0 or num < 0:
        return None
    return (num, den)


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Summarize per-file coverage from a gcovr Cobertura XML file."
    )
    ap.add_argument("cobertura_xml", help="Path to Cobertura XML (from gcovr --xml/--xml-pretty)")
    ap.add_argument("--repo-root", default=None, help="Optional repo root for path shortening")
    ap.add_argument("--out-tsv", required=True, help="Output TSV path")
    ap.add_argument("--bottom", type=int, default=15, help="How many lowest-branch files to keep in the printed subset")
    args = ap.parse_args()

    xml_path = args.cobertura_xml
    if not os.path.isfile(xml_path):
        print(f"missing cobertura xml: {xml_path}", file=sys.stderr)
        return 2

    tree = ET.parse(xml_path)
    root = tree.getroot()

    entries = {}

    # Cobertura structure: coverage/packages/package/classes/class
    for cls in root.findall(".//class"):
        filename = cls.attrib.get("filename")
        if not filename:
            continue

        # Compute counts from <lines><line ...> entries for stability across gcovr versions.
        ln_cov = 0
        ln_val = 0
        br_cov = 0
        br_val = 0

        for line in cls.findall("./lines/line"):
            ln_val += 1
            hits_s = line.attrib.get("hits", "0")
            try:
                hits = int(hits_s)
            except ValueError:
                hits = 0
            if hits > 0:
                ln_cov += 1

            if line.attrib.get("branch", "false") == "true":
                cc = line.attrib.get("condition-coverage")
                parsed = parse_condition_coverage(cc) if cc else None
                if parsed:
                    num, den = parsed
                    br_cov += num
                    br_val += den

        ln_rate = (ln_cov / ln_val) if ln_val > 0 else 0.0
        br_rate = (br_cov / br_val) if br_val > 0 else 0.0

        prev = entries.get(filename)
        cur = {
            "file": filename,
            "branch_rate": br_rate,
            "line_rate": ln_rate,
            "branches_covered": br_cov,
            "branches_valid": br_val,
            "lines_covered": ln_cov,
            "lines_valid": ln_val,
        }

        if prev is None:
            entries[filename] = cur
        else:
            # Prefer the entry with more valid branches; otherwise keep higher branch_rate.
            if cur["branches_valid"] > prev["branches_valid"]:
                entries[filename] = cur
            elif cur["branches_valid"] == prev["branches_valid"] and cur["branch_rate"] > prev["branch_rate"]:
                entries[filename] = cur

    repo_root = args.repo_root
    if repo_root:
        repo_root = os.path.abspath(repo_root)

    def short(path: str) -> str:
        # Keep paths readable even if gcovr emits build-prefixed relative paths.
        p = os.path.abspath(path)
        if repo_root and p.startswith(repo_root + os.sep):
            return p[len(repo_root) + 1 :]
        needle = os.sep + "src" + os.sep + "main" + os.sep + "cpp" + os.sep
        idx = p.find(needle)
        if idx != -1:
            return p[idx + 1 :]
        return path

    rows = list(entries.values())
    rows.sort(key=lambda r: (r["branch_rate"], r["line_rate"], r["file"]))

    out_dir = os.path.dirname(os.path.abspath(args.out_tsv))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(args.out_tsv, "w", encoding="utf-8") as f:
        f.write("file\tbranch_pct\tbranches\tline_pct\tlines\n")
        for r in rows:
            f.write(
                f"{short(r['file'])}\t{pct(r['branch_rate'])}\t{r['branches_covered']}/{r['branches_valid']}\t"
                f"{pct(r['line_rate'])}\t{r['lines_covered']}/{r['lines_valid']}\n"
            )

    # Print the bottom subset to stdout (excluding header).
    bottom_n = max(0, args.bottom)
    if bottom_n > 0:
        print("file\tbranch_pct\tbranches\tline_pct\tlines")
        for r in rows[:bottom_n]:
            print(
                f"{short(r['file'])}\t{pct(r['branch_rate'])}\t{r['branches_covered']}/{r['branches_valid']}\t"
                f"{pct(r['line_rate'])}\t{r['lines_covered']}/{r['lines_valid']}"
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())


