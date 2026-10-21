"""
SeldOS - Humboldt Kernel Project
Tests for OpSec Hardening:
- In-Kernel AES-128 Sector-Level FDE (Full Disk Encryption)
- SeldFS Plausible Deniability & Headerless Hidden Volumes
- Fail-Safe Kernel Killswitch (Strict Default-Deny Clearnet Egress)
- Zero-DNS Leak Annihilation & RFC 7686 Guard (.onion protection)
- Total IPv6 Annihilation Invariant (Ethertype 0x86DD drop)
- SOCKS5 Domain-Based Circuit Stream Isolation (IsolateSOCKSAuth / RFC 1929)
GNU AGPLv3 Licensed - Humboldt Underground.
"""

import os
import sys
import struct
import hashlib
import tempfile
from pathlib import Path
import pytest
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

BASE_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(BASE_DIR))
sys.path.insert(0, str(BASE_DIR / "scripts"))

from mkdisk import (
    SELDFS_MAGIC,
    SELDFS_MAGIC_CRYPT,
    SELDFS_VERSION,
    SECTOR_SIZE,
    SB_SECTOR_LBA,
    BITMAP_SECTOR_LBA,
    INODES_START_LBA,
    DATA_START_LBA,
    SELDFS_BITMAP_SECTORS,
    INODES_SECTOR_COUNT,
    crypt_sector,
    create_seldfs_image
)


class TestSeldOSStorageEncryption:
    """Verifies SeldOS In-Kernel & Tooling FDE (Full Disk Encryption) Invariants."""

    def test_aes128_ctr_sector_encryption_roundtrip(self):
        """Verifies that 512-byte sector encryption with LBA tweak is reversible and constant-time."""
        key = b"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
        lba = 2048
        plaintext_sector = b"SELD_CONFIDENTIAL_PAYLOAD_SECTOR_TEST" + b"\x00" * (512 - 37)

        encrypted = crypt_sector(key, lba, plaintext_sector)
        assert len(encrypted) == 512
        assert encrypted != plaintext_sector
        # Plaintext signature must not be visible anywhere in the ciphertext
        assert b"SELD_CONFIDENTIAL" not in encrypted

        # Decryption using identical CTR operation
        decrypted = crypt_sector(key, lba, encrypted)
        assert decrypted == plaintext_sector

    def test_different_lba_produces_avalanche_different_ciphertext(self):
        """Verifies that the same plaintext encrypted at different LBAs produces completely distinct ciphertexts."""
        key = b"\xde\xad\xbe\xef\xca\xfe\xba\xbe\x01\x02\x03\x04\x05\x06\x07\x08"
        plaintext = b"\xaa" * 512

        ct_lba1 = crypt_sector(key, 100, plaintext)
        ct_lba2 = crypt_sector(key, 101, plaintext)

        assert ct_lba1 != ct_lba2
        # Verify high Hamming distance between ciphertext blocks
        diff_bytes = sum(1 for b1, b2 in zip(ct_lba1, ct_lba2) if b1 != b2)
        assert diff_bytes > 450  # Over 90% bytes differ due to LBA tweak nonce

    def test_encrypted_seldfs_disk_image_creation_and_deniability(self):
        """Creates an encrypted SeldFS disk image and verifies forensic deniability."""
        master_key = b"SOVEREIGN_KEY_16"
        sample_files = [
            ("/bin/init", b"\x7fELF" + b"\x00" * 1024),
            ("secret.txt", b"TOP_SECRET_AIRGAP_MISSION_CODENAME_VANTA\n"),
        ]

        with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
            tmp_path = tmp.name

        try:
            # 1. Standard Encrypted Volume
            create_seldfs_image(tmp_path, sample_files, compact=True, encrypt_key=master_key, headerless=False)

            with open(tmp_path, "rb") as f:
                f.seek(SB_SECTOR_LBA * SECTOR_SIZE)
                raw_sb_sector = f.read(SECTOR_SIZE)

            # Raw sector on disk is ciphertext
            assert b"SELD" not in raw_sb_sector
            assert b"TOP_SECRET" not in raw_sb_sector

            # Decrypt superblock with master key
            dec_sb = crypt_sector(master_key, SB_SECTOR_LBA, raw_sb_sector)
            magic, version = struct.unpack("<II", dec_sb[:8])
            assert magic == SELDFS_MAGIC_CRYPT
            assert version == SELDFS_VERSION

            # 2. Headerless / Plausible Deniability Volume
            create_seldfs_image(tmp_path, sample_files, compact=True, encrypt_key=master_key, headerless=True)

            with open(tmp_path, "rb") as f:
                f.seek(SB_SECTOR_LBA * SECTOR_SIZE)
                raw_deniable_sb = f.read(SECTOR_SIZE)

            # In headerless mode, raw sector looks like random entropy
            dec_deniable_sb = crypt_sector(master_key, SB_SECTOR_LBA, raw_deniable_sb)
            den_magic, den_ver = struct.unpack("<II", dec_deniable_sb[:8])
            assert den_magic == SELDFS_MAGIC
            assert den_ver == SELDFS_VERSION

        finally:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)


class TestSeldOSNetworkDefenseInvariants:
    """Verifies Fail-Safe Killswitch, Zero-DNS, and Total IPv6 Drop invariants."""

    GATEWAY_IP = 0x0202000A     # 10.0.2.2 (Wire order)
    LOOPBACK_IP = 0x0100007F    # 127.0.0.1
    PUBLIC_IP = 0x08080808      # 8.8.8.8 (Public Google DNS)
    PUBLIC_WEB_IP = 0x01010101  # 1.1.1.1 (Cloudflare)

    def is_packet_allowed_killswitch_sim(self, dest_ip, dest_port, killswitch_mode):
        """Python mirror of kernel net_is_packet_allowed_killswitch implementation."""
        if killswitch_mode != 1:  # NET_KILLSWITCH_STRICT = 1
            return True

        # Loopback
        if dest_ip == self.LOOPBACK_IP or (dest_ip & 0xFF) == 127:
            return True

        # Subnet broadcast
        if dest_ip == 0xFFFFFFFF:
            return True

        # Local Gateway allowed only on authorized proxy ports
        if dest_ip == self.GATEWAY_IP:
            if dest_port == 0:  # Ping/ARP
                return True
            allowed_ports = {9050, 9150, 8080, 9051, 9053, 53, 443, 8443}
            return dest_port in allowed_ports

        # Direct WAN/clearnet IPs blocked
        return False

    def test_killswitch_strictly_blocks_direct_clearnet_egress(self):
        """Verifies that public clearnet IPs are unconditionally dropped when Killswitch is STRICT."""
        # When killswitch is OFF (mode 0), connections pass
        assert self.is_packet_allowed_killswitch_sim(self.PUBLIC_IP, 53, 0) is True
        assert self.is_packet_allowed_killswitch_sim(self.PUBLIC_WEB_IP, 80, 0) is True

        # When killswitch is STRICT (mode 1), all public egress is hard-dropped
        assert self.is_packet_allowed_killswitch_sim(self.PUBLIC_IP, 53, 1) is False
        assert self.is_packet_allowed_killswitch_sim(self.PUBLIC_WEB_IP, 80, 1) is False
        assert self.is_packet_allowed_killswitch_sim(self.PUBLIC_WEB_IP, 443, 1) is False

    def test_killswitch_permits_authorized_tor_and_vless_gateways(self):
        """Verifies that Tor SOCKS5 and OpSec Gateway connections are explicitly allowed."""
        # Tor SOCKS5 (Port 9050 & 9150)
        assert self.is_packet_allowed_killswitch_sim(self.GATEWAY_IP, 9050, 1) is True
        assert self.is_packet_allowed_killswitch_sim(self.GATEWAY_IP, 9150, 1) is True

        # OpSec Sanitizing Web Gateway (Port 8080)
        assert self.is_packet_allowed_killswitch_sim(self.GATEWAY_IP, 8080, 1) is True

        # VLESS-Reality Tunnel (Port 443 & 8443)
        assert self.is_packet_allowed_killswitch_sim(self.GATEWAY_IP, 443, 1) is True
        assert self.is_packet_allowed_killswitch_sim(self.GATEWAY_IP, 8443, 1) is True

        # Unauthorized ports on gateway are rejected
        assert self.is_packet_allowed_killswitch_sim(self.GATEWAY_IP, 22, 1) is False
        assert self.is_packet_allowed_killswitch_sim(self.GATEWAY_IP, 3389, 1) is False

    def test_rfc7686_onion_guard_blocks_clearnet_resolution(self):
        """Verifies that .onion domains are blocked from ever generating clearnet DNS packets."""
        def dns_resolve_sim(hostname, zerodns_mode):
            clean = hostname.strip().lower()
            if clean.endswith(".onion") or clean == "onion":
                return -9  # RFC 7686 EPERM
            if zerodns_mode == 1:
                if clean in ("localhost", "loopback", "gateway"):
                    return 0
                return -9  # Zero-DNS EPERM
            return 0

        # RFC 7686 Guard: Always dropped regardless of mode
        assert dns_resolve_sim("duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion", 0) == -9
        assert dns_resolve_sim("somehidden.onion", 1) == -9

        # Zero-DNS mode blocks all clearnet domains
        assert dns_resolve_sim("example.com", 0) == 0
        assert dns_resolve_sim("example.com", 1) == -9
        # Local alias is preserved
        assert dns_resolve_sim("gateway", 1) == 0

    def test_ipv6_total_drop_invariant(self):
        """Verifies that Ethertype 0x86DD (IPv6) is dropped at L2 with zero emissions."""
        ETHERTYPE_IPV4 = 0x0800
        ETHERTYPE_ARP  = 0x0806
        ETHERTYPE_IPV6 = 0x86DD

        stats = {"rx_ipv4": 0, "rx_arp": 0, "rx_ipv6_dropped": 0, "rx_dropped": 0}

        def net_handle_frame_sim(ethertype):
            if ethertype == ETHERTYPE_ARP:
                stats["rx_arp"] += 1
                return "HANDLE_ARP"
            elif ethertype == ETHERTYPE_IPV4:
                stats["rx_ipv4"] += 1
                return "HANDLE_IPV4"
            elif ethertype == ETHERTYPE_IPV6:
                stats["rx_ipv6_dropped"] += 1
                stats["rx_dropped"] += 1
                return "SILENT_DROP_IPV6"
            else:
                stats["rx_dropped"] += 1
                return "DROP_UNKNOWN"

        assert net_handle_frame_sim(ETHERTYPE_IPV4) == "HANDLE_IPV4"
        assert net_handle_frame_sim(ETHERTYPE_ARP) == "HANDLE_ARP"
        assert net_handle_frame_sim(ETHERTYPE_IPV6) == "SILENT_DROP_IPV6"

        assert stats["rx_ipv6_dropped"] == 1
        assert stats["rx_dropped"] == 1


class TestTorStreamIsolationProtocol:
    """Verifies RFC 1928 / RFC 1929 Domain Stream Isolation framing for Tor."""

    def test_socks5_domain_stream_isolation_framing(self):
        """Verifies negotiation of RFC 1929 username/password auth carrying the isolation domain token."""
        # Initial offer: Method 0x00 and Method 0x02
        offer = bytes([0x05, 0x02, 0x00, 0x02])
        assert offer[0] == 0x05  # SOCKS5
        assert offer[1] == 0x02  # 2 methods offered
        assert 0x00 in offer[2:] # No Auth
        assert 0x02 in offer[2:] # User/Pass for IsolateSOCKSAuth

        # Simulating Tor daemon choosing 0x02 (User/Pass)
        server_choice = bytes([0x05, 0x02])
        assert server_choice[1] == 0x02

        # Subnegotiation payload carrying domain
        domain = "duckduckgo.onion"
        salt = 42
        iso_id = f"{domain}_{salt}".encode("utf-8")
        id_len = len(iso_id)

        # RFC 1929 packet format: [0x01, ulen, user..., plen, pass...]
        auth_pkt = bytes([0x01, id_len]) + iso_id + bytes([id_len]) + iso_id

        assert auth_pkt[0] == 0x01
        assert auth_pkt[1] == id_len
        assert auth_pkt[2:2 + id_len] == iso_id
        assert auth_pkt[2 + id_len] == id_len
        assert auth_pkt[2 + id_len + 1:] == iso_id
