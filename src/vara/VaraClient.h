// VaraClient.h - the two TCP links to a VARA modem (HF, FM or SAT).
//
// VARA listens on a command port (8300) for text commands ended by CR and
// answers on the same socket, and on a data port (8301) for the payload of
// the session.  This class only moves bytes and lines; what they mean is
// the Controller's business.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>

class QTcpSocket;

class VaraClient : public QObject
{
    Q_OBJECT
public:
    explicit VaraClient(QObject *parent = nullptr);
    ~VaraClient() override;

    // Opens both links; connected() or connectFailed() follows.  timeoutMs
    // applies to the pair, as the 5-second socket timeout of the Python
    // version did.
    void connectToModem(const QString &host, quint16 commandPort, quint16 dataPort,
                        int timeoutMs = 5000);
    void disconnectFromModem();

    bool isConnected() const { return m_connected; }
    bool isConnecting() const { return m_connecting; }
    QString host() const { return m_host; }
    quint16 commandPort() const { return m_commandPort; }

    // Appends the CR terminator.  False when not connected.
    bool sendCommand(const QString &command);
    bool sendData(const QByteArray &data);
    // Bytes handed to sendData() and not yet passed to the system.
    qint64 dataBytesPending() const;

signals:
    void connected();
    void connectFailed(const QString &error);
    // The modem went away (VARA closed, link broken) after connected().
    void connectionLost(const QString &error);
    void commandReceived(const QString &line);
    void dataReceived(const QByteArray &data);

private:
    void onSocketConnected();
    void onSocketError();
    void onCommandReadyRead();
    void onDataReadyRead();
    void onSocketDisconnected();
    void teardown();

    QTcpSocket *m_command = nullptr;
    QTcpSocket *m_data = nullptr;
    QTimer m_timeout;
    QByteArray m_lineBuffer;
    QString m_host;
    quint16 m_commandPort = 0;
    bool m_connected = false;
    bool m_connecting = false;
};
