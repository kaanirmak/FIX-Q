cmake_minimum_required(VERSION 3.16)
project(Finora
    VERSION 1.4.0
    DESCRIPTION "Post-Quantum Cryptographic Security Gateway for Financial Protocols"
    HOMEPAGE_URL "https://github.com/finora-pqc/finora"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ──────────────────────────────────────────────
# Options
# ──────────────────────────────────────────────
option(FINORA_BUILD_EXAMPLES   "Build example applications (demo server, mock exchange, benchmarks)" OFF)
option(FINORA_BUILD_TOOLS      "Build standalone CLI tools (pqc_tool, tls_proxy)" OFF)
option(FINORA_BUILD_TESTS      "Build test suite (NIST KAT)" OFF)
option(FINORA_BUILD_SHARED     "Build shared library (libfinora)" ON)
option(FINORA_INSTALL           "Generate install targets" ON)

# ──────────────────────────────────────────────
# Dependencies
# ──────────────────────────────────────────────
find_package(OpenSSL 3.0 REQUIRED)

# Platform-specific: Homebrew on macOS
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/openssl@3")
    find_package(OpenSSL 3.0 REQUIRED)
endif()

# ──────────────────────────────────────────────
# Core Library (header-only + SDK)
# ──────────────────────────────────────────────
add_library(finora_headers INTERFACE)
target_include_directories(finora_headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(finora_headers INTERFACE OpenSSL::SSL OpenSSL::Crypto)
target_compile_features(finora_headers INTERFACE cxx_std_17)

# Shared library (C SDK: finora_client_connect / send / recv / disconnect)
if(FINORA_BUILD_SHARED)
    add_library(finora SHARED src/finora_sdk.cpp)
    target_link_libraries(finora PUBLIC finora_headers)
    set_target_properties(finora PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        OUTPUT_NAME finora
    )
endif()

# PQC Proxy executable (core product)
add_executable(finora_proxy src/pqc_proxy.cpp)
target_link_libraries(finora_proxy PRIVATE finora_headers)

# ──────────────────────────────────────────────
# Tools (optional)
# ──────────────────────────────────────────────
if(FINORA_BUILD_TOOLS)
    add_executable(finora_pqc_tool tools/pqc_tool.cpp)
    target_link_libraries(finora_pqc_tool PRIVATE finora_headers)

    add_executable(finora_tls_proxy tools/tls_proxy.cpp)
    target_link_libraries(finora_tls_proxy PRIVATE finora_headers)
endif()

# ──────────────────────────────────────────────
# Examples (optional)
# ──────────────────────────────────────────────
if(FINORA_BUILD_EXAMPLES)
    # Demo Web Server + Testbed UI
    add_executable(finora_demo_server examples/demo_server/web_server.cpp)
    target_include_directories(finora_demo_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    target_link_libraries(finora_demo_server PRIVATE finora_headers)

    # Mock Exchange (BIST simulator)
    add_executable(finora_mock_exchange examples/mock_exchange/mock_bist.cpp)
    target_link_libraries(finora_mock_exchange PRIVATE finora_headers)

    # Benchmarks
    add_executable(finora_benchmark examples/benchmarks/benchmark.cpp)
    target_link_libraries(finora_benchmark PRIVATE finora_headers)

    add_executable(finora_micro_bench examples/benchmarks/micro_bench.cpp)
    target_link_libraries(finora_micro_bench PRIVATE finora_headers)
endif()

# ──────────────────────────────────────────────
# Tests (optional)
# ──────────────────────────────────────────────
if(FINORA_BUILD_TESTS)
    enable_testing()
    add_executable(finora_test_nist_kat tests/test_nist_kat.cpp)
    target_link_libraries(finora_test_nist_kat PRIVATE finora_headers)
    add_test(NAME nist_kat COMMAND finora_test_nist_kat)
endif()

# ──────────────────────────────────────────────
# Install
# ──────────────────────────────────────────────
if(FINORA_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY include/finora
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
    )

    install(TARGETS finora_proxy
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    if(FINORA_BUILD_SHARED)
        install(TARGETS finora finora_headers
            EXPORT FinoraTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )

        install(EXPORT FinoraTargets
            FILE FinoraTargets.cmake
            NAMESPACE Finora::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Finora
        )

        configure_package_config_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FinoraConfig.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/FinoraConfig.cmake
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Finora
        )

        write_basic_package_version_file(
            ${CMAKE_CURRENT_BINARY_DIR}/FinoraConfigVersion.cmake
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
        )

        install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/FinoraConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/FinoraConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Finora
        )
    endif()

    install(FILES config/finora.yaml config/crypto.yaml
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/finora
    )

    install(FILES LICENSE README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )
endif()
