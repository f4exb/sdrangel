///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_COMMANDS_COMMANDKEYRECEIVER_H_
#define SDRBASE_COMMANDS_COMMANDKEYRECEIVER_H_

#include <QObject>

#include "export.h"

class QKeyEvent;

class SDRBASE_API CommandKeyReceiver : public QObject
{
    Q_OBJECT
public:
    CommandKeyReceiver();

    void setRelease(bool release) { m_release = release; }
    void setPass(bool pass) { m_pass = pass; }

protected:
    bool eventFilter(QObject* obj, QEvent* event);

private:
    bool m_release; //!< check release events
    bool m_pass;    //!< do not block events just tap them

    void keyEventHandler(QKeyEvent *e, Qt::Key& key, Qt::KeyboardModifiers& keyModifiers);
    bool isComposeKey(Qt::Key key);

    static const std::vector<Qt::Key> m_composeKeys;

signals:
    void capturedKey(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release);
};


#endif /* SDRBASE_COMMANDS_COMMANDKEYRECEIVER_H_ */
