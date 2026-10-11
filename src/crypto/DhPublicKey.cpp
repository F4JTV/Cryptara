// DhPublicKey.cpp - DER/PEM encoding of the Diffie-Hellman public key.
#include "DhPublicKey.h"

#include "CryptoBackend.h"

namespace crypto {

namespace {

// 1.2.840.10046.2.1 (ANSI X9.42 dhpublicnumber)
const QByteArray kOidX942 = QByteArray::fromHex("2a8648ce3e0201");
// 1.2.840.113549.1.3.1 (PKCS#3 dhKeyAgreement)
const QByteArray kOidPkcs3 = QByteArray::fromHex("2a864886f70d010301");

constexpr char kTagInteger = 0x02;
constexpr char kTagBitString = 0x03;
constexpr char kTagOid = 0x06;
constexpr char kTagSequence = 0x30;

QByteArray derLength(int length)
{
    QByteArray out;
    if (length < 0x80) {
        out.append(char(length));
    } else if (length < 0x100) {
        out.append(char(0x81));
        out.append(char(length));
    } else {
        out.append(char(0x82));
        out.append(char((length >> 8) & 0xff));
        out.append(char(length & 0xff));
    }
    return out;
}

QByteArray der(char tag, const QByteArray &content)
{
    return QByteArray(1, tag) + derLength(content.size()) + content;
}

// A non-negative big-endian integer, in its minimal DER form.
QByteArray derInteger(const QByteArray &bigEndian)
{
    int start = 0;
    while (start < bigEndian.size() - 1 && bigEndian.at(start) == '\0')
        ++start;
    QByteArray content = bigEndian.mid(start);
    if (content.isEmpty())
        content = QByteArray(1, '\0');
    if (static_cast<unsigned char>(content.at(0)) & 0x80)
        content.prepend('\0');
    return der(kTagInteger, content);
}

struct Tlv
{
    char tag = 0;
    QByteArray value;
};

bool readTlv(const QByteArray &data, int &pos, Tlv &tlv)
{
    if (pos + 2 > data.size())
        return false;
    tlv.tag = data.at(pos++);
    int length = static_cast<unsigned char>(data.at(pos++));
    if (length & 0x80) {
        const int count = length & 0x7f;
        if (count == 0 || count > 3 || pos + count > data.size())
            return false;
        length = 0;
        for (int i = 0; i < count; ++i)
            length = (length << 8) | static_cast<unsigned char>(data.at(pos++));
    }
    if (length > data.size() - pos)
        return false;
    tlv.value = data.mid(pos, length);
    pos += length;
    return true;
}

// The value of a DER INTEGER, big-endian without the sign byte, or empty
// if it is negative or malformed.
QByteArray integerValue(const Tlv &tlv)
{
    if (tlv.tag != kTagInteger || tlv.value.isEmpty())
        return QByteArray();
    if (static_cast<unsigned char>(tlv.value.at(0)) & 0x80)
        return QByteArray();
    QByteArray value = tlv.value;
    while (value.size() > 1 && value.at(0) == '\0')
        value.remove(0, 1);
    return value;
}

QByteArray leftPad(const QByteArray &value, int size)
{
    if (value.size() > size)
        return QByteArray();
    return QByteArray(size - value.size(), '\0') + value;
}

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

} // namespace

QString encodeDhPublicKey(const QByteArray &y)
{
    const QByteArray parameters = der(kTagSequence,
                                      derInteger(dhGroup14Prime())
                                      + derInteger(QByteArray(1, 2))
                                      + derInteger(dhGroup14Subprime()));
    const QByteArray algorithm = der(kTagSequence, der(kTagOid, kOidX942) + parameters);
    const QByteArray key = der(kTagBitString, QByteArray(1, '\0') + derInteger(y));
    const QByteArray spki = der(kTagSequence, algorithm + key);

    // PEM as Python writes it: 64 characters a line, final newline.
    const QByteArray body = spki.toBase64();
    QByteArray pem("-----BEGIN PUBLIC KEY-----\n");
    for (int i = 0; i < body.size(); i += 64)
        pem += body.mid(i, 64) + '\n';
    pem += "-----END PUBLIC KEY-----\n";
    return QString::fromLatin1(pem.toBase64());
}

QByteArray decodeDhPublicKey(const QString &encoded, QString *error)
{
    const auto outer = QByteArray::fromBase64Encoding(encoded.trimmed().toLatin1(),
                                                      QByteArray::AbortOnBase64DecodingErrors);
    if (!outer) {
        fail(error, QStringLiteral("not base64"));
        return QByteArray();
    }
    const QByteArray pem = *outer;
    static const QByteArray begin("-----BEGIN PUBLIC KEY-----");
    static const QByteArray end("-----END PUBLIC KEY-----");
    const int b = pem.indexOf(begin);
    const int e = pem.indexOf(end);
    if (b < 0 || e < b) {
        fail(error, QStringLiteral("not a PEM public key"));
        return QByteArray();
    }
    QByteArray body = pem.mid(b + begin.size(), e - b - begin.size());
    body = body.simplified().replace(' ', QByteArray());
    const auto inner = QByteArray::fromBase64Encoding(body, QByteArray::AbortOnBase64DecodingErrors);
    if (!inner) {
        fail(error, QStringLiteral("PEM body is not base64"));
        return QByteArray();
    }
    const QByteArray derBytes = *inner;

    // SubjectPublicKeyInfo ::= SEQUENCE { algorithm, subjectPublicKey }
    int pos = 0;
    Tlv spki;
    if (!readTlv(derBytes, pos, spki) || spki.tag != kTagSequence) {
        fail(error, QStringLiteral("malformed SubjectPublicKeyInfo"));
        return QByteArray();
    }
    pos = 0;
    Tlv algorithm, bitString;
    if (!readTlv(spki.value, pos, algorithm) || algorithm.tag != kTagSequence
        || !readTlv(spki.value, pos, bitString) || bitString.tag != kTagBitString) {
        fail(error, QStringLiteral("malformed SubjectPublicKeyInfo"));
        return QByteArray();
    }

    // AlgorithmIdentifier ::= SEQUENCE { OID, parameters }
    pos = 0;
    Tlv oid, parameters;
    if (!readTlv(algorithm.value, pos, oid) || oid.tag != kTagOid
        || !readTlv(algorithm.value, pos, parameters) || parameters.tag != kTagSequence) {
        fail(error, QStringLiteral("malformed algorithm identifier"));
        return QByteArray();
    }
    if (oid.value != kOidX942 && oid.value != kOidPkcs3) {
        fail(error, QStringLiteral("not a Diffie-Hellman key"));
        return QByteArray();
    }
    // Both forms start with p then g.
    pos = 0;
    Tlv pTlv, gTlv;
    if (!readTlv(parameters.value, pos, pTlv) || !readTlv(parameters.value, pos, gTlv)) {
        fail(error, QStringLiteral("malformed DH parameters"));
        return QByteArray();
    }
    const QByteArray p = integerValue(pTlv);
    const QByteArray g = integerValue(gTlv);
    if (p != dhGroup14Prime() || g != QByteArray(1, 2)) {
        fail(error, QStringLiteral("DH parameters are not RFC 3526 group 14"));
        return QByteArray();
    }

    // subjectPublicKey: BIT STRING (no unused bits) holding INTEGER y.
    if (bitString.value.isEmpty() || bitString.value.at(0) != '\0') {
        fail(error, QStringLiteral("malformed public key bit string"));
        return QByteArray();
    }
    const QByteArray keyBytes = bitString.value.mid(1);
    pos = 0;
    Tlv yTlv;
    if (!readTlv(keyBytes, pos, yTlv)) {
        fail(error, QStringLiteral("malformed public value"));
        return QByteArray();
    }
    const QByteArray y = leftPad(integerValue(yTlv), kDhGroup14Bytes);
    if (y.isEmpty()) {
        fail(error, QStringLiteral("malformed public value"));
        return QByteArray();
    }

    // 1 < y < p - 1: rejects the degenerate values that would force the
    // shared secret to 0, 1 or p - 1.
    QByteArray two(kDhGroup14Bytes, '\0');
    two[kDhGroup14Bytes - 1] = 2;
    QByteArray pMinusOne = dhGroup14Prime();
    pMinusOne[kDhGroup14Bytes - 1] = char(pMinusOne.at(kDhGroup14Bytes - 1) - 1);
    if (y < two || !(y < pMinusOne)) {
        fail(error, QStringLiteral("public value out of range"));
        return QByteArray();
    }
    return y;
}

} // namespace crypto
