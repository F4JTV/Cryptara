// DhPublicKey.h - the Diffie-Hellman public key as it travels on the air.
//
// The Python version sends its key as the base64 of a PEM "PUBLIC KEY"
// (SubjectPublicKeyInfo).  Recent versions of Python's cryptography library
// write it in the X9.42 form (OID dhpublicnumber, parameters p, g, q);
// older ones in the PKCS#3 form (OID dhKeyAgreement, parameters p, g).
// Both are read; the X9.42 form is the one written, byte for byte what a
// current Python station sends.
#pragma once

#include <QByteArray>
#include <QString>

namespace crypto {

// base64(PEM(SubjectPublicKeyInfo)) for the public value y (big-endian).
QString encodeDhPublicKey(const QByteArray &y);

// The public value y (big-endian, 256 bytes) carried by a key received
// from the other station, or an empty array with *error set when the key
// is malformed, uses other parameters than group 14, or is out of range.
QByteArray decodeDhPublicKey(const QString &encoded, QString *error = nullptr);

} // namespace crypto
