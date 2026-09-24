# Getting Started

This guide will have you running Finora in under 5 minutes.

## Prerequisites

| Dependency | Minimum Version | Notes |
|-----------|-----------------|-------|
| C++ Compiler | C++17 (GCC 9+, Clang 11+) | Apple Clang 13+ on macOS |
| OpenSSL | 3.0+ | Must include ML-KEM and ML-DSA providers |
| CMake | 3.16+ | Optional — Makefile also provided |

### macOS (Homebrew)

```bash
brew install openssl@3 cmake
```

### Ubuntu / Debian

```bash
sudo apt-get install build-essential cmake libssl-dev
```

### RHEL / CentOS / Fedora

```bash
sudo dnf install gcc-c++ cmake openssl-devel
```

## Build

### Option A: CMake (Recommended)

```bash
git clone https://github.com/finora-pqc/finora.git
cd finora

# Core framework only
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# With everything (examples, tools, tests)
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DFINORA_BUILD_EXAMPLES=ON \
    -DFINORA_BUILD_TOOLS=ON \
    -DFINORA_BUILD_TESTS=ON
cmake --build build -j$(nproc)
```

### Option B: Make

```bash
make prod    # Optimized build
make all     # Debug build
```

## Verify Installation

Run the NIST Known Answer Test suite:

```bash
# CMake
cd build && ctest --output-on-failure

# Make
make test_nist_kat
```

Expected output:
```
SUMMARY: 8 / 8 TESTS PASSED SUCCESSFULLY (100% COMPLIANCE)
```

## Run the PQC Proxy

### Standalone

```bash
./build/finora_proxy
# Listens on 127.0.0.1:5006, forwards to 127.0.0.1:5003
```

### With the Demo Environment

```bash
chmod +x scripts/*.sh
./scripts/start.sh
```

This starts:
1. **Mock Exchange** on port 5003 (simulated matching engine)
2. **PQC Proxy** on port 5006 (quantum-safe tunnel)
3. **TLS Proxy** on ports 5007/5008 (classical comparison)

You can now connect client applications or run the benchmark harness against port 5006.

### Stop Services

```bash
./scripts/stop.sh
```

## System Install

```bash
sudo cmake --install build --prefix /usr/local
```

This installs:
- Headers → `/usr/local/include/finora/`
- Library → `/usr/local/lib/libfinora.so` (or `.dylib`)
- Binary → `/usr/local/bin/finora_proxy`
- Config → `/usr/local/etc/finora/`
- CMake → `/usr/local/lib/cmake/Finora/`

## Next Steps

- [Architecture Overview](architecture.md) — Understand the system design
- [Integration Guide](integration-guide.md) — Deploy in your infrastructure
- [API Reference](api-reference.md) — C/C++ API documentation
- [Configuration Reference](configuration.md) — Customize `finora.yaml`
