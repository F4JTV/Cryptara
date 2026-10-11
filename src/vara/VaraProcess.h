// VaraProcess.h - starts and stops the VARA program when CRYPTARA launches it.
#pragma once

#include <QObject>
#include <QString>

class QProcess;

class VaraProcess : public QObject
{
    Q_OBJECT
public:
    explicit VaraProcess(QObject *parent = nullptr);
    ~VaraProcess() override;

    // Starts the executable.  False with *error on failure.
    bool start(const QString &modemType, const QString &executable, QString *error = nullptr);
    // Asks VARA to close (WM_CLOSE), then forces it after 5 seconds.
    void stop();
    bool isRunning() const;
    QString modemType() const { return m_modemType; }

private:
    QProcess *m_process = nullptr;
    QString m_modemType;
};
