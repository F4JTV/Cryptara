// CryptoBackendCng.cpp - CryptoBackend over Windows CNG (bcrypt.dll).
//
// CNG ships with every Windows since Vista and is the system's validated
// cryptographic library, so the program needs no OpenSSL or any other DLL.
// Raw Diffie-Hellman secrets (BCRYPT_KDF_RAW_SECRET) need Windows 8.1 or
// later, which Qt 6 requires anyway.
#include "CryptoBackend.h"

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>

#ifndef NT_SUCCESS
#  define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
// Declared only when the SDK targets Windows 8.1 or later.
#ifndef BCRYPT_KDF_RAW_SECRET
#  define BCRYPT_KDF_RAW_SECRET L"TRUNCATE"
#endif

namespace crypto {

namespace {

// CNG takes non-const byte pointers even for inputs it only reads.
PUCHAR in(const QByteArray &bytes)
{
    // A zero-length input still needs a valid pointer on some versions.
    static UCHAR empty = 0;
    return bytes.isEmpty() ? &empty
                           : reinterpret_cast<PUCHAR>(const_cast<char *>(bytes.constData()));
}

PUCHAR out(QByteArray &bytes)
{
    return reinterpret_cast<PUCHAR>(bytes.data());
}

// Algorithm providers are expensive to open: each is opened once and kept
// for the life of the process.  They are safe to share between threads.
BCRYPT_ALG_HANDLE openProvider(LPCWSTR algorithm, ULONG flags)
{
    BCRYPT_ALG_HANDLE handle = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&handle, algorithm, nullptr, flags)))
        return nullptr;
    return handle;
}

BCRYPT_ALG_HANDLE hmacProvider()
{
    static const BCRYPT_ALG_HANDLE handle =
        openProvider(BCRYPT_SHA256_ALGORITHM, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    return handle;
}

BCRYPT_ALG_HANDLE aesCbcProvider()
{
    static const BCRYPT_ALG_HANDLE handle = [] {
        BCRYPT_ALG_HANDLE h = openProvider(BCRYPT_AES_ALGORITHM, 0);
        if (!h)
            return h;
        const NTSTATUS status = BCryptSetProperty(
            h, BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_CBC)),
            sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
        if (!NT_SUCCESS(status)) {
            BCryptCloseAlgorithmProvider(h, 0);
            return BCRYPT_ALG_HANDLE(nullptr);
        }
        return h;
    }();
    return handle;
}

BCRYPT_ALG_HANDLE dhProvider()
{
    static const BCRYPT_ALG_HANDLE handle = openProvider(BCRYPT_DH_ALGORITHM, 0);
    return handle;
}

QByteArray leftPad(const QByteArray &value, int size)
{
    // Strip leading zeros, then pad back to the fixed width.
    int start = 0;
    while (start < value.size() - 1 && value.at(start) == '\0')
        ++start;
    const QByteArray trimmed = value.mid(start);
    if (trimmed.size() > size)
        return QByteArray();
    return QByteArray(size - trimmed.size(), '\0') + trimmed;
}

QByteArray paddedGenerator()
{
    QByteArray g(kDhGroup14Bytes, '\0');
    g[kDhGroup14Bytes - 1] = 2;
    return g;
}

// BCRYPT_DH_KEY_BLOB header followed by p, g, y and, for a private blob, x.
QByteArray dhKeyBlob(ULONG magic, const QByteArray &y, const QByteArray &x = QByteArray())
{
    BCRYPT_DH_KEY_BLOB header;
    header.dwMagic = magic;
    header.cbKey = kDhGroup14Bytes;
    QByteArray blob(reinterpret_cast<const char *>(&header), sizeof(header));
    blob += dhGroup14Prime();
    blob += paddedGenerator();
    blob += y;
    if (!x.isEmpty())
        blob += x;
    return blob;
}

bool aesCrypt(bool encrypt, const QByteArray &key, const QByteArray &iv,
              const QByteArray &input, QByteArray &output)
{
    if (key.size() != 32 || iv.size() != 16 || !aesCbcProvider())
        return false;
    if (!encrypt && (input.isEmpty() || input.size() % 16 != 0))
        return false;

    BCRYPT_KEY_HANDLE handle = nullptr;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(aesCbcProvider(), &handle, nullptr, 0,
                                               in(key), ULONG(key.size()), 0)))
        return false;

    // CNG updates the IV buffer in place, so each call gets a fresh copy.
    QByteArray ivCopy = iv;
    ULONG needed = 0;
    NTSTATUS status = encrypt
        ? BCryptEncrypt(handle, in(input), ULONG(input.size()), nullptr, out(ivCopy),
                        ULONG(ivCopy.size()), nullptr, 0, &needed, BCRYPT_BLOCK_PADDING)
        : BCryptDecrypt(handle, in(input), ULONG(input.size()), nullptr, out(ivCopy),
                        ULONG(ivCopy.size()), nullptr, 0, &needed, BCRYPT_BLOCK_PADDING);
    if (NT_SUCCESS(status)) {
        output.resize(int(needed));
        ivCopy = iv;
        ULONG written = 0;
        status = encrypt
            ? BCryptEncrypt(handle, in(input), ULONG(input.size()), nullptr, out(ivCopy),
                            ULONG(ivCopy.size()), out(output), needed, &written,
                            BCRYPT_BLOCK_PADDING)
            : BCryptDecrypt(handle, in(input), ULONG(input.size()), nullptr, out(ivCopy),
                            ULONG(ivCopy.size()), out(output), needed, &written,
                            BCRYPT_BLOCK_PADDING);
        output.resize(int(written));
    }
    BCryptDestroyKey(handle);
    if (!NT_SUCCESS(status)) {
        output.clear();
        return false;
    }
    return true;
}

} // namespace

const char *backendName()
{
    return "Windows CNG";
}

QByteArray randomBytes(int count)
{
    QByteArray bytes(count, '\0');
    if (count <= 0)
        return bytes;
    if (!NT_SUCCESS(BCryptGenRandom(nullptr, out(bytes), ULONG(count),
                                    BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        return QByteArray();
    return bytes;
}

QByteArray hmacSha256(const QByteArray &key, const QByteArray &data)
{
    if (!hmacProvider())
        return QByteArray();
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (!NT_SUCCESS(BCryptCreateHash(hmacProvider(), &hash, nullptr, 0, in(key),
                                     ULONG(key.size()), 0)))
        return QByteArray();
    QByteArray digest(32, '\0');
    bool ok = true;
    if (!data.isEmpty())
        ok = NT_SUCCESS(BCryptHashData(hash, in(data), ULONG(data.size()), 0));
    ok = ok && NT_SUCCESS(BCryptFinishHash(hash, out(digest), ULONG(digest.size()), 0));
    BCryptDestroyHash(hash);
    return ok ? digest : QByteArray();
}

QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt,
                        int iterations, int length)
{
    if (!hmacProvider() || iterations <= 0 || length <= 0)
        return QByteArray();
    QByteArray key(length, '\0');
    if (!NT_SUCCESS(BCryptDeriveKeyPBKDF2(hmacProvider(), in(password), ULONG(password.size()),
                                          in(salt), ULONG(salt.size()),
                                          ULONGLONG(iterations), out(key), ULONG(length), 0)))
        return QByteArray();
    return key;
}

bool aes256CbcEncrypt(const QByteArray &key, const QByteArray &iv,
                      const QByteArray &plaintext, QByteArray &ciphertext)
{
    return aesCrypt(true, key, iv, plaintext, ciphertext);
}

bool aes256CbcDecrypt(const QByteArray &key, const QByteArray &iv,
                      const QByteArray &ciphertext, QByteArray &plaintext)
{
    return aesCrypt(false, key, iv, ciphertext, plaintext);
}

// ---------------------------------------------------------------- DH

struct DhKeyPair::Impl
{
    BCRYPT_KEY_HANDLE key = nullptr;
    QByteArray publicY;

    ~Impl() { reset(); }
    void reset()
    {
        if (key)
            BCryptDestroyKey(key);
        key = nullptr;
        publicY.clear();
    }
};

DhKeyPair::DhKeyPair() : d(new Impl) {}
DhKeyPair::~DhKeyPair() = default;

bool DhKeyPair::generate()
{
    d->reset();
    if (!dhProvider())
        return false;

    BCRYPT_KEY_HANDLE handle = nullptr;
    if (!NT_SUCCESS(BCryptGenerateKeyPair(dhProvider(), &handle, kDhGroup14Bytes * 8, 0)))
        return false;

    // Our group instead of one CNG would pick: header, then p and g.
    BCRYPT_DH_PARAMETER_HEADER header;
    header.cbLength = ULONG(sizeof(header) + 2 * kDhGroup14Bytes);
    header.dwMagic = BCRYPT_DH_PARAMETERS_MAGIC;
    header.cbKeyLength = kDhGroup14Bytes;
    QByteArray parameters(reinterpret_cast<const char *>(&header), sizeof(header));
    parameters += dhGroup14Prime();
    parameters += paddedGenerator();

    bool ok = NT_SUCCESS(BCryptSetProperty(handle, BCRYPT_DH_PARAMETERS, out(parameters),
                                           ULONG(parameters.size()), 0))
           && NT_SUCCESS(BCryptFinalizeKeyPair(handle, 0));

    QByteArray blob;
    if (ok) {
        ULONG size = 0;
        ok = NT_SUCCESS(BCryptExportKey(handle, nullptr, BCRYPT_DH_PUBLIC_BLOB, nullptr, 0,
                                        &size, 0));
        if (ok) {
            blob.resize(int(size));
            ok = NT_SUCCESS(BCryptExportKey(handle, nullptr, BCRYPT_DH_PUBLIC_BLOB, out(blob),
                                            size, &size, 0));
            blob.resize(int(size));
        }
    }
    // Layout: BCRYPT_DH_KEY_BLOB, then p, g and y, each cbKey bytes.
    const int offset = int(sizeof(BCRYPT_DH_KEY_BLOB)) + 2 * kDhGroup14Bytes;
    if (ok && blob.size() >= offset + kDhGroup14Bytes) {
        const auto *keyBlob = reinterpret_cast<const BCRYPT_DH_KEY_BLOB *>(blob.constData());
        ok = keyBlob->dwMagic == BCRYPT_DH_PUBLIC_MAGIC && keyBlob->cbKey == ULONG(kDhGroup14Bytes)
             && blob.mid(int(sizeof(BCRYPT_DH_KEY_BLOB)), kDhGroup14Bytes) == dhGroup14Prime();
    } else {
        ok = false;
    }
    if (!ok) {
        BCryptDestroyKey(handle);
        return false;
    }
    d->key = handle;
    d->publicY = blob.mid(offset, kDhGroup14Bytes);
    return true;
}

bool DhKeyPair::importForTest(const QByteArray &privateX, const QByteArray &publicY)
{
    d->reset();
    if (!dhProvider())
        return false;
    const QByteArray x = leftPad(privateX, kDhGroup14Bytes);
    const QByteArray y = leftPad(publicY, kDhGroup14Bytes);
    if (x.isEmpty() || y.isEmpty())
        return false;
    QByteArray blob = dhKeyBlob(BCRYPT_DH_PRIVATE_MAGIC, y, x);
    BCRYPT_KEY_HANDLE handle = nullptr;
    if (!NT_SUCCESS(BCryptImportKeyPair(dhProvider(), nullptr, BCRYPT_DH_PRIVATE_BLOB, &handle,
                                        out(blob), ULONG(blob.size()), 0)))
        return false;
    d->key = handle;
    d->publicY = y;
    return true;
}

bool DhKeyPair::isValid() const
{
    return d->key != nullptr;
}

QByteArray DhKeyPair::publicValue() const
{
    return d->publicY;
}

bool DhKeyPair::agree(const QByteArray &peerY, QByteArray &secret) const
{
    secret.clear();
    if (!d->key)
        return false;
    const QByteArray y = leftPad(peerY, kDhGroup14Bytes);
    if (y.isEmpty())
        return false;

    QByteArray blob = dhKeyBlob(BCRYPT_DH_PUBLIC_MAGIC, y);
    BCRYPT_KEY_HANDLE peer = nullptr;
    if (!NT_SUCCESS(BCryptImportKeyPair(dhProvider(), nullptr, BCRYPT_DH_PUBLIC_BLOB, &peer,
                                        out(blob), ULONG(blob.size()), 0)))
        return false;

    BCRYPT_SECRET_HANDLE agreed = nullptr;
    bool ok = NT_SUCCESS(BCryptSecretAgreement(d->key, peer, &agreed, 0));
    QByteArray raw;
    if (ok) {
        ULONG size = 0;
        ok = NT_SUCCESS(BCryptDeriveKey(agreed, BCRYPT_KDF_RAW_SECRET, nullptr, nullptr, 0,
                                        &size, 0));
        if (ok) {
            raw.resize(int(size));
            ok = NT_SUCCESS(BCryptDeriveKey(agreed, BCRYPT_KDF_RAW_SECRET, nullptr, out(raw),
                                            size, &size, 0));
            raw.resize(int(size));
        }
        BCryptDestroySecret(agreed);
    }
    BCryptDestroyKey(peer);
    if (!ok || raw.isEmpty())
        return false;

    // CNG hands the raw secret over little-endian; Python's exchange()
    // returns it big-endian, padded to the size of p.
    std::reverse(raw.begin(), raw.end());
    secret = leftPad(raw, kDhGroup14Bytes);
    return !secret.isEmpty();
}

} // namespace crypto
