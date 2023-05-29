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
#ifndef INCLUDE_FEATURE_SIMPLEPTTCOMMAND_H_
#define INCLUDE_FEATURE_SIMPLEPTTCOMMAND_H_

#include <QObject>
#include <QProcess>

#include "util/message.h"
#include "util/messagequeue.h"

class SimplePTTCommand : public QObject
{
    Q_OBJECT
public:
    class MsgRun : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        const QString& getCommand() const { return m_command; }
        int getRxDeviceSetIndex() const { return m_rxDeviceSetIndex; }
        double getRxCenterFrequency() const { return m_rxCenterFrequency; }
        int getTxDeviceSetIndex() const { return m_txDeviceSetIndex; }
        double getTxCenterFrequency() const { return m_txCenterFrequency; }

        static MsgRun* create(const QString& command, int rxDeviceSetIndex, double rxCenterFrequency, int txDeviceSetIndex, double txCenterFrequency) {
            return new MsgRun(command, rxDeviceSetIndex, rxCenterFrequency, txDeviceSetIndex, txCenterFrequency);
        }

    private:
        QString m_command;
        int m_rxDeviceSetIndex;
        double m_rxCenterFrequency;
        int m_txDeviceSetIndex;
        double m_txCenterFrequency;

        MsgRun(const QString& command, int rxDeviceSetIndex, double rxCenterFrequency, int txDeviceSetIndex, double txCenterFrequency) :
            Message(),
            m_command(command),
            m_rxDeviceSetIndex(rxDeviceSetIndex),
            m_rxCenterFrequency(rxCenterFrequency),
            m_txDeviceSetIndex(txDeviceSetIndex),
            m_txCenterFrequency(txCenterFrequency)
        { }
    };

    SimplePTTCommand();
    ~SimplePTTCommand();
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }
    const QString& getLastLog() { return m_log; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
    QProcess *m_currentProcess;
    qint64 m_currentProcessPid;
    QProcess::ProcessState m_currentProcessState;
    QString m_log;
    uint64_t  m_currentProcessStartTimeStampms;
    uint64_t  m_currentProcessFinishTimeStampms;
    bool m_isInError;
    QProcess::ProcessError m_currentProcessError;
    int m_currentProcessExitCode;
    QProcess::ExitStatus m_currentProcessExitStatus;
    bool m_hasExited;
    MessageQueue *m_msgQueueToGUI; //!< Queue to report state to GUI
    MessageQueue m_inputMessageQueue;

    bool handleMessage(const Message& message);
    void run(const QString& command, int rxDeviceSetIndex, double rxCenterFrequency, int txDeviceSetIndex, double txCenterFrequency);

private slots:
    void processStateChanged(QProcess::ProcessState newState);
    void processError(QProcess::ProcessError error);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleInputMessages();
};

#endif // INCLUDE_FEATURE_SIMPLEPTTCOMMAND_H_
