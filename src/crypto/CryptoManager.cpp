// CryptoManager.cpp - see CryptoManager.h for the format.
#include "CryptoManager.h"

#include "DhPublicKey.h"

#include <QStringDecoder>

namespace {

const QByteArray kSessionInfo("vara-chat-session-key");
const QByteArray kHmacInfo("hmac");
const QByteArray kEncryptionContext("encryption");
const QByteArray kHmacContext("hmac");
const QString kOpen = QStringLiteral("<ENC>");
const QString kClose = QStringLiteral("</ENC>");

constexpr char kFlagPassword = 0x00;
constexpr char kFlagSession = 0x01;
constexpr int kSaltSize = 16;
constexpr int kIvSize = 16;
constexpr int kMacSize = 32;

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    return diff == 0;
}

} // namespace

CryptoManager::CryptoManager() = default;
CryptoManager::~CryptoManager() = default;

void CryptoManager::setPassword(const QString &password)
{
    const QString trimmed = password.trimmed();
    if (trimmed.isEmpty()) {
        m_password.clear();
        m_enabled = false;
        return;
    }
    m_password = trimmed.toUtf8();
}

bool CryptoManager::enable()
{
    if (m_password.isEmpty())
        return false;
    m_enabled = true;
    return true;
}

void CryptoManager::resetSession()
{
    m_sessionKey.clear();
    m_dh.reset();
    m_dhEstablished = false;
}

QString CryptoManager::generateDhKeyPair()
{
    auto pair = std::make_unique<crypto::DhKeyPair>();
    if (!pair->generate())
        return QString();
    m_dh = std::move(pair);
    return publicKey();
}

bool CryptoManager::importDhKeyPairForTest(const QByteArray &privateX, const QByteArray &publicY)
{
    auto pair = std::make_unique<crypto::DhKeyPair>();
    if (!pair->importForTest(privateX, publicY))
        return false;
    m_dh = std::move(pair);
    return true;
}

bool CryptoManager::hasDhKeyPair() const
{
    return m_dh && m_dh->isValid();
}

QString CryptoManager::publicKey() const
{
    return hasDhKeyPair() ? crypto::encodeDhPublicKey(m_dh->publicValue()) : QString();
}

bool CryptoManager::computeSharedSecret(const QString &peerPublicKey, QString *error)
{
    if (!hasDhKeyPair()) {
        if (error)
            *error = QStringLiteral("no local key pair");
        return false;
    }
    const QByteArray peerY = crypto::decodeDhPublicKey(peerPublicKey, error);
    if (peerY.isEmpty())
        return false;
    QByteArray secret;
    if (!m_dh->agree(peerY, secret)) {
        if (error)
            *error = QStringLiteral("key agreement failed");
        return false;
    }
    const QByteArray key = hkdf(secret, kSessionInfo);
    secret.fill('\0');
    if (key.size() != 32) {
        if (error)
            *error = QStringLiteral("key derivation failed");
        return false;
    }
    m_sessionKey = key;
    m_dhEstablished = true;
    return true;
}

QByteArray CryptoManager::deriveKey(const QByteArray &password, const QByteArray &salt,
                                    const QByteArray &context)
{
    // Python: PBKDF2HMAC(SHA256, length=32, salt=salt + context, 100000)
    return crypto::pbkdf2Sha256(password, salt + context, kPbkdf2Iterations, 32);
}

QByteArray CryptoManager::hkdf(const QByteArray &ikm, const QByteArray &info, int length)
{
    // RFC 5869 with Python's salt=None, i.e. a zero salt of hash length.
    const QByteArray prk = crypto::hmacSha256(QByteArray(32, '\0'), ikm);
    if (prk.size() != 32)
        return QByteArray();
    QByteArray okm, block;
    for (char counter = 1; okm.size() < length; ++counter) {
        block = crypto::hmacSha256(prk, block + info + QByteArray(1, counter));
        if (block.size() != 32)
            return QByteArray();
        okm += block;
    }
    return okm.left(length);
}

bool CryptoManager::isEncryptedMessage(const QString &message)
{
    return message.startsWith(kOpen) && message.endsWith(kClose);
}

QString CryptoManager::encrypt(const QString &plaintext, bool *ok) const
{
    if (ok)
        *ok = true;
    if (!m_enabled || (m_password.isEmpty() && m_sessionKey.isEmpty()))
        return plaintext;

    const bool useSession = m_dhEstablished && !m_sessionKey.isEmpty();
    QByteArray salt, encryptionKey, macKey;
    if (useSession) {
        encryptionKey = m_sessionKey;
        macKey = hkdf(m_sessionKey, kHmacInfo);
    } else {
        salt = crypto::randomBytes(kSaltSize);
        if (salt.size() == kSaltSize) {
            encryptionKey = deriveKey(m_password, salt, kEncryptionContext);
            macKey = deriveKey(m_password, salt, kHmacContext);
        }
    }
    const QByteArray iv = crypto::randomBytes(kIvSize);
    QByteArray ciphertext;
    if (encryptionKey.size() != 32 || macKey.size() != 32 || iv.size() != kIvSize
        || !crypto::aes256CbcEncrypt(encryptionKey, iv, plaintext.toUtf8(), ciphertext)) {
        if (ok)
            *ok = false;
        return QString();
    }
    const QByteArray mac = crypto::hmacSha256(macKey, iv + ciphertext);
    if (mac.size() != kMacSize) {
        if (ok)
            *ok = false;
        return QString();
    }
    QByteArray packet;
    if (useSession)
        packet = QByteArray(1, kFlagSession) + iv + ciphertext + mac;
    else
        packet = QByteArray(1, kFlagPassword) + salt + iv + ciphertext + mac;
    return kOpen + QString::fromLatin1(packet.toBase64()) + kClose;
}

CryptoManager::Decrypted CryptoManager::decrypt(const QString &message) const
{
    Decrypted result;
    result.text = message;
    if (!m_enabled) {
        result.status = isEncryptedMessage(message) ? DecryptStatus::Disabled
                                                    : DecryptStatus::NotEncrypted;
        return result;
    }
    if (!isEncryptedMessage(message)) {
        result.status = DecryptStatus::NotEncrypted;
        return result;
    }
    if (m_password.isEmpty() && m_sessionKey.isEmpty()) {
        result.status = DecryptStatus::NoKey;
        return result;
    }

    const QString encoded = message.mid(kOpen.size(), message.size() - kOpen.size() - kClose.size());
    const auto decoded = QByteArray::fromBase64Encoding(encoded.toLatin1(),
                                                        QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || (*decoded).isEmpty()) {
        result.status = DecryptStatus::Failed;
        return result;
    }
    const QByteArray packet = *decoded;

    QByteArray iv, ciphertext, mac, encryptionKey, macKey;
    if (packet.at(0) == kFlagSession) {
        if (!m_dhEstablished || m_sessionKey.isEmpty()) {
            result.status = DecryptStatus::NoSession;
            return result;
        }
        if (packet.size() < 1 + kIvSize + 16 + kMacSize) {
            result.status = DecryptStatus::Failed;
            return result;
        }
        iv = packet.mid(1, kIvSize);
        ciphertext = packet.mid(1 + kIvSize, packet.size() - 1 - kIvSize - kMacSize);
        mac = packet.right(kMacSize);
        encryptionKey = m_sessionKey;
        macKey = hkdf(m_sessionKey, kHmacInfo);
    } else {
        // Any other flag is read as a password message, as Python does.
        if (m_password.isEmpty()) {
            result.status = DecryptStatus::Failed;
            return result;
        }
        if (packet.size() < 1 + kSaltSize + kIvSize + 16 + kMacSize) {
            result.status = DecryptStatus::Failed;
            return result;
        }
        const QByteArray salt = packet.mid(1, kSaltSize);
        iv = packet.mid(1 + kSaltSize, kIvSize);
        ciphertext = packet.mid(1 + kSaltSize + kIvSize,
                                packet.size() - 1 - kSaltSize - kIvSize - kMacSize);
        mac = packet.right(kMacSize);
        encryptionKey = deriveKey(m_password, salt, kEncryptionContext);
        macKey = deriveKey(m_password, salt, kHmacContext);
    }

    const QByteArray expected = crypto::hmacSha256(macKey, iv + ciphertext);
    if (expected.size() != kMacSize) {
        result.status = DecryptStatus::Failed;
        return result;
    }
    if (!constantTimeEquals(mac, expected)) {
        result.status = DecryptStatus::BadHmac;
        return result;
    }

    QByteArray plaintext;
    if (!crypto::aes256CbcDecrypt(encryptionKey, iv, ciphertext, plaintext)) {
        result.status = DecryptStatus::Failed;
        return result;
    }
    // Python decodes strictly: invalid UTF-8 is an error, not replaced.
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString text = decoder(plaintext);
    if (decoder.hasError()) {
        result.status = DecryptStatus::Failed;
        return result;
    }
    result.status = DecryptStatus::Ok;
    result.text = text;
    return result;
}
