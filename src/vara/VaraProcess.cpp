// VaraProcess.cpp
#include "VaraProcess.h"

#include <QFileInfo>
#include <QProcess>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

VaraProcess::VaraProcess(QObject *parent) : QObject(parent) {}

VaraProcess::~VaraProcess()
{
    // A VARA started by us does not outlive us unless stop() was skipped;
    // the destructor of QProcess would kill it abruptly, so detach instead.
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect();
        m_process->setParent(nullptr);
        m_process = nullptr;
    }
}

bool VaraProcess::start(const QString &modemType, const QString &executable, QString *error)
{
    const QFileInfo info(executable);
    if (executable.isEmpty() || !info.exists()) {
        if (error)
            *error = tr("executable not found: %1").arg(executable);
        return false;
    }
    if (isRunning())
        stop();

    m_modemType = modemType;
    delete m_process;
    m_process = new QProcess(this);
    m_process->setProgram(info.absoluteFilePath());
    // VARA keeps its VARA.ini next to the program.
    m_process->setWorkingDirectory(info.absolutePath());
    m_process->setProcessChannelMode(QProcess::ForwardedChannels);
#ifdef Q_OS_WIN
    // The flags the Python version used: STARTF_USESHOWWINDOW with SW_HIDE,
    // and CREATE_NO_WINDOW.
    m_process->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) {
            args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
            args->startupInfo->wShowWindow = SW_HIDE;
            args->flags |= CREATE_NO_WINDOW;
        });
#endif
    m_process->start();
    if (!m_process->waitForStarted(5000)) {
        if (error)
            *error = m_process->errorString();
        delete m_process;
        m_process = nullptr;
        return false;
    }
    return true;
}

void VaraProcess::stop()
{
    if (!m_process)
        return;
    if (m_process->state() != QProcess::NotRunning) {
        // On Windows terminate() posts WM_CLOSE: VARA closes as if its
        // window were closed, saving its settings.
        m_process->terminate();
        if (!m_process->waitForFinished(5000)) {
            m_process->kill();
            m_process->waitForFinished(2000);
        }
    }
    delete m_process;
    m_process = nullptr;
}

bool VaraProcess::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}
