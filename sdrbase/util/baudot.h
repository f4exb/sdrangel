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

#ifndef INCLUDE_UTIL_BAUDOT_H
#define INCLUDE_UTIL_BAUDOT_H

#include <QString>
#include <QDateTime>
#include <QMap>

#include "export.h"

class SDRBASE_API Baudot {

public:

    enum CharacterSet {
        ITA2,
        UK,
        EUROPEAN,
        US,
        RUSSIAN, // MTK-2
        MURRAY
    };

    // QString used for fractions in figure set
    static const QStringList m_ita2Letter;
    static const QStringList m_ita2Figure;
    static const QStringList m_ukLetter;
    static const QStringList m_ukFigure;
    static const QStringList m_europeanLetter;
    static const QStringList m_europeanFigure;
    static const QStringList m_usLetter;
    static const QStringList m_usFigure;
    static const QStringList m_russianLetter;
    static const QStringList m_russianFigure;
    static const QStringList m_murrayLetter;
    static const QStringList m_murrayFigure;

};

class SDRBASE_API BaudotDecoder {

public:

    BaudotDecoder();
    void setCharacterSet(Baudot::CharacterSet characterSet = Baudot::ITA2);
    void setUnshiftOnSpace(bool unshiftOnSpace);
    void init();
    QString decode(char bits);

private:

    Baudot::CharacterSet m_characterSet;
    bool m_unshiftOnSpace;
    QStringList m_letters;
    QStringList m_figures;
    bool m_figure;

};

class SDRBASE_API BaudotEncoder {

public:

    BaudotEncoder();
    void setCharacterSet(Baudot::CharacterSet characterSet = Baudot::ITA2);
    void setUnshiftOnSpace(bool unshiftOnSpace);
    void setMsbFirst(bool msbFirst);
    void setStartBits(int startBits);
    void setStopBits(int stopBits);
    void init();
    bool encode(QChar c, unsigned& bits, unsigned int &bitCount);

private:

    void addCode(unsigned& bits, unsigned int& bitCount, unsigned int code) const;
    void addStartBits(unsigned int& bits, unsigned int& bitCount) const;
    void addStopBits(unsigned int& bits, unsigned int& bitCount) const;
    void addBits(unsigned int& bits, unsigned int& bitCount, int data, int count) const;
    unsigned reverseBits(unsigned int bits, unsigned int count) const;
    static unsigned reverse(unsigned int bits);

    Baudot::CharacterSet m_characterSet;
    bool m_unshiftOnSpace;
    QStringList m_chars[3];
    enum Page {
        LETTERS,
        FIGURES,
        CYRILLIC
    } m_page;
    bool m_msbFirst;
    int m_startBits;
    int m_stopBits;

};

#endif // INCLUDE_UTIL_BAUDOT_H

