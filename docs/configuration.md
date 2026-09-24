# Configuration Guide

Finora is configured using declarative YAML files. This document details all configuration directives in `finora.yaml` and `crypto.yaml`, along with production tuning recommendations.

---

## 1. Primary Configuration (`config/finora.yaml`)

### Example Configuration

```yaml
version: "1.0"

node:
  id: "ist-core-gateway-01"
  datacenter: "bist-equinix-dc"
  role: "ingress-proxy"

network:
  listen_address: "127.0.0.1:5006"
  target_upstream: "127.0.0.1:5003"
  tcp_nodelay: true
  quickack: true
  so_busy_poll: 50
  ring_buffer_entries: 65536

crypto:
  hybrid_mode: true
  kem:
    primary: "FIPS-203-ML-KEM-768"
    classical: "X25519"
  signature:
    algorithm: "FIPS-204-ML-DSA-65"
    verify_peer: true
  symmetric:
    cipher: "AES-256-GCM"
    rekey_interval_messages: 1000000
    rekey_interval_seconds: 3600

protocols:
  parser: "auto-detect"
  fix:
    strip_insecure_tags: false
    validate_checksum: true
    session_target_comp_id: "BORSA_IST"

telemetry:
  prometheus_endpoint: "0.0.0.0:9090"
  tracing_sample_rate: 0.001
  latency_percentiles: [0.50, 0.90, 0.99, 0.999]
```

---

### Section Details

#### `node`
- `id`: Human-readable identifier for the gateway instance. Exported as a metric label.
- `datacenter`: Physical or cloud facility (e.g. `bist-equinix-dc`, `aws-eu-central-1`).
- `role`: Deployment topology role (`ingress-proxy`, `egress-proxy`, `bridge`).

#### `network`
- `listen_address`: `IP:PORT` where Finora listens for unencrypted plaintext traffic from local applications.
- `target_upstream`: `IP:PORT` of the remote peer (remote gateway or exchange server).
- `tcp_nodelay`: `true` disables Nagle's algorithm (`TCP_NODELAY`), minimizing latency by sending packets immediately.
- `quickack`: `true` enables Linux `TCP_QUICKACK`, forcing immediate ACK delivery without delaying.
- `so_busy_poll`: Number of microseconds to busy-poll kernel network sockets (`SO_BUSY_POLL`). Recommended for dedicated HFT cores (e.g. `50`).
- `ring_buffer_entries`: Size of the internal lock-free ring buffer (must be a power of 2, default: `65536`).

#### `crypto`
- `hybrid_mode`: `true` (default) enables RFC 8446 / NIST hybrid key exchange (classical ECDH + post-quantum KEM).
- `kem.primary`: Post-quantum algorithm: `FIPS-203-ML-KEM-768` (recommended) or `FIPS-203-ML-KEM-1024`.
- `kem.classical`: Classical key agreement: `X25519` (recommended) or `secp256r1`.
- `signature.algorithm`: Digital signature algorithm: `FIPS-204-ML-DSA-65` (recommended) or `FIPS-204-ML-DSA-87`.
- `signature.verify_peer`: `true` mandates mutual authentication of peer identity public keys.
- `symmetric.cipher`: AEAD algorithm: `AES-256-GCM` (hardware accelerated via AES-NI / ARM Crypto).
- `symmetric.rekey_interval_messages`: Frequency in packet count to rotate ephemeral AES keys (e.g. `1000000`).
- `symmetric.rekey_interval_seconds`: Maximum session key duration before automatic re-handshake (e.g. `3600`).

#### `protocols`
- `parser`: `auto-detect` sniffs headers automatically, or specify `fix`, `ouch`, `itch`, `iso20022`, `jsonrpc`.
- `fix.validate_checksum`: `true` verifies Tag 10 checksum before encryption.
- `fix.strip_insecure_tags`: `false` by default; set to `true` to strip sensitive proprietary credentials before re-transmission.

#### `telemetry`
- `prometheus_endpoint`: HTTP scrape address for Prometheus metrics (e.g., `0.0.0.0:9090`).
- `tracing_sample_rate`: Ratio of messages sampled for detailed microsecond latency profiling (e.g., `0.001` = 0.1%).
- `latency_percentiles`: Reported latency buckets (e.g., `[0.50, 0.90, 0.99, 0.999]`).

---

## 2. High-Frequency Low-Latency Operating System Tuning

For production Linux environments, optimize kernel network parameters:

```bash
# Disable CPU frequency throttling
sudo cpupower frequency-set -g performance

# Increase socket receive and send buffers
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216"
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"

# Enable low latency socket polling
sudo sysctl -w net.core.busy_read=50
sudo sysctl -w net.core.busy_poll=50
```
