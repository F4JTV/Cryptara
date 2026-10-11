#!/usr/bin/env python3
"""The Diffie-Hellman exchange with an older Python station.

cryptography 3.x (OpenSSL 1.1) writes the public key in the PKCS#3 form,
without q; current versions write X9.42.  This runs the CryptoManager of
cryptara.py on such an old library (no PyQt6 needed) against test_crypto:

    oldvenv/bin/python check_old_cryptography.py cryptara.py build/test_crypto
"""
import ast
import subprocess
import sys

source = open(sys.argv[1], encoding="utf-8").read()
tree = ast.parse(source)
cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "CryptoManager")
namespace = {}
exec(compile("import base64, hashlib, secrets\n"
             "from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes\n"
             "from cryptography.hazmat.primitives import padding, hashes, serialization\n"
             "from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC\n"
             "from cryptography.hazmat.primitives.kdf.hkdf import HKDF\n"
             "from cryptography.hazmat.primitives.asymmetric import dh\n"
             "from cryptography.hazmat.backends import default_backend\n", "<imports>", "exec"), namespace)
exec(compile(ast.Module(body=[cls], type_ignores=[]), "<CryptoManager>", "exec"), namespace)

import contextlib, io, cryptography
from cryptography.hazmat.primitives import serialization

station = namespace["CryptoManager"]()
with contextlib.redirect_stdout(io.StringIO()):
    station.set_password("  correct horse battery 73  ")
    station.enable()
    ours = station.generate_dh_keypair()
der = serialization.load_pem_public_key(__import__("base64").b64decode(ours)).public_bytes(
    serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
form = "PKCS#3" if bytes.fromhex("2a864886f70d010301") in der else "X9.42"
print("cryptography", cryptography.__version__, "- key written in", form, "form")

out = subprocess.run([sys.argv[2], "--dh-respond", ours], capture_output=True, text=True).stdout
lines = dict(l.split(" ", 1) for l in out.splitlines() if " " in l)
if "PUBKEY" not in lines:
    sys.exit("FAILED: the C++ side refused the key: " + out)
with contextlib.redirect_stdout(io.StringIO()):
    ok = station.compute_shared_secret(lines["PUBKEY"])
    message = station.decrypt(lines["SESSION"])
failures = []
if not ok:
    failures.append("old Python refused the C++ key")
if station.session_key.hex() != lines["SESSIONKEY"]:
    failures.append("session keys differ")
if not message.startswith("Bonjour de la station"):
    failures.append("old Python could not decrypt: %r" % message)
print("FAILED: " + "; ".join(failures) if failures else "PASSED: same session key, message decrypted")
sys.exit(1 if failures else 0)
