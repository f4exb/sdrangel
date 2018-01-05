///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "command.h"
#include "util/simpleserializer.h"

#include <QKeySequence>
#include <QProcess>

Command::Command() :
    m_currentProcess(0),
    m_currentProcessState(QProcess::NotRunning),
    m_isInError(false),
    m_currentProcessError(QProcess::UnknownError),
    m_hasExited(false),
    m_currentProcessExitCode(0),
    m_currentProcessExitStatus(QProcess::NormalExit),
    m_currentProcessPid(0)
{
    m_currentProcessStartTimeStamp.tv_sec = 0;
    m_currentProcessStartTimeStamp.tv_usec = 0;
    m_currentProcessFinishTimeStamp.tv_sec = 0;
    m_currentProcessFinishTimeStamp.tv_usec = 0;

    resetToDefaults();
}

Command::Command(const Command& command) :
        QObject(),
        m_group(command.m_group),
        m_description(command.m_description),
        m_command(command.m_command),
        m_argString(command.m_argString),
        m_key(command.m_key),
        m_keyModifiers(command.m_keyModifiers),
        m_associateKey(command.m_associateKey),
        m_release(command.m_release),
        m_currentProcess(0),
        m_currentProcessState(QProcess::NotRunning),
        m_isInError(false),
        m_currentProcessError(QProcess::UnknownError),
        m_hasExited(false),
        m_currentProcessExitCode(0),
        m_currentProcessExitStatus(QProcess::NormalExit),
        m_currentProcessPid(0)
{
    m_currentProcessStartTimeStamp.tv_sec = 0;
    m_currentProcessStartTimeStamp.tv_usec = 0;
    m_currentProcessFinishTimeStamp.tv_sec = 0;
    m_currentProcessFinishTimeStamp.tv_usec = 0;
}

Command::~Command()
{
    if (m_currentProcess)
    {
#if QT_VERSION < 0x051000
        disconnect(m_currentProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#else
        disconnect(m_currentProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#endif
        disconnect(m_currentProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
        disconnect(m_currentProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));
        m_currentProcess->deleteLater();
    }
}

void Command::resetToDefaults()
{
    m_group = "default";
    m_description = "no name";
    m_command = "";
    m_argString = "";
    m_key = static_cast<Qt::Key>(0);
    m_keyModifiers = Qt::NoModifier,
    m_associateKey = false;
    m_release = false;
}

QByteArray Command::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_group);
    s.writeString(2, m_description);
    s.writeString(3, m_command);
    s.writeString(4, m_argString);
    s.writeS32(5, (int) m_key);
    s.writeS32(6, (int) m_keyModifiers);
    s.writeBool(7, m_associateKey);
    s.writeBool(8, m_release);

    return s.final();
}


bool Command::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        int tmpInt;

        d.readString(1, &m_group, "default");
        d.readString(2, &m_description, "no name");
        d.readString(3, &m_command, "");
        d.readString(4, &m_argString, "");
        d.readS32(5, &tmpInt, 0);
        m_key = static_cast<Qt::Key>(tmpInt);
        d.readS32(6, &tmpInt, 0);
        m_keyModifiers = static_cast<Qt::KeyboardModifiers>(tmpInt);
        d.readBool(7, &m_associateKey, false);
        d.readBool(8, &m_release, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QString Command::getKeyLabel() const
{
    if (m_key == 0)
    {
       return "";
    }
    else if (m_keyModifiers != Qt::NoModifier)
    {
        QString altGrStr = m_keyModifiers & Qt::GroupSwitchModifier ? "Gr " : "";
        int maskedModifiers = (m_keyModifiers & 0x3FFFFFFF) + ((m_keyModifiers & 0x40000000)>>3);
        return altGrStr + QKeySequence(maskedModifiers, m_key).toString();
    }
    else
    {
        return QKeySequence(m_key).toString();
    }
}

void Command::run(const QString& apiAddress, int apiPort, int deviceSetIndex)
{
    if (m_currentProcess)
    {
        qWarning("Command::run: process already running");
        return;
    }

    QString args = m_argString;

    if (m_argString.contains("%1"))
    {
        args = args.arg(apiAddress);
    }

    if (m_argString.contains("%2"))
    {
        args.replace("%2", "%1");
        args = args.arg(apiPort);
    }

    if (m_argString.contains("%3"))
    {
        args.replace("%3", "%1");
        args = args.arg(deviceSetIndex);
    }

    m_currentProcessCommandLine = QString("%1 %2").arg(m_command).arg(args);
    qDebug("Command::run: %s", qPrintable(m_currentProcessCommandLine));

    m_currentProcess = new QProcess(this);
    m_isInError = false;
    m_hasExited = false;

#if QT_VERSION < 0x051000
    connect(m_currentProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#else
    connect(m_currentProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#endif
    connect(m_currentProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
    connect(m_currentProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));

    m_currentProcess->setProcessChannelMode(QProcess::MergedChannels);
    gettimeofday(&m_currentProcessStartTimeStamp, 0);
    m_currentProcess->start(m_currentProcessCommandLine);
}

void Command::kill()
{
    if (m_currentProcess)
    {
        qDebug("Command::kill: %lld", m_currentProcessPid);
        m_currentProcess->kill();
    }
}

QProcess::ProcessState Command::getLastProcessState() const
{
    return m_currentProcessState;
}

bool Command::getLastProcessError(QProcess::ProcessError& error) const
{
    if (m_isInError) {
        error = m_currentProcessError;
    }

    return m_isInError;
}

bool Command::getLastProcessExit(int& exitCode, QProcess::ExitStatus& exitStatus) const
{
    if (m_hasExited)
    {
        exitCode = m_currentProcessExitCode;
        exitStatus = m_currentProcessExitStatus;
    }

    return m_hasExited;
}

const QString& Command::getLastProcessLog() const
{
    return m_log;
}

void Command::processStateChanged(QProcess::ProcessState newState)
{
    //qDebug("Command::processStateChanged: %d", newState);
    if (newState == QProcess::Running) {
        m_currentProcessPid = m_currentProcess->processId();
    }

    m_currentProcessState = newState;
}

void Command::processError(QProcess::ProcessError error)
{
    //qDebug("Command::processError: %d state: %d", error, m_currentProcessState);
    gettimeofday(&m_currentProcessFinishTimeStamp, 0);
    m_currentProcessError = error;
    m_isInError = true;

    if (m_currentProcessState == QProcess::NotRunning)
    {
        m_log = m_currentProcess->readAllStandardOutput();

#if QT_VERSION < 0x051000
        disconnect(m_currentProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#else
        disconnect(m_currentProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#endif
        disconnect(m_currentProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
        disconnect(m_currentProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));

        m_currentProcess->deleteLater(); // make sure other threads can still access it until all events have been processed
        m_currentProcess = 0; // for this thread it can assume it was deleted
    }
}

void Command::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    //qDebug("Command::processFinished: (%d) %d", exitCode, exitStatus);
    gettimeofday(&m_currentProcessFinishTimeStamp, 0);
    m_currentProcessExitCode = exitCode;
    m_currentProcessExitStatus = exitStatus;
    m_hasExited = true;
    m_log = m_currentProcess->readAllStandardOutput();

#if QT_VERSION < 0x051000
    disconnect(m_currentProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#else
    disconnect(m_currentProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#endif
    disconnect(m_currentProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
    disconnect(m_currentProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));

    m_currentProcess->deleteLater(); // make sure other threads can still access it until all events have been processed
    m_currentProcess = 0; // for this thread it can assume it was deleted
}
