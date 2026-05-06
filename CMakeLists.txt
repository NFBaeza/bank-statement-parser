cmake_minimum_required(VERSION 3.19)

project(PDFREADER
    VERSION 0.1.0
    DESCRIPTION "Qt6 PDF reader module for Chilean bank statements (factory pattern)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ---- Options ---------------------------------------------------------------
# When this project is included via add_subdirectory() from a parent project,
# the test executable and install rules are skipped automatically.
if(NOT DEFINED PDFREADER_IS_TOP_LEVEL)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(PDFREADER_IS_TOP_LEVEL ON)
    else()
        set(PDFREADER_IS_TOP_LEVEL OFF)
    endif()
endif()

option(PDFREADER_BUILD_TEST    "Build the standalone test executable"
       ${PDFREADER_IS_TOP_LEVEL})
option(PDFREADER_INSTALL       "Generate install rules for PDFReader"
       ${PDFREADER_IS_TOP_LEVEL})

# ---- Qt --------------------------------------------------------------------
# PDF text extraction is delegated to a Python (pdfplumber) sidecar invoked via
# QProcess — see tools/extract_pdf.py and src/bank.cpp. No PDF library is
# linked into the C++ side anymore.
find_package(Qt6 REQUIRED COMPONENTS Core Sql)

qt_standard_project_setup()

# ---- Library ---------------------------------------------------------------
set(PDFREADER_PUBLIC_HEADERS
    include/pdfparser/bank.h
    include/pdfparser/bankFactory.h
    include/pdfparser/simpleClassifier.h
    include/pdfparser/banks/bice.h
    include/pdfparser/banks/chile.h
    include/pdfparser/banks/santander.h
)

set(PDFREADER_SOURCES
    src/bank.cpp
    src/bankFactory.cpp
    src/simpleClassifier.cpp
    src/banks/bice.cpp
    src/banks/chile.cpp
    src/banks/santander.cpp
    # Other banks — uncomment once a concrete class is added:
    #   src/banks/wise.cpp
    #   src/banks/estado.cpp
)

add_library(PDFReader STATIC
    ${PDFREADER_SOURCES}
    ${PDFREADER_PUBLIC_HEADERS}
)
add_library(PDFReader::PDFReader ALIAS PDFReader)

target_include_directories(PDFReader
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/pdfparser
)

target_link_libraries(PDFReader
    PUBLIC
        Qt6::Core
        Qt6::Sql
)

# Inject the source-tree path so the dev build can find tools/extract_pdf.py
# and the .venv interpreter without env vars. Production builds should set
# PDFREADER_EXTRACTOR_BUNDLE (PyInstaller binary) instead.
target_compile_definitions(PDFReader PRIVATE
    PDFREADER_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(PDFReader PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME PDFReader
)

# ---- Test executable -------------------------------------------------------
if(PDFREADER_BUILD_TEST)
    qt_add_executable(PDFReaderTest main.cpp)
    target_link_libraries(PDFReaderTest PRIVATE PDFReader::PDFReader)
endif()

# ---- Install / export ------------------------------------------------------
if(PDFREADER_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS PDFReader
        EXPORT  PDFReaderTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    install(EXPORT PDFReaderTargets
        FILE        PDFReaderTargets.cmake
        NAMESPACE   PDFReader::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PDFReader
    )
endif()
