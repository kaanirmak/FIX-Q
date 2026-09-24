# Contributing to Finora

Thank you for your interest in contributing to Finora! We welcome contributions from cryptography researchers, network engineers, HFT developers, and systems programmers.

---

## Code of Conduct

Finora adheres to a standard open-source Code of Conduct. Please be respectful, constructive, and collaborative in all issues, pull requests, and community discussions.

---

## Development Setup

### Prerequisites
- C++17 compatible compiler (GCC 9+, Clang 11+, Apple Clang 13+)
- OpenSSL 3.0 or higher
- Make or CMake 3.16+

### Building and Testing

```bash
# Clone the repository
git clone https://github.com/finora-pqc/finora.git
cd finora

# Compile with Make
make all

# Run the test suite (NIST KAT tests)
make test_nist_kat
```

---

## Coding Standards

1. **C++17 Standard:** Use clean, idiomatic modern C++17.
2. **Zero-Allocation on Hot Path:** Never invoke `malloc`, `new`, `std::string` reallocation, or dynamic container growth inside functions processing streaming wire traffic. Use pre-allocated buffers and `std::string_view` or spans where appropriate.
3. **Constant-Time Cryptography:** Any cryptographic comparison or key operations must use constant-time operations (`CRYPTO_memcmp`, `OPENSSL_cleanse`).
4. **Header Architecture:**
   - Public C API headers belong in `include/finora/finora.h`.
   - Core C++ modular headers belong in `include/finora/*.hpp`.
   - Reusable third-party headers belong in `third_party/`.
5. **No Warnings:** Code must compile cleanly with `-Wall -Wextra` on both GCC and Clang.

---

## Submitting Pull Requests

1. **Fork the repository** on GitHub.
2. **Create a descriptive feature branch:**
   ```bash
   git checkout -b feature/pqc-rekey-optimizations
   ```
3. **Commit your changes:**
   - Use clear commit messages adhering to Conventional Commits:
     `feat: add zero-copy memory pool for ring buffer`
     `fix: handle sequence rollover in state guard`
     `docs: update integration guide with OUCH examples`
4. **Verify Tests:** Ensure `./bin/test_nist_kat` passes 100%.
5. **Open a Pull Request** against the `main` branch with a clear description of the problem solved.

---

## Security Vulnerabilities

Please **DO NOT** report security vulnerabilities via public GitHub issues. See our [Security Policy](SECURITY.md) for instructions on confidential disclosure.
