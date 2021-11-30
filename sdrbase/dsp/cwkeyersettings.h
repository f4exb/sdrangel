///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_DSP_CWKEYERSETTINGS_H_
#define SDRBASE_DSP_CWKEYERSETTINGS_H_

#include <QString>
#include <QByteArray>

#include "export.h"
#include "settings/serializable.h"

class SDRBASE_API CWKeyerSettings: public Serializable
{
public:
    typedef enum
    {
        CWNone,
        CWText,
        CWDots,
        CWDashes,
        CWKeyboard
    } CWMode;

    bool m_loop;
    CWMode m_mode;
    int m_sampleRate;
    QString m_text;
    int m_wpm;
    bool m_keyboardIambic;
    Qt::Key m_dotKey;
    Qt::KeyboardModifiers m_dotKeyModifiers;
    Qt::Key m_dashKey;
    Qt::KeyboardModifiers m_dashKeyModifiers;

    CWKeyerSettings();
    void resetToDefaults();

    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual void formatTo(SWGSDRangel::SWGObject *swgObject) const;
    virtual void updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject);
};



#endif /* SDRBASE_DSP_CWKEYERSETTINGS_H_ */
