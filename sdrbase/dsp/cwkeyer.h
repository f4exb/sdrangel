///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_DSP_CWKEYER_H_
#define SDRBASE_DSP_CWKEYER_H_

#include <QObject>
#include "util/export.h"

class SDRANGEL_API CWKeyer : public QObject {
    Q_OBJECT

public:
    typedef enum
    {
        CWText,
        CWKey,
        CWIambic
    } CWMode;

    typedef enum
    {
        KeySilent,
        KeyDot,
        KeyDash
    } CWKeyIambicState;

    typedef enum
    {
    	TextStart,
		TextStartChar,
		TextStartElement,
		TextElement,
		TextCharSpace,
		TextWordSpace,
		TextEnd
    } CWTextState;

    CWKeyer();
    ~CWKeyer();

    void setSampleRate(int sampleRate) { m_sampleRate = sampleRate; }
    void setWPM(int wpm);
    void setText(const QString& text) { m_text = text; }
    void setMode(CWMode mode) { m_mode = mode; }

    void setKey(bool key) { m_key = key; };
    void setDot(bool dotOn);
    void setDash(bool dashOn);

    int getSample();
    bool eom();
    void resetText() { m_textState = TextStart; }

private:
    int m_sampleRate;
    int m_wpm;
    int m_dotLength;   //!< dot length in samples
    QString m_text;
    int m_textPointer;
    int m_elementPointer;
    int m_samplePointer;
    bool m_elementSpace;
    bool m_characterSpace;
    bool m_key;
    bool m_dot;
    bool m_dash;
    bool m_elementOn;
    char m_asciiChar;
    CWMode m_mode;
    CWKeyIambicState m_keyIambicState;
    CWTextState m_textState;

    static const char m_asciiToMorse[128][7];

    void nextStateIambic();
    void nextStateText();
};



#endif /* SDRBASE_DSP_CWKEYER_H_ */
