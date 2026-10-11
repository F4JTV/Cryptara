// test_crypto.cpp - the C++ cryptography against vectors produced by the
// Python version (crypto_vectors.h), so that both stay compatible on the air.
//
//   test_crypto            run the checks
//   test_crypto --emit     also print messages encrypted here, for
//                          tests/interop/check_emitted.py to decrypt with
//                          the Python code
#include "crypto/CryptoBackend.h"
#include "crypto/CryptoManager.h"
#include "crypto/DhPublicKey.h"
#include "crypto_vectors.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <cstdio>

namespace {

int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

template <size_t N>
QByteArray bytes(const char (&literal)[N])
{
    return QByteArray(literal, int(N - 1));
}

template <size_t N>
QString text(const char (&literal)[N])
{
    return QString::fromUtf8(literal, int(N - 1));
}

void testPrimitives()
{
    // RFC 4231 test case 2 for HMAC-SHA256.
    CHECK(crypto::hmacSha256("Jefe", "what do ya want for nothing?").toHex()
          == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    // RFC 7914 section 11 (PBKDF2-HMAC-SHA256, c = 1).
    CHECK(crypto::pbkdf2Sha256("passwd", "salt", 1, 64).toHex()
          == "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
             "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    // NIST SP 800-38A F.2.5, CBC-AES256 encrypt, first block (no padding
    // check: the vector is one full block, so PKCS#7 adds a second one).
    const QByteArray key = QByteArray::fromHex(
        "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    const QByteArray iv = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");
    const QByteArray plain = QByteArray::fromHex("6bc1bee22e409f96e93d7e117393172a");
    QByteArray cipher, back;
    CHECK(crypto::aes256CbcEncrypt(key, iv, plain, cipher));
    CHECK(cipher.left(16).toHex() == "f58c4c04d6e5f1ba779eabfb5f7bfbd6");
    CHECK(cipher.size() == 32);
    CHECK(crypto::aes256CbcDecrypt(key, iv, cipher, back) && back == plain);
    // A corrupted last block must not decrypt (padding check).
    QByteArray bad = cipher;
    bad[31] = char(bad.at(31) ^ 0x55);
    CHECK(!crypto::aes256CbcDecrypt(key, iv, bad, back) || back != plain);
    // Empty plaintext: one block of padding.
    CHECK(crypto::aes256CbcEncrypt(key, iv, QByteArray(), cipher) && cipher.size() == 16);
    CHECK(crypto::aes256CbcDecrypt(key, iv, cipher, back) && back.isEmpty());

    const QByteArray r1 = crypto::randomBytes(32);
    const QByteArray r2 = crypto::randomBytes(32);
    CHECK(r1.size() == 32 && r2.size() == 32 && r1 != r2);

    // Group 14: q = (p - 1) / 2.
    CHECK(crypto::dhGroup14Prime().size() == 256);
    CHECK(crypto::dhGroup14Subprime().toHex().startsWith("7fffffffffffffffe487ed5110b4611a"));
    CHECK(crypto::dhGroup14Subprime().toHex().endsWith("7fffffffffffffff"));
}

void testKeyDerivation()
{
    const QByteArray password = text(vectors::kPassword).trimmed().toUtf8();
    CHECK(CryptoManager::deriveKey(password, bytes(vectors::kSalt), "encryption")
          == bytes(vectors::kKeyEncryption));
    CHECK(CryptoManager::deriveKey(password, bytes(vectors::kSalt), "hmac")
          == bytes(vectors::kKeyHmac));
    CHECK(CryptoManager::hkdf(bytes(vectors::kSharedSecret), "vara-chat-session-key")
          == bytes(vectors::kSessionKey));
    CHECK(CryptoManager::hkdf(bytes(vectors::kSessionKey), "hmac")
          == bytes(vectors::kSessionHmacKey));
}

void testPasswordMessages()
{
    CryptoManager m;
    m.setPassword(text(vectors::kPassword));
    CHECK(m.enable());

    // Python -> C++
    const auto d = m.decrypt(text(vectors::kPasswordMessage));
    CHECK(d.ok());
    CHECK(d.text == text(vectors::kMessage));

    // C++ -> C++, and the shape Python expects.
    bool ok = false;
    const QString enc = m.encrypt(text(vectors::kMessage), &ok);
    CHECK(ok && CryptoManager::isEncryptedMessage(enc));
    const QByteArray packet = QByteArray::fromBase64(enc.mid(5, enc.size() - 11).toLatin1());
    CHECK(packet.at(0) == 0x00);
    CHECK((packet.size() - 1 - 16 - 16 - 32) % 16 == 0);
    CHECK(m.decrypt(enc).text == text(vectors::kMessage));

    // Wrong password: HMAC refused.
    CryptoManager other;
    other.setPassword("not the password");
    other.enable();
    CHECK(other.decrypt(text(vectors::kPasswordMessage)).status
          == CryptoManager::DecryptStatus::BadHmac);

    // Off: returned untouched.
    m.disable();
    CHECK(m.encrypt("plain") == "plain");
    CHECK(m.decrypt(enc).status == CryptoManager::DecryptStatus::Disabled);
    CHECK(m.decrypt(enc).text == enc);

    // Blank password: not enabled, as in Python.
    CryptoManager blank;
    blank.setPassword("   ");
    CHECK(!blank.enable());

    // Garbage.
    m.enable();
    CHECK(m.decrypt("<ENC>%%%</ENC>").status == CryptoManager::DecryptStatus::Failed);
    CHECK(m.decrypt("<ENC>AAAA</ENC>").status == CryptoManager::DecryptStatus::Failed);
    CHECK(m.decrypt("hello").status == CryptoManager::DecryptStatus::NotEncrypted);
}

void testDiffieHellman()
{
    // The public key encoding is byte for byte what Python writes.
    CHECK(crypto::encodeDhPublicKey(bytes(vectors::kStationAPublicY))
          == text(vectors::kStationAPublicKey));
    QString error;
    CHECK(crypto::decodeDhPublicKey(text(vectors::kStationAPublicKey), &error)
          == bytes(vectors::kStationAPublicY));

    // Station A's known key pair against station B's public key: the same
    // secret and session key as Python computed.
    CryptoManager a;
    a.setPassword(text(vectors::kPassword));
    a.enable();
    CHECK(a.importDhKeyPairForTest(bytes(vectors::kStationAPrivateX),
                                   bytes(vectors::kStationAPublicY)));
    CHECK(a.computeSharedSecret(text(vectors::kStationBPublicKey), &error));
    CHECK(a.sessionKey() == bytes(vectors::kSessionKey));
    const auto d = a.decrypt(text(vectors::kSessionMessage));
    CHECK(d.ok() && d.text == text(vectors::kMessage));

    // Two fresh C++ stations agree with each other.
    CryptoManager s1, s2;
    s1.setPassword("x");
    s2.setPassword("x");
    s1.enable();
    s2.enable();
    const QString k1 = s1.generateDhKeyPair();
    const QString k2 = s2.generateDhKeyPair();
    CHECK(!k1.isEmpty() && !k2.isEmpty() && k1 != k2);
    CHECK(s1.computeSharedSecret(k2) && s2.computeSharedSecret(k1));
    CHECK(s1.sessionKey() == s2.sessionKey() && s1.sessionKey().size() == 32);
    bool ok = false;
    const QString enc = s1.encrypt(text(vectors::kMessage), &ok);
    CHECK(ok);
    CHECK(QByteArray::fromBase64(enc.mid(5, enc.size() - 11).toLatin1()).at(0) == 0x01);
    CHECK(s2.decrypt(enc).text == text(vectors::kMessage));

    // A session message without a session: refused with the right status.
    CryptoManager noSession;
    noSession.setPassword("x");
    noSession.enable();
    CHECK(noSession.decrypt(enc).status == CryptoManager::DecryptStatus::NoSession);

    // The older PKCS#3 encoding (OID dhKeyAgreement, no q) is read too.
    // Built here from station A's value with the same DER rules.
    const QByteArray y = bytes(vectors::kStationAPublicY);
    auto der = [](char tag, const QByteArray &content) {
        QByteArray len;
        if (content.size() < 0x80) len.append(char(content.size()));
        else if (content.size() < 0x100) { len.append(char(0x81)); len.append(char(content.size())); }
        else { len.append(char(0x82)); len.append(char(content.size() >> 8)); len.append(char(content.size() & 0xff)); }
        return QByteArray(1, tag) + len + content;
    };
    auto integer = [&](QByteArray v) {
        while (v.size() > 1 && v.at(0) == '\0') v.remove(0, 1);
        if (static_cast<unsigned char>(v.at(0)) & 0x80) v.prepend('\0');
        return der(0x02, v);
    };
    const QByteArray params = der(0x30, integer(crypto::dhGroup14Prime()) + integer(QByteArray(1, 2)));
    const QByteArray alg = der(0x30, der(0x06, QByteArray::fromHex("2a864886f70d010301")) + params);
    const QByteArray spki = der(0x30, alg + der(0x03, QByteArray(1, '\0') + integer(y)));
    const QByteArray body = spki.toBase64();
    QByteArray pem("-----BEGIN PUBLIC KEY-----\n");
    for (int i = 0; i < body.size(); i += 64) pem += body.mid(i, 64) + '\n';
    pem += "-----END PUBLIC KEY-----\n";
    CHECK(crypto::decodeDhPublicKey(QString::fromLatin1(pem.toBase64()), &error) == y);

    // Out-of-range public values are refused: 1 and p - 1.
    QByteArray one(256, '\0');
    one[255] = 1;
    CHECK(crypto::decodeDhPublicKey(crypto::encodeDhPublicKey(one), &error).isEmpty());
    QByteArray pm1 = crypto::dhGroup14Prime();
    pm1[255] = char(pm1.at(255) - 1);
    CHECK(crypto::decodeDhPublicKey(crypto::encodeDhPublicKey(pm1), &error).isEmpty());
    // Not base64, not PEM.
    CHECK(crypto::decodeDhPublicKey("@@@", &error).isEmpty());
    CHECK(crypto::decodeDhPublicKey(QString::fromLatin1(QByteArray("hello").toBase64()), &error).isEmpty());
}

void emitForPython()
{
    // Lines read by tests/interop/check_emitted.py.
    CryptoManager m;
    m.setPassword(text(vectors::kPassword));
    m.enable();
    std::printf("PASSWORD %s\n", qPrintable(m.encrypt(text(vectors::kMessage))));

    CryptoManager a;
    a.setPassword(text(vectors::kPassword));
    a.enable();
    a.importDhKeyPairForTest(bytes(vectors::kStationAPrivateX), bytes(vectors::kStationAPublicY));
    a.computeSharedSecret(text(vectors::kStationBPublicKey));
    std::printf("SESSION %s\n", qPrintable(a.encrypt(text(vectors::kMessage))));

    CryptoManager fresh;
    std::printf("PUBKEY %s\n", qPrintable(fresh.generateDhKeyPair()));
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    std::printf("Backend: %s\n", crypto::backendName());
    QElapsedTimer timer;
    timer.start();
    testPrimitives();
    testKeyDerivation();
    testPasswordMessages();
    testDiffieHellman();
    if (app.arguments().contains(QStringLiteral("--emit")))
        emitForPython();
    // --dh-respond KEY: answer a Diffie-Hellman request from another
    // implementation (tests/interop/check_old_cryptography.py).
    const int respond = app.arguments().indexOf(QStringLiteral("--dh-respond"));
    if (respond >= 0 && respond + 1 < app.arguments().size()) {
        CryptoManager m;
        m.setPassword(text(vectors::kPassword));
        m.enable();
        const QString ours = m.generateDhKeyPair();
        QString error;
        if (!m.computeSharedSecret(app.arguments().at(respond + 1), &error)) {
            std::printf("DHERROR %s\n", qPrintable(error));
            return 1;
        }
        std::printf("PUBKEY %s\nSESSIONKEY %s\nSESSION %s\n", qPrintable(ours),
                    m.sessionKey().toHex().constData(), qPrintable(m.encrypt(text(vectors::kMessage))));
    }
    std::printf("%s (%d failure(s), %lld ms)\n", g_failures ? "FAILED" : "PASSED", g_failures,
                static_cast<long long>(timer.elapsed()));
    return g_failures ? 1 : 0;
}
