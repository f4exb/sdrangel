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

#ifndef SDRGUI_GUI_COMMANDKEYRECEIVER_H_
#define SDRGUI_GUI_COMMANDKEYRECEIVER_H_

#include <QObject>

class QKeyEvent;

class CommandKeyReceiver : public QObject
{
    Q_OBJECT
public:
    CommandKeyReceiver();

    void setRelease(bool release) { m_release = release; }
    void setPass(bool release) { m_release = release; }

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


#endif /* SDRGUI_GUI_COMMANDKEYRECEIVER_H_ */
