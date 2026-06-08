"""
FIX-Q Post-Quantum Cryptography Tunnel
=======================================
Gerçek AES-256-GCM simetrik şifreleme ile korunan,
ML-KEM-768 ve ML-DSA-65 boyutlarını tam doğrulukla simüle eden hibrit PQC tüneli.

Kriptografik Yapı:
  - Key Exchange : X25519 (32B) + ML-KEM-768 (Ciphertext=1088B, SharedSecret=32B)
  - Symmetric    : AES-256-GCM (256-bit key, 12B nonce, 16B auth tag)
  - Signature    : ML-DSA-65 (Signature=3309B, PublicKey=1952B)

Not: ML-KEM ve ML-DSA NIST standardı olup, liboqs gerçek implementasyonudur.
     Bu modülde AES-256-GCM gerçek, KEM/DSA boyutları gerçekçi simülasyondur.
     Gerçek NIST PQ uygulaması için liboqs veya oqs-python entegre edilmelidir.
"""

import logging
import yaml
import os
import secrets
import struct
import hmac
import hashlib
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

logger = logging.getLogger(__name__)

# --- NIST Standart Boyutları ---
MLKEM768_CIPHERTEXT_SIZE = 1088   # bytes
MLKEM768_SHARED_SECRET_SIZE = 32  # bytes
MLDSA65_SIGNATURE_SIZE = 3309     # bytes
X25519_KEY_SIZE = 32              # bytes
AES_GCM_NONCE_SIZE = 12           # bytes
AES_GCM_TAG_SIZE = 16             # bytes (included in ciphertext by AESGCM)


class PQCTunnel:
    """
    Hybrid Post-Quantum Cryptography Tunnel.

    Wire Format (process_outgoing çıktısı):
    ┌──────────────────────────────────────────────────────────────────┐
    │ X25519 Ephemeral Key (32 bytes)                                  │
    │ ML-KEM-768 Ciphertext (1088 bytes)                               │
    │ AES-256-GCM Nonce (12 bytes)                                     │
    │ AES-256-GCM Ciphertext + Auth Tag (variable + 16 bytes)          │
    │ ML-DSA-65 Signature (3309 bytes)                                 │
    │ Plaintext Length (4 bytes, big-endian)                            │
    └──────────────────────────────────────────────────────────────────┘
    """

    def __init__(self, config_path="config/crypto.yaml"):
        self.config_path = config_path
        self.kem = "ML-KEM-768"
        self.dsa = "ML-DSA-65"
        self._load_config()

        # Gerçek 256-bit AES anahtarı (session key)
        # Gerçek dünyada bu ML-KEM key encapsulation ile türetilir.
        # Burada session başına sabit tutuyoruz (iki proxy aynı anahtarı paylaşır).
        self._aes_key = secrets.token_bytes(32)

        # HMAC signing key (ML-DSA simülasyonu için)
        # Gerçek dünyada ML-DSA private key olur.
        self._signing_key = secrets.token_bytes(64)

        logger.info(f"PQC Tunnel initialized: KEM={self.kem}, DSA={self.dsa}, AES-key={self._aes_key[:4].hex()}...")

    def _load_config(self):
        if os.path.exists(self.config_path):
            with open(self.config_path, "r") as f:
                cfg = yaml.safe_load(f)
                if "crypto" in cfg:
                    self.kem = cfg["crypto"].get("kem", {}).get("primary", self.kem)
                    self.dsa = cfg["crypto"].get("dsa", {}).get("primary", self.dsa)
                    logger.info(f"Loaded crypto config: KEM={self.kem}, DSA={self.dsa}")

    # ──────────────────────────────────────────────
    #  Gerçek AES-256-GCM Şifreleme / Deşifreleme
    # ──────────────────────────────────────────────

    def _aes_encrypt(self, plaintext: bytes) -> bytes:
        """AES-256-GCM ile gerçek şifreleme. Nonce + Ciphertext döner."""
        nonce = secrets.token_bytes(AES_GCM_NONCE_SIZE)
        aesgcm = AESGCM(self._aes_key)
        ciphertext = aesgcm.encrypt(nonce, plaintext, None)  # ciphertext includes 16B tag
        return nonce + ciphertext

    def _aes_decrypt(self, nonce_and_ciphertext: bytes) -> bytes:
        """AES-256-GCM ile gerçek deşifreleme."""
        nonce = nonce_and_ciphertext[:AES_GCM_NONCE_SIZE]
        ciphertext = nonce_and_ciphertext[AES_GCM_NONCE_SIZE:]
        aesgcm = AESGCM(self._aes_key)
        return aesgcm.decrypt(nonce, ciphertext, None)

    # ──────────────────────────────────────────────
    #  ML-KEM-768 Key Encapsulation (Boyut-Doğru Simülasyon)
    # ──────────────────────────────────────────────

    def _kem_encapsulate(self) -> tuple:
        """
        ML-KEM-768 encapsulation simülasyonu.
        Gerçek dünyada: (ciphertext, shared_secret) = ML-KEM.Encaps(public_key)
        Burada: Gerçek boyutlarda random ciphertext + shared secret üretir.
        """
        ciphertext = secrets.token_bytes(MLKEM768_CIPHERTEXT_SIZE)
        shared_secret = secrets.token_bytes(MLKEM768_SHARED_SECRET_SIZE)
        return ciphertext, shared_secret

    def _x25519_exchange(self) -> bytes:
        """X25519 ECDH key exchange simülasyonu. 32-byte ephemeral public key döner."""
        return secrets.token_bytes(X25519_KEY_SIZE)

    # ──────────────────────────────────────────────
    #  ML-DSA-65 Digital Signature (HMAC Tabanlı Simülasyon)
    # ──────────────────────────────────────────────

    def _sign(self, data: bytes) -> bytes:
        """
        ML-DSA-65 imza simülasyonu.
        Gerçek dünyada: signature = ML-DSA.Sign(private_key, data)
        Burada: HMAC-SHA512 ile imzalayıp, NIST boyutuna (3309 byte) pad'liyoruz.
        Bu hem boyut-doğru, hem de gerçek bir kriptografik doğrulama sağlar.
        """
        mac = hmac.new(self._signing_key, data, hashlib.sha512).digest()  # 64 bytes
        # ML-DSA-65 signature 3309 byte olmalı, kalan alanı random ile dolduralım
        padding = secrets.token_bytes(MLDSA65_SIGNATURE_SIZE - len(mac))
        return mac + padding

    def _verify(self, data: bytes, signature: bytes) -> bool:
        """
        ML-DSA-65 imza doğrulama simülasyonu.
        İlk 64 byte'ı HMAC-SHA512 ile karşılaştırır.
        """
        expected_mac = hmac.new(self._signing_key, data, hashlib.sha512).digest()
        received_mac = signature[:64]
        return hmac.compare_digest(expected_mac, received_mac)

    # ──────────────────────────────────────────────
    #  Ana Pipeline: process_outgoing / process_incoming
    # ──────────────────────────────────────────────

    def process_outgoing(self, raw_fix_data: bytes) -> bytes:
        """
        Ham FIX mesajını alır → Hibrit PQC zırhı uygular → Wire format döner.

        Adımlar:
          1. X25519 ephemeral key üret (32 bytes)
          2. ML-KEM-768 encapsulate et (1088 bytes ciphertext)
          3. AES-256-GCM ile şifrele (gerçek!)
          4. ML-DSA-65 ile imzala (3309 bytes signature)
          5. Hepsini paketleyerek wire format oluştur
        """
        # 1. X25519 key exchange
        x25519_pubkey = self._x25519_exchange()

        # 2. ML-KEM-768 encapsulation
        kem_ciphertext, _ = self._kem_encapsulate()

        # 3. AES-256-GCM gerçek şifreleme
        aes_output = self._aes_encrypt(raw_fix_data)  # nonce(12) + ciphertext + tag(16)

        # 4. Wire payload = X25519 + KEM + AES çıktısı
        payload_to_sign = x25519_pubkey + kem_ciphertext + aes_output

        # 5. ML-DSA-65 imza
        signature = self._sign(payload_to_sign)

        # 6. Son paket: payload + signature + plaintext_length (deşifreleme için)
        plaintext_len = struct.pack('>I', len(raw_fix_data))
        wire_packet = payload_to_sign + signature + plaintext_len

        return wire_packet

    def process_incoming(self, wire_packet: bytes) -> bytes:
        """
        Wire format'ı alır → Deşifre eder → Doğrular → Ham FIX mesajı döner.
        """
        # Son 4 byte: plaintext length
        plaintext_len = struct.unpack('>I', wire_packet[-4:])[0]

        # Signature: son 4 byte'tan önce, 3309 byte
        sig_end = len(wire_packet) - 4
        sig_start = sig_end - MLDSA65_SIGNATURE_SIZE
        signature = wire_packet[sig_start:sig_end]

        # Payload: baştan sig_start'a kadar
        payload = wire_packet[:sig_start]

        # İmza doğrulama
        if not self._verify(payload, signature):
            logger.error("ML-DSA-65 signature verification FAILED!")
            raise ValueError("Signature verification failed")

        # Payload parçalama
        x25519_key = payload[:X25519_KEY_SIZE]
        kem_ct = payload[X25519_KEY_SIZE:X25519_KEY_SIZE + MLKEM768_CIPHERTEXT_SIZE]
        aes_data = payload[X25519_KEY_SIZE + MLKEM768_CIPHERTEXT_SIZE:]

        # AES-256-GCM gerçek deşifreleme
        plaintext = self._aes_decrypt(aes_data)

        return plaintext

    # ──────────────────────────────────────────────
    #  Debug / Analiz Yardımcıları
    # ──────────────────────────────────────────────

    def get_wire_breakdown(self, wire_packet: bytes) -> dict:
        """
        Bir wire paketi alıp tüm katmanlarını ayrıştırır (UI gösterimi için).
        """
        plaintext_len = struct.unpack('>I', wire_packet[-4:])[0]
        sig_end = len(wire_packet) - 4
        sig_start = sig_end - MLDSA65_SIGNATURE_SIZE

        offset = 0
        x25519_key = wire_packet[offset:offset + X25519_KEY_SIZE]
        offset += X25519_KEY_SIZE

        kem_ct = wire_packet[offset:offset + MLKEM768_CIPHERTEXT_SIZE]
        offset += MLKEM768_CIPHERTEXT_SIZE

        aes_data = wire_packet[offset:sig_start]
        nonce = aes_data[:AES_GCM_NONCE_SIZE]
        ciphertext = aes_data[AES_GCM_NONCE_SIZE:]

        signature = wire_packet[sig_start:sig_end]

        return {
            "x25519_pubkey": x25519_key,
            "kem_ciphertext": kem_ct,
            "aes_nonce": nonce,
            "aes_ciphertext": ciphertext,
            "mldsa_signature": signature,
            "total_wire_size": len(wire_packet),
            "plaintext_size": plaintext_len,
        }
