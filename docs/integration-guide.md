# Integration Guide

This guide covers three deployment patterns for integrating Finora into your infrastructure.

## Deployment Pattern 1: Transparent Proxy (Recommended)

The simplest approach. No application changes required.

### Architecture

```
┌──────────┐         ┌─────────────────┐         ┌──────────────┐
│  Your    │  TCP    │  Finora Proxy   │  TCP    │  Upstream    │
│  App     │────────▶│  (PQC Shield)   │────────▶│  Server      │
│          │◀────────│                 │◀────────│              │
└──────────┘         └─────────────────┘         └──────────────┘
 localhost:5006          PQC Tunnel              exchange:5003
```

### Steps

1. **Deploy Finora** alongside your application server:

```bash
# Install
cmake --install build --prefix /opt/finora

# Configure
cat > /opt/finora/etc/finora/finora.yaml << 'EOF'
network:
  listen_address: "127.0.0.1:5006"
  target_upstream: "exchange.example.com:5003"
  tcp_nodelay: true
  so_busy_poll: 50
  ring_buffer_entries: 65536

crypto:
  hybrid_mode: true
  kem:
    primary: "FIPS-203-ML-KEM-768"
    classical: "X25519"
  signature:
    algorithm: "FIPS-204-ML-DSA-65"
  symmetric:
    cipher: "AES-256-GCM"
    rekey_interval_messages: 1000000
EOF
```

2. **Redirect your application** to connect to `localhost:5006` instead of the upstream directly.

3. **Start Finora:**

```bash
/opt/finora/bin/finora_proxy &
```

### Use Cases

| Industry | Application | Upstream |
|----------|------------|----------|
| **Capital Markets** | OMS / EMS | Stock Exchange (BIST, NYSE, LSE) |
| **Banking** | Core Banking System | SWIFT Network / Central Bank |
| **Crypto** | Custodial Wallet | Ethereum Node / Exchange API |
| **HFT** | Trading Engine | Co-located Matching Engine |

## Deployment Pattern 2: Embedded C SDK

For applications that need direct control over the PQC tunnel.

### Link the Library

```bash
# Compile your application
gcc -o my_app my_app.c -I/opt/finora/include -L/opt/finora/lib -lfinora -lcrypto -lssl

# Or with pkg-config
gcc -o my_app my_app.c $(pkg-config --cflags --libs finora)
```

### API Usage

```c
#include <finora/finora.h>

int main() {
    // 1. Establish PQC-protected connection
    finora_ctx_t* ctx = finora_client_connect(
        "10.0.1.50", 5006, "/etc/finora/finora.yaml"
    );
    if (!ctx) {
        fprintf(stderr, "PQC handshake failed\n");
        return 1;
    }

    // 2. Send data (automatically encrypted)
    // payload_type: 0x10=FIX, 0x20=ISO20022, 0x30=Web3
    const char* order = "8=FIX.4.4\x0135=D\x01...";
    int rc = finora_send(ctx, (uint8_t*)order, strlen(order), 0x10);
    if (rc < 0) { /* handle error */ }

    // 3. Receive response (automatically decrypted & verified)
    uint8_t buf[4096];
    ssize_t n = finora_recv(ctx, buf, sizeof(buf));
    if (n > 0) {
        printf("Received %zd bytes\n", n);
    }

    // 4. Clean disconnect (zeroes session keys from memory)
    finora_disconnect(ctx);
    return 0;
}
```

### CMake Integration

```cmake
find_package(Finora 1.4 REQUIRED)
target_link_libraries(my_app PRIVATE Finora::finora)
```

## Deployment Pattern 3: Header-Only C++ Integration

For C++ applications that want to use Finora's cryptographic primitives directly.

```cpp
#include <finora/pqc_crypto.hpp>
#include <finora/codec.hpp>
#include <finora/state_guard.hpp>

int main() {
    // Initialize the PQC engine
    PqcEngine engine;

    // Encrypt data with full PQC envelope
    std::string plaintext = "8=FIX.4.4\x0135=D\x01...";
    auto envelope = engine.wrap_with_full_pqc(
        reinterpret_cast<const uint8_t*>(plaintext.data()),
        plaintext.size()
    );

    // envelope.kem_ciphertext_hex   → 1088 bytes ML-KEM-768
    // envelope.mldsa_signature_hex  → 3309 bytes ML-DSA-65
    // envelope.aes_ciphertext_hex   → AES-256-GCM ciphertext

    // Detect protocol type
    auto proto = finora::FinoraCodec::sniff_protocol(
        reinterpret_cast<const uint8_t*>(plaintext.data()),
        plaintext.size()
    );
    // Returns ProtocolType::FIX_44

    return 0;
}
```

## Multi-Protocol Banking Integration

### ISO 20022 SWIFT MX Payments

```yaml
# finora.yaml — banking configuration
protocols:
  parser: "auto-detect"
  iso20022:
    validate_schema: true
    generate_pacs002: true
    supported_messages:
      - "pacs.008.001.08"  # Customer Credit Transfer
      - "pain.001.001.09"  # Payment Initiation
      - "pacs.002.001.10"  # Payment Status Report
```

### Web3 / Ethereum Node Protection

```yaml
# finora.yaml — crypto/web3 configuration
protocols:
  web3:
    mev_protection: true
    anti_replay: true
    supported_methods:
      - "eth_sendRawTransaction"
      - "eth_call"
      - "eth_getTransactionReceipt"
```

## Production Considerations

### High Availability

Deploy Finora in an active-passive configuration:

```
App ──▶ HAProxy ──▶ Finora-1 (active)  ──▶ Upstream
                 └▶ Finora-2 (standby) ──▶ Upstream
```

### Key Rotation

Finora supports automatic key rotation:

```yaml
crypto:
  symmetric:
    rekey_interval_messages: 1000000  # Every 1M messages
    rekey_interval_seconds: 3600     # Or every hour
```

### Monitoring

Finora exposes Prometheus-compatible metrics:

```yaml
telemetry:
  prometheus_endpoint: "0.0.0.0:9090"
  latency_percentiles: [0.50, 0.90, 0.99, 0.999]
```
