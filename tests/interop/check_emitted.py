#!/usr/bin/env python3
"""Decrypt, with the Python version's CryptoManager, what test_crypto --emit
produced in C++, and run a Diffie-Hellman exchange against the C++ public key.

    test_crypto --emit | python3 check_emitted.py /path/to/cryptara.py ../crypto_vectors.h
"""
import importlib.util
import os
import sys

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

PASSWORD = "  correct horse battery 73  "
MESSAGE = "Bonjour de la station, 73 ! éèà \U0001F512 <tag> | pipes"


def load(path):
    spec = importlib.util.spec_from_file_location("cryptara_original", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def header_bytes(header_text, name):
    import re
    match = re.search(r"%s\[\] = \"([^\"]*)\";" % name, header_text)
    return bytes(int(h, 16) for h in re.findall(r"\\x([0-9a-f]{2})", match.group(1)))


def rebuild_station_b(cryptara, header_path):
    from cryptography.hazmat.primitives.asymmetric import dh
    text = open(header_path, encoding="ascii").read()
    x = int.from_bytes(header_bytes(text, "kStationBPrivateX"), "big")
    a_pub = header_bytes(text, "kStationAPublicKey").decode("ascii")
    station = cryptara.CryptoManager()
    station.set_password(PASSWORD)
    station.enable()
    params = station.deserialize_public_key(a_pub).parameters().parameter_numbers()
    group = dh.DHParameterNumbers(params.p, params.g)
    y = pow(params.g, x, params.p)
    station.dh_private_key = dh.DHPrivateNumbers(x, dh.DHPublicNumbers(y, group)).private_key()
    station.dh_public_key = station.dh_private_key.public_key()
    if not station.compute_shared_secret(a_pub):
        raise SystemExit("could not rebuild station B")
    return station


def main():
    lines = {}
    for line in sys.stdin:
        tag, _, value = line.strip().partition(" ")
        if tag in ("PASSWORD", "SESSION", "PUBKEY"):
            lines[tag] = value
    real_stdout = sys.stdout
    sys.stdout = sys.stderr
    cryptara = load(sys.argv[1])
    failures = []

    m = cryptara.CryptoManager()
    m.set_password(PASSWORD)
    m.enable()
    if m.decrypt(lines["PASSWORD"]) != MESSAGE:
        failures.append("password-mode message from C++ not decrypted")

    # The C++ side used station A's known key against station B's public
    # key: rebuild station B from the vectors and read the session message.
    station_b = rebuild_station_b(cryptara, sys.argv[2])
    if station_b.decrypt(lines["SESSION"]) != MESSAGE:
        failures.append("session-key message from C++ not decrypted")

    # And a fresh Python station agrees with the fresh C++ public key.
    b = cryptara.CryptoManager()
    b.set_password(PASSWORD)
    b.enable()
    b.generate_dh_keypair()
    try:
        peer = b.deserialize_public_key(lines["PUBKEY"])
        secret = b.dh_private_key.exchange(peer)
        if len(secret) != 256:
            failures.append("unexpected secret length %d" % len(secret))
    except Exception as exc:  # noqa: BLE001
        failures.append("C++ public key refused by Python: %r" % exc)
    if not b.compute_shared_secret(lines["PUBKEY"]):
        failures.append("compute_shared_secret failed on the C++ key")

    sys.stdout = real_stdout
    if failures:
        print("FAILED:\n  " + "\n  ".join(failures))
        sys.exit(1)
    print("PASSED: Python decrypted the C++ message and accepted the C++ DH key")


if __name__ == "__main__":
    main()
