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

#include <QChar>
#include "cwkeyer.h"

/**
 * 0:  dot
 * 1:  dash
 * -1: end of sequence
 */
const char m_asciiToMorse[][128] = {
        {-1}, // 0
        {-1}, // 1
        {-1}, // 2
        {-1}, // 3
        {-1}, // 4
        {-1}, // 5
        {-1}, // 6
        {-1}, // 7
        {-1}, // 8
        {-1}, // 9
        {-1}, // 10
        {-1}, // 11
        {-1}, // 12
        {-1}, // 13
        {-1}, // 14
        {-1}, // 15
        {-1}, // 16
        {-1}, // 17
        {-1}, // 18
        {-1}, // 19
        {-1}, // 20
        {-1}, // 21
        {-1}, // 22
        {-1}, // 23
        {-1}, // 24
        {-1}, // 25
        {-1}, // 26
        {-1}, // 27
        {-1}, // 28
        {-1}, // 29
        {-1}, // 30
        {-1}, // 31
        {-1}, // 32 space is treated as word separator
        {1,0,1,0,1,1,-1}, // 33 !
        {0,1,0,0,1,0,-1}, // 34 "
        {-1}, // 35
        {-1}, // 36
        {-1}, // 37
        {-1}, // 38
        {0,1,1,1,1,0,-1}, // 39 '
        {1,0,1,1,0,1,-1}, // 40 (
        {1,0,1,1,0,1,-1}, // 41 )
        {-1}, // 42
        {0,1,0,1,0,-1},   // 43 +
        {1,1,0,0,1,1,-1}, // 44 ,
        {1,0,0,0,0,1,-1}, // 45 -
        {0,1,0,1,0,1,-1}, // 46 .
        {1,0,0,1,0,-1},   // 47 /
        {1,1,1,1,1,-1},   // 48 0
        {0,1,1,1,1,-1},   // 49 1
        {0,0,1,1,1,-1},   // 50 2
        {0,0,0,1,1,-1},   // 51 3
        {0,0,0,0,1,-1},   // 52 4
        {0,0,0,0,0,-1},   // 53 5
        {1,0,0,0,0,-1},   // 54 6
        {1,1,0,0,0,-1},   // 55 7
        {1,1,1,0,0,-1},   // 56 8
        {1,1,1,1,0,-1},   // 57 9
        {1,1,1,0,0,0,-1}, // 58 :
        {1,0,1,0,1,0,-1}, // 59 ;
        {-1}, // 60 <
        {1,0,0,0,1,-1},   // 61 =
        {-1}, // 62 >
        {0,0,1,1,0,0,-1}, // 63 ?
        {0,1,1,0,1,0,-1}, // 64 @
        {0,1,-1},         // 65 A
        {1,0,0,0,-1},     // 66 B
        {1,0,1,0,-1},     // 67 C
        {1,0,0,-1},       // 68 D
        {0,-1},           // 69 E
        {0,0,1,0,-1},     // 70 F
        {1,1,0,-1},       // 71 G
        {0,0,0,0,-1},     // 72 H
        {0,0,-1},         // 73 I
        {0,1,1,1,-1},     // 74 J
        {1,0,1,-1},       // 75 K
        {0,1,0,0,-1},     // 76 L
        {1,1,-1},         // 77 M
        {1,0,-1},         // 78 N
        {1,1,1,-1},       // 79 O
        {0,1,1,0,-1},     // 80 P
        {1,1,0,1,-1},     // 81 Q
        {0,1,0,-1},       // 82 R
        {0,0,0,-1},       // 83 S
        {1,-1},           // 84 T
        {0,0,1,-1},       // 85 U
        {0,0,0,1,-1},     // 86 V
        {0,1,1,-1},       // 87 W
        {1,0,0,1,-1},     // 88 X
        {1,0,1,1,-1},     // 89 Y
        {1,1,0,0,-1},     // 90 Z
        {-1}, // 91 [
        {-1}, // 92 \
        {-1}, // 93 ]
        {-1}, // 94 ^
        {-1}, // 95 _
        {-1}, // 96 `
        {0,1,-1},         // 97 A lowercase same as uppercase
        {1,0,0,0,-1},     // 98 B
        {1,0,1,0,-1},     // 99 C
        {1,0,0,-1},       // 100 D
        {0,-1},           // 101 E
        {0,0,1,0,-1},     // 102 F
        {1,1,0,-1},       // 103 G
        {0,0,0,0,-1},     // 104 H
        {0,0,-1},         // 105 I
        {0,1,1,1,-1},     // 106 J
        {1,0,1,-1},       // 107 K
        {0,1,0,0,-1},     // 108 L
        {1,1,-1},         // 109 M
        {1,0,-1},         // 110 N
        {1,1,1,-1},       // 111 O
        {0,1,1,0,-1},     // 112 P
        {1,1,0,1,-1},     // 113 Q
        {0,1,0,-1},       // 114 R
        {0,0,0,-1},       // 115 S
        {1,-1},           // 116 T
        {0,0,1,-1},       // 117 U
        {0,0,0,1,-1},     // 118 V
        {0,1,1,-1},       // 119 W
        {1,0,0,1,-1},     // 120 X
        {1,0,1,1,-1},     // 121 Y
        {1,1,0,0,-1},     // 122 Z
        {-1}, // 123 {
        {-1}, // 124 |
        {-1}, // 125 }
        {-1}, // 126 ~
        {-1}, // 127 DEL
};

CWKeyer::CWKeyer() :
    m_sampleRate(48000),
    m_textPointer(0),
	m_elementPointer(0),
    m_samplePointer(0),
    m_elementSpace(false),
    m_characterSpace(false),
    m_key(false),
    m_dot(false),
    m_dash(false),
    m_elementOn(false),
	m_asciiChar('\0'),
    m_mode(CWKey),
    m_keyIambicState(KeySilent),
	m_textState(TextStart)
{
    setWPM(13);
}

CWKeyer::~CWKeyer()
{
}

void CWKeyer::setWPM(int wpm)
{
    if ((wpm > 0) && (wpm < 21))
    {
        m_wpm = wpm;
        m_dotLength = (int) (m_sampleRate / (2.4f * wpm));
    }
}

void CWKeyer::setDot(bool dotOn)
{
    if (dotOn)
    {
        m_dash = false;
        m_dot = true;
    }
    else
    {
        m_dot = false;
    }
}

void CWKeyer::setDash(bool dashOn)
{
    if (dashOn)
    {
        m_dot = false;
        m_dash = true;
    }
    else
    {
        m_dash = false;
    }
}

int CWKeyer::getSample()
{
    if (m_mode == CWKey)
    {
        return m_key ? 1 : 0;
    }
    else if (m_mode == CWIambic)
    {
    	nextStateIambic();
        return m_key ? 1 : 0;
    }
    else if (m_mode == CWText)
    {
    	nextStateText();
        return m_key ? 1 : 0;
    }
    else
    {
    	return 0;
    }
}

void CWKeyer::nextStateIambic()
{
    switch (m_keyIambicState)
    {
    case KeySilent:
        if (m_dot)
        {
            m_keyIambicState = KeyDot;
            m_samplePointer = 0;
        }
        else if (m_dash)
        {
            m_keyIambicState = KeyDash;
            m_samplePointer = 0;
        }

        m_key = false;
        break;
    case KeyDot:
        if (m_samplePointer < m_dotLength) // dot key
        {
            m_key = true;
            m_samplePointer++;
        }
        else if (m_samplePointer < 2*m_dotLength) // dot silence (+1 dot length)
        {
            m_key = false;
            m_samplePointer++;
        }
        else // end
        {
            if (m_dash)
            {
                m_samplePointer = 0;
                m_keyIambicState = KeyDash;
            }
            else if (!m_dot)
            {
                m_keyIambicState = KeySilent;
            }

            m_samplePointer = 0;
            m_key = false;
        }
        break;
    case KeyDash:
        if (m_samplePointer < 3*m_dotLength) // dash key
        {
            m_key = true;
            m_samplePointer++;
        }
        else if (m_samplePointer < 4*m_dotLength) // dash silence (+1 dot length)
        {
            m_key = false;
            m_samplePointer++;
        }
        else // end
        {
            if (m_dot)
            {
                m_samplePointer = 0;
                m_keyIambicState = KeyDot;
            }
            else if (!m_dash)
            {
                m_keyIambicState = KeySilent;
            }

            m_samplePointer = 0;
            m_key = false;
        }
        break;
    default:
        m_samplePointer = 0;
        m_key = false;
        break;
    }
}


void CWKeyer::nextStateText()
{
	// TODO...
	switch (m_textState)
	{
	case TextStart:
		m_samplePointer = 0;
		m_elementPointer = 0;
		m_textPointer = 0;
		m_textState = TextStartChar;
		nextStateText();
		break;
	case TextStartChar:
		m_samplePointer = 0;
		m_elementPointer = 0;
		m_textState = TextStartElement;
		break;
	case TextStartElement:
		m_samplePointer = 0;
		m_textState = TextElement;
		break;
	case TextElement:
		nextStateIambic(); // dash or dot
		if (m_samplePointer == 0) // done
		{
			m_textState = TextStartElement; // next element
		}
		break;
	case TextCharSpace:
		if (m_samplePointer < 3*m_dotLength)
		{
			m_samplePointer++;
			m_key = false;
		}
		else
		{
			m_samplePointer = 0;
			m_textState = TextStartChar;
		}
		break;
	case TextWordSpace:
		if (m_samplePointer < 7*m_dotLength)
		{
			m_samplePointer++;
			m_key = false;
		}
		else
		{
			m_samplePointer = 0;
			m_textState = TextStartChar;
		}
		break;
	case TextEnd:
	default:
		m_key = false;
		break;
	}


	if (m_samplePointer == 0) // element not started or finished
	{
		if (m_asciiToMorse[m_elementPointer][m_asciiChar] == -1) // end of morse symbol
		{
			m_keyIambicState = KeyCharSpace;
			m_samplePointer++;
			m_key = false;
		}
		else
		{
			if (m_asciiToMorse[m_elementPointer][m_asciiChar] == 0) // dot
			{
				m_dot = true;
				m_dash = false;
			}
			else // dash
			{
				m_dot = false;
				m_dash = true;
			}

			nextStateIambic();
		}

		if (m_textPointer < m_text.length())
		{
			m_asciiChar = (m_text[m_textPointer]).toAscii();

			if (m_asciiChar < 0) {
				m_asciiChar = 0;
			}

			m_elementPointer = 0;
			m_textPointer++;
		}
		else
		{
			// TODO: end of text
		}
	}
	else
	{
		if ((m_keyIambicState == KeyDot) || (m_keyIambicState == KeyDash))
		{
			nextStateIambic();
		}
		else if (m_keyIambicState == KeyCharSpace)
		{
			if (m_samplePointer < 2*m_dotLength) // rest of a dash period (+2 dot periods)
			{
				m_samplePointer++;
				m_key = false;
			}
		}
	}
}
