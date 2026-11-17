import ctypes
import datetime
import hashlib
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, padding, rsa, x25519
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.x509.oid import NameOID

BASE_DIR = Path(__file__).resolve().parent.parent

HARNESS_C = r"""
#include "seld_aes128_gcm.h"
#include "seld_x25519.h"
#include "seld_hkdf.h"
#include "seld_sha512.h"
#include "seld_crypto_verify.h"
#include "sha256.h"
#include <string.h>

int test_verify_cert_parent(const uint8_t* child_der, size_t child_len,
                            const uint8_t* parent_der, size_t parent_len) {
    struct seld_x509_cert child, parent;
    if (seld_x509_parse(child_der, child_len, &child) != 0) return -1;
    if (seld_x509_parse(parent_der, parent_len, &parent) != 0) return -2;
    if (child.issuer_len != parent.subject_len ||
        memcmp(child.issuer_der, parent.subject_der, child.issuer_len) != 0) {
        return -3;
    }
    return seld_crypto_verify_sig(&parent.pub_key, child.sig_alg, child.pss_salt_len,
                                  child.tbs_ptr, child.tbs_len,
                                  child.sig_ptr, child.sig_len);
}

int test_verify_tls13_cv_from_cert(const uint8_t* cert_der, size_t cert_len,
                                   uint16_t sig_scheme,
                                   const uint8_t* content, size_t content_len,
                                   const uint8_t* sig, size_t sig_len) {
    struct seld_x509_cert cert;
    if (seld_x509_parse(cert_der, cert_len, &cert) != 0) return -1;
    return seld_crypto_verify_tls13_cv(&cert.pub_key, sig_scheme, content, content_len, sig, sig_len);
}
"""


def _build_crypto_so(tmpdir: str) -> ctypes.CDLL:
    harness_path = os.path.join(tmpdir, "harness.c")
    with open(harness_path, "w") as f:
        f.write(HARNESS_C)

    so_path = os.path.join(tmpdir, "lib_seld_crypto.so")
    srcs = [
        str(BASE_DIR / "userspace/libc/src/sha256.c"),
        str(BASE_DIR / "userspace/libc/src/seld_sha512.c"),
        str(BASE_DIR / "userspace/libc/src/seld_aes128_gcm.c"),
        str(BASE_DIR / "userspace/libc/src/seld_x25519.c"),
        str(BASE_DIR / "userspace/libc/src/seld_hkdf.c"),
        str(BASE_DIR / "userspace/libc/src/seld_crypto_verify.c"),
        harness_path,
    ]
    cmd = [
        "gcc",
        "-shared",
        "-fPIC",
        "-O2",
        "-Wall",
        "-Wextra",
        f"-I{BASE_DIR / 'userspace/libc/include'}",
        *srcs,
        "-o",
        so_path,
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if res.returncode != 0:
        raise RuntimeError(f"gcc failed:\n{res.stdout}\n{res.stderr}")
    return ctypes.CDLL(so_path)


class TestSeldCryptoAndX509(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        cls.lib = _build_crypto_so(cls._tmp.name)

        cls.lib.seld_aes128_gcm_encrypt.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_char_p,
        ]
        cls.lib.seld_aes128_gcm_decrypt.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_char_p,
        ]
        cls.lib.seld_aes128_gcm_decrypt.restype = ctypes.c_int

        cls.lib.seld_x25519.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        cls.lib.seld_x25519_base.argtypes = [ctypes.c_char_p, ctypes.c_char_p]

        cls.lib.seld_hmac_sha256.argtypes = [
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p,
        ]
        cls.lib.seld_hkdf_extract.argtypes = [
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p,
        ]
        cls.lib.seld_hkdf_expand_label.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_uint16, ctypes.c_char_p,
        ]

        cls.lib.seld_sha384_hash.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p]
        cls.lib.seld_sha512_hash.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p]

        cls.lib.seld_x509_verify_chain.argtypes = [
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.POINTER(ctypes.c_size_t),
            ctypes.c_size_t,
            ctypes.c_uint64,
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_size_t,
        ]
        cls.lib.seld_x509_verify_chain.restype = ctypes.c_int

        cls.lib.test_verify_cert_parent.argtypes = [
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_size_t,
        ]
        cls.lib.test_verify_cert_parent.restype = ctypes.c_int

        cls.lib.test_verify_tls13_cv_from_cert.argtypes = [
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_uint16,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_size_t,
        ]
        cls.lib.test_verify_tls13_cv_from_cert.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_fips180_4_sha384_sha512_vectors(self):
        out384 = ctypes.create_string_buffer(48)
        out512 = ctypes.create_string_buffer(64)
        for msg in [b"", b"abc", b"A" * 200]:
            self.lib.seld_sha384_hash(msg, len(msg), out384)
            self.lib.seld_sha512_hash(msg, len(msg), out512)
            self.assertEqual(out384.raw, hashlib.sha384(msg).digest())
            self.assertEqual(out512.raw, hashlib.sha512(msg).digest())

    def test_nist_sp800_38d_aes128_gcm_and_tamper_rejection(self):
        # NIST SP 800-38D Test Case 4 (key, iv, aad, pt -> ct, tag)
        key = bytes.fromhex("feffe9928665731c6d6a8f9467308308")
        iv = bytes.fromhex("cafebabefacedbaddecaf888")
        aad = bytes.fromhex("feedfacedeadbeeffeedfacedeadbeefabaddad2")
        pt = bytes.fromhex(
            "d9313225f88406e5a55909c5aff5269a"
            "86a7a9531534f7da2e4c303d8a318a72"
            "1c3c0c95956809532fcf0e2449a6b525"
            "b16aedf5aa0de657ba637b39"
        )
        expected_ct = bytes.fromhex(
            "42831ec2217774244b7221b784d0d49c"
            "e3aa212f2c02a4e035c17e2329aca12e"
            "21d514b25466931c7d8f6a5aac84aa05"
            "1ba30b396a0aac973d58e091"
        )
        expected_tag = bytes.fromhex("5bc94fbc3221a5db94fae95ae7121a47")

        ct_out = ctypes.create_string_buffer(len(pt))
        tag_out = ctypes.create_string_buffer(16)
        self.lib.seld_aes128_gcm_encrypt(key, iv, aad, len(aad), pt, len(pt), ct_out, tag_out)
        self.assertEqual(ct_out.raw, expected_ct)
        self.assertEqual(tag_out.raw, expected_tag)

        pt_out = ctypes.create_string_buffer(len(pt))
        rc = self.lib.seld_aes128_gcm_decrypt(key, iv, aad, len(aad), ct_out.raw, len(ct_out), tag_out.raw, pt_out)
        self.assertEqual(rc, 0)
        self.assertEqual(pt_out.raw, pt)

        # Interop against cryptography AESGCM + negative 1-bit tamper rejection
        aesgcm = AESGCM(key)
        ref = aesgcm.encrypt(iv, b"seldos-tls-payload", aad)
        ct2 = ref[:-16]
        tag2 = ref[-16:]
        dec2 = ctypes.create_string_buffer(len(ct2))
        self.assertEqual(self.lib.seld_aes128_gcm_decrypt(key, iv, aad, len(aad), ct2, len(ct2), tag2, dec2), 0)
        self.assertEqual(dec2.raw, b"seldos-tls-payload")

        bad_tag = bytes([tag2[0] ^ 1]) + tag2[1:]
        self.assertNotEqual(self.lib.seld_aes128_gcm_decrypt(key, iv, aad, len(aad), ct2, len(ct2), bad_tag, dec2), 0)

        bad_ct = bytes([ct2[0] ^ 0x80]) + ct2[1:]
        self.assertNotEqual(self.lib.seld_aes128_gcm_decrypt(key, iv, aad, len(aad), bad_ct, len(ct2), tag2, dec2), 0)

    def test_rfc7748_x25519_vectors_and_agreement(self):
        # RFC 7748 Section 6.1 test vector 1
        a_scalar = bytes.fromhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a")
        b_scalar = bytes.fromhex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb")
        expected_a_pub = bytes.fromhex("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a")
        expected_shared = bytes.fromhex("4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742")

        a_pub = ctypes.create_string_buffer(32)
        b_pub = ctypes.create_string_buffer(32)
        shared = ctypes.create_string_buffer(32)
        self.lib.seld_x25519_base(a_pub, a_scalar)
        self.lib.seld_x25519_base(b_pub, b_scalar)
        self.assertEqual(a_pub.raw, expected_a_pub)

        self.lib.seld_x25519(shared, a_scalar, b_pub.raw)
        self.assertEqual(shared.raw, expected_shared)

        # Interop against cryptography X25519
        py_priv = x25519.X25519PrivateKey.generate()
        py_pub = py_priv.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
        c_priv = os.urandom(32)
        c_pub = ctypes.create_string_buffer(32)
        c_shared = ctypes.create_string_buffer(32)
        self.lib.seld_x25519_base(c_pub, c_priv)
        self.lib.seld_x25519(c_shared, c_priv, py_pub)
        py_shared = py_priv.exchange(x25519.X25519PublicKey.from_public_bytes(c_pub.raw))
        self.assertEqual(c_shared.raw, py_shared)

        # Low-order identity point produces all-zero shared secret
        zero_shared = ctypes.create_string_buffer(32)
        self.lib.seld_x25519(zero_shared, a_scalar, b"\x00" * 32)
        self.assertEqual(zero_shared.raw, b"\x00" * 32)

    def test_rfc5869_rfc8446_hkdf_vectors(self):
        # RFC 5869 Test Case 1 (PRK)
        ikm = bytes.fromhex("0b" * 22)
        salt = bytes.fromhex("000102030405060708090a0b0c")
        expected_prk = bytes.fromhex("077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5")
        prk = ctypes.create_string_buffer(32)
        self.lib.seld_hkdf_extract(salt, len(salt), ikm, len(ikm), prk)
        self.assertEqual(prk.raw, expected_prk)

    def _build_cert(self, subject_cn: str, pub_key, issuer_name, issuer_priv,
                    sig_hash, is_ca: bool = False, padding_mode: str = "pkcs1",
                    not_before=None, not_after=None, san_names=None):
        now = datetime.datetime(2026, 1, 1, tzinfo=datetime.timezone.utc)
        nb = not_before or (now - datetime.timedelta(days=30))
        na = not_after or (now + datetime.timedelta(days=365))
        subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, subject_cn)])
        builder = (
            x509.CertificateBuilder()
            .subject_name(subject)
            .issuer_name(issuer_name)
            .public_key(pub_key)
            .serial_number(x509.random_serial_number())
            .not_valid_before(nb)
            .not_valid_after(na)
            .add_extension(x509.BasicConstraints(ca=is_ca, path_length=None), critical=True)
        )
        if is_ca:
            builder = builder.add_extension(
                x509.KeyUsage(
                    digital_signature=False, content_commitment=False,
                    key_encipherment=False, data_encipherment=False,
                    key_agreement=False, key_cert_sign=True, crl_sign=True,
                    encipher_only=False, decipher_only=False,
                ),
                critical=True,
            )
        if san_names:
            builder = builder.add_extension(
                x509.SubjectAlternativeName([x509.DNSName(n) for n in san_names]),
                critical=False,
            )
        if isinstance(issuer_priv, rsa.RSAPrivateKey):
            rsa_pad = (
                padding.PSS(mgf=padding.MGF1(sig_hash), salt_length=sig_hash.digest_size)
                if padding_mode == "pss"
                else padding.PKCS1v15()
            )
            cert = builder.sign(private_key=issuer_priv, algorithm=sig_hash, rsa_padding=rsa_pad)
        else:
            cert = builder.sign(private_key=issuer_priv, algorithm=sig_hash)
        return cert.public_bytes(serialization.Encoding.DER), subject

    def test_x509_sig_verification_and_mitm_forged_root_rejection(self):
        # 1. Verify RSA-PKCS1, RSA-PSS (SHA-256 & SHA-384), ECDSA P-256, ECDSA P-384 cert verification
        ca_rsa = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        ca_rsa_der, ca_rsa_name = self._build_cert("Test RSA CA", ca_rsa.public_key(),
                                                   x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Test RSA CA")]),
                                                   ca_rsa, hashes.SHA256(), is_ca=True)

        leaf_ec256 = ec.generate_private_key(ec.SECP256R1())
        leaf_pss256_der, _ = self._build_cert("pss256.example", leaf_ec256.public_key(), ca_rsa_name,
                                              ca_rsa, hashes.SHA256(), padding_mode="pss")
        self.assertEqual(self.lib.test_verify_cert_parent(leaf_pss256_der, len(leaf_pss256_der), ca_rsa_der, len(ca_rsa_der)), 0)

        leaf_pss384_der, _ = self._build_cert("pss384.example", leaf_ec256.public_key(), ca_rsa_name,
                                              ca_rsa, hashes.SHA384(), padding_mode="pss")
        self.assertEqual(self.lib.test_verify_cert_parent(leaf_pss384_der, len(leaf_pss384_der), ca_rsa_der, len(ca_rsa_der)), 0)

        ca_ec384 = ec.generate_private_key(ec.SECP384R1())
        ca_ec384_der, ca_ec384_name = self._build_cert("Test P384 CA", ca_ec384.public_key(),
                                                       x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Test P384 CA")]),
                                                       ca_ec384, hashes.SHA384(), is_ca=True)
        leaf_from_p384, _ = self._build_cert("ec384.example", leaf_ec256.public_key(), ca_ec384_name,
                                             ca_ec384, hashes.SHA384())
        self.assertEqual(self.lib.test_verify_cert_parent(leaf_from_p384, len(leaf_from_p384), ca_ec384_der, len(ca_ec384_der)), 0)

        # 2. Tamper 1 byte inside leaf cert DER -> signature verification must fail!
        tampered = bytearray(leaf_pss256_der)
        tampered[len(tampered) // 3] ^= 0x01
        self.assertNotEqual(self.lib.test_verify_cert_parent(bytes(tampered), len(tampered), ca_rsa_der, len(ca_rsa_der)), 0)

        # 3. TLS 1.3 CertificateVerify verification (RSA-PSS 0x0804, ECDSA P-256 0x0403, ECDSA P-384 0x0503) + tamper test
        cv_msg = b"\x20" * 64 + b"TLS 1.3, server CertificateVerify\x00" + hashlib.sha256(b"transcript").digest()

        rsa_cv_sig = ca_rsa.sign(cv_msg, padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=32), hashes.SHA256())
        self.assertEqual(self.lib.test_verify_tls13_cv_from_cert(ca_rsa_der, len(ca_rsa_der), 0x0804, cv_msg, len(cv_msg), rsa_cv_sig, len(rsa_cv_sig)), 0)
        self.assertNotEqual(self.lib.test_verify_tls13_cv_from_cert(ca_rsa_der, len(ca_rsa_der), 0x0804, cv_msg + b"!", len(cv_msg) + 1, rsa_cv_sig, len(rsa_cv_sig)), 0)

        ec256_cv_sig = leaf_ec256.sign(cv_msg, ec.ECDSA(hashes.SHA256()))
        self.assertEqual(self.lib.test_verify_tls13_cv_from_cert(leaf_pss256_der, len(leaf_pss256_der), 0x0403, cv_msg, len(cv_msg), ec256_cv_sig, len(ec256_cv_sig)), 0)
        bad_ec256 = bytearray(ec256_cv_sig)
        bad_ec256[-1] ^= 0x01
        self.assertNotEqual(self.lib.test_verify_tls13_cv_from_cert(leaf_pss256_der, len(leaf_pss256_der), 0x0403, cv_msg, len(cv_msg), bytes(bad_ec256), len(bad_ec256)), 0)

        ec384_cv_sig = ca_ec384.sign(cv_msg, ec.ECDSA(hashes.SHA384()))
        self.assertEqual(self.lib.test_verify_tls13_cv_from_cert(ca_ec384_der, len(ca_ec384_der), 0x0503, cv_msg, len(cv_msg), ec384_cv_sig, len(ec384_cv_sig)), 0)

        # 4. CRITICAL MITM REGRESSION TEST:
        # Attacker mints a fake CA claiming O="Internet Security Research Group", CN="ISRG Root X1"
        # with their own RSA key and issues a leaf cert. seld_x509_verify_chain MUST REJECT IT!
        attacker_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        forged_isrg_name = x509.Name([
            x509.NameAttribute(NameOID.COUNTRY_NAME, "US"),
            x509.NameAttribute(NameOID.ORGANIZATION_NAME, "Internet Security Research Group"),
            x509.NameAttribute(NameOID.COMMON_NAME, "ISRG Root X1"),
        ])
        forged_root = (
            x509.CertificateBuilder()
            .subject_name(forged_isrg_name)
            .issuer_name(forged_isrg_name)
            .public_key(attacker_key.public_key())
            .serial_number(123456)
            .not_valid_before(datetime.datetime(2025, 1, 1, tzinfo=datetime.timezone.utc))
            .not_valid_after(datetime.datetime(2030, 1, 1, tzinfo=datetime.timezone.utc))
            .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
            .sign(attacker_key, hashes.SHA256())
        ).public_bytes(serialization.Encoding.DER)

        forged_leaf, _ = self._build_cert("example.com", leaf_ec256.public_key(), forged_isrg_name,
                                          attacker_key, hashes.SHA256(), san_names=["example.com"])

        ptrs = (ctypes.c_void_p * 2)(
            ctypes.cast(ctypes.c_char_p(forged_leaf), ctypes.c_void_p),
            ctypes.cast(ctypes.c_char_p(forged_root), ctypes.c_void_p),
        )
        lens = (ctypes.c_size_t * 2)(len(forged_leaf), len(forged_root))
        root_name = ctypes.create_string_buffer(128)
        now_epoch = int(datetime.datetime(2026, 6, 1, tzinfo=datetime.timezone.utc).timestamp())
        rc = self.lib.seld_x509_verify_chain(ptrs, lens, 2, now_epoch, None, root_name, 128)
        self.assertNotEqual(rc, 0, "Forged ISRG Root X1 MITM chain MUST fail cryptographic verification!")

        # 5. Non-CA leaf signed by non-CA intermediate (BasicConstraints cA=FALSE) -> MUST FAIL
        non_ca_inter_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        non_ca_inter_der, non_ca_inter_name = self._build_cert(
            "Not A CA", non_ca_inter_key.public_key(), ca_rsa_name, ca_rsa, hashes.SHA256(), is_ca=False
        )
        bad_leaf_der, _ = self._build_cert(
            "bad.example", leaf_ec256.public_key(), non_ca_inter_name, non_ca_inter_key, hashes.SHA256()
        )
        # Signature leaf->non_ca_inter matches mathematically, so verify_cert_parent is 0,
        # while full chain validation rejects non-CA intermediate.
        self.assertEqual(self.lib.test_verify_cert_parent(bad_leaf_der, len(bad_leaf_der), non_ca_inter_der, len(non_ca_inter_der)), 0)
        ptrs3 = (ctypes.c_void_p * 3)(
            ctypes.cast(ctypes.c_char_p(bad_leaf_der), ctypes.c_void_p),
            ctypes.cast(ctypes.c_char_p(non_ca_inter_der), ctypes.c_void_p),
            ctypes.cast(ctypes.c_char_p(ca_rsa_der), ctypes.c_void_p),
        )
        lens3 = (ctypes.c_size_t * 3)(len(bad_leaf_der), len(non_ca_inter_der), len(ca_rsa_der))
        self.assertNotEqual(self.lib.seld_x509_verify_chain(ptrs3, lens3, 3, now_epoch, None, root_name, 128), 0)

    def test_real_system_root_cas_trusted_store(self):
        # Verify real ISRG Root X1 and DigiCert Global Root G2 certificates from /etc/ssl/certs
        now_epoch = int(datetime.datetime(2026, 6, 1, tzinfo=datetime.timezone.utc).timestamp())
        leaf_priv = ec.generate_private_key(ec.SECP256R1())
        for pem_path, cn in [
            ("/etc/ssl/certs/ISRG_Root_X1.pem", "ISRG Root X1"),
            ("/etc/ssl/certs/DigiCert_Global_Root_G2.pem", "DigiCert Global Root G2"),
        ]:
            if not os.path.exists(pem_path):
                continue
            cert = x509.load_pem_x509_certificate(Path(pem_path).read_bytes())
            der = cert.public_bytes(serialization.Encoding.DER)

            # Genuine self-signature must verify cleanly (exercises RSA-4096 / RSA-2048 math)
            self.assertEqual(self.lib.test_verify_cert_parent(der, len(der), der, len(der)), 0,
                             f"Self-signature verification failed for {pem_path}")

            # CA certificate must NOT be accepted directly as a TLS leaf certificate (-5)
            ptrs1 = (ctypes.c_void_p * 1)(ctypes.cast(ctypes.c_char_p(der), ctypes.c_void_p))
            lens1 = (ctypes.c_size_t * 1)(len(der))
            root_name = ctypes.create_string_buffer(128)
            rc_leaf_ca = self.lib.seld_x509_verify_chain(ptrs1, lens1, 1, now_epoch, None, root_name, 128)
            self.assertEqual(rc_leaf_ca, -5)

            # Even if attacker attaches the REAL Root CA in chain[1], a spoofed leaf signed
            # with attacker's key must be rejected (-11)
            spoof_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
            spoof_leaf, _ = self._build_cert("target.example", leaf_priv.public_key(),
                                             cert.subject, spoof_key, hashes.SHA256(),
                                             san_names=["target.example"])
            ptrs2 = (ctypes.c_void_p * 2)(
                ctypes.cast(ctypes.c_char_p(spoof_leaf), ctypes.c_void_p),
                ctypes.cast(ctypes.c_char_p(der), ctypes.c_void_p),
            )
            lens2 = (ctypes.c_size_t * 2)(len(spoof_leaf), len(der))
            rc_spoof = self.lib.seld_x509_verify_chain(ptrs2, lens2, 2, now_epoch, None, root_name, 128)
            self.assertNotEqual(rc_spoof, 0, f"Spoofed leaf under {cn} must fail verification")


if __name__ == "__main__":
    unittest.main()
