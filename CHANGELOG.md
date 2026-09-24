# Changelog

All notable changes to the Finora framework will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.4.0] - 2026-09-24

### Added
- **NIST FIPS 203 & 204 Standard Support:** Direct compliance with FIPS 203 (ML-KEM-768) and FIPS 204 (ML-DSA-65).
- **Hybrid Post-Quantum Handshake:** Seamless integration of X25519 ECDH + ML-KEM-768 with HKDF-SHA256 session derivation (RFC 8446).
- **Pure-C SDK (`include/finora/finora.h`):** Zero-allocation C interface (`finora_client_connect`, `finora_send`, `finora_recv`, `finora_disconnect`) for linking from C, Rust, Go, Python, and C#.
- **Zero-Allocation Hot Path Streaming:** AES-256-GCM authenticated streaming encryption with fixed 48-byte wire overhead.
- **Finora State Guard:** 64-bit sequence counters and sliding bitmask window anti-replay protection.
- **Multi-Protocol Zero-Copy Sniffing:** Instant detection and handling of FIX, OUCH, ITCH, ISO 20022 XML/JSON, and Web3 JSON-RPC packets.
- **Modern Build System:** Standardized `CMakeLists.txt` with `find_package(Finora)` support and updated multi-platform `Makefile`.
- **Comprehensive Documentation Suite:** Architecture guide, 5-minute quickstart, integration patterns, wire format specification, and benchmark reports.

### Changed
- Refactored repository into a modular framework architecture (`include/finora/`, `src/`, `examples/`, `tools/`, `docs/`).
- Standardized open-source licensing to Apache License 2.0 with enterprise patent grant protection.

---

## [1.0.0] - 2026-01-15
- Initial prototype of PQC TLS proxy and FIX protocol encapsulation.
