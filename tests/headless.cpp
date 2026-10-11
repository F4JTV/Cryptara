// headless.cpp - the Controller without its window, for the interop tests.
//
// Reads commands on stdin, one per line, and prints what the window would
// show on stdout:
//   CHAT <kind> <text>          a chat line
//   LOG <level> <text>          a system log line
//   ASK <kind> <title> | <text> a dialog, answered at once by the policy
//   STATE key=value ...         after every state change
//   BBSWINDOW <title>           the BBS window opened, with its rows
//   ROW <cells, tab separated>
//
// Commands:
//   start <settings file>       load the settings (no auto start)
//   set <key> <value>           change a setting (applied at once)
//   connect | disconnect-modem
//   call <station> [via1] [via2]
//   send <text>                 send a message
//   macro <n>                   F<n>
//   crypto                      the encryption button
//   file <path>                 send a file
//   bbs | bbsget <name> | bbsrefresh
//   hangup                      disconnect the session
//   answer yes|no               how questions are answered (default yes)
//   password <text>             what password prompts receive
//   savedir <folder>            where save prompts put files
//   quit
#include "app/Controller.h"
#include "app/Models.h"

#include <QCoreApplication>
#include <QDir>
#include <QSocketNotifier>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <cstdio>

#ifndef Q_OS_WIN
#  include <unistd.h>
#endif

namespace {

void out(const QString &line)
{
    QByteArray bytes = line.toUtf8();
    bytes.replace('\n', "\\n");
    std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Controller controller;
    bool answerYes = true;
    QString password;
    QString saveDir = QDir::tempPath();

    QObject::connect(&controller, &Controller::chatLine, [](const QString &text, const QString &kind) {
        out(QStringLiteral("CHAT %1 %2").arg(kind, text));
    });
    QObject::connect(&controller, &Controller::logLine, [](const QString &text, const QString &level) {
        out(QStringLiteral("LOG %1 %2").arg(level, text));
    });
    QObject::connect(&controller, &Controller::stateChanged, [&controller] {
        out(QStringLiteral("STATE modem=%1 session=%2 remote=%3 crypto=%4 bbs=%5 ptt=%6")
                .arg(controller.modemConnected()).arg(controller.sessionActive())
                .arg(controller.remoteStation().isEmpty() ? QStringLiteral("-") : controller.remoteStation())
                .arg(controller.cryptoOn()).arg(controller.remoteBbs()).arg(controller.ptt()));
    });
    QObject::connect(&controller, &Controller::transferChanged, [&controller] {
        out(QStringLiteral("TRANSFER %1 %2").arg(controller.transferState(), controller.transferText()));
    });
    QObject::connect(&controller, &Controller::messageRequested,
                     [&](int id, const QString &icon, const QString &title, const QString &text,
                         const QString &informative, bool question, bool) {
        out(QStringLiteral("ASK %1 %2 | %3 %4").arg(question ? QStringLiteral("question") : icon, title, text,
                                                     informative));
        if (question)
            QTimer::singleShot(0, &controller, [&controller, id, answerYes] { controller.answer(id, answerYes); });
    });
    QObject::connect(&controller, &Controller::passwordRequested,
                     [&](int id, const QString &title, const QString &) {
        out(QStringLiteral("ASK password %1").arg(title));
        QTimer::singleShot(0, &controller, [&controller, id, password] {
            controller.answerText(id, !password.isEmpty(), password);
        });
    });
    QObject::connect(&controller, &Controller::saveFileRequested,
                     [&](int id, const QString &, const QString &fileName, const QString &) {
        const QString path = QDir(saveDir).filePath(fileName);
        out(QStringLiteral("ASK save %1").arg(path));
        QTimer::singleShot(0, &controller, [&controller, id, path] { controller.answerFile(id, path); });
    });
    QObject::connect(&controller, &Controller::bbsWindowRequested, [&controller](const QString &title) {
        out(QStringLiteral("BBSWINDOW %1").arg(title));
        auto *model = qobject_cast<TableModel *>(controller.bbsModel());
        for (int row = 0; model && row < model->count(); ++row)
            out(QStringLiteral("ROW %1\t%2").arg(model->cell(row, 0), model->cell(row, 1)));
    });

    auto handle = [&](const QString &line) {
        const QString command = line.section(QLatin1Char(' '), 0, 0);
        const QString rest = line.section(QLatin1Char(' '), 1);
        if (command == QLatin1String("start")) {
            controller.start(rest, false);
        } else if (command == QLatin1String("set")) {
            const QString key = rest.section(QLatin1Char(' '), 0, 0);
            const QString value = rest.section(QLatin1Char(' '), 1);
            QVariantMap map = controller.settingsMap();
            const QVariant old = map.value(key);
            if (old.typeId() == QMetaType::Bool)
                map.insert(key, value == QLatin1String("1") || value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
            else if (old.typeId() == QMetaType::Int)
                map.insert(key, value.toInt());
            else
                map.insert(key, value);
            controller.applySettings(map);
        } else if (command == QLatin1String("connect") || command == QLatin1String("disconnect-modem")) {
            controller.toggleModem();
        } else if (command == QLatin1String("call")) {
            const QStringList parts = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            controller.callStation(parts.value(0), parts.value(1), parts.value(2), -1);
        } else if (command == QLatin1String("send")) {
            out(QStringLiteral("SENT %1").arg(controller.sendMessage(rest)));
        } else if (command == QLatin1String("macro")) {
            controller.sendMacro(rest.toInt());
        } else if (command == QLatin1String("crypto")) {
            controller.toggleCrypto();
        } else if (command == QLatin1String("file")) {
            controller.sendFile(rest);
        } else if (command == QLatin1String("bbs")) {
            controller.openBbs();
        } else if (command == QLatin1String("bbsget")) {
            controller.bbsDownload(rest);
        } else if (command == QLatin1String("bbsrefresh")) {
            controller.bbsRefresh();
        } else if (command == QLatin1String("cq")) {
            controller.sendCq(QString(), QString(), -1);
        } else if (command == QLatin1String("hangup")) {
            controller.disconnectSession();
        } else if (command == QLatin1String("answer")) {
            answerYes = rest.trimmed() != QLatin1String("no");
        } else if (command == QLatin1String("password")) {
            password = rest;
        } else if (command == QLatin1String("savedir")) {
            saveDir = rest;
        } else if (command == QLatin1String("quit")) {
            controller.shutdown();
            QCoreApplication::quit();
        } else if (!command.isEmpty()) {
            out(QStringLiteral("ERROR unknown command %1").arg(command));
        }
    };

    // stdin, line by line, from the event loop.
#ifdef Q_OS_WIN
    // QSocketNotifier does not work on console handles: poll a thread instead.
    QThread *reader = QThread::create([&] {
        QTextStream in(stdin);
        QString line;
        while (in.readLineInto(&line))
            QMetaObject::invokeMethod(&app, [&, line] { handle(line.trimmed()); }, Qt::QueuedConnection);
        QMetaObject::invokeMethod(&app, [] { QCoreApplication::quit(); }, Qt::QueuedConnection);
    });
    reader->start();
#else
    QSocketNotifier notifier(fileno(stdin), QSocketNotifier::Read);
    QByteArray pending;
    QObject::connect(&notifier, &QSocketNotifier::activated, [&] {
        char buffer[4096];
        const ssize_t n = ::read(fileno(stdin), buffer, sizeof buffer);
        if (n <= 0) {
            notifier.setEnabled(false);
            QCoreApplication::quit();
            return;
        }
        pending.append(buffer, int(n));
        int end;
        while ((end = pending.indexOf('\n')) >= 0) {
            const QString line = QString::fromUtf8(pending.left(end)).trimmed();
            pending.remove(0, end + 1);
            handle(line);
        }
    });
#endif
    out(QStringLiteral("READY"));
    return app.exec();
}
