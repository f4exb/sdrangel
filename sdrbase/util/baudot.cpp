///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include "baudot.h"

// https://en.wikipedia.org/wiki/Baudot_code
// We use < for FIGS and > for LTRS and ^ for Cyrillic
// Unicode used for source file encoding

const QStringList Baudot::m_ita2Letter = {
    "\0",   "E",    "\n",   "A",    " ",    "S",    "I",    "U",
    "\r",   "D",    "R",    "J",    "N",    "F",    "C",    "K",
    "T",    "Z",    "L",    "W",    "H",    "Y",    "P",    "Q",
    "O",    "B",    "G",    "<",    "M",    "X",    "V",    ">"
};

const QStringList Baudot::m_ita2Figure = {
    "\0",   "3",    "\n",   "-",    " ",    "\'",   "8",    "7",
    "\r",   "\x5",  "4",    "\a",   ",",    "!",    ":",    "(",
    "5",    "+",    ")",    "2",    "£",    "6",    "0",    "1",
    "9",    "?",    "&",    "<",    ".",    "/",    "=",    ">"
};

const QStringList Baudot::m_ukLetter = {
    "\0",   "A",    "E",    "/",    "Y",    "U",    "I",    "O",
    "<",    "J",    "G",    "H",    "B",    "C",    "F",    "D",
    " ",    "-",    "X",    "Z",    "S",    "T",    "W",    "V",
    "\b",   "K",    "M",    "L",    "R",    "Q",    "N",    "P"
};

const QStringList Baudot::m_ukFigure = {
    "\0",   "1",    "2",    "⅟",   "3",    "4",    "³⁄",    "5",
    " ",    "6",    "7",    "¹",   "8",    "9",    "⁵⁄",    "0",
    ">",    ".",    "⁹⁄",   ":",   "⁷⁄",   "²",    "?",     "\'",
    "\b",   "(",    ")",    "=",    "-",   "/",    "£",     "+"
};

const QStringList Baudot::m_europeanLetter = {
    "\0",   "A",    "E",    "É",    "Y",    "U",    "I",    "O",
    "<",    "J",    "G",    "H",    "B",    "C",    "F",    "D",
    " ",    "t",    "X",    "Z",    "S",    "T",    "W",    "V",
    "\b",   "K",    "M",    "L",    "R",    "Q",    "N",    "P"
};

const QStringList Baudot::m_europeanFigure = {
    "\0",   "1",    "2",   "&",    "3",    "4",    "º",     "5",
    " ",    "6",    "7",    "H̱",   "8",    "9",    "F̱",    "0",
    ">",    ".",    ",",   ":",    ";",    "!",    "?",     "\'",
    "\b",   "(",    ")",    "=",    "-",   "/",    "№",     "%"
};

const QStringList Baudot::m_usLetter = {
    "\0",   "E",    "\n",   "A",    " ",    "S",    "I",    "U",
    "\r",   "D",    "R",    "J",    "N",    "F",    "C",    "K",
    "T",    "Z",    "L",    "W",    "H",    "Y",    "P",    "Q",
    "O",    "B",    "G",    "<",    "M",    "X",    "V",    ">"
};

const QStringList Baudot::m_usFigure = {
    "\0",   "3",    "\n",   "-",    " ",    "\a",   "8",    "7",
    "\r",   "\x5",  "4",    "\'",   ",",    "!",    ":",    "(",
    "5",    "\"",   ")",    "2",    "#",    "6",    "0",    "1",
    "9",    "?",    "&",    "<",    ".",    "/",    ";",    ">"
};

const QStringList Baudot::m_russianLetter = {
    "\0",   "Е",    "\n",   "А",    " ",    "С",    "И",    "У",
    "\r",   "Д",    "Р",    "Й",    "Ч",    "Ф",    "Ц",    "К",
    "Т",    "З",    "Л",    "В",    "Х",    "Ы",    "П",    "Я",
    "О",    "Б",    "Г",    "<",    "М",    "Ь",    "Ж",    ">"
};

const QStringList Baudot::m_russianFigure = {
    "\0",   "3",    "\n",   "-",    " ",    "\'",   "8",    "7",
    "\r",   "Ч",    "4",    "Ю",    ",",    "Э",    ":",    "(",
    "5",    "+",    ")",    "2",    "Щ",    "6",    "0",    "1",
    "9",    "?",    "Ш",    "<",    ".",    "/",    ";",    ">"
};

const QStringList Baudot::m_murrayLetter = {
    " ",    "E",    "?",    "A",    ">",    "S",    "I",    "U",
    "\n",   "D",    "R",    "J",    "N",    "F",    "C",    "K",
    "T",    "Z",    "L",    "W",    "H",    "Y",    "P",    "Q",
    "O",    "B",    "G",    "<",    "M",    "X",    "V",    "\b"
};

const QStringList Baudot::m_murrayFigure = {
    " ",    "3",    "?",    " ",    ">",    "'",    "8",    "7",
    "\n",   "²",    "4",    "⁷⁄",   "-",    "⅟",    "(",    "⁹⁄",
    "5",    ".",    "/",    "2",    "⁵⁄",   "6",    "0",    "1",
    "9",    "?",    "³⁄",   "<",    ",",    "£",    ")",    "\b"
};

BaudotDecoder::BaudotDecoder()
{
    setCharacterSet(Baudot::ITA2);
    setUnshiftOnSpace(false);
    init();
}

void BaudotDecoder::setCharacterSet(Baudot::CharacterSet characterSet)
{
    m_characterSet = characterSet;
    switch (m_characterSet)
    {
    case Baudot::ITA2:
        m_letters = Baudot::m_ita2Letter;
        m_figures = Baudot::m_ita2Figure;
        break;
    case Baudot::UK:
        m_letters = Baudot::m_ukLetter;
        m_figures = Baudot::m_ukFigure;
        break;
    case Baudot::EUROPEAN:
        m_letters = Baudot::m_europeanLetter;
        m_figures = Baudot::m_europeanFigure;
        break;
    case Baudot::US:
        m_letters = Baudot::m_usLetter;
        m_figures = Baudot::m_usFigure;
        break;
    case Baudot::RUSSIAN:
        m_letters = Baudot::m_russianLetter;
        m_figures = Baudot::m_russianFigure;
        break;
    case Baudot::MURRAY:
        m_letters = Baudot::m_murrayLetter;
        m_figures = Baudot::m_murrayFigure;
        break;
    default:
        qDebug() << "BaudotDecoder::BaudotDecoder: Unsupported character set " << m_characterSet;
        m_letters = Baudot::m_ita2Letter;
        m_figures = Baudot::m_ita2Figure;
        m_characterSet = Baudot::ITA2;
        break;
    }
}

void BaudotDecoder::setUnshiftOnSpace(bool unshiftOnSpace)
{
    m_unshiftOnSpace = unshiftOnSpace;
}

void BaudotDecoder::init()
{
    m_figure = false;
}

QString BaudotDecoder::decode(char bits)
{
    QString c = m_figure ? m_figures[(int)bits] : m_letters[(int)bits];

    if ((c == ">") || (m_unshiftOnSpace && (c == " ")))
    {
        // Switch to letters
        m_figure = false;
        if (m_characterSet == Baudot::RUSSIAN) {
            m_letters = Baudot::m_ita2Letter;
        }
    }
    if (c == "<")
    {
        // Switch to figures
        m_figure = true;
    }
    if ((m_characterSet == Baudot::RUSSIAN) && (c == "\0"))
    {
        // Switch to Cyrillic
        m_figure = false;
        m_letters = Baudot::m_russianLetter;
        c = "^";
    }

    return c;
}

BaudotEncoder::BaudotEncoder()
{
    setCharacterSet(Baudot::ITA2);
    setUnshiftOnSpace(false);
    setMsbFirst(false);
    setStartBits(1);
    setStopBits(1);
    init();
}

void BaudotEncoder::setCharacterSet(Baudot::CharacterSet characterSet)
{
    m_characterSet = characterSet;
    switch (m_characterSet)
    {
    case Baudot::ITA2:
        m_chars[LETTERS] = Baudot::m_ita2Letter;
        m_chars[FIGURES] = Baudot::m_ita2Figure;
        break;
    case Baudot::UK:
        m_chars[LETTERS] = Baudot::m_ukLetter;
        m_chars[FIGURES] = Baudot::m_ukFigure;
        break;
    case Baudot::EUROPEAN:
        m_chars[LETTERS] = Baudot::m_europeanLetter;
        m_chars[FIGURES] = Baudot::m_europeanFigure;
        break;
    case Baudot::US:
        m_chars[LETTERS] = Baudot::m_usLetter;
        m_chars[FIGURES] = Baudot::m_usFigure;
        break;
    case Baudot::RUSSIAN:
        m_chars[LETTERS] = Baudot::m_ita2Letter;
        m_chars[FIGURES] = Baudot::m_russianFigure;
        break;
    case Baudot::MURRAY:
        m_chars[LETTERS] = Baudot::m_murrayLetter;
        m_chars[FIGURES] = Baudot::m_murrayFigure;
        break;
    default:
        qDebug() << "BaudotEncoder::BaudotEncoder: Unsupported character set " << m_characterSet;
        m_chars[LETTERS] = Baudot::m_ita2Letter;
        m_chars[FIGURES] = Baudot::m_ita2Figure;
        m_characterSet = Baudot::ITA2;
        break;
    }
    m_chars[(int)CYRILLIC] = Baudot::m_russianLetter;
}

void BaudotEncoder::setUnshiftOnSpace(bool unshiftOnSpace)
{
    m_unshiftOnSpace = unshiftOnSpace;
}

void BaudotEncoder::setMsbFirst(bool msbFirst)
{
    m_msbFirst = msbFirst;
}

// startBits should be 0 or 1
void BaudotEncoder::setStartBits(int startBits)
{
    m_startBits = startBits;
}

// stopBits should be 0, 1 or 2
void BaudotEncoder::setStopBits(int stopBits)
{
    m_stopBits = stopBits;
}

void BaudotEncoder::init()
{
    m_page = LETTERS;
}

bool BaudotEncoder::encode(QChar c, unsigned &bits, unsigned int &bitCount)
{
    bits = 0;
    bitCount = 0;

    // Only upper case is supported
    c = c.toUpper();
    QString s(c);

    if (s == '>')
    {
        addCode(bits, bitCount, m_chars[m_page].indexOf(s));
        m_page = LETTERS;
        return true;
    }
    else if (s == '<')
    {
        addCode(bits, bitCount, m_chars[m_page].indexOf(s));
        m_page = FIGURES;
        return true;
    }
    else if ((m_characterSet == Baudot::RUSSIAN) && (s == '\0'))
    {
        addCode(bits, bitCount, m_chars[m_page].indexOf(s));
        m_page = CYRILLIC;
        return true;
    }

    // We could create reverse look-up tables to speed this up, but it's only 200 baud...

    // Is character in current page? If so, use that, as it avoids switching
    if (m_chars[m_page].contains(s))
    {
        addCode(bits, bitCount, m_chars[m_page].indexOf(s));
        return true;
    }
    else
    {
        // Look for character in other pages
        const QString switchPage[] = { ">", "<", "\0" };

        for (int page = m_page == LETTERS ? 1 : 0; page < ((m_characterSet == Baudot::RUSSIAN) ? 3 : 2); page++)
        {
            if (m_chars[page].contains(s))
            {
                // Switch to page
                addCode(bits, bitCount, m_chars[m_page].indexOf(switchPage[page]));
                m_page = (BaudotEncoder::Page)page;

                addCode(bits, bitCount, m_chars[m_page].indexOf(s));
                return true;
            }
        }
    }

    return false;
}

void BaudotEncoder::addCode(unsigned& bits, unsigned int& bitCount, unsigned int code) const
{
    const unsigned int codeLen = 5;

    addStartBits(bits, bitCount);
    code = reverseBits(code, codeLen);
    addBits(bits, bitCount, code, codeLen);
    addStopBits(bits, bitCount);
}

void BaudotEncoder::addStartBits(unsigned& bits, unsigned int& bitCount) const
{
    // Start bit is 0
    addBits(bits, bitCount, 0, m_startBits);
}

void BaudotEncoder::addStopBits(unsigned& bits, unsigned int& bitCount) const
{
    // Stop bit is 1
    addBits(bits, bitCount, ((1 << m_stopBits)) - 1, m_stopBits);
}

void BaudotEncoder::addBits(unsigned& bits, unsigned int& bitCount, int data, int count) const
{
    bits |= data << bitCount;
    bitCount += count;
}

unsigned BaudotEncoder::reverseBits(unsigned bits, unsigned int count) const
{
    if (m_msbFirst) {
        return BaudotEncoder::reverse(bits) >> (sizeof(unsigned int) * 8 - count);
    } else {
        return bits;
    }
}

unsigned int BaudotEncoder::reverse(unsigned int x)
{
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));

    return((x >> 16) | (x << 16));
}
