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

#include "cwkeyer.h"

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
        {-1}, // 32
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
        {0,1,-1},         // 97 A
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
    m_elementSpace(false),
    m_characterSpace(false),
    m_key(false),
    m_dot(false),
    m_dash(false),
    m_elementOn(false),
    m_mode(CWKey),
    m_keyState(KeySilent)
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
        switch (m_keyState)
        {
        case KeySilent:
            if (m_dot)
            {
                m_keyState = KeyDot;
                m_elementPointer = 0;
            }
            else if (m_dash)
            {
                m_keyState = KeyDash;
                m_elementPointer = 0;
            }

            m_key = false;
            break;
        case KeyDot:
            if (m_elementPointer < m_dotLength) // dot key
            {
                m_key = true;
                m_elementPointer++;
            }
            else if (m_elementPointer < 2*m_dotLength) // dot silence
            {
                m_key = false;
                m_elementPointer++;
            }
            else // end
            {
                if (m_dash)
                {
                    m_elementPointer = 0;
                    m_keyState = KeyDash;
                }
                else if (!m_dot)
                {
                    m_keyState = KeySilent;
                }

                m_elementPointer = 0;
                m_key = false;
            }
            break;
        case KeyDash:
            if (m_elementPointer < 3*m_dotLength) // dash key
            {
                m_key = true;
                m_elementPointer++;
            }
            else if (m_elementPointer < 4*m_dotLength) // dash silence
            {
                m_key = false;
                m_elementPointer++;
            }
            else // end
            {
                if (m_dot)
                {
                    m_elementPointer = 0;
                    m_keyState = KeyDot;
                }
                else if (!m_dash)
                {
                    m_keyState = KeySilent;
                }

                m_elementPointer = 0;
                m_key = false;
            }
            break;
        default:
            m_elementPointer = 0;
            m_key = false;
            break;
        }

        return m_key ? 1 : 0;
    }
}

