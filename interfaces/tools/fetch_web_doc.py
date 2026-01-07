#!/usr/bin/env python3
"""
Fetch a documentation URL, cache raw HTML, and extract a text+links summary.

Use cases:
  - CHARMM docs landing pages
  - Rosetta docs pages
  - Quantum ESPRESSO docs pages
  - Any other HTML doc where you want a locally greppable text snapshot

Outputs (in --out-dir):
  - page.html              (raw)
  - page.txt               (extracted visible-ish text)
  - links.json             (all discovered links)
  - summary.md             (short report: title + top links)

No third-party dependencies (stdlib only).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request
from dataclasses import dataclass
from html.parser import HTMLParser


@dataclass(frozen=True)
class Link:
    text: str
    href: str


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
        self._links: list[Link] = []

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

        # Add newlines around block-ish tags to reduce text smearing.
        if tag in {"p", "br", "hr", "li", "ul", "ol", "h1", "h2", "h3", "h4", "h5", "h6", "pre", "div"}:
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
                abs_href = urllib.parse.urljoin(self._base_url, href)
                self._links.append(Link(text=txt, href=abs_href))
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

        # keep some whitespace, normalize later
        if data and not data.isspace():
            self._text_parts.append(data)

        if self._in_a:
            self._a_text_parts.append(data)

    def title(self) -> str:
        return " ".join("".join(self._title_parts).split()).strip()

    def text(self) -> str:
        raw = "".join(self._text_parts)
        # Normalize: collapse repeated spaces, collapse excessive blank lines.
        raw = re.sub(r"[ \t\r\f\v]+", " ", raw)
        raw = re.sub(r"\n{3,}", "\n\n", raw)
        return raw.strip() + "\n"

    def links(self) -> list[Link]:
        # Dedup on href preserving first-seen.
        seen: set[str] = set()
        out: list[Link] = []
        for l in self._links:
            if l.href in seen:
                continue
            seen.add(l.href)
            out.append(l)
        return out


def _http_get(url: str, timeout_s: int) -> bytes:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "osprey-interfaces-webdoc/1.0",
            "Accept": "text/html,application/xhtml+xml",
        },
        method="GET",
    )
    with urllib.request.urlopen(req, timeout=timeout_s) as resp:
        return resp.read()


def _write_text(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def _write_bytes(path: str, b: bytes) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("url", help="Documentation page URL")
    ap.add_argument("--out-dir", required=True, help="Output directory (created if missing)")
    ap.add_argument("--timeout", type=int, default=30, help="HTTP timeout seconds")
    ap.add_argument("--max-links", type=int, default=80, help="How many links to show in summary.md")
    args = ap.parse_args(argv)

    url = args.url
    out_dir = args.out_dir

    html_bytes = _http_get(url, timeout_s=args.timeout)
    _write_bytes(os.path.join(out_dir, "page.html"), html_bytes)

    parser = _HtmlTextAndLinks(base_url=url)
    parser.feed(html_bytes.decode("utf-8", errors="replace"))

    title = parser.title() or "(no title)"
    text = parser.text()
    links = parser.links()

    _write_text(os.path.join(out_dir, "page.txt"), text)
    _write_text(
        os.path.join(out_dir, "links.json"),
        json.dumps([{"text": l.text, "href": l.href} for l in links], indent=2) + "\n",
    )

    stamp = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    md_lines: list[str] = []
    md_lines.append(f"# Web doc snapshot\n")
    md_lines.append(f"- Source: `{url}`")
    md_lines.append(f"- Title: `{title}`")
    md_lines.append(f"- Generated: `{stamp}`\n")
    md_lines.append("## Top links\n")
    for l in links[: args.max_links]:
        txt = l.text if l.text else l.href
        md_lines.append(f"- [{txt}]({l.href})")
    md_lines.append("")
    _write_text(os.path.join(out_dir, "summary.md"), "\n".join(md_lines))

    print(f"Wrote: {os.path.join(out_dir, 'page.html')}")
    print(f"Wrote: {os.path.join(out_dir, 'page.txt')}")
    print(f"Wrote: {os.path.join(out_dir, 'links.json')}")
    print(f"Wrote: {os.path.join(out_dir, 'summary.md')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

