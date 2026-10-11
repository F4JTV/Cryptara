// CryptoBackendOpenSsl.cpp - CryptoBackend over OpenSSL 3.
//
// Development only: this is what lets the program and its tests build and
// run on a Linux machine.  The Windows build uses CryptoBackendCng.cpp and
// does not need OpenSSL.
#include "CryptoBackend.h"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace crypto {

namespace {

const unsigned char *u(const QByteArray &bytes)
{
    return reinterpret_cast<const unsigned char *>(bytes.constData());
}

struct BnDeleter { void operator()(BIGNUM *bn) const { BN_clear_free(bn); } };
struct CtxDeleter { void operator()(BN_CTX *ctx) const { BN_CTX_free(ctx); } };
using Bn = std::unique_ptr<BIGNUM, BnDeleter>;
using BnCtx = std::unique_ptr<BN_CTX, CtxDeleter>;

Bn toBn(const QByteArray &bytes)
{
    return Bn(BN_bin2bn(u(bytes), bytes.size(), nullptr));
}

QByteArray fromBn(const BIGNUM *bn, int width)
{
    QByteArray bytes(width, '\0');
    if (BN_bn2binpad(bn, reinterpret_cast<unsigned char *>(bytes.data()), width) != width)
        return QByteArray();
    return bytes;
}

bool aesCrypt(bool encrypt, const QByteArray &key, const QByteArray &iv,
              const QByteArray &input, QByteArray &output)
{
    if (key.size() != 32 || iv.size() != 16)
        return false;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;
    output.resize(input.size() + 16);
    int n1 = 0, n2 = 0;
    bool ok = EVP_CipherInit_ex(ctx, EVP_aes_256_cbc(), nullptr, u(key), u(iv), encrypt ? 1 : 0) == 1
           && EVP_CipherUpdate(ctx, reinterpret_cast<unsigned char *>(output.data()), &n1,
                               u(input), input.size()) == 1
           && EVP_CipherFinal_ex(ctx, reinterpret_cast<unsigned char *>(output.data()) + n1, &n2) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        output.clear();
        return false;
    }
    output.resize(n1 + n2);
    return true;
}

} // namespace

const char *backendName()
{
    return "OpenSSL (development build)";
}

QByteArray randomBytes(int count)
{
    QByteArray bytes(count, '\0');
    if (count > 0 && RAND_bytes(reinterpret_cast<unsigned char *>(bytes.data()), count) != 1)
        return QByteArray();
    return bytes;
}

QByteArray hmacSha256(const QByteArray &key, const QByteArray &data)
{
    QByteArray digest(32, '\0');
    size_t size = 0;
    static const unsigned char empty = 0;
    if (!EVP_Q_mac(nullptr, "HMAC", nullptr, "SHA256", nullptr,
                   key.isEmpty() ? &empty : u(key), size_t(key.size()),
                   u(data), size_t(data.size()),
                   reinterpret_cast<unsigned char *>(digest.data()), size_t(digest.size()), &size)
        || size != 32)
        return QByteArray();
    return digest;
}

QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt,
                        int iterations, int length)
{
    QByteArray key(length, '\0');
    if (PKCS5_PBKDF2_HMAC(password.constData(), password.size(), u(salt), salt.size(),
                          iterations, EVP_sha256(), length,
                          reinterpret_cast<unsigned char *>(key.data())) != 1)
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
    if (ciphertext.isEmpty() || ciphertext.size() % 16 != 0)
        return false;
    return aesCrypt(false, key, iv, ciphertext, plaintext);
}

struct DhKeyPair::Impl
{
    QByteArray x;
    QByteArray y;
};

DhKeyPair::DhKeyPair() : d(new Impl) {}
DhKeyPair::~DhKeyPair() = default;

bool DhKeyPair::generate()
{
    d->x.clear();
    d->y.clear();
    Bn p = toBn(dhGroup14Prime());
    Bn g(BN_new());
    Bn x(BN_new());
    Bn y(BN_new());
    Bn range(BN_dup(p.get()));
    BnCtx ctx(BN_CTX_new());
    if (!p || !g || !x || !y || !range || !ctx || !BN_set_word(g.get(), 2))
        return false;
    // x uniform in [2, p - 2].
    if (!BN_sub_word(range.get(), 3) || !BN_priv_rand_range(x.get(), range.get())
        || !BN_add_word(x.get(), 2))
        return false;
    if (!BN_mod_exp(y.get(), g.get(), x.get(), p.get(), ctx.get()))
        return false;
    d->x = fromBn(x.get(), kDhGroup14Bytes);
    d->y = fromBn(y.get(), kDhGroup14Bytes);
    return !d->x.isEmpty() && !d->y.isEmpty();
}

bool DhKeyPair::importForTest(const QByteArray &privateX, const QByteArray &publicY)
{
    Bn x = toBn(privateX);
    Bn y = toBn(publicY);
    if (!x || !y)
        return false;
    d->x = fromBn(x.get(), kDhGroup14Bytes);
    d->y = fromBn(y.get(), kDhGroup14Bytes);
    return !d->x.isEmpty() && !d->y.isEmpty();
}

bool DhKeyPair::isValid() const
{
    return !d->x.isEmpty();
}

QByteArray DhKeyPair::publicValue() const
{
    return d->y;
}

bool DhKeyPair::agree(const QByteArray &peerY, QByteArray &secret) const
{
    secret.clear();
    if (d->x.isEmpty())
        return false;
    Bn p = toBn(dhGroup14Prime());
    Bn x = toBn(d->x);
    Bn peer = toBn(peerY);
    Bn s(BN_new());
    BnCtx ctx(BN_CTX_new());
    if (!p || !x || !peer || !s || !ctx)
        return false;
    if (!BN_mod_exp(s.get(), peer.get(), x.get(), p.get(), ctx.get()))
        return false;
    secret = fromBn(s.get(), kDhGroup14Bytes);
    return !secret.isEmpty();
}

} // namespace crypto
