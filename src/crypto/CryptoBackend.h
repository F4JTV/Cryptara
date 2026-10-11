// CryptoBackend.h - the cryptographic primitives CRYPTARA needs, behind one
// small interface.
//
// The Windows build implements them with CNG (bcrypt.dll, part of Windows
// since Vista: nothing to install or ship).  CryptoBackendOpenSsl.cpp is the
// same interface over OpenSSL, used only to build and test the program on a
// Linux development machine.
//
// Everything works on raw bytes.  The message format, the key derivations
// and the public-key encoding that make CRYPTARA compatible with the Python
// version live above this layer, in CryptoManager and DhPublicKey.
#pragma once

#include <QByteArray>
#include <memory>

namespace crypto {

// Cryptographically secure random bytes.
QByteArray randomBytes(int count);

// HMAC-SHA256 of data under key (32 bytes out).
QByteArray hmacSha256(const QByteArray &key, const QByteArray &data);

// PBKDF2-HMAC-SHA256.
QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt,
                        int iterations, int length);

// AES-256-CBC with PKCS#7 padding.  key is 32 bytes, iv 16 bytes.
// Return false on any failure (wrong sizes, bad padding on decryption).
bool aes256CbcEncrypt(const QByteArray &key, const QByteArray &iv,
                      const QByteArray &plaintext, QByteArray &ciphertext);
bool aes256CbcDecrypt(const QByteArray &key, const QByteArray &iv,
                      const QByteArray &ciphertext, QByteArray &plaintext);

// The RFC 3526 group 14 (2048-bit MODP) parameters, big-endian, as used by
// the Python version.  p is 256 bytes, g is 2, q = (p - 1) / 2.
const QByteArray &dhGroup14Prime();
const QByteArray &dhGroup14Subprime();
constexpr int kDhGroup14Bytes = 256;

// One Diffie-Hellman key pair over group 14.
class DhKeyPair
{
public:
    DhKeyPair();
    ~DhKeyPair();
    DhKeyPair(const DhKeyPair &) = delete;
    DhKeyPair &operator=(const DhKeyPair &) = delete;

    // A fresh key pair.  False if the system refused.
    bool generate();
    // Test hook: a key pair with a known private exponent x and public
    // value y (both big-endian), to check the agreement against vectors
    // computed by the Python version.
    bool importForTest(const QByteArray &privateX, const QByteArray &publicY);

    bool isValid() const;
    // Our public value y = g^x mod p, big-endian, kDhGroup14Bytes long.
    QByteArray publicValue() const;
    // The shared secret peerY^x mod p, big-endian, left-padded with zeros
    // to kDhGroup14Bytes - exactly what Python's DHPrivateKey.exchange()
    // returns.  False on failure.
    bool agree(const QByteArray &peerY, QByteArray &secret) const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

// The name of the implementation, for the log and the About box.
const char *backendName();

} // namespace crypto
