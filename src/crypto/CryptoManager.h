// CryptoManager.h - CRYPTARA's message encryption, compatible on the air
// with the Python version (1.x).
//
// Wire format, unchanged:
//   <ENC>base64(flag | [salt] | iv | ciphertext | hmac)</ENC>
//   flag 0x00: key from the shared password, PBKDF2-HMAC-SHA256, 100 000
//              iterations, salt = 16 random bytes + context ("encryption"
//              or "hmac"); the 16-byte salt travels with the message.
//   flag 0x01: key from the Diffie-Hellman session: the AES key is
//              HKDF-SHA256(secret, info "vara-chat-session-key"), the HMAC
//              key HKDF-SHA256(that key, info "hmac").
//   AES-256-CBC with PKCS#7 padding, random 16-byte IV, HMAC-SHA256 over
//   iv | ciphertext.
#pragma once

#include "CryptoBackend.h"

#include <QByteArray>
#include <QString>
#include <memory>

class CryptoManager
{
public:
    static constexpr int kPbkdf2Iterations = 100000;

    enum class DecryptStatus {
        NotEncrypted,   // not an <ENC> message: text returned as is
        Disabled,       // encryption is off on our side: text returned as is
        Ok,
        NoKey,          // neither password nor session key configured
        NoSession,      // session-key message but no DH session here
        BadHmac,        // wrong password or corrupted message
        Failed          // malformed message or decryption error
    };

    struct Decrypted
    {
        DecryptStatus status = DecryptStatus::NotEncrypted;
        QString text;   // the plaintext, or the input when not decrypted
        bool ok() const { return status == DecryptStatus::Ok; }
    };

    CryptoManager();
    ~CryptoManager();

    // The shared password, trimmed of surrounding blanks as Python does.
    // An empty password clears it and turns encryption off.
    void setPassword(const QString &password);
    bool hasPassword() const { return !m_password.isEmpty(); }

    // On only when a password is set, as in Python.
    bool enable();
    void disable() { m_enabled = false; }
    bool isEnabled() const { return m_enabled; }

    // Forget the Diffie-Hellman key pair and session key (the password
    // stays).
    void resetSession();

    // A new key pair; returns our public key in its on-air encoding, or an
    // empty string if the system refused.
    QString generateDhKeyPair();
    bool hasDhKeyPair() const;
    QString publicKey() const;
    // Derives the session key from the other station's public key.
    bool computeSharedSecret(const QString &peerPublicKey, QString *error = nullptr);
    bool sessionEstablished() const { return m_dhEstablished; }

    // "<ENC>...</ENC>" for plaintext.  When encryption is off, plaintext
    // unchanged.  An empty string with *ok false if encryption failed:
    // nothing must then be sent.
    QString encrypt(const QString &plaintext, bool *ok = nullptr) const;
    Decrypted decrypt(const QString &message) const;

    static bool isEncryptedMessage(const QString &message);

    // Exposed for the tests.
    static QByteArray deriveKey(const QByteArray &password, const QByteArray &salt,
                                const QByteArray &context);
    static QByteArray hkdf(const QByteArray &ikm, const QByteArray &info, int length = 32);
    QByteArray sessionKey() const { return m_sessionKey; }
    // Test hook: a fixed key pair instead of a random one.
    bool importDhKeyPairForTest(const QByteArray &privateX, const QByteArray &publicY);

private:
    QByteArray m_password;      // UTF-8
    QByteArray m_sessionKey;
    bool m_enabled = false;
    std::unique_ptr<crypto::DhKeyPair> m_dh;
    bool m_dhEstablished = false;
};
