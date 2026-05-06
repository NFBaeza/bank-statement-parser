#!/usr/bin/env python3
"""PDF text extractor used by the C++ Bank class via QProcess.

Reads a PDF path from argv[1] and writes a JSON document to stdout:
    {"pages": ["page-1 text", "page-2 text", ...]}
On error: prints a JSON error to stdout and exits non-zero.
"""
import json
import sys

import pdfplumber


def extract(path: str) -> dict:
    pages: list[str] = []
    with pdfplumber.open(path) as pdf:
        for page in pdf.pages:
            text = page.extract_text(x_tolerance=2, y_tolerance=2) or ""
            pages.append(text)
    return {"pages": pages}


def main() -> int:
    if len(sys.argv) != 2:
        json.dump({"error": "usage: extract_pdf.py <file.pdf>"}, sys.stdout)
        return 2
    try:
        json.dump(extract(sys.argv[1]), sys.stdout, ensure_ascii=False)
        return 0
    except Exception as e:
        json.dump({"error": f"{type(e).__name__}: {e}"}, sys.stdout)
        return 1


if __name__ == "__main__":
    sys.exit(main())
