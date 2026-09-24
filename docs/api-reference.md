# Finora API Reference

Comprehensive reference for the Finora Post-Quantum Cryptographic Framework APIs (C SDK and C++17 Core Engine).

---

## 1. Pure-C SDK (`finora.h`)

The C API provides an ABI-stable, zero-allocation C interface suitable for linking into C, C++, Rust, Python (ctypes/cffi), Go (cgo), Java (JNI), or C# (.NET P/Invoke) applications.

### Header

```c
#include <finora/finora.h>
```

### Types

#### `finora_ctx_t`
```c
typedef struct finora_ctx_t finora_ctx_t;
```
Opaque handle representing an active encrypted PQC session context.

#### Protocol Types (`uint8_t payload_type`)
| Constant | Value | Description |
|:---|:---|:---|
| `FINORA_PROTO_RAW` | `0x00` | Raw binary / opaque byte stream |
| `FINORA_PROTO_FIX` | `0x01` | Financial Information eXchange (FIX 4.2 / 4.4 / 5.0SP2) |
| `FINORA_PROTO_OUCH` | `0x02` | Nasdaq OUCH protocol |
| `FINORA_PROTO_ITCH` | `0x03` | Nasdaq ITCH market data feed |
| `FINORA_PROTO_ISO20022` | `0x04` | ISO 20022 Financial Messages (pacs.008, pain.001, camt.053) |
| `FINORA_PROTO_JSONRPC` | `0x05` | Web3 / Ethereum JSON-RPC (`eth_sendRawTransaction`, etc.) |

---

### Functions

#### `finora_client_connect`

Establishes a TCP connection to a remote Finora PQC Gateway, performs the Hybrid Post-Quantum handshake (NIST FIPS 203 ML-KEM-768 + X25519 + HKDF-SHA256), and initializes AES-256-GCM symmetric stream ciphers.

```c
finora_ctx_t* finora_client_connect(const char* remote_ip, 
                                    uint16_t port, 
                                    const char* config_path);
```

- **Parameters:**
  - `remote_ip`: IPv4 or IPv6 address of the remote gateway (e.g., `"127.0.0.1"`).
  - `port`: Remote gateway port (e.g., `9800`).
  - `config_path`: Path to `finora.yaml` configuration file, or `NULL` to use default parameters.
- **Returns:** Non-null `finora_ctx_t*` pointer on success; `NULL` on connection or handshake failure.
- **Thread Safety:** Safe to call across threads for creating independent sessions.

---

#### `finora_send`

Encrypts and transmits a financial payload using zero-copy ring buffers and AES-256-GCM authenticated encryption. Wraps the payload into the `0x464E` Finora Wire Format envelope with monotonic sequence numbering.

```c
int finora_send(finora_ctx_t* ctx, 
                const uint8_t* payload, 
                size_t length, 
                uint8_t payload_type);
```

- **Parameters:**
  - `ctx`: Valid session context returned by `finora_client_connect`.
  - `payload`: Pointer to the unencrypted message bytes.
  - `length`: Byte size of the payload.
  - `payload_type`: Protocol discriminator (e.g. `FINORA_PROTO_FIX`).
- **Returns:** `0` on success; negative error code on failure:
  - `-1`: Invalid context or null payload.
  - `-2`: Payload exceeds maximum frame size (64 KB).
  - `-3`: Encryption failure.
  - `-4`: Socket write error / peer disconnected.
- **Latency:** Critical path overhead < 15 microseconds.

---

#### `finora_recv`

Receives an encrypted frame from the wire, verifies the Finora magic header (`0x464E`), authenticates the AEAD tag, verifies the anti-replay sequence window, and writes decrypted plaintext into `buffer`.

```c
ssize_t finora_recv(finora_ctx_t* ctx, 
                    uint8_t* buffer, 
                    size_t max_length);
```

- **Parameters:**
  - `ctx`: Valid session context.
  - `buffer`: Pre-allocated buffer to store the decrypted payload.
  - `max_length`: Maximum capacity of `buffer`.
- **Returns:** Number of decrypted bytes written to `buffer`, or negative error code:
  - `0`: Connection closed gracefully by peer.
  - `-1`: Invalid context or buffer.
  - `-2`: Authentication tag verification failed (tampering detected).
  - `-3`: Sequence number replay or window violation detected.
  - `-4`: Buffer too small for decrypted payload.

---

#### `finora_disconnect`

Terminates the session, zeroes all cryptographic keying material in memory (mitigating cold boot and heap inspection attacks), and closes underlying sockets.

```c
void finora_disconnect(finora_ctx_t* ctx);
```

- **Parameters:**
  - `ctx`: Session context. After this call, `ctx` is invalidated.

---

### Pure-C Example

```c
#include <stdio.h>
#include <string.h>
#include <finora/finora.h>

int main(void) {
    // 1. Establish PQC connection
    finora_ctx_t* session = finora_client_connect("127.0.0.1", 9800, "config/finora.yaml");
    if (!session) {
        fprintf(stderr, "Failed to establish PQC connection\n");
        return 1;
    }

    // 2. Prepare FIX message
    const char* fix_order = "8=FIX.4.2\x01" "9=65\x01" "35=D\x01" "49=CLIENT\x01" "56=EXCHANGE\x01"
                            "11=ORD12345\x01" "55=GARAN.E\x01" "54=1\x01" "38=1000\x01" "44=125.50\x01" "10=128\x01";

    // 3. Send encrypted
    int rc = finora_send(session, (const uint8_t*)fix_order, strlen(fix_order), 0x01);
    if (rc != 0) {
        fprintf(stderr, "Send error: %d\n", rc);
    }

    // 4. Receive execution report
    uint8_t rx_buf[4096];
    ssize_t n = finora_recv(session, rx_buf, sizeof(rx_buf));
    if (n > 0) {
        printf("Received %zd bytes decrypted\n", n);
    }

    // 5. Clean teardown
    finora_disconnect(session);
    return 0;
}
```

---

## 2. C++17 Core Engine (`include/finora/`)

For low-latency C++ engines requiring direct access to protocol sniffers, lock-free ring buffers, and post-quantum cryptographic primitives.

### Namespace: `finora`

#### `AuthManager` (`finora/auth_manager.hpp`)
Decoupled peer authentication engine supporting both high-frequency trading colocation static pinning and dynamic lattice signature verification. Mandated as a strict pre-condition before ML-KEM-768 execution.

```cpp
#include <finora/auth_manager.hpp>

// Mode 1: CeFi Bilateral Pinning (BIST, FIX, ISO 20022 Colocation)
finora::AuthManager auth_cefi(finora::AuthMode::BILATERAL_PINNING);
auto res1 = auth_cefi.authenticate_peer("BIST_CORE_01", nullptr, 0);
if (res1.authenticated) {
    // Verified in ~5.1 µs via constant-time pre-shared pinned peer matrix
}

// Mode 2: Web3 ML-DSA-65 Verification (Dynamic RPC, MEV Relays)
finora::AuthManager auth_web3(finora::AuthMode::ML_DSA_65_VERIFY);
auto sig = auth_web3.sign_challenge(challenge_nonce, 32);
auto res2 = auth_web3.authenticate_peer("RPC_VALIDATOR_01", sig.data(), sig.size(), challenge_nonce, 32);
if (res2.authenticated) {
    // Verified in ~131.8 µs via NIST FIPS 204 ML-DSA-65 lattice signature
}
```

---

### Namespace: `finora::pqc`

#### `PQCCryptoEngine` (`finora/pqc_crypto.hpp`)
Core cryptographic engine executing NIST FIPS 203, FIPS 204, and RFC 8446 hybrid key exchanges.

```cpp
#include <finora/pqc_crypto.hpp>

// Key encapsulation
finora::pqc::MLKEM768 kem;
auto keypair = kem.generate_keypair();
auto [ciphertext, shared_secret_sender] = kem.encapsulate(keypair.public_key);
auto shared_secret_receiver = kem.decapsulate(ciphertext, keypair.private_key);

// Digital signatures
finora::pqc::MLDSA65 dsa;
auto dsa_keys = dsa.generate_keypair();
std::vector<uint8_t> signature = dsa.sign(message, dsa_keys.private_key);
bool valid = dsa.verify(message, signature, dsa_keys.public_key);

// Hybrid KEM (X25519 + ML-KEM-768)
finora::pqc::HybridKEM hybrid;
auto hybrid_keys = hybrid.generate_keypair();
auto encap_result = hybrid.encapsulate(hybrid_keys.public_key);
auto master_secret = hybrid.decapsulate(encap_result.ciphertext, hybrid_keys.private_key);
```

#### Wire Format Envelope (`finora/pqc_crypto.hpp`)

```cpp
// Wrap plaintext into 0x464E envelope with AES-256-GCM
std::vector<uint8_t> wire_packet = finora::pqc::wrap_envelope(
    plaintext_data,
    session_key,
    sequence_number,
    finora::codec::ProtocolType::FIX
);

// Unwrap and verify
auto unwrap_result = finora::pqc::unwrap_envelope(wire_packet, session_key);
if (unwrap_result.success) {
    // Access unwrap_result.payload, unwrap_result.seq_num
}
```

---

### Namespace: `finora::codec`

#### `ProtocolCodec` (`finora/codec.hpp`)
High-speed zero-copy protocol discriminator. Sniffs packets in < 5 nanoseconds by inspecting leading byte patterns without memory copies.

```cpp
#include <finora/codec.hpp>

finora::codec::ProtocolType proto = finora::codec::sniff_protocol(data, len);
switch (proto) {
    case finora::codec::ProtocolType::FIX:
        // Tag 8=FIX detection
        break;
    case finora::codec::ProtocolType::OUCH:
        // OUCH packet
        break;
    case finora::codec::ProtocolType::ISO20022:
        // XML / JSON banking message
        break;
    case finora::codec::ProtocolType::JSONRPC:
        // Web3 transaction
        break;
}
```

---

### Namespace: `finora::ring_buffer`

#### `RingBuffer<T, Capacity>` (`finora/ring_buffer.hpp`)
Cache-line aligned (64-byte padding), lock-free, single-producer single-consumer circular buffer. Guaranteed zero dynamic allocations (`malloc`/`new`) on the critical processing path.

```cpp
#include <finora/ring_buffer.hpp>

finora::ring_buffer::RingBuffer<finora::pqc::PacketSlot, 1024> queue;

// Producer thread
finora::pqc::PacketSlot slot;
slot.length = len;
std::memcpy(slot.data, input, len);
queue.push(slot);

// Consumer thread
finora::pqc::PacketSlot out_slot;
if (queue.pop(out_slot)) {
    // Process out_slot
}
```

---

### Namespace: `finora::state_guard`

#### `StateGuard` (`finora/state_guard.hpp`)
Tracks sequence numbers, sliding window anti-replay protection, and telemetry metrics (packets per second, packet loss, duplicate detection).

```cpp
#include <finora/state_guard.hpp>

finora::state_guard::StateGuard guard(1024); // Window size 1024 packets

if (!guard.validate_and_advance(incoming_seq_num)) {
    // Replay attack or out-of-order outside window! Drop packet.
}
```
