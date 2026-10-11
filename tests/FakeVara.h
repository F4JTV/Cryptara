// FakeVara.h - two simulated VARA modems linked by a perfect radio path.
//
// Each modem listens on a command port and a data port like VARA does
// (MYCALL, LISTEN, CHAT, COMPRESSION, VERSION, BW, CONNECT, DISCONNECT,
// ABORT, CQFRAME), connects the two stations when one calls the other's
// MYCALL, and moves data to the other side a few kilobytes at a time,
// reporting BUFFER and PTT as the real one does.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class FakeModem : public QObject
{
    Q_OBJECT
public:
    explicit FakeModem(QObject *parent = nullptr) : QObject(parent)
    {
        m_command.listen(QHostAddress::LocalHost, 0);
        m_data.listen(QHostAddress::LocalHost, 0);
        connect(&m_command, &QTcpServer::newConnection, this, [this] {
            m_cmd = m_command.nextPendingConnection();
            connect(m_cmd, &QTcpSocket::readyRead, this, &FakeModem::readCommands);
        });
        connect(&m_data, &QTcpServer::newConnection, this, [this] {
            m_dat = m_data.nextPendingConnection();
            connect(m_dat, &QTcpSocket::readyRead, this, &FakeModem::readData);
        });
        m_air.setInterval(20);
        connect(&m_air, &QTimer::timeout, this, &FakeModem::transmit);
    }

    void setPeer(FakeModem *peer) { m_peer = peer; }
    quint16 commandPort() const { return m_command.serverPort(); }
    quint16 dataPort() const { return m_data.serverPort(); }
    QString myCall() const { return m_myCall; }
    QStringList commands;   // every command received, for the checks

    void say(const QString &line)
    {
        if (m_cmd)
            m_cmd->write(line.toUtf8() + '\r');
    }
    void deliver(const QByteArray &bytes)
    {
        if (m_dat)
            m_dat->write(bytes);
    }

private:
    void readCommands()
    {
        m_lines += m_cmd->readAll();
        int end;
        while ((end = m_lines.indexOf('\r')) >= 0) {
            const QString line = QString::fromUtf8(m_lines.left(end)).trimmed();
            m_lines.remove(0, end + 1);
            if (!line.isEmpty())
                handle(line);
        }
    }

    void handle(const QString &line)
    {
        commands << line;
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString verb = parts.value(0);
        if (verb == QLatin1String("MYCALL")) {
            m_myCall = parts.value(1);
            say(QStringLiteral("OK"));
        } else if (verb == QLatin1String("VERSION")) {
            say(QStringLiteral("VERSION 4.8.9-fake"));
        } else if (verb == QLatin1String("CONNECT") && parts.size() >= 3) {
            const QString source = parts.at(1);
            const QString destination = parts.at(2);
            say(QStringLiteral("OK"));
            if (m_peer && m_peer->m_myCall.compare(destination, Qt::CaseInsensitive) == 0) {
                QTimer::singleShot(150, this, [this, source, destination] {
                    const QString connected = QStringLiteral("CONNECTED %1 %2 2300").arg(source, destination);
                    m_session = m_peer->m_session = true;
                    say(connected);
                    m_peer->say(connected);
                });
            }
        } else if (verb == QLatin1String("DISCONNECT") || verb == QLatin1String("ABORT")) {
            say(QStringLiteral("OK"));
            if (m_session) {
                // What is still in the buffers goes first, as VARA does.
                QTimer::singleShot(m_buffer.isEmpty() ? 100 : 600, this, [this] {
                    m_session = false;
                    say(QStringLiteral("DISCONNECTED"));
                    if (m_peer) {
                        m_peer->m_session = false;
                        m_peer->say(QStringLiteral("DISCONNECTED"));
                    }
                    m_buffer.clear();
                });
            }
        } else if (verb == QLatin1String("CQFRAME")) {
            say(QStringLiteral("OK"));
            if (m_peer && !m_peer->m_session)
                m_peer->say(line);
        } else {
            say(QStringLiteral("OK"));
        }
    }

    void readData()
    {
        const QByteArray bytes = m_dat->readAll();
        if (!m_session)
            return;   // VARA drops data outside a session
        m_buffer += bytes;
        say(QStringLiteral("BUFFER %1").arg(m_buffer.size()));
        if (!m_air.isActive())
            m_air.start();
    }

    void transmit()
    {
        if (m_buffer.isEmpty() || !m_session) {
            m_air.stop();
            if (m_ptt) {
                m_ptt = false;
                say(QStringLiteral("PTT OFF"));
            }
            return;
        }
        if (!m_ptt) {
            m_ptt = true;
            say(QStringLiteral("PTT ON"));
        }
        const QByteArray frame = m_buffer.left(6000);
        m_buffer.remove(0, frame.size());
        if (m_peer)
            m_peer->deliver(frame);
        say(QStringLiteral("BUFFER %1").arg(m_buffer.size()));
    }

    QTcpServer m_command;
    QTcpServer m_data;
    QTcpSocket *m_cmd = nullptr;
    QTcpSocket *m_dat = nullptr;
    QByteArray m_lines;
    QByteArray m_buffer;
    QTimer m_air;
    FakeModem *m_peer = nullptr;
    QString m_myCall;
    bool m_session = false;
    bool m_ptt = false;
};
