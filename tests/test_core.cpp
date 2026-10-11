// test_core.cpp - settings file, translations, and two Controllers talking
// through simulated VARA modems: call, chat, encryption with Diffie-Hellman,
// files, BBS, auto-responder.  On Windows this runs the CNG backend for real.
#include "FakeVara.h"
#include "app/Controller.h"
#include "app/I18n.h"
#include "app/Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <functional>

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

bool waitFor(const std::function<bool()> &condition, int timeoutMs = 10000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > timeoutMs)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return true;
}

void pause(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

// ------------------------------------------------------------------ settings

void testSettings()
{
    // Exactly what Python's configparser writes (see tests/interop).
    const QString python = QStringLiteral(
        "[CRYPTARA]\nmodem_type = VARA FM\ncmd_port = 8300\nvara_hf_path = C:\\VARA\\VARA.exe\n"
        "auto_message = Bonjour {CALLSIGN}, merci.\n\t\n\tLigne 3 : 73 !\nlisten = False\n"
        "macro_f1 = Rapport 100%% ok; #pas un commentaire\nqth = Nice, France = C\u00f4te d'Azur\n"
        "; a comment\nbbs_enabled = true\n\n[OTHER]\nmycall = NOPE\n");
    const auto values = Settings::parseIni(python, QStringLiteral("CRYPTARA"));
    CHECK(values.value(QStringLiteral("modem_type")) == QStringLiteral("VARA FM"));
    CHECK(values.value(QStringLiteral("vara_hf_path")) == QStringLiteral("C:\\VARA\\VARA.exe"));
    CHECK(values.value(QStringLiteral("auto_message"))
          == QStringLiteral("Bonjour {CALLSIGN}, merci.\n\nLigne 3 : 73 !"));
    CHECK(values.value(QStringLiteral("macro_f1")) == QStringLiteral("Rapport 100% ok; #pas un commentaire"));
    CHECK(values.value(QStringLiteral("qth")) == QStringLiteral("Nice, France = C\u00f4te d'Azur"));
    CHECK(!values.contains(QStringLiteral("mycall")));

    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cryptara_settings.conf"));
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly));
    file.write(python.toUtf8());
    file.close();

    Settings settings;
    CHECK(settings.load(path));
    CHECK(settings.string(QStringLiteral("modem_type")) == QStringLiteral("VARA FM"));
    CHECK(settings.integer(QStringLiteral("cmd_port")) == 8300);
    CHECK(settings.flag(QStringLiteral("listen")) == false);
    CHECK(settings.flag(QStringLiteral("bbs_enabled")) == true);
    CHECK(settings.integer(QStringLiteral("data_port")) == 8301);       // default kept
    CHECK(settings.string(QStringLiteral("theme")) == QStringLiteral("light"));

    // Round trip, multi-line and percent signs included.
    settings.set(QStringLiteral("macro_f2"), QStringLiteral("50% done\nsecond line"));
    CHECK(settings.save(path));
    Settings again;
    CHECK(again.load(path));
    CHECK(again.toMap() == settings.toMap());
    QFile written(path);
    CHECK(written.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(written.readAll());
    CHECK(text.startsWith(QStringLiteral("[CRYPTARA]\nmodem_type = VARA FM\n")));
    CHECK(text.contains(QStringLiteral("macro_f2 = 50%% done\n\tsecond line\n")));
    CHECK(text.contains(QStringLiteral("listen = False\n")));
}

// -------------------------------------------------------------- translations

void testTranslations()
{
    const QStringList keys = I18n::keys();
    CHECK(QSet<QString>(keys.begin(), keys.end()).size() == keys.size());   // no duplicates

    // Every key used by the sources exists.
    const QDir root(QStringLiteral(CRYPTARA_SOURCE_DIR));
    static const QRegularExpression cppKey(QStringLiteral("\\bT\\(\"([a-z0-9_]+)\"\\)"));
    static const QRegularExpression qmlKey(QStringLiteral("\\bs\\.([a-z][a-z0-9_]+)"));
    int used = 0;
    QDirIterator it(root.filePath(QStringLiteral("src")), {QStringLiteral("*.cpp"), QStringLiteral("*.qml")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QString source = QString::fromUtf8(file.readAll());
        const auto &re = path.endsWith(QLatin1String(".qml")) ? qmlKey : cppKey;
        auto matches = re.globalMatch(source);
        while (matches.hasNext()) {
            const QString key = matches.next().captured(1);
            ++used;
            if (!I18n::hasKey(key)) {
                std::fprintf(stderr, "missing translation key '%s' in %s\n", qPrintable(key),
                             qPrintable(QFileInfo(path).fileName()));
                ++g_failures;
            }
        }
    }
    CHECK(used > 300);

    // Same placeholders in both languages.
    I18n &i18n = I18n::instance();
    static const QRegularExpression placeholder(QStringLiteral("%\\d"));
    i18n.setPreference(QStringLiteral("en"));
    QMap<QString, QStringList> english;
    for (const QString &key : keys) {
        QStringList found;
        auto m = placeholder.globalMatch(i18n.t(key.toLatin1().constData()));
        while (m.hasNext())
            found << m.next().captured(0);
        found.sort();
        english.insert(key, found);
    }
    i18n.setPreference(QStringLiteral("fr"));
    for (const QString &key : keys) {
        QStringList found;
        auto m = placeholder.globalMatch(i18n.t(key.toLatin1().constData()));
        while (m.hasNext())
            found << m.next().captured(0);
        found.sort();
        if (found != english.value(key)) {
            std::fprintf(stderr, "placeholders differ for '%s'\n", qPrintable(key));
            ++g_failures;
        }
    }
    CHECK(i18n.t("btn_send") == QStringLiteral("Envoyer"));
    i18n.setPreference(QStringLiteral("en"));
    CHECK(i18n.t("btn_send") == QStringLiteral("Send"));
}

// ------------------------------------------------------------------ stations

struct Station
{
    Controller controller;
    QStringList chat;
    QStringList log;
    QStringList asked;
    bool answerYes = true;
    QString password;
    QString saveDir;
    bool bbsWindow = false;

    explicit Station(const QString &settingsPath)
    {
        QObject::connect(&controller, &Controller::chatLine, [this](const QString &t, const QString &k) {
            chat << k + QLatin1Char(' ') + t;
        });
        QObject::connect(&controller, &Controller::logLine, [this](const QString &t, const QString &) {
            log << t;
        });
        QObject::connect(&controller, &Controller::messageRequested,
                         [this](int id, const QString &, const QString &title, const QString &text,
                                const QString &, bool question, bool) {
            asked << title + QStringLiteral(" | ") + text;
            if (question)
                QTimer::singleShot(0, &controller, [this, id] { controller.answer(id, answerYes); });
        });
        QObject::connect(&controller, &Controller::passwordRequested, [this](int id, const QString &, const QString &) {
            asked << QStringLiteral("password");
            QTimer::singleShot(0, &controller, [this, id] {
                controller.answerText(id, !password.isEmpty(), password);
            });
        });
        QObject::connect(&controller, &Controller::saveFileRequested,
                         [this](int id, const QString &, const QString &name, const QString &) {
            const QString path = QDir(saveDir).filePath(name);
            QTimer::singleShot(0, &controller, [this, id, path] { controller.answerFile(id, path); });
        });
        QObject::connect(&controller, &Controller::bbsWindowRequested, [this](const QString &) {
            bbsWindow = true;
        });
        controller.start(settingsPath, false);
    }

    bool chatHas(const QString &needle) const
    {
        for (const QString &line : chat)
            if (line.contains(needle))
                return true;
        return false;
    }
    bool logHas(const QString &needle) const
    {
        for (const QString &line : log)
            if (line.contains(needle))
                return true;
        return false;
    }
};

QString writeSettings(const QString &dir, const QString &call, quint16 cmd, quint16 data,
                      const QList<QPair<QString, QString>> &extra = {})
{
    QList<QPair<QString, QString>> items = {
        {QStringLiteral("modem_type"), QStringLiteral("VARA HF")},
        {QStringLiteral("host"), QStringLiteral("127.0.0.1")},
        {QStringLiteral("cmd_port"), QString::number(cmd)},
        {QStringLiteral("data_port"), QString::number(data)},
        {QStringLiteral("mycall"), call},
        {QStringLiteral("auto_start_vara"), QStringLiteral("False")},
        {QStringLiteral("language"), QStringLiteral("en")},
    };
    items += extra;
    const QString path = QDir(dir).filePath(QStringLiteral("cryptara_settings.conf"));
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(Settings::writeIni(QStringLiteral("CRYPTARA"), items).toUtf8());
    return path;
}

QByteArray randomBytes(int size)
{
    QByteArray bytes(size, '\0');
    for (int i = 0; i < size; ++i)
        bytes[i] = char(QRandomGenerator::global()->bounded(256));
    return bytes;
}

void testTwoStations()
{
    QTemporaryDir dirA, dirB;
    FakeModem modemA, modemB;
    modemA.setPeer(&modemB);
    modemB.setPeer(&modemA);

    // B shares a folder and keeps the session (auto_disconnect off).
    QDir(dirB.path()).mkpath(QStringLiteral("bbs"));
    const QByteArray shared = randomBytes(30000) + QByteArray(20000, 'x');
    {
        QFile f(QDir(dirB.path()).filePath(QStringLiteral("bbs/shared file \u00e9.bin")));
        f.open(QIODevice::WriteOnly);
        f.write(shared);
    }
    QDir(dirA.path()).mkpath(QStringLiteral("rx"));
    QDir(dirB.path()).mkpath(QStringLiteral("rx"));

    Station a(writeSettings(dirA.path(), QStringLiteral("N0AAA"), modemA.commandPort(), modemA.dataPort(),
                            {{QStringLiteral("timestamp"), QStringLiteral("False")}}));
    Station b(writeSettings(dirB.path(), QStringLiteral("N0BBB"), modemB.commandPort(), modemB.dataPort(),
                            {{QStringLiteral("bbs_enabled"), QStringLiteral("True")},
                             {QStringLiteral("bbs_folder"), QDir(dirB.path()).filePath(QStringLiteral("bbs"))},
                             {QStringLiteral("auto_disconnect"), QStringLiteral("False")},
                             {QStringLiteral("file_rx_path"), QDir(dirB.path()).filePath(QStringLiteral("rx"))},
                             {QStringLiteral("timestamp"), QStringLiteral("False")}}));
    a.saveDir = QDir(dirA.path()).filePath(QStringLiteral("rx"));
    a.password = b.password = QStringLiteral("shared secret 73");

    // ---- link to the modems, start-up sequence
    a.controller.toggleModem();
    b.controller.toggleModem();
    CHECK(waitFor([&] { return a.controller.modemConnected() && b.controller.modemConnected(); }));
    CHECK(waitFor([&] { return modemA.commands.contains(QStringLiteral("LISTEN CQ")); }));
    CHECK(modemA.commands.mid(0, 6) == (QStringList{QStringLiteral("CHAT ON"), QStringLiteral("MYCALL N0AAA"),
                                                     QStringLiteral("VERSION"), QStringLiteral("LISTEN ON"),
                                                     QStringLiteral("LISTEN CQ"), QStringLiteral("COMPRESSION ON")}));
    CHECK(waitFor([&] { return a.logHas(QStringLiteral("VARA version: 4.8.9-fake")); }));

    // ---- CQ
    b.controller.sendCq(QString(), QString(), -1);
    CHECK(waitFor([&] { return a.logHas(QStringLiteral("CQ received from N0BBB")); }));
    CHECK(modemB.commands.contains(QStringLiteral("CQFRAME N0BBB 2300")));

    // ---- call
    a.controller.callStation(QStringLiteral("n0bbb"), QString(), QString(), 0);
    CHECK(waitFor([&] { return a.controller.sessionActive() && b.controller.sessionActive(); }));
    CHECK(modemA.commands.contains(QStringLiteral("BW500")));
    CHECK(modemA.commands.contains(QStringLiteral("CONNECT N0AAA N0BBB")));
    CHECK(a.controller.remoteStation() == QStringLiteral("N0BBB"));
    CHECK(b.controller.remoteStation() == QStringLiteral("N0AAA"));
    // B's BBS beacon.
    CHECK(waitFor([&] { return a.controller.remoteBbs(); }));

    // ---- clear chat, both ways; our own line appears once delivered
    CHECK(a.controller.sendMessage(QStringLiteral("Hello B, \u00e9\u00e8 \U0001F4FB")));
    CHECK(waitFor([&] { return b.chatHas(QStringLiteral("rx N0AAA: Hello B, \u00e9\u00e8 \U0001F4FB")); }));
    CHECK(waitFor([&] { return a.chatHas(QStringLiteral("tx N0AAA: Hello B")); }));
    CHECK(b.controller.sendMessage(QStringLiteral("Hello A")));
    CHECK(waitFor([&] { return a.chatHas(QStringLiteral("rx N0BBB: Hello A")); }));

    // ---- encryption: password, then Diffie-Hellman on A's request
    a.controller.toggleCrypto();
    CHECK(waitFor([&] { return a.controller.crypto().sessionEstablished()
                               && b.controller.crypto().sessionEstablished(); }, 15000));
    CHECK(a.controller.cryptoOn() && b.controller.cryptoOn());
    CHECK(a.controller.crypto().sessionKey() == b.controller.crypto().sessionKey());
    CHECK(a.controller.sendMessage(QStringLiteral("encrypted hello")));
    CHECK(waitFor([&] { return b.chatHas(QStringLiteral("rx N0AAA \U0001F512: encrypted hello")); }));
    CHECK(b.controller.sendMessage(QStringLiteral("encrypted reply")));
    CHECK(waitFor([&] { return a.chatHas(QStringLiteral("rx N0BBB \U0001F512: encrypted reply")); }));

    // ---- an encrypted file, A to B (B saves into its folder)
    const QByteArray payload = randomBytes(40000) + QByteArray(60000, 'A');
    const QString source = QDir(dirA.path()).filePath(QStringLiteral("report \u00e7a.dat"));
    {
        QFile f(source);
        f.open(QIODevice::WriteOnly);
        f.write(payload);
    }
    a.controller.sendFile(source);
    CHECK(waitFor([&] { return b.chatHas(QStringLiteral("File received: report \u00e7a.dat")); }, 30000));
    CHECK(waitFor([&] { return a.chatHas(QStringLiteral("File sent: report \u00e7a.dat")); }, 10000));
    {
        QFile f(QDir(dirB.path()).filePath(QStringLiteral("rx/report \u00e7a.dat")));
        CHECK(f.open(QIODevice::ReadOnly) && f.readAll() == payload);
    }

    // ---- BBS: list, then download (A has no folder: the save prompt)
    a.controller.openBbs();
    CHECK(waitFor([&] { return a.bbsWindow; }));
    a.controller.bbsDownload(QStringLiteral("shared file \u00e9.bin"));
    CHECK(waitFor([&] { return a.chatHas(QStringLiteral("File received: shared file \u00e9.bin")); }, 30000));
    {
        QFile f(QDir(a.saveDir).filePath(QStringLiteral("shared file \u00e9.bin")));
        CHECK(f.open(QIODevice::ReadOnly) && f.readAll() == shared);
    }
    // Outside the shared folder: refused.
    a.controller.bbsDownload(QStringLiteral("../cryptara_settings.conf"));
    CHECK(waitFor([&] { return b.logHas(QStringLiteral("Access denied")); }));

    // ---- B switches encryption off; A agrees
    b.controller.toggleCrypto();
    CHECK(waitFor([&] { return !a.controller.cryptoOn(); }));
    CHECK(waitFor([&] { return b.asked.join(QLatin1Char('\n')).contains(QStringLiteral("Synchronization successful")); }));
    CHECK(a.controller.sendMessage(QStringLiteral("clear again")));
    CHECK(waitFor([&] { return b.chatHas(QStringLiteral("rx N0AAA: clear again")); }));

    // ---- and back on (passwords known: CRYPTON / CRYPTONACK)
    b.controller.toggleCrypto();
    CHECK(waitFor([&] { return a.controller.cryptoOn(); }));
    CHECK(a.controller.sendMessage(QStringLiteral("encrypted once more")));
    CHECK(waitFor([&] { return b.chatHas(QStringLiteral("rx N0AAA \U0001F512: encrypted once more")); }));

    // ---- macro
    a.controller.saveMacros({QStringLiteral("73 de {MYCALL} to {CALLSIGN}")});
    a.controller.sendMacro(1);
    CHECK(waitFor([&] { return b.chatHas(QStringLiteral("73 de N0AAA to N0BBB")); }));

    // ---- hang up: both sides back to idle, the DH session forgotten
    a.controller.disconnectSession();
    CHECK(waitFor([&] { return !a.controller.sessionActive() && !b.controller.sessionActive(); }));
    CHECK(!a.controller.cryptoOn() && !a.controller.crypto().sessionEstablished());
    CHECK(!a.controller.remoteBbs());

    // ---- auto-responder on B, for A's incoming call; then B hangs up
    QVariantMap bs = b.controller.settingsMap();
    bs.insert(QStringLiteral("auto_enabled"), true);
    bs.insert(QStringLiteral("auto_message"), QStringLiteral("Auto {CALLSIGN} de {MYCALL}"));
    bs.insert(QStringLiteral("auto_disconnect"), true);
    bs.insert(QStringLiteral("auto_delay"), 1);
    b.controller.applySettings(bs);
    a.chat.clear();
    a.controller.callStation(QStringLiteral("N0BBB"), QString(), QString(), -1);
    CHECK(waitFor([&] { return a.chatHas(QStringLiteral("rx N0BBB: Auto N0AAA de N0BBB")); }, 15000));
    CHECK(waitFor([&] { return !a.controller.sessionActive() && !b.controller.sessionActive(); }, 15000));

    // ---- the caller's own auto-responder stays quiet on outgoing calls
    QVariantMap as = a.controller.settingsMap();
    as.insert(QStringLiteral("auto_enabled"), true);
    as.insert(QStringLiteral("auto_message"), QStringLiteral("SHOULD NOT BE SENT"));
    a.controller.applySettings(as);
    bs.insert(QStringLiteral("auto_enabled"), false);
    b.controller.applySettings(bs);
    b.chat.clear();
    a.controller.callStation(QStringLiteral("N0BBB"), QString(), QString(), -1);
    CHECK(waitFor([&] { return a.controller.sessionActive(); }));
    pause(2500);
    CHECK(!b.chatHas(QStringLiteral("SHOULD NOT BE SENT")));
    a.controller.disconnectSession();
    CHECK(waitFor([&] { return !a.controller.sessionActive(); }));

    a.controller.shutdown();
    b.controller.shutdown();

    if (qEnvironmentVariableIsSet("CRYPTARA_TEST_VERBOSE")) {
        for (const Station *station : {&a, &b}) {
            std::printf("==== %s chat\n", qPrintable(station->controller.myCall()));
            for (const QString &line : station->chat)
                std::printf("  %s\n", qPrintable(line));
            std::printf("==== %s log\n", qPrintable(station->controller.myCall()));
            for (const QString &line : station->log)
                if (!line.contains(QLatin1String("CMD: BUFFER")) && !line.contains(QLatin1String("CMD: PTT"))
                    && !line.contains(QLatin1String("CMD: OK")))
                    std::printf("  %s\n", qPrintable(line));
            std::printf("==== %s dialogs\n", qPrintable(station->controller.myCall()));
            for (const QString &line : station->asked)
                std::printf("  %s\n", qPrintable(QString(line).replace(QLatin1Char('\n'), QLatin1Char(' '))));
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QElapsedTimer timer;
    timer.start();
    testSettings();
    testTranslations();
    testTwoStations();
    std::printf("%s (%d checks, %d failure(s), %lld ms)\n", g_failures ? "FAILED" : "PASSED", g_checks,
                g_failures, static_cast<long long>(timer.elapsed()));
    return g_failures ? 1 : 0;
}
