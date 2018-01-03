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

class Command
{
public:
    Command();
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
    void setPressKey(Qt::Key key) { m_pressKey = key; }
    Qt::Key getPressKey() const { return m_pressKey; }
    void setReleaseKey(Qt::Key key) { m_releaseKey = key; }
    Qt::Key getReleaseKey() const { return m_releaseKey; }

private:
    QString m_command;
    QString m_argString;
    QString m_group;
    QString m_description;
    Qt::Key m_pressKey;
    Qt::Key m_releaseKey;
};



#endif /* SDRBASE_COMMANDS_COMMAND_H_ */
