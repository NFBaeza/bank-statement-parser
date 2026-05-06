# PDFReader

Qt 6 library that parses Chilean bank-statement PDFs into structured
transactions through a Factory pattern. PDF text extraction is delegated
to a small Python ([pdfplumber](https://github.com/jsvine/pdfplumber))
sidecar invoked through `QProcess`, keeping the C++ side free of any PDF
runtime dependency.

It mirrors the structure of
[NFBaeza/XLSXReader](https://github.com/NFBaeza/XLSXReader) but for PDF
input. The repo ships as a **library / module** meant to be linked into a
parent application (e.g. *Budget Monitor*); a small test executable is
included for local development.

## Layout

```
PDFReader/
├── CMakeLists.txt
├── main.cpp                  # standalone test driver
├── requirements.txt          # Python deps for the extractor sidecar
├── tools/
│   └── extract_pdf.py        # pdfplumber-based extractor (stdout: JSON)
├── include/
│   ├── bank.h                # abstract Bank base + shared helpers
│   ├── bankFactory.h
│   ├── simpleClassifier.h
│   └── banks/                # per-bank headers
│       ├── bice.h
│       ├── chile.h
│       └── santander.h
└── src/
    ├── bank.cpp              # QProcess sidecar call, helpers
    ├── bankFactory.cpp
    ├── simpleClassifier.cpp
    └── banks/
        ├── bice.cpp
        ├── chile.cpp         # debit + credit (with cuota expansion)
        └── santander.cpp
```

## Architecture

```
              ┌──────────────────────────────────────────┐
              │  Bank::extractPdfText (C++, QProcess)    │
              │     ──>  python tools/extract_pdf.py X   │
              │     <──  {"pages": ["...","...","..."]}  │
              └──────────────────────────────────────────┘
                                │
                                ▼
              per-bank parser (BICE / Chile / Santander)
                  • unwraps wrapped rows
                  • regex-extracts named columns
                  • SimpleClassifier tags the description
                                │
                                ▼
                       QList<Transaction>
```

`Bank::extractPdfText()` spawns the Python sidecar, captures its JSON
output and returns a `QStringList` of per-page text. Each derived bank
parses that text with its own column regex.

Helpers shared by every bank live on the base class:

- `Bank::spanishMonth("abr") → 4`
- `Bank::cleanDescription(...)` strips embedded dates, times, RUTs,
  `monto $ X`, long numeric IDs and bank-specific noise (`O.Gerencia`,
  `Agustinas`, `boleta N XXX`, …).
- `Bank::unwrapRows(pageText, rowStartRx)` joins continuation lines onto
  the row currently being assembled.

## Requirements

- **C++ side**: CMake ≥ 3.19, Qt **6.8** (`Core`, `Sql`), C++17 compiler.
- **Python sidecar**: Python 3.10+ with `pdfplumber` (pinned in
  [requirements.txt](requirements.txt)).

No PDF library (Poppler, QtPdf) is linked into C++ anymore — the sidecar
owns all PDF parsing.

## Setup

```bash
# 1. C++ deps already installed via your distro / Qt installer.

# 2. Python sidecar — one-time:
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

The build picks up `.venv/bin/python` and `tools/extract_pdf.py`
automatically (CMake injects the project root as a compile-time path).

## Build (standalone)

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.x/gcc_64
cmake --build build -j
./build/PDFReaderTest
```

The test driver in [main.cpp](main.cpp) loads one PDF from `files/` and
dumps the parsed transactions to stdout. Edit it to point at whatever
statement you want to test.

## Use as a module from a parent CMake project

```cmake
add_subdirectory(third_party/PDFReader)   # PDFREADER_BUILD_TEST auto-OFF
target_link_libraries(MyApp PRIVATE PDFReader::PDFReader)
```

Headers are exposed via the target's `INTERFACE` include directories —
`#include "bankFactory.h"` just works.

If you instead `cmake --install` the library, consume it with:

```cmake
find_package(PDFReader CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE PDFReader::PDFReader)
```

## Sidecar overrides (production / packaging)

`Bank::extractPdfText()` resolves the sidecar in this order:

1. `PDFREADER_EXTRACTOR_BUNDLE` — path to a single PyInstaller binary
   (recommended for shipping; no Python needed at runtime).
2. `PDFREADER_EXTRACTOR_PYTHON` + `PDFREADER_EXTRACTOR_SCRIPT` — explicit
   interpreter and script paths.
3. Fallback: `<project>/.venv/bin/python <project>/tools/extract_pdf.py`
   (auto-detected during dev via the `PDFREADER_PROJECT_ROOT` compile
   definition).
4. Last resort: `python3` on `PATH`.

To freeze the sidecar into a single binary:

```bash
.venv/bin/pip install pyinstaller
.venv/bin/pyinstaller --onefile tools/extract_pdf.py
# resulting binary at dist/extract_pdf
export PDFREADER_EXTRACTOR_BUNDLE=/abs/path/to/dist/extract_pdf
```

## API sketch

```cpp
auto bank = BankFactory::create("bice", "debit");
if (bank) {
    bank->readBankMovements("/abs/path/to/statement.pdf");
}
```

Internally `Bank::readBankMovements()` extracts text via the sidecar and
dispatches to the credit/debit implementation defined by each derived
bank class (`typeAccount` is `"debit"` or `"credit"`).

`SimpleClassifier` tags descriptions with categories such as
`groceries`, `drugstore`, `food delivery`, `transport`, `gas`,
`utilities`, `healthcare`, `education`, `subscription`, `online shopping`,
`retail`, `clothes`, `insurance`, `bank transfer`, `withdraw cash`,
`card payment`, `online payment`, `bank comission`, `paycheck`,
`investment`, `deposit`, `purchase`, `others`.

## Adding a new bank

1. Create `include/banks/<bank>.h` declaring `class <Bank> : public Bank`.
2. Create `src/banks/<bank>.cpp` implementing
   `readBankMovementsCredit()` and `readBankMovementsDebit()` (input:
   pre-extracted page text). Use `Bank::unwrapRows`,
   `Bank::cleanDescription`, `Bank::spanishMonth` to avoid duplicating
   logic.
3. Add the new files to `PDFREADER_PUBLIC_HEADERS` and
   `PDFREADER_SOURCES` in [CMakeLists.txt](CMakeLists.txt).
4. In [src/bankFactory.cpp](src/bankFactory.cpp), `#include` the header
   and add the matching `case` in `BankFactory::create()`.

[src/banks/bice.cpp](src/banks/bice.cpp) is the cleanest worked example
for a checking-account parser; [src/banks/chile.cpp](src/banks/chile.cpp)
shows credit-card parsing with cuota (installment) expansion.
