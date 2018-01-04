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

#ifndef SDRBASE_COMMANDS_COMMAND_H_
#define SDRBASE_COMMANDS_COMMAND_H_

#include <Qt>
#include <QByteArray>
#include <QString>
#include <QMetaType>
#include <QObject>
#include <QProcess>
#include <sys/time.h>

class Command : public QObject
{
    Q_OBJECT
public:
    Command();
    Command(const Command& command);
    ~Command();
    void resetToDefaults();

    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    void setCommand(const QString& command) { m_command = command; }
    const QString& getCommand() const { return m_command; }
    void setArgString(const QString& argString) { m_argString = argString; }
    const QString& getArgString() const { return m_argString; }
    void setGroup(const QString& group) { m_group = group; }
    const QString& getGroup() const { return m_group; }
    void setDescription(const QString& description) { m_description = description; }
    const QString& getDescription() const { return m_description; }
    void setKey(Qt::Key key) { m_key = key; }
    Qt::Key getKey() const { return m_key; }
    void setKeyModifiers(Qt::KeyboardModifiers keyModifiers) { m_keyModifiers = keyModifiers; }
    Qt::KeyboardModifiers getKeyModifiers() const { return m_keyModifiers; }
    void setAssociateKey(bool associate) { m_associateKey = associate; }
    bool getAssociateKey() const { return m_associateKey; }
    void setRelease(bool release) { m_release = release; }
    bool getRelease() const { return m_release; }
    QString getKeyLabel() const;

    void run(const QString& apiAddress, int apiPort, int deviceSetIndex);
    void kill();
    QProcess::ProcessState getLastProcessState() const;
    bool getLastProcessError(QProcess::ProcessError& error) const;
    bool getLastProcessExit(int& exitCode, QProcess::ExitStatus& exitStatus) const;
    const QString& getLastProcessLog() const;
    struct timeval getLastProcessStartTimestamp() const { return m_currentProcessStartTimeStamp; }
    struct timeval getLastProcessFinishTimestamp() const { return m_currentProcessFinishTimeStamp; }
    const QString& getLastProcessCommandLine() const { return m_currentProcessCommandLine; }
    qint64 getLastProcessPid() const { return m_currentProcessPid; }

    static bool commandCompare(const Command *c1, Command *c2)
    {
        if (c1->m_group != c2->m_group)
        {
            return c1->m_group < c2->m_group;
        }
        else
        {
            if (c1->m_description != c2->m_description) {
                return c1->m_description < c2->m_description;
            }
            else
            {
                if (c1->m_key != c2->m_key) {
                    return c1->m_key < c2->m_key;
                } else {
                    return c1->m_release;
                }
            }
        }
    }

private:
    QString m_group;
    QString m_description;
    QString m_command;
    QString m_argString;
    Qt::Key m_key;
    Qt::KeyboardModifiers m_keyModifiers;
    bool m_associateKey;
    bool m_release;
    QProcess *m_currentProcess;
    QProcess::ProcessState m_currentProcessState;
    bool m_isInError;
    QProcess::ProcessError m_currentProcessError;
    bool m_hasExited;
    int m_currentProcessExitCode;
    QProcess::ExitStatus m_currentProcessExitStatus;
    QString m_log;
    struct timeval m_currentProcessStartTimeStamp;
    struct timeval m_currentProcessFinishTimeStamp;
    QString m_currentProcessCommandLine;
    qint64 m_currentProcessPid;

private slots:
    void processStateChanged(QProcess::ProcessState newState);
    void processError(QProcess::ProcessError error);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
};

Q_DECLARE_METATYPE(const Command*);
Q_DECLARE_METATYPE(Command*);

#endif /* SDRBASE_COMMANDS_COMMAND_H_ */
