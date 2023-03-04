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

const QString Baudot::m_ita2Letter[] = {
    "\0",   "E",    "\n",   "A",    " ",    "S",    "I",    "U",
    "\r",   "D",    "R",    "J",    "N",    "F",    "C",    "K",
    "T",    "Z",    "L",    "W",    "H",    "Y",    "P",    "Q",
    "O",    "B",    "G",    "<",    "M",    "X",    "V",    ">"
};

const QString Baudot::m_ita2Figure[] = {
    "\0",   "3",    "\n",   "-",    " ",    "\'",   "8",    "7",
    "\r",   "\x5",  "4",    "\a",   ",",    "!",    ":",    "(",
    "5",    "+",    ")",    "2",    "£",    "6",    "0",    "1",
    "9",    "?",    "&",    "<",    ".",    "/",    "=",    ">"
};

const QString Baudot::m_ukLetter[] = {
    "\0",   "A",    "E",    "/",    "Y",    "U",    "I",    "O",
    "<",    "J",    "G",    "H",    "B",    "C",    "F",    "D",
    " ",    "-",    "X",    "Z",    "S",    "T",    "W",    "V",
    "\b",   "K",    "M",    "L",    "R",    "Q",    "N",    "P"
};

const QString Baudot::m_ukFigure[] = {
    "\0",   "1",    "2",    "⅟",   "3",    "4",    "³⁄",    "5",
    " ",    "6",    "7",    "¹",   "8",    "9",    "⁵⁄",    "0",
    ">",    ".",    "⁹⁄",   ":",   "⁷⁄",   "²",    "?",     "\'",
    "\b",   "(",    ")",    "=",    "-",   "/",    "£",     "+"
};

const QString Baudot::m_europeanLetter[] = {
    "\0",   "A",    "E",    "É",    "Y",    "U",    "I",    "O",
    "<",    "J",    "G",    "H",    "B",    "C",    "F",    "D",
    " ",    "t",    "X",    "Z",    "S",    "T",    "W",    "V",
    "\b",   "K",    "M",    "L",    "R",    "Q",    "N",    "P"
};

const QString Baudot::m_europeanFigure[] = {
    "\0",   "1",    "2",   "&",    "3",    "4",    "º",     "5",
    " ",    "6",    "7",    "H̱",   "8",    "9",    "F̱",    "0",
    ">",    ".",    ",",   ":",    ";",    "!",    "?",     "\'",
    "\b",   "(",    ")",    "=",    "-",   "/",    "№",     "%"
};

const QString Baudot::m_usLetter[] = {
    "\0",   "E",    "\n",   "A",    " ",    "S",    "I",    "U",
    "\r",   "D",    "R",    "J",    "N",    "F",    "C",    "K",
    "T",    "Z",    "L",    "W",    "H",    "Y",    "P",    "Q",
    "O",    "B",    "G",    "<",    "M",    "X",    "V",    ">"
};

const QString Baudot::m_usFigure[] = {
    "\0",   "3",    "\n",   "-",    " ",    "\a",   "8",    "7",
    "\r",   "\x5",  "4",    "\'",   ",",    "!",    ":",    "(",
    "5",    "\"",   ")",    "2",    "#",    "6",    "0",    "1",
    "9",    "?",    "&",    "<",    ".",    "/",    ";",    ">"
};

const QString Baudot::m_russianLetter[] = {
    "\0",   "Е",    "\n",   "А",    " ",    "С",    "И",    "У",
    "\r",   "Д",    "П",    "Й",    "Н",    "Ф",    "Ц",    "К",
    "Т",    "З",    "Л",    "В",    "Х",    "Ы",    "P",    "Я",
    "О",    "Б",    "Г",    "<",    "М",    "Ь",    "Ж",    ">"
};

const QString Baudot::m_russianFigure[] = {
    "\0",   "3",    "\n",   "-",    " ",    "\'",   "8",    "7",
    "\r",   "Ч",    "4",    "Ю",    ",",    "Э",    ":",    "(",
    "5",    "+",    ")",    "2",    "Щ",    "6",    "0",    "1",
    "9",    "?",    "Ш",    "<",    ".",    "/",    ";",    ">"
};

const QString Baudot::m_murrayLetter[] = {
    " ",    "E",    "?",    "A",    ">",    "S",    "I",    "U",
    "\n",   "D",    "R",    "J",    "N",    "F",    "C",    "K",
    "T",    "Z",    "L",    "W",    "H",    "Y",    "P",    "Q",
    "O",    "B",    "G",    "<",    "M",    "X",    "V",    "\b"
};

const QString Baudot::m_murrayFigure[] = {
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

    if ((c == '>') || (m_unshiftOnSpace && (c == " ")))
    {
        // Switch to letters
        m_figure = false;
        if (m_characterSet == Baudot::RUSSIAN) {
            m_letters = Baudot::m_ita2Letter;
        }
    }
    if (c == '<')
    {
        // Switch to figures
        m_figure = true;
    }
    if ((m_characterSet == Baudot::RUSSIAN) && (c == '\0'))
    {
        // Switch to Cyrillic
        m_figure = false;
        m_letters = Baudot::m_russianLetter;
        c = '^';
    }

    return c;
}

