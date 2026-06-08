import sys
import os
import json
import argparse
import secrets
import struct

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from pqc_tunnel import PQCTunnel

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--hex-message', required=True, help='Hex-encoded FIX message to encrypt')
    parser.add_argument('--tls-latency', type=float, default=0.0, help='Measured TLS 1.3 latency in ms')
    parser.add_argument('--pqc-latency', type=float, default=0.0, help='Measured PQC Tunnel latency in ms')
    parser.add_argument('--bist-response-hex', default='', help='Hex-encoded response message from BIST')
    args = parser.parse_args()
    
    try:
        # Decode the hex message to bytes
        msg_bytes = bytes.fromhex(args.hex_message)
        
        # Initialize PQCTunnel
        tunnel = PQCTunnel()
        
        # ──────────────────────────────────────────────
        # 1. PQC Tunnel Outgoing Processing & Breakdown
        # ──────────────────────────────────────────────
        wire_packet = tunnel.process_outgoing(msg_bytes)
        breakdown = tunnel.get_wire_breakdown(wire_packet)
        
        # ──────────────────────────────────────────────
        # 2. TLS 1.3 Standard Record Simulation
        # ──────────────────────────────────────────────
        # Standard TLS 1.3 application data record:
        # - Header (5 bytes): ContentType = 23 (0x17), Version = 3.3 (0x0303), Length (2 bytes)
        # - Ciphertext (variable): Encrypted [Plaintext + 1 byte Inner ContentType (0x17)]
        # - Authentication Tag (16 bytes)
        plaintext_len = len(msg_bytes)
        inner_plaintext = msg_bytes + b'\x17' # Plaintext + Inner Content Type
        
        # Encrypt inner plaintext with AES-256-GCM to get exact ciphertext and tag
        # We use a dummy key and nonce for size-accurate simulation
        dummy_tls_key = secrets.token_bytes(32)
        dummy_tls_nonce = secrets.token_bytes(12)
        from cryptography.hazmat.primitives.ciphers.aead import AESGCM
        aesgcm = AESGCM(dummy_tls_key)
        tls_ciphertext_with_tag = aesgcm.encrypt(dummy_tls_nonce, inner_plaintext, None)
        
        tls_ciphertext = tls_ciphertext_with_tag[:-16]
        tls_tag = tls_ciphertext_with_tag[-16:]
        
        tls_record_len = len(tls_ciphertext_with_tag)
        tls_header = struct.pack('>BHH', 23, 0x0303, tls_record_len)
        
        # ──────────────────────────────────────────────
        # 3. Formats & Final Result preparation
        # ──────────────────────────────────────────────
        bist_resp_clean = ''
        if args.bist_response_hex:
            bist_resp_bytes = bytes.fromhex(args.bist_response_hex)
            bist_resp_clean = bist_resp_bytes.decode('utf-8', errors='replace').replace('\x01', '|')
        
        # Prepare JSON response
        result = {
            "status": "success",
            "plaintext": msg_bytes.decode('utf-8', errors='replace').replace('\x01', '|'),
            "bist_response": bist_resp_clean,
            "tls_latency": args.tls_latency,
            "pqc_latency": args.pqc_latency,
            
            # PQC breakdown
            "pqc_total_wire_size": breakdown["total_wire_size"],
            "pqc_plaintext_size": breakdown["plaintext_size"],
            "x25519_pubkey_hex": breakdown["x25519_pubkey"].hex(),
            "kem_ciphertext_hex": breakdown["kem_ciphertext"].hex(),
            "aes_nonce_hex": breakdown["aes_nonce"].hex(),
            "aes_ciphertext_hex": breakdown["aes_ciphertext"].hex(),
            "mldsa_signature_hex": breakdown["mldsa_signature"].hex(),
            
            # Standard TLS 1.3 breakdown
            "tls_total_wire_size": len(tls_header) + len(tls_ciphertext_with_tag),
            "tls_header_hex": tls_header.hex(),
            "tls_nonce_hex": dummy_tls_nonce.hex(), # standard TLS 1.3 sequence XOR'd IV
            "tls_ciphertext_hex": tls_ciphertext.hex(),
            "tls_tag_hex": tls_tag.hex(),
        }
        print(json.dumps(result, indent=2))
        
    except Exception as e:
        error_result = {
            "status": "error",
            "error_message": str(e)
        }
        print(json.dumps(error_result, indent=2))
        sys.exit(1)

if __name__ == '__main__':
    main()
