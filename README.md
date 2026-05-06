# PDFReader

Qt 6.8 module that reads bank-statement PDFs (Chilean banks) using the
first-party **QtPdf** module and exposes them through a Factory pattern,
mirroring the structure of [NFBaeza/XLSXReader](https://github.com/NFBaeza/XLSXReader)
but for PDF input instead of XLSX.

This repo is a **library / module** — it is meant to be linked into a parent
application (e.g. *Budget Monitor*). A small test executable is included for
local development.

## Layout

```
PDFReader/
├── CMakeLists.txt
├── main.cpp                  # standalone test entry point
├── include/
│   ├── bank.h                # abstract Bank base
│   ├── bankFactory.h         # Factory
│   ├── simpleClassifier.h    # regex-based transaction classifier
│   └── banks/                # per-bank headers go here (BICE, Santander, …)
└── src/
    ├── bank.cpp
    ├── bankFactory.cpp
    ├── simpleClassifier.cpp
    └── banks/                # per-bank .cpp implementations go here
```

The concrete per-bank classes (`BICE`, `Santander`, `Wise`, `Estado`, `Chile`)
are intentionally **not** shipped here — derive `Bank` and register your
implementation in `BankFactory::create()`.

## Requirements

- CMake ≥ 3.19
- Qt **6.8** with the `Pdf` component installed
  (`Qt6::Core`, `Qt6::Gui`, `Qt6::Sql`, `Qt6::Pdf`)
- C++17 compiler

## Build (standalone)

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.x/gcc_64
cmake --build build -j
./build/PDFReaderTest
```

## Use as a module from a parent CMake project

Drop this repo into your parent project (submodule, vendored copy, FetchContent)
and:

```cmake
add_subdirectory(third_party/PDFReader)   # PDFREADER_BUILD_TEST auto-OFF here
target_link_libraries(MyApp PRIVATE PDFReader::PDFReader)
```

Public headers are exposed automatically via the target's `INTERFACE`
include directories — `#include "bankFactory.h"` just works.

If you instead `cmake --install` the library, consume it with:

```cmake
find_package(PDFReader CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE PDFReader::PDFReader)
```

## Adding a new bank

1. Create `include/banks/<bank>.h` declaring `class <Bank> : public Bank`.
2. Create `src/banks/<bank>.cpp` implementing `readBankMovementsCredit()`
   and `readBankMovementsDebit()` (input: pre-extracted page text).
3. Add the new `.cpp` to `PDFREADER_SOURCES` in [CMakeLists.txt](CMakeLists.txt).
4. In [src/bankFactory.cpp](src/bankFactory.cpp), `#include` the header and
   uncomment the matching `case` in `BankFactory::create()`.

## API sketch

```cpp
auto bank = BankFactory::create("bice", "debit");
if (bank) {
    bank->readBankMovements("/abs/path/to/statement.pdf");
}
```

Internally `Bank::readBankMovements()` extracts the text of every page via
`QPdfDocument::getAllText(page)` and dispatches to the credit/debit parser
based on `typeAccount`.