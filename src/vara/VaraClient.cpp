// VaraClient.cpp
#include "VaraClient.h"

#include <QTcpSocket>

VaraClient::VaraClient(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (!m_connecting)
            return;
        teardown();
        emit connectFailed(tr("timed out"));
    });
}

VaraClient::~VaraClient()
{
    teardown();
}

void VaraClient::connectToModem(const QString &host, quint16 commandPort, quint16 dataPort,
                                int timeoutMs)
{
    teardown();
    m_host = host;
    m_commandPort = commandPort;
    m_connecting = true;

    m_command = new QTcpSocket(this);
    m_data = new QTcpSocket(this);
    for (QTcpSocket *socket : {m_command, m_data}) {
        // VARA is fed small commands and must see them at once.
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        connect(socket, &QTcpSocket::connected, this, &VaraClient::onSocketConnected);
        connect(socket, &QTcpSocket::errorOccurred, this, &VaraClient::onSocketError);
        connect(socket, &QTcpSocket::disconnected, this, &VaraClient::onSocketDisconnected);
    }
    connect(m_command, &QTcpSocket::readyRead, this, &VaraClient::onCommandReadyRead);
    connect(m_data, &QTcpSocket::readyRead, this, &VaraClient::onDataReadyRead);

    m_timeout.start(timeoutMs);
    m_command->connectToHost(host, commandPort);
    m_data->connectToHost(host, dataPort);
}

void VaraClient::disconnectFromModem()
{
    teardown();
}

void VaraClient::teardown()
{
    m_timeout.stop();
    m_connected = false;
    m_connecting = false;
    m_lineBuffer.clear();
    for (QTcpSocket **socket : {&m_command, &m_data}) {
        if (!*socket)
            continue;
        (*socket)->disconnect(this);
        (*socket)->abort();
        (*socket)->deleteLater();
        *socket = nullptr;
    }
}

void VaraClient::onSocketConnected()
{
    if (!m_connecting || !m_command || !m_data)
        return;
    if (m_command->state() == QAbstractSocket::ConnectedState
        && m_data->state() == QAbstractSocket::ConnectedState) {
        m_timeout.stop();
        m_connecting = false;
        m_connected = true;
        emit connected();
    }
}

void VaraClient::onSocketError()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    const QString error = socket ? socket->errorString() : tr("socket error");
    if (m_connecting) {
        teardown();
        emit connectFailed(error);
    } else if (m_connected) {
        teardown();
        emit connectionLost(error);
    }
}

void VaraClient::onSocketDisconnected()
{
    if (m_connected) {
        teardown();
        emit connectionLost(tr("the modem closed the connection"));
    }
}

void VaraClient::onCommandReadyRead()
{
    if (!m_command)
        return;
    m_lineBuffer += m_command->readAll();
    // Lines end with CR (VARA) or LF; blank ones are skipped.
    for (;;) {
        int end = -1;
        for (int i = 0; i < m_lineBuffer.size(); ++i) {
            if (m_lineBuffer.at(i) == '\r' || m_lineBuffer.at(i) == '\n') {
                end = i;
                break;
            }
        }
        if (end < 0)
            break;
        const QString line = QString::fromUtf8(m_lineBuffer.left(end)).trimmed();
        m_lineBuffer.remove(0, end + 1);
        if (!line.isEmpty())
            emit commandReceived(line);
        if (!m_command)   // a handler may have torn the link down
            return;
    }
}

void VaraClient::onDataReadyRead()
{
    if (!m_data)
        return;
    const QByteArray data = m_data->readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

bool VaraClient::sendCommand(const QString &command)
{
    if (!m_connected || !m_command)
        return false;
    return m_command->write(command.toUtf8() + '\r') >= 0;
}

bool VaraClient::sendData(const QByteArray &data)
{
    if (!m_connected || !m_data)
        return false;
    return m_data->write(data) == data.size();
}

qint64 VaraClient::dataBytesPending() const
{
    return m_data ? m_data->bytesToWrite() : 0;
}
