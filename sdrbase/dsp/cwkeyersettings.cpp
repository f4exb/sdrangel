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

#include "SWGCWKeyerSettings.h"
#include "util/simpleserializer.h"
#include "cwkeyersettings.h"

CWKeyerSettings::CWKeyerSettings()
{
    resetToDefaults();
}

void CWKeyerSettings::resetToDefaults()
{
    m_loop = false;
    m_mode = CWNone;
    m_sampleRate = 48000;
    m_text = "";
    m_wpm = 13;
    m_keyboardIambic = true;
    m_dotKey = Qt::Key_Period;
    m_dotKeyModifiers = Qt::NoModifier;
    m_dashKey = Qt::Key_Minus;
    m_dashKeyModifiers = Qt::NoModifier;
}

QByteArray CWKeyerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeBool(2, m_loop);
    s.writeS32(3, (int) m_mode);
    s.writeS32(4, m_sampleRate);
    s.writeString(5, m_text);
    s.writeS32(6, m_wpm);
    s.writeS32(7, (int) m_dotKey);
    s.writeU32(8, (unsigned int) m_dotKeyModifiers);
    s.writeS32(9, (int) m_dashKey);
    s.writeU32(10, (unsigned int) m_dashKeyModifiers);
    s.writeBool(11, m_keyboardIambic);

    return s.final();
}

bool CWKeyerSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        int intval;
        unsigned int uintval;

        d.readBool(2, &m_loop, false);
        d.readS32(3, &intval, 0);
        m_mode = (CWMode) intval;
        d.readS32(4, &m_sampleRate, 48000);
        d.readString(5, &m_text, "");
        d.readS32(6, &m_wpm, 13);
        d.readS32(7, &intval, (int) Qt::Key_Period);
        m_dotKey = (Qt::Key) (intval < 0 ? 0 : intval);
        d.readU32(8, &uintval, 0);
        m_dotKeyModifiers = (Qt::KeyboardModifiers) uintval;
        d.readS32(9, &intval, (int) Qt::Key_Minus);
        m_dashKey = (Qt::Key) (intval < 0 ? 0 : intval);
        d.readU32(10, &uintval, 0);
        m_dashKeyModifiers = (Qt::KeyboardModifiers) uintval;
        d.readBool(11, &m_keyboardIambic, true);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void CWKeyerSettings::formatTo(SWGSDRangel::SWGObject *swgObject) const
{
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = static_cast<SWGSDRangel::SWGCWKeyerSettings *>(swgObject);

    apiCwKeyerSettings->setLoop(m_loop ? 1 : 0);
    apiCwKeyerSettings->setMode((int) m_mode);
    apiCwKeyerSettings->setSampleRate(m_sampleRate);

    if (apiCwKeyerSettings->getText()) {
        *apiCwKeyerSettings->getText() = m_text;
    } else {
        apiCwKeyerSettings->setText(new QString(m_text));
    }

    apiCwKeyerSettings->setWpm(m_wpm);
    apiCwKeyerSettings->setKeyboardIambic(m_keyboardIambic ? 1 : 0);
    apiCwKeyerSettings->setDotKey((int) m_dotKey);
    apiCwKeyerSettings->setDotKeyModifiers((unsigned int) m_dotKeyModifiers);
    apiCwKeyerSettings->setDashKey((int) m_dashKey);
    apiCwKeyerSettings->setDashKeyModifiers((unsigned int) m_dashKeyModifiers);
}

void CWKeyerSettings::updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject)
{
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings =
        static_cast<SWGSDRangel::SWGCWKeyerSettings *>(const_cast<SWGSDRangel::SWGObject *>(swgObject));

    if (keys.contains("cwKeyer.loop")) {
        m_loop = apiCwKeyerSettings->getLoop() != 0;
    }
    if (keys.contains("cwKeyer.mode")) {
        m_mode = (CWKeyerSettings::CWMode) apiCwKeyerSettings->getMode();
    }
    if (keys.contains("cwKeyer.text")) {
        m_text = *apiCwKeyerSettings->getText();
    }
    if (keys.contains("cwKeyer.sampleRate")) {
        m_sampleRate = apiCwKeyerSettings->getSampleRate();
    }
    if (keys.contains("cwKeyer.wpm")) {
        m_wpm = apiCwKeyerSettings->getWpm();
    }
    if (keys.contains("cwKeyer.keyboardIambic")) {
        m_keyboardIambic = apiCwKeyerSettings->getKeyboardIambic() != 0;
    }
    if (keys.contains("cwKeyer.dotKey")) {
        m_dotKey = (Qt::Key) apiCwKeyerSettings->getDotKey();
    }
    if (keys.contains("cwKeyer.dotKeyModifiers")) {
        m_dotKeyModifiers = (Qt::KeyboardModifiers) apiCwKeyerSettings->getDotKeyModifiers();
    }
    if (keys.contains("cwKeyer.dashKey")) {
        m_dashKey = (Qt::Key) apiCwKeyerSettings->getDashKey();
    }
    if (keys.contains("cwKeyer.dashKeyModifiers")) {
        m_dashKeyModifiers = (Qt::KeyboardModifiers) apiCwKeyerSettings->getDashKeyModifiers();
    }
}
