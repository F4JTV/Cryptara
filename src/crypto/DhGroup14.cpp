// DhGroup14.cpp - the RFC 3526 group 14 parameters, shared by every backend.
#include "CryptoBackend.h"

namespace crypto {

// RFC 3526, section 3: the 2048-bit MODP group, generator 2.  The Python
// version hard-codes the same prime, so both stations always agree on it.
static const char kPrimeHex[] =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
    "15728E5A8AACAA68FFFFFFFFFFFFFFFF";

const QByteArray &dhGroup14Prime()
{
    static const QByteArray p = QByteArray::fromHex(kPrimeHex);
    return p;
}

const QByteArray &dhGroup14Subprime()
{
    // q = (p - 1) / 2: p is odd, so this is a right shift by one bit.
    static const QByteArray q = [] {
        const QByteArray &p = dhGroup14Prime();
        QByteArray out(p.size(), '\0');
        unsigned carry = 0;
        for (int i = 0; i < p.size(); ++i) {
            const unsigned byte = static_cast<unsigned char>(p.at(i));
            out[i] = static_cast<char>((byte >> 1) | (carry << 7));
            carry = byte & 1u;
        }
        return out;
    }();
    return q;
}

} // namespace crypto
