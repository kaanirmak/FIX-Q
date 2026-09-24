# Security Policy & Cryptographic Model

Finora is designed from the ground up to protect high-value financial, banking, and Web3 infrastructure against both classical and quantum computing adversaries.

---

## 1. Threat Model & Quantum Adversaries

### 1.1 "Harvest Now, Decrypt Later" (HNDL) Attacks
Adversaries actively record encrypted network traffic today with the objective of decrypting it when cryptanalytically relevant quantum computers (CRQCs) become operational.
- **Vulnerability of Classical Cryptography:** RSA-2048, ECDH (secp256r1, X25519), and classical DSA can be solved in polynomial time using Shor's Algorithm ($\mathcal{O}((\log N)^3)$).
- **Finora Mitigation:** Ephemeral key agreement is protected with **NIST FIPS 203 ML-KEM-768** (Module Learning with Errors). Even if an adversary records today's traffic, a quantum computer cannot recover the session keys.

### 1.2 Grover's Algorithm vs Symmetric Ciphers
Grover's algorithm reduces brute-force search complexity of symmetric keys from $2^k$ to $2^{k/2}$.
- **Finora Mitigation:** All symmetric data encryption mandates **AES-256-GCM**, providing at least 128 bits of post-quantum security margin under Grover's algorithm.

### 1.3 Man-in-the-Middle (MITM) and Forgery Attacks
Quantum adversaries could forge classical digital signatures (e.g. RSA, ECDSA).
- **Finora Mitigation:** Peer identity and session initiation messages are authenticated using **NIST FIPS 204 ML-DSA-65** lattice signatures, ensuring post-quantum non-repudiation and origin authenticity.


### 1.5 Authentication Architecture & Network Separation (KEM vs DSA)
A common cryptographic fallacy is assuming that a Key Encapsulation Mechanism (ML-KEM-768) alone provides an authenticated session.
- **Cryptographic Principle:** ML-KEM provides IND-CCA2 confidentiality (encryption key agreement), but **does NOT authenticate peer identity**. An unauthenticated KEM is vulnerable to Man-in-the-Middle (MitM) attacks.
- **Finora Dual-Mode Authentication Architecture:**
  1. **Enterprise Fixed Infrastructure (BIST Colocation, FIX, ISO 20022 SWIFT):**
     - Operates over dedicated leased lines and datacenter cross-connects.
     - Uses **Static Identity Pinning (Pre-Shared Peer Matrix)** in configuration.
     - Bypasses X.509/ASN.1 CA traversal and dynamic revocation queries, achieving raw KEM speed (~93 µs) with zero MitM risk.
  2. **Dynamic Open Infrastructure (Web3 Dedicated RPC, MEV Relays):**
     - Operates over dynamic, untrusted networks.
     - Mandates **NIST FIPS 204 ML-DSA-65** digital signature verification over the ephemeral handshake transcript.
     - Complete authenticated handshake (ML-KEM + ML-DSA verify) executes in ~220–250 µs, remaining ~5x faster than classical TLS 1.3 while eliminating MitM vulnerabilities.

### 1.4 Replay and Order Injection Attacks
Financial market protocols are vulnerable to packet replays where adversaries re-transmit valid historical orders to disrupt market state.
- **Finora Mitigation:** Every frame includes a 64-bit strictly monotonic sequence number validated against an atomic sliding bitmask window (`finora::state_guard::StateGuard`). Duplicate or out-of-order packets are dropped immediately.

---

## 2. Standards Compliance

| Standard | Algorithm | Role in Finora | Quantum Security Category |
|:---|:---|:---|:---|
| **NIST FIPS 203** | ML-KEM-768 | Primary Key Encapsulation (KEM) | NIST Category 3 (Equivalent to AES-192) |
| **NIST FIPS 204** | ML-DSA-65 | Digital Signature Authentication | NIST Category 3 (Equivalent to SHA-384) |
| **RFC 8446** | X25519 + ML-KEM-768 | Hybrid Key Exchange & HKDF-SHA256 | Failsafe Dual Security |
| **NIST SP 800-38D**| AES-256-GCM | Authenticated Encryption with Associated Data | $\ge$ 128 bits under Grover |

---

## 3. Cryptographic Hygiene & Memory Sanitization

### Constant-Time Execution
All polynomial arithmetic and key validation routines in Finora utilize constant-time primitives provided by OpenSSL 3.0+ to prevent side-channel timing attacks and cache-collision attacks.

### Secure Memory Zeroization
Upon session termination or error handling, all sensitive keying material (ephemeral private keys, shared secrets, AES session keys, and expanded key schedules) is securely wiped from memory using `OPENSSL_cleanse()` or compiler-guaranteed `explicit_bzero()`, preventing core dumps or memory scan extraction.

### Automatic Ephemeral Rekeying
Finora enforces continuous key rotation:
- After **1,000,000 messages** (configurable).
- After **3,600 seconds** (1 hour) of continuous session activity.
- Upon any detected transport anomaly or sequence gap exceeding window bounds.

---

## 4. Reporting Vulnerabilities

If you discover a potential security vulnerability in Finora, please notify the security team directly at **security@finora.io** (or create a private GitHub Security Advisory). **Do not open public GitHub issues for security vulnerabilities.**
