// Chime.cpp
#include "Chime.h"

#include <QByteArray>
#include <QtEndian>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <mmsystem.h>
#endif

namespace {

// 16-bit mono PCM WAV: E6 then A6, 120 ms each, soft attack and decay.
QByteArray makeChime()
{
    constexpr int rate = 22050;
    constexpr double pi = 3.14159265358979323846;
    const double tones[] = {1318.5, 1760.0};
    const int noteSamples = rate * 12 / 100;
    QByteArray pcm;
    pcm.reserve(2 * noteSamples * 2);
    for (double frequency : tones) {
        for (int i = 0; i < noteSamples; ++i) {
            const double t = double(i) / rate;
            const double attack = std::min(1.0, i / (rate * 0.005));
            const double decay = std::exp(-t * 18.0);
            const double sample = 0.35 * attack * decay
                                  * (std::sin(2 * pi * frequency * t)
                                     + 0.25 * std::sin(4 * pi * frequency * t));
            const qint16 value = qint16(std::lround(sample * 32767.0 / 1.25));
            char bytes[2];
            qToLittleEndian(value, bytes);
            pcm.append(bytes, 2);
        }
    }
    auto le32 = [](quint32 v) { char b[4]; qToLittleEndian(v, b); return QByteArray(b, 4); };
    auto le16 = [](quint16 v) { char b[2]; qToLittleEndian(v, b); return QByteArray(b, 2); };
    QByteArray wav("RIFF");
    wav += le32(quint32(36 + pcm.size()));
    wav += "WAVEfmt ";
    wav += le32(16) + le16(1) + le16(1) + le32(rate) + le32(rate * 2) + le16(2) + le16(16);
    wav += "data";
    wav += le32(quint32(pcm.size()));
    wav += pcm;
    return wav;
}

} // namespace

namespace chime {

void play()
{
#ifdef Q_OS_WIN
    // PlaySound reads the buffer while it plays asynchronously: it must
    // outlive the call, hence static.
    static const QByteArray wav = makeChime();
    PlaySoundW(reinterpret_cast<LPCWSTR>(wav.constData()), nullptr,
               SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
    static const QByteArray wav = makeChime();
    Q_UNUSED(wav);
#endif
}

} // namespace chime
