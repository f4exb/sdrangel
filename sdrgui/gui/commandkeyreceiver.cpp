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

#include <gui/commandkeyreceiver.h>
#include <QEvent>
#include <QKeyEvent>

const std::vector<Qt::Key> CommandKeyReceiver::m_composeKeys = {Qt::Key_Shift, Qt::Key_Control, Qt::Key_Meta, Qt::Key_Alt, Qt::Key_AltGr};

CommandKeyReceiver::CommandKeyReceiver() :
    m_release(false),
    m_pass(true)
{
}

bool CommandKeyReceiver::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if ((!keyEvent->isAutoRepeat()) && (!isComposeKey(static_cast<Qt::Key>(keyEvent->key()))))
        {
//            qDebug("KeyReceiver::eventFilter: KeyPress");
            Qt::Key key;
            Qt::KeyboardModifiers keyModifiers;
            keyEventHandler(keyEvent, key, keyModifiers);
            emit capturedKey(key, keyModifiers, false);

            if (!m_pass) { // do not pass the event
                return true;
            }
        }
    }
    else if (m_release && (event->type()==QEvent::KeyRelease))
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if ((!keyEvent->isAutoRepeat()) && (!isComposeKey(static_cast<Qt::Key>(keyEvent->key()))))
        {
//            qDebug("KeyReceiver::eventFilter: KeyRelease");
            Qt::Key key;
            Qt::KeyboardModifiers keyModifiers;
            keyEventHandler(keyEvent, key, keyModifiers);
            emit capturedKey(key, keyModifiers, true);

            if (!m_pass) { // do not pass the event
                return true;
            }
        }
    }

    return QObject::eventFilter(obj, event); // pass the event on
}

void CommandKeyReceiver::keyEventHandler(QKeyEvent *e, Qt::Key& key, Qt::KeyboardModifiers& keyModifiers)
{
    key = static_cast<Qt::Key>(e->key());

    if (e->modifiers())
    {
        keyModifiers = e->modifiers();
    }
    else
    {
        keyModifiers = Qt::NoModifier;
    }
}

bool CommandKeyReceiver::isComposeKey(Qt::Key key)
{
    auto it = std::find(m_composeKeys.begin(), m_composeKeys.end(), key);
    return it != m_composeKeys.end();
}
