// Controller.h - CRYPTARA's application logic, behind the QML interface.
//
// This is MainWindow of the Python version without the widgets: the VARA
// session, the encryption handshake, file transfer, BBS, CQ, auto-responder
// and macros.  The protocol on the data link is unchanged, so this version
// talks to stations running the Python one:
//
//   every message ends with <EOF>
//   <ENC>...</ENC>                       encrypted text (CryptoManager)
//   <CRYPTREQ>key</CRYPTREQ>             encryption request + our DH key
//   <CRYPTACK>key</CRYPTACK>             accepted + their DH key
//   <CRYPTREFUSED></CRYPTREFUSED>        refused
//   <CRYPTOFF></CRYPTOFF>, <CRYPTOFFACK></CRYPTOFFACK>,
//   <CRYPTOFFREFUSED></CRYPTOFFREFUSED>, <CRYPTON></CRYPTON>,
//   <CRYPTONACK></CRYPTONACK>            switching encryption off and on
//   <DHKEY>key</DHKEY>                   older key exchange, still answered
//   <FILEINFO>enc|name64|size|zsize|transfer
//   <FILEDATA>enc|name64|size|zsize|data  zlib + base64 (+ encryption)
//   <FILE>name64|size|zsize|data         older file format, still read
//   <BBSON>, <BBSLIST>, <BBSFILES>json</BBSFILES>, <BBSGET>name</BBSGET>
//
// Where the Python version stopped on a modal QMessageBox, this one asks
// the QML for an answer (messageRequested and friends) and carries on in a
// callback when it comes.
#pragma once

#include "Models.h"
#include "Settings.h"
#include "crypto/CryptoManager.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <functional>

class VaraClient;
class VaraProcess;

class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY stateChanged)
    Q_PROPERTY(QString myCall READ myCall NOTIFY stateChanged)
    Q_PROPERTY(bool modemConnected READ modemConnected NOTIFY stateChanged)
    Q_PROPERTY(bool modemConnecting READ modemConnecting NOTIFY stateChanged)
    Q_PROPERTY(QString modemType READ modemType NOTIFY stateChanged)
    Q_PROPERTY(bool sessionActive READ sessionActive NOTIFY stateChanged)
    Q_PROPERTY(QString remoteStation READ remoteStation NOTIFY stateChanged)
    Q_PROPERTY(bool ptt READ ptt NOTIFY stateChanged)
    Q_PROPERTY(bool cryptoOn READ cryptoOn NOTIFY stateChanged)
    Q_PROPERTY(bool remoteBbs READ remoteBbs NOTIFY stateChanged)
    Q_PROPERTY(bool digipeatersEnabled READ digipeatersEnabled NOTIFY stateChanged)
    Q_PROPERTY(QStringList bandwidths READ bandwidths NOTIFY stateChanged)
    Q_PROPERTY(int defaultBandwidthIndex READ defaultBandwidthIndex NOTIFY stateChanged)
    Q_PROPERTY(bool bandwidthEnabled READ bandwidthEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool transferVisible READ transferVisible NOTIFY transferChanged)
    Q_PROPERTY(bool transferBusy READ transferBusy NOTIFY transferChanged)
    Q_PROPERTY(QString transferText READ transferText NOTIFY transferChanged)
    Q_PROPERTY(QString transferState READ transferState NOTIFY transferChanged)
    Q_PROPERTY(double transferValue READ transferValue NOTIFY transferChanged)
    Q_PROPERTY(double transferMaximum READ transferMaximum NOTIFY transferChanged)
    Q_PROPERTY(bool tooltips READ tooltips NOTIFY preferencesChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY preferencesChanged)
    Q_PROPERTY(QObject *chatModel READ chatModel CONSTANT)
    Q_PROPERTY(QObject *logModel READ logModel CONSTANT)
    Q_PROPERTY(QObject *cqModel READ cqModel CONSTANT)
    Q_PROPERTY(QObject *bbsModel READ bbsModel CONSTANT)
    Q_PROPERTY(QObject *qsoModel READ qsoModel CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString cryptoBackend READ cryptoBackend CONSTANT)

public:
    explicit Controller(QObject *parent = nullptr);
    ~Controller() override;

    // Load the settings from path (default location when empty) and start
    // as the Python version did: launch VARA or connect, if a callsign is set.
    void start(const QString &settingsPath = QString(), bool autoStart = true);

    QString windowTitle() const;
    QString myCall() const { return m_settings.string(QStringLiteral("mycall")); }
    bool modemConnected() const;
    bool modemConnecting() const;
    QString modemType() const;
    bool sessionActive() const { return m_sessionActive; }
    QString remoteStation() const { return m_remoteStation; }
    bool ptt() const { return m_ptt; }
    bool cryptoOn() const { return m_cryptoButton; }
    bool remoteBbs() const { return m_remoteBbsActive; }
    bool digipeatersEnabled() const;
    QStringList bandwidths() const;
    int defaultBandwidthIndex() const;
    bool bandwidthEnabled() const;
    bool transferVisible() const { return m_transferVisible; }
    bool transferBusy() const { return m_tx.active || m_txPreparing; }
    QString transferText() const { return m_transferText; }
    QString transferState() const { return m_transferState; }
    double transferValue() const { return m_transferValue; }
    double transferMaximum() const { return m_transferMaximum; }
    bool tooltips() const { return m_settings.flag(QStringLiteral("tooltips_enabled")); }
    QString theme() const { return m_settings.string(QStringLiteral("theme")); }
    QObject *chatModel() { return &m_chat; }
    QObject *logModel() { return &m_log; }
    QObject *cqModel() { return &m_cq; }
    QObject *bbsModel() { return &m_bbs; }
    QObject *qsoModel() { return &m_qso; }
    QString version() const;
    QString cryptoBackend() const;

    // ---- actions from the interface
    Q_INVOKABLE void toggleModem();
    Q_INVOKABLE void callStation(const QString &station, const QString &via1,
                                 const QString &via2, int bandwidthIndex);
    Q_INVOKABLE void disconnectSession();
    Q_INVOKABLE void sendCq(const QString &via1, const QString &via2, int bandwidthIndex);
    Q_INVOKABLE void clearCq();
    Q_INVOKABLE void connectToCq(int row, int bandwidthIndex);
    // True when the text was handed to the modem (the input then clears).
    Q_INVOKABLE bool sendMessage(const QString &text);
    Q_INVOKABLE void sendMacro(int number);
    Q_INVOKABLE void toggleCrypto();
    Q_INVOKABLE void sendFile(const QString &pathOrUrl);
    Q_INVOKABLE void openBbs();
    Q_INVOKABLE void bbsDownload(const QString &fileName);
    Q_INVOKABLE void bbsRefresh();
    Q_INVOKABLE void saveChat();
    Q_INVOKABLE void saveLog();
    Q_INVOKABLE void showQsoLog();
    Q_INVOKABLE void toggleTooltips();
    Q_INVOKABLE void setTheme(const QString &theme);

    Q_INVOKABLE QVariantMap settingsMap() const { return m_settings.toMap(); }
    Q_INVOKABLE void applySettings(const QVariantMap &values);
    Q_INVOKABLE QStringList macros() const;
    Q_INVOKABLE void saveMacros(const QStringList &macros);
    Q_INVOKABLE void testMacro(const QString &text);

    // Answers to the requests below.
    Q_INVOKABLE void answer(int id, bool yes);
    Q_INVOKABLE void answerText(int id, bool ok, const QString &text);
    Q_INVOKABLE void answerFile(int id, const QString &pathOrUrl);

    // Window closing: link down, VARA stopped if we started it, settings saved.
    Q_INVOKABLE void shutdown();

    // Paths for the file dialogs.
    Q_INVOKABLE QString toLocalPath(const QString &url) const;
    Q_INVOKABLE QString toFileUrl(const QString &path) const;
    Q_INVOKABLE QString folderUrlOf(const QString &filePath) const;

    // Screenshot mode (--screenshot): sample content, nothing saved.
    Q_INVOKABLE void loadDemo();
    Q_INVOKABLE void demoQuestion();

    // For the tests: the engine without its QML.
    Settings &settings() { return m_settings; }
    CryptoManager &crypto() { return m_crypto; }
    void setSettingsPath(const QString &path) { m_settingsPath = path; }

signals:
    void stateChanged();
    void transferChanged();
    void preferencesChanged();
    // icon: "information", "warning", "critical", "question".  A question
    // is answered Yes/No through answer(); the others with OK.
    void messageRequested(int id, const QString &icon, const QString &title,
                          const QString &text, const QString &informative,
                          bool question, bool defaultYes);
    void passwordRequested(int id, const QString &title, const QString &prompt);
    void saveFileRequested(int id, const QString &title, const QString &fileName,
                           const QString &folder);
    void bbsWindowRequested(const QString &title);
    void qsoLogRequested();
    void stationFieldsRequested(const QString &callsign, const QString &via1,
                                const QString &via2);
    // The plain text of every chat and log line, for the headless tests.
    void chatLine(const QString &text, const QString &kind);
    void logLine(const QString &text, const QString &level);

private:
    // ---- dialogs
    using Answer = std::function<void(bool)>;
    using TextAnswer = std::function<void(bool, const QString &)>;
    using FileAnswer = std::function<void(const QString &)>;
    void ask(const QString &title, const QString &text, const QString &informative,
             bool defaultYes, Answer then, const QString &icon = QStringLiteral("question"));
    void inform(const QString &icon, const QString &title, const QString &text,
                const QString &informative = QString());
    void askPassword(const QString &title, const QString &prompt, TextAnswer then);
    void askSaveFile(const QString &title, const QString &fileName, FileAnswer then);

    // ---- output
    void log(const QString &message, const QString &level = QStringLiteral("info"));
    void appendChat(const QString &text, const QString &kind);
    QString stamp() const;
    QString chatPrefix(const QString &who) const;

    // ---- modem
    void connectToVara();
    void autoStartVara();
    void onModemConnected();
    void onModemConnectFailed(const QString &error);
    void onModemLost(const QString &error);
    void markModemDown();
    void handleCommand(const QString &command);
    void handleData(const QByteArray &data);
    void dispatchMessage(const QString &message);
    void checkConnectionTimeout();
    QString executableFor(const QString &modemType) const;
    bool sendText(const QString &payload);
    int bandwidthHz(int index) const;

    // ---- session
    void handleConnected(const QString &callsign, bool incoming);
    void handleDisconnected(const QString &callsign);
    void addReceivedCq(const QString &callsign, const QString &bandwidth,
                       const QString &via1, const QString &via2);
    void doCall(const QString &station, const QString &via1, const QString &via2,
                int bandwidthIndex);

    // ---- encryption
    void initiateCryptoRequest();
    void handleCryptoRequest(const QString &peerKey);
    void handleCryptoAck(const QString &peerKey);
    void handleCryptoRefused();
    void handleCryptoOff();
    void handleCryptoOffAck();
    void handleCryptoOffRefused();
    void handleCryptoOn();
    void handleCryptoOnAck();
    void handleDhKey(const QString &peerKey);
    void setCryptoButton(bool on);
    QString remoteOr(const QString &fallback) const;
    QString decryptError(CryptoManager::DecryptStatus status) const;

    // ---- files
    struct Outgoing
    {
        bool active = false;
        bool bbs = false;
        QString fileName;
        qint64 originalSize = 0;
        qint64 compressedSize = 0;
        double compressionRatio = 0;
        QByteArray payload;      // the whole <FILEDATA>...<EOF> message
        qint64 pushed = 0;       // bytes handed to the modem so far
        int percent = 0;
        QElapsedTimer clock;     // since the payload was fully pushed
        bool pushedAll = false;
    };
    struct Incoming
    {
        bool active = false;
        QString name;
        qint64 total = 0;
        bool encrypted = false;
        QElapsedTimer clock;
    };
    void prepareAndSendFile(const QString &path, bool bbs);
    void startFileTransfer();
    void pushNextChunk();
    void updateOutgoingProgress();
    void finishOutgoing(bool timedOut);
    void abortTransfers(const QString &reason);
    void receiveFileInfo(const QString &info);
    void receiveFileData(const QString &data);
    void receiveOldFile(const QString &data);
    void finishIncoming(const QString &fileName, const QByteArray &content,
                        qint64 originalSize, qint64 compressedSize, bool oldFormat);
    void writeReceivedFile(const QString &path, const QString &fileName, const QByteArray &content,
                           qint64 originalSize, double ratio, bool oldFormat);
    void setTransfer(bool visible, const QString &text, const QString &state,
                     double value, double maximum);
    void scheduleTransferReset();
    static QString safeFileName(const QString &name);
    static QString uniquePath(const QString &folder, const QString &fileName);

    // ---- BBS
    void sendBbsOn();
    void handleBbsListRequest();
    void handleBbsFiles(const QString &json);
    void handleBbsGetRequest(const QString &fileName);
    void showBbsWindow();
    bool bbsFolderUsable(QString *folder) const;

    // ---- auto-responder, macros
    void triggerAutoResponder();
    QString expandMacro(const QString &text, const QString &callsignFallback = QString()) const;

    // ---- settings
    void saveSettings();

    Settings m_settings;
    QString m_settingsPath;
    CryptoManager m_crypto;
    VaraClient *m_vara = nullptr;
    VaraProcess *m_varaProcess = nullptr;
    QString m_connectedModemType;   // type of the modem we are linked to
    int m_connectRetries = 0;

    // session state (VARAClient attributes in the Python version)
    bool m_sessionActive = false;
    quint64 m_sessionSerial = 0;    // changes with every connection: late
                                    // answers to a dialog are then ignored
    bool m_callPlaced = false;      // we sent CONNECT and wait for VARA
    bool m_pendingConnect = false;
    bool m_channelBusy = false;
    QString m_remoteStation;
    bool m_ptt = false;
    bool m_cryptoButton = false;
    bool m_remoteBbsActive = false;
    bool m_autoResponderActive = false;
    bool m_autoDisconnectPending = false;
    QStringList m_pendingMessages;  // shown once VARA reports BUFFER 0

    QByteArray m_rxBuffer;
    Outgoing m_tx;
    bool m_txPreparing = false;     // waiting on a question or the FILEINFO delay
    Incoming m_rx;
    QTimer m_chunkTimer;
    QTimer m_txWatchdog;
    QTimer m_transferReset;
    bool m_transferVisible = false;
    QString m_transferText;
    QString m_transferState = QStringLiteral("idle");
    double m_transferValue = 0;
    double m_transferMaximum = 0;

    struct Qso { QString callsign; QDateTime start; QDateTime end; };
    QVector<Qso> m_qsoLog;
    QVariantList m_bbsFiles;        // [{name, size}]

    LineModel m_chat{5000};
    LineModel m_log{5000};
    CqModel m_cq;
    TableModel m_bbs{{QStringLiteral("name"), QStringLiteral("size")}};
    TableModel m_qso{{QStringLiteral("callsign"), QStringLiteral("start"),
                      QStringLiteral("end"), QStringLiteral("duration")}};

    int m_nextRequest = 1;
    QHash<int, Answer> m_answers;
    QHash<int, TextAnswer> m_textAnswers;
    QHash<int, FileAnswer> m_fileAnswers;
    bool m_shutDown = false;
    bool m_demo = false;
};
