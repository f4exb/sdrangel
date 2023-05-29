///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////
#include "util/timeutil.h"
#include "util/messagequeue.h"

#include "simplepttmessages.h"
#include "simplepttcommand.h"

MESSAGE_CLASS_DEFINITION(SimplePTTCommand::MsgRun, Message)

SimplePTTCommand::SimplePTTCommand() :
    m_currentProcess(nullptr),
    m_currentProcessPid(0),
    m_currentProcessState(QProcess::NotRunning),
    m_isInError(false),
    m_currentProcessError(QProcess::UnknownError),
    m_currentProcessExitCode(0),
    m_currentProcessExitStatus(QProcess::NormalExit),
    m_hasExited(false),
    m_msgQueueToGUI(nullptr)
{
    m_currentProcessStartTimeStampms = 0;
    m_currentProcessFinishTimeStampms = 0;
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

SimplePTTCommand::~SimplePTTCommand()
{
    if (m_currentProcess)
    {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
        disconnect(m_currentProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#else
        disconnect(m_currentProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#endif
        disconnect(m_currentProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
        disconnect(m_currentProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));
        m_currentProcess->deleteLater();
    }
}

void SimplePTTCommand::processStateChanged(QProcess::ProcessState newState)
{
    //qDebug("Command::processStateChanged: %d", newState);
    if (newState == QProcess::Running) {
        m_currentProcessPid = m_currentProcess->processId();
    }

    m_currentProcessState = newState;
}

void SimplePTTCommand::processError(QProcess::ProcessError error)
{
    //qDebug("Command::processError: %d state: %d", error, m_currentProcessState);
    m_currentProcessFinishTimeStampms = TimeUtil::nowms();
    m_currentProcessError = error;
    m_isInError = true;

    SimplePTTMessages::MsgCommandError *msg = SimplePTTMessages::MsgCommandError::create(
        m_currentProcessFinishTimeStampms, m_currentProcessError
    );

    if (m_currentProcessState == QProcess::NotRunning)
    {
        m_log = m_currentProcess->readAllStandardOutput();
        msg->getLog() = m_log;

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
        disconnect(m_currentProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#else
        disconnect(m_currentProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#endif
        disconnect(m_currentProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
        disconnect(m_currentProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));

        m_currentProcess->deleteLater(); // make sure other threads can still access it until all events have been processed
        m_currentProcess = nullptr; // for this thread it can assume it was deleted
    }

    if (m_msgQueueToGUI) {
        m_msgQueueToGUI->push(msg);
    } else {
        delete msg;
    }
}

void SimplePTTCommand::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug("SimplePTTCommand::processFinished: (%d) %d", exitCode, exitStatus);
    m_currentProcessFinishTimeStampms = TimeUtil::nowms();
    m_currentProcessExitCode = exitCode;
    m_currentProcessExitStatus = exitStatus;
    m_hasExited = true;
    m_log = m_currentProcess->readAllStandardOutput();

    if (m_msgQueueToGUI)
    {
        SimplePTTMessages::MsgCommandFinished *msg = SimplePTTMessages::MsgCommandFinished::create(
            m_currentProcessFinishTimeStampms,
            exitCode,
            exitStatus
        );
        msg->getLog() = m_log;
        m_msgQueueToGUI->push(msg);
    }

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    disconnect(m_currentProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#else
    disconnect(m_currentProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#endif
    disconnect(m_currentProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
    disconnect(m_currentProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));

    m_currentProcess->deleteLater(); // make sure other threads can still access it until all events have been processed
    m_currentProcess = nullptr; // for this thread it can assume it was deleted
}

void SimplePTTCommand::run(const QString& command, int rxDeviceSetIndex, double rxCenterFrequency, int txDeviceSetIndex, double txCenterFrequency)
{
    if (command == "") {
        return;
    }

    qDebug("SimplePTTCommand::run: %s", qPrintable(command));

    m_currentProcess = new QProcess(this);
    m_isInError = false;
    m_hasExited = false;
    QString args = QString("%1 %2 %3 %4").arg(rxDeviceSetIndex).arg(rxCenterFrequency).arg(txDeviceSetIndex).arg(txCenterFrequency);

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    connect(m_currentProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#else
    connect(m_currentProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
#endif
    connect(m_currentProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
    connect(m_currentProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));

    m_currentProcess->setProcessChannelMode(QProcess::MergedChannels);
    m_currentProcessStartTimeStampms = TimeUtil::nowms();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList allArgs = args.split(" ", Qt::SkipEmptyParts);
#else
    QStringList allArgs = args.split(" ", QString::SkipEmptyParts);
#endif
    m_currentProcess->start(command, allArgs);
}

void SimplePTTCommand::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool SimplePTTCommand::handleMessage(const Message& message)
{
    if (MsgRun::match(message))
    {
        qDebug("SimplePTTCommand::handleMessage: MsgRun");
        const MsgRun& cmd = (const MsgRun&) message;
        run(cmd.getCommand(), cmd.getRxDeviceSetIndex(), cmd.getRxCenterFrequency(), cmd.getTxDeviceSetIndex(), cmd.getTxCenterFrequency());
        return true;
    }

    return false;
}
