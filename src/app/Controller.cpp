// Controller.cpp - see Controller.h.  The order of the functions follows
// MainWindow in the Python version, so the two can be read side by side.
#include "Controller.h"

#include "Chime.h"
#include "I18n.h"
#include "crypto/CryptoBackend.h"
#include "vara/VaraClient.h"
#include "vara/VaraProcess.h"
#include "version.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringDecoder>
#include <QUrl>
#include <QtEndian>

#include <algorithm>
#include <cmath>

namespace {

const QString kEof = QStringLiteral("<EOF>");
const QByteArray kEofBytes = QByteArrayLiteral("<EOF>");

constexpr int kChunkSize = 8192;             // as the Python version
constexpr int kChunkIntervalMs = 50;
constexpr qint64 kMaxFileSize = 2 * 1024 * 1024;
constexpr qint64 kLargeFile = 500 * 1024;
constexpr qint64 kMaxInflated = 256LL * 1024 * 1024;
constexpr int kBbsSendTimeoutMs = 300000;    // 5 minutes, BBS sends only

const QString kHf = QStringLiteral("VARA HF");
const QString kFm = QStringLiteral("VARA FM");
const QString kSat = QStringLiteral("VARA SAT");

QString localPath(const QString &pathOrUrl)
{
    if (pathOrUrl.startsWith(QLatin1String("file:")))
        return QUrl(pathOrUrl).toLocalFile();
    return pathOrUrl;
}

QString fixed1(double value)
{
    return QString::number(value, 'f', 1);
}

// Python's str.split('|', 4): at most five parts.
QStringList splitFields(const QString &text, int maxSplits)
{
    QStringList parts;
    int start = 0;
    for (int i = 0; i < maxSplits; ++i) {
        const int bar = text.indexOf(QLatin1Char('|'), start);
        if (bar < 0)
            break;
        parts.append(text.mid(start, bar - start));
        start = bar + 1;
    }
    parts.append(text.mid(start));
    return parts;
}

bool decodeUtf8(const QByteArray &bytes, QString &out)
{
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    out = decoder(bytes);
    return !decoder.hasError();
}

bool parseSize(const QString &text, qint64 &value)
{
    bool ok = false;
    value = text.trimmed().toLongLong(&ok);
    return ok && value >= 0;
}

// zlib stream (what Python's zlib.compress writes) without Qt's 4-byte
// length prefix, and back.
QByteArray zlibCompress(const QByteArray &data)
{
    return qCompress(data, 9).mid(4);
}

bool zlibDecompress(const QByteArray &stream, qint64 expectedSize, QByteArray &out)
{
    if (expectedSize > kMaxInflated)
        return false;
    QByteArray framed(4, '\0');
    qToBigEndian(quint32(expectedSize), framed.data());
    framed += stream;
    out = qUncompress(framed);
    return expectedSize == 0 || !out.isEmpty();
}

} // namespace

// =================================================================== setup

Controller::Controller(QObject *parent) : QObject(parent)
{
    m_vara = new VaraClient(this);
    m_varaProcess = new VaraProcess(this);
    connect(m_vara, &VaraClient::connected, this, &Controller::onModemConnected);
    connect(m_vara, &VaraClient::connectFailed, this, &Controller::onModemConnectFailed);
    connect(m_vara, &VaraClient::connectionLost, this, &Controller::onModemLost);
    connect(m_vara, &VaraClient::commandReceived, this, &Controller::handleCommand);
    connect(m_vara, &VaraClient::dataReceived, this, &Controller::handleData);

    m_chunkTimer.setInterval(kChunkIntervalMs);
    connect(&m_chunkTimer, &QTimer::timeout, this, &Controller::pushNextChunk);
    m_txWatchdog.setSingleShot(true);
    connect(&m_txWatchdog, &QTimer::timeout, this, [this] {
        if (m_tx.active)
            finishOutgoing(true);
    });
    m_transferReset.setSingleShot(true);
    m_transferReset.setInterval(3000);
    connect(&m_transferReset, &QTimer::timeout, this, [this] {
        if (m_tx.active || m_txPreparing || m_rx.active)
            return;
        setTransfer(false, T("file_no_transfer"), QStringLiteral("idle"), 0, 0);
    });
    m_transferText = T("file_no_transfer");
}

Controller::~Controller() = default;

void Controller::start(const QString &settingsPath, bool autoStart)
{
    m_settingsPath = settingsPath.isEmpty() ? Settings::defaultPath() : settingsPath;
    QString error;
    if (!m_settings.load(m_settingsPath, &error))
        log(QStringLiteral("%1: %2").arg(T("settings_load_error"), error), QStringLiteral("error"));
    I18n::instance().setPreference(m_settings.string(QStringLiteral("language")));
    m_transferText = T("file_no_transfer");
    emit stateChanged();
    emit preferencesChanged();
    emit transferChanged();

    // As at start-up of the Python version: with a callsign configured,
    // launch VARA (or just connect) half a second after the window shows.
    if (autoStart && !myCall().isEmpty()) {
        if (m_settings.flag(QStringLiteral("auto_start_vara")))
            QTimer::singleShot(500, this, &Controller::autoStartVara);
        else
            QTimer::singleShot(500, this, &Controller::connectToVara);
    }
}

QString Controller::version() const
{
    return QStringLiteral(CRYPTARA_VERSION_STRING);
}

QString Controller::cryptoBackend() const
{
    return QString::fromLatin1(crypto::backendName());
}

QString Controller::windowTitle() const
{
    return myCall().isEmpty() ? QStringLiteral("CRYPTARA")
                              : QStringLiteral("CRYPTARA - %1").arg(myCall());
}

bool Controller::modemConnected() const
{
    return m_demo || (m_vara && m_vara->isConnected());
}

bool Controller::modemConnecting() const
{
    return m_vara && m_vara->isConnecting();
}

QString Controller::modemType() const
{
    return m_connectedModemType;
}

bool Controller::digipeatersEnabled() const
{
    // Digipeaters: VARA FM only.
    return m_connectedModemType == kFm;
}

QStringList Controller::bandwidths() const
{
    // VARA FM and SAT take their bandwidth from VARA itself; HF offers
    // 500 and 2300 Hz (2750 Hz is reserved to other licences).
    if (m_connectedModemType == kHf)
        return {QStringLiteral("500 Hz"), QStringLiteral("2300 Hz")};
    return {};
}

int Controller::defaultBandwidthIndex() const
{
    return m_connectedModemType == kHf ? 1 : -1;
}

bool Controller::bandwidthEnabled() const
{
    return m_connectedModemType == kHf;
}

// ================================================================= dialogs

void Controller::ask(const QString &title, const QString &text, const QString &informative,
                     bool defaultYes, Answer then, const QString &icon)
{
    if (m_shutDown)
        return;
    const int id = m_nextRequest++;
    m_answers.insert(id, std::move(then));
    emit messageRequested(id, icon, title, text, informative, true, defaultYes);
}

void Controller::inform(const QString &icon, const QString &title, const QString &text,
                        const QString &informative)
{
    if (m_shutDown)
        return;
    emit messageRequested(m_nextRequest++, icon, title, text, informative, false, true);
}

void Controller::askPassword(const QString &title, const QString &prompt, TextAnswer then)
{
    if (m_shutDown)
        return;
    const int id = m_nextRequest++;
    m_textAnswers.insert(id, std::move(then));
    emit passwordRequested(id, title, prompt);
}

void Controller::askSaveFile(const QString &title, const QString &fileName, FileAnswer then)
{
    if (m_shutDown)
        return;
    const int id = m_nextRequest++;
    m_fileAnswers.insert(id, std::move(then));
    emit saveFileRequested(id, title, fileName, QUrl::fromLocalFile(QDir::homePath()).toString());
}

void Controller::answer(int id, bool yes)
{
    const Answer then = m_answers.take(id);
    if (then)
        then(yes);
}

void Controller::answerText(int id, bool ok, const QString &text)
{
    const TextAnswer then = m_textAnswers.take(id);
    if (then)
        then(ok, text);
}

void Controller::answerFile(int id, const QString &pathOrUrl)
{
    const FileAnswer then = m_fileAnswers.take(id);
    if (then)
        then(localPath(pathOrUrl));
}

// ================================================================== output

QString Controller::stamp() const
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

QString Controller::chatPrefix(const QString &who) const
{
    // "Message timestamps" in Settings > Interface.
    if (m_settings.flag(QStringLiteral("timestamp")))
        return QStringLiteral("[%1] %2: ").arg(stamp(), who);
    return who + QStringLiteral(": ");
}

void Controller::appendChat(const QString &text, const QString &kind)
{
    m_chat.append(text, kind);
    emit chatLine(text, kind);
    if (kind == QLatin1String("rx") && m_settings.flag(QStringLiteral("sound")))
        chime::play();
}

void Controller::log(const QString &message, const QString &level)
{
    const QString line = QStringLiteral("[%1] %2").arg(stamp(), message);
    m_log.append(line, level);
    emit logLine(line, level);
}

// =================================================================== modem

QString Controller::executableFor(const QString &modemType) const
{
    if (modemType == kHf)
        return m_settings.string(QStringLiteral("vara_hf_path"));
    if (modemType == kFm)
        return m_settings.string(QStringLiteral("vara_fm_path"));
    if (modemType == kSat)
        return m_settings.string(QStringLiteral("vara_sat_path"));
    return QString();
}

void Controller::autoStartVara()
{
    const QString modemType = m_settings.string(QStringLiteral("modem_type"));
    if (modemType != kHf && modemType != kFm && modemType != kSat) {
        log(QStringLiteral("%1: %2").arg(T("unknown_modem_type"), modemType), QStringLiteral("error"));
        return;
    }
    const QString executable = executableFor(modemType);
    if (executable.isEmpty()) {
        log(QStringLiteral("⚠️ %1").arg(T("modem_path_not_configured").arg(modemType)),
            QStringLiteral("warning"));
        log(T("open_settings_to_configure"), QStringLiteral("info"));
        return;
    }
    if (!QFileInfo::exists(executable)) {
        log(QStringLiteral("❌ %1 %2: %3").arg(T("executable_not_found"), modemType, executable),
            QStringLiteral("error"));
        inform(QStringLiteral("warning"), T("dlg_executable_not_found"),
               T("msg_executable_missing").arg(modemType, executable));
        return;
    }

    log(QStringLiteral("🚀 %1 %2...").arg(T("launching"), modemType), QStringLiteral("info"));
    QString error;
    if (m_varaProcess->start(modemType, executable, &error)) {
        log(QStringLiteral("✅ %1 %2").arg(modemType, T("launched_successfully")),
            QStringLiteral("success"));
        // The Python version slept 3 s then waited 2 s more.  VARA can take
        // longer on a slow machine: a few more attempts follow if refused.
        m_connectRetries = 5;
        QTimer::singleShot(5000, this, [this] {
            if (!modemConnected() && !modemConnecting())
                connectToVara();
        });
    } else {
        log(QStringLiteral("❌ %1 %2: %3").arg(T("launch_failed"), modemType, error),
            QStringLiteral("error"));
        inform(QStringLiteral("critical"), T("dlg_launch_failed"),
               T("msg_cannot_launch").arg(modemType));
    }
}

void Controller::toggleModem()
{
    if (modemConnected() || modemConnecting()) {
        m_connectRetries = 0;
        if (m_sessionActive) {
            const QString remote = m_remoteStation;
            m_sessionActive = false;
            m_pendingConnect = false;
            m_remoteStation.clear();
            handleDisconnected(remote);
        }
        m_vara->disconnectFromModem();
        log(T("log_modem_disconnected"), QStringLiteral("info"));
        markModemDown();
        return;
    }
    connectToVara();
}

void Controller::connectToVara()
{
    if (modemConnected() || modemConnecting())
        return;
    const QString host = m_settings.string(QStringLiteral("host"));
    const int commandPort = m_settings.integer(QStringLiteral("cmd_port"));
    const int dataPort = m_settings.integer(QStringLiteral("data_port"));
    m_vara->connectToModem(host, quint16(commandPort), quint16(dataPort));
    emit stateChanged();
}

void Controller::onModemConnected()
{
    m_connectRetries = 0;
    m_connectedModemType = m_settings.string(QStringLiteral("modem_type"));
    log(T("connected_to_modem").arg(m_connectedModemType, m_vara->host())
            .arg(m_vara->commandPort()),
        QStringLiteral("success"));

    // The start-up sequence of the Python version.  CHAT ON tunes VARA for
    // keyboard exchanges (timing, latency, S/N reports).
    m_vara->sendCommand(QStringLiteral("CHAT ON"));
    const QString call = myCall();
    if (!call.isEmpty())
        m_vara->sendCommand(QStringLiteral("MYCALL %1").arg(call));
    m_vara->sendCommand(QStringLiteral("VERSION"));
    if (m_settings.flag(QStringLiteral("listen")))
        m_vara->sendCommand(QStringLiteral("LISTEN ON"));
    // Receive the CQ frames of other stations.
    m_vara->sendCommand(QStringLiteral("LISTEN CQ"));
    if (m_settings.flag(QStringLiteral("compression")))
        m_vara->sendCommand(QStringLiteral("COMPRESSION ON"));
    emit stateChanged();
}

void Controller::onModemConnectFailed(const QString &error)
{
    if (m_connectRetries > 0) {
        --m_connectRetries;
        QTimer::singleShot(2000, this, [this] {
            if (!modemConnected() && !modemConnecting())
                connectToVara();
        });
        emit stateChanged();
        return;
    }
    log(QStringLiteral("%1: %2").arg(T("error_connection"), error), QStringLiteral("error"));
    markModemDown();
}

void Controller::onModemLost(const QString &error)
{
    log(QStringLiteral("⚠️ %1: %2").arg(T("modem_link_lost"), error), QStringLiteral("warning"));
    if (m_sessionActive) {
        const QString remote = m_remoteStation;
        m_sessionActive = false;
        m_pendingConnect = false;
        m_remoteStation.clear();
        handleDisconnected(remote);
    }
    markModemDown();
}

void Controller::markModemDown()
{
    m_ptt = false;
    m_pendingConnect = false;
    m_callPlaced = false;
    emit stateChanged();
}

bool Controller::sendText(const QString &payload)
{
    return m_vara && m_vara->sendData(payload.toUtf8());
}

int Controller::bandwidthHz(int index) const
{
    const QStringList list = bandwidths();
    if (index < 0 || index >= list.size())
        index = defaultBandwidthIndex();
    if (index < 0 || index >= list.size())
        return 0;
    return list.at(index).section(QLatin1Char(' '), 0, 0).toInt();
}

void Controller::callStation(const QString &station, const QString &via1, const QString &via2,
                             int bandwidthIndex)
{
    if (!modemConnected()) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("modem_not_connected"));
        return;
    }
    if (m_sessionActive) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("session_already_active"));
        return;
    }
    if (m_pendingConnect) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("call_already_in_progress"));
        return;
    }
    const QString call = myCall().trimmed();
    if (call.isEmpty()) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("configure_callsign"));
        return;
    }
    const QString callsign = station.trimmed().toUpper();
    if (callsign.isEmpty()) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("enter_callsign"));
        return;
    }
    if (callsign == call) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("cannot_call_yourself"));
        return;
    }
    const QString v1 = via1.trimmed().toUpper();
    const QString v2 = via2.trimmed().toUpper();
    if (m_channelBusy) {
        ask(T("channel_busy_title"), T("channel_busy_question"), QString(), false,
            [=](bool yes) {
                if (yes)
                    doCall(callsign, v1, v2, bandwidthIndex);
            });
        return;
    }
    doCall(callsign, v1, v2, bandwidthIndex);
}

void Controller::doCall(const QString &callsign, const QString &via1In, const QString &via2In,
                        int bandwidthIndex)
{
    if (!modemConnected() || m_sessionActive)
        return;
    const bool fm = m_connectedModemType == kFm;
    // Digipeaters exist on VARA FM only.
    const QString via1 = fm ? via1In : QString();
    const QString via2 = fm ? via2In : QString();
    const int bandwidth = bandwidthHz(bandwidthIndex);

    QString path = callsign;
    if (!via1.isEmpty() && !via2.isEmpty())
        path = QStringLiteral("%1 via %2 via %3").arg(callsign, via1, via2);
    else if (!via1.isEmpty())
        path = QStringLiteral("%1 via %2").arg(callsign, via1);

    // CONNECT MYCALL DESTCALL [VIA DIGI1 [DIGI2]]
    QString command = QStringLiteral("CONNECT %1 %2").arg(myCall(), callsign);
    if (!via1.isEmpty()) {
        command += QStringLiteral(" VIA %1").arg(via1);
        if (!via2.isEmpty())
            command += QLatin1Char(' ') + via2;
    }
    m_callPlaced = true;
    auto sendConnect = [this, command] {
        if (modemConnected())
            m_vara->sendCommand(command);
    };
    // The bandwidth is set before the call, on VARA HF only (BW2300, no
    // space); VARA gets 100 ms to apply it.
    if (m_connectedModemType == kHf && bandwidth > 0) {
        m_vara->sendCommand(QStringLiteral("BW%1").arg(bandwidth));
        QTimer::singleShot(100, this, sendConnect);
    } else {
        sendConnect();
    }

    if (m_connectedModemType == kHf && bandwidth > 0)
        log(QStringLiteral("%1 %2 %3 %4 Hz...").arg(T("calling"), path, T("on")).arg(bandwidth),
            QStringLiteral("info"));
    else
        log(QStringLiteral("%1 %2...").arg(T("calling"), path), QStringLiteral("info"));

    // 30-second timeout.
    QTimer::singleShot(30000, this, &Controller::checkConnectionTimeout);
}

void Controller::disconnectSession()
{
    if (modemConnected())
        m_vara->sendCommand(QStringLiteral("DISCONNECT"));
}

void Controller::checkConnectionTimeout()
{
    if (modemConnected() && m_pendingConnect && !m_sessionActive) {
        log(QStringLiteral("%1 - %2").arg(T("connection_timeout"), T("cancelling")),
            QStringLiteral("warning"));
        m_vara->sendCommand(QStringLiteral("ABORT"));
        m_pendingConnect = false;
        m_callPlaced = false;
    }
}

void Controller::sendCq(const QString &via1In, const QString &via2In, int bandwidthIndex)
{
    if (!modemConnected()) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("modem_not_connected"));
        return;
    }
    if (m_sessionActive) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("no_cq_in_session"));
        return;
    }
    const QString call = myCall().trimmed().toUpper();
    if (call.isEmpty()) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("configure_callsign"));
        return;
    }
    QString command;
    if (m_connectedModemType == kHf) {
        // VARA HF: CQFRAME Source BW
        command = QStringLiteral("CQFRAME %1 %2").arg(call).arg(bandwidthHz(bandwidthIndex));
    } else if (m_connectedModemType == kSat) {
        // VARA SAT: CQFRAME Source
        command = QStringLiteral("CQFRAME %1").arg(call);
    } else if (m_connectedModemType == kFm) {
        // VARA FM: CQFRAME Source Digi1 Digi2
        const QString via1 = via1In.trimmed().toUpper();
        const QString via2 = via2In.trimmed().toUpper();
        command = QStringLiteral("CQFRAME %1").arg(call);
        if (!via1.isEmpty()) {
            command += QLatin1Char(' ') + via1;
            if (!via2.isEmpty())
                command += QLatin1Char(' ') + via2;
        }
    } else {
        log(QStringLiteral("%1: %2").arg(T("unknown_modem_type"), m_connectedModemType),
            QStringLiteral("error"));
        return;
    }
    m_vara->sendCommand(command);
    log(QStringLiteral("📢 %1: %2").arg(T("cq_sent"), command), QStringLiteral("info"));
}

void Controller::addReceivedCq(const QString &callsign, const QString &bandwidth,
                               const QString &via1, const QString &via2)
{
    CqEntry entry{callsign, bandwidth, via1, via2, QDateTime::currentDateTime()};
    if (m_cq.addOrUpdate(entry))
        log(QStringLiteral("📡 %1 %2").arg(T("cq_received_from"), callsign), QStringLiteral("info"));
}

void Controller::connectToCq(int row, int bandwidthIndex)
{
    const CqEntry *entry = m_cq.at(row);
    if (!entry)
        return;
    const QString callsign = entry->callsign;
    const QString via1 = entry->via1;
    const QString via2 = entry->via2;
    QString via;
    if (!via1.isEmpty())
        via = via2.isEmpty() ? via1 : via1 + QLatin1Char(' ') + via2;
    const QString viaInfo = via.isEmpty() ? QString() : QStringLiteral(" via %1").arg(via);
    ask(T("cq_connect_title"), T("cq_connect_question").arg(callsign + viaInfo), QString(), true,
        [=](bool yes) {
            if (!yes)
                return;
            emit stationFieldsRequested(callsign, via1, via2);
            callStation(callsign, via1, via2, bandwidthIndex);
        });
}

void Controller::clearCq()
{
    if (m_cq.count() == 0)
        return;
    ask(T("cq_clear_title"), T("cq_clear_question"), QString(), true, [this](bool yes) {
        if (!yes)
            return;
        m_cq.clear();
        log(QStringLiteral("🗑️ %1").arg(T("cq_cleared")), QStringLiteral("info"));
    });
}

// ============================================================== encryption

void Controller::setCryptoButton(bool on)
{
    if (m_cryptoButton == on)
        return;
    m_cryptoButton = on;
    emit stateChanged();
}

QString Controller::remoteOr(const QString &fallback) const
{
    return m_remoteStation.isEmpty() ? fallback : m_remoteStation;
}

void Controller::toggleCrypto()
{
    if (!m_sessionActive)
        return;
    const quint64 serial = m_sessionSerial;

    if (m_cryptoButton) {
        // Switching off.
        m_crypto.disable();
        setCryptoButton(false);
        log(QStringLiteral("🔓 %1").arg(T("log_encryption_disabled")), QStringLiteral("info"));
        if (m_sessionActive) {
            sendText(QStringLiteral("<CRYPTOFF></CRYPTOFF>") + kEof);
            log(QStringLiteral("📤 %1").arg(T("crypto_off_notification_sent")), QStringLiteral("info"));
        }
        return;
    }

    if (m_crypto.hasPassword()) {
        // Password already known: reactivation.
        m_crypto.enable();
        setCryptoButton(true);
        log(QStringLiteral("🔒 %1").arg(T("log_encryption_enabled")), QStringLiteral("success"));
        if (m_sessionActive) {
            sendText(QStringLiteral("<CRYPTON></CRYPTON>") + kEof);
            log(QStringLiteral("📤 %1").arg(T("crypto_on_notification_sent")), QStringLiteral("info"));
        }
        return;
    }

    askPassword(T("crypto_setup"), T("crypto_password_prompt"),
                [this, serial](bool ok, const QString &password) {
        if (!ok || password.trimmed().isEmpty()) {
            setCryptoButton(false);
            return;
        }
        m_crypto.setPassword(password);
        m_crypto.enable();
        setCryptoButton(true);
        log(QStringLiteral("🔒 %1").arg(T("crypto_enabled")), QStringLiteral("success"));

        if (m_sessionActive && serial == m_sessionSerial) {
            // Offer the Diffie-Hellman session (Perfect Forward Secrecy).
            ask(QStringLiteral("Perfect Forward Secrecy"), T("pfs_question"), QString(), true,
                [this, serial](bool yes) {
                    if (yes) {
                        if (serial == m_sessionSerial)
                            initiateCryptoRequest();
                    } else {
                        inform(QStringLiteral("information"), T("dlg_encryption_enabled"),
                               T("msg_encryption_enabled_session"));
                    }
                });
        } else {
            inform(QStringLiteral("information"), T("dlg_encryption_enabled"),
                   T("msg_encryption_enabled_no_session"));
        }
    });
}

void Controller::initiateCryptoRequest()
{
    if (!m_sessionActive) {
        inform(QStringLiteral("warning"), T("dlg_error"), T("must_be_connected_station"));
        return;
    }
    log(QStringLiteral("🔑 %1...").arg(T("crypto_sending_request")), QStringLiteral("info"));
    const QString publicKey = m_crypto.generateDhKeyPair();
    if (publicKey.isEmpty()) {
        log(QStringLiteral("❌ %1").arg(T("crypto_error_dh_keygen")), QStringLiteral("error"));
        return;
    }
    sendText(QStringLiteral("<CRYPTREQ>%1</CRYPTREQ>").arg(publicKey) + kEof);
    log(QStringLiteral("🔑 %1...").arg(T("crypto_request_sent")), QStringLiteral("info"));
}

void Controller::handleCryptoRequest(const QString &peerKey)
{
    log(QStringLiteral("🔑 %1").arg(T("crypto_request_received")), QStringLiteral("info"));
    const quint64 serial = m_sessionSerial;
    const QString remote = m_remoteStation;

    ask(T("dlg_encryption_request"), T("msg_wants_encrypted_session").arg(remote), QString(), true,
        [this, serial, remote, peerKey](bool yes) {
        if (serial != m_sessionSerial || !m_sessionActive)
            return;
        if (!yes) {
            log(QStringLiteral("❌ %1").arg(T("crypto_request_refused")), QStringLiteral("warning"));
            sendText(QStringLiteral("<CRYPTREFUSED></CRYPTREFUSED>") + kEof);
            return;
        }
        askPassword(T("dlg_encryption_password"), T("msg_enter_shared_password").arg(remote),
                    [this, serial, peerKey](bool ok, const QString &password) {
            if (serial != m_sessionSerial || !m_sessionActive)
                return;
            if (!ok || password.trimmed().isEmpty()) {
                log(QStringLiteral("❌ %1").arg(T("crypto_password_not_entered")),
                    QStringLiteral("warning"));
                sendText(QStringLiteral("<CRYPTREFUSED></CRYPTREFUSED>") + kEof);
                return;
            }
            m_crypto.setPassword(password);
            m_crypto.enable();
            setCryptoButton(true);
            log(QStringLiteral("🔒 %1").arg(T("log_encryption_enabled")), QStringLiteral("success"));

            QString ourKey;
            if (!m_crypto.hasDhKeyPair()) {
                log(QStringLiteral("🔑 %1...").arg(T("crypto_generating_our_dh")), QStringLiteral("info"));
                ourKey = m_crypto.generateDhKeyPair();
            } else {
                ourKey = m_crypto.publicKey();
            }
            if (ourKey.isEmpty()) {
                log(QStringLiteral("❌ %1").arg(T("crypto_error_dh_keygen")), QStringLiteral("error"));
                return;
            }
            sendText(QStringLiteral("<CRYPTACK>%1</CRYPTACK>").arg(ourKey) + kEof);
            log(QStringLiteral("🔑 %1").arg(T("crypto_response_sent_with_key")), QStringLiteral("info"));

            QString error;
            if (m_crypto.computeSharedSecret(peerKey, &error)) {
                log(QStringLiteral("✅ %1 - %2!").arg(T("crypto_dh_session_established"),
                                                     T("crypto_pfs_enabled")),
                    QStringLiteral("success"));
                inform(QStringLiteral("information"), T("dlg_encryption_enabled"),
                       T("msg_secure_session_ready"));
            } else {
                log(QStringLiteral("❌ %1: %2").arg(T("crypto_error_shared_secret"), error),
                    QStringLiteral("error"));
            }
        });
    });
}

void Controller::handleCryptoAck(const QString &peerKey)
{
    log(QStringLiteral("🔑 %1").arg(T("crypto_response_received")), QStringLiteral("info"));
    QString error;
    if (m_crypto.computeSharedSecret(peerKey, &error)) {
        log(QStringLiteral("✅ %1 - %2!").arg(T("crypto_dh_session_established"),
                                             T("crypto_pfs_enabled")),
            QStringLiteral("success"));
        inform(QStringLiteral("information"), T("dlg_secure_session_established"),
               T("msg_dh_success_long"));
    } else {
        log(QStringLiteral("❌ %1: %2").arg(T("crypto_error_dh_session"), error),
            QStringLiteral("error"));
        inform(QStringLiteral("warning"), T("dlg_dh_error"), T("msg_dh_failed_password_used"));
    }
}

void Controller::handleCryptoRefused()
{
    log(QStringLiteral("❌ %1").arg(T("crypto_other_refused")), QStringLiteral("warning"));
    inform(QStringLiteral("warning"), T("dlg_encryption_refused"),
           T("msg_peer_refused_encryption").arg(remoteOr(T("other_station"))));
}

void Controller::handleCryptoOff()
{
    log(QStringLiteral("🔓 %1").arg(T("crypto_other_disabled")), QStringLiteral("info"));
    const quint64 serial = m_sessionSerial;
    ask(T("dlg_encryption_disabled_by_peer"),
        T("msg_peer_disabled_encryption").arg(remoteOr(T("your_peer"))),
        T("msg_also_disable_question"), false,
        [this, serial](bool yes) {
            if (serial != m_sessionSerial)
                return;
            if (yes) {
                m_crypto.disable();
                setCryptoButton(false);
                log(QStringLiteral("🔓 %1").arg(T("log_encryption_disabled")), QStringLiteral("info"));
                if (m_sessionActive) {
                    sendText(QStringLiteral("<CRYPTOFFACK></CRYPTOFFACK>") + kEof);
                    log(QStringLiteral("📤 %1").arg(T("crypto_off_ack_sent")), QStringLiteral("info"));
                }
            } else {
                log(QStringLiteral("🔒 %1").arg(T("crypto_kept_active")), QStringLiteral("info"));
                if (m_sessionActive) {
                    sendText(QStringLiteral("<CRYPTOFFREFUSED></CRYPTOFFREFUSED>") + kEof);
                    log(QStringLiteral("📤 %1").arg(T("crypto_off_refused_sent")), QStringLiteral("info"));
                }
                inform(QStringLiteral("information"), T("dlg_encryption_maintained"),
                       T("msg_keep_encrypting").arg(remoteOr(T("other_station"))));
            }
        },
        QStringLiteral("information"));
}

void Controller::handleCryptoOffAck()
{
    log(QStringLiteral("✅ %1").arg(T("crypto_other_also_disabled")), QStringLiteral("success"));
    inform(QStringLiteral("information"), T("dlg_sync_success"),
           T("msg_both_clear").arg(remoteOr(T("other_station"))));
}

void Controller::handleCryptoOffRefused()
{
    log(QStringLiteral("⚠️ %1").arg(T("crypto_other_wants_keep")), QStringLiteral("warning"));
    const quint64 serial = m_sessionSerial;
    const QString remote = remoteOr(T("other_station"));
    ask(T("dlg_deactivation_refused"), T("msg_desync").arg(remote),
        T("msg_desync_details").arg(remoteOr(T("your_peer"))), true,
        [this, serial](bool yes) {
            if (serial != m_sessionSerial)
                return;
            if (!yes) {
                log(QStringLiteral("⚠️ %1").arg(T("continuing_without_encryption")),
                    QStringLiteral("warning"));
                return;
            }
            if (m_crypto.hasPassword()) {
                m_crypto.enable();
                setCryptoButton(true);
                log(QStringLiteral("🔒 %1").arg(T("crypto_reactivated")), QStringLiteral("success"));
                return;
            }
            askPassword(T("dlg_encryption_password"),
                        T("msg_enter_shared_password").arg(remoteOr(T("your_peer"))),
                        [this](bool ok, const QString &password) {
                if (ok && !password.trimmed().isEmpty()) {
                    m_crypto.setPassword(password);
                    m_crypto.enable();
                    setCryptoButton(true);
                    log(QStringLiteral("🔒 %1").arg(T("crypto_reactivated")), QStringLiteral("success"));
                } else {
                    log(QStringLiteral("❌ %1").arg(T("reactivation_cancelled")), QStringLiteral("warning"));
                }
            });
        },
        QStringLiteral("warning"));
}

void Controller::handleCryptoOn()
{
    log(QStringLiteral("🔒 %1").arg(T("crypto_other_reactivated")), QStringLiteral("info"));
    const QString text = T("msg_peer_reactivated").arg(remoteOr(T("your_peer")));
    if (m_crypto.isEnabled()) {
        inform(QStringLiteral("information"), T("dlg_encryption_reactivated"), text,
               T("msg_already_encrypting"));
        return;
    }
    const quint64 serial = m_sessionSerial;
    ask(T("dlg_encryption_reactivated"), text, T("msg_also_reactivate_question"), true,
        [this, serial](bool yes) {
            if (!yes || serial != m_sessionSerial || m_crypto.isEnabled())
                return;
            auto confirm = [this] {
                if (m_sessionActive) {
                    sendText(QStringLiteral("<CRYPTONACK></CRYPTONACK>") + kEof);
                    log(QStringLiteral("📤 %1").arg(T("crypto_on_ack_sent")), QStringLiteral("info"));
                }
            };
            if (m_crypto.hasPassword()) {
                m_crypto.enable();
                setCryptoButton(true);
                log(QStringLiteral("🔒 %1").arg(T("crypto_reactivated")), QStringLiteral("success"));
                confirm();
                return;
            }
            askPassword(T("dlg_encryption_password"),
                        T("msg_enter_shared_password").arg(remoteOr(T("your_peer"))),
                        [this, serial, confirm](bool ok, const QString &password) {
                if (serial != m_sessionSerial || !ok || password.trimmed().isEmpty())
                    return;
                m_crypto.setPassword(password);
                m_crypto.enable();
                setCryptoButton(true);
                log(QStringLiteral("🔒 %1").arg(T("log_encryption_enabled")), QStringLiteral("success"));
                confirm();
            });
        },
        QStringLiteral("information"));
}

void Controller::handleCryptoOnAck()
{
    log(QStringLiteral("✅ %1").arg(T("crypto_other_also_reactivated")), QStringLiteral("success"));
    inform(QStringLiteral("information"), T("dlg_sync_success"),
           T("msg_both_encrypted").arg(remoteOr(T("other_station"))));
}

void Controller::handleDhKey(const QString &peerKey)
{
    // The older exchange, kept so older stations still get a session.
    log(QStringLiteral("🔑 %1").arg(T("dh_key_received_old_format")), QStringLiteral("info"));
    if (!m_crypto.hasDhKeyPair()) {
        log(QStringLiteral("🔑 %1...").arg(T("generating_our_key_pair")), QStringLiteral("info"));
        const QString ourKey = m_crypto.generateDhKeyPair();
        if (ourKey.isEmpty()) {
            log(QStringLiteral("❌ %1").arg(T("crypto_error_dh_keygen")), QStringLiteral("error"));
            return;
        }
        sendText(QStringLiteral("<DHKEY>%1</DHKEY>").arg(ourKey) + kEof);
        log(QStringLiteral("🔑 %1").arg(T("dh_key_sent_response")), QStringLiteral("info"));
    }
    QString error;
    if (m_crypto.computeSharedSecret(peerKey, &error)) {
        log(QStringLiteral("✅ %1 - %2!").arg(T("dh_session_established"), T("crypto_pfs_enabled")),
            QStringLiteral("success"));
        inform(QStringLiteral("information"), T("dlg_secure_session_established"),
               T("msg_dh_success_long"));
    } else {
        log(QStringLiteral("❌ %1: %2").arg(T("crypto_error_dh_session"), error),
            QStringLiteral("error"));
        inform(QStringLiteral("warning"), T("dlg_dh_error"), T("msg_dh_failed_password_used"));
    }
}

QString Controller::decryptError(CryptoManager::DecryptStatus status) const
{
    switch (status) {
    case CryptoManager::DecryptStatus::NoKey:
        return T("dec_no_password");
    case CryptoManager::DecryptStatus::NoSession:
        return T("dec_no_session");
    case CryptoManager::DecryptStatus::BadHmac:
        return T("dec_bad_hmac");
    case CryptoManager::DecryptStatus::Disabled:
        return T("dec_disabled");
    case CryptoManager::DecryptStatus::Failed:
        return T("dec_failed");
    case CryptoManager::DecryptStatus::Ok:
    case CryptoManager::DecryptStatus::NotEncrypted:
        break;
    }
    return QString();
}

// ================================================================ messages

bool Controller::sendMessage(const QString &text)
{
    if (!m_sessionActive)
        return false;
    const QString message = text.trimmed();
    if (message.isEmpty())
        return false;

    bool ok = true;
    const QString payload = m_crypto.isEnabled() ? m_crypto.encrypt(message, &ok) : message;
    if (!ok) {
        // Never fall back to clear text when encryption was asked for.
        log(QStringLiteral("❌ %1").arg(T("encryption_failed_not_sent")), QStringLiteral("error"));
        return false;
    }
    if (!sendText(payload + kEof))
        return false;
    // Shown once the other station has it (VARA reports BUFFER 0).
    const QString who = myCall().isEmpty() ? T("me") : myCall();
    m_pendingMessages.append(chatPrefix(m_crypto.isEnabled() ? who + QStringLiteral(" 🔒") : who)
                             + message);
    return true;
}

void Controller::sendMacro(int number)
{
    if (number < 1 || number > 12)
        return;
    const QString name = QStringLiteral("F%1").arg(number);
    if (!m_sessionActive) {
        log(QStringLiteral("⚠️ %1").arg(T("macro_no_session")), QStringLiteral("warning"));
        return;
    }
    const QString macro = m_settings.string(QStringLiteral("macro_f%1").arg(number));
    if (macro.isEmpty()) {
        log(QStringLiteral("⚠️ %1").arg(T("macro_empty").arg(name)), QStringLiteral("warning"));
        return;
    }
    if (sendMessage(expandMacro(macro)))
        log(QStringLiteral("📤 %1").arg(T("macro_sent").arg(name)), QStringLiteral("info"));
}

QString Controller::expandMacro(const QString &text, const QString &callsignFallback) const
{
    if (text.isEmpty())
        return QString();
    const QDateTime now = QDateTime::currentDateTime();
    QString out = text;
    out.replace(QStringLiteral("{MYCALL}"), myCall());
    out.replace(QStringLiteral("{NAME}"), m_settings.string(QStringLiteral("operator_name")));
    out.replace(QStringLiteral("{FIRSTNAME}"), m_settings.string(QStringLiteral("operator_firstname")));
    out.replace(QStringLiteral("{QTH}"), m_settings.string(QStringLiteral("qth")));
    out.replace(QStringLiteral("{GRID}"), m_settings.string(QStringLiteral("grid_square")));
    out.replace(QStringLiteral("{RIG}"), m_settings.string(QStringLiteral("rig")));
    out.replace(QStringLiteral("{ANTENNA}"), m_settings.string(QStringLiteral("antenna")));
    out.replace(QStringLiteral("{CALLSIGN}"),
                m_remoteStation.isEmpty() ? callsignFallback : m_remoteStation);
    out.replace(QStringLiteral("{TIME}"), now.toString(QStringLiteral("HH:mm")));
    out.replace(QStringLiteral("{DATE}"), now.toString(QStringLiteral("yyyy-MM-dd")));
    return out;
}

QStringList Controller::macros() const
{
    QStringList list;
    for (int i = 1; i <= 12; ++i)
        list.append(m_settings.string(QStringLiteral("macro_f%1").arg(i)));
    return list;
}

void Controller::saveMacros(const QStringList &macros)
{
    for (int i = 0; i < 12 && i < macros.size(); ++i)
        m_settings.set(QStringLiteral("macro_f%1").arg(i + 1), macros.at(i));
    saveSettings();
    log(QStringLiteral("💾 %1").arg(T("macros_saved")), QStringLiteral("success"));
}

void Controller::testMacro(const QString &text)
{
    inform(QStringLiteral("information"), T("dlg_test_macro"),
           QStringLiteral("%1\n%2\n\n%3\n%4").arg(T("test_macro_original"), text,
                                                  T("test_macro_expanded"), expandMacro(text)));
}

// ================================================================== events

void Controller::handleCommand(const QString &command)
{
    log(QStringLiteral("CMD: %1").arg(command), QStringLiteral("debug"));
    const QStringList parts = command.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    if (command.startsWith(QLatin1String("PTT"))) {
        m_ptt = command.contains(QLatin1String("ON"));
        emit stateChanged();
    } else if (command.startsWith(QLatin1String("PENDING"))) {
        if (parts.size() > 1) {
            m_pendingConnect = true;
            log(QStringLiteral("%1 %2...").arg(T("connecting_to"), parts.at(1)), QStringLiteral("info"));
        }
    } else if (command.startsWith(QLatin1String("CANCELPENDING"))) {
        m_pendingConnect = false;
        log(T("call_cancelled"), QStringLiteral("warning"));
    } else if (command.startsWith(QLatin1String("CONNECTED"))) {
        const QString call = myCall().toUpper();
        if (parts.size() >= 3) {
            // CONNECTED Source Destination BW: the remote station is the
            // one that is not us.
            const QString source = parts.at(1);
            const QString destination = parts.at(2);
            const QString bandwidth = parts.size() > 3 ? parts.at(3) : QStringLiteral("?");
            QString remote;
            bool incoming;
            if (source.toUpper() == call) {
                remote = destination;
                incoming = false;
            } else if (destination.toUpper() == call) {
                remote = source;
                incoming = true;
            } else {
                remote = source;
                incoming = !m_callPlaced;
            }
            m_remoteStation = remote;
            m_sessionActive = true;
            m_pendingConnect = false;
            log(QStringLiteral("%1 %2, %3: %4").arg(T("connected_to"), remote, T("bandwidth"), bandwidth),
                QStringLiteral("info"));
            handleConnected(remote, incoming);
        } else if (parts.size() > 1) {
            // Older format, without the bandwidth.
            m_remoteStation = parts.at(1);
            m_sessionActive = true;
            m_pendingConnect = false;
            handleConnected(parts.at(1), !m_callPlaced);
        }
    } else if (command.startsWith(QLatin1String("DISCONNECTED"))) {
        m_sessionActive = false;
        m_pendingConnect = false;
        const QString remote = m_remoteStation;
        m_remoteStation.clear();
        if (!remote.isEmpty())
            handleDisconnected(remote);
        else
            emit stateChanged();
    } else if (command.startsWith(QLatin1String("BUSY"))) {
        m_channelBusy = command.contains(QLatin1String("TRUE")) || command.contains(QLatin1String("ON"));
        log(QStringLiteral("%1 %2").arg(T("channel"), m_channelBusy ? T("busy") : T("free")),
            QStringLiteral("info"));
    } else if (command.startsWith(QLatin1String("CQFRAME"))) {
        // VARA HF: CQFRAME Source BW / SAT: CQFRAME Source /
        // FM: CQFRAME Source Digi1 Digi2
        if (parts.size() >= 2) {
            QString bandwidth, via1, via2;
            if (parts.size() >= 3) {
                const QString third = parts.at(2);
                bool digits = !third.isEmpty();
                for (const QChar c : third)
                    digits = digits && c.isDigit();
                if (digits || third == QLatin1String("NARROW") || third == QLatin1String("WIDE")) {
                    bandwidth = third;
                } else {
                    via1 = third;
                    if (parts.size() >= 4)
                        via2 = parts.at(3);
                }
            }
            addReceivedCq(parts.at(1), bandwidth, via1, via2);
        }
    } else if (command.startsWith(QLatin1String("VERSION"))) {
        log(QStringLiteral("%1: %2").arg(T("vara_version"), command.mid(7).trimmed()),
            QStringLiteral("info"));
    } else if (command.startsWith(QLatin1String("BUFFER"))) {
        bool ok = false;
        const qint64 inBuffer = parts.size() >= 2 ? parts.at(1).toLongLong(&ok) : 0;
        if (!ok)
            return;
        if (inBuffer == 0) {
            // Everything sent has been acknowledged: show what was waiting.
            for (const QString &message : std::as_const(m_pendingMessages))
                appendChat(message, QStringLiteral("tx"));
            m_pendingMessages.clear();
            if (m_autoDisconnectPending) {
                m_autoDisconnectPending = false;
                const quint64 serial = m_sessionSerial;
                QTimer::singleShot(1000, this, [this, serial] {
                    if (m_sessionActive && serial == m_sessionSerial)
                        disconnectSession();
                });
            }
        }
        if (m_tx.active && !m_tx.payload.isEmpty()) {
            // What went out over the air: what we handed to VARA minus what
            // it still holds.
            const qint64 total = m_tx.payload.size();
            const qint64 transmitted = m_tx.pushed - inBuffer;
            m_tx.percent = int(std::clamp<qint64>(transmitted * 100 / total, 0, 100));
            updateOutgoingProgress();
            if (m_tx.pushedAll && m_tx.percent >= 100)
                finishOutgoing(false);
        }
    } else if (command.startsWith(QLatin1String("IAMALIVE"))) {
        // VARA heartbeat.
    } else if (command.startsWith(QLatin1String("SN "))) {
        // Signal/noise report (CHAT ON); the Python version only printed it.
    } else if (command.contains(QLatin1String("WRONG")) || command.contains(QLatin1String("ERROR"))
               || command.contains(QLatin1String("FAIL"))) {
        log(QStringLiteral("%1: %2").arg(T("vara_error"), command), QStringLiteral("error"));
        m_pendingConnect = false;
    }
}

void Controller::handleData(const QByteArray &data)
{
    m_rxBuffer += data;

    if (m_rx.active && m_rx.total > 0) {
        const qint64 received = m_rxBuffer.size();
        const int percent = int(std::min<qint64>(99, received * 100 / m_rx.total));
        const double elapsed = m_rx.clock.elapsed() / 1000.0;
        QString text = QStringLiteral("📥 %1 %2: %3%").arg(T("receiving"), m_rx.name).arg(percent);
        if (elapsed > 1)
            text += QStringLiteral(" (%1 %2)").arg(qRound64(received / elapsed)).arg(T("bytes_per_second"));
        setTransfer(true, text, QStringLiteral("active"), double(std::min(received, m_rx.total)),
                    double(m_rx.total));
    }

    // Every complete message, not only the first one of the chunk.
    for (;;) {
        const int end = m_rxBuffer.indexOf(kEofBytes);
        if (end < 0)
            break;
        const QByteArray message = m_rxBuffer.left(end);
        m_rxBuffer.remove(0, end + kEofBytes.size());
        dispatchMessage(QString::fromUtf8(message));
    }
}

void Controller::dispatchMessage(const QString &msg)
{
    auto tagged = [&msg](const char *open, const char *close) {
        return msg.startsWith(QLatin1String(open)) && msg.endsWith(QLatin1String(close));
    };
    auto inner = [&msg](int openSize, int closeSize) {
        return msg.mid(openSize, msg.size() - openSize - closeSize);
    };

    if (tagged("<CRYPTREQ>", "</CRYPTREQ>")) {
        handleCryptoRequest(inner(10, 11));
    } else if (tagged("<CRYPTACK>", "</CRYPTACK>")) {
        handleCryptoAck(inner(10, 11));
    } else if (tagged("<CRYPTREFUSED>", "</CRYPTREFUSED>")) {
        handleCryptoRefused();
    } else if (tagged("<CRYPTOFF>", "</CRYPTOFF>")) {
        handleCryptoOff();
    } else if (tagged("<CRYPTOFFACK>", "</CRYPTOFFACK>")) {
        handleCryptoOffAck();
    } else if (tagged("<CRYPTOFFREFUSED>", "</CRYPTOFFREFUSED>")) {
        handleCryptoOffRefused();
    } else if (tagged("<CRYPTON>", "</CRYPTON>")) {
        handleCryptoOn();
    } else if (tagged("<CRYPTONACK>", "</CRYPTONACK>")) {
        handleCryptoOnAck();
    } else if (tagged("<DHKEY>", "</DHKEY>")) {
        handleDhKey(inner(7, 8));
    } else if (msg == QLatin1String("<BBSON>")) {
        m_remoteBbsActive = true;
        log(QStringLiteral("📁 %1").arg(T("bbs_detected")), QStringLiteral("info"));
        emit stateChanged();
    } else if (msg == QLatin1String("<BBSLIST>")) {
        handleBbsListRequest();
    } else if (tagged("<BBSFILES>", "</BBSFILES>")) {
        handleBbsFiles(inner(10, 11));
    } else if (tagged("<BBSGET>", "</BBSGET>")) {
        handleBbsGetRequest(inner(8, 9));
    } else if (msg.startsWith(QLatin1String("<FILEINFO>"))) {
        receiveFileInfo(msg.mid(10));
    } else if (msg.startsWith(QLatin1String("<FILEDATA>"))) {
        receiveFileData(msg.mid(10));
    } else if (msg.startsWith(QLatin1String("<FILE>"))) {
        receiveOldFile(msg.mid(6));
    } else if (!msg.isEmpty()) {
        const QString remote = m_remoteStation.isEmpty() ? QStringLiteral("Station") : m_remoteStation;
        if (CryptoManager::isEncryptedMessage(msg)) {
            const CryptoManager::Decrypted d = m_crypto.decrypt(msg);
            const QString shown = d.ok() || d.status == CryptoManager::DecryptStatus::Disabled
                                      ? d.text
                                      : decryptError(d.status);
            appendChat(chatPrefix(remote + QStringLiteral(" 🔒")) + shown, QStringLiteral("rx"));
        } else {
            appendChat(chatPrefix(remote) + msg, QStringLiteral("rx"));
        }
    }
}

// ================================================================= session

void Controller::handleConnected(const QString &callsign, bool incoming)
{
    ++m_sessionSerial;
    m_callPlaced = false;
    log(QStringLiteral("✓ %1 %2").arg(T("connected_to"), callsign), QStringLiteral("success"));
    emit stateChanged();

    // The BBS beacon, half a second in, unless the auto-responder is about
    // to hang up.
    const bool autoDisconnect = m_settings.flag(QStringLiteral("auto_disconnect"));
    QString folder;
    if (m_settings.flag(QStringLiteral("bbs_enabled")) && !autoDisconnect && bbsFolderUsable(&folder)) {
        const quint64 serial = m_sessionSerial;
        QTimer::singleShot(500, this, [this, serial] {
            if (m_sessionActive && serial == m_sessionSerial)
                sendBbsOn();
        });
    }

    // The auto-responder answers incoming connections, a second in.
    if (incoming && m_settings.flag(QStringLiteral("auto_enabled")) && !m_autoResponderActive) {
        const quint64 serial = m_sessionSerial;
        QTimer::singleShot(1000, this, [this, serial] {
            if (m_sessionActive && serial == m_sessionSerial && !m_autoResponderActive)
                triggerAutoResponder();
        });
    }

    m_qsoLog.append({callsign, QDateTime::currentDateTime(), QDateTime()});
}

void Controller::handleDisconnected(const QString &callsign)
{
    ++m_sessionSerial;
    m_callPlaced = false;
    log(QStringLiteral("✗ %1 %2").arg(T("disconnected_from"), callsign), QStringLiteral("info"));

    // Encryption off with the session; the Diffie-Hellman key belonged to
    // this session and is forgotten (the password is kept).
    setCryptoButton(false);
    m_crypto.disable();
    m_crypto.resetSession();

    m_remoteBbsActive = false;
    m_bbsFiles.clear();
    m_autoResponderActive = false;
    m_autoDisconnectPending = false;
    abortTransfers(T("session_ended"));

    if (!m_qsoLog.isEmpty() && m_qsoLog.last().callsign == callsign && !m_qsoLog.last().end.isValid())
        m_qsoLog.last().end = QDateTime::currentDateTime();
    emit stateChanged();
}

void Controller::triggerAutoResponder()
{
    m_autoResponderActive = true;
    const int delay = m_settings.integer(QStringLiteral("auto_delay"));
    const quint64 serial = m_sessionSerial;
    QTimer::singleShot(std::max(0, delay) * 1000, this, [this, serial] {
        const QString message = expandMacro(m_settings.string(QStringLiteral("auto_message")),
                                            QStringLiteral("OM"));
        if (!message.isEmpty() && m_sessionActive && serial == m_sessionSerial) {
            bool ok = true;
            const QString payload = m_crypto.isEnabled() ? m_crypto.encrypt(message, &ok) : message;
            if (ok && sendText(payload + kEof)) {
                const QString who = myCall().isEmpty() ? QStringLiteral("Auto") : myCall();
                m_pendingMessages.append(
                    chatPrefix(m_crypto.isEnabled() ? who + QStringLiteral(" 🔒 (AUTO)")
                                                    : who + QStringLiteral(" (AUTO)"))
                    + message);
                // Hang up once VARA reports the message delivered (BUFFER 0).
                if (m_settings.flag(QStringLiteral("auto_disconnect")))
                    m_autoDisconnectPending = true;
            }
        }
        m_autoResponderActive = false;
    });
    log(QStringLiteral("%1 (%2: %3s)").arg(T("auto_responder_enabled"), T("delay")).arg(delay),
        QStringLiteral("info"));
}

// =================================================================== files

QString Controller::safeFileName(const QString &name)
{
    // The name comes from the other station: no folders, nothing Windows
    // refuses in a file name.
    QString base = name;
    base.replace(QLatin1Char('\\'), QLatin1Char('/'));
    base = base.section(QLatin1Char('/'), -1);
    QString out;
    for (const QChar c : base) {
        if (c.unicode() < 32 || QStringLiteral("<>:\"/\\|?*").contains(c))
            out += QLatin1Char('_');
        else
            out += c;
    }
    out = out.trimmed();
    while (out.endsWith(QLatin1Char('.')))
        out.chop(1);
    if (out.isEmpty() || out == QLatin1String(".."))
        out = QStringLiteral("received_file");
    return out;
}

QString Controller::uniquePath(const QString &folder, const QString &fileName)
{
    const QDir dir(folder);
    QString path = dir.filePath(fileName);
    if (!QFileInfo::exists(path))
        return path;
    // name_1.ext, name_2.ext... as os.path.splitext() splits.
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    const QString base = dot > 0 ? fileName.left(dot) : fileName;
    const QString ext = dot > 0 ? fileName.mid(dot) : QString();
    for (int counter = 1;; ++counter) {
        path = dir.filePath(QStringLiteral("%1_%2%3").arg(base).arg(counter).arg(ext));
        if (!QFileInfo::exists(path))
            return path;
    }
}

void Controller::setTransfer(bool visible, const QString &text, const QString &state,
                             double value, double maximum)
{
    m_transferVisible = visible;
    m_transferText = text;
    m_transferState = state;
    m_transferValue = value;
    m_transferMaximum = maximum;
    emit transferChanged();
}

void Controller::scheduleTransferReset()
{
    m_transferReset.start();
}

void Controller::sendFile(const QString &pathOrUrl)
{
    if (!m_sessionActive)
        return;
    prepareAndSendFile(localPath(pathOrUrl), false);
}

void Controller::prepareAndSendFile(const QString &path, bool bbs)
{
    if (!m_sessionActive) {
        log(QStringLiteral("⚠️ %1").arg(T("no_active_session")), QStringLiteral("warning"));
        return;
    }
    if (transferBusy()) {
        log(QStringLiteral("⚠️ %1").arg(T("transfer_already_running")), QStringLiteral("warning"));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (bbs) {
            log(QStringLiteral("❌ %1: %2").arg(T("bbs_send_error"), file.errorString()),
                QStringLiteral("error"));
        } else {
            inform(QStringLiteral("critical"), T("dlg_error"),
                   QStringLiteral("%1\n%2").arg(T("cannot_send_file"), file.errorString()));
            log(QStringLiteral("%1: %2").arg(T("error_sending_file"), file.errorString()),
                QStringLiteral("error"));
        }
        return;
    }
    const QByteArray content = file.readAll();
    file.close();
    const QString fileName = QFileInfo(path).fileName();
    const qint64 originalSize = content.size();

    // 2 MB at most from the button (no limit for the BBS, as before).
    if (!bbs && originalSize > kMaxFileSize) {
        inform(QStringLiteral("warning"), T("dlg_file_too_large"),
               T("msg_file_too_large").arg(QString::number(originalSize / 1024.0 / 1024.0, 'f', 2)));
        return;
    }

    const QByteArray compressed = zlibCompress(content);
    const qint64 compressedSize = compressed.size();
    const double ratio = originalSize > 0 ? (1.0 - double(compressedSize) / originalSize) * 100.0 : 0.0;
    log(T("compression_log").arg(originalSize).arg(compressedSize).arg(fixed1(ratio)),
        QStringLiteral("info"));

    const quint64 serial = m_sessionSerial;
    auto proceed = [this, serial, bbs, fileName, originalSize, compressed, compressedSize, ratio] {
        if (!m_sessionActive || serial != m_sessionSerial)
            return;
        QString encoded = QString::fromLatin1(compressed.toBase64());
        bool encrypted = false;
        if (m_crypto.isEnabled()) {
            log(QStringLiteral("🔒 %1...").arg(T("encrypting_file")), QStringLiteral("info"));
            bool ok = true;
            const QString sealed = m_crypto.encrypt(encoded, &ok);
            if (!ok) {
                log(QStringLiteral("❌ %1").arg(T("encryption_failed_not_sent")), QStringLiteral("error"));
                return;
            }
            if (CryptoManager::isEncryptedMessage(sealed)) {
                // The <ENC> tags come off: the payload goes into FILEDATA.
                encoded = sealed.mid(5, sealed.size() - 11);
                encrypted = true;
            }
        }
        const QString nameB64 = QString::fromLatin1(fileName.toUtf8().toBase64());
        const QString flag = encrypted ? QStringLiteral("1") : QStringLiteral("0");
        const QString dataMessage = QStringLiteral("<FILEDATA>%1|%2|%3|%4|%5")
                                        .arg(flag, nameB64).arg(originalSize).arg(compressedSize)
                                        .arg(encoded)
                                    + kEof;
        const QString infoMessage = QStringLiteral("<FILEINFO>%1|%2|%3|%4|%5")
                                        .arg(flag, nameB64).arg(originalSize).arg(compressedSize)
                                        .arg(dataMessage.size())
                                    + kEof;
        if (encrypted && !bbs)
            log(QStringLiteral("🔒 %1: %2").arg(T("sending_encrypted_file"), fileName), QStringLiteral("info"));
        else
            log(QStringLiteral("%1: %2").arg(T("sending_metadata"), fileName), QStringLiteral("info"));
        sendText(infoMessage);

        m_tx = Outgoing();
        m_tx.bbs = bbs;
        m_tx.fileName = fileName;
        m_tx.originalSize = originalSize;
        m_tx.compressedSize = compressedSize;
        m_tx.compressionRatio = ratio;
        m_tx.payload = dataMessage.toUtf8();
        m_txPreparing = true;
        emit transferChanged();
        // Half a second for the metadata to go first, as before.
        QTimer::singleShot(500, this, [this, serial] {
            if (!m_txPreparing)
                return;
            if (!m_sessionActive || serial != m_sessionSerial) {
                m_txPreparing = false;
                emit transferChanged();
                return;
            }
            startFileTransfer();
        });
    };

    if (!bbs && compressedSize > kLargeFile) {
        const qint64 narrow = compressedSize / 400;   // ~400 B/s in NARROW
        const qint64 wide = compressedSize / 1200;    // ~1200 B/s in WIDE
        ask(T("dlg_large_file"),
            T("msg_large_file").arg(QString::number(qRound64(compressedSize / 1024.0)))
                .arg(narrow / 60).arg(narrow % 60).arg(wide / 60).arg(wide % 60),
            QString(), false, [this, proceed](bool yes) {
                if (yes)
                    proceed();
                else
                    log(T("send_cancelled_by_user"), QStringLiteral("warning"));
            });
        return;
    }
    proceed();
}

void Controller::startFileTransfer()
{
    m_txPreparing = false;
    m_tx.active = true;
    m_tx.pushed = 0;
    m_tx.percent = 0;
    m_tx.pushedAll = false;
    const qint64 total = m_tx.payload.size();
    setTransfer(true, QStringLiteral("📤 %1 %2...").arg(T("sending"), m_tx.fileName),
                QStringLiteral("active"), 0, double(total));
    if (!m_tx.bbs) {
        log(T("sending_file_log").arg(m_tx.fileName).arg(m_tx.originalSize).arg(m_tx.compressedSize),
            QStringLiteral("info"));
        log(QStringLiteral("%1 (%2 %3)").arg(T("sending_file_in_chunks")).arg(total).arg(T("bytes")),
            QStringLiteral("info"));
    }
    // 8 KB every 50 ms, as before, so VARA's input is never flooded.
    pushNextChunk();
    if (m_tx.active && !m_tx.pushedAll)
        m_chunkTimer.start();
}

void Controller::pushNextChunk()
{
    if (!m_tx.active || m_tx.pushedAll) {
        m_chunkTimer.stop();
        return;
    }
    const qint64 total = m_tx.payload.size();
    const QByteArray chunk = m_tx.payload.mid(int(m_tx.pushed), kChunkSize);
    if (!m_vara->sendData(chunk)) {
        m_chunkTimer.stop();
        abortTransfers(T("modem_not_connected"));
        return;
    }
    m_tx.pushed += chunk.size();
    if (m_tx.pushed >= total) {
        m_chunkTimer.stop();
        m_tx.pushedAll = true;
        m_tx.clock.start();
        if (!m_tx.bbs)
            log(QStringLiteral("%1 (%2 %3)").arg(T("data_sent_to_buffer")).arg(total).arg(T("bytes")),
                QStringLiteral("info"));
        else
            m_txWatchdog.start(kBbsSendTimeoutMs);
    }
}

void Controller::updateOutgoingProgress()
{
    if (!m_tx.active)
        return;
    const qint64 total = m_tx.payload.size();
    const qint64 done = total * m_tx.percent / 100;
    QString text = m_transferText;
    const double elapsed = m_tx.pushedAll ? m_tx.clock.elapsed() / 1000.0 : 0.0;
    if (elapsed > 1 && m_tx.percent > 0) {
        text = QStringLiteral("📤 %1 %2: %3% (%4 %5)")
                   .arg(T("sending"), m_tx.fileName).arg(m_tx.percent)
                   .arg(qRound64(done / elapsed)).arg(T("bytes_per_second"));
    }
    setTransfer(true, text, QStringLiteral("active"), double(done), double(total));
}

void Controller::finishOutgoing(bool timedOut)
{
    if (timedOut)
        log(QStringLiteral("⚠️ %1").arg(T("file_send_timeout")), QStringLiteral("warning"));
    m_chunkTimer.stop();
    m_txWatchdog.stop();
    const Outgoing done = m_tx;
    m_tx = Outgoing();
    const double total = double(done.payload.size());
    setTransfer(true, QStringLiteral("✅ %1: %2").arg(T("file_sent"), done.fileName),
                QStringLiteral("done"), total, total);
    scheduleTransferReset();
    if (done.bbs) {
        log(QStringLiteral("✅ %1: %2").arg(T("bbs_file_sent"), done.fileName), QStringLiteral("success"));
        return;
    }
    const QString who = myCall().isEmpty() ? T("me") : myCall();
    appendChat(chatPrefix(who)
                   + QStringLiteral("📎 %1").arg(T("chat_file_sent").arg(done.fileName)
                                                    .arg(done.originalSize)
                                                    .arg(fixed1(done.compressionRatio))),
               QStringLiteral("tx"));
    log(QStringLiteral("✓ %1: %2").arg(T("file_sent"), done.fileName), QStringLiteral("success"));
}

void Controller::abortTransfers(const QString &reason)
{
    const bool outgoing = m_tx.active || m_txPreparing;
    if (outgoing) {
        log(QStringLiteral("⚠️ %1: %2").arg(T("transfer_interrupted"), reason), QStringLiteral("warning"));
        m_chunkTimer.stop();
        m_txWatchdog.stop();
        m_tx = Outgoing();
        m_txPreparing = false;
    }
    if (m_rx.active)
        m_rx = Incoming();
    m_rxBuffer.clear();
    if (outgoing || m_transferVisible)
        setTransfer(false, T("file_no_transfer"), QStringLiteral("idle"), 0, 0);
}

void Controller::receiveFileInfo(const QString &info)
{
    // enc|name64|size|zsize|transfer  (older: no enc flag)
    const QStringList parts = splitFields(info, 4);
    QString flag, name64, sizeText, zsizeText, transferText;
    if (parts.size() == 4) {
        flag = QStringLiteral("0");
        name64 = parts.at(0); sizeText = parts.at(1); zsizeText = parts.at(2); transferText = parts.at(3);
    } else if (parts.size() == 5) {
        flag = parts.at(0); name64 = parts.at(1); sizeText = parts.at(2); zsizeText = parts.at(3);
        transferText = parts.at(4);
    } else {
        log(T("invalid_fileinfo_format"), QStringLiteral("error"));
        return;
    }
    QString fileName;
    qint64 originalSize = 0, compressedSize = 0, transferSize = 0;
    if (!decodeUtf8(QByteArray::fromBase64(name64.toLatin1()), fileName)
        || !parseSize(sizeText, originalSize) || !parseSize(zsizeText, compressedSize)
        || !parseSize(transferText, transferSize)) {
        log(QStringLiteral("%1: %2").arg(T("fileinfo_error"), info.left(80)), QStringLiteral("error"));
        return;
    }
    const bool encrypted = flag == QLatin1String("1");

    m_rx = Incoming();
    m_rx.active = true;
    m_rx.name = fileName;
    m_rx.total = transferSize;
    m_rx.encrypted = encrypted;
    m_rx.clock.start();

    if (encrypted) {
        setTransfer(true, QStringLiteral("📥 🔒 %1 %2 (%3)...").arg(T("receiving"), fileName, T("encrypted")),
                    QStringLiteral("active"), 0, double(transferSize));
        log(QStringLiteral("🔒 %1: %2").arg(T("receiving_encrypted_file"), fileName), QStringLiteral("info"));
    } else {
        setTransfer(true, QStringLiteral("📥 %1 %2...").arg(T("receiving"), fileName),
                    QStringLiteral("active"), 0, double(transferSize));
    }
    log(T("receiving_file_log").arg(fileName).arg(originalSize).arg(compressedSize).arg(transferSize),
        QStringLiteral("info"));
}

void Controller::receiveFileData(const QString &data)
{
    auto fail = [this](const QString &message) {
        log(message, QStringLiteral("error"));
        m_rx = Incoming();
        setTransfer(false, T("file_no_transfer"), QStringLiteral("idle"), 0, 0);
    };

    const QStringList parts = splitFields(data, 4);
    QString flag, name64, sizeText, zsizeText, encoded;
    if (parts.size() == 4) {
        flag = QStringLiteral("0");
        name64 = parts.at(0); sizeText = parts.at(1); zsizeText = parts.at(2); encoded = parts.at(3);
    } else if (parts.size() == 5) {
        flag = parts.at(0); name64 = parts.at(1); sizeText = parts.at(2); zsizeText = parts.at(3);
        encoded = parts.at(4);
    } else {
        fail(T("invalid_filedata_format"));
        return;
    }
    QString fileName;
    qint64 originalSize = 0, compressedSize = 0;
    if (!decodeUtf8(QByteArray::fromBase64(name64.toLatin1()), fileName)
        || !parseSize(sizeText, originalSize) || !parseSize(zsizeText, compressedSize)) {
        fail(T("invalid_filedata_format"));
        return;
    }
    const bool encrypted = flag == QLatin1String("1");
    const double total = m_rx.total > 0 ? double(m_rx.total) : 100.0;

    if (encrypted) {
        setTransfer(true, QStringLiteral("📥 🔒 %1 %2...").arg(T("decrypting"), fileName),
                    QStringLiteral("active"), total * 0.2, total);
        const CryptoManager::Decrypted d =
            m_crypto.decrypt(QStringLiteral("<ENC>%1</ENC>").arg(encoded));
        if (!d.ok()) {
            const QString reason = decryptError(d.status == CryptoManager::DecryptStatus::Disabled
                                                    ? CryptoManager::DecryptStatus::Disabled
                                                    : d.status);
            fail(QStringLiteral("❌ %1: %2").arg(T("cannot_decrypt_file"), reason));
            inform(QStringLiteral("critical"), T("dlg_decryption_error"), T("msg_cannot_decrypt_file").arg(fileName));
            return;
        }
        encoded = d.text;
        log(QStringLiteral("✅ %1").arg(T("file_decrypted")), QStringLiteral("success"));
    }

    setTransfer(true, QStringLiteral("📥 %1 %2...").arg(T("decoding"), fileName),
                QStringLiteral("active"), total * 0.3, total);
    const QByteArray compressed = QByteArray::fromBase64(encoded.toLatin1());
    if (compressed.size() != compressedSize) {
        fail(T("wrong_compressed_size").arg(compressed.size()).arg(compressedSize));
        return;
    }
    setTransfer(true, QStringLiteral("📥 %1 %2...").arg(T("decompressing"), fileName),
                QStringLiteral("active"), total * 0.6, total);
    QByteArray content;
    if (!zlibDecompress(compressed, originalSize, content)) {
        fail(QStringLiteral("%1: %2").arg(T("decompression_error"), T("corrupted_data")));
        return;
    }
    if (content.size() != originalSize) {
        fail(T("wrong_original_size").arg(content.size()).arg(originalSize));
        return;
    }
    finishIncoming(fileName, content, originalSize, compressedSize, false);
}

void Controller::receiveOldFile(const QString &data)
{
    // name64|size|zsize|data, the format before FILEINFO/FILEDATA.
    m_rx = Incoming();
    auto fail = [this](const QString &message) {
        log(message, QStringLiteral("error"));
        setTransfer(false, T("file_no_transfer"), QStringLiteral("idle"), 0, 0);
    };
    setTransfer(true, QStringLiteral("📥 %1...").arg(T("receiving_file")), QStringLiteral("active"), 0, 100);
    const QStringList parts = splitFields(data, 3);
    if (parts.size() != 4) {
        fail(T("invalid_file_format"));
        return;
    }
    QString fileName;
    qint64 originalSize = 0, compressedSize = 0;
    if (!parseSize(parts.at(1), originalSize) || !parseSize(parts.at(2), compressedSize)
        || !decodeUtf8(QByteArray::fromBase64(parts.at(0).toLatin1()), fileName)) {
        fail(T("invalid_file_format"));
        return;
    }
    setTransfer(true, QStringLiteral("📥 %1 %2...").arg(T("decoding"), fileName), QStringLiteral("active"), 30, 100);
    const QByteArray compressed = QByteArray::fromBase64(parts.at(3).toLatin1());
    if (compressed.size() != compressedSize) {
        fail(T("wrong_compressed_size").arg(compressed.size()).arg(compressedSize));
        return;
    }
    setTransfer(true, QStringLiteral("📥 %1 %2...").arg(T("decompressing"), fileName), QStringLiteral("active"), 60, 100);
    QByteArray content;
    if (!zlibDecompress(compressed, originalSize, content)) {
        fail(QStringLiteral("%1: %2").arg(T("decompression_error"), T("corrupted_data")));
        return;
    }
    if (content.size() != originalSize) {
        fail(T("wrong_original_size").arg(content.size()).arg(originalSize));
        return;
    }
    finishIncoming(fileName, content, originalSize, compressedSize, true);
}

void Controller::finishIncoming(const QString &fileName, const QByteArray &content,
                                qint64 originalSize, qint64 compressedSize, bool oldFormat)
{
    const double ratio = originalSize > 0 ? (1.0 - double(compressedSize) / originalSize) * 100.0 : 0.0;
    log(T("decompression_log").arg(compressedSize).arg(originalSize).arg(fixed1(ratio)),
        QStringLiteral("info"));
    const double total = oldFormat ? 100.0 : (m_rx.total > 0 ? double(m_rx.total) : 100.0);
    setTransfer(true, QStringLiteral("📥 %1...").arg(T("saving")), QStringLiteral("active"), total * 0.9, total);

    const QString safeName = safeFileName(fileName);
    const QString folder = m_settings.string(QStringLiteral("file_rx_path")).trimmed();
    if (!folder.isEmpty() && QFileInfo(folder).isDir()) {
        writeReceivedFile(uniquePath(folder, safeName), fileName, content, originalSize, ratio, oldFormat);
        return;
    }
    // No folder configured: ask, as before.
    askSaveFile(T("save_received_file"), safeName,
                [this, fileName, content, originalSize, ratio, oldFormat](const QString &path) {
        if (path.isEmpty()) {
            log(T("file_receive_cancelled"), QStringLiteral("warning"));
            m_rx = Incoming();
            setTransfer(false, T("file_no_transfer"), QStringLiteral("idle"), 0, 0);
            return;
        }
        writeReceivedFile(path, fileName, content, originalSize, ratio, oldFormat);
    });
}

void Controller::writeReceivedFile(const QString &path, const QString &fileName,
                                   const QByteArray &content, qint64 originalSize, double ratio,
                                   bool oldFormat)
{
    Q_UNUSED(oldFormat);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit()) {
        log(QStringLiteral("%1: %2").arg(T("file_reception_error"), file.errorString()),
            QStringLiteral("error"));
        m_rx = Incoming();
        setTransfer(false, T("file_no_transfer"), QStringLiteral("idle"), 0, 0);
        inform(QStringLiteral("critical"), T("dlg_error"),
               QStringLiteral("%1\n%2").arg(T("cannot_receive_file"), file.errorString()));
        return;
    }
    m_rx = Incoming();
    setTransfer(true, QStringLiteral("✅ %1: %2").arg(T("file_received"), fileName),
                QStringLiteral("done"), 1, 1);
    scheduleTransferReset();
    const QString remote = m_remoteStation.isEmpty() ? QStringLiteral("Station") : m_remoteStation;
    appendChat(chatPrefix(remote)
                   + QStringLiteral("📎 %1").arg(T("chat_file_received").arg(fileName)
                                                    .arg(originalSize).arg(fixed1(ratio))),
               QStringLiteral("rx"));
    log(QStringLiteral("✓ %1: %2").arg(T("file_received_saved"), QDir::toNativeSeparators(path)),
        QStringLiteral("success"));
    inform(QStringLiteral("information"), T("dlg_file_received"),
           T("msg_file_received").arg(QDir::toNativeSeparators(path)).arg(originalSize)
               .arg(fixed1(ratio)));
}

// ===================================================================== BBS

bool Controller::bbsFolderUsable(QString *folder) const
{
    const QString path = m_settings.string(QStringLiteral("bbs_folder")).trimmed();
    if (folder)
        *folder = path;
    return !path.isEmpty() && QFileInfo(path).isDir();
}

void Controller::sendBbsOn()
{
    if (!m_sessionActive)
        return;
    sendText(QStringLiteral("<BBSON>") + kEof);
    log(QStringLiteral("📁 %1").arg(T("bbs_beacon_sent")), QStringLiteral("info"));
}

void Controller::handleBbsListRequest()
{
    if (!m_settings.flag(QStringLiteral("bbs_enabled"))) {
        log(QStringLiteral("⚠️ %1").arg(T("bbs_disabled")), QStringLiteral("warning"));
        return;
    }
    QString folder;
    if (!bbsFolderUsable(&folder)) {
        log(QStringLiteral("⚠️ %1").arg(T("bbs_invalid_folder")), QStringLiteral("warning"));
        return;
    }
    QJsonArray list;
    const QFileInfoList entries =
        QDir(folder).entryInfoList(QDir::Files | QDir::Hidden | QDir::System, QDir::Name);
    for (const QFileInfo &entry : entries)
        list.append(QJsonObject{{QStringLiteral("name"), entry.fileName()},
                                {QStringLiteral("size"), double(entry.size())}});
    const QString json = QString::fromUtf8(QJsonDocument(list).toJson(QJsonDocument::Compact));
    sendText(QStringLiteral("<BBSFILES>%1</BBSFILES>").arg(json) + kEof);
    log(QStringLiteral("📁 %1 (%2 %3)").arg(T("bbs_list_sent")).arg(list.size()).arg(T("lbl_files")),
        QStringLiteral("info"));
}

void Controller::handleBbsFiles(const QString &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        log(QStringLiteral("❌ %1: %2").arg(T("bbs_parse_error"), error.errorString()),
            QStringLiteral("error"));
        m_bbsFiles.clear();
        return;
    }
    m_bbsFiles.clear();
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        m_bbsFiles.append(QVariantMap{{QStringLiteral("name"), object.value(QStringLiteral("name")).toString()},
                                      {QStringLiteral("size"), object.value(QStringLiteral("size")).toDouble()}});
    }
    log(QStringLiteral("📁 %1 (%2 %3)").arg(T("bbs_list_received")).arg(m_bbsFiles.size()).arg(T("lbl_files")),
        QStringLiteral("info"));
    // The window opens by itself when the list arrives.
    QTimer::singleShot(100, this, &Controller::showBbsWindow);
}

void Controller::handleBbsGetRequest(const QString &fileName)
{
    if (!m_settings.flag(QStringLiteral("bbs_enabled"))) {
        log(QStringLiteral("⚠️ %1").arg(T("bbs_disabled")), QStringLiteral("warning"));
        return;
    }
    QString folder;
    if (!bbsFolderUsable(&folder)) {
        log(QStringLiteral("⚠️ %1").arg(T("bbs_invalid_folder")), QStringLiteral("warning"));
        return;
    }
    // Only a file directly inside the shared folder, as listed.
    const QFileInfo info(QDir(folder), fileName);
    const bool inside = !fileName.contains(QLatin1Char('/')) && !fileName.contains(QLatin1Char('\\'))
                        && fileName != QLatin1String("..") && fileName != QLatin1String(".")
                        && QFileInfo(info.absolutePath()).canonicalFilePath()
                               == QFileInfo(folder).canonicalFilePath();
    if (!inside) {
        log(QStringLiteral("⚠️ %1: %2").arg(T("bbs_access_denied"), fileName), QStringLiteral("warning"));
        return;
    }
    if (!info.isFile()) {
        log(QStringLiteral("⚠️ %1: %2").arg(T("bbs_file_not_found"), fileName), QStringLiteral("warning"));
        return;
    }
    log(QStringLiteral("📤 %1: %2").arg(T("bbs_sending_file"), fileName), QStringLiteral("info"));
    prepareAndSendFile(info.absoluteFilePath(), true);
}

void Controller::openBbs()
{
    if (!m_remoteBbsActive) {
        inform(QStringLiteral("warning"), T("bbs_window_title"), T("bbs_not_active"));
        return;
    }
    if (m_bbsFiles.isEmpty()) {
        // Ask for the list; the window opens when it arrives.
        sendText(QStringLiteral("<BBSLIST>") + kEof);
        log(QStringLiteral("📁 %1").arg(T("bbs_requesting_list")), QStringLiteral("info"));
        return;
    }
    showBbsWindow();
}

void Controller::showBbsWindow()
{
    if (!m_sessionActive)
        return;
    QVector<QStringList> rows;
    for (const QVariant &value : std::as_const(m_bbsFiles)) {
        const QVariantMap file = value.toMap();
        const double size = file.value(QStringLiteral("size")).toDouble();
        QString text;
        if (size < 1024)
            text = QStringLiteral("%1 %2").arg(qint64(size)).arg(T("unit_bytes"));
        else if (size < 1024 * 1024)
            text = QStringLiteral("%1 %2").arg(fixed1(size / 1024)).arg(T("unit_kb"));
        else
            text = QStringLiteral("%1 %2").arg(fixed1(size / (1024 * 1024))).arg(T("unit_mb"));
        rows.append({file.value(QStringLiteral("name")).toString(), text});
    }
    m_bbs.setRows(rows);
    emit bbsWindowRequested(QStringLiteral("📁 %1 - %2").arg(T("bbs_window_title"), m_remoteStation));
}

void Controller::bbsDownload(const QString &fileName)
{
    if (!m_sessionActive || fileName.isEmpty())
        return;
    sendText(QStringLiteral("<BBSGET>%1</BBSGET>").arg(fileName) + kEof);
    log(QStringLiteral("📥 %1: %2").arg(T("bbs_downloading"), fileName), QStringLiteral("info"));
}

void Controller::bbsRefresh()
{
    if (!m_sessionActive)
        return;
    m_bbsFiles.clear();
    sendText(QStringLiteral("<BBSLIST>") + kEof);
    log(QStringLiteral("📁 %1").arg(T("bbs_refreshing")), QStringLiteral("info"));
}

// ================================================================ settings

void Controller::saveSettings()
{
    if (m_demo)
        return;
    QString error;
    if (!m_settings.save(m_settingsPath.isEmpty() ? Settings::defaultPath() : m_settingsPath, &error))
        log(QStringLiteral("%1: %2").arg(T("settings_save_error"), error), QStringLiteral("error"));
}

void Controller::applySettings(const QVariantMap &values)
{
    const Settings old = m_settings;
    m_settings.merge(values);
    // As the Python dialog did: callsign and locator in capitals.
    m_settings.set(QStringLiteral("mycall"), myCall().trimmed().toUpper());
    m_settings.set(QStringLiteral("grid_square"),
                   m_settings.string(QStringLiteral("grid_square")).trimmed().toUpper());
    saveSettings();
    I18n::instance().setPreference(m_settings.string(QStringLiteral("language")));
    emit stateChanged();
    emit preferencesChanged();

    const QString oldType = old.string(QStringLiteral("modem_type"));
    const QString newType = m_settings.string(QStringLiteral("modem_type"));
    if (oldType != newType) {
        // A different modem: unlink, close the VARA we started, start the
        // new one (whatever "start automatically" says, as before).
        if (modemConnected()) {
            log(T("disconnecting_from").arg(oldType), QStringLiteral("info"));
            if (m_sessionActive) {
                const QString remote = m_remoteStation;
                m_sessionActive = false;
                m_remoteStation.clear();
                handleDisconnected(remote);
            }
            m_vara->disconnectFromModem();
            markModemDown();
        }
        if (m_varaProcess->isRunning()) {
            log(T("closing_modem").arg(oldType), QStringLiteral("info"));
            m_varaProcess->stop();
        }
        log(T("starting_modem").arg(newType), QStringLiteral("info"));
        QTimer::singleShot(1000, this, &Controller::autoStartVara);
        return;
    }

    if (modemConnected()) {
        for (const char *key : {"host", "cmd_port", "data_port", "mycall"}) {
            if (old.value(QLatin1String(key)) != m_settings.value(QLatin1String(key))) {
                inform(QStringLiteral("information"), T("restart_required"), T("connection_params_changed"));
                break;
            }
        }
    }
}

void Controller::toggleTooltips()
{
    const bool enabled = !tooltips();
    m_settings.set(QStringLiteral("tooltips_enabled"), enabled);
    saveSettings();
    emit preferencesChanged();
    log(QStringLiteral("ℹ️ %1").arg(enabled ? T("tooltips_enabled") : T("tooltips_disabled")),
        QStringLiteral("info"));
}

void Controller::setTheme(const QString &theme)
{
    m_settings.set(QStringLiteral("theme"), theme);
    saveSettings();
    emit preferencesChanged();
}

// ============================================================ chat & logs

void Controller::saveChat()
{
    const QString content = m_chat.plainText();
    if (content.trimmed().isEmpty()) {
        inform(QStringLiteral("information"), T("dlg_empty_chat"), T("msg_empty_chat"));
        return;
    }
    const QString name = QStringLiteral("chat_%1.txt")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    askSaveFile(T("save_chat_title"), name, [this](const QString &path) {
        if (path.isEmpty())
            return;
        QSaveFile file(path);
        const QString text = QStringLiteral("=== CRYPTARA Chat ===\nDate: %1\nStation: %2\n\n%3")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                      myCall().isEmpty() ? QStringLiteral("N/A") : myCall(),
                                      m_chat.plainText());
        if (file.open(QIODevice::WriteOnly) && file.write(text.toUtf8()) >= 0 && file.commit()) {
            log(QStringLiteral("💾 %1: %2").arg(T("chat_saved"), QDir::toNativeSeparators(path)),
                QStringLiteral("success"));
            inform(QStringLiteral("information"), T("dlg_save_success"),
                   QStringLiteral("%1\n%2").arg(T("msg_chat_saved_to"), QDir::toNativeSeparators(path)));
        } else {
            log(QStringLiteral("❌ %1: %2").arg(T("chat_save_error"), file.errorString()), QStringLiteral("error"));
            inform(QStringLiteral("critical"), T("dlg_error"),
                   QStringLiteral("%1\n%2").arg(T("chat_save_error"), file.errorString()));
        }
    });
}

static bool writeLogFile(const QString &path, const QString &station, const QString &chat,
                         QString *error)
{
    QSaveFile file(path);
    const QString text = QStringLiteral("=== CRYPTARA Log ===\nDate: %1\nStation: %2\n\n%3")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                  station.isEmpty() ? QStringLiteral("N/A") : station, chat);
    if (file.open(QIODevice::WriteOnly) && file.write(text.toUtf8()) >= 0 && file.commit())
        return true;
    if (error)
        *error = file.errorString();
    return false;
}

static QString newLogPath()
{
    return QDir(Settings::configDir())
        .filePath(QStringLiteral("cryptara_log_%1.txt")
                      .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
}

void Controller::saveLog()
{
    // As before: the conversation, into the configuration folder.
    const QString path = newLogPath();
    QString error;
    if (writeLogFile(path, myCall(), m_chat.plainText(), &error))
        inform(QStringLiteral("information"), T("dlg_success"),
               QStringLiteral("%1: %2").arg(T("log_saved"), QDir::toNativeSeparators(path)));
    else
        inform(QStringLiteral("critical"), T("dlg_error"),
               QStringLiteral("%1: %2").arg(T("save_error"), error));
}

void Controller::showQsoLog()
{
    QVector<QStringList> rows;
    for (const Qso &qso : std::as_const(m_qsoLog)) {
        QStringList row{qso.callsign, qso.start.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))};
        if (qso.end.isValid()) {
            const qint64 seconds = qso.start.secsTo(qso.end);
            row << qso.end.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                << QStringLiteral("%1m %2s").arg(seconds / 60).arg(seconds % 60);
        } else {
            row << T("qso_in_progress") << QStringLiteral("-");
        }
        rows.append(row);
    }
    m_qso.setRows(rows);
    emit qsoLogRequested();
}

// ================================================================ shutdown

void Controller::shutdown()
{
    if (m_shutDown)
        return;
    if (modemConnected())
        m_vara->disconnectFromModem();
    if (m_varaProcess->isRunning()) {
        log(QStringLiteral("🛑 %1...").arg(T("closing_vara")), QStringLiteral("info"));
        m_varaProcess->stop();
    }
    // "Save log" in Settings > Interface: the conversation, on closing.
    if (m_settings.flag(QStringLiteral("save_log")) && !m_chat.plainText().trimmed().isEmpty())
        writeLogFile(newLogPath(), myCall(), m_chat.plainText(), nullptr);
    saveSettings();
    m_shutDown = true;
}

// ================================================================== helpers

QString Controller::toLocalPath(const QString &url) const
{
    return QDir::toNativeSeparators(localPath(url));
}

QString Controller::toFileUrl(const QString &path) const
{
    return QUrl::fromLocalFile(QDir::fromNativeSeparators(path)).toString();
}

QString Controller::folderUrlOf(const QString &filePath) const
{
    return QUrl::fromLocalFile(QFileInfo(QDir::fromNativeSeparators(filePath)).absolutePath()).toString();
}

void Controller::loadDemo()
{
    // What the screenshots show: a session in VARA HF, a few messages, CQs
    // and log lines.  Settings are neither read nor written.
    m_demo = true;
    m_settings.set(QStringLiteral("mycall"), QStringLiteral("N0CALL"));
    m_connectedModemType = kHf;
    m_sessionActive = true;
    m_remoteStation = QStringLiteral("W1AW");
    m_cryptoButton = true;
    m_remoteBbsActive = true;
    m_ptt = true;
    const QString me = QStringLiteral("N0CALL");
    appendChat(QStringLiteral("[14:02:11] W1AW: Good afternoon, signals 59 here."), QStringLiteral("rx"));
    appendChat(QStringLiteral("[14:02:40] %1: Thanks, all good. Switching to encryption.").arg(me), QStringLiteral("tx"));
    appendChat(QStringLiteral("[14:03:05] W1AW 🔒: Secure session established on my side."), QStringLiteral("rx"));
    appendChat(QStringLiteral("[14:03:31] %1 🔒: Sending the exercise report now.").arg(me), QStringLiteral("tx"));
    appendChat(QStringLiteral("[14:05:12] %1: 📎 %2").arg(me, T("chat_file_sent").arg(QStringLiteral("report.pdf")).arg(48211).arg(QStringLiteral("37.4"))),
               QStringLiteral("tx"));
    m_cq.addOrUpdate({QStringLiteral("K1ABC"), QStringLiteral("2300"), QString(), QString(),
                      QDateTime::currentDateTime().addSecs(-600)});
    m_cq.addOrUpdate({QStringLiteral("F4XYZ"), QStringLiteral("500"), QString(), QString(),
                      QDateTime::currentDateTime().addSecs(-180)});
    m_cq.addOrUpdate({QStringLiteral("W1AW"), QStringLiteral("2300"), QString(), QString(),
                      QDateTime::currentDateTime().addSecs(-60)});
    log(T("connected_to_modem").arg(kHf, QStringLiteral("localhost")).arg(8300), QStringLiteral("success"));
    log(QStringLiteral("CMD: VERSION 4.8.9"), QStringLiteral("debug"));
    log(QStringLiteral("📡 %1 W1AW").arg(T("cq_received_from")), QStringLiteral("info"));
    log(QStringLiteral("✓ %1 W1AW").arg(T("connected_to")), QStringLiteral("success"));
    log(QStringLiteral("🔒 %1").arg(T("crypto_enabled")), QStringLiteral("success"));
    log(QStringLiteral("⚠️ %1").arg(T("file_send_timeout")), QStringLiteral("warning"));
    log(QStringLiteral("%1: WRONG").arg(T("vara_error")), QStringLiteral("error"));
    setTransfer(true, QStringLiteral("📤 %1 report.pdf: 62% (412 %2)").arg(T("sending"), T("bytes_per_second")),
                QStringLiteral("active"), 62, 100);
    emit stateChanged();
}

void Controller::demoQuestion()
{
    ask(T("dlg_encryption_request"), T("msg_wants_encrypted_session").arg(QStringLiteral("W1AW")),
        QString(), true, [](bool) {});
}
