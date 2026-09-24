# Architecture

## System Overview

Finora operates as a transparent security gateway that intercepts, encrypts, and authenticates network traffic between financial applications and their upstream counterparts. It requires **zero modifications** to existing application code.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        FINORA PQC GATEWAY                                │
│                                                                          │
│  ┌─────────────┐   ┌─────────────┐   ┌────────────┐   ┌──────────────┐  │
│  │  finora-     │   │  finora-    │   │  finora-   │   │  finora-     │  │
│  │  interceptor │──▶│  codec      │──▶│  crypto-   │──▶│  state-      │  │
│  │              │   │             │   │  core      │   │  guard       │  │
│  │ Ring Buffer  │   │ Protocol    │   │ ML-KEM-768 │   │ Seq Window   │  │
│  │ TCP_NODELAY  │   │ Sniffing    │   │ ML-DSA-65  │   │ Anti-Replay  │  │
│  │ SO_BUSY_POLL │   │ FIX/OUCH/   │   │ AES-256-GCM│   │ Telemetry    │  │
│  │              │   │ ISO20022/   │   │ X25519     │   │              │  │
│  │              │   │ Web3 RPC    │   │ HKDF-SHA256│   │              │  │
│  └─────────────┘   └─────────────┘   └────────────┘   └──────────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
```

## Core Modules

### Module 1: `finora-interceptor` (Ring Buffer & Network I/O)

**File:** `include/finora/ring_buffer.hpp`

The network I/O layer uses a pre-allocated, lock-free ring buffer to achieve zero heap allocation on the critical path. All socket reads and writes use stack-allocated aligned buffers.

Key design decisions:
- **64-byte cache-line alignment** (`alignas(64)`) prevents false sharing
- **`TCP_NODELAY`** disables Nagle's algorithm for sub-millisecond delivery
- **`SO_BUSY_POLL`** enables kernel busy polling (50µs) for deterministic tail latency
- Pre-allocated ring buffer with 65,536 entries (configurable)

### Module 2: `finora-codec` (Protocol Detection & Parsing)

**File:** `include/finora/codec.hpp`

Deep packet inspection engine that automatically identifies the application-layer protocol:

| Protocol | Detection Method | Wire Type Byte |
|----------|-----------------|----------------|
| FIX 4.x/5.x | `8=FIX.` magic prefix | `0x10` |
| OUCH 5.x | Type byte `0x4F` at offset 0 | `0x11` |
| ITCH 5.0 | Length-prefixed binary | `0x11` |
| ISO 20022 | `<?xml` + `urn:iso:std:iso:20022` | `0x20` |
| Web3 JSON-RPC | `{"jsonrpc":` / `{"method":` | `0x30` |
| CME SBE | SBE message header pattern | `0x12` |

The codec also implements frame boundary detection for each protocol, enabling correct message-level encryption even when TCP delivers partial frames.

### Module 3: `finora-crypto-core` (PQC Engine)

**File:** `include/finora/pqc_crypto.hpp`

The cryptographic heart of Finora, implementing a hybrid post-quantum + classical encryption pipeline:

```
Handshake:
  X25519 ECDH ──┐
                 ├── HKDF-SHA-256 ──▶ 32-byte Shared Secret ──▶ AES-256-GCM Key
  ML-KEM-768 ───┘

Per-Message:
  Plaintext ──▶ AES-256-GCM Encrypt ──▶ Ciphertext + Tag
                    │
  ML-DSA-65 Sign ───┘ (over entire wire envelope)
```

**Streaming AEAD:** The engine uses `thread_local` OpenSSL cipher contexts to avoid per-message allocation. The 96-bit IV is constructed deterministically from a 64-bit monotonic sequence counter + 32-bit random salt, eliminating the need for random number generation on the hot path.

### Module 4: `finora-state-guard` (Security State Machine)

**File:** `include/finora/state_guard.hpp`

Implements two critical security mechanisms:

1. **Sequence Sliding Window** — Tracks the highest received sequence number and maintains a 2^64-entry bitmap of recently seen sequences. Out-of-order messages within the window are accepted; messages behind the window are rejected.

2. **Anti-Replay Filter** — LRU-based hash table tracking recently seen message digests. Even if an attacker replays a message with a valid sequence number, the duplicate digest is detected and rejected.

## Wire Format

See [Wire Format Specification](wire-format.md) for the complete binary protocol definition.

```
┌─────────┬─────────┬──────┬──────────┬─────────┬──────────┬─────┬──────────┬──────┬───────────┬─────────────┐
│ Magic   │ Version │ Type │ Session  │ Seq Num │ Payload  │ IV  │ X25519   │ KEM  │ Encrypted │ ML-DSA-65   │
│ 0x464E  │  0x01   │ Byte │   ID     │ (64b)   │  Length  │(12B)│  (32B)   │(1088)│ Payload   │ Signature   │
│  (2B)   │  (1B)   │ (1B) │  (4B)   │  (8B)   │  (4B)   │     │          │      │ + GCM Tag │   (3309B)   │
└─────────┴─────────┴──────┴──────────┴─────────┴──────────┴─────┴──────────┴──────┴───────────┴─────────────┘
                                         FINORA WIRE HEADER (32 BYTES)
```

## Data Flow

### Outbound (Application → Network)

1. Application writes plaintext to `localhost:5006`
2. **Interceptor** reads into ring buffer (zero-copy)
3. **Codec** sniffs protocol, finds frame boundary, assigns wire type
4. **Crypto Core** encrypts frame with AES-256-GCM, wraps in wire envelope
5. **State Guard** assigns sequence number, records for anti-replay
6. Wire envelope (header + KEM + ciphertext + signature) sent to upstream

### Inbound (Network → Application)

1. Upstream response arrives at proxy
2. **State Guard** validates sequence number against sliding window
3. **Crypto Core** verifies ML-DSA-65 signature, decrypts AES-256-GCM payload
4. **Interceptor** writes decrypted plaintext back to application socket
