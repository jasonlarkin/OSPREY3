#!/usr/bin/env python3
"""
Convert a PDF into:
  - extracted text (best-effort)
  - page images (optional, best-effort)

This is meant to turn vendor manuals (e.g., Amber24.pdf) into locally-searchable artifacts.

Strategy:
  1) Prefer poppler utils if present:
     - pdftotext  -> out.txt
     - pdftoppm   -> images/page-0001.png ...
  2) Fallback to PyPDF2 text extraction if installed.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def _run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def _has(cmd: str) -> bool:
    return shutil.which(cmd) is not None


def _poppler_text(pdf: Path, out_txt: Path) -> bool:
    if not _has("pdftotext"):
        return False
    out_txt.parent.mkdir(parents=True, exist_ok=True)
    # -layout preserves columns somewhat; still imperfect.
    _run(["pdftotext", "-layout", str(pdf), str(out_txt)])
    return True


def _poppler_images(pdf: Path, out_dir: Path, dpi: int) -> bool:
    if not _has("pdftoppm"):
        return False
    out_dir.mkdir(parents=True, exist_ok=True)
    # Produces page-0001.png, page-0002.png, ...
    prefix = out_dir / "page"
    _run(["pdftoppm", "-png", "-r", str(dpi), str(pdf), str(prefix)])
    return True


def _pypdf2_text(pdf: Path, out_txt: Path) -> bool:
    try:
        import PyPDF2  # type: ignore
    except Exception:
        return False

    out_txt.parent.mkdir(parents=True, exist_ok=True)
    reader = PyPDF2.PdfReader(str(pdf))
    parts: list[str] = []
    for i, page in enumerate(reader.pages):
        try:
            t = page.extract_text() or ""
        except Exception as e:
            t = f"\n[PyPDF2 extract_text failed on page {i}: {e}]\n"
        parts.append(t)
    out_txt.write_text("\n\n".join(parts) + "\n", encoding="utf-8")
    return True


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pdf", help="Path to PDF (e.g., Amber24.pdf)")
    ap.add_argument("--out-dir", required=True, help="Output directory")
    ap.add_argument("--text", action="store_true", help="Extract text to out.txt")
    ap.add_argument("--images", action="store_true", help="Render page images")
    ap.add_argument("--dpi", type=int, default=150, help="DPI for images (pdftoppm)")
    args = ap.parse_args(argv)

    pdf = Path(args.pdf)
    if not pdf.exists():
        raise FileNotFoundError(str(pdf))

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    did_any = False

    if args.text:
        out_txt = out_dir / "out.txt"
        if _poppler_text(pdf, out_txt):
            print(f"Wrote: {out_txt} (pdftotext)")
            did_any = True
        elif _pypdf2_text(pdf, out_txt):
            print(f"Wrote: {out_txt} (PyPDF2 fallback)")
            did_any = True
        else:
            print("No text extractor available. Install poppler-utils (pdftotext) or PyPDF2.", file=sys.stderr)

    if args.images:
        img_dir = out_dir / "images"
        if _poppler_images(pdf, img_dir, dpi=args.dpi):
            print(f"Wrote images under: {img_dir} (pdftoppm)")
            did_any = True
        else:
            print("No image renderer available. Install poppler-utils (pdftoppm).", file=sys.stderr)

    if not did_any:
        return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

