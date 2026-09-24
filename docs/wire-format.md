# Finora Wire Format Specification

**Document Version:** 1.4.0  
**Status:** Standard Protocol Specification  
**Magic Bytes:** `0x464E` (`ASCII "FN"`)

---

## 1. Overview

The **Finora Wire Format** defines a low-overhead, deterministic, post-quantum secure transport envelope for wrapping high-frequency financial protocols (FIX, OUCH, ITCH), ISO 20022 banking messages, and Web3 RPC transactions.

The protocol supports two operational frame layouts:
1. **Full Handshake Envelope (Frame Type A):** Ephemeral Hybrid KEM + Post-Quantum Digital Signature + Authenticated Ciphertext. Used for session negotiation and re-keying frames.
2. **Zero-Allocation Hot-Path Frame (Frame Type B):** Ultra-low-latency streaming frame authenticated with AES-256-GCM using derived session keys. Critical path overhead is fixed at **48 bytes** total envelope overhead.

---

## 2. Common Wire Header (`WireHeader`)

Every Finora packet begins with a deterministic, 32-byte byte-aligned header (`#pragma pack(push, 1)`):

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Magic (0x464E)       |    Version    |  Payload Type |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Session ID                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                   Sequence Number (64-bit)                    +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Length                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                       IV / Nonce (96-bit)                     +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Field Definitions

| Field | Size (Bytes) | Type | Description |
|:---|:---|:---|:---|
| **Magic** | 2 | `uint16_t` | Network byte order `0x464E` (`ASCII "FN"`). Discard packets failing this check immediately. |
| **Version** | 1 | `uint8_t` | Protocol version (`0x01` for Finora v1.x). |
| **Payload Type** | 1 | `uint8_t` | Protocol discriminator: `0x10` (FIX), `0x11` (OUCH), `0x12` (ITCH), `0x20` (ISO 20022), `0x30` (Web3 RPC), `0x00` (Raw). |
| **Session ID** | 4 | `uint32_t` | Ephemeral session identifier negotiated during handshake. |
| **Sequence Number**| 8 | `uint64_t` | Monotonically increasing counter per session. Used for anti-replay sliding window validation. |
| **Payload Length** | 4 | `uint32_t` | Length in bytes of the ciphertext segment (excluding tag and signatures). |
| **IV / Nonce** | 12 | `uint8_t[12]` | 96-bit unique cryptographic nonce for AES-256-GCM. Generated via HKDF counter expansion. |

Total Header Size: **32 bytes**.

---

## 3. Frame Layouts

### 3.1 Streaming Hot-Path Frame (Type B)

Used for continuous trading traffic after session key establishment. Designed for zero memory allocation in ring buffers:

```
+-------------------------------------------------------------+
|                     WireHeader (32 Bytes)                   |
+-------------------------------------------------------------+
|              AES-256-GCM Ciphertext (N Bytes)               |
+-------------------------------------------------------------+
|              AES-256-GCM Auth Tag (16 Bytes)                |
+-------------------------------------------------------------+
```

- **Total Overhead:** 48 bytes (`32 bytes header + 16 bytes tag`)
- **Memory Footprint:** Static buffer contiguous allocation.
- **Latency Impact:** ~400 nanoseconds encryption + tagging.

---

### 3.2 Full Handshake / Enveloped Frame (Type A)

Used for cryptographic handshake, zero-trust perimeter cross-connects, or isolated authenticated messages:

```
+-------------------------------------------------------------+
|                     WireHeader (32 Bytes)                   |
+-------------------------------------------------------------+
|               Ephemeral X25519 PubKey (32 Bytes)            |
+-------------------------------------------------------------+
|             NIST FIPS 203 ML-KEM-768 CT (1,088 Bytes)       |
+-------------------------------------------------------------+
|              AES-256-GCM Ciphertext (N Bytes)               |
+-------------------------------------------------------------+
|              AES-256-GCM Auth Tag (16 Bytes)                |
+-------------------------------------------------------------+
|            NIST FIPS 204 ML-DSA-65 Sig (3,309 Bytes)        |
+-------------------------------------------------------------+
```

### Signature Coverage
The ML-DSA-65 signature is computed over:
$$\text{SignedContent} = \text{X25519\_Pub} \parallel \text{MLKEM768\_CT} \parallel \text{IV} \parallel \text{Ciphertext} \parallel \text{Tag}$$

This guarantees:
1. **Confidentiality:** Post-quantum security (ML-KEM-768) + classical security (X25519).
2. **Authenticity:** Quantum-resistant non-repudiation (ML-DSA-65).
3. **Integrity:** AEAD authenticated encryption (AES-256-GCM).

---

## 4. Anti-Replay Sliding Window

Finora includes stateful sequence protection in `finora/state_guard.hpp`:

- Each session maintains an expected sequence range: $[S_{\max} - W + 1, S_{\max}]$, where $W$ is the configurable sliding window size (default: 1,024 packets).
- Packet sequence validation rules:
  1. $S > S_{\max}$: Valid new packet. $S_{\max} \leftarrow S$, window advances.
  2. $S \le S_{\max} - W$: **Rejected** (too old / duplicate replay attack).
  3. $S_{\max} - W < S \le S_{\max}$: Inspected against bitmask. If bit already set $\rightarrow$ **Duplicate Replay Attack detected**, connection alerts raised, packet discarded.

---

## 5. Security & Verification

To verify packet correctness programmatically:
```cpp
#include <finora/pqc_crypto.hpp>

std::vector<uint8_t> packet = receive_from_socket();
if (packet.size() < sizeof(finora::WireHeader)) {
    // Malformed packet
}

const finora::WireHeader* hdr = reinterpret_cast<const finora::WireHeader*>(packet.data());
if (ntohs(hdr->magic) != finora::MAGIC) {
    // Drop: Not a valid Finora frame
}
```
